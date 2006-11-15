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

/*
 * Force some VGA alignments on the mode.
 * For instance, horizontal units are always character (byte) aligned.
 */
static void
chrome_vga_align(struct fb_var_screeninfo *mode)
{
#define VGA_H_ALIGN(x) (x) = ((x) + 7) & ~7

	VGA_H_ALIGN(mode->width);

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
	clock = 1000000000 / mode->pixclock; /* idiots. */
	if ((clock < 20000000) || (clock > 200000000)) {
		printk(KERN_WARNING "Dotclock %dHz is out of range.\n", clock);
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
