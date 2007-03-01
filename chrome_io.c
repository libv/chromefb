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
 * Very noisy debugging.
 */
/* #define IO_DEBUG_ENABLE 1 */
#ifdef IO_DEBUG_ENABLE
#define IO_DEBUG_WRITE(name, write) \
        printk(KERN_DEBUG "IO %s -> 0x%02X\n", (name), (write))
#define IO_DEBUG_MASK(name, read, write, mask) \
        printk(KERN_DEBUG "IO %s 0x%02X -> 0x%02X@0x%02X\n", (name), (read), (write), (mask))
#define IO_DEBUG_INDEX_WRITE(name, index, write) \
        printk(KERN_DEBUG "IO %s%02X -> 0x%02X\n", (name), (index), (write))
#define IO_DEBUG_INDEX_MASK(name, index, read, write, mask) \
        printk(KERN_DEBUG "IO %s%02X 0x%02X -> 0x%02X@0x%02X\n", (name), (index), (read), (write), (mask))
#else
#define IO_DEBUG_WRITE(name, write)
#define IO_DEBUG_MASK(name, read, write, mask)
#define IO_DEBUG_INDEX_WRITE(name, index, write)
#define IO_DEBUG_INDEX_MASK(name, index, read, write, mask)
#endif

/*
 * Remapped VGA access.
 */

#define CHROME_VGA_BASE               0x8000
#define CHROME_VGA_ATTR_INDEX         CHROME_VGA_BASE + 0x3C0
#define CHROME_VGA_ATTR_WRITE         CHROME_VGA_BASE + 0x3C0
#define CHROME_VGA_ATTR_READ          CHROME_VGA_BASE + 0x3C1
#define CHROME_VGA_STAT0              CHROME_VGA_BASE + 0x3C2
#define CHROME_VGA_MISC_WRITE         CHROME_VGA_BASE + 0x3C2
#define CHROME_VGA_ENABLE             CHROME_VGA_BASE + 0x3C3
#define CHROME_VGA_SEQ_INDEX          CHROME_VGA_BASE + 0x3C4
#define CHROME_VGA_SEQ_VALUE          CHROME_VGA_BASE + 0x3C5
#define CHROME_VGA_DAC_MASK           CHROME_VGA_BASE + 0x3C6
#define CHROME_VGA_DAC_READ_ADDRESS   CHROME_VGA_BASE + 0x3C7
#define CHROME_VGA_DAC_WRITE_ADDRESS  CHROME_VGA_BASE + 0x3C8
#define CHROME_VGA_DAC                CHROME_VGA_BASE + 0x3C9
#define CHROME_VGA_MISC_READ          CHROME_VGA_BASE + 0x3CC
#define CHROME_VGA_GRAPH_INDEX        CHROME_VGA_BASE + 0x3CE
#define CHROME_VGA_GRAPH_VALUE        CHROME_VGA_BASE + 0x3CF
#define CHROME_VGA_CR_INDEX           CHROME_VGA_BASE + 0x3D4
#define CHROME_VGA_CR_VALUE           CHROME_VGA_BASE + 0x3D5
#define CHROME_VGA_STAT1              CHROME_VGA_BASE + 0x3DA

/* Make code more imminently readable */
#define CHROME_VGA(info, offset) *((volatile unsigned char *) (info)->iobase + (offset))

/*
 * Misc register.
 */
unsigned char
chrome_vga_misc_read(struct chrome_info *info)
{
	return CHROME_VGA(info, CHROME_VGA_MISC_READ);
}

void
chrome_vga_misc_write(struct chrome_info *info, unsigned char value)
{
        IO_DEBUG_WRITE("Misc", value);

	CHROME_VGA(info, CHROME_VGA_MISC_WRITE) = value;
}

void
chrome_vga_misc_mask(struct chrome_info *info, unsigned char value,
                     unsigned char mask)
{
	unsigned char tmp = CHROME_VGA(info, CHROME_VGA_MISC_READ);

        IO_DEBUG_MASK("Misc", tmp, value, mask);

	tmp &= ~mask;
	tmp |= value & mask;

	CHROME_VGA(info, CHROME_VGA_MISC_WRITE) = tmp;
}

/*
 * CR registers.
 */
unsigned char
chrome_vga_cr_read(struct chrome_info *info, unsigned char index)
{
	CHROME_VGA(info, CHROME_VGA_CR_INDEX) = index;

	return CHROME_VGA(info, CHROME_VGA_CR_VALUE);
}

void
chrome_vga_cr_write(struct chrome_info *info, unsigned char index,
                    unsigned char value)
{
        IO_DEBUG_INDEX_WRITE("CR", index, value);

	CHROME_VGA(info, CHROME_VGA_CR_INDEX) = index;
	CHROME_VGA(info, CHROME_VGA_CR_VALUE) = value;
}

void
chrome_vga_cr_mask(struct chrome_info *info, unsigned char index,
                   unsigned char value, unsigned char mask)
{
	unsigned char tmp;

	CHROME_VGA(info, CHROME_VGA_CR_INDEX) = index;
	tmp = CHROME_VGA(info, CHROME_VGA_CR_VALUE);

        IO_DEBUG_INDEX_MASK("CR", index, tmp, value, mask);

	tmp &= ~mask;
	tmp |= value & mask;

	CHROME_VGA(info, CHROME_VGA_CR_VALUE) = tmp;
}

/*
 * Sequence registers.
 */
unsigned char
chrome_vga_seq_read(struct chrome_info *info, unsigned char index)
{
	CHROME_VGA(info, CHROME_VGA_SEQ_INDEX) = index;

	return CHROME_VGA(info, CHROME_VGA_SEQ_VALUE);
}

void
chrome_vga_seq_write(struct chrome_info *info, unsigned char index,
                     unsigned char value)
{
        IO_DEBUG_INDEX_WRITE("SR", index, value);

	CHROME_VGA(info, CHROME_VGA_SEQ_INDEX) = index;
	CHROME_VGA(info, CHROME_VGA_SEQ_VALUE) = value;
}

void
chrome_vga_seq_mask(struct chrome_info *info, unsigned char index,
                    unsigned char value, unsigned char mask)
{
	unsigned char tmp;

	CHROME_VGA(info, CHROME_VGA_SEQ_INDEX) = index;
	tmp = CHROME_VGA(info, CHROME_VGA_SEQ_VALUE);

        IO_DEBUG_INDEX_MASK("SR", index, tmp, value, mask);

	tmp &= ~mask;
	tmp |= value & mask;

	CHROME_VGA(info, CHROME_VGA_SEQ_VALUE) = tmp;
}

/*
 * VGA Enable register.
 */
unsigned char
chrome_vga_enable_read(struct chrome_info *info)
{
	return CHROME_VGA(info, CHROME_VGA_ENABLE);
}

void
chrome_vga_enable_write(struct chrome_info *info, unsigned char value)
{
        IO_DEBUG_WRITE("Enable", value);

	CHROME_VGA(info, CHROME_VGA_ENABLE) = value;
}

void
chrome_vga_enable_mask(struct chrome_info *info, unsigned char value,
                       unsigned char mask)
{
	unsigned char tmp = CHROME_VGA(info, CHROME_VGA_ENABLE);

        IO_DEBUG_MASK("Enable", tmp, value, mask);

	tmp &= ~mask;
	tmp |= value & mask;

	CHROME_VGA(info, CHROME_VGA_ENABLE) = tmp;
}

/*
 * Graphics registers.
 */
unsigned char
chrome_vga_graph_read(struct chrome_info *info, unsigned char index)
{
	CHROME_VGA(info, CHROME_VGA_GRAPH_INDEX) = index;

	return CHROME_VGA(info, CHROME_VGA_GRAPH_VALUE);
}

void
chrome_vga_graph_write(struct chrome_info *info, unsigned char index,
                       unsigned char value)
{
        IO_DEBUG_INDEX_WRITE("GR", index, value);

	CHROME_VGA(info, CHROME_VGA_GRAPH_INDEX) = index;
	CHROME_VGA(info, CHROME_VGA_GRAPH_VALUE) = value;
}

void
chrome_vga_graph_mask(struct chrome_info *info, unsigned char index,
                      unsigned char value, unsigned char mask)
{
	unsigned char tmp;

	CHROME_VGA(info, CHROME_VGA_GRAPH_INDEX) = index;
	tmp = CHROME_VGA(info, CHROME_VGA_GRAPH_VALUE);

        IO_DEBUG_INDEX_MASK("GR", index, tmp, value, mask);

	tmp &= ~mask;
	tmp |= value & mask;

	CHROME_VGA(info, CHROME_VGA_GRAPH_VALUE) = tmp;
}
/*
 * Attribute registers.
 */
unsigned char
chrome_vga_attr_read(struct chrome_info *info, unsigned char index)
{
        unsigned char stat, stored, ret;

        stat = CHROME_VGA(info, CHROME_VGA_STAT1);
        stored = CHROME_VGA(info, CHROME_VGA_ATTR_INDEX);

        stat = CHROME_VGA(info, CHROME_VGA_STAT1);

	CHROME_VGA(info, CHROME_VGA_ATTR_INDEX) = index;

	ret = CHROME_VGA(info, CHROME_VGA_ATTR_READ);


        stat = CHROME_VGA(info, CHROME_VGA_STAT1);
        CHROME_VGA(info, CHROME_VGA_ATTR_INDEX) = stored;

        return ret;
}

void
chrome_vga_attr_write(struct chrome_info *info, unsigned char index,
                      unsigned char value)
{
        unsigned char stat, stored;

        stat = CHROME_VGA(info, CHROME_VGA_STAT1);
        stored = CHROME_VGA(info, CHROME_VGA_ATTR_INDEX);

        stat = CHROME_VGA(info, CHROME_VGA_STAT1);
	CHROME_VGA(info, CHROME_VGA_ATTR_INDEX) = index;
	CHROME_VGA(info, CHROME_VGA_ATTR_WRITE) = value;

        stat = CHROME_VGA(info, CHROME_VGA_STAT1);
        CHROME_VGA(info, CHROME_VGA_ATTR_INDEX) = stored;
}

void
chrome_vga_attr_mask(struct chrome_info *info, unsigned char index,
                     unsigned char value, unsigned char mask)
{
	unsigned char stat, tmp, stored;

	stat = CHROME_VGA(info, CHROME_VGA_STAT1);
        stored = CHROME_VGA(info, CHROME_VGA_ATTR_INDEX);

        stat = CHROME_VGA(info, CHROME_VGA_STAT1);
        CHROME_VGA(info, CHROME_VGA_ATTR_INDEX) = index;
	tmp = CHROME_VGA(info, CHROME_VGA_ATTR_READ);

        IO_DEBUG_INDEX_MASK("ATTR", index, tmp, value, mask);

	tmp &= ~mask;
	tmp |= value & mask;

        stat = CHROME_VGA(info, CHROME_VGA_STAT1);
        CHROME_VGA(info, CHROME_VGA_ATTR_INDEX) = index;
	CHROME_VGA(info, CHROME_VGA_ATTR_WRITE) = tmp;

        stat = CHROME_VGA(info, CHROME_VGA_STAT1);
        CHROME_VGA(info, CHROME_VGA_ATTR_INDEX) = stored;
}

/*
 * DAC/Palette registers.
 */
void
chrome_vga_dac_mask_write(struct chrome_info *info, unsigned char value)
{
	CHROME_VGA(info, CHROME_VGA_DAC_MASK) = value;
}

void
chrome_vga_dac_read_address(struct chrome_info *info, unsigned char value)
{
	CHROME_VGA(info, CHROME_VGA_DAC_READ_ADDRESS) = value;
}

void
chrome_vga_dac_write_address(struct chrome_info *info, unsigned char value)
{
	CHROME_VGA(info, CHROME_VGA_DAC_WRITE_ADDRESS) = value;
}


void
chrome_vga_dac_write(struct chrome_info *info, unsigned char value)
{
	CHROME_VGA(info, CHROME_VGA_DAC) = value;
}

unsigned char
chrome_vga_dac_read(struct chrome_info *info)
{
	return CHROME_VGA(info, CHROME_VGA_DAC);
}
