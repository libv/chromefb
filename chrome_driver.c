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

#ifdef MODULE

MODULE_AUTHOR("(c) 2003-2006 by Luc Verhaegen (libv@skynet.be)");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FBDev driver for VIA Unichrome and Chrome IGPs");

#endif /* MODULE */

static struct pci_device_id chrome_devices[] = {
	{PCI_VENDOR_ID_VIA, 0x3122, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1},
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
