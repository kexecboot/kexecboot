/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2008-2010 Yuri Bushmelev <jay4mail@gmail.com>
 *  Copyright (c) 2008 Thomas Kunze <thommycheck@gmx.de>
 *  Copyright (c) 2006 Matthew Allum <mallum@o-hand.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef _HAVE_FB_H
#define _HAVE_FB_H

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "util.h"
#include "res/font.h"
#include "xpm.h"

enum RGBMode {
    RGB565,
    BGR565,
    RGB888,
    BGR888,
    GENERIC,
};

typedef struct FB {
	int fd;
	int type;
	int visual;
	int width, height;
	int bpp;
	int stride;
	char *data;
	char *backbuffer;
	char *base;

	int screensize;
	int angle;
	int real_width, real_height;

	enum RGBMode rgbmode;
	int red_offset;
	int red_length;
	int green_offset;
	int green_length;
	int blue_offset;
	int blue_length;
} FB;

void fb_destroy(FB * fb);

FB *fb_new(int angle);

inline void
fb_plot_pixel(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue);

void
fb_draw_rect(FB * fb,
	     int x,
	     int y,
	     int width, int height, uint8 red, uint8 green, uint8 blue);

#if 0
void
fb_draw_image(FB * fb,
	      int x,
	      int y,
	      int img_width,
	      int img_height, int img_bytes_pre_pixel, uint8 * rle_data);
#endif

void fb_draw_xpm_image(FB * fb, int x, int y, struct xpm_parsed_t *xpm_data);

void
fb_text_size(FB * fb,
	     int *width, int *height, const Font * font, const char *text);

void
fb_draw_text(FB * fb,
	     int x,
	     int y,
	     uint8 red,
	     uint8 green, uint8 blue, const Font * font, const char *text);

void fb_render(FB * fb);

#endif	/* _HAVE_FB_H */
