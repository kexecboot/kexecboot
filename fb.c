/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2008-2011 Yuri Bushmelev <jay4mail@gmail.com>
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

#include <errno.h>

#include "config.h"
#include "fb.h"

static inline void
fb_respect_angle(FB *fb, int x, int y, int *dx, int *dy, int *nx)
{
/*	// Not sure about this check. IMHO programmer should do this check in his head
	if (x < 0 || x > fb->width - 1 || y < 0 || y > fb->height - 1) {
		*dx = 0;
		*dy = 0;
		if (nx) *nx = 0;
		return;
	}
*/
	switch (fb->angle) {
	case 270:
		*dy = x;
		*dx = fb->real_width - y - 1;
		if (nx) *nx = fb->stride;
		break;
	case 180:
		*dx = fb->real_width - x - 1;
		*dy = fb->real_height - y - 1;
		if (nx) *nx = -fb->byte_pp;
		break;
	case 90:
		*dx = y;
		*dy = fb->real_height - x - 1;
		if (nx) *nx = -fb->stride;
		break;
	case 0:
	default:
		*dx = x;
		*dy = y;
		if (nx) *nx = fb->byte_pp;
		break;
	}
/*	// Not sure about this check. IMHO programmer should do this check in his head
	if (*dx < 0 || *dx > fb->real_width - 1 || *dy < 0 || *dy > fb->real_height - 1) {
		*dx = 0;
		*dy = 0;
		if (nx) *nx = 0;
		return;
	}
*/
}

/**************************************************************************
 * Pixel plotting routines
 */
#ifdef USE_32BPP
static void
fb_plot_pixel_32bpp(FB * fb, int x, int y, kx_ccomp red, kx_ccomp green, kx_ccomp blue)
{
	static char *offset;
	static int ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy, NULL);
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
fb_plot_pixel_24bpp(FB * fb, int x, int y, kx_ccomp red, kx_ccomp green, kx_ccomp blue)
{
	static char *offset;
	static int ox, oy, r;

	fb_respect_angle(fb, x, y, &ox, &oy, NULL);
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
fb_plot_pixel_18bpp(FB * fb, int x, int y, kx_ccomp red, kx_ccomp green, kx_ccomp blue)
{
	static char *offset;
	static int ox, oy, r;

	fb_respect_angle(fb, x, y, &ox, &oy, NULL);
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
fb_plot_pixel_16bpp(FB * fb, int x, int y, kx_ccomp red, kx_ccomp green, kx_ccomp blue)
{
	static char *offset;
	static int ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy, NULL);
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
fb_plot_pixel_4bpp(FB * fb, int x, int y, kx_ccomp red, kx_ccomp green, kx_ccomp blue)
{
	static char *offset;
	static int off, ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy, NULL);
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
fb_plot_pixel_2bpp(FB * fb, int x, int y, kx_ccomp red, kx_ccomp green, kx_ccomp blue)
{
	static char *offset;
	static int off, ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy, NULL);
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
fb_plot_pixel_1bpp(FB * fb, int x, int y, kx_ccomp red, kx_ccomp green, kx_ccomp blue)
{
	static char *offset;
	static int off, ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy, NULL);
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
fb_draw_hline_32bpp(FB *fb, int x, int y, int length, kx_ccomp red, kx_ccomp green, kx_ccomp blue)
{
	static char *offset;
	static int ox, oy, nx;
	static uint32_t color;

	fb_respect_angle(fb, x, y, &ox, &oy, &nx);
	offset = fb->backbuffer + ((oy * fb->real_width + ox) << 2);
	if (offset > (fb->backbuffer + fb->screensize - fb->byte_pp)) return;

	if (RGB == fb->rgbmode) {
		color = (uint32_t)blue << 16 | (uint32_t)green << 8 | (uint32_t)red;
	} else {
		color = (uint32_t)red << 16 | (uint32_t)green << 8 | (uint32_t)blue;
	}

	if (length > fb->width - x)
		oy = fb->width - x;
	else
		oy = length;

	for(; oy > 0; oy--) {
		*(volatile uint32_t *) offset = color;
		offset += nx;
	}
}
#endif

#ifdef USE_24BPP
static void
fb_draw_hline_24bpp(FB *fb, int x, int y, int length, kx_ccomp red, kx_ccomp green, kx_ccomp blue)
{
	static char *offset;
	static int ox, oy, nx, r;
	static kx_ccomp c1, c3;

	fb_respect_angle(fb, x, y, &ox, &oy, &nx);
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

	for(; oy > 0; oy--) {
		*(offset) = c1;
		*(offset+1) = green;
		*(offset+2) = c3;
		offset += nx;
	}
}
#endif

#ifdef USE_18BPP
static void
fb_draw_hline_18bpp(FB *fb, int x, int y, int length, kx_ccomp red, kx_ccomp green, kx_ccomp blue)
{
	static char *offset;
	static int ox, oy, nx, r;
	static kx_ccomp c1, c2, c3;

	fb_respect_angle(fb, x, y, &ox, &oy, &nx);
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

	for(; oy > 0; oy--) {
		*(offset) = c1;
		*(offset+1) = c2;
		*(offset+2) = c3;
		offset += nx;
	}
}
#endif

#ifdef USE_16BPP
static void
fb_draw_hline_16bpp(FB *fb, int x, int y, int length, kx_ccomp red, kx_ccomp green, kx_ccomp blue)
{
	static char *offset;
	static int ox, oy, nx;
	static uint16_t color;

	fb_respect_angle(fb, x, y, &ox, &oy, &nx);
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

	for(; oy > 0; oy--) {
		*(volatile uint16_t *) offset = color;
		offset += nx;
	}
}
#endif

#ifdef USE_4BPP
/* FIXME: BROKEN for rotated fb */
static void
fb_draw_hline_4bpp(FB *fb, int x, int y, int length, kx_ccomp red, kx_ccomp green, kx_ccomp blue)
{
	static char *offset;
	static int off, ox, oy, len, mask, color;
	static int cdata[2];

	fb_respect_angle(fb, x, y, &ox, &oy, NULL);
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
fb_draw_hline_2bpp(FB *fb, int x, int y, int length, kx_ccomp red, kx_ccomp green, kx_ccomp blue)
{
	static char *offset;
	static int off, ox, oy, len, color;
	static int cdata[4], mask[4];

	fb_respect_angle(fb, x, y, &ox, &oy, NULL);
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
fb_draw_hline_1bpp(FB *fb, int x, int y, int length, kx_ccomp red, kx_ccomp green, kx_ccomp blue)
{
	static char *offset;
	static int off, ox, oy, len, color;

	fb_respect_angle(fb, x, y, &ox, &oy, NULL);
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
 * We can use uint32_t transfers because of screen dimensions
 * are always even numbers: (W/2*2)*(H/2*2) = (W*H/4)*4
 */

void
fb_memcpy(char *src, char *dst, int length)
{
	static USE_FB_TRANS_TYPE *s, *d;
	static int n;

	s = (USE_FB_TRANS_TYPE *)src;
	d = (USE_FB_TRANS_TYPE *)dst;
	n = USE_FB_TRANS_LENGTH(length);

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

	if ((ioctl(fb->fd, FBIOPUT_VSCREENINFO, fb_var) == 0) && (32 == fb_var->bits_per_pixel)) {
		log_msg(lg, "Switched to a 32bpp mode");
		return 1;
	} else {
		log_msg(lg, "Failed to switch to a 32bpp mode, trying 16bpp");
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

	if ((ioctl(fb->fd, FBIOPUT_VSCREENINFO, fb_var) == 0) && (16 == fb_var->bits_per_pixel)) {
		log_msg(lg, "Switched to a 16bpp mode");
		return 1;
	} else {
		log_msg(lg, "Failed to switch to a 16bpp mode, giving up");
	}

	return 0;
}

#ifdef DEBUG
void print_fb(FB *fb)
{
	log_msg(lg, "Framebuffer structure");
	log_msg(lg, "Descriptor: %d", fb->fd);
	log_msg(lg, "Type: %d", fb->type);
	log_msg(lg, "Visual: %d", fb->visual);
	log_msg(lg, "Width: %d, height: %d", fb->width, fb->height);
	log_msg(lg, "Real width: %d, real height: %d", fb->real_width, fb->real_height);
	log_msg(lg, "BPP: %d, depth: %d", fb->bpp, fb->depth);
	log_msg(lg, "Stride: %d", fb->stride);

	log_msg(lg, "Screensize: %d", fb->screensize);
	log_msg(lg, "Angle: %d", fb->angle);

	log_msg(lg, "RGBmode: %d", fb->rgbmode);
	log_msg(lg, "Red offset: %d, red length: %d", fb->red_offset, fb->red_length);
	log_msg(lg, "Green offset: %d, green length: %d", fb->green_offset, fb->green_length);
	log_msg(lg, "Blue offset: %d, blue length: %d", fb->blue_offset, fb->blue_length);
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
		log_msg(lg, "Error opening /dev/fb0: %s", ERRMSG);
		goto fail;
	}

	if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb_var) == -1) {
		log_msg(lg, "Error getting variable framebuffer info: %s", ERRMSG);
		goto fail;
	}

	if (fb_var.bits_per_pixel != 1 && fb_var.bits_per_pixel != 2
		&& fb_var.bits_per_pixel < 16)
	{
		log_msg(lg,
			"Error, no support currently for %i bpp frame buffers\n"
			"Trying to change pixel format...",
			fb_var.bits_per_pixel);
		if (!attempt_to_change_pixel_format(fb, &fb_var))
			goto fail;
	}
	if (ioctl (fb->fd, FBIOGET_VSCREENINFO, &fb_var) == -1)
	{
		log_msg(lg, "Error getting variable framebuffer info (2): %s", ERRMSG);
		goto fail;
	}

	/* NB: It looks like the fbdev concept of fixed vs variable screen info is
	 * broken. The line_length is part of the fixed info but it can be changed
	 * if you set a new pixel format. */
	if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb_fix) == -1) {
		log_msg(lg, "Error getting fixed framebuffer info: %s", ERRMSG);
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
		log_msg(lg, "Error cannot mmap framebuffer: %s", ERRMSG);
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

#ifdef DEBUG
	print_fb(fb);
#endif

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
		log_msg(lg, "Sorry, your bpp (%d) and/or depth (%d) are not supported yet", fb->bpp, fb->depth);
		fb_destroy(fb);
		return NULL;
		break;
	}

	return fb;

fail:
	if (fb)
		fb_destroy(fb);

	return NULL;
}


/**************************************************************************
 * Graphic primitives
 */
void fb_plot_pixel(FB *fb, int x, int y, kx_rgba rgba)
{
	kx_ccomp r, g, b, a;

	rgba2comp(rgba, &r, &g, &b, &a);
	fb->plot_pixel(fb, x, y, r, g, b);
}


void fb_draw_hline(FB *fb, int x, int y, int length, kx_rgba rgba)
{
	kx_ccomp r, g, b, a;

	rgba2comp(rgba, &r, &g, &b, &a);
	fb->draw_hline(fb, x, y, length, r, g, b);
}


void fb_draw_rect(FB * fb, int x, int y, int width, int height,
		kx_rgba rgba)
{
	static int dy;
	kx_ccomp r, g, b, a;

	rgba2comp(rgba, &r, &g, &b, &a);
	for (dy = y; dy < y+height; dy++)
		fb->draw_hline(fb, x, dy, width, r, g, b);
}


void fb_draw_rounded_rect(FB * fb, int x, int y, int width, int height,
		kx_rgba rgba)
{
	static int dy;
	kx_ccomp r, g, b, a;

	if (height < 4) return;

	rgba2comp(rgba, &r, &g, &b, &a);

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


int fb_draw_constrained_text(FB * fb, int x, int y,
		int max_x, int max_y, kx_rgba rgba,
		const Font * font, const char *text)
{
	int h, w, cx, cy, dx, dy;
	char *c = (char *) text;
	kx_ccomp r, g, b, a;
	u_int32_t gl;

	rgba2comp(rgba, &r, &g, &b, &a);

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

		/* Wrap by max width if any and if we are not on first char *
		if ( (max_x > 0) && (dx > x) && (dx + w > max_x) ) {
			dy += h;
			dx = x;
		}*/

		/* Stop if max height exceeded */
		if ( (max_y > 0) && (dy + h > max_y) ) {
			break;
		}

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

	return dy - y + h;
}


void fb_draw_text(FB * fb, int x, int y, kx_rgba rgba,
		const Font * font, const char *text)
{
	fb_draw_constrained_text(fb, x, y, 0, 0, rgba, font, text);
}


/* Draw picture on framebuffer */
void fb_draw_picture(FB * fb, int x, int y, kx_picture *pic)
{
	if (NULL == pic) return;

	unsigned int i, j;
	int dx = 0, dy = 0;
	kx_rgba *pixel, color;
	kx_ccomp r, g, b, a;

	pixel = pic->pixels;
	dy = y;
	for (i = 0; i < pic->height; i++) {
		dx = x;
		for (j = 0; j < pic->width; j++) {
			color = *pixel;
			rgba2comp(color, &r, &g, &b, &a);
			/* TODO Add transparency processing */
			/* Draw pixel if not fully transparent */
			if (255 != a) fb->plot_pixel(fb, dx, dy, r, g, b);
			++dx;
			++pixel;
		}
		++dy;
	}
}

/* Free picture's data structure */
void fb_destroy_picture(kx_picture* pic)
{
	if (NULL == pic) return;
	dispose(pic->pixels);
	free(pic);
}


