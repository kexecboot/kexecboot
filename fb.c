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
 * TODO: Add support for 8bpp and 12bpp modes.
 * NOTE: Modes 1bpp, 2bpp, 4bpp and 18bpp should be tested.
 */

#include "fb.h"

void fb_render(FB * fb)
{
	/* klibc uses 8bit transfers that breaks image on tosa */
	/* memcpy(fb->data, fb->backbuffer, fb->screensize); */
	uint16 *source, *dest;
	int n = fb->screensize/2;

	source = (uint16 *)fb->backbuffer;
	dest = (uint16 *)fb->data;

	while (n--) {
		*dest++ = *source++;
	}
}

void fb_destroy(FB * fb)
{
	if (fb->fd >= 0)
		close(fb->fd);
	if(fb->backbuffer)
		free(fb->backbuffer);
	free(fb);
}

static int
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

	if (fb->bpp <= 8)
		fb->depth = fb->bpp;	/* color depth for 1,2,4,8 bpp modes */
	else
		fb->depth = fb->red_length + fb->green_length + fb->blue_length;


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
	case 32:
		fb->plot_pixel = fb_plot_pixel_32bpp;
		fb->draw_hline = fb_draw_hline_32bpp;
		break;
	case 24:
		fb->plot_pixel = fb_plot_pixel_24bpp;
		fb->draw_hline = fb_draw_hline_24bpp;
		break;
	case 18:
		fb->plot_pixel = fb_plot_pixel_18bpp;
		fb->draw_hline = fb_draw_hline_18bpp;
		break;
	case 16:
		fb->plot_pixel = fb_plot_pixel_16bpp;
		fb->draw_hline = fb_draw_hline_16bpp;
		break;
	case 4:
		fb->plot_pixel = fb_plot_pixel_4bpp;
		fb->draw_hline = fb_draw_hline_4bpp;
		break;
	case 2:
		fb->plot_pixel = fb_plot_pixel_2bpp;
		fb->draw_hline = fb_draw_hline_2bpp;
		break;
	case 1:
		fb->plot_pixel = fb_plot_pixel_1bpp;
		fb->draw_hline = fb_draw_hline_1bpp;
		break;
	}

	return fb;

fail:
	if (fb)
		fb_destroy(fb);

	return NULL;
}

#ifdef DEBUG
void print_fb(FB *fb)
{
	DPRINTF("Framebuffer structure");
	DPRINTF("Descriptor: %d\n", fd);
	DPRINTF("Type: %d\n", type);
	DPRINTF("Visual: %d\n", visual);
	DPRINTF("Width: %d, height: %d\n", width, height);
	DPRINTF("Real width: %d, real height: %d\n", real_width, real_height);
	DPRINTF("BPP: %d\n", bpp);
	DPRINTF("Stride: %d\n", stride);

	DPRINTF("Screensize: %d\n", screensize);
	DPRINTF("Angle: %d\n", angle);

	DPRINTF("RGBmode: %d\n", rgbmode);
	DPRINTF("Red offset: %d, red length: %d\n", red_offset, red_length);
	DPRINTF("Green offset: %d, green length: %d\n", green_offset, green_length);
	DPRINTF("Blue offset: %d, blue length: %d\n", blue_offset, blue_length);
}
#endif


static inline void
fb_respect_angle(FB *fb, int x, int y, int *dx, int *dy)
{
	switch (fb->angle) {
	case 270:
		&dy = x;
		&dx = fb->height - y - 1;
		break;
	case 180:
		&dx = fb->width - x - 1;
		&dy = fb->height - y - 1;
		break;
	case 90:
		&dx = y;
		&dy = fb->width - x - 1;
		break;
	case 0:
	default:
		&dx = x;
		&dy = y;
		break;
	}
}


/**************************************************************************
 * Pixel plotting routines
 */
static void
fb_plot_pixel_32bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy);
	offset = fb->backbuffer + ((oy * fb->width + ox) << 2);
	if (offset > (fb->backbuffer + fb->screensize - 4)) return;

	if (RGB = fb->rgbmode) {
		*(offset) = blue
		*(offset + 2) = red;
	} else {
		*(offset) = red;
		*(offset + 2) = blue;
	}
	*(offset + 1) = green;
}

static void
fb_plot_pixel_24bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy, r;

	fb_respect_angle(fb, x, y, &ox, &oy);
	r = oy * fb->width + ox;
	offset = fb->backbuffer + (r + (r << 1));
	if (offset > (fb->backbuffer + fb->screensize - 3)) return;

	if (RGB = fb->rgbmode) {
		*(offset) = blue
		*(offset + 2) = red;
	} else {
		*(offset) = red;
		*(offset + 2) = blue;
	}
	*(offset + 1) = green;
}

static void
fb_plot_pixel_18bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy, r;

	fb_respect_angle(fb, x, y, &ox, &oy);
	r = oy * fb->width + ox;
	offset = fb->backbuffer + (r + (r << 1));
	if (offset > (fb->backbuffer + fb->screensize - 3)) return;

	if (RGB = fb->rgbmode) {
			*(offset++)     = (blue >> 2) | ((green & 0x0C) << 4);
			*(offset++) = ((green & 0xF0) >> 4) | ((red & 0x3C) << 2);
			*(offset++) = (red & 0xC0) >> 6;
	} else {
			*(offset++)     = (red >> 2) | ((green & 0x0C) << 4);
			*(offset++) = ((green & 0xF0) >> 4) | ((blue & 0x3C) << 2);
			*(offset++) = (blue & 0xC0) >> 6;
	}
}

static void
fb_plot_pixel_16bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy);
	offset = fb->backbuffer + ((oy * fb->width + ox) << 1);
	if (offset > (fb->backbuffer + fb->screensize - 2)) return;

	if (RGB = fb->rgbmode) {
		*(volatile uint16_t *) (offset)
			= ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
	} else {
		*(volatile uint16_t *) (offset)
			= ((blue >> 3) << 11) | ((green >> 2) << 5) | (red >> 3);
	}
}

static void
fb_plot_pixel_4bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int off, ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy);
	off = (oy * fb->width + ox) << 2; /* Bit offset */
	oy = off >> 3; /* Target byte offset */

	offset = fb->backbuffer + oy;
	if (offset > (fb->backbuffer + fb->screensize)) return;

	ox = 4 - off + (oy << 3); /* 8 - bpp - off % 8 = bits num for left shift */
	*offset = *offset & ~(0x0F << ox)
			| ( ((11*red + (green << 4) + 5*blue) >> 8) << ox );
}

static void
fb_plot_pixel_2bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int off, ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy);
	off = (oy * fb->width + ox) << 1; /* Bit offset */
	oy = off >> 3; /* Target byte offset */

	offset = fb->backbuffer + oy;
	if (offset > (fb->backbuffer + fb->screensize)) return;

	ox = 6 - off + (oy << 3); /* 8 - bpp - off % 8 = bits num for left shift */
	*offset = *offset & ~(3 << ox)
			| ( ((11*red + (green << 4) + 5*blue) >> 11) << ox );
}

static void
fb_plot_pixel_1bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int off, ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy);
	off = oy * fb->width + ox; /* Bit offset */
	oy = off >> 3; /* Target byte offset */

	offset = fb->backbuffer + oy;
	if (offset > (fb->backbuffer + fb->screensize)) return;

	/* off - (obyte << 3) = off % 8;  8 - bpp - off % 8 = bits num for left shift */
	if (((11*red + (green << 4) + 5*blue) >> 5) >= 128)
		*offset = *offset | ( 1 << (7 - off + (oy << 3) ) );
	else
		*offset = *offset & ~( 1 << (7 - off + (oy << 3) ) );
}


/**************************************************************************
 * Horizontal line drawing routines
 */
static void
fb_draw_hline_32bpp(FB *fb, int x, int y, int length, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy);
	offset = fb->backbuffer + ((oy * fb->width + ox) << 2);
	if (offset > (fb->backbuffer + fb->screensize - 4)) return;

	if (length > fb->width - ox)
		oy = fb->width - ox;
	else
		oy = length;

	for(; oy > 0; oy--) {
		if (RGB = fb->rgbmode) {
			*(offset++) = blue;
			*(offset++) = green;
			*(offset++) = red;
		} else {
			*(offset++) = red;
			*(offset++) = green;
			*(offset++) = blue;
		}
		++offset;	/* Padding or transparency byte */
	}
}

static void
fb_draw_hline_24bpp(FB *fb, int x, int y, int length, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy, r;

	fb_respect_angle(fb, x, y, &ox, &oy);
	r = oy * fb->width + ox;
	offset = fb->backbuffer + (r + (r << 1));
	if (offset > (fb->backbuffer + fb->screensize - 3)) return;

	if (length > fb->width - ox)
		oy = fb->width - ox;
	else
		oy = length;

	for(; oy > 0; oy--) {
		if (RGB = fb->rgbmode) {
			*(offset++) = blue;
			*(offset++) = green;
			*(offset++) = red;
		} else {
			*(offset++) = red;
			*(offset++) = green;
			*(offset++) = blue;
		}
	}
}

static void
fb_draw_hline_18bpp(FB *fb, int x, int y, int length, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy, r;

	fb_respect_angle(fb, x, y, &ox, &oy);
	r = oy * fb->width + ox;
	offset = fb->backbuffer + (r + (r << 1));
	if (offset > (fb->backbuffer + fb->screensize - 3)) return;

	if (length > fb->width - ox)
		oy = fb->width - ox;
	else
		oy = length;

	for(; oy > 0; oy--) {
		if (RGB = fb->rgbmode) {
			*(offset++)     = (blue >> 2) | ((green & 0x0C) << 4);
			*(offset++) = ((green & 0xF0) >> 4) | ((red & 0x3C) << 2);
			*(offset++) = (red & 0xC0) >> 6;
		} else {
			*(offset++)     = (red >> 2) | ((green & 0x0C) << 4);
			*(offset++) = ((green & 0xF0) >> 4) | ((blue & 0x3C) << 2);
			*(offset++) = (blue & 0xC0) >> 6;
		}
	}
}

static void
fb_draw_hline_16bpp(FB *fb, int x, int y, int length, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int ox, oy;

	fb_respect_angle(fb, x, y, &ox, &oy);
	offset = fb->backbuffer + ((oy * fb->width + ox) << 1);
	if (offset > (fb->backbuffer + fb->screensize - 2)) return;

	if (length > fb->width - ox)
		oy = fb->width - ox;
	else
		oy = length;

	for(; oy > 0; oy--) {
		if (RGB = fb->rgbmode) {
			*(volatile uint16_t *) (offset)
				= ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
		} else {
			*(volatile uint16_t *) (offset)
				= ((blue >> 3) << 11) | ((green >> 2) << 5) | (red >> 3);
		}
		offset += 2;
	}
}

static void
fb_draw_hline_4bpp(FB *fb, int x, int y, int length, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int off, ox, oy, len, mask, color;
	static int cdata[2];

	fb_respect_angle(fb, x, y, &ox, &oy);
	off = (oy * fb->width + ox) << 2; /* Bit offset */
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
		*offset = *offset & mask | cdata[ox];
		ox = ~ox & 1;
		mask = ~mask;
		if (!ox) ++offset;
	}
}

static void
fb_draw_hline_2bpp(FB *fb, int x, int y, int length, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int off, ox, oy, len, color;
	static int cdata[4], mask[4];

	fb_respect_angle(fb, x, y, &ox, &oy);
	off = (oy * fb->width + ox) << 1; /* Bit offset */
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
		*offset = *offset & mask[ox] | cdata[ox];
		if (++ox > 6) {
			ox = 0;
			++offset;
		}
	}
}

static void
fb_draw_hline_1bpp(FB *fb, int x, int y, int length, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;
	static int off, ox, oy, len, color;

	fb_respect_angle(fb, x, y, &ox, &oy);
	off = oy * fb->width + ox; /* Bit offset */
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
				color = 128
			}
		}
}

void fb_draw_rect(FB * fb, int x, int y, int width, int height,
		uint8 red, uint8 green, uint8 blue)
{
	int dx, dy;

	for (dy = 0; dy < height; dy++)
		for (dx = 0; dx < width; dx++)
			fb->plot_pixel(fb, x + dx, y + dy, red, green,
				      blue);
}


/* Draw xpm image from parsed data */
void fb_draw_xpm_image(FB * fb, int x, int y, struct xpm_parsed_t *xpm_data)
{
	if (NULL == xpm_data) return;

	unsigned int i, j;
	int dx = 0, dy = 0;
	struct xpm_color_t **xpm_pixel, *xpm_color;

	xpm_pixel = xpm_data->pixels;
	dy = y;
	for (i = 0; i < xpm_data->height; i++) {
		dx = x;
		for (j = 0; j < xpm_data->width; j++) {
			xpm_color = *xpm_pixel;
			if (NULL != xpm_color) {	/* Non-transparent pixel */
				fb->plot_pixel(fb, dx, dy, xpm_color->r, xpm_color->g,
					xpm_color->b);
			}
			++dx;
			++xpm_pixel;
		}
		++dy;
	}
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

void fb_text_size(FB * fb, int *width, int *height, const Font * font,
		const char *text)
{
	char *c = (char *) text;
	int n, w, h, mw;

	n = strlen(text);
	mw = h = w = 0;

	for(;*c;c++){
		if (*c == '\n') {
			if (w > mw)
				mw = 0;
			h += font->height;
			continue;
		}

		w += font_glyph(font, *c, NULL);
	}

	*width = (w > mw) ? w : mw;
	*height = (h == 0) ? font->height : h;
}

void fb_draw_text(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue,
		const Font * font, const char *text)
{
	int h, w, n, cx, cy, dx, dy;
	char *c = (char *) text;

	n = strlen(text);
	h = font->height;
	dx = dy = 0;

	for(; *c;c++){
		u_int32_t *glyph = NULL;

		if (*c == '\n') {
			dy += h;
			dx = 0;
			continue;
		}

		w = font_glyph(font, *c, &glyph);

		if (glyph == NULL)
			continue;

		for (cy = 0; cy < h; cy++) {
			u_int32_t g = *glyph++;

			for (cx = 0; cx < w; cx++) {
				if (g & 0x80000000)
					fb->plot_pixel(fb, x + dx + cx,
						      y + dy + cy, red,
						      green, blue);
				g <<= 1;
			}
		}

		dx += w;
	}
}
