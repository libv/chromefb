/*
 * chromefb: driver for VIAs UniChrome and Chrome IGP graphics.
 *
 * Copyright (c) 2006-2007 by Luc Verhaegen (libv@skynet.be)
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
 * Adds support for VIAs Host bridges.
 *
 * This way, we don't depend on BIOS initialised scratch areas.
 *
 * Here we:
 *   - Get FB size.
 *   - Get RAM type.
 *   - Get physical FB offset.
 *   - Find out whether we have direct CPU Access.
 */

#include <linux/fb.h>
#include <linux/pci.h>

#include "chrome.h"

/*
 *
 */
static void
chrome_identify(struct chrome_info *info)
{
        struct {
                unsigned int id;
                char *name;
        } chromes[] = {
                { PCI_CHIP_VT3122, "VT3122 (CastleRock)"},
                { PCI_CHIP_VT7205, "VT7205 (UniChrome)"},
                { PCI_CHIP_VT3108, "VT3108 (UniChrome Pro)"},
                { 0, NULL}
        };

        struct {
                unsigned int id;
                char *name;
        } hosts[] = {
                { HOST_BRIDGE_CLE266, "CLE266"},
                { HOST_BRIDGE_KM400,  "KM400/KN400"},
                { HOST_BRIDGE_P4M800, "P4M800"},
                { HOST_BRIDGE_K8M800, "K8M800/K8N800"},
                { 0, NULL}
        };

        char *chipname, *hostname;
        int i;

        chipname = "Unknown";
        for (i = 0; chromes[i].name; i++)
                if (info->id == chromes[i].id) {
                        chipname = chromes[i].name;
                        break;
                }

        hostname = "Unknown";
        for (i = 0; hosts[i].name; i++)
                if (info->host == hosts[i].id) {
                        hostname = hosts[i].name;
                        break;
                }

        printk(KERN_INFO "Found %s on %s Host Bridge (rev. 0x%02X)\n",
               chipname, hostname, info->host_rev);
}

/*
 *
 */
static unsigned int
CLE266_ram_type(struct pci_dev *dev)
{
        u8 tmp, ddr;
        int freq = 0;

        DBG(__func__);

        pci_read_config_byte(dev, 0x54, &tmp);
        tmp >>= 6;
        switch (tmp) {
        case 0x01:
                freq = 3; /* x33.333Mhz = 100Mhz */
                break;
        case 0x02:
        case 0x03:
                freq = 4; /* 133Mhz */
                break;
        default:
                printk(KERN_ERR "%s: Illegal FSB frequency: 0x%02X\n",
                       __func__, tmp);
                return -1;
        }

        pci_read_config_byte(dev, 0x69, &tmp);
        tmp >>= 6;
        if (tmp & 0x02)
                freq--;
        else if (tmp & 0x01)
                freq++;

        if ((freq != 3) && (freq != 4)) {
                printk(KERN_ERR "%s: Illegal RAM frequency: 0x%02X\n",
                       __func__, freq);
                return -1;
        }

        pci_read_config_byte(dev, 0x60, &ddr);
        pci_read_config_byte(dev, 0xE3, &tmp);

        if (tmp & 0x02) /* FB is on banks 2/3 */
                ddr >>= 2;
        ddr &= 0x03;

        if (ddr == 0x02) { /* DDR */
                switch (freq) {
                case 3:
                        return RAM_TYPE_DDR200;
                case 4:
                        return RAM_TYPE_DDR266;
                default:
                        break;
                }
        }

        printk(KERN_ERR "%s: Unhandled RAM type: 0x%02X\n", __func__, ddr);
        return -1;
}

/*
 *
 */
static unsigned int
KM400_ram_type(struct chrome_info *info, struct pci_dev *dev)
{
        u8 fsb, ram, tmp;

        DBG(__func__);

        pci_read_config_byte(dev, 0x54, &fsb); /* FSB frequency */
        fsb >>= 6;

        pci_read_config_byte(dev, 0x69, &ram); /* FSB vs DRAM Clock */
        ram >>= 6;

        if (info->host_rev < 0x80) { /* KM400 */
                switch (fsb) { /* FSB Clock */
                case 0x00:
                        switch (ram) {
                        case 0x00:
                                return RAM_TYPE_DDR200;
                        case 0x01:
                                return RAM_TYPE_DDR266;
                        case 0x02:
                                return RAM_TYPE_DDR400;
                        case 0x03:
                                return RAM_TYPE_DDR333;
                        }
                        break;
                case 0x01:
                        switch (ram) {
                        case 0x00:
                                return RAM_TYPE_DDR266;
                        case 0x01:
                                return RAM_TYPE_DDR333;
                        case 0x02:
                                return RAM_TYPE_DDR400;
                        }
                        break;
                case 0x02:
                case 0x03: /* No 200Mhz FSB on KM400 */
                        switch (ram) {
                        case 0x00:
                                return RAM_TYPE_DDR333;
                        case 0x02:
                                return RAM_TYPE_DDR400;
                        case 0x03:
                                return RAM_TYPE_DDR266;
                        }
                        break;
                }
        } else { /* KM400A */
                pci_read_config_byte(dev, 0x67, &tmp); /* No idea */
                if (tmp & 0x80)
                        ram |= 0x04;

                switch (fsb) {
                case 0x00:
                        switch (ram) {
                        case 0x00:
                                return RAM_TYPE_DDR200;
                        case 0x01:
                                return RAM_TYPE_DDR266;
                        case 0x03:
                                return RAM_TYPE_DDR333;
                        case 0x07:
                                return RAM_TYPE_DDR400;
                        }
                        break;
                case 0x01:
                        switch (ram) {
                        case 0x00:
                                return RAM_TYPE_DDR266;
                        case 0x01:
                                return RAM_TYPE_DDR333;
                        case 0x03:
                                return RAM_TYPE_DDR400;
                        }
                        break;
                case 0x02:
                        switch (ram) {
                        case 0x00:
                                return RAM_TYPE_DDR400;
                        case 0x04:
                                return RAM_TYPE_DDR333;
                        case 0x06:
                                return RAM_TYPE_DDR266;
                        }
                        break;
                case 0x03:
                        switch (ram) {
                        case 0x00:
                                return RAM_TYPE_DDR333;
                        case 0x01:
                                return RAM_TYPE_DDR400;
                        case 0x04:
                                return RAM_TYPE_DDR266;
                        }
                        break;
                }
        }

        printk(KERN_ERR "%s: Illegal RAM type: FSB %1d, RAM, %1d\n",
               __func__, fsb, ram);
        return -1;
}

/*
 *
 */
static unsigned int
P4M800_ram_type(struct pci_dev *ram)
{
        u8 fsb, fsb_to_ram;
        struct pci_dev *dev;
        int freq;

        DBG(__func__);

        /* 0x4296 */
        dev = pci_find_slot(0, 4);
        pci_read_config_byte(dev, 0xF3, &fsb); /* VIA scratch area */
        switch(fsb >> 5) {
        case 0:
                freq = 3; /* x33Mhz  = 100Mhz */
                break;
        case 1:
                freq = 4; /* 133Mhz */
                break;
        case 3:
                freq = 5; /* 166Mhz */
                break;
        case 2:
                freq = 6; /* 200Mhz */
                break;
        case 4:
                freq = 7; /* 233Mhz */
                break;
        default:
                printk(KERN_ERR "%s: Unhandled FSB: %d\n", __func__, fsb);
                return -1;
        }

        /* FSB to RAM timing from 0x3296 */
        pci_read_config_byte(ram, 0x68, &fsb_to_ram);
        fsb_to_ram &= 0x0F;

        if (fsb_to_ram & 0x02)
                freq -= fsb_to_ram >> 2;
        else {
                freq += fsb_to_ram >> 2;
                if (fsb_to_ram & 0x01)
                        freq++;
        }

        switch (freq) {
        case 0x03:
                return RAM_TYPE_DDR200;
        case 0x04:
                return RAM_TYPE_DDR266;
        case 0x05:
                return RAM_TYPE_DDR333;
        case 0x06:
                return RAM_TYPE_DDR400;
        default:
                break;
        }

        printk(KERN_ERR "%s: Illegal RAM type: FSB %1d, FSBtoRAM, %1d (%d)\n",
               __func__, fsb, fsb_to_ram, freq);
        return -1;
}

/*
 * Pokes the K8 DRAM controller directly.
 */
static unsigned int
K8M800_ram_type(void)
{
        struct pci_dev *dev;
        unsigned char tmp;

        DBG(__func__);

        /* AMD K8 DRAM Controller */
        dev = pci_get_device(0x1022, 0x1102, NULL);
        if (!dev)
                return -1;

        pci_read_config_byte(dev, 0x96, &tmp);
        tmp = (tmp >> 4) & 0x07;
        switch(tmp) {
        case 0x00:
                return RAM_TYPE_DDR200;
        case 0x02:
                return RAM_TYPE_DDR266;
        case 0x05:
                return RAM_TYPE_DDR333;
        case 0x07:
                return RAM_TYPE_DDR400;
        default:
                printk(KERN_ERR "%s: Unhandled RAM Type: 0x%02X.\n",
                       __func__, tmp);
                return -1;
        }
}

/*
 *
 */
static char *
chrome_ram_type_string(unsigned int ram_type)
{
        struct {
                unsigned int type;
                char *string;
        } ram[] = {
                { RAM_TYPE_DDR200, "100Mhz DDR - PC1600"},
                { RAM_TYPE_DDR266, "133Mhz DDR - PC2100"},
                { RAM_TYPE_DDR333, "166Mhz DDR - PC2700"},
                { RAM_TYPE_DDR400, "200Mhz DDR - PC3200"},
                { 0, NULL},
        };
        int i;

        for (i = 0; ram[i].string; i++)
                if (ram_type == ram[i].type)
                        return ram[i].string;

        return "Unknown";
}

/*
 * Fully identify the host bridge.
 * Send out complete message about device found.
 * Hand back the pci_dev of the ramcontroller, so that subsequent functions
 * can use this directly.
 */
int
chrome_host(struct chrome_info *info)
{
        struct pci_dev *host, *ram;
        u8 tmp;
        u16 tmp16;
        int direct;

        DBG(__func__);

        host = pci_find_slot(0, 0);
        if (host->vendor != 0x1106) {
                printk(KERN_ERR
                       "%s: this is not a VIA Host Bridge: 0x%04X:0x%04X.\n",
                       __func__, host->vendor, host->device);
                return -1;
        }

        /* Bail on unknown hostbridges */
        switch (host->device) {
        case HOST_BRIDGE_CLE266:
        case HOST_BRIDGE_KM400:
        case HOST_BRIDGE_P4M800:
        case HOST_BRIDGE_K8M800:
                break;
        default:
                printk(KERN_ERR
                       "%s: unhandled VIA Host Bridge: 0x%04X:0x%04X.\n",
                       __func__, host->vendor, host->device);
                return -1;
        }

        info->host = host->device;
        pci_read_config_byte(host, 0xF6, &tmp);
        info->host_rev = tmp;

        chrome_identify(info);

        /* Get FB Size */
        switch (info->host) {
        case HOST_BRIDGE_CLE266:
        case HOST_BRIDGE_KM400:
                ram = host;
                pci_read_config_byte(ram, 0xE1, &tmp);
                break;
        default:
                ram = pci_find_slot(0, 3);
                pci_read_config_byte(ram, 0xA1, &tmp);
                break;
        }
        info->fbsize = (1 << ((tmp & 0x70) >> 4)) * 1024;
        if (!info->fbsize) {
                printk(KERN_ERR
                       "%s: RAM Controller reserves no memory for FB.\n",
                       __func__);
                return -1;
        }
        host = NULL;

        /* Get RAM type */
        switch (info->host) {
        case HOST_BRIDGE_CLE266:
                info->ram_type = CLE266_ram_type(ram);
                break;
        case HOST_BRIDGE_KM400:
                info->ram_type = KM400_ram_type(info, ram);
                break;
        case HOST_BRIDGE_P4M800:
                info->ram_type = P4M800_ram_type(ram);
                break;
        case HOST_BRIDGE_K8M800:
                info->ram_type = K8M800_ram_type();
                break;
        default:
                break;
        }

        /* Get FB Base, and check if we have direct access. */
        direct = 0;
        info->fb_physical = info->pci_dev->resource[0].start;
        switch (info->host) {
        case HOST_BRIDGE_CLE266:
        case HOST_BRIDGE_KM400:
                pci_read_config_word(ram, 0xE0, &tmp16);
                if ((tmp16 & 0x0001) && (tmp16 & 0x0FFE)) {
                        direct = 1;
                        info->fb_physical = (tmp16 & 0xFFE) << 20;
                }
                break;
        case HOST_BRIDGE_K8M800:
                /* CPU <-> FB rocks here, Chrome <-> FB sucks. */
                pci_read_config_byte(ram, 0x47, &tmp); /* Get real RAM size */
                info->fb_physical = (tmp << 24) - (info->fbsize << 10);
                direct = 1;
                break;
        default:
                pci_read_config_word(ram, 0xA0, &tmp16);
                if ((tmp16 & 0x0001) && (tmp16 & 0x0FFE)) {
                        direct = 1;
                        info->fb_physical = (tmp16 & 0xFFE) << 20;
                }
                break;
        }

        printk(KERN_INFO "Found %dkB FB (%s) at 0x%08X%s\n", info->fbsize,
               chrome_ram_type_string(info->ram_type), info->fb_physical,
               (direct ? " (Direct CPU Access)" : ""));

        return 0;
}
