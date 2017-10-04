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

#include "config.h"

#ifdef USE_FBMENU
#include <errno.h>

#include "fb.h"


static unsigned int compose_color (kx_rgba rgba) {

	kx_ccomp r, g, b, a;
	kx_ccomp c1, c2, c3;
	kx_rgba color;

	rgba2comp(rgba, &r, &g, &b, &a);

	switch (fb.bpp) {

		case 16:
			if (RGB == fb.rgbmode)
				color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
			else
				color = ((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3);
			break;

		case 18:
			if (RGB == fb.rgbmode) {

				c1 = (r >> 2) | ((g & 0x0C) << 4);
				c2 = ((g & 0xF0) >> 4) | ((b & 0x3C) << 2);
				c3 = (b & 0xC0) >> 6;
			}
			else {
				c1 = (b >> 2) | ((g & 0x0C) << 4);
			        c2 = ((g & 0xF0) >> 4) | ((r & 0x3C) << 2);
				c3 = (r & 0xC0) >> 6;
			}

			color = (uint32_t)c1 << 16 | (uint32_t)c2 << 8 | (uint32_t)c3;
			break;
		case 24:
		case 32:
			if (RGB == fb.rgbmode)
				color = (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
			else
				color = (uint32_t)b << 16 | (uint32_t)g << 8 | (uint32_t)r;
			break;
		default:
			color = rgba;
			break;
	}

	/* add alpha channel */
	color = (a << 24) | color;

	return color;
}

static inline void
fb_respect_angle(int x, int y, int *dx, int *dy, int *nx)
{
/*	// Not sure about this check. IMHO programmer should do this check in his head
	if (x < 0 || x > fb.width - 1 || y < 0 || y > fb.height - 1) {
		*dx = 0;
		*dy = 0;
		if (nx) *nx = 0;
		return;
	}
*/
	switch (fb.angle) {
	case 270:
		*dy = x;
		*dx = fb.real_width - y - 1;
		if (nx) *nx = fb.stride;
		break;
	case 180:
		*dx = fb.real_width - x - 1;
		*dy = fb.real_height - y - 1;
		if (nx) *nx = -fb.byte_pp;
		break;
	case 90:
		*dx = y;
		*dy = fb.real_height - x - 1;
		if (nx) *nx = -fb.stride;
		break;
	case 0:
	default:
		*dx = x;
		*dy = y;
		if (nx) *nx = fb.byte_pp;
		break;
	}
/*	// Not sure about this check. IMHO programmer should do this check in his head
	if (*dx < 0 || *dx > fb.real_width - 1 || *dy < 0 || *dy > fb.real_height - 1) {
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
fb_plot_pixel_32bpp(int x, int y, kx_rgba color)
{
	static char *offset;
	static int ox, oy;

	fb_respect_angle(x, y, &ox, &oy, NULL);
	offset = fb.backbuffer + oy * fb.stride + (ox << 2);
	if (offset > (fb.backbuffer + fb.screensize - fb.byte_pp)) return;

	*(volatile uint32_t *) offset = (uint32_t) color;
}
#endif

#ifdef USE_24BPP
static void
fb_plot_pixel_24bpp(int x, int y, kx_rgba color)
{
	static char *offset;
	static int ox, oy;

	fb_respect_angle(x, y, &ox, &oy, NULL);
	offset = fb.backbuffer + oy * fb.stride + (ox + (ox << 1));
	if (offset > (fb.backbuffer + fb.screensize - fb.byte_pp)) return;

	*(volatile char *) (offset) = (color & 0x000000FF);
	*(volatile char *) (offset + 1) = (color & 0x0000FF00) >> 8;
	*(volatile char *) (offset + 2) = (color & 0x00FF0000) >> 16;
}
#endif

#ifdef USE_18BPP
static void
fb_plot_pixel_18bpp(int x, int y, kx_rgba color)
{
	static char *offset;
	static int ox, oy;

	fb_respect_angle(x, y, &ox, &oy, NULL);
	offset = fb.backbuffer + oy * fb.stride + (ox + (ox << 1));
	if (offset > (fb.backbuffer + fb.screensize - fb.byte_pp)) return;

	*(volatile char *) (offset) = (color & 0x000000FF);
	*(volatile char *) (offset + 1) = (color & 0x0000FF00) >> 8;
	*(volatile char *) (offset + 2) = (color & 0x00FF0000) >> 16;
}
#endif

#ifdef USE_16BPP
static void
fb_plot_pixel_16bpp(int x, int y, kx_rgba color)
{
	static char *offset;
	static int ox, oy;

	fb_respect_angle(x, y, &ox, &oy, NULL);
	offset = fb.backbuffer + oy * fb.stride + (ox << 1);
	if (offset > (fb.backbuffer + fb.screensize - fb.byte_pp)) return;

	*(volatile uint16_t *) offset = (uint16_t) color;
}
#endif

/**************************************************************************
 * Horizontal line drawing routines
 */
#ifdef USE_32BPP
static void
fb_draw_hline_32bpp(int x, int y, int length, kx_rgba color)
{
	static char *offset;
	static int ox, oy, nx;

	fb_respect_angle(x, y, &ox, &oy, &nx);
	offset = fb.backbuffer + oy * fb.stride + (ox << 2);
	if (offset > (fb.backbuffer + fb.screensize - fb.byte_pp)) return;

	if (length > fb.width - x)
		oy = fb.width - x;
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
fb_draw_hline_24bpp(int x, int y, int length, kx_rgba color)
{
	static char *offset;
	static int ox, oy, nx;

	fb_respect_angle(x, y, &ox, &oy, &nx);
	offset = fb.backbuffer + oy * fb.stride + (ox + (ox << 1));
	if (offset > (fb.backbuffer + fb.screensize - fb.byte_pp)) return;

	if (length > fb.width - x)
		oy = fb.width - x;
	else
		oy = length;

	for(; oy > 0; oy--) {
		*(volatile char *) (offset) = (color & 0x000000FF);
		*(volatile char *) (offset + 1) = (color & 0x0000FF00) >> 8;
		*(volatile char *) (offset + 2) = (color & 0x00FF0000) >> 16;
		offset += nx;
	}
}
#endif

#ifdef USE_18BPP
static void
fb_draw_hline_18bpp(int x, int y, int length, kx_rgba color)
{
	static char *offset;
	static int ox, oy, nx;

	fb_respect_angle(x, y, &ox, &oy, &nx);
	offset = fb.backbuffer + oy * fb.stride + (ox + (ox << 1));
	if (offset > (fb.backbuffer + fb.screensize - fb.byte_pp)) return;

	if (length > fb.width - x)
		oy = fb.width - x;
	else
		oy = length;

	for(; oy > 0; oy--) {
		*(volatile char *) (offset) = (color & 0x000000FF);
		*(volatile char *) (offset + 1) = (color & 0x0000FF00) >> 8;
		*(volatile char *) (offset + 2) = (color & 0x00FF0000) >> 16;
		offset += nx;
	}
}
#endif

#ifdef USE_16BPP
static void
fb_draw_hline_16bpp(int x, int y, int length, kx_rgba color)
{
	static char *offset;
	static int ox, oy, nx;

	fb_respect_angle(x, y, &ox, &oy, &nx);
	offset = fb.backbuffer + oy * fb.stride + (ox << 1);
	if (offset > (fb.backbuffer + fb.screensize - fb.byte_pp)) return;

	if (length > fb.width - x)
		oy = fb.width - x;
	else
		oy = length;

	for(; oy > 0; oy--) {
		*(volatile uint16_t *) offset = (uint16_t) color;
		offset += nx;
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

#ifdef USE_FBUI_UPDATE

/*
 * Many phones use command mode LCDs to save power where the LCD panel needs to
 * be manually refreshed. Kernel can do that for us in some cases if configured
 * so, but at least the Motorola phones like droid 4 has the signed stock kernel
 * locked into command mode only. We can check this and enable the manual update
 * mode if needed.
 */
static int fb_quirk_check_manual_update(void)
{
	enum omapfb_update_mode update_mode;
	int error;

	if (strncmp("omapfb", fb.id, 5))
		return 0;

	error = ioctl(fb.fd, OMAPFB_GET_UPDATE_MODE, &update_mode);
	if (error < 0)
		return 0;

	if (update_mode == OMAPFB_MANUAL_UPDATE)
		return 1;

	return 0;
}

/* Flush command mode LCD if needed */
static void fb_quirk_manual_update(void)
{
	struct omapfb_update_window uw;

	if (!fb.needs_manual_update)
		return;

	uw.x = 0;
	uw.y = 0;
	uw.width = fb.real_width;
	uw.height = fb.real_height;

	ioctl(fb.fd, OMAPFB_UPDATE_WINDOW, &uw);
	ioctl(fb.fd, OMAPFB_SYNC_GFX);
}

#else
static inline int fb_quirk_check_manual_update(void)
{
	return 0;
}
static inline void fb_quirk_manual_update(void)
{
}
#endif

/* Move backbuffer contents to videomemory */
void fb_render()
{
	fb_memcpy(fb.backbuffer, fb.data, fb.screensize);
	fb_quirk_manual_update();
}

/* Save backbuffer contents to further usage */
char *fb_dump()
{
	char *dump;

	dump = malloc(fb.screensize);
	if (NULL == dump) return NULL;

	fb_memcpy(fb.backbuffer, dump, fb.screensize);
	return dump;
}

/* Restore saved backbuffer */
void fb_restore(char *dump)
{
	if (NULL == dump) return;
	fb_memcpy(dump, fb.backbuffer, fb.screensize);
}


void fb_destroy()
{
	if (fb.fd >= 0)
		close(fb.fd);
	if(fb.backbuffer)
		free(fb.backbuffer);
}

int
attempt_to_change_pixel_format(struct fb_var_screeninfo *fb_var)
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

	if ((ioctl(fb.fd, FBIOPUT_VSCREENINFO, fb_var) == 0) && (32 == fb_var->bits_per_pixel)) {
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

	if ((ioctl(fb.fd, FBIOPUT_VSCREENINFO, fb_var) == 0) && (16 == fb_var->bits_per_pixel)) {
		log_msg(lg, "Switched to a 16bpp mode");
		return 1;
	} else {
		log_msg(lg, "Failed to switch to a 16bpp mode, giving up");
	}

	return 0;
}

#ifdef DEBUG
void print_fb()
{
	log_msg(lg, "Framebuffer structure");
	log_msg(lg, "Descriptor: %d", fb.fd);
	log_msg(lg, "Type: %d", fb.type);
	log_msg(lg, "Visual: %d", fb.visual);
	log_msg(lg, "Width: %d, height: %d", fb.width, fb.height);
	log_msg(lg, "Real width: %d, real height: %d", fb.real_width, fb.real_height);
	log_msg(lg, "BPP: %d, depth: %d", fb.bpp, fb.depth);
	log_msg(lg, "Stride: %d", fb.stride);

	log_msg(lg, "Screensize: %d", fb.screensize);
	log_msg(lg, "Angle: %d", fb.angle);

	log_msg(lg, "RGBmode: %d", fb.rgbmode);
	log_msg(lg, "Red offset: %d, red length: %d", fb.red_offset, fb.red_length);
	log_msg(lg, "Green offset: %d, green length: %d", fb.green_offset, fb.green_length);
	log_msg(lg, "Blue offset: %d, blue length: %d", fb.blue_offset, fb.blue_length);
}
#endif

int fb_new(int angle)
{
	struct fb_var_screeninfo fb_var;
	struct fb_fix_screeninfo fb_fix;
	int off;
	char *fbdev;

	fbdev = getenv("FBDEV");
	if (fbdev == NULL)
		fbdev = "/dev/fb0";

	memset(&fb, 0, sizeof(FB));

	fb.fd = -1;

	if ((fb.fd = open(fbdev, O_RDWR)) < 0) {
		log_msg(lg, "Error opening /dev/fb0: %s", ERRMSG);
		goto fail;
	}

	if (ioctl(fb.fd, FBIOGET_VSCREENINFO, &fb_var) == -1) {
		log_msg(lg, "Error getting variable framebuffer info: %s", ERRMSG);
		goto fail;
	}

	if (fb_var.bits_per_pixel < 16)
	{
		log_msg(lg,
			"Error, no support for %i bpp frame buffers\n"
			"Trying to change pixel format...",
			fb_var.bits_per_pixel);
		if (!attempt_to_change_pixel_format(&fb_var))
			goto fail;
	}
	if (ioctl (fb.fd, FBIOGET_VSCREENINFO, &fb_var) == -1)
	{
		log_msg(lg, "Error getting variable framebuffer info (2): %s", ERRMSG);
		goto fail;
	}

	/* NB: It looks like the fbdev concept of fixed vs variable screen info is
	 * broken. The line_length is part of the fixed info but it can be changed
	 * if you set a new pixel format. */
	if (ioctl(fb.fd, FBIOGET_FSCREENINFO, &fb_fix) == -1) {
		log_msg(lg, "Error getting fixed framebuffer info: %s", ERRMSG);
		goto fail;
	}

	fb.real_width = fb.width = fb_var.xres;
	fb.real_height = fb.height = fb_var.yres;
	fb.bpp = fb_var.bits_per_pixel;
	fb.byte_pp = fb.bpp >> 3;
	fb.stride = fb_fix.line_length;
	fb.type = fb_fix.type;
	fb.visual = fb_fix.visual;
	strncpy(fb.id, fb_fix.id, 16);

	fb.screensize = fb.stride * fb.height;
	fb.backbuffer = malloc(fb.screensize);

	fb.red_offset = fb_var.red.offset;
	fb.red_length = fb_var.red.length;
	fb.green_offset = fb_var.green.offset;
	fb.green_length = fb_var.green.length;
	fb.blue_offset = fb_var.blue.offset;
	fb.blue_length = fb_var.blue.length;

	fb.depth = fb.red_length + fb.green_length + fb.blue_length;
	if (18 != fb.depth) fb.depth = fb.bpp;	/* according to some info 18bpp is reported as 24bpp */

	if ((fb.red_offset > fb.green_offset) && (fb.green_offset > fb.blue_offset)) {
		fb.rgbmode = RGB;
	} else if ((fb.red_offset < fb.green_offset) && (fb.green_offset < fb.blue_offset)) {
		fb.rgbmode = BGR;
	} else {
		fb.rgbmode = GENERIC;
	}

	if (fb_quirk_check_manual_update())
		fb.needs_manual_update = 1;

	fb.base = (char *) mmap((caddr_t) NULL,
				 /*fb_fix.smem_len */
				 fb.stride * fb.height,
				 PROT_READ | PROT_WRITE,
				 MAP_SHARED, fb.fd, 0);

	if (fb.base == (char *) -1) {
		log_msg(lg, "Error cannot mmap framebuffer: %s", ERRMSG);
		goto fail;
	}

	off =
	    (unsigned long) fb_fix.smem_start %
	    (unsigned long) getpagesize();

	fb.data = fb.base + off;
	fb.angle = angle;

	switch (fb.angle) {
	case 270:
	case 90:
		fb.width = fb.real_height;
		fb.height = fb.real_width;
		break;
	case 180:
	case 0:
	default:
		break;
	}

#ifdef DEBUG
	print_fb(fb);
#endif

	switch (fb.depth) {
#ifdef USE_32BPP
	case 32:
		fb.plot_pixel = fb_plot_pixel_32bpp;
		fb.draw_hline = fb_draw_hline_32bpp;
		break;
#endif
#ifdef USE_24BPP
	case 24:
		fb.plot_pixel = fb_plot_pixel_24bpp;
		fb.draw_hline = fb_draw_hline_24bpp;
		break;
#endif
#ifdef USE_18BPP
	case 18:
		fb.plot_pixel = fb_plot_pixel_18bpp;
		fb.draw_hline = fb_draw_hline_18bpp;
		break;
#endif
#ifdef USE_16BPP
	case 16:
		fb.plot_pixel = fb_plot_pixel_16bpp;
		fb.draw_hline = fb_draw_hline_16bpp;
		break;
#endif
	default:
		/* We have no drawing functions for this mode ATM */
		log_msg(lg, "Sorry, your bpp (%d) and/or depth (%d) are not supported yet", fb.bpp, fb.depth);
		fb_destroy(fb);
		return -1;
		break;
	}

	return 0;

fail:
	fb_destroy();
	return -1;
}


/**************************************************************************
 * Graphic primitives
 */
void fb_plot_pixel(int x, int y, kx_rgba rgba)
{
	kx_rgba color;

	color = compose_color(rgba);

	fb.plot_pixel(x, y, color);
}


void fb_draw_hline(int x, int y, int length, kx_rgba rgba)
{
	kx_rgba color;

	color = compose_color(rgba);

	fb.draw_hline(x, y, length, color);
}


void fb_draw_rect(int x, int y, int width, int height,
		kx_rgba rgba)
{
	static int dy;
	kx_rgba color;

	color = compose_color(rgba);

	for (dy = y; dy < y+height; dy++)
		fb.draw_hline(x, dy, width, color);
}


void fb_draw_rounded_rect(int x, int y, int width, int height,
		kx_rgba rgba)
{
	static int dy;
	kx_rgba color;

	if (height < 4) return;

	color = compose_color(rgba);

	/* Top rounded part */
	dy = y;
	fb.draw_hline(x+2, dy++, width-4, color);
	fb.draw_hline(x+1, dy++, width-2, color);

	for (; dy < y+height-2; dy++)
		fb.draw_hline(x, dy, width, color);

	/* Bottom rounded part */
	fb.draw_hline(x+1, dy++, width-2, color);
	fb.draw_hline(x+2, dy++, width-4, color);
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
void fb_text_size(int *width, int *height, const Font * font,
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


int fb_draw_constrained_text(int x, int y,
		int max_x, int max_y, kx_rgba rgba,
		const Font * font, const char *text)
{
	int h, w, cx, cy, dx, dy;
	char *c = (char *) text;
	u_int32_t gl;
	kx_rgba color;

	color = compose_color(rgba);

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
					fb.plot_pixel(dx + cx,
						      dy + cy, color);
				gl <<= 1;
			}
		}

		dx += w;
	}

	return dy - y + h;
}


void fb_draw_text(int x, int y, kx_rgba rgba,
		const Font * font, const char *text)
{
	fb_draw_constrained_text(x, y, 0, 0, rgba, font, text);
}


/* Draw picture on framebuffer */
void fb_draw_picture(int x, int y, kx_picture *pic)
{
	if (NULL == pic) return;

	unsigned int i, j;
	int dx = 0, dy = 0;
	kx_rgba *pixel, color;

	pixel = pic->pixels;
	dy = y;
	for (i = 0; i < pic->height; i++) {
		dx = x;
		for (j = 0; j < pic->width; j++) {
			color = *pixel;
			color = compose_color (color);
			/* TODO Add transparency processing */
			/* Draw pixel if not fully transparent */
			if ((color & 0xFF000000) == 0)
				fb.plot_pixel(dx, dy, color);
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

#endif	/* USE_FBMENU */
