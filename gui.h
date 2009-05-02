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

#include "fb.h"
#include "xpm.h"
#include "devicescan.h"	// FIXME should be replaced by menu.h

struct gui_t {
	FB *fb;
	struct xpm_parsed_t *icon_logo, *icon_cf, *icon_mmc, *icon_memory;
	struct xpm_parsed_t **icons;	/* Custom menu icons array */
	unsigned int icons_count;
};

struct gui_t *gui_init(int angle);

void gui_show_menu(struct gui_t *gui, struct bootconf_t *bc, int current,
		struct xpm_parsed_t **icons_array);

void gui_show_text(struct gui_t *gui, const char *text);

void gui_destroy(struct gui_t *gui);
