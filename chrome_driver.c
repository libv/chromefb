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
 * Hardware support is currently limited to VT3122, VT7205 and VT3108.
 *
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/pci.h>

#include "chrome.h"
#include "chrome_io.h"

/*
 *
 * FB driver initialisation.
 *
 */

/*
 * Store the full VGA/textmode state for later restoration.
 */
static void
chrome_textmode_store(struct chrome_info *info)
{
	struct chrome_state *state = &info->state;
	int i;

	DBG(__func__);

	memset(state, 0, sizeof(struct chrome_state));

	/* Don't bother with a synchronous reset, we're not touching anything */

	/* CR registers */
	for (i = 0x00; i < 0x1E; i++)
		state->CR[i] = chrome_vga_cr_read(info, i);
	/* 0x1E - 0x32: unused */
	for (i = 0x33; i < 0xA3; i++)
		state->CR[i] = chrome_vga_cr_read(info, i);

	/* SR registers */
	for (i = 0x00; i < 0x05; i++)
		state->SR[i] = chrome_vga_seq_read(info, i);
	/* 05 - 0x0F: unused */
	for (i = 0x10; i < 0x50; i++)
		state->SR[i] = chrome_vga_seq_read(info, i);

	/* Graph registers */
	for (i = 0x00; i < 0x08; i++)
		state->GR[i] = chrome_vga_graph_read(info, i);

	/* Attribute registers */
	for (i = 0x00; i < 0x14; i++)
		state->AR[i] = chrome_vga_attr_read(info, i);

	state->Misc = chrome_vga_misc_read(info);

	/* store VGA memory? */
	if (!(state->AR[0x10] & 0x01)) { /* don't bother when in graphics mode */
		/* four planes, two are font data, two are text */
		state->planes = (unsigned char *) vmalloc(4 * VGA_FB_PLANE_SIZE);

		if (state->planes)
			/* Cheat: just grab the lowest 256kB from already mapped FB */
			memcpy(state->planes, info->fbbase, 4 * VGA_FB_PLANE_SIZE);
		else
			printk(KERN_ERR "Unable to store VGA FB planes.\n");
	}

	/* store palette */
	chrome_vga_dac_read_address(info, 0x00);
	for (i = 0; i < 0x100; i++) {
		state->palette[i].red = chrome_vga_dac_read(info);
		state->palette[i].green = chrome_vga_dac_read(info);
		state->palette[i].blue = chrome_vga_dac_read(info);
	}
}

/*
 * Restore the saved VGA/textmode state.
 */
static void
chrome_textmode_restore(struct chrome_info *info)
{
	struct chrome_state *state = &info->state;
	int i;

	DBG(__func__);

        /* Synchronous reset */
	chrome_vga_seq_mask(info, 0x00, 0x00, 0x02);

	/* CR registers */
	for (i = 0x00; i < 0x1E; i++)
		chrome_vga_cr_write(info, i, state->CR[i]);
	/* 0x1E - 0x32: unused */
	for (i = 0x33; i < 0xA3; i++)
		chrome_vga_cr_write(info, i, state->CR[i]);

	/* SR registers */
	chrome_vga_seq_write(info, 0x00, state->SR[0x00] & 0xFD);
	for (i = 0x01; i < 0x05; i++)
		chrome_vga_seq_write(info, i, state->SR[i]);
	/* 05 - 0x0F: unused */
	for (i = 0x10; i < 0x50; i++)
		chrome_vga_seq_write(info, i, state->SR[i]);

	/* Graph registers */
	for (i = 0x00; i < 0x08; i++)
		chrome_vga_graph_write(info, i, state->GR[i]);

	/* Attribute registers */
	for (i = 0x00; i < 0x14; i++)
		chrome_vga_attr_write(info, i, state->AR[i]);

	/* Restore FB */
	if (state->planes)
		memcpy(info->fbbase, state->planes, 4 * VGA_FB_PLANE_SIZE);

	/* Restore palette */
	chrome_vga_dac_write_address(info, 0x00);
	for (i = 0; i < 0x100; i++) {
		chrome_vga_dac_write(info, state->palette[i].red);
		chrome_vga_dac_write(info, state->palette[i].green);
		chrome_vga_dac_write(info, state->palette[i].blue);
	}

	/* Reset clock */
	chrome_vga_seq_mask(info, 0x40, 0x06, 0x06);
	chrome_vga_seq_mask(info, 0x40, 0x00, 0x06);

	chrome_vga_misc_write(info, state->Misc);

	/* Synchronous reset disable */
	chrome_vga_seq_mask(info, 0x00, 0x02, 0x02);
}

/*
 *
 */
static int
chrome_open(struct fb_info *fb_info, int user)
{
	struct chrome_info *info = (struct chrome_info *) fb_info;
	int count;

	DBG(__func__);

	count = atomic_read(&info->fb_ref_count);
    
	if (!count)
		chrome_textmode_store(info);

	atomic_inc(&info->fb_ref_count);

	return 0;
}

/*
 *
 */
static int
chrome_release(struct fb_info *fb_info, int user)
{
	struct chrome_info *info = (struct chrome_info *) fb_info;
	int count;

	DBG(__func__);

	count = atomic_read(&info->fb_ref_count);

	if (!count)
		return -EINVAL;

	if (count == 1)
		chrome_textmode_restore(info);

	atomic_dec(&info->fb_ref_count);

	return 0;
}

/*
 *
 */
static int
chrome_check_var(struct fb_var_screeninfo *mode, struct fb_info *fb_info)
{
	struct chrome_info *info = (struct chrome_info *) fb_info;
	__u32 temp, bytes_per_pixel;
	int ret;

	DBG(__func__);

	/* bpp */
	switch (mode->bits_per_pixel) {
	case 8:
	case 16:
		bytes_per_pixel = mode->bits_per_pixel / 8;
		break;
	case 24:
	case 32:
		bytes_per_pixel = 4;
		break;
	default:
		printk(KERN_WARNING "Unsupported bitdepth: %dbpp.\n", 
		       mode->bits_per_pixel);
		return -EINVAL;	
	}

	/* Virtual */
	temp = mode->xres_virtual * mode->yres_virtual * bytes_per_pixel;
	if (temp >= info->fbsize) {
		printk(KERN_WARNING "Not enough FB space to house %dx%d@%2dbpp",
		       mode->xres_virtual, mode->yres_virtual,
		       mode->bits_per_pixel);
		return -EINVAL;
	}

	/* Mode */
        ret = chrome_mode_valid(info, mode);
	if (ret)
		return ret;

	/* Set up memory layout */
	switch (mode->bits_per_pixel) {
	case 8:
		/* Indexed, so null everything except length.
		 * Length defines the size of data in the palette. */
		mode->red.offset = 0;
		mode->red.length = 8;
		mode->red.msb_right = 0;

		mode->green.offset = 0;
		mode->green.length = 8;
		mode->green.msb_right = 0;

		mode->blue.offset = 0;
		mode->blue.length = 8;
		mode->blue.msb_right = 0;

		mode->transp.offset = 0;
		mode->transp.length = 0;
		mode->transp.msb_right = 0;

		break;
	case 16:
		mode->red.offset = 11;
		mode->red.length = 5;
		mode->red.msb_right = 0;

		mode->green.offset = 5;
		mode->green.length = 6;
		mode->green.msb_right = 0;

		mode->blue.offset = 0;
		mode->blue.length = 5;
		mode->blue.msb_right = 0;

		mode->transp.offset = 0;
		mode->transp.length = 0;
		mode->transp.msb_right = 0;

		break;
	case 24:
	case 32:
		mode->red.offset = 16;
		mode->red.length = 8;
		mode->red.msb_right = 0;

		mode->green.offset = 8;
		mode->green.length = 8;
		mode->green.msb_right = 0;

		mode->blue.offset = 0;
		mode->blue.length = 8;
		mode->blue.msb_right = 0;

		mode->transp.offset = 0;
		mode->transp.length = 0;
		mode->transp.msb_right = 0;
		
		break;
	default:
		/* checked earlier - so shouldn't happen */
		return -EINVAL;
	}

	/* Rotation - not yet. */
	if (mode->rotate) {
		printk(KERN_WARNING "We don't do rotation yet.\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * Doesn't deal with CRTC1/2 palette switching currently.
 */
static int 
chrome_setcmap(struct fb_cmap *cmap, struct fb_info *fb_info)
{
	struct chrome_info *info = (struct chrome_info *) fb_info;
	int i;

	DBG(__func__);

	if (cmap->start + cmap->len > 0xFF)
		return -EINVAL;

	chrome_vga_dac_mask_write(info, 0xFF);

	chrome_vga_dac_write_address(info, cmap->start);
	for (i = cmap->start; i < (cmap->start + cmap->len); i++) {
		chrome_vga_dac_write(info, cmap->red[i] & 0xFF);
		chrome_vga_dac_write(info, cmap->green[i] & 0xFF);
		chrome_vga_dac_write(info, cmap->blue[i] & 0xFF);
	}

	/* So, erm... What about the overscan colour then?
	 * Are you telling me FB has no notion of that either?
	 */
	/* just pick 0x00, which is hopefully black*/
	chrome_vga_attr_write(info, 0x11, 0x00);

	/* We still need to set the Gamma enable bits somewhere */
	return 0;
}

/*
 * Very rudimentary, could be mightily advanced though.
 */
static int 
chrome_blank(int blank, struct fb_info *fb_info)
{
	struct chrome_info *info = (struct chrome_info *) fb_info;

	DBG(__func__);

	switch (blank) {
	case FB_BLANK_UNBLANK:
		chrome_vga_cr_mask(info, 0x17, 0x80, 0x80);
		/* outputs? */
		break;
	case FB_BLANK_NORMAL:
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_POWERDOWN:
		chrome_vga_cr_mask(info, 0x17, 0x00, 0x80);
		/* outputs? */
		/* We could do so much more here */
		break;
	default:
		return -EINVAL; 
	}
	return 0;
}

/*
 * Primary only currently.
 */
static int
chrome_pan_display(struct fb_var_screeninfo *mode, struct fb_info *fb_info)
{
	struct chrome_info *info = (struct chrome_info *) fb_info;
	__u32 base;
	
	DBG(__func__);

	base = (mode->xoffset * mode->xres_virtual) + mode->yoffset;
	if (mode->bits_per_pixel < 24)
		base *= mode->bits_per_pixel / 8;
	else
		base *= 4;
	base >>= 1;

	chrome_vga_cr_write(info, 0x0C, (base >> 8) & 0xFF);
	chrome_vga_cr_write(info, 0x0D, base & 0xFF);
	chrome_vga_cr_write(info, 0x34, (base >> 16) & 0xFF);
	
	/* doesn't hurt even on VT3122 */
	chrome_vga_cr_mask(info, 0x48, base >> 24, 0x03);

	return 0;
}


#ifdef UNUSED
/*
 * Sync 2D engine.
 */
static int
chrome_sync(struct fb_info *fb_info)
{
	/* not using 2D engine yet. */
	return 0;
}
#endif

/*
 * FB driver callbacks.
 */
static struct fb_ops chrome_ops __devinitdata = {
	.owner =  THIS_MODULE,
	.fb_open =  chrome_open,
	.fb_release =  chrome_release,
	.fb_check_var =  chrome_check_var,
	/* .fb_set_par =  chrome_set_par, */
	.fb_setcolreg =  NULL, /* use set_cmap instead */
	.fb_setcmap = chrome_setcmap,
	.fb_blank =  chrome_blank,
	.fb_pan_display =  chrome_pan_display,
#if 0
	.fb_fillrect =  chrome_fillrect,
	.fb_copyarea =  chrome_copyarea,
	.fb_imageblit =  chrome_imageblit,
	.fb_cursor =  chrome_cursor,
	.fb_sync =  chrome_sync,
#endif
};


/*
 *
 * Driver module initialisation.
 *
 */

#ifdef MODULE

MODULE_AUTHOR("(c) 2003-2006 by Luc Verhaegen (libv@skynet.be)");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FBDev driver for VIA Unichrome and Chrome IGPs");

#endif /* MODULE */

static struct pci_device_id chrome_devices[] = {
	{PCI_VENDOR_ID_VIA, PCI_CHIP_VT3122, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1},
	{PCI_VENDOR_ID_VIA, PCI_CHIP_VT7205, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1},
	{PCI_VENDOR_ID_VIA, PCI_CHIP_VT3108, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1},
#ifdef CHROME_ENOHW
        {PCI_VENDOR_ID_VIA, PCI_CHIP_VT3118, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
        {PCI_VENDOR_ID_VIA, PCI_CHIP_VT3344, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
        {PCI_VENDOR_ID_VIA, PCI_CHIP_VT3157, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
        {PCI_VENDOR_ID_VIA, PCI_CHIP_VT3230, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
        {PCI_VENDOR_ID_VIA, PCI_CHIP_VT3343, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
        {PCI_VENDOR_ID_VIA, PCI_CHIP_VT3371, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
#endif
	{0, 0, 0, 0, 0, 0, 0}
};

MODULE_DEVICE_TABLE(pci, chrome_devices);

/*
 * Basic initialisation of fb_info structure.
 */
static struct chrome_info *
chrome_alloc_fb_info(struct pci_dev *dev, const struct pci_device_id *id)
{
	struct chrome_info *info;

	DBG(__func__);

	info = (struct chrome_info*) kmalloc(sizeof(struct chrome_info),
                                             GFP_KERNEL);
	if (!info)
		return NULL;

	info->id = id->device;
	info->pci_dev = dev;

	return info;
}

/*
 * Handles everything needed to claim and initialise the IO area.
 * Do this before we make any chrome io calls, otherwise we will segfault.
 */
static int
chrome_io_init(struct chrome_info *info)
{
	unsigned int iobase, ioend;

	DBG(__func__);

	iobase = info->pci_dev->resource[1].start;
	ioend = info->pci_dev->resource[1].end;
	
	if (!request_mem_region(iobase, ioend - iobase, DRIVER_NAME)) {
		printk(KERN_ERR "%s: Cannot request IO resource.\n", __func__);
		return -ENODEV;
	}

	info->iobase = ioremap_nocache(iobase, 0x9000);
	if (!info->iobase) {
		printk(KERN_ERR "%s: Unable to remap IO region.\n", __func__);
		release_mem_region(iobase, ioend - iobase);
		return -ENODEV;
	}

	/* enable VGA */
	chrome_vga_enable_mask(info, 0x01, 0x01);

	/* set CR to 0x3Dx */
	chrome_vga_misc_mask(info, 0x01, 0x01);

	/* unlock extended io */
	chrome_vga_seq_write(info, 0x10, 0x01);

	/* enable MMIO for primary */
	chrome_vga_seq_mask(info, 0x10, 0x60, 0x60);

	return 0;
}

/*
 * If we don't release the region when unloading, then there will be no
 * subsequent reloading possible.
 */
static void
chrome_io_release(struct chrome_info *info)
{
	unsigned int iobase, ioend;

	DBG(__func__);

	iobase = info->pci_dev->resource[1].start;
	ioend = info->pci_dev->resource[1].end;

	release_mem_region(iobase, ioend - iobase);

	/* induce segfault upon next access */
	info->iobase = NULL;
}

/*
 * Do everything to grab FB memory and then making sure that it is fully
 * accessible.
 */
static int
chrome_fb_init(struct chrome_info *info)
{
	unsigned int fbbase, fbend;

	DBG(__func__);

	fbbase = info->pci_dev->resource[0].start;
	fbend = info->pci_dev->resource[0].end;
	
	if (!request_mem_region(fbbase, fbend - fbbase, DRIVER_NAME)) {
		printk(KERN_ERR "%s: Cannot request FB resource.\n", __func__);
		return -ENODEV;
	}

	info->fbbase = ioremap_nocache(fbbase, 0x9000);
	if (!info->fbbase) {
		printk(KERN_ERR "%s: Unable to remap FB region.\n", __func__);
		release_mem_region(fbbase, fbend - fbbase);
		return -ENODEV;
	}

	/* enable writing to all VGA planes */
	chrome_vga_seq_write(info, 0x02, 0x0F);
	
	/* enable extended VGA memory */
	chrome_vga_seq_write(info, 0x04, 0x0E);

	/* enable extended memory access */
	chrome_vga_seq_mask(info, 0x1A, 0x08, 0x08);

	return 0;
}

/*
 * If we don't release the region when unloading, then there will be no
 * subsequent reloading possible.
 */
static void
chrome_fb_release(struct chrome_info *info)
{
	unsigned int fbbase, fbend;

	DBG(__func__);

	fbbase = info->pci_dev->resource[0].start;
	fbend = info->pci_dev->resource[0].end;

	release_mem_region(fbbase, fbend - fbbase);

	/* induce segfault upon next access */
	info->fbbase = NULL;
}

/*
 * Main initialisation routine.
 */
static int __devinit 
chrome_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	struct chrome_info *info;
	int err;

	DBG(__func__);

	err = pci_enable_device(dev);
	if (err)
		goto cleanup_err;

	info = chrome_alloc_fb_info(dev, id);
	if (!info)
		return -ENOMEM;

	/* Get, amongst others, FB Size straight from the RAM controller */
	via_ramctrl_info(info);
	if (!info->fbsize) {
		printk(KERN_ERR "%s: 0kB FB ram found. Exiting\n", __func__);
		goto cleanup_info;
	}

	/* Enable IO */
	err = chrome_io_init(info);
	if (err)
		goto cleanup_info;

	/* Claim FB */
	err = chrome_fb_init(info);
	if (err)
		goto cleanup_io;

	/* Attach */
	pci_set_drvdata(dev, &info->fb_info);
	return 0;

cleanup_fb:
	chrome_fb_release(info);
cleanup_io:
	chrome_io_release(info);
cleanup_info:
	kfree(info);
cleanup_err:
	return err;
}

/*
 *
 */
static void __devexit 
chrome_remove(struct pci_dev *dev)
{
    struct chrome_info *info = (struct chrome_info *) pci_get_drvdata(dev);

	DBG(__func__);

	if (info) {
		if (info->fbbase)
			chrome_fb_release(info);

		if (info->iobase)
			chrome_io_release(info);
		
		pci_set_drvdata(dev, NULL);
		kfree(info);
	}
}

static struct pci_driver chrome_driver = {
	.name =		DRIVER_NAME,
	.id_table =	chrome_devices,
	.probe =	chrome_probe,
	.remove =	__devexit_p(chrome_remove)
};


/* chrome_setup is not needed yet: no options to handle */

/*
 *
 */
static int __init 
chrome_init(void)
{
	DBG(__func__);

	return pci_register_driver(&chrome_driver);
}

module_init(chrome_init);

/*
 *
 */
#ifdef MODULE
static void __exit 
chrome_exit(void)
{
	pci_unregister_driver(&chrome_driver);
}

module_exit(chrome_exit);
#endif /* MODULE */
