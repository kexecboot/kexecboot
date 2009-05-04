/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2009 Yuri Bushmelev <jay4mail@gmail.com>
 *  Copyright (c) 2009 Omegamoon <omegamoon@gmail.com>
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

#include <stdlib.h>
#include <stdio.h>
#include "menu.h"
#include "util.h"

struct menu_t *menu_init(int size)
{
	struct menu_t *menu;

	menu = malloc(sizeof(*menu));
	if (NULL == menu) {
		DPRINTF("Can't allocate menu\n");
		return NULL;
	}

	menu->list = malloc(size * sizeof(*(menu->list)));
	if (NULL == menu->list) {
		DPRINTF("Can't allocate menu items array\n");
		free(menu);
		return NULL;
	}

	menu->size = size;
	menu->fill = 0;
	return menu;
}


void menu_destroy(struct menu_t *menu)
{
	int i;
	for (i = 0; i < menu->fill; i++)
		free(menu->list[i]);
	free(menu);
}


int menu_add_item(struct menu_t *menu, char *label, int tag, struct menu_t *submenu)
{
	struct menu_item_t *mi;

	if (NULL == menu) return -1;

	mi = malloc(sizeof(*mi));
	if (NULL == mi) {
		DPRINTF("Can't allocate memory for menu item\n");
		return -1;
	}

	mi->label = label;
	mi->tag = tag;
	mi->submenu = submenu;

	menu->list[menu->fill] = mi;
	++menu->fill;
	if (menu->fill == menu->size) {
		menu->size <<= 1;	/* size *= 2; */
		menu->list = realloc(menu->list, menu->size * sizeof(*(menu->list)));
	}

	return menu->fill - 1;
}
