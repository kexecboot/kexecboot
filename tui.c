/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2011 Yuri Bushmelev <jay4mail@gmail.com>
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
#ifdef USE_TEXTUI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __KLIBC__
/* KLIBC have no TIOCGWINSZ and struct winsize */
#define TIOCGWINSZ 0x5413
struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};
#else
#include <sys/ioctl.h>	/* struct winsize */
#endif 

#include <signal.h>

#include "tui.h"
#include "termseq.h"
#include "res/theme.h"

/* Get text size */
void term_text_size(int *width, int *height, const char *text)
{
	char *c;
	int n, w, h, mw;

	n = strlenn(text);
	if (0 == n) {
		*width = 0;
		*height = 0;
		return;
	}

	h = 1;
	mw = w = 0;

	for(c = (char *)text; *c; c++){
		if (*c == '\n') {
			if (w > mw) mw = w;
			w = 0;
			++h;
			continue;
		}

		++w;
	}

	*width = (w > mw) ? w : mw;
	*height = h;
}


/* Clear/fill terminal with color */
void term_clear(kx_tui *tui, char *cseq, int x, int y)
{
	fprintf(tui->ts, "%s" TERM_CSI_ED TERM_CSI "%d;%d" TERM_CUP,
			cseq, y+1, x+1);
}


/* Read height/width from terminal and store into term structure */
void term_reread_size(kx_tui *tui)
{
	struct winsize sz;

	memset(&sz, 0, sizeof(sz));
	ioctl(fileno(tui->ts), TIOCGWINSZ, &sz);
	tui->width  = sz.ws_col;
	tui->height = sz.ws_row;
}


static void sigwinch_handler(int sig_num)
{
	/* FIXME: process terminal size changes */
}


kx_tui *tui_init(FILE *ts)
{
	kx_tui *tui;

	tui = malloc(sizeof(*tui));
	if (!tui) {
		DPRINTF("Can't allocate memory for TUI structure");
		return NULL;
	}

	tui->ts = ts;
	/* FIXME: process terminal size changes */
	/* signal(SIGWINCH, sigwinch_handler); */

	term_reread_size(tui);

	return tui;
}


void tui_show_menu(kx_tui *tui, kx_menu *menu)
{
	int i,j;
	int slots = TUI_LYT_MENU_HEIGHT/TUI_LYT_MNI_HEIGHT;
	kx_menu_level *ml;
	kx_menu_item *mi;
	static int firstslot=0;
	int cur_no;

	/* Goto 1,1; switch color; draw 3 lines */
	fprintf(tui->ts, TERM_CSI_ED TERM_CSI "1;1" TERM_CUP TUI_CLR_BG TERM_CSI_EL "\n"
		" KEXECBOOT" TERM_CSI_EEL "\n" TERM_CSI_EL "\n");

	ml = menu->current;			/* active menu level */
	cur_no = ml->current_no;	/* active menu item index */

	if(cur_no < firstslot)
		firstslot = cur_no;
	if(cur_no > firstslot + slots -1)
		firstslot = cur_no - (slots -1);

	for(i=1, j=firstslot; i <= slots && j< ml->count; i++, j++) {
		mi = ml->list[j];
		if (j == cur_no) {
			fprintf(tui->ts, " %s %-40s %s\n %s %40s %s\n",
				TERM_CSI TERM_RV TERM_SGR, mi->label, TERM_CSI TERM_NON_RV TERM_SGR TERM_CSI_EEL,
				TERM_CSI TERM_RV TERM_SGR, (mi->description ? mi->description : ""), TERM_CSI TERM_NON_RV TERM_SGR TERM_CSI_EEL);
		} else {
			fprintf(tui->ts, "  %-40s%s\n  %40s%s\n",
				mi->label, TERM_CSI_EEL,
				(mi->description ? mi->description : ""), TERM_CSI_EEL);
		}
	}
}

void tui_show_text(kx_tui *tui, kx_text *text)
{
	int i, y, w, h;
	int max_y;
	
	/* Goto 1,1; switch color; draw 3 lines */
	fprintf(tui->ts, TERM_CSI_ED TERM_CSI "1;1" TERM_CUP TUI_CLR_BG TERM_CSI_EL "\n"
		" KEXECBOOT" TERM_CSI_EEL "\n" TERM_CSI_EL "\n");

	/* No text to show */
	if ((!text) || (text->rows->fill <= 1)) return;

	/* Size constraints */
	max_y = tui->height - 1;

	for (i = text->current_line_no, y = TUI_LYT_MENU_TOP;
		( (i < text->rows->fill) && (y < max_y) );
		 i++
	) {
		term_text_size(&w, &h, text->rows->list[i]);
		/* FIXME: wrap long lines */
		fprintf(tui->ts, " %s\n",
			text->rows->list[i]);
		y += h;
	}
}

void tui_show_msg(kx_tui *tui, const char *text)
{
	/* Goto 1,1; switch color; draw 3 lines */
	fprintf(tui->ts, TERM_CSI_ED TERM_CSI "1;1" TERM_CUP TUI_CLR_BG TERM_CSI_EL "\n"
		" %s" TERM_CSI_EEL "\n" TERM_CSI_EL "\n", text);
}

void tui_destroy(kx_tui *tui)
{
	dispose(tui);
}

#endif /* USE_TEXTUI */
