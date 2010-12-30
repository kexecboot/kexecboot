/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2008-2009 Yuri Bushmelev <jay4mail@gmail.com>
 *  Copyright (c) 2008 Thomas Kunze <thommycheck@gmx.de>
 *  Copyright (c) 2006 Matthew Allum <mallum@o-hand.com>
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

#ifndef _HAVE_KEXECBOOT_H
#define _HAVE_KEXECBOOT_H

#include "config.h"

/* Default event interface. Can be redefined */
#ifndef KXB_EVENTIF
#define KXB_EVENTIF "/dev/event0"
#endif

/* Default FB angle. Can be redefined */
#ifndef KXB_FBANGLE
#define KXB_FBANGLE 0
#endif

#endif	/* _HAVE_KEXECBOOT_H */
