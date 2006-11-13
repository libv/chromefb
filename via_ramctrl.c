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
 * Adds support for VIAs ram controllers, since we can't trust BIOSes.
 *   - Set up direct access range.
 *   - Get FB size.
 *   - Get RAM timing.
 */

#include <linux/fb.h>
#include <linux/pci.h>

#include "chrome.h"

/*
 *
 */
static void
via_ramctrl_direct_enable(struct pci_dev *fbdev, struct pci_dev *dev, int where)
{
	unsigned int tmp;
	int error;

	DBG(__func__);

	printk(KERN_INFO "Enabling Direct FB access.\n");

	error = pci_read_config_dword(dev, (where & 0xFC), &tmp);
	if (error) {
		printk(KERN_ERR "%s: read failed: 0x%X\n", __func__, error);
		return;
	}

	/* Set Address */
	tmp &= 0xFFFFF001;
	tmp |= ((fbdev->resource[0].start >> 20) & 0xFFE);

	/* Enable */
	tmp |= 0x01;

	error = pci_write_config_dword(dev, (where & 0xFC), tmp);
	if (error) {
		printk(KERN_ERR "%s: read failed: 0x%X\n", __func__, error);
		return;
	}
}

#ifdef UNUSED
/*
 *
 */
static void
via_ramctrl_direct_disable(struct pci_dev *dev, int where)
{
	unsigned char tmp;
	int error;

	DBG(__func__);

	printk(KERN_INFO "Disabling Direct FB access.\n");
    
	error = pci_read_config_byte(dev, (where & 0xFC), &tmp);
	if (error) {
		printk(KERN_ERR "%s: read failed: 0x%X\n", __func__, error);
		return;
	}

	error = pci_write_config_byte(dev, (where & 0xFC), tmp & 0xFE);
	if (error) {
		printk(KERN_ERR "%s: read failed: 0x%X\n", __func__, error);
		return;
	}
}
#endif

/*
 *
 */
static int
via_ramctrl_fb_size(struct pci_dev *dev, int where)
{
	unsigned char tmp;
	int error;

	DBG(__func__);

	error = pci_read_config_byte(dev, (where & 0xFC) | 0x01, &tmp);
	if (error) {
		printk(KERN_ERR "%s: read failed: 0x%X\n", __func__, error);
		return 0;
	}

	return (1 << ((tmp & 0x70) >> 4)) * 1024;
}

#if 0
/*
 *
 */
static unsigned char
via_ramctrl_ram_type_CLE266(struct pci_dev *dev)
{
	DBG(__func__);
}
#endif

/*
 * Here we grab the pci_dev of the ramcontroller of the northbridge.
 * We don't need to bother with waking it, it should be there already.
 */
void
via_ramctrl_info(struct chrome_info *info)
{
	struct pci_dev *dev;

	DBG(__func__);

	switch (info->id) {
	case PCI_CHIP_VT3122:
		dev = pci_find_device(PCI_VENDOR_ID_VIA, 0x3123, NULL);
		if (!dev) {
			printk(KERN_ERR "%s: RamController not found.\n",
			       __func__);
			return;
		}

		info->fbsize = via_ramctrl_fb_size(dev, 0xE0);

		via_ramctrl_direct_enable(info->pci_dev, dev, 0xE0);

		/* info->ramtype = via_ramctrl_ram_type_CLE266(dev); */

		break;
	case PCI_CHIP_VT7205:
		dev = pci_find_device(PCI_VENDOR_ID_VIA, 0x3205, NULL);
		if (!dev) {
			printk(KERN_ERR "%s: RamController not found.\n",
			       __func__);
			return;
		}

		info->fbsize = via_ramctrl_fb_size(dev, 0xE0);

		via_ramctrl_direct_enable(info->pci_dev, dev, 0xE0);

		break;
	case PCI_CHIP_VT3108:
		/* We don't do direct access: ram sits off the CPU */
	default:
		printk(KERN_ERR "%s: unhandled chip 0x%04X\n", __func__,
		       info->id);
		break;
	}

}
