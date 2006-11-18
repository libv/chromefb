/*
 * chromefb: driver for VIAs UniChrome and Chrome IGP graphics.
 *
 * Copyright (c) 2006      by Luc Verhaegen (libv@skynet.be)
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * This code of course borrows heavily of my xf86-video-unichrome code.
 * Care has been taken to only use that code that's fully my work.
 *
 */
/*
 * Validates and sets up Modes and relations.
 *
 */

/*
 * FB's mode model is fundamentally broken. The people who wrote this stuff up
 * were too busy trying to be different from X to come up with something
 * sensible.
 *
 * How the hell am i able to specify overscan, which is fundamentally different
 * from blanking (and this difference is important), when both are cluelessly
 * hashed together into margin. As an added bonus, in VESA speak, margin
 * signifies overscan. How fun is that.
 *
 * Difference is good, but only so when difference is paired with common sense.
 */

#include <linux/fb.h>

#include "chrome.h"
#include "chrome_io.h"

/*
 * Force some VGA alignments on the mode.
 * For instance, horizontal units are always character (byte) aligned.
 */
static void
chrome_vga_align(struct fb_var_screeninfo *mode)
{
#define VGA_H_ALIGN(x) (x) = ((x) + 7) & ~7

	VGA_H_ALIGN(mode->xres);
	VGA_H_ALIGN(mode->right_margin);
	VGA_H_ALIGN(mode->hsync_len);
	VGA_H_ALIGN(mode->left_margin);
}

/*
 * TODO: properly track overscan and adjust blanking accordingly.
 */
static int 
chrome_crtc1_mode_valid(struct chrome_info *info, struct fb_var_screeninfo *mode)
{
	__u32 total, blank_start, sync_start, sync_end, blank_end;
	__u32 right_overscan, left_overscan, upper_overscan, lower_overscan;
	__u32 temp, bytes_per_pixel;

	/* Set up for Horizontal timing */
	blank_start = mode->xres;
	sync_start = blank_start + mode->right_margin;
	sync_end = sync_start + mode->hsync_len;
	blank_end = sync_end + mode->left_margin;
	total = blank_end;

	/* in the above calculations, overscan is zero */
	right_overscan = 0;
	left_overscan = 0;

	if (total > 4100) {
		printk(KERN_WARNING "Horizontal Total out of range.\n");
		return -EINVAL;
	}

	if (mode->xres > 2048) {
		printk(KERN_WARNING "Horizontal Display out of range.\n");
		return -EINVAL;
	}

	/* In a normal world, this would be a perfectly valid seperate check.
	 * In FB-land however, blank_start is per default equal to xres.
	 */
	if (blank_start > 2048) {
		printk(KERN_WARNING "Horizontal Blanking Start out of range.\n");
		return -EINVAL;
	}

	if ((blank_end - blank_start) > 1025) {
		printk(KERN_WARNING "Horizontal Blanking End out of range.\n");
		return -EINVAL;
	}

	if (sync_start > 4095) {
		printk(KERN_WARNING "Horizontal Sync Start out of range.\n");
		return -EINVAL;
	}

	if ((sync_end - sync_start) > 256) {
		printk(KERN_WARNING "Horizontal Sync End out of range.\n");
		return -EINVAL;
	}

	/* Set up for Vertical timing */
	blank_start = mode->yres;
	sync_start = blank_start + mode->lower_margin;
	sync_end = sync_start + mode->vsync_len;
	blank_end = sync_end + mode->upper_margin;
	total = blank_end;

	/* in the above calculations, overscan is zero */
	lower_overscan = 0;
	upper_overscan = 0;

	if (total > 2049) {
		printk(KERN_WARNING "Vertical Total out of range.\n");
		return -EINVAL;
	}

	if (mode->yres > 2048) {
		printk(KERN_WARNING "Vertical Display out of range.\n");
		return -EINVAL;
	}

	if (sync_start > 2047) {
		printk(KERN_WARNING "Vertical Sync Start out of range.\n");
		return -EINVAL;
	}

	if ((sync_end - sync_start) > 16) {
		printk(KERN_WARNING "Vertical Sync End out of range.\n");
		return -EINVAL;
	}

	if (blank_start > 2048) {
		printk(KERN_WARNING "Vertical Blanking Start out of range.\n");
		return -EINVAL;
	}

	if ((blank_end - blank_start) > 257) {
		printk(KERN_WARNING "Vertical Blanking End out of range.\n");
		return -EINVAL;
	}

	/* Check Virtual */
	if (mode->bits_per_pixel < 24) /* don't do an extensive check here */
		bytes_per_pixel = mode->bits_per_pixel / 8;
	else
		bytes_per_pixel = 4;	

	/* We can't always pan all the way */
	/* Calculate the maximum offset */
	temp = mode->xres_virtual * bytes_per_pixel;
	temp *= mode->yres_virtual - mode->yres_virtual + 1;
	temp -= (mode->xres - 1) * bytes_per_pixel;

	switch (info->id) {
	case PCI_CHIP_VT3122:
		if (temp < 0x1FFFFFF)
			break;
		printk(KERN_WARNING "Virtual resolution exceeds panning limit");
		return -EINVAL;
	case PCI_CHIP_VT7205:
	case PCI_CHIP_VT3108:
		if (temp < 0x7FFFFFF) /* equals MAX FB size */
			break;
		printk(KERN_WARNING "Virtual resolution exceeds panning limit");
		return -EINVAL;
	default:
		printk(KERN_WARNING "%s: unsupported chip: 0x%4X\n", 
		       __func__, info->id);
		return -EINVAL;
	}

	return 0;
}

/*
 *
 */
int
chrome_mode_valid(struct chrome_info *info, struct fb_var_screeninfo *mode)
{
	int ret, clock;

	DBG(__func__);

	chrome_vga_align(mode);

	/* Clock */
	clock = PICOS2KHZ(mode->pixclock); /* idiots. */
	if ((clock < 20000) || (clock > 200000)) {
		printk(KERN_WARNING "Dotclock %dkHz is out of range.\n", clock);
		return -EINVAL;
	}

	/* CRTC */
	ret = chrome_crtc1_mode_valid(info, mode);
	if (ret)
		return ret;

	/* Outputs */
	/* Here we also check whether a CRT can handle this timing. */

	return 0;
}

/*
 *
 */
static void
chrome_mode_crtc_primary(struct chrome_info *info, struct fb_var_screeninfo *mode)
{
	__u32 blank_start, blank_end, sync_start, sync_end, total;
	__u16 temp, bytes_per_pixel;

	/* Unlock all registers */
	chrome_vga_cr_mask(info, 0x11, 0x00, 0x80); /* modify starting address */
	chrome_vga_cr_mask(info, 0x03, 0x80, 0x80); /* enable vsync access */
	chrome_vga_cr_mask(info, 0x47, 0x00, 0x01); /* unlock CRT registers */

	/* stop sequencer */
	chrome_vga_seq_write(info, 0x00, 0x00);

	/* set up misc register */
	temp = 0x23;
	if (!(mode->sync & FB_SYNC_HOR_HIGH_ACT))
		temp |= 0x40;
	if (!(mode->sync & FB_SYNC_VERT_HIGH_ACT))
		temp |= 0x80;
	temp |= 0x0C; /* Undefined/external clock */
	chrome_vga_misc_write(info, temp);
    
	/* Sequence registers */
	chrome_vga_seq_write(info, 0x01, 0xDF);

	/* 8bit lut / 80 text columns / wrap-around / extended mode */
	chrome_vga_seq_mask(info, 0x15, 0xA2, 0xE2);

	/* 555/565 -- bpp */
	switch (mode->bits_per_pixel) {
	case 8:
		chrome_vga_seq_mask(info, 0x15, 0x00, 0x1C);
		break;
	case 16:
		chrome_vga_seq_mask(info, 0x15, 0x14, 0x1C);
		break;
	case 24:
	case 32:
	default: /* silently continue on - should've been caught earlier */
		chrome_vga_seq_mask(info, 0x15, 0x0C, 0x1C);
		break;
	}

	/* Set up graphics registers -- do we really need to? */
	chrome_vga_graph_write(info, 0x00, 0x00);
	chrome_vga_graph_write(info, 0x01, 0x00);
	chrome_vga_graph_write(info, 0x02, 0x00);
	chrome_vga_graph_write(info, 0x03, 0x00);
	chrome_vga_graph_write(info, 0x04, 0x00);
	chrome_vga_graph_write(info, 0x05, 0x40);
	chrome_vga_graph_write(info, 0x06, 0x05);
	chrome_vga_graph_write(info, 0x07, 0x0F);
	chrome_vga_graph_write(info, 0x08, 0xFF);

	/* Null the offsets */
	chrome_vga_graph_write(info, 0x20, 0x00);
	chrome_vga_graph_write(info, 0x21, 0x00);
	chrome_vga_graph_write(info, 0x22, 0x00);

	/* Attribute registers */
	for (temp = 0; temp < 0x10; temp++)
		chrome_vga_attr_write(info, temp, temp);
	chrome_vga_attr_write(info, 0x10, 0x41);
	chrome_vga_attr_write(info, 0x11, 0xFF);
	chrome_vga_attr_write(info, 0x12, 0x0F);
	chrome_vga_attr_write(info, 0x13, 0x00);
	chrome_vga_attr_write(info, 0x14, 0x00);

	/* Finally, the good stuff, the CRTC */
	/* Do the FB dance first. */
	blank_start = mode->xres;
	sync_start = blank_start + mode->right_margin;
	sync_end = sync_start + mode->hsync_len;
	blank_end = sync_end + mode->left_margin;
	total = blank_end;

	/* horizontal total : 4100 */
	temp = (total >> 3) - 5;
	chrome_vga_cr_write(info, 0x00, temp & 0xFF);
	chrome_vga_cr_mask(info, 0x36, temp >> 5, 0x08);

	/* horizontal address : 2048 */
	temp = (mode->xres >> 3) - 1;
	chrome_vga_cr_write(info, 0x01, temp & 0xFF);

	/* horizontal blanking start : 2048 */
	temp = (blank_start >> 3) - 1;
	chrome_vga_cr_write(info, 0x02, temp & 0xFF);

	/* horizontal blanking end : start + 1025 */
	temp = (blank_end >> 3) - 1;
	chrome_vga_cr_mask(info, 0x03, temp, 0x1F);
	chrome_vga_cr_mask(info, 0x05, temp << 2, 0x80);
	chrome_vga_cr_mask(info, 0x33, temp >> 1, 0x20);

	/* CrtcHSkew ??? */

	/* horizontal sync start : 4095 */
	temp = sync_start >> 3;
	chrome_vga_cr_write(info, 0x04, temp & 0xFF);
	chrome_vga_cr_mask(info, 0x33, temp >> 4, 0x10);

	/* horizontal sync end : start + 256 */
	temp = sync_end >> 3;
	chrome_vga_cr_mask(info, 0x05, temp, 0x1F);

	/* Dance again for Vertical timing */
	blank_start = mode->yres;
	sync_start = blank_start + mode->lower_margin;
	sync_end = sync_start + mode->vsync_len;
	blank_end = sync_end + mode->upper_margin;
	total = blank_end;

	/* vertical total : 2049 */
	temp = total - 2;
	chrome_vga_cr_write(info, 0x06, temp & 0xFF);
	chrome_vga_cr_mask(info, 0x07, temp >> 8, 0x01);
	chrome_vga_cr_mask(info, 0x07, temp >> 4, 0x20);
	chrome_vga_cr_mask(info, 0x35, temp >> 10, 0x01);

	/* vertical address : 2048 */
	temp = mode->xres - 1;
	chrome_vga_cr_write(info, 0x12, temp & 0xFF);
	chrome_vga_cr_mask(info, 0x07, temp >> 7, 0x02);
	chrome_vga_cr_mask(info, 0x07, temp >> 3, 0x40);
	chrome_vga_cr_mask(info, 0x35, temp >> 8, 0x04);

	/* vertical sync start : 2047 */
	temp = sync_start;
	chrome_vga_cr_write(info, 0x10, temp & 0xFF);
	chrome_vga_cr_mask(info, 0x07, temp >> 6, 0x04);
	chrome_vga_cr_mask(info, 0x07, temp >> 2, 0x80);
	chrome_vga_cr_mask(info, 0x35, temp >> 9, 0x02);

	/* vertical sync end : start + 16 -- other bits someplace? */
	chrome_vga_cr_mask(info, 0x11, sync_end, 0x0F);

	/* line compare: We are not doing splitscreen so 0x3FFF */
	chrome_vga_cr_write(info, 0x18, 0xFF);
	chrome_vga_cr_mask(info, 0x07, 0x10, 0x10);
	chrome_vga_cr_mask(info, 0x09, 0x40, 0x40);
	chrome_vga_cr_mask(info, 0x33, 0x07, 0x06);
	chrome_vga_cr_mask(info, 0x35, 0x10, 0x10);

	/* zero Maximum scan line */
	chrome_vga_cr_mask(info, 0x09, 0x00, 0x1F);
	chrome_vga_cr_write(info, 0x14, 0x00);

	/* vertical blanking start : 2048 */
	temp = blank_start - 1;
	chrome_vga_cr_write(info, 0x15, temp & 0xFF);
	chrome_vga_cr_mask(info, 0x07, temp >> 5, 0x08);
	chrome_vga_cr_mask(info, 0x09, temp >> 4, 0x20);
	chrome_vga_cr_mask(info, 0x35, temp >> 7, 0x08);

	/* vertical blanking end : start + 257 */
	temp = blank_end - 1;
	chrome_vga_cr_write(info, 0x16, temp & 0xFF);

	/* vga row scan preset */
	chrome_vga_cr_write(info, 0x08, 0x00);

	if (mode->bits_per_pixel < 24)
		bytes_per_pixel = mode->bits_per_pixel / 8;
	else
		bytes_per_pixel = 4;

	/* offset */
	temp = mode->xres_virtual * bytes_per_pixel / 8;
	/* Make sure that this is 32byte aligned */
	if (temp & 0x03) {
		temp += 0x03;
		temp &= ~0x03;
	}
	chrome_vga_cr_write(info, 0x13, temp & 0xFF);
	chrome_vga_cr_mask(info, 0x35, temp >> 3, 0xE0);

	/* fetch count */
	temp = mode->xres * bytes_per_pixel / 8;
	/* Make sure that this is 32byte aligned */
	if (temp & 0x03) {
		temp += 0x03;
		temp &= ~0x03;
	}
	chrome_vga_seq_write(info, 0x1C, (temp >> 1) & 0xFF);
	chrome_vga_seq_mask(info, 0x1D, temp >> 9, 0x03);

	/* some leftovers */
	chrome_vga_cr_mask(info, 0x32, 0, 0xFF); /* Mode control */
	chrome_vga_cr_mask(info, 0x33, 0, 0x48); /* HSync control */
}

/*
 *
 * PLLs.
 *
 */
/* 
 *
 */
static void
chrome_pll_primary_set(struct chrome_info *info, __u32 clock)
{
	printk(KERN_DEBUG "%s to 0x%x\n", __func__, clock);

	switch (info->id) {
	case PCI_CHIP_VT3122:
	case PCI_CHIP_VT7205:
		chrome_vga_seq_write(info, 0x46, clock >> 8);
		chrome_vga_seq_write(info, 0x47, clock & 0xFF);
		break;
	case PCI_CHIP_VT3108:
		chrome_vga_seq_write(info, 0x44, clock >> 16);
		chrome_vga_seq_write(info, 0x45, (clock >> 8) & 0xFF);
		chrome_vga_seq_write(info, 0x46, clock & 0xFF);
		break;
	default:
		printk(KERN_ERR "%s: Unsupported chipset 0x%04X\n",
		       __func__, info->id);
		return;
	}

	chrome_vga_seq_mask(info, 0x40, 0x02, 0x02);
	chrome_vga_seq_mask(info, 0x40, 0x00, 0x02);

	chrome_vga_misc_mask(info, 0x00, 0x00); /* poke */
}

/*
 *
 */
static __u32
vt3122_pll_generate_best(int clock, int shift, int min_div, int max_div,
                         int *best_diff)
{
	__u32 pll = 0;
	__u8 pll_shift;
	int div, mult, diff;

	switch (shift) {
	case 4:
		pll_shift = 0x80;
		break;
	case 2:
		pll_shift = 0x40;
		break;
	default:
		pll_shift = 0x00;
		break;
	}

	for (div = min_div; div <= max_div; div++) {
		mult = clock * div * shift * 1000;
		mult /= 14318;
		mult += 500; /* round */
		mult /= 1000;

		if (mult < 129) {
			diff = clock - mult * 14318 / div / shift;

			if (diff < 0)
				diff *= -1;

			if (diff < *best_diff) {
				*best_diff = diff;
				pll = ((pll_shift | div) << 8) | mult;
			}
		}
	}

	return pll;
}

/*
 * This might seem nasty and ugly, but it's the best solution given the crappy
 * limitations the VT3122 pll has.
 *
 * The below information has been gathered using nothing but a lot of time and
 * perseverance.
 */
static __u32
vt3122_pll_generate(int clock)
{
	__u32 pll;
	int diff = 300000;

	DBG(__func__);

	if (clock > 72514)
		pll = vt3122_pll_generate_best(clock, 1, 2, 25, &diff);
	else if (clock > 71788)
		pll = vt3122_pll_generate_best(clock, 1, 16, 24, &diff);
	else if (clock > 71389) {
		pll = 0x1050; /* Big singularity. */
        
		diff = clock - 71590;
		if (diff < 0)
			diff *= -1;
	} else if (clock > 48833) {
		__u32 tmp_pll;

		pll = vt3122_pll_generate_best(clock, 2, 7, 18, &diff);
    
		if (clock > 69024)
			tmp_pll = vt3122_pll_generate_best(clock, 1, 15, 23, &diff);
		else if (clock > 63500)
			tmp_pll = vt3122_pll_generate_best(clock, 1, 15, 21, &diff);
		else if (clock > 52008)
			tmp_pll = vt3122_pll_generate_best(clock, 1, 17, 19, &diff);
		else
			tmp_pll = vt3122_pll_generate_best(clock, 1, 17, 17, &diff);

		if (tmp_pll)
			pll = tmp_pll;
	} else if (clock > 35220)
		pll = vt3122_pll_generate_best(clock, 2, 11, 24, &diff);
	else if (clock > 34511)
		pll = vt3122_pll_generate_best(clock, 2, 11, 23, &diff);
	else if (clock > 33441)
		pll = vt3122_pll_generate_best(clock, 2, 13, 22, &diff);
	else if (clock > 31967)
		pll = vt3122_pll_generate_best(clock, 2, 11, 21, &diff);
	else
		pll = vt3122_pll_generate_best(clock, 4, 8, 19, &diff);
    
	printk(KERN_DEBUG "%s: pll: 0x%04X (%d off from %d)\n",
	       __func__, pll, diff, clock);
	return pll;
}

/*
 *
 */
static int
vt3108_pll_generate_best(int clock, int shift, int div, int old_diff,
                         __u32 *pll)
{
	int mult;
	int diff;
	
	mult = clock * (div << shift) * 1000;
	mult /= 14318;
	mult += 500; /* round */
	mult /= 1000;

	if (mult > 257) /* Don't go over 0xFF + 2; wobbly */
		return old_diff;

	diff = clock - mult * 14318 / (div << shift);
	if (diff < 0)
		diff *= -1;
    
	if (diff < old_diff) {
		*pll = (mult - 2) << 16;
		*pll |= div - 2;
		*pll |= shift << 10;
		return diff;
	} else
		return old_diff;
}

/*
 *
 */
static __u32
vt3108_pll_generate(int clock)
{
	__u32 pll;
	int diff = 300000;
	int i;

	DBG(__func__);

	for (i = 2; i < 15; i++)
		diff = vt3108_pll_generate_best(clock, 0, i, diff, &pll);

	for (i = 2; i < 15; i++) 
		diff = vt3108_pll_generate_best(clock, 1, i, diff, &pll);

	for (i = 2; i < 32; i++) 
		diff = vt3108_pll_generate_best(clock, 2, i, diff, &pll);

	for (i = 2; i < 21; i++) 
		diff = vt3108_pll_generate_best(clock, 3, i, diff, &pll);

	printk(KERN_DEBUG "%s: PLL: 0x%04X (%d off from %d)\n",
	       __func__, pll, diff, clock);

	return pll;
}

/*
 *
 */
__u32
chrome_pll_generate(struct chrome_info *info, int clock)
{
	DBG(__func__);

	switch (info->id) {
	case PCI_CHIP_VT3122:
	case PCI_CHIP_VT7205:
		return vt3122_pll_generate(clock);
	case PCI_CHIP_VT3108:
		return vt3108_pll_generate(clock);
	default:
		printk(KERN_WARNING "%s: Unhandled Chipset: 0x%04X\n",
		       __func__, info->id);
		return 0;
	}
}


/*
 * Primary only, so far.
 */
int 
chrome_mode_write(struct chrome_info *info, struct fb_var_screeninfo *mode)
{
	__u32 pll;

	chrome_vga_cr_mask(info, 0x17, 0x00, 0x80);

	chrome_mode_crtc_primary(info, mode);

	/* handle outputs here */

	pll = chrome_pll_generate(info, PICOS2KHZ(mode->pixclock));
	chrome_pll_primary_set(info, pll);

	chrome_vga_cr_mask(info, 0x17, 0x80, 0x80);

	return 0;
}
