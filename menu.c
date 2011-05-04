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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "menu.h"
#include "util.h"

/* Create menu of 'size' submenus/levels */
kx_menu *menu_create(kx_menu_dim size)
{
	kx_menu *menu;

	menu = malloc(sizeof(*menu));
	if (NULL == menu) {
		DPRINTF("Can't allocate menu");
		return NULL;
	}

	menu->list = malloc(size * sizeof(*(menu->list)));
	if (NULL == menu->list) {
		DPRINTF("Can't allocate menu levels array");
		free(menu);
		return NULL;
	}

	menu->size = size;
	menu->count = 0;
	menu->next_id = 0;
	return menu;
}

/* Get next available menu id */
kx_menu_id menu_get_next_id(kx_menu *menu)
{
	return menu->next_id++;
}

/* Create menu level (submenu) of 'size' items */
kx_menu_level *menu_level_create(kx_menu *menu, kx_menu_dim size,
		kx_menu_level *parent)
{
	kx_menu_level *level;
	
	if (!menu) return NULL;

	/* Resize list when needed before adding item */
	if (menu->count >= menu->size) {
		kx_menu_level **new_list;
		kx_menu_dim new_size;

		new_size = menu->size * 2;
		new_list = realloc(menu->list, new_size * sizeof(*(menu->list)));
		if (NULL == new_list) {
			DPRINTF("Can't resize menu levels list");
			return NULL;
		}

		menu->size = new_size;
		menu->list = new_list;
	}
	
	level = malloc(sizeof(*level));
	if (NULL == level) {
		DPRINTF("Can't allocate menu level");
		return NULL;
	}

	level->list = malloc(size * sizeof(*(level->list)));
	if (NULL == level->list) {
		DPRINTF("Can't allocate menu items array");
		free(level);
		return NULL;
	}

	level->size = size;
	level->count = 0;
	level->current_no = 0;
	level->parent = parent;
	
	menu->list[menu->count++] = level;
	
	return level;
}


/* Add menu item to menu level */
kx_menu_item *menu_item_add(kx_menu_level *level, kx_menu_id id,
		char *label, char *description, kx_menu_level *submenu)
{
	kx_menu_item *item;

	if (!level) return NULL;
	
	/* Resize list when needed before adding item */
	if (level->count >= level->size) {
		kx_menu_item **new_list;
		kx_menu_dim new_size;

		new_size = level->size * 2;
		new_list = realloc(level->list, new_size * sizeof(*(level->list)));
		if (NULL == new_list) {
			DPRINTF("Can't resize menu items list");
			return NULL;
		}

		level->size = new_size;
		level->list = new_list;
	}
	
	item = malloc(sizeof(*item));
	if (NULL == item) {
		DPRINTF("Can't allocate menu level");
		return NULL;
	}

	item->label = strdup(label);
	item->description = ( description ? strdup(description) : NULL );
	item->id = id;
	item->submenu = submenu;
	
	level->list[level->count++] = item;
	
	return item;
}


void menu_destroy(kx_menu *menu, int destroy_data)
{
	int i,j;
	kx_menu_level *ml;
	kx_menu_item *mi;
	
	/* remove all levels/submenus */
	for (i = 0; i < menu->count; i++) {
		ml = menu->list[i];
		if (ml) {
			/* remove all items */
			for (j = 0; j < ml->count; j++) {
				mi = ml->list[j];
				if (mi) {
					dispose(mi->label);
					dispose(mi->description);
					if (destroy_data && mi->data) free(mi->data);
					free(mi);
				}
			}
			free(ml->list);
			free(ml);
		}
	}
	free(menu->list);
	free(menu);
}


#define menu_item_set_current_and_return(level, index) \
	if ( (level)->list[(index)] ) { \
		(level)->current_no = (index); \
		(level)->current = (level)->list[(index)]; \
		return (index); \
	}

/* Select next/prev/first item in current level */
kx_menu_dim menu_item_select(kx_menu *menu, int direction)
{
	static kx_menu_level *ml;
	static kx_menu_dim cur_no, i, step, start;
	
	ml = menu->current;
	cur_no = ml->current_no;

	/* Just select first available item */
	if (0 == direction)
		for (i = 0; i < ml->count; i++)
			menu_item_set_current_and_return(ml, i);
	
	if (direction > 0) {
		step = 1;
		start = 0;
	} else {
		step = -1;
		start = ml->count - 1;
	}
	
	/* Try to get next/prev item */
	for (i = cur_no + step; ( (i < ml->count) && (i >= 0) ); i += step)
		menu_item_set_current_and_return(ml, i);
	
	/* We have reached end/begin of list -> wrap to begin/end */
	for (i = start; ( (i < ml->count) && (i >= 0) ); i += step)
		menu_item_set_current_and_return(ml, i);
	
	return 0;
}

/* Select no'th item in current level */
kx_menu_dim menu_item_select_by_no(kx_menu *menu, int no)
{
	static kx_menu_level *ml;
	static kx_menu_dim i, j;

	ml = menu->current;

	if (no >= ml->count) {
		return -1;
	}

	/* Rewind to item no */
	for (i=0, j=0; (i < ml->count) && (j < no); i++) {
		if (ml->list[i]) ++j;
	}

	menu_item_set_current_and_return(ml, i);

	/* we shouldn't reach this point */
	return -1;
}


inline void menu_item_set_data(kx_menu_item *item, void *data)
{
	item->data = data;
}
