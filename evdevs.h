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
#ifndef _HAVE_EVDEVS_H_
#define _HAVE_EVDEVS_H_

#include "util.h"
#include "config.h"

/* Menu/keyboard/TS actions */
enum actions_t {
	A_ERROR = -1,
	A_EXIT = 0,
	A_NONE,
	A_UP,
	A_DOWN,
	A_MAINMENU,
	A_SYSMENU,
	A_REBOOT,
	A_SHUTDOWN,
	A_RESCAN,
	A_DEBUG,
	A_SELECT,
#ifdef USE_TIMEOUT
	A_TIMEOUT,
#endif
	A_DEVICES
};

/* Event devices parameters */
struct ev_params_t {
	int count;		/* Count of event descriptors */
	int *fd;		/* Array of event devices file descriptors */
	fd_set fds;		/* FD set of file descriptors for select() */
	int maxfd;		/* Maximum fd number for select() */
};

/* Return list of found event devices */
struct charlist *scan_event_devices();

/* Open event devices and return array of descriptors */
int *open_event_devices(struct charlist *evlist);

/* Close opened devices */
void close_event_devices(int *ev_fds, int size);

/* Scan for event devices */
int scan_evdevs(struct ev_params_t *ev);

/* Read and process events */
enum actions_t process_events(struct ev_params_t *ev);

#endif //_HAVE_EVDEVS_H_
