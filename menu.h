/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2009-2011 Yuri Bushmelev <jay4mail@gmail.com>
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

typedef int kx_menu_id;
typedef unsigned int kx_menu_dim;

struct kx_menu;
struct kx_menu_level;

typedef struct {
	kx_menu_id id;				/* Unique id */
	char *label;				/* Item label */
	char *description;			/* Item description */
	void *data;					/* User driven data */
	struct kx_menu_level *submenu;	/* Sub-menu if any */
} kx_menu_item;

typedef struct kx_menu_level {
	kx_menu_dim size;			/* Allocated items count */
	kx_menu_dim count;			/* Filled items count */
	kx_menu_dim current_no;		/* Current active item No */
	kx_menu_item *current;		/* Current active item */
	struct kx_menu_level *parent;	/* Upper menu level */
	kx_menu_item **list;		/* Menu items array */
} kx_menu_level;

typedef struct kx_menu {
	kx_menu_id next_id;
	kx_menu_dim size;			/* Allocated items count */
	kx_menu_dim count;			/* Filled items count */
	kx_menu_level *top;			/* Main menu */
	kx_menu_level *current;		/* Current active menu */
	kx_menu_level **list;		/* Menu levels array */
} kx_menu;


/* Create menu of 'size' submenus/levels */
kx_menu *menu_create(kx_menu_dim size);

/* Get next available menu id */
kx_menu_id menu_get_next_id(kx_menu *menu);

/* Select next/prev/first item in current level */
kx_menu_dim menu_item_select(kx_menu *menu, int direction);

/* Create menu level (submenu) of 'size' items */
kx_menu_level *menu_level_create(kx_menu *menu, kx_menu_dim size, 
		kx_menu_level *parent);

/* Add menu item to menu level */
kx_menu_item *menu_item_add(kx_menu_level *level, kx_menu_id id,
		char *label, char *description, kx_menu_level *submenu);

void menu_item_set_data(kx_menu_item *item, void *data);

void menu_destroy(kx_menu *menu, int destroy_data);


#endif /* _HAVE_MENU_H_*/
