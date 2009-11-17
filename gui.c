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

#include "res/theme.h"


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

	gui->icons = malloc(sizeof(*(gui->icons)) * ICON_ARRAY_SIZE);

	gui->icons[ICON_LOGO] = xpm_parse_image(logo_xpm, XPM_ROWS(logo_xpm), bpp);
	gui->icons[ICON_SYSTEM] = xpm_parse_image(system_xpm, XPM_ROWS(system_xpm), bpp);
	gui->icons[ICON_BACK] = xpm_parse_image(back_xpm, XPM_ROWS(back_xpm), bpp);
	gui->icons[ICON_REBOOT] = xpm_parse_image(reboot_xpm, XPM_ROWS(reboot_xpm), bpp);
	gui->icons[ICON_RESCAN] = xpm_parse_image(rescan_xpm, XPM_ROWS(rescan_xpm), bpp);
	gui->icons[ICON_DEBUG] = xpm_parse_image(debug_xpm, XPM_ROWS(debug_xpm), bpp);
	gui->icons[ICON_HD] = xpm_parse_image(hd_xpm, XPM_ROWS(hd_xpm), bpp);
	gui->icons[ICON_SD] = xpm_parse_image(sd_xpm, XPM_ROWS(sd_xpm), bpp);
	gui->icons[ICON_MMC] = xpm_parse_image(mmc_xpm, XPM_ROWS(mmc_xpm), bpp);
	gui->icons[ICON_MEMORY] = xpm_parse_image(memory_xpm, XPM_ROWS(memory_xpm), bpp);
//	gui->icons[ICON_EXIT] = xpm_parse_image(exit_xpm, XPM_ROWS(exit_xpm), bpp);
	gui->icons[ICON_EXIT] = NULL;

	gui->loaded_icons = NULL;
	gui->menu_icons = NULL;

	return gui;
}


/* Destroy gui */
void gui_destroy(struct gui_t *gui)
{
	if (NULL == gui) return;
	enum icon_id_t i;

	free_xpmlist(gui->menu_icons, 0);
	free_xpmlist(gui->loaded_icons, 1);

	for (i=ICON_LOGO; i<ICON_ARRAY_SIZE; i++) {
		xpm_destroy_parsed(gui->icons[i]);
	}
	free(gui->icons);

	fb_destroy(gui->fb);
	free(gui);
}


/* Draw background with logo and text */
void draw_background(struct gui_t *gui, const char *text)
{
	FB *fb = gui->fb;

	int margin = fb->width/40;

	/* Clear the background with #ecece1 */
	fb_draw_rect(fb, 0, 0, fb->width, fb->height, COLOR_BG);

	fb_draw_xpm_image(fb, 0, 0, gui->icons[ICON_LOGO]);

	fb_draw_text (fb, 32 + margin, margin, COLOR_TEXT,
		DEFAULT_FONT, text);
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
			COLOR_BRDR);
		fb_draw_rect(fb, margin, slot*height+margin, fb->width-2*margin,
			height-2*margin, COLOR_BG);
	}

	if (NULL != icon) {
		fb_draw_xpm_image(fb, margin, slot * height + margin, icon);
	}

	fb_draw_text (fb, 32 + 8 + margin, slot * height + 4, COLOR_TEXT,
			DEFAULT_FONT, item->label);

	if (NULL != item->submenu) {
		/* Draw something to show that here is submenu available */
	}
}


/* Display bootlist menu with selection */
void gui_show_menu(struct gui_t *gui, struct menu_t *menu, int current)
{
	int i,j;
	int slotheight = 40;	/* FIXME Fix hardcoded height */
	int slots = gui->fb->height/slotheight -1;
	// struct boot that is in fist slot
	static int firstslot=0;

	if (1 == menu->fill) {
		draw_background(gui, "No bootable devices found.\nR: Reboot  S: Rescan devices");
	} else {
		draw_background(gui, "KEXECBOOT - Linux bootloader");
	}

	if(current < firstslot)
		firstslot=current;
	if(current > firstslot + slots -1)
		firstslot = current - (slots -1);

	if (NULL == gui->menu_icons) {
		for(i=1, j=firstslot; i <= slots && j< menu->fill; i++, j++) {
			draw_slot(gui, menu->list[j], i, slotheight, j == current, NULL);
		}
	} else {
		for(i=1, j=firstslot; i <= slots && j< menu->fill; i++, j++) {
			draw_slot(gui, menu->list[j], i, slotheight, j == current,
					gui->menu_icons->list[j]);
		}
	}

	fb_render(gui->fb);
}


/* Display custom text near logo */
void gui_show_text(struct gui_t *gui, const char *text)
{
	draw_background(gui, text);
	fb_render(gui->fb);
}

