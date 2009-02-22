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

#include "xpm.h"
#include "rgbtab.h"

char *xpmColorKeys[] = {
	"s",				/* key #1: symbol */
	"m",				/* key #2: mono visual */
	"g4",				/* key #3: 4 grays visual */
	"g",				/* key #4: gray visual */
	"c",				/* key #5: color visual */
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
	char *qstart, *qs;	/* Start and end of quoted string */
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
								DPRINTF("Values: %d, %d, %d, %d\n", width, height, ncolors, chpp);
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
	int len;

	len = strlen(hex);
	if (len == 3 + 1) {			/* #abc */
		rgb->r = hchar2int(hex[1]);
		rgb->g = hchar2int(hex[2]);
		rgb->b = hchar2int(hex[3]);
	} else if (len == 6 + 1) {	/* abcdef */
		rgb->r = hchar2int(hex[1]) << 4 | hchar2int(hex[2]);
		rgb->g = hchar2int(hex[3]) << 4 | hchar2int(hex[4]);
		rgb->b = hchar2int(hex[5]) << 4 | hchar2int(hex[6]);
	} else if (len == 12 + 1) {	/* #32329999CCCC */
		/* so for now only take two digits */
		rgb->r = hchar2int(hex[1]) << 4 | hchar2int(hex[2]);
		rgb->g = hchar2int(hex[5]) << 4 | hchar2int(hex[6]);
		rgb->b = hchar2int(hex[9]) << 4 | hchar2int(hex[10]);
	} else {
		return -1;
	}

	return 0;
}


/* Create hash key from color key of XPM image */
hkey_t hkey_char(char *pixel)
{
	unsigned int len;
	hkey_t key = 0;
	char *p;

	if (NULL == pixel) return 0;

	len = strlen(pixel);
	if ( len > sizeof(hkey_t)/sizeof(unsigned char) ) {
		/* We can't use pixel as key directly */
		return hkey_crc32(pixel, len);
	} else {
		for(p = pixel; p < pixel + len; p++) {
			key = key << (sizeof(unsigned char) << 3) | (unsigned char)*p;
		}
		return key;
	}
}


/* Process XPM image data and make it 'drawable' */
struct xpm_parsed_t *xpm_parse_image(char **xpm_data, const int rows,
		unsigned int bpp)
{
	int width = 0, height = 0, ncolors = 0, chpp = 0;	/* XPM image values */
	int len, i, is_transparent;
	struct xpm_parsed_t *xpm_parsed;	/* return value */
	/* Associative arrays: misc, color table, color names */
	struct htable_t *ht_ctable, *ht_cnames;
	struct hdata_t *hdata;
	/* Named color structure */
	struct xpm_named_color_t *cname;
	/* Color structures: array and temporary pointer */
	struct xpm_color_t *xpm_colors, *xpm_color;
	struct xpm_color_t **xpm_pixels, **xpm_pixel;
	char **cline;	/* XPM line colors array */
	enum xpm_ckey_t key;
	char **data = NULL, *p, *tmp;
	char *pixel;
	char ckey[3];	/* One of 'm', 'g', 'g4', 'c', 's' */
	char *color;
	unsigned char c;

	if (rows < 3) {
		DPRINTF("XPM image array should have at least 3 rows!\n");
		return NULL;
	}

	/** PARSE IMAGE VALUES **/
	width = get_nni(xpm_data[0], &p);
	height = get_nni(p, &tmp);
	ncolors = get_nni(tmp, &p);
	chpp = get_nni(p, &tmp);

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

	/* Allocate array of colors */
	xpm_colors = malloc(sizeof(*xpm_colors) * ncolors);
	if (NULL == xpm_colors) {
		DPRINTF("Can't allocate memory for xpm colors array\n");
		goto free_nothing;
	}
	xpm_color = xpm_colors;

	/* Allocate pixel */
	pixel = malloc((chpp + 1) * sizeof(char));
	if (NULL == pixel) {
		DPRINTF("Can't allocate memory for color key\n");
		goto free_xpm_colors;
	}

	/* pixel->color array */
	ht_ctable = htable_create(ncolors, 5);
	if (NULL == ht_ctable) {
		DPRINTF("Can't allocate memory for pixel->color table\n");
		goto free_pixel;
	}

	/* Allocate colors line array */
	cline = malloc((XPM_KEY_UNKNOWN) * sizeof(char *));
	if (NULL == cline) {
		DPRINTF("Can't allocate memory for colors line array\n");
		goto free_ht_ctable;
	}

	/* Allocate colors line array items */
	*cline = malloc((XPM_KEY_UNKNOWN) * MAX_COLORNAME_LEN);
	color = *cline;
	if (NULL == color) {
		DPRINTF("Can't allocate memory for colors line array\n");
		goto free_cline;
	}
	for(i = 1; i < (XPM_KEY_UNKNOWN); i++) {
		cline[i] = color + i * MAX_COLORNAME_LEN;
	}

	/* name->color array */
	ht_cnames = htable_create(XPM_ROWS(xpm_color_names), 5);
	if (NULL == ht_cnames) {
		DPRINTF("Can't allocate memory for name->color table\n");
		goto free_cline;
	}

	/* Fill name -> color array */
	cname = xpm_color_names;
	while (NULL != cname->name) {
		if ( -1 == htable_bin_insert(ht_cnames, hkey_crc32(cname->name,
				strlen(cname->name)), cname, sizeof(cname)) )
		{
			DPRINTF("Can't store name->color pair\n");
			goto free_ht_cnames;
		}
		++cname;
	}

	/** PARSE COLORS **/
	for (data = xpm_data + 1; data < xpm_data + ncolors + 1; data++) {

		DPRINTF("%s\n", *data);
		p = *data;

		/* Split first chpp chars (pixel) */
		strncpy(pixel, p, chpp);
		pixel[chpp] = '\0';
		p += chpp;

		/* Clear colors */
		*cline[XPM_KEY_MONO] = '\0';
		*cline[XPM_KEY_GRAY4] = '\0';
		*cline[XPM_KEY_GRAY] = '\0';
		*cline[XPM_KEY_COLOR] = '\0';

		/* Iterate over <ckey> <color> pairs */
		/* FIXME: <color> can have spaces in name */
		while (p < *(data+1)) {
			/* Get ckey */
			len = get_word(p, &tmp);
			if (0 == len) break;
			p = tmp + len;
			if ( len > sizeof(ckey) ) len = sizeof(ckey) - 1;
			strncpy(ckey, tmp, len);
			ckey[len] = '\0';

			/* Get color */
			len = get_word(p, &tmp);
			if (0 == len) break;

			switch (ckey[0]) {
			case 'c':	/* color */
				key = XPM_KEY_COLOR;
				break;
			case 'm':	/* mono */
				key = XPM_KEY_MONO;
				break;
			case 'g':	/* grey/grey4 */
				if ('4' == ckey[1]) {	/* grey4 */
					key = XPM_KEY_GRAY4;
				} else {	/* grey */
					key = XPM_KEY_GRAY;
				}
				break;
			default:	/* We are not using this ckey ('s') */
				continue;
			}

			/* Store color */
			p = tmp + len;
			if (len > MAX_COLORNAME_LEN) len = MAX_COLORNAME_LEN - 1;
			strncpy(cline[key], tmp, len);
			cline[key][len] = '\0';
		}

		/* Select color according to supplied bpp */
		color = NULL;
		if ( 1 == bpp ) {		/* mono */
			color = cline[XPM_KEY_MONO];
		} else if ( 2 == bpp) {	/* 4 grays */
			color = cline[XPM_KEY_GRAY4];
		}

		if ((NULL == color) || ('\0' == color[0])) {
			color = cline[XPM_KEY_COLOR];
			if ('\0' == color)
				color = cline[XPM_KEY_GRAY];
			if ('\0' == color)
				color = cline[XPM_KEY_GRAY4];
			if ('\0' == color)
				color = cline[XPM_KEY_MONO];

			if ('\0' == color) {
				DPRINTF("Wrong XPM format: wrong colors line '%s'\n", *data);
				goto free_ht_cnames;
			}
		}

		is_transparent = 0;

		/* Parse color to rgb */
		if ('#' == color[0]) {	/* hex */
			if ( -1 == hex2rgb(color, xpm_color) ) {
				DPRINTF("Can't parse hex color code '%s'\n", color);
				goto free_ht_cnames;
			}

		} else {	/* name */
			/* Strip spaces and lowercase */
			tmp = color;
			while('\0' != *tmp) {
				c = *tmp;
				if (' ' == c) {
					memmove(tmp, tmp+1, color + strlen(color) - tmp);
					continue;
				} else {
					*tmp = tolower(c);
				}
				++tmp;
			}

			/* Check for transparent color */
			if( 0 == strcmp(color, "none") ) {
				is_transparent = 1;
			} else {

				/* check for "grey" */
				tmp = strstr(color, "grey");
				if (NULL != tmp) {
					tmp[2] = 'a';	/* Convert to "gray" */
				}

				hdata = htable_bin_search(ht_cnames, hkey_crc32(color, strlen(color)));

				if (NULL == hdata) {
					DPRINTF("Color name '%s' not in colors database, returning red\n", color);
					/* Return 'red' color like libXpm does */
					xpm_color->b = 0;
					xpm_color->g = 0;
					xpm_color->r = 0xFF;
				} else {
					cname = (struct xpm_named_color_t *)hdata->data;
					xpm_color->b = (cname->rgb) & 0x000000FFU;
					xpm_color->g = (cname->rgb >> 8) & 0x000000FFU;
					xpm_color->r = (cname->rgb >> 16) & 0x000000FFU;
				}

			}
		}

		/* Store color key and pointer to color to ht_ctable */
		tmp = NULL;

		if (-1 == htable_bin_insert(ht_ctable, hkey_char(pixel),
				(is_transparent ? (struct xpm_color_t **)&tmp : &xpm_color), sizeof(xpm_color)))
		{
			DPRINTF("Can't store pixel->cpointer pair\n");
			goto free_ht_cnames;
		}

		++xpm_color;
	}

	/* Destroy name -> color table */
	htable_destroy(ht_cnames);

	/* Destroy colors line array */
	free(cline[0]);
	free(cline);

	/* Allocate memory for pixels data */
	xpm_pixels = malloc(width * chpp * height * sizeof(*xpm_pixels));
	if (NULL == xpm_pixels) {
		DPRINTF("Can't allocate memory for xpm pixels data\n");
		goto free_ht_ctable;
	}

	xpm_pixel = xpm_pixels;

	/** PARSE PIXELS DATA **/
	/* data should be in right position already */
	for ( ; data < xpm_data + 1 + ncolors + height; data++) {

		DPRINTF("%s\n", *data);

		if (strlen(*data) != (width * chpp)) {
			DPRINTF("Wrong XPM format: pixel data length is not equal to width (%d != %d)\n",
			strlen(*data), width);
			goto free_xpm_pixels;
		}

		/* Iterate over pixels (every chpp chars) */
		for(p = *data; p < *data + width; p += chpp) {

			/* Get pixel color key  and search it in colors table */
			strncpy(pixel, p, chpp);
			pixel[chpp] = '\0';

			hdata = htable_bin_search(ht_ctable, hkey_char(pixel));
			if (NULL == hdata) {
				DPRINTF("Wrong XPM format: pixel pixel not found\n");
				goto free_xpm_pixels;
			}

			*xpm_pixel = *(struct xpm_color_t **)hdata->data;
			++xpm_pixel;
		}

	}

	/* Destroy colors table */
	htable_destroy(ht_ctable);
	free(pixel);

	xpm_pixel = xpm_pixels;
	for(i = 0; i < height; i++) {
		for(len = 0; len < width; len++) {
			xpm_color = *xpm_pixel;
			if (NULL == xpm_color) {
				DPRINTF("...... ");
			} else {
				DPRINTF("%.2x%.2x%.2x ", xpm_color->r, xpm_color->g, xpm_color->b);
			}
			++xpm_pixel;
		}
		DPRINTF("\n");
	}

	/* Prepare return values */
	xpm_parsed = malloc(sizeof(*xpm_parsed));
	if (NULL == xpm_parsed) {
		DPRINTF("Can't allocate memory for return values\n");
		goto free_xpm_pixels_colors;
	}

	xpm_parsed->width = width;
	xpm_parsed->height = height;
	xpm_parsed->ncolors = ncolors;
	xpm_parsed->chpp = chpp;
	xpm_parsed->colors = xpm_colors;
	xpm_parsed->pixels = xpm_pixels;

	return xpm_parsed;

free_ht_cnames:
	htable_destroy(ht_cnames);

free_cline:
	if (NULL != cline[0]) free(cline[0]);
	free(cline);

free_ht_ctable:
	htable_destroy(ht_ctable);

free_pixel:
	free(pixel);

free_xpm_colors:
	free(xpm_colors);

free_nothing:
	return NULL;

free_xpm_pixels:
	htable_destroy(ht_ctable);
	free(pixel);

free_xpm_pixels_colors:
	free(xpm_pixels);
	free(xpm_colors);
	return NULL;

}

