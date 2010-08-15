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

/*
 * TODO: Add support for 8bpp, 12bpp and 15bpp modes.
 * NOTE: Modes 1bpp, 2bpp, 4bpp and 18bpp should be tested.
 */

#include "fb.h"

static inline void
fb_respect_angle(FB *fb, int x, int y, int *dx, int *dy)
{
	switch (fb->angle) {
	case 270:
		*dy = x;
		*dx = fb->height - y - 1;
		break;
	case 180:
		*dx = fb->width - x - 1;
		*dy = fb->height - y - 1;
		break;
	case 90:
		*dx = y;
		*dy = fb->width - x - 1;
		break;
	case 0:
	default:
		*dx = x;
		*dy = y;
		break;
	}
}

/**************************************************************************
 * Pixel plotting routines
 */
#ifdef USE_32BPP
static void
fb_plot_pixel_32bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy);
	offset = fb->backbuffer + ((oy * fb->real_width + ox) << 2);
	if (offset > (fb->backbuffer + fb->screensize - fb->byte_pp)) return;

	if (RGB == fb->rgbmode) {
		*(offset) = blue;
		*(offset + 2) = red;
	} else {
		*(offset) = red;
		*(offset + 2) = blue;
	}
	*(offset + 1) = green;
}
#endif

#ifdef USE_24BPP
static void
fb_plot_pixel_24bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy, r;

	fb_respect_angle(fb, x, y, &ox, &oy);
	r = oy * fb->real_width + ox;
	offset = fb->backbuffer + (r + (r << 1));
	if (offset > (fb->backbuffer + fb->screensize - fb->byte_pp)) return;

	if (RGB == fb->rgbmode) {
		*(offset) = blue;
		*(offset + 2) = red;
	} else {
		*(offset) = red;
		*(offset + 2) = blue;
	}
	*(offset + 1) = green;
}
#endif

#ifdef USE_18BPP
static void
fb_plot_pixel_18bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy, r;

	fb_respect_angle(fb, x, y, &ox, &oy);
	r = oy * fb->real_width + ox;
	offset = fb->backbuffer + (r + (r << 1));
	if (offset > (fb->backbuffer + fb->screensize - fb->byte_pp)) return;

	if (RGB == fb->rgbmode) {
			*(offset++)     = (blue >> 2) | ((green & 0x0C) << 4);
			*(offset++) = ((green & 0xF0) >> 4) | ((red & 0x3C) << 2);
			*(offset++) = (red & 0xC0) >> 6;
	} else {
			*(offset++)     = (red >> 2) | ((green & 0x0C) << 4);
			*(offset++) = ((green & 0xF0) >> 4) | ((blue & 0x3C) << 2);
			*(offset++) = (blue & 0xC0) >> 6;
	}
}
#endif

#ifdef USE_16BPP
static void
fb_plot_pixel_16bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy);
	offset = fb->backbuffer + ((oy * fb->real_width + ox) << 1);
	if (offset > (fb->backbuffer + fb->screensize - fb->byte_pp)) return;

	if (RGB == fb->rgbmode) {
		*(volatile uint16_t *) (offset)
			= ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
	} else {
		*(volatile uint16_t *) (offset)
			= ((blue >> 3) << 11) | ((green >> 2) << 5) | (red >> 3);
	}
}
#endif

#ifdef USE_4BPP
static void
fb_plot_pixel_4bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int off, ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy);
	off = (oy * fb->real_width + ox) << 2; /* Bit offset */
	oy = off >> 3; /* Target byte offset */

	offset = fb->backbuffer + oy;
	if (offset > (fb->backbuffer + fb->screensize)) return;

	ox = 4 - off + (oy << 3); /* 8 - bpp - off % 8 = bits num for left shift */
	*offset = (*offset & ~(0x0F << ox))
			| ( ((11*red + (green << 4) + 5*blue) >> 8) << ox );
}
#endif

#ifdef USE_2BPP
static void
fb_plot_pixel_2bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int off, ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy);
	off = (oy * fb->real_width + ox) << 1; /* Bit offset */
	oy = off >> 3; /* Target byte offset */

	offset = fb->backbuffer + oy;
	if (offset > (fb->backbuffer + fb->screensize)) return;

	ox = 6 - off + (oy << 3); /* 8 - bpp - off % 8 = bits num for left shift */
	*offset = (*offset & ~(3 << ox))
			| ( ((11*red + (green << 4) + 5*blue) >> 11) << ox );
}
#endif

#ifdef USE_1BPP
static void
fb_plot_pixel_1bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int off, ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy);
	off = oy * fb->real_width + ox; /* Bit offset */
	oy = off >> 3; /* Target byte offset */

	offset = fb->backbuffer + oy;
	if (offset > (fb->backbuffer + fb->screensize)) return;

	/* off - (obyte << 3) = off % 8;  8 - bpp - off % 8 = bits num for left shift */
	if (((11*red + (green << 4) + 5*blue) >> 5) >= 128)
		*offset = *offset | ( 1 << (7 - off + (oy << 3) ) );
	else
		*offset = *offset & (~( 1 << (7 - off + (oy << 3) ) ));
}
#endif

/**************************************************************************
 * Horizontal line drawing routines
 */
#ifdef USE_32BPP
static void
fb_draw_hline_32bpp(FB *fb, int x, int y, int length, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy;
	static uint32 color;

	fb_respect_angle(fb, x, y, &ox, &oy);
	offset = fb->backbuffer + ((oy * fb->real_width + ox) << 2);
	if (offset > (fb->backbuffer + fb->screensize - fb->byte_pp)) return;

	if (RGB == fb->rgbmode) {
		color = (uint32)blue << 16 | (uint32)green << 8 | (uint32)red;
	} else {
		color = (uint32)red << 16 | (uint32)green << 8 | (uint32)blue;
	}

	if (length > fb->width - x)
		oy = fb->width - x;
	else
		oy = length;

	switch(fb->angle) {
	case 90:
		ox = -fb->stride;
	case 270:
		ox = fb->stride;
		break;
	case 0:
		ox = fb->byte_pp;
	case 180:
		ox = -fb->byte_pp;
		break;
	}

	for(; oy > 0; oy--) {
		*(volatile uint32 *) offset = color;
		offset += ox;
	}
}
#endif

#ifdef USE_24BPP
static void
fb_draw_hline_24bpp(FB *fb, int x, int y, int length, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy, r;
	static uint8 c1, c3;

	fb_respect_angle(fb, x, y, &ox, &oy);
	r = oy * fb->real_width + ox;
	offset = fb->backbuffer + (r + (r << 1));
	if (offset > (fb->backbuffer + fb->screensize - fb->byte_pp)) return;

	if (RGB == fb->rgbmode) {
		c1 = blue;
		c3 = red;
	} else {
		c1 = red;
		c3 = blue;
	}

	if (length > fb->width - x)
		oy = fb->width - x;
	else
		oy = length;

	switch(fb->angle) {
	case 90:
		ox = -fb->stride;
	case 270:
		ox = fb->stride;
		break;
	case 0:
		ox = fb->byte_pp;
	case 180:
		ox = -fb->byte_pp;
		break;
	}

	for(; oy > 0; oy--) {
		*(offset) = c1;
		*(offset+1) = green;
		*(offset+2) = c3;
		offset += ox;
	}
}
#endif

#ifdef USE_18BPP
static void
fb_draw_hline_18bpp(FB *fb, int x, int y, int length, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy, r;
	static uint8 c1, c2, c3;

	fb_respect_angle(fb, x, y, &ox, &oy);
	r = oy * fb->real_width + ox;
	offset = fb->backbuffer + (r + (r << 1));
	if (offset > (fb->backbuffer + fb->screensize - fb->byte_pp)) return;

	if (RGB == fb->rgbmode) {
		c1 = (blue >> 2) | ((green & 0x0C) << 4);
		c2 = ((green & 0xF0) >> 4) | ((red & 0x3C) << 2);
		c3 = (red & 0xC0) >> 6;
	} else {
		c1 = (red >> 2) | ((green & 0x0C) << 4);
		c2 = ((green & 0xF0) >> 4) | ((blue & 0x3C) << 2);
		c3 = (blue & 0xC0) >> 6;
	}

	if (length > fb->width - x)
		oy = fb->width - x;
	else
		oy = length;

	switch(fb->angle) {
	case 90:
		ox = -fb->stride;
	case 270:
		ox = fb->stride;
		break;
	case 0:
		ox = fb->byte_pp;
	case 180:
		ox = -fb->byte_pp;
		break;
	}

	for(; oy > 0; oy--) {
		*(offset) = c1;
		*(offset+1) = c2;
		*(offset+2) = c3;
		offset += ox;
	}
}
#endif

#ifdef USE_16BPP
static void
fb_draw_hline_16bpp(FB *fb, int x, int y, int length, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy;
	static uint16 color;

	fb_respect_angle(fb, x, y, &ox, &oy);
	offset = fb->backbuffer + ((oy * fb->real_width + ox) << 1);
	if (offset > (fb->backbuffer + fb->screensize - fb->byte_pp)) return;

	if (RGB == fb->rgbmode) {
		color = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
	} else {
		color = ((blue >> 3) << 11) | ((green >> 2) << 5) | (red >> 3);
	}

	if (length > fb->width - x)
		oy = fb->width - x;
	else
		oy = length;

	switch(fb->angle) {
	case 90:
		ox = -fb->stride;
	case 270:
		ox = fb->stride;
		break;
	case 0:
		ox = fb->byte_pp;
	case 180:
		ox = -fb->byte_pp;
		break;
	}

	for(; oy > 0; oy--) {
		*(volatile uint16 *) offset = color;
		offset += ox;
	}
}
#endif

#ifdef USE_4BPP
/* FIXME: BROKEN for rotated fb */
static void
fb_draw_hline_4bpp(FB *fb, int x, int y, int length, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int off, ox, oy, len, mask, color;
	static int cdata[2];

	fb_respect_angle(fb, x, y, &ox, &oy);
	off = (oy * fb->real_width + ox) << 2; /* Bit offset */
	oy = off >> 3; /* Target byte offset */

	offset = fb->backbuffer + oy;
	if (offset > (fb->backbuffer + fb->screensize)) return;

	if (length > fb->width - ox)
		len = fb->width - ox;
	else
		len = length;

	ox = off + (oy << 3);
	mask = ~(0x0F << ox); /* 0 -> F0, 4 -> 0F */
	ox = ox >> 2; /* ox = ox/4 -> [0..1] */
	color = (11*red + (green << 4) + 5*blue) >> 8;
	cdata[0] = color << 4;
	cdata[1] = color;
	for(; len > 0; len--) {
		*offset = (*offset & mask) | cdata[ox];
		ox = ~ox & 1;
		mask = ~mask;
		if (!ox) ++offset;
	}
}
#endif

#ifdef USE_2BPP
/* FIXME: BROKEN for rotated fb */
static void
fb_draw_hline_2bpp(FB *fb, int x, int y, int length, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int off, ox, oy, len, color;
	static int cdata[4], mask[4];

	fb_respect_angle(fb, x, y, &ox, &oy);
	off = (oy * fb->real_width + ox) << 1; /* Bit offset */
	oy = off >> 3; /* Target byte offset */

	offset = fb->backbuffer + oy;
	if (offset > (fb->backbuffer + fb->screensize)) return;

	if (length > fb->width - ox)
		len = fb->width - ox;
	else
		len = length;

	ox = off + (oy << 3);
	ox = ox >> 1; /* ox = ox/2 -> [0..3] */
	color = (11*red + (green << 4) + 5*blue) >> 11;
	cdata[0] = color << 6;
	cdata[1] = color << 4;
	cdata[2] = color << 2;
	cdata[3] = color;
	mask[0] = 0b00111111;
	mask[1] = 0b11001111;
	mask[2] = 0b11110011;
	mask[3] = 0b11111100;
	for(; len > 0; len--) {
		*offset = (*offset & mask[ox]) | cdata[ox];
		if (++ox > 6) {
			ox = 0;
			++offset;
		}
	}
}
#endif

#ifdef USE_1BPP
/* FIXME: BROKEN for rotated fb */
static void
fb_draw_hline_1bpp(FB *fb, int x, int y, int length, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int off, ox, oy, len, color;

	fb_respect_angle(fb, x, y, &ox, &oy);
	off = oy * fb->real_width + ox; /* Bit offset */
	oy = off >> 3; /* Target byte offset */

	offset = fb->backbuffer + oy;
	if (offset > (fb->backbuffer + fb->screensize)) return;

	if (length > fb->width - ox)
		len = fb->width - ox;
	else
		len = length;

	ox = off + (oy << 3); /* [0-7] */
	color = 1 << (7 - ox);
	if ( ((11*red + (green << 4) + 5*blue) >> 5) >= 128 )
		for(; len > 0; len--) {
			*offset = *offset | color;
			color >>= 1;	/* shift right by 1 bit */
			if (++ox > 7) {
				ox = 0;
				++offset;
				color = 128;
			}
		}
	else
		for(; len > 0; len--) {
			*offset = *offset & ~color;
			color >>= 1;	/* shift right by 1 bit */
			if (++ox > 7) {
				ox = 0;
				++offset;
				color = 128;
			}
		}
}
#endif

/*
 * NOTE: klibc uses 8bit transfers that breaks image on tosa
 * So we will use own memcpy.
 * We can use uint32 transfers because of screen dimensions
 * are always even numbers: (W/2*2)*(H/2*2) = (W*H/4)*4
 */

void
fb_memcpy(char *src, char *dst, int length)
{
	static uint32 *s, *d;
	static int n;

	s = (uint32 *)src;
	d = (uint32 *)dst;
	n = length >> 2;

	while (n--) {
		*(d++) = *(s++);
	}
}

/* Move backbuffer contents to videomemory */
void fb_render(FB * fb)
{
	fb_memcpy(fb->backbuffer, fb->data, fb->screensize);
}

/* Save backbuffer contents to further usage */
char *fb_dump(FB * fb)
{
	char *dump;

	dump = malloc(fb->screensize);
	if (NULL == dump) return NULL;

	fb_memcpy(fb->backbuffer, dump, fb->screensize);
	return dump;
}

/* Restore saved backbuffer */
void fb_restore(FB * fb, char *dump)
{
	if (NULL == dump) return;
	fb_memcpy(dump, fb->backbuffer, fb->screensize);
}


void fb_destroy(FB * fb)
{
	if (fb->fd >= 0)
		close(fb->fd);
	if(fb->backbuffer)
		free(fb->backbuffer);
	free(fb);
}

int
attempt_to_change_pixel_format(FB * fb, struct fb_var_screeninfo *fb_var)
{
	/* By default the framebuffer driver may have set an oversized
	 * yres_virtual to support VT scrolling via the panning interface.
	 *
	 * We don't try and maintain this since it's more likely that we
	 * will fail to increase the bpp if the driver's pre allocated
	 * framebuffer isn't large enough.
	 */
	fb_var->yres_virtual = fb_var->yres;

	/* First try setting an 8,8,8,0 pixel format so we don't have to do
	 * any conversions while drawing. */

	fb_var->bits_per_pixel = 32;

	fb_var->red.offset = 0;
	fb_var->red.length = 8;

	fb_var->green.offset = 8;
	fb_var->green.length = 8;

	fb_var->blue.offset = 16;
	fb_var->blue.length = 8;

	fb_var->transp.offset = 0;
	fb_var->transp.length = 0;

	if (ioctl(fb->fd, FBIOPUT_VSCREENINFO, fb_var) == 0) {
		fprintf(stdout,
			"Switched to a 32 bpp 8,8,8 frame buffer\n");
		return 1;
	} else {
		fprintf(stderr,
			"Error, failed to switch to a 32 bpp 8,8,8 frame buffer\n");
	}

	/* Otherwise try a 16bpp 5,6,5 format */

	fb_var->bits_per_pixel = 16;

	fb_var->red.offset = 11;
	fb_var->red.length = 5;

	fb_var->green.offset = 5;
	fb_var->green.length = 6;

	fb_var->blue.offset = 0;
	fb_var->blue.length = 5;

	fb_var->transp.offset = 0;
	fb_var->transp.length = 0;

	if (ioctl(fb->fd, FBIOPUT_VSCREENINFO, fb_var) == 0) {
		fprintf(stdout,
			"Switched to a 16 bpp 5,6,5 frame buffer\n");
		return 1;
	} else {
		fprintf(stderr,
			"Error, failed to switch to a 16 bpp 5,6,5 frame buffer\n");
	}

	return 0;
}

#ifdef DEBUG
void print_fb(FB *fb)
{
	DPRINTF("Framebuffer structure");
	DPRINTF("Descriptor: %d\n", fb->fd);
	DPRINTF("Type: %d\n", fb->type);
	DPRINTF("Visual: %d\n", fb->visual);
	DPRINTF("Width: %d, height: %d\n", fb->width, fb->height);
	DPRINTF("Real width: %d, real height: %d\n", fb->real_width, fb->real_height);
	DPRINTF("BPP: %d\n", fb->bpp);
	DPRINTF("Stride: %d\n", fb->stride);

	DPRINTF("Screensize: %d\n", fb->screensize);
	DPRINTF("Angle: %d\n", fb->angle);

	DPRINTF("RGBmode: %d\n", fb->rgbmode);
	DPRINTF("Red offset: %d, red length: %d\n", fb->red_offset, fb->red_length);
	DPRINTF("Green offset: %d, green length: %d\n", fb->green_offset, fb->green_length);
	DPRINTF("Blue offset: %d, blue length: %d\n", fb->blue_offset, fb->blue_length);
}
#endif

FB *fb_new(int angle)
{
	struct fb_var_screeninfo fb_var;
	struct fb_fix_screeninfo fb_fix;
	int off;
	char *fbdev;

	FB *fb = NULL;

	fbdev = getenv("FBDEV");
	if (fbdev == NULL)
		fbdev = "/dev/fb0";

	if ((fb = malloc(sizeof(FB))) == NULL) {
		perror("Error no memory");
		goto fail;
	}

	memset(fb, 0, sizeof(FB));

	fb->fd = -1;

	if ((fb->fd = open(fbdev, O_RDWR)) < 0) {
		perror("Error opening /dev/fb0");
		goto fail;
	}

	if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb_var) == -1) {
		perror("Error getting variable framebuffer info");
		goto fail;
	}

	if (fb_var.bits_per_pixel != 1 && fb_var.bits_per_pixel != 2
		&& fb_var.bits_per_pixel < 16)
	{
		fprintf(stderr,
			"Error, no support currently for %i bpp frame buffers\n"
			"Trying to change pixel format...\n",
			fb_var.bits_per_pixel);
		if (!attempt_to_change_pixel_format(fb, &fb_var))
			goto fail;
	}
	if (ioctl (fb->fd, FBIOGET_VSCREENINFO, &fb_var) == -1)
	{
		perror ("Error getting variable framebuffer info (2)");
		goto fail;
	}

	/* NB: It looks like the fbdev concept of fixed vs variable screen info is
	 * broken. The line_length is part of the fixed info but it can be changed
	 * if you set a new pixel format. */
	if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb_fix) == -1) {
		perror("Error getting fixed framebuffer info");
		goto fail;
	}

	fb->real_width = fb->width = fb_var.xres;
	fb->real_height = fb->height = fb_var.yres;
	fb->bpp = fb_var.bits_per_pixel;
	fb->byte_pp = fb->bpp >> 3;
	fb->stride = fb_fix.line_length;
	fb->type = fb_fix.type;
	fb->visual = fb_fix.visual;

	fb->screensize = fb->stride * fb->height;
	fb->backbuffer = malloc(fb->screensize);

	fb->red_offset = fb_var.red.offset;
	fb->red_length = fb_var.red.length;
	fb->green_offset = fb_var.green.offset;
	fb->green_length = fb_var.green.length;
	fb->blue_offset = fb_var.blue.offset;
	fb->blue_length = fb_var.blue.length;

	fb->depth = fb->red_length + fb->green_length + fb->blue_length;
	if (18 != fb->depth) fb->depth = fb->bpp;	/* according to some info 18bpp is reported as 24bpp */

	if ((fb->red_offset > fb->green_offset) && (fb->green_offset > fb->blue_offset)) {
		fb->rgbmode = RGB;
	} else if ((fb->red_offset < fb->green_offset) && (fb->green_offset < fb->blue_offset)) {
		fb->rgbmode = BGR;
	} else {
		fb->rgbmode = GENERIC;
	}

	fb->base = (char *) mmap((caddr_t) NULL,
				 /*fb_fix.smem_len */
				 fb->stride * fb->height,
				 PROT_READ | PROT_WRITE,
				 MAP_SHARED, fb->fd, 0);

	if (fb->base == (char *) -1) {
		perror("Error cannot mmap framebuffer ");
		goto fail;
	}

	off =
	    (unsigned long) fb_fix.smem_start %
	    (unsigned long) getpagesize();

	fb->data = fb->base + off;
#if 0
	/* FIXME: No support for 8pp as yet  */
	if (visual == FB_VISUAL_PSEUDOCOLOR
	    || visual == FB_VISUAL_STATIC_PSEUDOCOLOR) {
		static struct fb_cmap cmap;

		cmap.start = 0;
		cmap.len = 16;
		cmap.red = saved_red;
		cmap.green = saved_green;
		cmap.blue = saved_blue;
		cmap.transp = NULL;

		ioctl(fb, FBIOGETCMAP, &cmap);
	}

	if (!status)
		atexit(bogl_done);
	status = 2;
#endif

	fb->angle = angle;

	switch (fb->angle) {
	case 270:
	case 90:
		fb->width = fb->real_height;
		fb->height = fb->real_width;
		break;
	case 180:
	case 0:
	default:
		break;
	}

	switch (fb->depth) {
#ifdef USE_32BPP
	case 32:
		fb->plot_pixel = fb_plot_pixel_32bpp;
		fb->draw_hline = fb_draw_hline_32bpp;
		break;
#endif
#ifdef USE_24BPP
	case 24:
		fb->plot_pixel = fb_plot_pixel_24bpp;
		fb->draw_hline = fb_draw_hline_24bpp;
		break;
#endif
#ifdef USE_18BPP
	case 18:
		fb->plot_pixel = fb_plot_pixel_18bpp;
		fb->draw_hline = fb_draw_hline_18bpp;
		break;
#endif
#ifdef USE_16BPP
	case 16:
		fb->plot_pixel = fb_plot_pixel_16bpp;
		fb->draw_hline = fb_draw_hline_16bpp;
		break;
#endif
#ifdef USE_4BPP
	case 4:
		fb->plot_pixel = fb_plot_pixel_4bpp;
		fb->draw_hline = fb_draw_hline_4bpp;
		break;
#endif
#ifdef USE_2BPP
	case 2:
		fb->plot_pixel = fb_plot_pixel_2bpp;
		fb->draw_hline = fb_draw_hline_2bpp;
		break;
#endif
#ifdef USE_1BPP
	case 1:
		fb->plot_pixel = fb_plot_pixel_1bpp;
		fb->draw_hline = fb_draw_hline_1bpp;
		break;
#endif
	default:
		/* We have no drawing functions for this mode ATM */
		DPRINTF("Sorry, your bpp (%d) and/or depth (%d) are not supported yet.\n", fb->bpp, fb->depth);
		fb_destroy(fb);
		return NULL;
		break;
	}

#ifdef DEBUG
	print_fb(fb);
#endif

	return fb;

fail:
	if (fb)
		fb_destroy(fb);

	return NULL;
}


/**************************************************************************
 * Graphic primitives
 */
void fb_plot_pixel(FB *fb, int x, int y, uint32 rgb)
{
	static uint8 r, g, b;

	xrgb2comp(rgb, &r, &g, &b);
	fb->plot_pixel(fb, x, y, r, g, b);
}


void fb_draw_hline(FB *fb, int x, int y, int length, uint32 rgb)
{
	static uint8 r, g, b;

	xrgb2comp(rgb, &r, &g, &b);
	fb->draw_hline(fb, x, y, length, r, g, b);
}


void fb_draw_rect(FB * fb, int x, int y, int width, int height,
		uint32 rgb)
{
	static int dy;
	static uint8 r, g, b;

	xrgb2comp(rgb, &r, &g, &b);
	for (dy = y; dy < y+height; dy++)
		fb->draw_hline(fb, x, dy, width, r, g, b);
}


void fb_draw_rounded_rect(FB * fb, int x, int y, int width, int height,
		uint32 rgb)
{
	static int dy;
	static uint8 r, g, b;

	if (height < 4) return;

	xrgb2comp(rgb, &r, &g, &b);

	/* Top rounded part */
	dy = y;
	fb->draw_hline(fb, x+2, dy++, width-4, r, g, b);
	fb->draw_hline(fb, x+1, dy++, width-2, r, g, b);

	for (; dy < y+height-2; dy++)
		fb->draw_hline(fb, x, dy, width, r, g, b);

	/* Bottom rounded part */
	fb->draw_hline(fb, x+1, dy++, width-2, r, g, b);
	fb->draw_hline(fb, x+2, dy++, width-4, r, g, b);
}


/* Font rendering code based on BOGL by Ben Pfaff */

static int font_glyph(const Font * font, char wc, u_int32_t ** bitmap)
{
	int mask = font->index_mask;
	int i;

	for (;;) {
		for (i = font->offset[wc & mask]; font->index[i]; i += 2) {
			if ((font->index[i] & ~mask) == (wc & ~mask)) {
				if (bitmap != NULL)
					*bitmap =
					    &font->content[font->
							   index[i + 1]];
				return font->index[i] & mask;
			}
		}
	}
	return 0;
}

/* Return text width and height in pixels. Will return 0,0 for empty text */
void fb_text_size(FB * fb, int *width, int *height, const Font * font,
		const char *text)
{
	char *c = (char *) text;
	int n, w, h, mw;

	n = strlenn(text);
	if (0 == n) {
		*width = 0;
		*height = 0;
		return;
	}

	h = font->height;
	mw = w = 0;

	for(;*c;c++){
		if (*c == '\n') {
			if (w > mw) mw = w;
			w = 0;
			h += font->height;
			continue;
		}

		w += font_glyph(font, *c, NULL);
	}

	*width = (w > mw) ? w : mw;
	*height = h;
}

void fb_draw_text(FB * fb, int x, int y, uint32 rgb,
		const Font * font, const char *text)
{
	int h, w, n, cx, cy, dx, dy;
	char *c = (char *) text;
	uint8 r, g, b;
	u_int32_t gl;

	xrgb2comp(rgb, &r, &g, &b);

	n = strlen(text);
	h = font->height;
	dx = x; dy = y;

	for(; *c;c++){
		u_int32_t *glyph = NULL;

		if (*c == '\n') {
			dy += h;
			dx = x;
			continue;
		}

		w = font_glyph(font, *c, &glyph);

		if (glyph == NULL)
			continue;

		for (cy = 0; cy < h; cy++) {
			gl = *glyph++;

			for (cx = 0; cx < w; cx++) {
				if (gl & 0x80000000)
					fb->plot_pixel(fb, dx + cx,
						      dy + cy, r, g, b);
				gl <<= 1;
			}
		}

		dx += w;
	}
}
