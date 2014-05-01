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

#include "config.h"

#ifdef USE_TEXTUI
/** TEXT UI colors **/
#include "termseq.h"

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
