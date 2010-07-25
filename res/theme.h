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

/* Colors */
#define COLOR_BG		0xECECE1
#define COLOR_BRDR		0xFF0000
#define COLOR_TEXT		0x000000
