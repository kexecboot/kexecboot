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

#ifndef _HAVE_MENU_H_
#define _HAVE_MENU_H_

#include "config.h"

struct menu_item_t;

struct menu_t {
	unsigned int size;				/* Allocated items count */
	unsigned int fill;				/* Filled items count */
	struct menu_item_t **list;		/* Menu items array */
};

struct menu_item_t {
	char *label;				/* Item label */
	int tag;					/* User-driven tag */
	struct menu_t *submenu;		/* Sub-menu if exists */
};


struct menu_t *menu_init(int size);

void menu_destroy(struct menu_t *menu);

int menu_add_item(struct menu_t *menu, char *label, int tag, struct menu_t *submenu);


#endif /* _HAVE_MENU_H_*/
