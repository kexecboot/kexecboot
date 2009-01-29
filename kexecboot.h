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

#ifndef _HAVE_TZBL_H
#define _HAVE_TZBL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <sys/reboot.h>
#include "fb.h"
#include "devicescan.h"
#include "res/logo-img.h"
#include "res/cf-img.h"
#include "res/mmc-img.h"
#include "res/logo-img.h"
#include "res/memory-img.h"
#include "res/radeon-font.h"

/* Macro for dealing with NULL strings */
#define strlenn(s)	( (NULL != s) ? (strlen(s)) : 0 )

/* Debug macro */
/* #define DEBUG */
#ifdef DEBUG
#define DPRINTF(fmt, args...)	do { \
		fprintf(stderr, fmt, ##args); \
	} while (0)
#else
#define DPRINTF(fmt, args...)	do { } while (0)
#endif


/* Tags we want from /proc/cmdline */
char *wanted_tags[] = {
	"mtdparts",
	NULL
};

struct model_angle {
	char *model;
	int angle;
};

#endif
