/* bdf to C converter from
   
   BOGL - Ben's Own Graphics Library.
   Written by Ben Pfaff <pfaffben@debian.org>.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA. */

#define _GNU_SOURCE 1
#include <sys/types.h>
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wctype.h>
#include <errno.h>
#include <limits.h>


/* As a temporary measure, we do this here rather than in config.h,
   which would probably make more sense. */
#ifndef MB_LEN_MAX
#define MB_LEN_MAX 6		/* for UTF-8 */
#endif

/* Proportional font structure definition. */
struct bogl_font {
	char *name;		/* Font name. */
	int height;		/* Height in pixels. */
	int index_mask;		/* ((1 << N) - 1). */
	int *offset;		/* (1 << N) offsets into index. */
	int *index;
	/* An index entry consists of ((wc & ~index_mask) | width) followed
	   by an offset into content. A list of such entries is terminated
	   by the value 0. */
	u_int32_t *content;
	/* 32-bit right-padded bitmap array. The bitmap for a single glyph
	   consists of (height * ((width + 31) / 32)) values. */
	wchar_t default_char;
};

struct bogl_font *bogl_read_bdf(char *filename);
static void print_glyph(u_int32_t * content, int height, int w);

int main(int argc, char *argv[])
{
	struct bogl_font *font;
	int index_size = 0;
	int content_size = 0;
	int i, j, k, cp, n;
	char buf[MB_LEN_MAX + 1];

	setlocale(LC_ALL, "");

	/* Check for proper usage. */
	if (!argc == 2 ) {
		fprintf(stderr, "Usage:\n%s font.bdf > font.c\n", argv[0]);
		return EXIT_FAILURE;
	}

	/* Read font file. */
	font = bogl_read_bdf(argv[1]);
	if (!font) {
		return EXIT_FAILURE;
	}

	/* Output header. */
	printf("#include \"font.h\"\n");

	/* Output offsets, and get index_size and content_size. */
	printf("\n/* Offsets into index. */\n");
	printf("static int _%s_offset[%d] = {\n", font->name,
	       font->index_mask + 1);
	for (i = 0; i <= font->index_mask; i++) {
		printf("  %d, /* (0x%x) */\n", font->offset[i], i);
		for (j = font->offset[i]; font->index[j] != 0; j += 2) {
			k = font->index[j + 1] +
			    font->height *
			    (((font->index[j] & font->index_mask) +
			      31) / 32);
			if (k > content_size)
				content_size = k;
		}
		if (j > index_size)
			index_size = j;
	}
	++index_size;
	printf("};\n");

	/* Output index. */
	printf("\n/* Index into content data. */\n");
	printf("static int _%s_index[%d] = {\n", font->name, index_size);
	i = 0;
	while (i < index_size)
		if (font->index[i] != 0 && i < index_size - 1) {
			printf("  0x%x, %d,\n", font->index[i],
			       font->index[i + 1]);
			i += 2;
		} else if (font->index[i] == 0)
			printf("  %d,\n", font->index[i++]);
		else
			printf("  %d, /* Hm... */\n", font->index[i++]);
	printf("};\n");

	/* Print out each character's picture and data. */
	printf("\n/* Font character content data. */\n");
	printf("static u_int32_t _%s_content[] = {\n\n", font->name);
	cp = 0;
	while (cp < content_size) {
		int width = 0;
		for (i = 0; i <= font->index_mask; i++)
			for (j = font->offset[i]; font->index[j] != 0;
			     j += 2)
				if (font->index[j + 1] == cp) {
					wchar_t wc =
					    (font->index[j] & ~font->
					     index_mask) | i;
					int w =
					    font->index[j] & font->
					    index_mask;
					if (iswprint(wc)) {
						wctomb(0, 0);
						n = wctomb(buf, wc);
						buf[(n == -1) ? 0 : n] =
						    '\0';
						printf
						    ("/* %d: character %s (0x%lx), width %d */\n",
						     cp, buf, (long) wc,
						     w);
					} else
						printf
						    ("/* %d: unprintable character 0x%lx, width %d */\n",
						     cp, (long) wc, w);
					if (w > width)
						width = w;
				}
		if (width
		    && cp + font->height * ((width + 31) / 32) <=
		    content_size) {
			print_glyph(&font->content[cp], font->height,
				    width);
			printf("\n");
			cp += font->height * ((width + 31) / 32);
		} else
			printf("0x%08x,\n", font->content[cp++]);
	}
	printf("};\n\n");

	/* Print the font structure definition. */
	printf("/* Exported structure definition. */\n");
	printf("const struct Font %s_font = {\n", font->name);
	printf("  \"%s\",\n", font->name);
	printf("  %d,\n", font->height);
	printf("  0x%x,\n", font->index_mask);
	printf("  _%s_offset,\n", font->name);
	printf("  _%s_index,\n", font->name);
	printf("  _%s_content,\n", font->name);
	printf("};\n");

	return EXIT_SUCCESS;
}

/* Print a picture of the glyph for human inspection, then the hex
   data for machine consumption. */
static void print_glyph(u_int32_t * content, int height, int width)
{
	int i, j;

	printf("/* +");
	for (i = 0; i < width; i++)
		printf("-");
	printf("+\n");
	for (i = 0; i < height; i++) {
		printf("   |");
		for (j = 0; j < width; j++)
			putchar(content[i + (j / 32) * height] &
				(1 << (31 - j % 32)) ? '*' : ' ');
		printf("|\n");
	}
	printf("   +");
	for (i = 0; i < width; i++)
		printf("-");
	printf("+ */\n");

	for (i = 0; i < height * ((width + 31) / 32); i++)
		printf("0x%08x,\n", content[i]);
}


#define INDEX_BITS 8
#define INDEX_MASK ((1<<INDEX_BITS)-1)

struct bogl_glyph {
	unsigned long char_width;	/* ((wc & ~index_mask) | width) */
	struct bogl_glyph *next;
	u_int32_t *content;	/* (height * ((width + 31) / 32)) values */
};

/* Returns nonzero iff LINE begins with PREFIX. */
static inline int matches_prefix(const char *line, const char *prefix)
{
	return !strncmp(line, prefix, strlen(prefix));
}

/* Reads a .bdf font file in FILENAME into a struct bogl_font, which
   is returned.  On failure, returns NULL, in which case an error
   message can be retrieved using bogl_error(). */
struct bogl_font *bogl_read_bdf(char *filename)
{
	char *font_name;
	int font_height;
	struct bogl_glyph *font_glyphs[1 << INDEX_BITS];

	/* Line buffer, current buffer size, and line number in input file. */
	char *line;
	size_t line_size;
	int ln = 0;

	/* Number of characters in font, theoretically, and the number that
	   we've seen.  These can be different at the end because there may
	   be named characters in the font that aren't encoded into
	   character set. */
	int n_chars;
	int char_index;

	/* Font ascent and descent in pixels. */
	int ascent;
	int descent;

	/* Character to substitute when a character lacks a glyph of its
	   own.  Normally the space character. */
	int default_char;

	/* .bdf file. */
	FILE *file;

	/* Read a line from FILE.  Returns nonzero if successful, reports an
	   error message failing if not.  Strips trailing
	   whitespace from input line. */
	int read_line(void) {
		ssize_t len;

		ln++;
		len = getline(&line, &line_size, file);
		if (len == -1) {
			if (ferror(file)) {
				printf("reading %s: %s", filename,
				       strerror(errno));
				return 0;
			} else {
				printf("%s:%d: unexpected end of file",
				       filename, ln);
				return 0;
			}
		}

		while (len > 0 && isspace((unsigned char) line[len - 1]))
			line[--len] = 0;

		return 1;
	}

	/* Attempt to malloc NBYTES bytes.  Sets a BOGL error message on
	   failure.  Returns the result of the malloc() operation in any
	   case. */
	void *smalloc(size_t nbytes) {
		void *p = malloc(nbytes);
		if (p == NULL) {
			printf("%s:%d: virtual memory exhausted", filename,
			       ln);
			return 0;
		}
		return p;
	}

	/* Parse header information.  Returns nonzero if successful, reports
	   an error message failing if not.

	   On success will always: set ascent, descent, default_char,
	   font_height, and initialize font_glyphs. */
	int read_bdf_header(void) {
		int bbx = 0, bby = 0, bbw = 0, bbh = 0;
		ascent = descent = 0;
		default_char = ' ';

		for (;;) {
			if (!read_line())
				return 0;

			if (matches_prefix(line, "FONT_ASCENT "))
				ascent =
				    atoi(line + strlen("FONT_ASCENT "));
			else if (matches_prefix(line, "FONT_DESCENT"))
				descent =
				    atoi(line + strlen("FONT_DESCENT"));
			else if (matches_prefix(line, "DEFAULT_CHAR "))
				default_char =
				    atoi(line + strlen("DEFAULT_CHAR "));
			else if (matches_prefix(line, "FONTBOUNDINGBOX "))
				sscanf(line + strlen("FONTBOUNDINGBOX "),
				       "%d %d %d %d", &bbw, &bbh, &bbx,
				       &bby);
			else if (matches_prefix(line, "CHARS "))
				break;
		}

		n_chars = atoi(line + strlen("CHARS "));
		if (n_chars < 1) {
			printf("%s:%d: font contains no characters",
			       filename, ln);
			return 0;
		}

		/* Adjust ascent and descent based on bounding box. */
		if (-bby > descent)
			descent = -bby;
		if (bby + bbh > ascent)
			ascent = bby + bbh;

		font_height = ascent + descent;
		if (font_height <= 0) {
			printf
			    ("%s:%d: font ascent (%d) + descent (%d) must be "
			     "positive", filename, ln, ascent, descent);
			return 0;
		}

		{
			int i;

			for (i = 0; i < (1 << INDEX_BITS); i++)
				font_glyphs[i] = NULL;
		}

		return 1;
	}

	/* Parse a character definition.  Returns nonzero if successful,
	   reports an error message failing  if not.

	   Sets the character data into font->content, font->offset, and
	   font->width as appropriate.  Updates char_index. */
	int read_character(void) {
		int encoding = -1;
		int width = INT_MIN;
		int bbx = 0, bby = 0, bbw = 0, bbh = 0;

		/* Read everything for this character up to the bitmap. */
		for (;;) {
			if (!read_line())
				return 0;

			if (matches_prefix(line, "ENCODING "))
				encoding =
				    atoi(line + strlen("ENCODING "));
			else if (matches_prefix(line, "DWIDTH "))
				width = atoi(line + strlen("DWIDTH "));
			else if (matches_prefix(line, "BBX "))
				sscanf(line + strlen("BBX "),
				       "%d %d %d %d", &bbw, &bbh, &bbx,
				       &bby);
			else if (matches_prefix(line, "BITMAP"))
				break;
		}

		/* Adjust width based on bounding box. */
		if (width == INT_MIN) {
			printf("%s:%d: character width not specified",
			       filename, ln);
			return 0;
		}
		if (bbx < 0) {
			width -= bbx;
			bbx = 0;
		}
		if (bbx + bbw > width)
			width = bbx + bbw;

		/* Put the character's encoding into the font table. */
		if (encoding != -1 && width <= INDEX_MASK) {
			u_int32_t *content, *bm;
			int i;
			struct bogl_glyph **t;

			t = &font_glyphs[encoding & INDEX_MASK];
			while (*t &&
			       (((*t)->char_width & ~INDEX_MASK) !=
				(encoding & ~INDEX_MASK)))
				t = &((*t)->next);
			if (*t) {
				printf("duplicate entry for character");
				return 0;
			}

			*t = smalloc(sizeof(struct bogl_glyph));
			if (*t == NULL)
				return 0;
			content = smalloc(sizeof(u_int32_t) *
					  font_height * ((width + 31) /
							 32));
			if (content == NULL) {
				free(*t);
				*t = NULL;
				return 0;
			}

			memset(content, 0,
			       sizeof(u_int32_t) * font_height *
			       ((width + 31) / 32));
			(*t)->char_width =
			    (encoding & ~INDEX_MASK) | width;
			(*t)->next = NULL;
			(*t)->content = content;

			/* Read the glyph bitmap. */
			/* FIXME: This won't work for glyphs wider than 32 pixels. */
			bm = content;
			for (i = 0;; i++) {
				int row;

				if (!read_line())
					return 0;
				if (matches_prefix(line, "ENDCHAR"))
					break;

				if (encoding == -1)
					continue;

				row =
				    font_height - descent - bby - bbh + i;
				if (row < 0 || row >= font_height)
					continue;
				bm[row] =
				    strtol(line, NULL,
					   16) << (32 - 4 * strlen(line) -
						   bbx);
			}
		}

		/* Advance to next glyph. */
		if (encoding != -1)
			char_index++;

		return 1;
	}

	void free_font_glyphs(void) {
		int i;
		struct bogl_glyph *t, *tnext;

		for (i = 0; i < (1 << INDEX_BITS); i++)
			for (t = font_glyphs[i]; t != NULL; t = tnext) {
				tnext = t->next;
				free(t);
			}
	}

	/* Open the file. */
	file = fopen(filename, "r");
	if (file == NULL) {
		printf("opening %s: %s\n", filename, strerror(errno));
		return NULL;
	}

	/* Make the font name based on the filename.  This is probably not
	   the best thing to do, but it seems to work okay for now. */
	{
		unsigned char *cp;

		font_name = strdup(filename);
		if (font_name == NULL) {
			printf("virtual memory exhausted");
			goto lossage;
		}

		cp = strstr(font_name, ".bdf");
		if (cp)
			*cp = 0;
		for (cp = (unsigned char *) font_name; *cp; cp++)
			if (!isalnum(*cp))
				*cp = '_';
	}

	line = NULL;
	line_size = 0;

	char_index = 0;

	/* Read the header. */
	if (!read_bdf_header())
		goto lossage;

	/* Read all the glyphs. */
	{
		for (;;) {
			if (!read_line())
				goto lossage;

			if (matches_prefix(line, "STARTCHAR ")) {
#if 0
				if (char_index >= n_chars) {
					printf
					    ("%s:%d: font contains more characters than "
					     "declared", filename, ln);
					goto lossage;
				}
#endif

				if (!read_character())
					goto lossage;
			} else if (matches_prefix(line, "ENDFONT"))
				break;
		}

		/* Make sure that we found at least one encoded character. */
		if (!char_index) {
			printf
			    ("%s:%d: font contains no encoded characters",
			     filename, ln);
			goto lossage;
		}
	}

	/* Build the bogl_font structure. */
	{
		struct bogl_font *font = NULL;
		int *offset = NULL;
		int *index = NULL;
		u_int32_t *content = NULL;
		int index_size = 0, indexp = 0;
		int content_size = 0, contentp = 0;
		int i, j;

		for (i = 0; i < (1 << INDEX_BITS); i++) {
			struct bogl_glyph *t = font_glyphs[i];
			if (t != NULL) {
				for (; t != NULL; t = t->next) {
					index_size += 2;
					content_size +=
					    font_height *
					    (((t->
					       char_width & INDEX_MASK) +
					      31) / 32);
				}
				++index_size;
			}
		}

		font = smalloc(sizeof(struct bogl_font));
		offset = smalloc(sizeof(int) * (1 << INDEX_BITS));
		index = smalloc(sizeof(int) * index_size);
		content = smalloc(sizeof(u_int32_t) * content_size);
		if (font == NULL || offset == NULL || index == NULL
		    || content == NULL) {
			free(font), free(offset), free(index),
			    free(content);
			goto lossage;
		}

		for (i = 0; i < (1 << INDEX_BITS); i++) {
			struct bogl_glyph *t = font_glyphs[i];
			if (t == NULL)
				offset[i] = index_size - 1;
			else {
				offset[i] = indexp;
				for (; t != NULL; t = t->next) {
					int n = font_height *
					    (((t->
					       char_width & INDEX_MASK) +
					      31) / 32);
					index[indexp++] = t->char_width;
					index[indexp++] = contentp;
					for (j = 0; j < n; j++)
						content[contentp++] =
						    t->content[j];
				}
				index[indexp++] = 0;
			}
		}
#if 0
		if (indexp != index_size || contentp != content_size) {
			printf("Internal error");
			return NULL;
		}
#endif

		font->name = font_name;
		font->height = font_height;
		font->index_mask = INDEX_MASK;
		font->index = index;
		font->offset = offset;
		font->content = content;
		font->default_char = default_char;

		/* Clean up. */
		free_font_glyphs();
		fclose(file);
		free(line);
		return font;
	}

      lossage:
	/* Come here on error. */
	free_font_glyphs();
	free(line);
	fclose(file);
	return NULL;
}
