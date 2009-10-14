/*
 *  kexecboot - A kexec based bootloader
 *  XPM parsing routines based on libXpm
 *
 *  Copyright (c) 2008-2009 Yuri Bushmelev <jay4mail@gmail.com>
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include "xpm.h"
#include "rgbtab.h"

/* XPM pixel structure (internal) */
struct xpm_pixel_t {
	char *pixel;
	struct xpm_color_t *rgb;
};

/* XPM metadata (internal, not needed for drawing code) */
struct xpm_meta_t {
	struct xpm_parsed_t *xpm_parsed;
	unsigned int chpp;		/* number of characters per pixel */
	unsigned int ncolors;	/* number of colors */
	char *pixnames;		/* place for pixel names */
	struct xpm_pixel_t *pixdata;	/* array of named pixels */
};


/* Free XPM image array allocated by xpm_load_image() */
void xpm_destroy_image(char **xpm_data, const int rows)
{
	unsigned int i = 0;

	if (NULL != xpm_data) {
		for(i = 0; i < rows; i++) {
			if (NULL != xpm_data[i]) free(xpm_data[i]);
		};
		free(xpm_data);
	}
}


/* Free XPM image data allocated by xpm_parse_image() */
void xpm_destroy_parsed(struct xpm_parsed_t *xpm)
{
	if (NULL == xpm) return;

	if (NULL != xpm->colors) free(xpm->colors);
	if (NULL != xpm->pixels) free(xpm->pixels);
	free(xpm);
}


/* Load XPM image into array of strings */
int xpm_load_image(char ***xpm_data, const char *filename)
{
	int in_comment = 0;	/* We are inside comment block */
	int in_quotes = 0;	/* We are inside double quotes */
	/* In what block we are: header, values, colors, pixels */
	enum xpm_blocks {
		XPM_START,
		XPM_VALUES,
		XPM_COLORS,
		XPM_PIXELS,
		XPM_EXTENSIONS,
		XPM_END
	} in_block = XPM_START;
	int width = 0, height = 0, ncolors = 0, chpp = 0;	/* XPM image values */
	unsigned int n = 0;
	int fail = 0;	/* 1 = don't free data, 2 = free data */
	int len, nr;
	int f;
	char **data = NULL, *p, *e, *tmp;
	char *qstart = NULL, *qs;	/* Start and end of quoted string */
	struct stat sb;
	char buf[1500];	/* read() buffer */
	/* Quoted string storage buffer */
	char qsbuf[640 * 4 + 1];	/* (640 px max width * 4 chars per pixel + '\0') */

	if (NULL == xpm_data) {
		DPRINTF("Nowhere to store xpm data pointer\n");
		return -1;
	}

	f = open(filename, O_RDONLY);
    if (f < 0) {
		perror(filename);
		return -1;
	}

	if ( -1 == fstat(f, &sb) ) {
		perror(filename);
		close(f);
		return -1;
	}

	/* Check file size */
	if (sb.st_size > MAX_XPM_FILE_SIZE) {
		DPRINTF("%s is too big (%d bytes)\n", filename, (int)sb.st_size);
		close(f);
		return -1;
	}

	qsbuf[0] = '\0';
	while ( (nr = read(f, buf, sizeof(buf))) > 0 ) {
		/* Parse block by chars */
		e = buf + nr;	/* End of buffer */
		for (p = buf; p < e; p++) {
			switch (*p) {
			case '/':	/* Comment block start */
				if ( !in_comment && ('*' == *(p+1)) ) {
					in_comment = 1;
					++p;	/* Skip next char */
					continue;
				}
				break;
			case '*':	/* Comment block end */
				if ( in_comment && ('/' == *(p+1)) ) {
					in_comment = 0;
					++p;	/* Skip next char */
					continue;
				}
				break;

			case '"':	/* Quotes */
				if (!in_comment) {
					if (!in_quotes) {
						qstart = p + 1;	/* Start of quoted string */
					} else {
						/* End of quoted string found */
						if ( (p - qstart + strlen(qsbuf) + 1 ) > sizeof(qsbuf) ) {
							/* String is too long, we cant parse it */
							DPRINTF("XPM data string is too long\n");
							fail = 2;
							break;
						}
						/* Append current string to qsbuf */
						strncat(qsbuf, qstart, p - qstart);
						len = strlen(qsbuf);

						if (len > 0) {
							/* Now we have de-quoted string in qsbuf */
							switch (in_block) {
							case XPM_VALUES:	/* Process values */
								width = get_nni(qsbuf, &qs);
								height = get_nni(qs, &tmp);
								ncolors = get_nni(tmp, &qs);
								chpp = get_nni(qs, &tmp);

								if (width < 0 || height < 0 || ncolors < 0 || chpp < 0) {
									break;
								}

								/* Create new copy to store */
								qs = strdup(qsbuf);
								if (NULL == qs) {
									DPRINTF("Can't allocate memory for values\n");
									fail = 1;
									break;
								}

								/* Allocate memory for XPM data array */
								data = malloc((ncolors + height + 1) * sizeof(*data));
								if (NULL == data) {
									free(qs);
									DPRINTF("Can't allocate memory for values\n");
									fail = 1;
									break;
								}
								data[n] = qs;
								++n;

								in_block = XPM_COLORS;
								break;

							case XPM_COLORS:
								if (n >= ncolors + 1) {
									in_block = XPM_PIXELS;
								}
							case XPM_PIXELS:	/* Process colors or pixels data */
								if (n >= ncolors + height + 1) {
									in_block = XPM_EXTENSIONS;
									break;
								}

								/* Create copy to store */
								qs = strdup(qsbuf);
								if (NULL == qs) {
									DPRINTF("Can't allocate memory for colors or pixels\n");
									fail = 2;
									break;
								}

								/* Append string to array */
								data[n] = qs;
								/* DPRINTF("%d: %s\n", n, data[n]); */
								++n;
								break;
							default:
								break;
							} /* switch (in_block) */

							qsbuf[0] = '\0'; /* Clear qsbuf */

						} /* if (len > 0) */

					} /* else */
					in_quotes = !in_quotes;
					continue;
				}
				break;

			case '{':	/* XPM image headers start */
				if ( (XPM_START == in_block) && !in_quotes && !in_comment) {
					in_block = XPM_VALUES;
					continue;
				}
				break;
			case '}':	/* XPM image end */
				if ( !in_quotes && !in_comment )
				{
					in_block = XPM_END;
				}
				break;
			default:
				break;
			} /* switch (*p) */

			if ( (XPM_END == in_block) || fail ) break;
		} /* for */

		if ( (XPM_END == in_block) || fail ) break;

		/* When we still in quoted block store it */
		if (in_quotes) {
			if ( (e - qstart + strlen(qsbuf) + 1) > sizeof(qsbuf) ) {
				/* String is too long. We can't parse it */
				DPRINTF("XPM data string is too long\n");
				fail = 2;
				break;
			}
			/* Append end of current buffer to quoted string buffer */
			strncat(qsbuf, qstart, e - qstart);
			/* Move qstart to start of (new) buffer */
			qstart = buf;
		}

	} /* while (read()) */
	close(f);

	/*
	 * Now we can have one of following statuses:
	 * 1) EOF or read() error found
	 * 2) malloc failed (fail != 0)
	 * 3) wrong file format found (in_block != XPM_END)
	 * 4) all data is parsed just fine :)
	 */

	if ( (XPM_END != in_block) && (2 == fail) ) {
		/* We should free data */
		if (NULL != data) {
			for(len = 0; len < n; len++) {
				if (NULL != data[len]) free(data[len]);
			}
			free(data);
		}
		return -1;
	}

	*xpm_data = data;

    return n;
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


int hex2rgb(char *hex, struct xpm_color_t *rgb)
{
	switch (strlen(hex)) {
	case 3 + 1:		/* #abc */
		rgb->r = hchar2int(hex[1]);
		rgb->g = hchar2int(hex[2]);
		rgb->b = hchar2int(hex[3]);
		break;
	case 6 + 1:		/* abcdef */
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

/* Local function to convert color name to rgb structure */
int cname2rgb(char *cname, struct xpm_color_t *rgb)
{
	char *tmp, *color;
	int len, half, rest;
	unsigned char c;
	/* Named color structure */
	struct xpm_named_color_t *cn;

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
	len = XPM_ROWS(xpm_color_names);

	half = len >> 1;
	rest = len - half;

	cn = xpm_color_names + half;
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
		rgb->r = cn->r;
		rgb->g = cn->g;
		rgb->b = cn->b;
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


/* Local function to parse color line
 * NOTE: It will modify 'data'.
 */
void parse_cline(char *data, char **colors)
{
	int len = 0;
	enum xpm_ckey_t key, newkey = XPM_KEY_UNKNOWN;
	char *p, *e, *tmp, *color;

	/* Clear colors */
	colors[XPM_KEY_MONO] = NULL;
	colors[XPM_KEY_GRAY4] = NULL;
	colors[XPM_KEY_GRAY] = NULL;
	colors[XPM_KEY_COLOR] = NULL;

	e = data + strlen(data);
	color = NULL;
	key = XPM_KEY_UNKNOWN;
	p = data;
	/* Iterate over (<ckey> <color>) pairs */
	while (p < e) {

		tmp = get_word(p, &p);	/* Get word */
		if (NULL != tmp) {
			len = p - tmp;

			/* If there is no color data and there is no key found.. */
			if ( (NULL == color) && (XPM_KEY_UNKNOWN != key) ) {
				color = tmp;	/* ..then it's start of color data */
			}

			newkey = XPM_KEY_UNKNOWN;

			if (1 == len) {
				switch (tmp[0]) {
				case 'c':	/* color */
					newkey = XPM_KEY_COLOR;
					break;
				case 'm':	/* mono */
					newkey = XPM_KEY_MONO;
					break;
				case 'g':	/* gray */
					newkey = XPM_KEY_GRAY;
					break;
				case 's':	/* symbol */
					newkey = XPM_KEY_SYMBOL;
					break;
				default:
					break;
				}
			} else if ( (2 == len) && ('g' == tmp[0]) && ('4' == tmp[1]) ) {
				/* gray4 */
				newkey = XPM_KEY_GRAY4;
			}
		}

		/* Do we have found new key or end of string? */
		if ( (XPM_KEY_UNKNOWN != newkey) || (NULL == tmp) || ('\0' == *p) ) {

			/* If we have color and this is not symbolic key store it */
			if ((XPM_KEY_SYMBOL != key) && (NULL != color)) {
				/* skip back key and zero-terminate the color name */
				*(p-len-1) = '\0';
				colors[key] = color;
				color = NULL;
			}

			key = newkey;
		}

		if (NULL == tmp) return;

	} /* while */

}


/* Local function that parse colors */
int xpm_parse_colors(char **xpm_data, unsigned int bpp,
		struct xpm_meta_t *xpm_meta)
{
	int rc, chpp;
	/* Color structures: array and temporary pointer */
	struct xpm_color_t *xpm_color;
	char **data, **e, *p;
	char *color;
	char *pixel;
	struct xpm_pixel_t *pdata;
	/* Array of colors in line */
	char *colors[XPM_KEY_SYMBOL];
	/* Color line buffer */
	char line[MAX_XPM_CLINE_SIZE];

	xpm_color = xpm_meta->xpm_parsed->colors;
	pixel = xpm_meta->pixnames;
	pdata = xpm_meta->pixdata;
	chpp = xpm_meta->chpp;

	e = xpm_data + xpm_meta->ncolors;
	for (data = xpm_data; data < e; data++) {

		p = *data;

		/* Split first chpp chars (pixel) */
		strncpy(pixel, p, chpp);
		pixel[chpp] = '\0';
		p += chpp;

		/* Create temporary copy for parsing */
		strncpy(line, p, sizeof(line)-1);
		line[sizeof(line)-1] = '\0';

		/* Parse */
		parse_cline(line, colors);

		/* Select color according to supplied bpp */
		color = NULL;
		if ( 1 == bpp ) {		/* mono */
			color = colors[XPM_KEY_MONO];
		} else if ( 2 == bpp) {	/* 4 grays */
			color = colors[XPM_KEY_GRAY4];
		}

		if (NULL == color) {
			color = colors[XPM_KEY_COLOR];
			if (NULL == color)
				color = colors[XPM_KEY_GRAY];
			if (NULL == color)
				color = colors[XPM_KEY_GRAY4];
			if (NULL == color)
				color = colors[XPM_KEY_MONO];

			if (NULL == color) {
				DPRINTF("Wrong XPM format: wrong colors line '%s'\n", *data);
				return -1;
			}
		}

		/* Parse color to rgb + t */
		rc = 0;
		if ('#' == *color) {	/* hex */
			rc = hex2rgb(color, xpm_color);
		} else {				/* name */
			rc = cname2rgb(color, xpm_color);
		}

		if ( -1 == rc ) {
			DPRINTF("Can't parse color '%s'\n", color);
			return -1;
		}

		pdata->pixel = pixel;
		if (1 == rc) {	/* transparent pixel */
			pdata->rgb = NULL;
		} else {
			pdata->rgb = xpm_color;
		}

		++xpm_color;
		pixel += chpp + 1;	/* Go next place */
		++pdata;
	}

	return 0;
}


/* Local function to parse pixels data */
int xpm_parse_pixels(char **xpm_data, struct xpm_meta_t *xpm_meta)
{
	int chpp, width, i;
	struct xpm_color_t **xpm_pixel, **ctable = NULL;
	struct xpm_pixel_t *pdata, *edata;
	char **data, **e;
	char *p;
	char *pixel = NULL;
	unsigned char pix;

	chpp = xpm_meta->chpp;
	edata = xpm_meta->pixdata + xpm_meta->ncolors;

	/* Optimize some cases for speed. Prepare lookup tables */
	if (chpp < 3) {	/* chpp should be positive integer */
		width = XPM_ASCII_RANGE(1);		/* 96 */
		if (2 == chpp) width *= width;	/* (96 * 96) */

		ctable = malloc(width * sizeof(*ctable));
		if (NULL == ctable) {
			DPRINTF("Can't allocate memory for colors lookup table\n");
			goto free_nothing;
		}

		/* Clean lookup table */
		xpm_pixel = ctable;
		for (i = 0; i < width; i++) {
			*xpm_pixel = NULL;
		}

		/* Fill lookup table */
		for(pdata = xpm_meta->pixdata; pdata < edata; pdata++) {
			if ( ((unsigned char)pdata->pixel[0] < 32) ||
					((unsigned char)pdata->pixel[0] > 127) ||
					( (2 == chpp) && (
					((unsigned char)pdata->pixel[1] < 32) ||
					((unsigned char)pdata->pixel[1] > 127) ) ) )
			{
				DPRINTF("Pixel char is out of range [32-127]\n");
				goto free_ctable;
			}

			pix = pdata->pixel[0] - ' ';

			if (2 == chpp) {
				/* (y * 96 + x) */
				ctable[XPM_ASCII_RANGE(pix) + pdata->pixel[1] - ' '] = pdata->rgb;
			} else {	/* 1 == chpp */
				ctable[pix] = pdata->rgb;
			}
		}
	} else {
		pixel = malloc((chpp + 1) * sizeof(char));
		if (NULL == pixel) {
			DPRINTF("Can't allocate pixel\n");
			goto free_nothing;	/* ctable is not allocated here */
		}
	}

	width = xpm_meta->xpm_parsed->width;
	xpm_pixel = xpm_meta->xpm_parsed->pixels;
	e = xpm_data + xpm_meta->xpm_parsed->height;
	for (data = xpm_data; data < e; data++) {

		if (strlen(*data) != (width * chpp)) {
			DPRINTF("Wrong XPM format: pixel data length is not equal to width (%d != %d)\n",
			strlen(*data), width);
			goto free_ctable;
		}

		/* Iterate over pixels (every chpp chars) */
		for(p = *data; p < *data + width; p += chpp) {

			/* Optimize some cases for speed */
			switch (chpp) {
			case 1:
				*xpm_pixel = ctable[(unsigned char)*p - ' '];
				break;

			case 2:
				pix = *p - ' ';
				*xpm_pixel = ctable[XPM_ASCII_RANGE(pix) + *(p+1) - ' '];
				break;

			default:
				strncpy(pixel, p, chpp);
				pixel[chpp] = '\0';

				/* Search pixel */
				i = 0;	/* Used as flag */
				for(pdata = xpm_meta->pixdata; pdata < edata; pdata++) {
					if ( 0 != strcmp(pdata->pixel, pixel) ) continue;
					*xpm_pixel = pdata->rgb;
					i = 1;
				}
				if (0 == i) *xpm_pixel = NULL;	/* Consider this pixel as transparent */

				break;
			}

			++xpm_pixel;
		}

	}

	if (chpp < 3)
		free(ctable);
	else
		free(pixel);

	return 0;

free_ctable:
	if (chpp < 3)
		free(ctable);
	else
		free(pixel);

free_nothing:
	return -1;
}



/* Process XPM image data and make it 'drawable' */
struct xpm_parsed_t *xpm_parse_image(char **xpm_data, const int rows,
		unsigned int bpp)
{
	int width = 0, height = 0, ncolors = 0, chpp = 0;	/* XPM image values */
	struct xpm_parsed_t *xpm_parsed;	/* return value */
	struct xpm_meta_t xpm_meta;	/* XPM metadata */
	char *p;

	if (NULL == xpm_data) return NULL;

	if (rows < 3) {
		DPRINTF("XPM image array should have at least 3 rows!\n");
		return NULL;
	}

	/* Parse image values */
	width = get_nni(xpm_data[0], &p);
	height = get_nni(p, &p);
	ncolors = get_nni(p, &p);
	chpp = get_nni(p, &p);

	if (width < 0 || height < 0 || ncolors < 0 || chpp < 0) {
		DPRINTF("Wrong XPM format: wrong values (%d, %d, %d, %d)\n",
			width, height, ncolors, chpp);
		goto free_nothing;
	}

	if ( rows != (1 + ncolors + height) ) {
		DPRINTF("Wrong XPM format: passed and parsed sizes are not equal (%d != %d)\n",
			rows, 1 + ncolors + height);
		goto free_nothing;
	}

	if ( ncolors > (1 << (8 * chpp)) ) {
		DPRINTF("Wrong XPM format: there are more colors than char_per_pixel can serve (%d > %d)\n",
			ncolors, 1 << (8 * chpp) );
		goto free_nothing;
	}

	/* Prepare return values */
	xpm_parsed = malloc(sizeof(*xpm_parsed));
	if (NULL == xpm_parsed) {
		DPRINTF("Can't allocate memory for return values\n");
		goto free_nothing;
	}

	/* Store values */
	xpm_parsed->width = width;
	xpm_parsed->height = height;

	xpm_meta.ncolors = ncolors;
	xpm_meta.chpp = chpp;
	xpm_meta.xpm_parsed = xpm_parsed;

	/* Allocate array of colors */
	xpm_parsed->colors = malloc(sizeof(*(xpm_parsed->colors)) * ncolors);
	if (NULL == xpm_parsed->colors) {
		DPRINTF("Can't allocate memory for xpm colors array\n");
		goto free_xpm_parsed;
	}

	/* Allocate place for pixel data */
	xpm_meta.pixdata = malloc(ncolors * sizeof(*(xpm_meta.pixdata)));
	if (NULL == xpm_meta.pixdata) {
		DPRINTF("Can't allocate memory for pixel data array\n");
		goto free_xpm_parsed;	/* Colors are freed by same function */
	}

	/* Allocate place for pixel names */
	xpm_meta.pixnames = malloc(ncolors * (chpp + 1) * sizeof(char));
	if (NULL == xpm_meta.pixnames) {
		DPRINTF("Can't allocate memory for pixel names array\n");
		goto free_pixdata;
	}

	/* Parse colors data */
	if ( -1 == xpm_parse_colors(xpm_data + 1, bpp, &xpm_meta) )
	{
		DPRINTF("Can't parse xpm colors\n");
		goto free_pixnames;
	}

	/* Allocate memory for pixels data */
	xpm_parsed->pixels = malloc(width * chpp * height * sizeof(*(xpm_parsed->pixels)));
	if (NULL == xpm_parsed->pixels) {
		DPRINTF("Can't allocate memory for xpm pixels data\n");
		goto free_pixnames;
	}

	/* Parse pixels data */
	if ( -1 == xpm_parse_pixels(xpm_data + 1 + ncolors, &xpm_meta) )
	{
		DPRINTF("Can't parse xpm pixels\n");
		goto free_pixnames;
	}

	free(xpm_meta.pixnames);
	free(xpm_meta.pixdata);
	return xpm_parsed;

free_pixnames:
	free(xpm_meta.pixnames);

free_pixdata:
	free(xpm_meta.pixdata);

free_xpm_parsed:
	xpm_destroy_parsed(xpm_parsed);

free_nothing:
	return NULL;

}


/* Allocate bootconf structure */
struct xpmlist_t *create_xpmlist(unsigned int size)
{
	struct xpmlist_t *xl;

	xl = malloc(sizeof(*xl));
	if (NULL == xl) return NULL;

	xl->list = malloc( size * sizeof(*(xl->list)) );
	if (NULL == xl->list) {
		free(xl);
		return NULL;
	}

	xl->size = size;
	xl->fill = 0;

	return xl;
}


int addto_xpmlist(struct xpmlist_t *xl, struct xpm_parsed_t *xpm)
{
	if (NULL == xl) return -1;

	xl->list[xl->fill] = xpm;
	++xl->fill;

	/* Resize list when needed */
	if (xl->fill > xl->size) {
		struct xpm_parsed_t **new_list;

		xl->size <<= 1;	/* size *= 2; */
		new_list = realloc( xl->list, xl->size * sizeof(*(xl->list)) );
		if (NULL == new_list) {
			DPRINTF("Can't resize xpm list\n");
			return -1;
		}

		xl->list = new_list;
	}

	/* Return item No. */
	return xl->fill - 1;
}


struct xpm_parsed_t *xpm_by_tag(struct xpmlist_t *xl, int tag)
{
	int i;

	if (NULL == xl) return NULL;

	for (i = 0; i < xl->fill; i++)
		if ((NULL != xl->list[i]) && (xl->list[i]->tag == tag))
			return xl->list[i];
	return NULL;
}


/* Free bootconf structure */
void free_xpmlist(struct xpmlist_t *xl)
{
	int i;

	if (NULL == xl) return;

	for (i = 0; i < xl->fill; i++)
		xpm_destroy_parsed(xl->list[i]);
	free(xl);
}

