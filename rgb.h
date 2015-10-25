/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2008-2011 Yuri Bushmelev <jay4mail@gmail.com>
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

#include "config.h"

#ifdef USE_FBMENU
#include "util.h"

/* RGB ordering */
enum RGBMode {
    BGR,
    RGB,
    GENERIC
};

/* Color packed into uint32_t (RGBA) */
typedef uint32_t kx_rgba;

/* Color component */
typedef uint8_t kx_ccomp;

/* Pack color components into uint32_t */
#define comp2rgba(r,g,b,a) \
	((kx_rgba)(r)<<24|(kx_rgba)(g)<<16|(kx_rgba)(b)<<8|(kx_rgba)(a))

/* Get alpha-channel from rgba color */
#define rgba2a(rgba) \
	((kx_rgba)(rgba) & (kx_rgba)0x000000FF)

/* Named color structure */
typedef struct {
	char *name;
	kx_rgba rgba;
} kx_named_color;

/* Convert RGBA uint32 to red/green/blue/alpha components */
void rgba2comp(kx_rgba rgba, kx_ccomp *red, kx_ccomp *green,
		kx_ccomp *blue, kx_ccomp *alpha);

/* Convert hex rgb color to rgba color */
kx_rgba hex2rgba(char *hex);

/* Convert color name to rgba color */
kx_rgba cname2rgba(char *cname);

#endif	/* USE_FBMENU */
#endif	/* HAVE_RGB_H */
