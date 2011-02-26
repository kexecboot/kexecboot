/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2011 Yuri Bushmelev <jay4mail@gmail.com>
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

#ifndef _HAVE_TERMSEQ_H_
#define _HAVE_TERMSEQ_H_

/* Terminal control sequences according to console_codes(4) */

/* Escape */
#define TERM_ESC		"\033"
/* Control Sequence Introducer */
#define TERM_CSI		TERM_ESC "["

/* Erase Display */
#define TERM_CSI_ED		TERM_CSI "2J"
/* Erase Line */
#define TERM_CSI_EEL	TERM_CSI "K"
#define TERM_CSI_EL		TERM_CSI "2K"

/* Move CUrsor to Position (row,col) (origin: 1,1) */
#define TERM_CUP		"H"

/* Set Graphics Rendition */
#define TERM_SGR		"m"

/* Colors */
#define TERM_FG_BLACK	"30"
#define TERM_FG_RED		"31"
#define TERM_FG_GREEN	"32"
#define TERM_FG_BROWN	"33"
#define TERM_FG_BLUE	"34"
#define TERM_FG_MAGENTA	"35"
#define TERM_FG_CYAN	"36"
#define TERM_FG_WHITE	"37"

#define TERM_BG_BLACK	"40"
#define TERM_BG_RED		"41"
#define TERM_BG_GREEN	"42"
#define TERM_BG_BROWN	"43"
#define TERM_BG_BLUE	"44"
#define TERM_BG_MAGENTA	"45"
#define TERM_BG_CYAN	"46"
#define TERM_BG_WHITE	"47"

/* Reverse video */
#define TERM_RV			"7"
#define TERM_NON_RV		"27"

#endif /* _HAVE_TERMSEQ_H_*/
