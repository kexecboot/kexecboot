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
	A_PARENTMENU,
	A_SUBMENU,
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

typedef enum {
	KX_IT_EVDEV,
	KX_IT_TTY,
	KX_IT_SOCKET
} kx_input_type;

typedef struct {
	unsigned int size;
	unsigned int count;
	int *fds;
	kx_input_type *fdtypes;
	fd_set fdset;
	int maxfd;
} kx_inputs;


/* Initialize inputs structure */
int inputs_init(kx_inputs *inputs, unsigned int size);

/* Cleanup inputs structure */
void inputs_clean(kx_inputs *inputs);

/* Add input */
int inputs_add_fd(kx_inputs *inputs, int fd, kx_input_type type);

/* Scan for possible inputs and open them */
int inputs_open(kx_inputs *inputs);

/* Close opened inputs */
void inputs_close(kx_inputs *inputs);

/* Prepare inputs for processing */
int inputs_preprocess(kx_inputs *inputs);

/* Read and process events */
enum actions_t inputs_process(kx_inputs *inputs);


#endif //_HAVE_EVDEVS_H_
