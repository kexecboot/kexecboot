/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2009 Yuri Bushmelev <jay4mail@gmail.com>
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

#include "fb.h"
#include "xpm.h"
#include "menu.h"

struct gui_t {
	FB *fb;
	struct xpm_parsed_t *icon_logo, *icon_system, *icon_cf, *icon_mmc, *icon_memory;
	struct xpmlist_t *loaded_icons;	/* Custom icons */
	struct xpmlist_t *menu_icons;	/* Custom menu associated icons */
};

struct gui_t *gui_init(int angle);

void gui_show_menu(struct gui_t *gui, struct menu_t *menu, int current);

void gui_show_text(struct gui_t *gui, const char *text);

void gui_destroy(struct gui_t *gui);

#endif /* _HAVE_GUI_H_*/
