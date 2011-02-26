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

#ifndef _HAVE_TUI_H_
#define _HAVE_TUI_H_

#include "config.h"
#ifdef USE_TEXTUI

#include "menu.h"
#include "util.h"

typedef char * kx_term_color;

typedef struct {
	FILE *ts;
	int x,y;
	int height, width;
} kx_tui;


kx_tui *tui_init();

void tui_show_menu(kx_tui *tui, kx_menu *menu);

void tui_show_text(kx_tui *tui, kx_text *text);

void tui_show_msg(kx_tui *tui, const char *text);

void tui_destroy(kx_tui *tui);

#endif /* USE_TEXTUI */
#endif /* _HAVE_TUI_H_*/
