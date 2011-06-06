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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "rgb.h"
#include "rgbtab.h"

inline void
rgba2comp(kx_rgba rgba, kx_ccomp *red, kx_ccomp *green,
		kx_ccomp *blue, kx_ccomp *alpha)
{
	*alpha = (kx_ccomp) (rgba & 0x000000FF);
	*blue =  (kx_ccomp)((rgba & 0x0000FF00) >> 8);
	*green = (kx_ccomp)((rgba & 0x00FF0000) >> 16);
	*red =   (kx_ccomp)((rgba & 0xFF000000) >> 24);
}


kx_ccomp hchar2int(unsigned char c)
{
	static int r;

	if (c >= '0' && c <= '9')
		r = c - '0';
	else if (c >= 'a' && c <= 'f')
		r = c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		r = c - 'A' + 10;
	else
		r = 0;

	return (r);
}

/* Convert hex rgb color to rgb color */
kx_rgba hex2rgba(char *hex)
{
	static kx_ccomp r, g, b, a;
	switch (strlen(hex)) {
	case 3 + 1:		/* #abc */
		r = hchar2int(hex[1]);
		g = hchar2int(hex[2]);
		b = hchar2int(hex[3]);
		a = 0;
		break;
	case 6 + 1:		/* #abcdef */
		r = hchar2int(hex[1]) << 4 | hchar2int(hex[2]);
		g = hchar2int(hex[3]) << 4 | hchar2int(hex[4]);
		b = hchar2int(hex[5]) << 4 | hchar2int(hex[6]);
		a = 0;
		break;
	case 8 + 1:		/* #abcdefaa */
		r = hchar2int(hex[1]) << 4 | hchar2int(hex[2]);
		g = hchar2int(hex[3]) << 4 | hchar2int(hex[4]);
		b = hchar2int(hex[5]) << 4 | hchar2int(hex[6]);
		a = hchar2int(hex[7]) << 4 | hchar2int(hex[8]);
		break;
	case 12 + 1:	/* #32329999CCCC */
		/* so for now only take two digits */
		r = hchar2int(hex[1]) << 4 | hchar2int(hex[2]);
		g = hchar2int(hex[5]) << 4 | hchar2int(hex[6]);
		b = hchar2int(hex[9]) << 4 | hchar2int(hex[10]);
		a = 0;
		break;
	default:
		/* Return black transparent color */
		return comp2rgba(0, 0, 0, 255);
	}
	return comp2rgba(r, g, b, a);
}

/* Convert color name to rgb color */
kx_rgba cname2rgba(char *cname)
{
	char *tmp, *color;
	int len, half, rest;
	unsigned char c;
	kx_named_color *cn;

	color = strdup(cname);
	if (NULL == color) {
		DPRINTF("Can't allocate memory for color name copy (%s)", cname);
		return -1;
	}
	len = strlen(cname);

	/* Strip spaces and lowercase */
	tmp = color;
	while('\0' != *tmp) {
		c = *tmp;
		if (' ' == c) {
			memmove(tmp, tmp+1, color + len - tmp);
			continue;
		} else {
			*tmp = tolower(c);
		}
		++tmp;
	}

	/* Check for transparent color */
	if( 0 == strcmp(color, "none") ) {
		free(color);
		/* Return black transparent color */
		return comp2rgba(0, 0, 0, 255);
	}

	/* check for "grey" */
	tmp = strstr(color, "grey");
	if (NULL != tmp) {
		tmp[2] = 'a';	/* Convert to "gray" */
	}

	/* Binary search in color names array */
	len = ROWS(color_names);

	half = len >> 1;
	rest = len - half;

	cn = color_names + half;
	len = 1; /* Used as flag */

	while (half > 0) {
		half = rest >> 1;
		len = strcmp(tmp, cn->name);
		if (len < 0) {
			cn -= half;
		} else if (len > 0) {
			cn += half;
		} else {
			/* len will be 0 here */
			break;
		}
		rest -= half;
	}

	free(color);
	if (0 == len) {	/* Found */
		return cn->rgba;
	} else {		/* Not found */
		log_msg(lg, "Color name '%s' not in colors database, returning transparent red", cname);
		/* Return 'red' color like libXpm does */
		return comp2rgba(255, 0, 0, 255);
	}
}

