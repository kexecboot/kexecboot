/*
 *  kexecboot - A kexec based bootloader
 *  XPM parsing routines based on libXpm
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "xpm.h"

/* XPM pixel structure (internal) */
struct xpm_pixel_t {
	char *pixel;
	struct rgb_color *rgb;
};

/* XPM metadata (internal, not needed for drawing code) */
struct xpm_meta_t {
	kx_picture *xpm_parsed;
	unsigned int chpp;		/* number of characters per pixel */
	unsigned int ncolors;	/* number of colors */
	char *pixnames;		/* place for pixel names */
	struct xpm_pixel_t *pixdata;	/* array of named pixels */
	unsigned int ctable_size;		/* color lookup table size */
	struct rgb_color **ctable;	/* color lookup table */
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
	struct rgb_color *xpm_color, **ctable;
	char **data, **e, *p;
	char *color;
	char *pixel;
	struct xpm_pixel_t *pdata;
	unsigned char c1, c2;
	/* Array of colors in line */
	char *colors[XPM_KEY_SYMBOL];
	/* Color line buffer */
	char line[MAX_XPM_CLINE_SIZE];

	xpm_color = xpm_meta->xpm_parsed->colors;
	pixel = xpm_meta->pixnames;
	ctable = xpm_meta->ctable;
	pdata = xpm_meta->pixdata;
	chpp = xpm_meta->chpp;

	e = xpm_data + xpm_meta->ncolors;
	for (data = xpm_data; data < e; data++) {

		/* If we have ctable then don't use pixnames */
		if (NULL != ctable) {
			/* Skip pixel chars. Will be accessible via *data */
			p = *data + chpp;
		} else {
			p = *data;
			/* Split first chpp chars (pixel) */
			strncpy(pixel, p, chpp);
			pixel[chpp] = '\0';
		}

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

		if (1 == rc) {	/* transparent pixel */
			pdata->rgb = NULL;
		} else {
			pdata->rgb = xpm_color;
		}

		/* Insert pixel into lookup table */
		if (NULL != ctable) {
			pdata->pixel = NULL;
			c1 = (unsigned char)(*data)[0];
			if (2 == chpp) c2 = (unsigned char)(*data)[1];

			if ( (c1 < 32) || (c1 > 127) ||
					( (2 == chpp) && ( (c2 < 32) || (c2 > 127) ) )
			) {
				DPRINTF("Pixel char is out of range [32-127]\n");
			} else {
				c1 -= ' ';
				if (1 == chpp) {
					ctable[c1] = pdata->rgb;
				} else {
					/* (y * 96 + x) */
					ctable[XPM_ASCII_RANGE(c1) + c2 - ' '] = pdata->rgb;
				}
			}
		} else {
			pdata->pixel = pixel;
			pixel += chpp + 1;	/* Go next place */
		}

		++xpm_color;
		++pdata;
	}

	return 0;
}


/* Local function to parse pixels data */
int xpm_parse_pixels(char **xpm_data, struct xpm_meta_t *xpm_meta)
{
	int chpp, width, i, dlen;
	struct rgb_color **xpm_pixel, **ctable;
	struct xpm_pixel_t *pdata, *edata;
	char **data, **e;
	char *p;
	unsigned char c1, c2;

	chpp = xpm_meta->chpp;
	edata = xpm_meta->pixdata + xpm_meta->ncolors;
	ctable = xpm_meta->ctable;

	width = xpm_meta->xpm_parsed->width;
	xpm_pixel = xpm_meta->xpm_parsed->pixels;
	e = xpm_data + xpm_meta->xpm_parsed->height;
	for (data = xpm_data; data < e; data++) {

		dlen = strlen(*data);
		if (dlen != (width * chpp)) {
			DPRINTF("Wrong XPM format: pixel data length is not equal to width (%d != %d)\n",
			dlen, width * chpp);
		}

		/* Iterate over pixels (every chpp chars) */
		for (p = *data; p < *data + dlen; p += chpp) {

			if (NULL != ctable) {
				/* We have lookup table - use it */
				c1 = (unsigned char)*p;
				if (2 == chpp) c2 = (unsigned char)*(p+1);
				if ( (c1 < 32) || (c1 > 127) ||
						( (2 == chpp) && ( (c2 < 32) || (c2 > 127) ) )
				) {
					DPRINTF("Pixel char is out of range [32-127]\n");
					*xpm_pixel = NULL; 	/* Consider this pixel as transparent */
				} else {
					c1 -= ' ';
					if (1 == chpp) {
						*xpm_pixel = ctable[c1];
					} else {
						*xpm_pixel = ctable[XPM_ASCII_RANGE(c1) + c2 - ' '];
					}
				}
			} else {
				/* Search pixel */
				i = 0;	/* Used as flag */
				for (pdata = xpm_meta->pixdata; pdata < edata; pdata++) {
					if ( 0 == strncmp(pdata->pixel, p, chpp) ) {
						*xpm_pixel = pdata->rgb;
						i = 1;
						break;
					}
				}
				if (0 == i) *xpm_pixel = NULL;	/* Consider this pixel as transparent */
			}

			++xpm_pixel;
		}

	}

	return 0;
}



/* Process XPM image data and make it 'drawable' */
kx_picture *xpm_parse_image(char **xpm_data, const int rows,
		unsigned int bpp)
{
	int width = 0, height = 0, ncolors = 0, chpp = 0;	/* XPM image values */
	kx_picture *xpm_parsed;	/* return value */
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

	/* Allocate colors lookup table when chpp is 1 or 2 */
	if (chpp < 3) {
		xpm_meta.ctable_size = XPM_ASCII_RANGE(1);		/* 96 */
		if (2 == chpp) xpm_meta.ctable_size = XPM_ASCII_RANGE(xpm_meta.ctable_size);	/* (96 * 96) */

		xpm_meta.ctable = malloc(xpm_meta.ctable_size * sizeof(xpm_meta.ctable));
		if (NULL == xpm_meta.ctable) {
			DPRINTF("Can't allocate memory for colors lookup table\n");
			xpm_meta.ctable_size = 0;
		}
		xpm_meta.pixnames = NULL;
	} else {
		xpm_meta.ctable_size = 0;
		xpm_meta.ctable = NULL;
	}

	/* If we have no lookup table then allocate place for pixel names */
	if (NULL == xpm_meta.ctable) {
		xpm_meta.pixnames = malloc(ncolors * (chpp + 1) * sizeof(char));
		if (NULL == xpm_meta.pixnames) {
			DPRINTF("Can't allocate memory for pixel names array\n");
			goto free_pixdata;
		}
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

	dispose(xpm_meta.ctable);
	dispose(xpm_meta.pixnames);
	free(xpm_meta.pixdata);
	return xpm_parsed;

free_pixnames:
	dispose(xpm_meta.ctable);
	dispose(xpm_meta.pixnames);

free_pixdata:
	free(xpm_meta.pixdata);

free_xpm_parsed:
	fb_destroy_picture(xpm_parsed);

free_nothing:
	return NULL;

}

