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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "rgb.h"
#include "rgbtab.h"

inline void
xrgb2comp(uint32 rgb, uint8 *red, uint8 *green, uint8 *blue)
{
	*blue =  rgb & 0x000000FF;
	*green = (rgb & 0x0000FF00) >> 8;
	*red = rgb >> 16;
}


static int hchar2int(unsigned char c)
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

/* Convert hex rgb color to rgb structure */
int hex2rgb(char *hex, struct rgb_color *rgb)
{
	switch (strlen(hex)) {
	case 3 + 1:		/* #abc */
		rgb->r = hchar2int(hex[1]);
		rgb->g = hchar2int(hex[2]);
		rgb->b = hchar2int(hex[3]);
		break;
	case 6 + 1:		/* #abcdef */
		rgb->r = hchar2int(hex[1]) << 4 | hchar2int(hex[2]);
		rgb->g = hchar2int(hex[3]) << 4 | hchar2int(hex[4]);
		rgb->b = hchar2int(hex[5]) << 4 | hchar2int(hex[6]);
		break;
	case 12 + 1:	/* #32329999CCCC */
		/* so for now only take two digits */
		rgb->r = hchar2int(hex[1]) << 4 | hchar2int(hex[2]);
		rgb->g = hchar2int(hex[5]) << 4 | hchar2int(hex[6]);
		rgb->b = hchar2int(hex[9]) << 4 | hchar2int(hex[10]);
		break;
	default:
		return -1;
	}
	return 0;
}

/* Convert color name to rgb structure */
int cname2rgb(char *cname, struct rgb_color *rgb)
{
	char *tmp, *color;
	int len, half, rest;
	unsigned char c;
	/* Named color structure */
	struct named_rgb_color *cn;

	color = strdup(cname);
	if (NULL == color) {
		DPRINTF("Can't allocate memory for color name copy (%s)\n", cname);
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
		rgb->r = 0;
		rgb->g = 0;
		rgb->b = 0;
		return 1;
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

	if (0 == len) {	/* Found */
		xrgb2comp(cn->rgb, &rgb->r, &rgb->g, &rgb->b);
	} else {		/* Not found */
		DPRINTF("Color name '%s' not in colors database, returning red\n", color);
		/* Return 'red' color like libXpm does */
		rgb->r = 0xFF;
		rgb->g = 0;
		rgb->b = 0;
	}
	free(color);
	return 0;
}

