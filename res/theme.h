/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2008-2011 Yuri Bushmelev <jay4mail@gmail.com>
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

#include "../config.h"

#ifdef USE_ICONS
/** Icons **/
#include "icons/logo.xpm"
#include "icons/system.xpm"

#include "icons/back.xpm"
#include "icons/reboot.xpm"
#include "icons/rescan.xpm"
#include "icons/debug.xpm"
#include "icons/shutdown.xpm"
#include "icons/exit.xpm"

#include "icons/storage.xpm"
#include "icons/mmc.xpm"
#include "icons/memory.xpm"
#endif /* USE_ICONS */

#ifdef USE_FBMENU
/** Font **/
#include "fonts/ter-u16n-ascii.h"
#define DEFAULT_FONT	(&ter_u16n_ascii_font)

/** Colors **/
#define COLOR_BG		0xECECE1
#define COLOR_BRDR		0xFF0000
#define COLOR_TEXT		0x000000

/* Colors for new layout */
#define CLR_BG			0x000000
#define CLR_BG_PAD		CLR_BG
#define CLR_BG_TEXT		0xFFFFFF

/* Menu */
#define CLR_MENU_BG		0xCCCCCC
#define CLR_MENU_FRAME	0xFFFFFF

/* Menu item */
#define CLR_MNI_BG		0xCCCCCC
#define CLR_MNI_PAD		0xCCCCCC
#define CLR_MNI_LINE	0x999999
#define CLR_MNI_TEXT	0x000000

/* Selected menu item */
#define CLR_SMNI_BG		0xFFFFFF
#define CLR_SMNI_PAD	0xCCCCCC
#define CLR_SMNI_LINE	0x999999
#define CLR_SMNI_TEXT	0x000000


/** Layout **/
#define LYT_HEIGHT			(gui->height)	/* Real GUI height & width */
#define LYT_WIDTH			(gui->width)

#define LYT_HDR_HEIGHT		60			/* Part above menu height */
#define LYT_FTR_HEIGHT		20			/* Part below menu height */

#define LYT_FRAME_SIZE		2			/* Offset of menu frame */
#define LYT_MENU_FRAME_SIZE	2			/* Menu frame thickness */

#define LYT_PAD_ICON_TOFF	1			/* Offset of icon inside pad from top */
#define LYT_PAD_ICON_LOFF	1			/* Offset of icon inside pad from left */

/* Layout: header icon pad (logo) */
#define LYT_HDR_PAD_HEIGHT	33
#define LYT_HDR_PAD_WIDTH	33
#define LYT_HDR_PAD_TOP		(LYT_HDR_HEIGHT - LYT_MENU_FRAME_SIZE - LYT_HDR_PAD_HEIGHT)/2
#define LYT_HDR_PAD_LEFT	LYT_FRAME_SIZE + LYT_MENU_FRAME_SIZE + 3

/* Layout: header text (centered) */
//#define LYT_HDR_TEXT_TOP	5
//#define LYT_HDR_TEXT_LEFT	5

/* Layout: menu frame (border around menu area) */
#define LYT_MENU_FRAME_HEIGHT	LYT_HEIGHT - LYT_HDR_HEIGHT - LYT_FTR_HEIGHT
#define LYT_MENU_FRAME_WIDTH	LYT_WIDTH - LYT_FRAME_SIZE - LYT_FRAME_SIZE
#define LYT_MENU_FRAME_TOP		LYT_HDR_HEIGHT - LYT_MENU_FRAME_SIZE
#define LYT_MENU_FRAME_LEFT		LYT_FRAME_SIZE

/* Layout: menu area */
#define LYT_MENU_AREA_HEIGHT	LYT_MENU_FRAME_HEIGHT - LYT_MENU_FRAME_SIZE - LYT_MENU_FRAME_SIZE
#define LYT_MENU_AREA_WIDTH		LYT_MENU_FRAME_WIDTH - LYT_MENU_FRAME_SIZE - LYT_MENU_FRAME_SIZE
#define LYT_MENU_AREA_TOP		LYT_MENU_FRAME_TOP + LYT_MENU_FRAME_SIZE
#define LYT_MENU_AREA_LEFT		LYT_MENU_FRAME_LEFT + LYT_MENU_FRAME_SIZE

/* Layout: menu item */
#define LYT_MNI_HEIGHT 			40			/* Menu item height */
#define LYT_MNI_WIDTH 			LYT_WIDTH - (LYT_FRAME_SIZE + LYT_MENU_FRAME_SIZE)*2
#define LYT_MNI_LEFT 			LYT_MENU_AREA_LEFT

/* Layout: menu item separator line */
#define LYT_MNI_LINE_WIDTH		LYT_MNI_WIDTH	/* Menu separator line length */
#define LYT_MNI_LINE_HEIGHT		1			/* Menu separator line height */
#define LYT_MNI_LINE_TOP		LYT_MNI_HEIGHT - LYT_MNI_LINE_HEIGHT

/* Layout: menu item icon pad */
#define LYT_MNI_PAD_HEIGHT		LYT_HDR_PAD_HEIGHT
#define LYT_MNI_PAD_WIDTH		LYT_HDR_PAD_WIDTH
#define LYT_MNI_PAD_LEFT		LYT_HDR_PAD_LEFT
#define LYT_MNI_PAD_TOP			(LYT_MNI_HEIGHT - LYT_MNI_LINE_HEIGHT - LYT_MNI_PAD_HEIGHT)/2

/* Layout: menu item text */
//#define LYT_MNI_TEXT_TOP		5			/* Menu item text top pos - middle */
#define LYT_MNI_TEXT_LEFT		LYT_MNI_PAD_LEFT + LYT_MNI_PAD_WIDTH + 3
#endif	/* USE_FBMENU */


#ifdef USE_TEXTUI
/** TEXT UI colors **/
#include "../termseq.h"

/* Background color pair */
#define TUI_CLR_BG		TERM_CSI TERM_BG_BLACK ";" TERM_FG_WHITE TERM_SGR

/* Menu background */
#define TUI_CLR_MENU	TERM_CSI TERM_BG_BLACK ";" TERM_FG_WHITE TERM_SGR

/* Menu items */
#define TUI_CLR_MNI		TERM_CSI TERM_BG_BLACK ";" TERM_FG_WHITE TERM_SGR

/* Selected menu item */
#define TUI_CLR_SMNI	TERM_CSI TERM_BG_WHITE ";" TERM_FG_BLACK TERM_SGR

/** TEXT UI layout **/
#define TUI_LYT_HEIGHT			(tui->height)
#define TUI_LYT_WIDTH			(tui->width)

#define TUI_LYT_HDR_TOP			1
#define TUI_LYT_HDR_HEIGHT		3
#define TUI_LYT_FTR_HEIGHT		1
#define TUI_LYT_FTR_TOP			TUI_LYT_HEIGHT - TUI_LYT_FTR_HEIGHT

#define TUI_LYT_MENU_TOP		TUI_LYT_HDR_HEIGHT
#define TUI_LYT_MENU_LEFT		1
#define TUI_LYT_MENU_WIDTH		TUI_LYT_WIDTH - TUI_LYT_MENU_LEFT - TUI_LYT_MENU_LEFT
#define TUI_LYT_MENU_HEIGHT		TUI_LYT_FTR_TOP - TUI_LYT_MENU_TOP

#define TUI_LYT_MNI_LEFT		TUI_LYT_MENU_LEFT
#define TUI_LYT_MNI_WIDTH		TUI_LYT_MENU_WIDTH \
										- (TUI_LYT_MNI_LEFT - TUI_LYT_MENU_LEFT)*2
#define TUI_LYT_MNI_HEIGHT		2

#define TUI_LYT_MNI_TEXT_LEFT	1
#define TUI_LYT_MNI_TEXT_WIDTH	TUI_LYT_MNI_WIDTH - TUI_LYT_MNI_TEXT_LEFT*2
#endif /* USE_TEXTUI */
