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

#include "config.h"

#ifdef USE_ICONS
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "xpm.h"

/* XPM metadata (internal, not needed for drawing code) */
struct xpm_meta_t {
	kx_picture *xpm_parsed;
	unsigned int chpp;			/* number of characters per pixel */
	unsigned int ncolors;		/* number of colors */
	unsigned int ctable_size;	/* color lookup table size */
	kx_rgba *ctable;			/* color lookup table */
	char *cids;				/* array of color id's */
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
		return -1;
	}

	f = open(filename, O_RDONLY);
	if (f < 0) {
		log_msg(lg, "Can't open %s: %s", filename, ERRMSG);
		return -1;
	}

	if ( -1 == fstat(f, &sb) ) {
		log_msg(lg, "Can't stat %s: %s", filename, ERRMSG);
		close(f);
		return -1;
	}

	/* Check file size */
	if (sb.st_size > MAX_XPM_FILE_SIZE) {
		log_msg(lg, "%s is too big (%d bytes)", filename, (int)sb.st_size);
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
							log_msg(lg, "XPM data string is too long");
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
									DPRINTF("Can't allocate memory for values");
									fail = 1;
									break;
								}

								/* Allocate memory for XPM data array */
								data = malloc((ncolors + height + 1) * sizeof(*data));
								if (NULL == data) {
									free(qs);
									DPRINTF("Can't allocate memory for values");
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
									DPRINTF("Can't allocate memory for colors or pixels");
									fail = 2;
									break;
								}

								/* Append string to array */
								data[n] = qs;
								/* DPRINTF("%d: %s", n, data[n]); */
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
				log_msg(lg, "XPM data string is too long");
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
	int chpp;
	kx_rgba cval, *ctable;
	unsigned char c1, c2;
	char **data, *color, *cidptr;
	/* Array of colors in line */
	char *colors[XPM_KEY_SYMBOL];
	/* Color line buffer */
	char line[MAX_XPM_CLINE_SIZE];

	c1 = c2 = '\0';
	chpp = xpm_meta->chpp;
	cidptr = xpm_meta->cids;
	ctable = xpm_meta->ctable;

	for (data = xpm_data; data < xpm_data + xpm_meta->ncolors; data++) {

		/* Create temporary copy for parsing (w/o color id) */
		strncpy(line, *data + chpp, sizeof(line)-1);
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
				log_msg(lg, "Wrong XPM format: wrong colors line '%s'", *data);
				return -1;
			}
		}

		/* Get rgba value from color hex or name */
		if ('#' == *color) {
			cval = hex2rgba(color);		/* hex */
		} else {
			cval = cname2rgba(color);	/* name */
		}

		/* Store color value */
		/* NOTE: following conditions are mutually exclusive within
		 * single image so it's possible to move ctable pointer
		 */
		if (chpp <= 2) {
			/* Build colors lookup table */
			c1 = (unsigned char)(*data)[0];
			if (2 == chpp) c2 = (unsigned char)(*data)[1];

			if ( (c1 < 32) || (c1 > 127) ||
					( (2 == chpp) && ( (c2 < 32) || (c2 > 127) ) )
			) {
				log_msg(lg, "Pixel char is out of range [32-127]");
			} else {
				c1 -= ' ';
				if (1 == chpp) {
					ctable[c1] = cval;
				} else {
					/* (y * 96 + x) */
					ctable[XPM_ASCII_RANGE(c1) + c2 - ' '] = cval;
				}
			}
		} else {
			/* Build color id's array */
			strncpy(cidptr, *data, chpp);
			cidptr += chpp;
			/* Store color value */
			*(ctable++) = cval;
		}
	}
	return 0;
}


/* Local function to parse pixels data */
int xpm_parse_pixels(char **xpm_data, struct xpm_meta_t *xpm_meta)
{
	int chpp, cwidth, found, dlen;
	kx_rgba *ctable, *pixptr;
	char **data, *cidptr, *e;
	char *p;
	unsigned char c1, c2;

	c1 = c2 = '\0';
	chpp = xpm_meta->chpp;
	ctable = xpm_meta->ctable;
	cwidth = chpp * xpm_meta->xpm_parsed->width;
	pixptr = xpm_meta->xpm_parsed->pixels;
	e = xpm_meta->cids + xpm_meta->ncolors * chpp;

	for (data = xpm_data; data < xpm_data + xpm_meta->xpm_parsed->height; data++) {

		dlen = strlen(*data);
		if (dlen != cwidth) {
			log_msg(lg, "Wrong XPM format: pixel data length is not equal to width (%d != %d)",
			dlen, cwidth);
		}

		/* Iterate over pixels (every chpp chars) */
		for (p = *data; p < *data + dlen; p += chpp) {

			/* NOTE: following conditions are mutually exclusive within
			* single image so it's possible to move ctable pointer
			*/
			if (chpp <= 2) {
				/* Use lookup table */
				c1 = (unsigned char)*p;
				if (2 == chpp) c2 = (unsigned char)*(p+1);
				if ( (c1 < 32) || (c1 > 127) ||
						( (2 == chpp) && ( (c2 < 32) || (c2 > 127) ) )
				) {
					log_msg(lg, "Pixel char is out of range [32-127]");
					*pixptr = comp2rgba(0, 0, 0, 255);	/* Consider this pixel as transparent */
				} else {
					c1 -= ' ';
					if (1 == chpp) {
						*pixptr = ctable[c1];
					} else {
						*pixptr = ctable[XPM_ASCII_RANGE(c1) + c2 - ' '];
					}
				}
			} else {
				/* Search pixel */
				found = 0;	/* Used as flag */
				for (cidptr = xpm_meta->cids;
					cidptr < e;
					cidptr += chpp, ctable++
				) {
					if ( 0 == strncmp(cidptr, p, chpp) ) {
						*pixptr = *ctable;
						found = 1;
						break;
					}
				}
				if (0 == found) *pixptr = comp2rgba(0, 0, 0, 255);	/* Consider this pixel as transparent */
			}
			++pixptr;
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
		log_msg(lg, "XPM image array should have at least 3 rows!");
		return NULL;
	}

	/* Parse image values */
	width = get_nni(xpm_data[0], &p);
	height = get_nni(p, &p);
	ncolors = get_nni(p, &p);
	chpp = get_nni(p, &p);

	if (width < 0 || height < 0 || ncolors < 0 || chpp < 0) {
		log_msg(lg, "Wrong XPM format: wrong values (%d, %d, %d, %d)",
			width, height, ncolors, chpp);
		goto free_nothing;
	}

	if ( rows != (1 + ncolors + height) ) {
		log_msg(lg, "Wrong XPM format: passed and parsed sizes are not equal (%d != %d)",
			rows, 1 + ncolors + height);
		goto free_nothing;
	}

	if ( ncolors > (1 << (8 * chpp)) ) {
		log_msg(lg, "Wrong XPM format: there are more colors than char_per_pixel can serve (%d > %d)",
			ncolors, 1 << (8 * chpp) );
		goto free_nothing;
	}

	/* Prepare return values */
	xpm_parsed = malloc(sizeof(*xpm_parsed));
	if (NULL == xpm_parsed) {
		DPRINTF("Can't allocate memory for return values");
		goto free_nothing;
	}

	/* Store values */
	xpm_parsed->width = width;
	xpm_parsed->height = height;

	xpm_meta.ncolors = ncolors;
	xpm_meta.chpp = chpp;
	xpm_meta.xpm_parsed = xpm_parsed;

	/* Allocate place for color values */
	switch (chpp) {
	case 1:
		xpm_meta.ctable_size = XPM_ASCII_RANGE(1);		/* 96 */
		xpm_meta.cids = NULL;
		break;
	case 2:
		xpm_meta.ctable_size = XPM_ASCII_RANGE(xpm_meta.ctable_size);	/* (96 * 96) */
		xpm_meta.cids = NULL;
		break;
	default:
		xpm_meta.ctable_size = ncolors;

		/* Allocate place for color id's (ncolors * chpp chars)
		 * Only used when no lookup table is applicable
		 * NOTE: id's are stored w/o terminating '\0'
		 */
		xpm_meta.cids = malloc(ncolors * chpp * sizeof(*(xpm_meta.cids)));
		if (NULL == xpm_meta.cids) {
			DPRINTF("Can't allocate memory for colors id data array");
			goto free_xpm_parsed;	/* Colors are freed by same function */
		}
		break;
	}

	xpm_meta.ctable = malloc(xpm_meta.ctable_size * sizeof(*(xpm_meta.ctable)));
	if (NULL == xpm_meta.ctable) {
		DPRINTF("Can't allocate memory for colors lookup table");
		goto free_cids;
	}

	/* Parse colors data */
	if ( -1 == xpm_parse_colors(xpm_data + 1, bpp, &xpm_meta) )
	{
		log_msg(lg, "Can't parse xpm colors");
		goto free_ctable;
	}

	/* Allocate memory for pixels data */
	xpm_parsed->pixels = malloc(width * height * sizeof(*(xpm_parsed->pixels)));
	if (NULL == xpm_parsed->pixels) {
		DPRINTF("Can't allocate memory for xpm pixels data");
		goto free_ctable;
	}

	/* Parse pixels data */
	if ( -1 == xpm_parse_pixels(xpm_data + 1 + ncolors, &xpm_meta) )
	{
		log_msg(lg, "Can't parse xpm pixels");
		goto free_ctable;
	}

	free(xpm_meta.ctable);
	dispose(xpm_meta.cids);
	return xpm_parsed;

free_ctable:
	dispose(xpm_meta.ctable);

free_cids:
	dispose(xpm_meta.cids);

free_xpm_parsed:
	fb_destroy_picture(xpm_parsed);

free_nothing:
	return NULL;

}

#endif	// USE_ICONS
