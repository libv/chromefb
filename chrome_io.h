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
#ifndef HAVE_CHROMEFB_IO_H
#define HAVE_CHROMEFB_IO_H

unsigned char chrome_vga_misc_read(struct chrome_info *info);
void chrome_vga_misc_write(struct chrome_info *info, unsigned char value);
void chrome_vga_misc_mask(struct chrome_info *info, unsigned char value,
                          unsigned char mask);

unsigned char chrome_vga_cr_read(struct chrome_info *info, unsigned char index);
void chrome_vga_cr_write(struct chrome_info *info, unsigned char index,
                         unsigned char value);
void chrome_vga_cr_mask(struct chrome_info *info, unsigned char index,
                        unsigned char value, unsigned char mask);

unsigned char chrome_vga_seq_read(struct chrome_info *info, unsigned char index);
void chrome_vga_seq_write(struct chrome_info *info, unsigned char index,
                         unsigned char value);
void chrome_vga_seq_mask(struct chrome_info *info, unsigned char index,
                        unsigned char value, unsigned char mask);

unsigned char chrome_vga_enable_read(struct chrome_info *info);
void chrome_vga_enable_write(struct chrome_info *info, unsigned char value);
void chrome_vga_enable_mask(struct chrome_info *info, unsigned char value,
                            unsigned char mask);

#endif /* HAVE_CHROMEFB_IO_H */
