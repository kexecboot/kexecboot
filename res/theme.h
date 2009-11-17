/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2008-2009 Yuri Bushmelev <jay4mail@gmail.com>
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

/* Icons */
#include "logo.xpm"
#include "system.xpm"

#include "back.xpm"
#include "reboot.xpm"
#include "rescan.xpm"
#include "debug.xpm"

#include "hd.xpm"
#include "sd.xpm"
#include "mmc.xpm"
#include "memory.xpm"


/* Font */
#include "radeon-font.h"
#define DEFAULT_FONT	(&radeon_font)

#define MYRGB(r,g,b)	(r), (g), (b)
/* Colors */
#define COLOR_BG		MYRGB(0xEC, 0xEC, 0xE1)
#define COLOR_BRDR		MYRGB(0xFF, 0, 0)
#define COLOR_TEXT		MYRGB(0, 0, 0)
