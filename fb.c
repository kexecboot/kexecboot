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
 * TODO Replace one pixel drawing function with bpp checking with multiple
 * functions (one for each bpp).
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

	if (0 == fb->byte_pp)
		fb->screensize = fb->width * fb->height; /* FIXME: it should be fine-tuned according to fb->stride */
	else
		fb->screensize = fb->width * fb->height * fb->byte_pp;

	fb->backbuffer = malloc(fb->screensize);

	fb->red_offset = fb_var.red.offset;
	fb->red_length = fb_var.red.length;
	fb->green_offset = fb_var.green.offset;
	fb->green_length = fb_var.green.length;
	fb->blue_offset = fb_var.blue.offset;
	fb->blue_length = fb_var.blue.length;

	/* FIXME: should we switch to depth from bpp? */
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
	case 24:
		fb->plot_pixel = fb_plot_pixel_24bpp;
		break;
	case 18:
		fb->plot_pixel = fb_plot_pixel_18bpp;
		break;
	case 16:
		fb->plot_pixel = fb_plot_pixel_16bpp;
		break;
	case 2:
		fb->plot_pixel = fb_plot_pixel_2bpp;
		break;
	case 1:
		fb->plot_pixel = fb_plot_pixel_1bpp;
		break;
	}

	fb->draw_line = NULL;

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
	DPRINTF("Red offset: %d, red lenght: %d\n", red_offset, red_length);
	DPRINTF("Green offset: %d, green lenght: %d\n", green_offset, green_length);
	DPRINTF("Blue offset: %d, blue lenght: %d\n", blue_offset, blue_length);
}
#endif

/* pixel offset (for 1bpp or 2bpp */
static inline int
fb_offset_pixel(FB *fb, int x, int y)
{
	static int ox, oy;
	switch (fb->angle) {
	case 270:
		oy = x;
		ox = fb->height - y - 1;
		break;
	case 180:
		ox = fb->width - x - 1;
		oy = fb->height - y - 1;
		break;
	case 90:
		ox = y;
		oy = fb->width - x - 1;
		break;
	case 0:
	default:
		ox = x;
		oy = y;
		break;
	}

	switch (fb->bpp) {
	case 2: return (oy * (fb->stride << 2)) + ox;
	case 1: return (oy * (fb->stride << 3)) + ox;
	}
}

/* byte offset */
static inline int
fb_offset_byte(FB *fb, int x, int y)
{
	static int ox, oy;
	switch (fb->angle) {
	case 270:
		oy = x;
		ox = fb->height - y - 1;
		break;
	case 180:
		ox = fb->width - x - 1;
		oy = fb->height - y - 1;
		break;
	case 90:
		ox = y;
		oy = fb->width - x - 1;
		break;
	case 0:
	default:
		ox = x;
		oy = y;
		break;
	}

	return (oy * fb->stride) + (ox * (fb->byte_pp));
}


inline void
fb_plot_pixel_24bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;

	offset = fb->backbuffer + fb_offset_byte(fb, x, y);

	if (offset > (fb->backbuffer + fb->screensize - fb->byte_pp)) return;

	if (RGB = fb->rgbmode) {
		*(offset++) = blue
		*(offset + 2) = red;
	} else {
		*(offset) = red;
		*(offset + 2) = blue;
	}
	*(offset + 1) = green;
}

inline void
fb_plot_pixel_18bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;

	offset = fb->backbuffer + fb_offset_byte(fb, x, y);

	if (offset > (fb->backbuffer + fb->screensize - fb->byte_pp)) return;

	if (RGB = fb->rgbmode) {
			*(offset)     = (blue >> 2) | ((green & 0x0C) << 4);
			*(offset + 1) = ((green & 0xF0) >> 4) | ((red & 0x3C) << 2);
			*(offset + 2) = (red & 0xC0) >> 6;
	} else {
			*(offset)     = (red >> 2) | ((green & 0x0C) << 4);
			*(offset + 1) = ((green & 0xF0) >> 4) | ((blue & 0x3C) << 2);
			*(offset + 2) = (blue & 0xC0) >> 6;
	}
}

inline void
fb_plot_pixel_16bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static char *offset;

	offset = fb->backbuffer + fb_offset_byte(fb, x, y);

	if (offset > (fb->backbuffer + fb->screensize - fb->byte_pp)) return;

	if (RGB = fb->rgbmode) {
		*(volatile uint16_t *) (offset)
			= ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
	} else {
		*(volatile uint16_t *) (offset)
			= ((blue >> 3) << 11) | ((green >> 2) << 5) | (red >> 3);
	}
}

inline void
fb_plot_pixel_2bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static int off, shift;

	off = fb_offset_bit(fb, x, y);
	shift = (3 - (off & 3)) << 1;

	*(fb->backbuffer + (off >> 2)) = (*(fb->backbuffer + (off >> 2)) & ~(3 << shift))
	| (((11*red + 16*green + 5*blue) >> 11) << shift);
}

inline void
fb_plot_pixel_1bpp(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	static int off, shift;

	off = fb_offset_bit(fb, x, y);
	shift = 7 - (off & 7);

	if (((11*red + 16*green + 5*blue) >> 5) >= 128)
		*(fb->backbuffer + (off >> 3)) |= (1 << shift);
	else
		*(fb->backbuffer + (off >> 3)) &= ~(1 << shift);
}


inline void
fb_plot_pixel(FB * fb, int x, int y, uint8 red, uint8 green, uint8 blue)
{
	int off, shift;

	if ( (x < 0) || (x > (fb->width - 1)) || (y < 0) || (y > (fb->height - 1)) )
		return;

	switch (fb->angle) {
	case 270:
		off = fb_offset(fb, fb->height - y - 1, x);
		break;
	case 180:
		off = fb_offset(fb, fb->width - x - 1, fb->height - y - 1);
		break;
	case 90:
		off = fb_offset(fb, y, fb->width - x - 1);
		break;
	case 0:
	default:
		off = fb_offset(fb, x, y);
		break;
	}

	if (fb->rgbmode == RGB) {
	switch (fb->bpp)
		{
		case 24:
		case 32:
			*(fb->backbuffer + off)     = blue;
			*(fb->backbuffer + off + 1) = green;
			*(fb->backbuffer + off + 2) = red;
			break;
		case 16:
			*(volatile uint16_t *) (fb->backbuffer + off)
				= ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
			break;
		default:
			/* depth not supported yet */
			break;
		}
	} else if (fb->rgbmode == BGR) {
	switch (fb->bpp)
		{
		case 24:
		case 32:
			*(fb->backbuffer + off)     = red;
			*(fb->backbuffer + off + 1) = green;
			*(fb->backbuffer + off + 2) = blue;
			break;
		/* FIXME: this was compared against sum of color length, not bpp */
		/* FIXME: should we add this to other blocks? */
		case 18:
			*(fb->backbuffer + off)     = (red >> 2) | ((green & 0x0C) << 4);
			*(fb->backbuffer + off + 1) = ((green & 0xF0) >> 4) | ((blue & 0x3C) << 2);
			*(fb->backbuffer + off + 2) = (blue & 0xC0) >> 6;
			break;
		case 16:
			*(volatile uint16_t *) (fb->backbuffer + off)
				= ((blue >> 3) << 11) | ((green >> 2) << 5) | (red >> 3);
			break;
		default:
			/* depth not supported yet */
			break;
		}
	} else {
		switch (fb->bpp)
		{
		case 32:
			*(volatile uint32_t *) (fb->backbuffer + off)
			= ((red >> (8 - fb->red_length)) << fb->red_offset)
				| ((green >> (8 - fb->green_length)) << fb->green_offset)
				| ((blue >> (8 - fb->blue_length)) << fb->blue_offset);
			break;
		case 16:
			*(volatile uint16_t *) (fb->backbuffer + off)
			= ((red >> (8 - fb->red_length)) << fb->red_offset)
				| ((green >> (8 - fb->green_length)) << fb->green_offset)
				| ((blue >> (8 - fb->blue_length)) << fb->blue_offset);
			break;
		case 2:
				shift = (3 - (off & 3)) << 1;
				*(fb->backbuffer + (off >> 2)) = (*(fb->backbuffer + (off >> 2)) & ~(3 << shift))
				| (((11*red + 16*green + 5*blue) >> 11) << shift);
			break;
		case 1:
			shift = 7 - (off & 7);
			if (((11*red + 16*green + 5*blue) >> 5) >= 128)
				*(fb->backbuffer + (off >> 3)) |= (1 << shift);
			else
				*(fb->backbuffer + (off >> 3)) &= ~(1 << shift);
			break;
		default:
			/* depth not supported yet */
			break;
		}
	}

}

void fb_draw_rect(FB * fb, int x, int y, int width, int height,
		uint8 red, uint8 green, uint8 blue)
{
	int dx, dy;

	for (dy = 0; dy < height; dy++)
		for (dx = 0; dx < width; dx++)
			fb_plot_pixel(fb, x + dx, y + dy, red, green,
				      blue);
}

#if 0
void fb_draw_image(FB * fb, int x, int y, int img_width, int img_height,
		int img_bytes_per_pixel, uint8 * rle_data)
{
	uint8 *p = rle_data;
	int dx = 0, dy = 0, total_len;
	unsigned int len;

	total_len = img_width * img_height * img_bytes_per_pixel;

	/* FIXME: Optimise, check for over runs ... */
	while ((p - rle_data) < total_len) {
		len = *(p++);

		if (len & 128) {
			len -= 128;

			if (len == 0)
				break;

			do {
				fb_plot_pixel(fb, x + dx, y + dy, *p,
					      *(p + 1), *(p + 2));
				if (++dx >= img_width) {
					dx = 0;
					dy++;
				}
			}
			while (--len && (p - rle_data) < total_len);

			p += img_bytes_per_pixel;
		} else {
			if (len == 0)
				break;

			do {
				fb_plot_pixel(fb, x + dx, y + dy, *p,
					      *(p + 1), *(p + 2));
				if (++dx >= img_width) {
					dx = 0;
					dy++;
				}
				p += img_bytes_per_pixel;
			}
			while (--len && (p - rle_data) < total_len);
		}
	}
}
#endif


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
				fb_plot_pixel(fb, dx, dy, xpm_color->r, xpm_color->g,
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
					fb_plot_pixel(fb, x + dx + cx,
						      y + dy + cy, red,
						      green, blue);
				g <<= 1;
			}
		}

		dx += w;
	}
}
