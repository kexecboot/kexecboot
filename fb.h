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
#include "rgb.h"

typedef struct FB *FBPTR;

typedef void (*plot_pixel_func)(FBPTR fb, int x, int y,
		uint8 red, uint8 green, uint8 blue);

typedef void (*draw_hline_func)(FBPTR fb, int x, int y, int length,
		uint8 red, uint8 green, uint8 blue);

typedef struct FB {
	int fd;
	int type;
	int visual;
	int width, height;
	int bpp;
	int depth;		/* Color depth to enable 18bpp mode */
	int byte_pp;	/* Byte per pixel, 0 for bpp < 8 */
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

	plot_pixel_func plot_pixel;
	draw_hline_func draw_hline;
} FB;

void fb_destroy(FB * fb);

FB *fb_new(int angle);

#ifdef DEBUG
void print_fb(FB *fb);
#endif

void
fb_draw_rect(FB * fb, int x, int y,
		int width, int height, uint32 rgb);

void
fb_draw_rounded_rect(FB * fb, int x, int y,
		int width, int height, uint32 rgb);

void
fb_text_size(FB * fb, int *width, int *height,
		const Font * font, const char *text);

void
fb_draw_text(FB * fb, int x, int y, uint32 rgb,
		const Font * font, const char *text);

void fb_render(FB * fb);

#endif	/* _HAVE_FB_H */
