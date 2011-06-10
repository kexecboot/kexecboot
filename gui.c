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

#include "config.h"
#ifdef USE_FBMENU

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fb.h"
#include "xpm.h"
#include "gui.h"

#include "res/theme.h"


/* Draw background with logo */
void draw_background_low(struct gui_t *gui)
{
	static FB *fb;
	static int w, h;

	fb = gui->fb;

	/* Fill background */
	fb_draw_rect(fb, 0, 0, fb->width, fb->height, CLR_BG);

	/* Draw icon pad */
	fb_draw_rounded_rect(fb, gui->x + LYT_HDR_PAD_LEFT,
			gui->y + LYT_HDR_PAD_TOP,
			LYT_HDR_PAD_WIDTH, LYT_HDR_PAD_HEIGHT, CLR_BG_PAD);

	/* Draw icon */
	fb_draw_picture(fb, gui->x + LYT_HDR_PAD_LEFT + LYT_PAD_ICON_LOFF,
			gui->y + LYT_HDR_PAD_TOP + LYT_PAD_ICON_TOFF,
			gui->icons[ICON_LOGO]);

	/* Draw menu frame */
	fb_draw_rounded_rect(fb, gui->x + LYT_MENU_FRAME_LEFT,
			gui->y + LYT_MENU_FRAME_TOP,
			LYT_MENU_FRAME_WIDTH,
			LYT_MENU_FRAME_HEIGHT,
			CLR_MENU_FRAME);

	/* Draw menu area */
	fb_draw_rect(fb, gui->x + LYT_MENU_AREA_LEFT,
			gui->y + LYT_MENU_AREA_TOP,
			LYT_MENU_AREA_WIDTH,
			LYT_MENU_AREA_HEIGHT,
			CLR_MENU_BG);
	
	/* Draw kexecboot version right aligned at bottom */
	fb_text_size(gui->fb, &w, &h, DEFAULT_FONT, "v." PACKAGE_VERSION);
	fb_draw_text(fb, gui->x + LYT_MENU_AREA_LEFT + LYT_MENU_AREA_WIDTH - w,
			gui->y + LYT_MENU_FRAME_TOP + LYT_MENU_FRAME_HEIGHT + (LYT_FTR_HEIGHT - h)/2,
			CLR_BG_TEXT, DEFAULT_FONT, "v." PACKAGE_VERSION);
	
}


struct gui_t *gui_init(int angle)
{
	struct gui_t *gui;
	FB *fb;
	int bpp;

	gui = malloc(sizeof(*gui));
	if (NULL == gui) {
		DPRINTF("Can't allocate memory for GUI structure");
		return NULL;
	}

	/* init framebuffer */
	fb = fb_new(angle);

	if (NULL == fb) {
		log_msg(lg, "Can't initialize framebuffer");
		return NULL;
	}

	gui->fb = fb;

	/* Tune GUI size */
#ifdef USE_FBUI_WIDTH
	if (fb->width > USE_FBUI_WIDTH)
		gui->width = USE_FBUI_WIDTH;
	else
#endif
		gui->width = fb->width;

#ifdef USE_FBUI_HEIGHT
	if (fb->height > USE_FBUI_HEIGHT)
		gui->height = USE_FBUI_HEIGHT;
	else
#endif
		gui->height = fb->height;

	gui->x = (fb->width - gui->width)/2;
	gui->y = (fb->height - gui->height)/2;

	/* Parse compiled images.
	 * We don't care about result because drawing code is aware
	 */
	bpp = fb->bpp;

	gui->icons = malloc(sizeof(*(gui->icons)) * ICON_ARRAY_SIZE);

	gui->icons[ICON_LOGO] = xpm_parse_image(logo_xpm, ROWS(logo_xpm), bpp);
	gui->icons[ICON_STORAGE] = xpm_parse_image(storage_xpm, ROWS(storage_xpm), bpp);
	gui->icons[ICON_MMC] = xpm_parse_image(mmc_xpm, ROWS(mmc_xpm), bpp);
	gui->icons[ICON_MEMORY] = xpm_parse_image(memory_xpm, ROWS(memory_xpm), bpp);
	gui->icons[ICON_SYSTEM] = xpm_parse_image(system_xpm, ROWS(system_xpm), bpp);
	gui->icons[ICON_BACK] = xpm_parse_image(back_xpm, ROWS(back_xpm), bpp);
	gui->icons[ICON_RESCAN] = xpm_parse_image(rescan_xpm, ROWS(rescan_xpm), bpp);
	gui->icons[ICON_DEBUG] = xpm_parse_image(debug_xpm, ROWS(debug_xpm), bpp);
	gui->icons[ICON_REBOOT] = xpm_parse_image(reboot_xpm, ROWS(reboot_xpm), bpp);
	gui->icons[ICON_SHUTDOWN] = xpm_parse_image(shutdown_xpm, ROWS(shutdown_xpm), bpp);
	gui->icons[ICON_EXIT] = xpm_parse_image(exit_xpm, ROWS(exit_xpm), bpp);

#ifdef USE_BG_BUFFER
	/* Pre-draw background and store it in special buffer */
	draw_background_low(gui);
	gui->bg_buffer = fb_dump(fb);
#endif

	return gui;
}


/* Destroy gui */
void gui_destroy(struct gui_t *gui)
{
	if (NULL == gui) return;
	enum icon_id_t i;

	for (i=ICON_LOGO; i<ICON_ARRAY_SIZE; i++) {
		fb_destroy_picture(gui->icons[i]);
	}
	free(gui->icons);

	fb_destroy(gui->fb);
	free(gui);
}

/* Draw text */
void draw_bg_text(struct gui_t *gui, const char *text)
{
	static int w, h;

	/* Calculate text size */
	fb_text_size(gui->fb, &w, &h, DEFAULT_FONT, text);

	/* Draw text */
	fb_draw_text(gui->fb, gui->x + LYT_HDR_PAD_LEFT + LYT_HDR_PAD_WIDTH + 2 +
			(gui->width - (LYT_HDR_PAD_LEFT + LYT_HDR_PAD_WIDTH + 2)*2 - w - LYT_FRAME_SIZE)/2,
			gui->y + (LYT_MENU_FRAME_TOP - h)/2,
			CLR_BG_TEXT, DEFAULT_FONT, text);
}


/* Draw background and text */
void draw_background(struct gui_t *gui, const char *text)
{
#ifdef USE_BG_BUFFER
	if (NULL != gui->bg_buffer) {
		/* If we have bg buffer use it */
		fb_restore(gui->fb, gui->bg_buffer);
	} else {
		/* else draw bg */
		draw_background_low(gui);
		log_msg(lg, "bg_buffer is empty");
	}
#else
	/* Have bg buffer disabled. Draw bg */
	draw_background_low(gui);
#endif
	/* Draw text on bg */
	draw_bg_text(gui, text);
}


/* Draw one slot in menu */
void draw_slot(struct gui_t *gui, kx_menu_item *item, int slot, int height,
		int iscurrent)
{
	static FB *fb;
	static kx_rgba cbg, cpad, ctext, cline;
	static int slot_top, w, h, h2;
	static kx_picture *icon;
	
	fb = gui->fb;

	if (!iscurrent) {
		cbg =   CLR_MNI_BG;
		cpad =  CLR_MNI_PAD;
		ctext = CLR_MNI_TEXT;
		cline = CLR_MNI_LINE;
	} else {
		cbg =   CLR_SMNI_BG;
		cpad =  CLR_SMNI_PAD;
		ctext = CLR_SMNI_TEXT;
		cline = CLR_SMNI_LINE;
	}
	
	icon = (kx_picture *)item->data;

	slot_top = gui->y + LYT_MENU_AREA_TOP + LYT_MNI_HEIGHT * (slot-1); /* Slots are numbered from 1 */
	/* Draw background */
	fb_draw_rect(fb, gui->x + LYT_MNI_LEFT,
			slot_top,
			LYT_MNI_WIDTH,
			height,	cbg);

	/* Draw icon pad */
	fb_draw_rounded_rect(fb, gui->x + LYT_MNI_PAD_LEFT,
			slot_top + LYT_MNI_PAD_TOP,
			LYT_MNI_PAD_WIDTH, LYT_MNI_PAD_HEIGHT, cpad);

	/* Draw icon */
	if (NULL != icon) {
		fb_draw_picture(fb, gui->x + LYT_MNI_PAD_LEFT + LYT_PAD_ICON_LOFF,
				slot_top + LYT_MNI_PAD_TOP + LYT_PAD_ICON_TOFF,
				icon);
	}

	/* Calculate text size */
	fb_text_size(fb, &w, &h, DEFAULT_FONT, item->label);

	/* Draw label text. Align middle unless description exists */
	fb_draw_text(fb,
			gui->x + LYT_MNI_TEXT_LEFT,
			slot_top + (item->description ? LYT_MNI_PAD_TOP : (height - h)/2),
			ctext, DEFAULT_FONT, item->label);

	/* Draw description if available */
	if (item->description) {
		h2 = h;	/* Save label's height */
		/* Calculate text size */
		fb_text_size(fb, &w, &h, DEFAULT_FONT, item->description);

		/* Draw description right aligned */
		fb_draw_text(fb,
				gui->x + LYT_MENU_AREA_LEFT + LYT_MENU_AREA_WIDTH - w - 3,
				slot_top + LYT_MNI_PAD_TOP + h2 + 1,
				cline, DEFAULT_FONT, item->description);
	}

	/* Draw something to show that here is submenu available *
	if (NULL != item->submenu) {
	}
	*/

	/* Draw line */
	fb_draw_rect(fb, gui->x + LYT_MNI_LEFT,
			slot_top + LYT_MNI_LINE_TOP,
			LYT_MNI_LINE_WIDTH,
			LYT_MNI_LINE_HEIGHT, cline);
}


/* Display bootlist menu with selection */
void gui_show_menu(struct gui_t *gui, kx_menu *menu)
{
	int i,j;
	int slotheight = LYT_MNI_HEIGHT;
	int slots = gui->height/slotheight -1;
	kx_menu_level *ml;
	// struct boot that is in fist slot
	static int firstslot=0;
	int cur_no;

	ml = menu->current;			/* active menu level */
	cur_no = ml->current_no;	/* active menu item index */
	
	/* FIXME: shouldn't be done here */
	if (1 == ml->count) {
		/* Only system menu in list */
		draw_background(gui, "No boot devices found.\nR: Reboot  S: Rescan");
	} else {
		draw_background(gui, "KEXECBOOT");
	}

	if(cur_no < firstslot)
		firstslot = cur_no;
	if(cur_no > firstslot + slots -1)
		firstslot = cur_no - (slots -1);

	for(i=1, j=firstslot; i <= slots && j< ml->count; i++, j++) {
		draw_slot(gui, ml->list[j], i, slotheight, j == cur_no);
	}

	fb_render(gui->fb);
}


void gui_show_text(struct gui_t *gui, kx_text *text)
{
	int i, y;
	int max_x, max_y;

	draw_background(gui, "KEXECBOOT");

	/* No text to show */
	if ((!text) || (text->rows->fill <= 1)) return;

	/* Size constraints */
	max_x = gui->x + LYT_MENU_AREA_LEFT + LYT_MENU_AREA_WIDTH;
	max_y = gui->y + LYT_MENU_AREA_TOP + LYT_MENU_AREA_HEIGHT;

	for (i = text->current_line_no, y = gui->y + LYT_MENU_AREA_TOP;
		( (i < text->rows->fill) && (y < max_y) );
		 i++
	) {
		y += fb_draw_constrained_text(gui->fb, gui->x + LYT_MENU_AREA_LEFT, y,
				max_x, max_y,
				CLR_MNI_TEXT, DEFAULT_FONT,
				text->rows->list[i]);
	}
	fb_render(gui->fb);
}


/* Display custom text near logo */
void gui_show_msg(struct gui_t *gui, const char *text)
{
	draw_background(gui, text);
	fb_render(gui->fb);
}

#endif /* USE_FBMENU */
