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
 * Contains handy abstractions of neccessary IO calls.
 */
#include <linux/fb.h>

#include "chrome.h"

/*
 * Remapped VGA access.
 */

#define CHROME_VGA_BASE        0x8000
#define CHROME_VGA_MISC_READ   CHROME_VGA_BASE + 0x3CC
#define CHROME_VGA_MISC_WRITE  CHROME_VGA_BASE + 0x3C2
#define CHROME_VGA_ENABLE      CHROME_VGA_BASE + 0x3C3
#define CHROME_VGA_SEQ_INDEX   CHROME_VGA_BASE + 0x3C4
#define CHROME_VGA_SEQ_VALUE   CHROME_VGA_BASE + 0x3C5
#define CHROME_VGA_CR_INDEX    CHROME_VGA_BASE + 0x3D4
#define CHROME_VGA_CR_VALUE    CHROME_VGA_BASE + 0x3D5

/*
 * Misc register.
 */
unsigned char 
chrome_vga_misc_read(struct chrome_info *info)
{
    return *((unsigned char *) info->iobase + CHROME_VGA_MISC_READ);
}

void 
chrome_vga_misc_write(struct chrome_info *info, unsigned char value)
{
    *((unsigned char *) info->iobase + CHROME_VGA_MISC_WRITE) = value;
}

void 
chrome_vga_misc_mask(struct chrome_info *info, unsigned char value,
                     unsigned char mask)
{
    unsigned char tmp = 
        *((unsigned char *) info->iobase + CHROME_VGA_MISC_READ);

    tmp &= ~mask;
    tmp |= value & mask;

    *((unsigned char *) info->iobase + CHROME_VGA_MISC_WRITE) = tmp;
}

/*
 * CR registers.
 */
unsigned char 
chrome_vga_cr_read(struct chrome_info *info, unsigned char index)
{
    *((unsigned char *) info->iobase + CHROME_VGA_CR_INDEX) = index;

    return *((unsigned char *) info->iobase + CHROME_VGA_CR_VALUE);
}

void 
chrome_vga_cr_write(struct chrome_info *info, unsigned char index,
                    unsigned char value)
{
    *((unsigned char *) info->iobase + CHROME_VGA_CR_INDEX) = index;
    *((unsigned char *) info->iobase + CHROME_VGA_CR_VALUE) = value;
}

void 
chrome_vga_cr_mask(struct chrome_info *info, unsigned char index,
                   unsigned char value, unsigned char mask)
{
    unsigned char tmp;

    *((unsigned char *) info->iobase + CHROME_VGA_CR_INDEX) = index;
    tmp = *((unsigned char *) info->iobase + CHROME_VGA_CR_VALUE);

    tmp &= ~mask;
    tmp |= value & mask;

    *((unsigned char *) info->iobase + CHROME_VGA_CR_VALUE) = tmp;
}

/*
 * Sequence registers.
 */
unsigned char 
chrome_vga_seq_read(struct chrome_info *info, unsigned char index)
{
    *((unsigned char *) info->iobase + CHROME_VGA_SEQ_INDEX) = index;

    return *((unsigned char *) info->iobase + CHROME_VGA_SEQ_VALUE);
}

void 
chrome_vga_seq_write(struct chrome_info *info, unsigned char index,
                     unsigned char value)
{
    *((unsigned char *) info->iobase + CHROME_VGA_SEQ_INDEX) = index;
    *((unsigned char *) info->iobase + CHROME_VGA_SEQ_VALUE) = value;
}

void 
chrome_vga_seq_mask(struct chrome_info *info, unsigned char index,
                    unsigned char value, unsigned char mask)
{
    unsigned char tmp;

    *((unsigned char *) info->iobase + CHROME_VGA_SEQ_INDEX) = index;
    tmp = *((unsigned char *) info->iobase + CHROME_VGA_SEQ_VALUE);

    tmp &= ~mask;
    tmp |= value & mask;

    *((unsigned char *) info->iobase + CHROME_VGA_SEQ_VALUE) = tmp;
}

/*
 *
 */
unsigned char 
chrome_vga_enable_read(struct chrome_info *info)
{
    return *((unsigned char *) info->iobase + CHROME_VGA_ENABLE);
}

void 
chrome_vga_enable_write(struct chrome_info *info, unsigned char value)
{
    *((unsigned char *) info->iobase + CHROME_VGA_ENABLE) = value;
}

void 
chrome_vga_enable_mask(struct chrome_info *info, unsigned char value,
                       unsigned char mask)
{
    unsigned char tmp = 
        *((unsigned char *) info->iobase + CHROME_VGA_ENABLE);

    tmp &= ~mask;
    tmp |= value & mask;

    *((unsigned char *) info->iobase + CHROME_VGA_ENABLE) = tmp;
}
