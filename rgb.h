/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2008-2010 Yuri Bushmelev <jay4mail@gmail.com>
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

#ifndef _HAVE_RGB_H
#define _HAVE_RGB_H

#include "util.h"

/* Colors triplet structure */
struct rgb_color {
	uint8 r;
	uint8 g;
	uint8 b;
};

/* RGB ordering */
enum RGBMode {
    BGR,
    RGB,
    GENERIC
};

/* Convert XRGB uint32 to red/green/blue components */
inline void
xrgb2comp(uint32 rgb, uint8 *red, uint8 *green, uint8 *blue);

/* Convert hex rgb color to rgb structure */
int hex2rgb(char *hex, struct rgb_color *rgb);

/* Convert color name to rgb structure */
int cname2rgb(char *cname, struct rgb_color *rgb);

#endif	/* HAVE_RGB_H */
