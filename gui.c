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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fb.h"
#include "xpm.h"
#include "gui.h"

#include "res/radeon-font.h"
#include "res/logo.xpm"
#include "res/cf.xpm"
#include "res/mmc.xpm"
#include "res/memory.xpm"


struct gui_t *gui_init(int angle)
{
	struct gui_t *gui;
	int bpp;

	gui = malloc(sizeof(*gui));
	if (NULL == gui) {
		DPRINTF("Can't allocate memory for GUI structure\n");
		return NULL;
	}

	/* init framebuffer */
	gui->fb = fb_new(angle);

	if (NULL == gui->fb) {
		DPRINTF("Can't initialize framebuffer\n");
		return NULL;
	}

	/* Parse compiled images.
	 * We don't care about result because drawing code is aware
	 */
	bpp = gui->fb->bpp;
	gui->icon_logo = xpm_parse_image(logo_xpm, XPM_ROWS(logo_xpm), bpp);
	gui->icon_cf = xpm_parse_image(cf_xpm, XPM_ROWS(cf_xpm), bpp);
	gui->icon_mmc = xpm_parse_image(mmc_xpm, XPM_ROWS(mmc_xpm), bpp);
	gui->icon_memory = xpm_parse_image(memory_xpm, XPM_ROWS(memory_xpm), bpp);

	return gui;
}


/* Destroy gui */
void gui_destroy(struct gui_t *gui)
{
	if (NULL == gui) return;

	xpm_destroy_parsed(gui->icon_logo);
	xpm_destroy_parsed(gui->icon_cf);
	xpm_destroy_parsed(gui->icon_mmc);
	xpm_destroy_parsed(gui->icon_memory);
	fb_destroy(gui->fb);
	free(gui);
}


/* Draw background with logo and text */
void draw_background(struct gui_t *gui, const char *text)
{
	FB *fb = gui->fb;

	int margin = fb->width/40;

	/* Clear the background with #ecece1 */
	fb_draw_rect(fb, 0, 0, fb->width, fb->height,0xec, 0xec, 0xe1);

	fb_draw_xpm_image(fb, 0, 0, gui->icon_logo);

	fb_draw_text (fb, 32 + margin, margin, 0, 0, 0,
		&radeon_font, text);
}


/* Draw one slot in menu */
void draw_slot(struct gui_t *gui, struct menu_item_t *item, int slot, int height,
		int iscurrent, struct xpm_parsed_t *icon)
{
	FB *fb = gui->fb;

	int margin = (height - 32)/2;
	if(!iscurrent)
		fb_draw_rect(fb, 0, slot*height, fb->width, height,
			0xec, 0xec, 0xe1);
	else { //draw red border
		fb_draw_rect(fb, 0, slot*height, fb->width, height,
			0xff, 0x00, 0x00);
		fb_draw_rect(fb, margin, slot*height+margin, fb->width-2*margin,
			height-2*margin, 0xec, 0xec, 0xe1);
	}

/* FIXME Disabled at moment of menu migration
	if (NULL != icon) {
		fb_draw_xpm_image(fb, 0, 0, icon);
	} else if(!strncmp(item->device, "/dev/hd", strlen("/dev/hd"))) {
		fb_draw_xpm_image(fb, margin, slot * height + margin, gui->icon_cf);
	} else if(!strncmp(item->device, "/dev/mmcblk", strlen("/dev/mmcblk"))) {
		fb_draw_xpm_image(fb, margin, slot * height + margin, gui->icon_mmc);
	} else if(!strncmp(item->device, "/dev/mtdblock", strlen("/dev/mtdblock"))) {
		fb_draw_xpm_image(fb, margin, slot * height + margin, gui->icon_memory);
	}
*/

	fb_draw_text (fb, 32 + margin, slot * height + 4, 0, 0, 0,
			&radeon_font, item->label);

	if (NULL != item->submenu) {
		/* Draw something to show that here is submenu available */
	}
}


/* Display bootlist menu with selection */
void gui_show_menu(struct gui_t *gui, struct menu_t *menu, int current,
		struct xpm_parsed_t **icons_array)
{
	int i,j;
	int slotheight = 40;	/* FIXME Fix hardcoded height */
	int slots = gui->fb->height/slotheight -1;
	// struct boot that is in fist slot
	static int firstslot=0;

	if (1 == menu->fill) {
		draw_background(gui, "No bootable devices found.\nInsert bootable device!\nR: Reboot  S: Rescan devices");
	} else {
		draw_background(gui, "Make your choice by selecting\nan item with the cursor keys.\nOK/Enter: Boot selected device\nR: Reboot  S: Rescan devices");
	}

	if(current < firstslot)
		firstslot=current;
	if(current > firstslot + slots -1)
		firstslot = current - (slots -1);
	if (NULL == icons_array) for(i=1, j=firstslot; i <= slots && j< menu->fill; i++, j++) {
		draw_slot(gui, menu->list[j], i, slotheight, j == current, NULL);
	} else for(i=1, j=firstslot; i <= slots && j< menu->fill; i++, j++) {
		draw_slot(gui, menu->list[j], i, slotheight, j == current, icons_array[j]);
	}
	fb_render(gui->fb);
}


/* Display custom text near logo */
void gui_show_text(struct gui_t *gui, const char *text)
{
	draw_background(gui, text);
	fb_render(gui->fb);
}

#if 0
/* Iterate through bootlist and associate icons with items */
/* FIXME This function is not working because of umounted devices */
int associate_icons(struct bootconf_t *bc, int bpp, struct xpm_parsed_t ***icons_array)
{
	int i, rows;
	char *tmp, **xpm_data;
	struct boot_item_t **pbi;
	struct xpm_parsed_t **icons, **pi, *icon;

	if (0 == bc->fill) return 0;	/* may be (-1)? */

	/* Allocate array of bl->fill num icons */
	icons = malloc(sizeof(icons) * bc->fill);
	if (NULL == icons) {
		DPRINTF("Can't allocate memory for icons array\n");
		*icons_array = NULL;
		return -1;
	}

	pbi = bc->list;
	pi = icons;
	for (i = 0; i < bc->fill; i++) {

		icon = NULL;

		tmp = (*pbi)->iconpath;
		if (NULL != tmp) {	/* Load and parse image */
			rows = xpm_load_image(&xpm_data, tmp);
			if (rows > 0) {
				icon = xpm_parse_image(xpm_data, rows, bpp);
				/* destroy loaded data - not needed anymore */
				xpm_destroy_image(xpm_data, rows);
			}
		}

		/* Store and go to next item */
		*pi = icon;
		++pbi;
		++pi;
	}
	*icons_array = icons;

	return i;
}


void free_associated_icons(struct xpm_parsed_t **icons_array, int ia_size)
{
	if ( (NULL == icons_array) || (0 == ia_size) ) return;

	int i;
	struct xpm_parsed_t **p = icons_array;

	for (i = 0; i < ia_size; i++) {
		if (NULL != *p) xpm_destroy_parsed(*p);
		++p;
	}
}
#endif
