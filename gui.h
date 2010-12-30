/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2009-2010 Yuri Bushmelev <jay4mail@gmail.com>
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
#include "fb.h"
#include "xpm.h"
#include "menu.h"

enum icon_id_t {
	ICON_LOGO = 0,
	ICON_SYSTEM,
	ICON_BACK,
	ICON_REBOOT,
	ICON_RESCAN,
	ICON_DEBUG,
	ICON_HD,
	ICON_SD,
	ICON_MMC,
	ICON_MEMORY,
	ICON_EXIT,

	ICON_ARRAY_SIZE		/* should be latest item */
};

struct gui_t {
	FB *fb;
	int x,y;
	int height, width;
#ifdef USE_BG_BUFFER
	char *bg_buffer;
#endif
	struct xpm_parsed_t **icons;
	struct xpmlist_t *loaded_icons;	/* Custom icons */
	struct xpmlist_t *menu_icons;	/* Custom menu associated icons */
};


struct gui_t *gui_init(int angle);

void gui_show_menu(struct gui_t *gui, struct menu_t *menu, int current);

void gui_show_text(struct gui_t *gui, const char *text);

void gui_destroy(struct gui_t *gui);

#endif /* _HAVE_GUI_H_*/
