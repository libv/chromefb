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

#ifndef HAVE_CHROMEFB_H
#define HAVE_CHROMEFB_H

/* Build in ""support"" for those devices we know we can't support? */
#if 0
#define CHROME_ENOHW 1
#else
#undef CHROME_ENOHW
#endif

#define DRIVER_NAME "chromefb"

/* Build in extra debug information */
#define DBG(x)  printk(KERN_DEBUG "chromefb: %s\n", (x));

/* All Unichrome and Chrome devices that this driver should support.
 * Introduce own, but only sensible, chip naming */
#define PCI_CHIP_VT3122 0x3122 /* CastleRock : on CLE266 */
#define PCI_CHIP_VT7205 0x7205 /* Unichrome : on KM400, KN400, P4M800 */
#define PCI_CHIP_VT3108 0x3108 /* Unichrome Pro B: on K8M800, K8N800 */
#define PCI_CHIP_VT3118 0x3118 /* Unichrome Pro A: on CN400, PM800, PM880 */
#define PCI_CHIP_VT3344 0x3344 /* Unichrome Pro: P4M800Pro, VN800, CN700 */
#define PCI_CHIP_VT3157 0x3157 /* Unichrome Pro: CX700, VX700 */
#define PCI_CHIP_VT3230 0x3230 /* Chrome 9: K8M890CE, K8N890CE */
#define PCI_CHIP_VT3343 0x3343 /* Unichrome Pro: P4M890 */
#define PCI_CHIP_VT3371 0x3371 /* Chrome 9: P4M900 */

/*
 * Stores the full textmode state.
 */
struct chrome_state {
    /* VGA registers + extensions */
    unsigned char CR[0xA2];
    unsigned char SR[0x1D];
    unsigned char GR[0x08];
    unsigned char AR[0x14];
    unsigned char Misc;

    /* all four 4 VGA FB planes (0xA0000) */
#define VGA_FB_PLANE_SIZE 64*1024
    unsigned char *planes;

    /* DAC */
    struct {
        unsigned char red;
        unsigned char green;
        unsigned char blue;
    } palette[0x100];
};

/*
 * Holds all our information.
 */
struct chrome_info {
    struct fb_info  fb_info;

    struct pci_dev  *pci_dev;

    unsigned int  id;

    void __iomem  *fbbase;
    unsigned int  fbsize;

    void __iomem  *iobase;

    atomic_t  fb_ref_count;

    struct chrome_state state;

#if 0
    struct list_head  *crtcs;
    struct list_head  *outputs;
#endif

};

#if 0
/*
 * Holds all outputs.
 */
struct chrome_output {
    struct list_head  node;

};
#endif

/* from via_ramctrl.c */
void via_ramctrl_info(struct chrome_info *info);

#endif /* HAVE_CHROMEFB_H */
