/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2009-2011 Yuri Bushmelev <jay4mail@gmail.com>
 *  Copyright (c) 2008 Thomas Kunze <thommycheck@gmx.de>
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

#ifndef _HAVE_GUI_H_
#define _HAVE_GUI_H_

#include "config.h"

#ifdef USE_FBMENU
#include "fb.h"
#include "menu.h"

#ifdef USE_ICONS
#include "xpm.h"

enum icon_id_t {
	ICON_LOGO = 0,
	ICON_STORAGE,
	ICON_MMC,
	ICON_MEMORY,
	ICON_SYSTEM,
	ICON_BACK,
	ICON_RESCAN,
	ICON_DEBUG,
	ICON_REBOOT,
	ICON_SHUTDOWN,
	ICON_EXIT,

	ICON_ARRAY_SIZE		/* should be latest item */
};
#endif

struct gui_t {
	FB *fb;
	int x,y;
	int height, width;
#ifdef USE_BG_BUFFER
	char *bg_buffer;
#endif
#ifdef USE_ICONS
	kx_picture **icons;
#endif
};


struct gui_t *gui_init(int angle);

void gui_show_menu(struct gui_t *gui, kx_menu *menu);

void gui_show_text(struct gui_t *gui, kx_text *text);

void gui_show_msg(struct gui_t *gui, const char *text);

/* Clear screen */
void gui_clear(struct gui_t *gui);

void gui_destroy(struct gui_t *gui);

#endif /* USE_FBMENU */
#endif /* _HAVE_GUI_H_*/
