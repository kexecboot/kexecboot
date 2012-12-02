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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <asm/types.h>
#include <stdint.h>
#include <linux/input.h>
#include <limits.h>

#include "config.h"
#include "evdevs.h"
#include "util.h"

#define BITS_PER_LONG (sizeof(long) * CHAR_BIT)

/* this macro is used to tell if "bit" is set in "array"
 * it selects a long from the array, and does a boolean AND
 * operation with a long that only has the relevant bit set.
 * eg. to check for the 12th bit, we do (array[0] & 1<<12)
 */
#define test_bit(bit, array)    (array[bit/BITS_PER_LONG] & (1UL<<(bit%BITS_PER_LONG)))

int evdev_is_suitable(int fd)
{
	long evtype_bitmask[(EV_MAX/BITS_PER_LONG) + 1];

	/* Clean evtype_bitmask structure */
	memset(evtype_bitmask, 0, sizeof(evtype_bitmask));

	/* Ask device features */
	if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evtype_bitmask) < 0) {
		log_msg(lg, "+ can't get evdev features: %s", ERRMSG);
		return 0;
	}

#ifdef DEBUG
	int yalv;
	for (yalv = 0; yalv < EV_MAX; yalv++) {
		if (test_bit(yalv, evtype_bitmask)) {
			/* this means that the bit is set in the event types list */
			switch (yalv) {
			case EV_SYN:
				log_msg(lg, " + Sync");
				break;
			case EV_KEY:
				log_msg(lg, " + Keys or Buttons");
				break;
			case EV_REL:
				log_msg(lg, " + Relative Axes");
				break;
			case EV_ABS:
				log_msg(lg, " + Absolute Axes");
				break;
			case EV_MSC:
				log_msg(lg, " + Something miscellaneous");
				break;
			case EV_SW:
				log_msg(lg, " + Switch");
				break;
			case EV_LED:
				log_msg(lg, " + LEDs");
				break;
			case EV_SND:
				log_msg(lg, " + Sounds");
				break;
			case EV_REP:
				log_msg(lg, " + Repeat");
				break;
			case EV_FF:
				log_msg(lg, " + Force Feedback");
				break;
			case EV_PWR:
				log_msg(lg, " + Power");
				break;
			case EV_FF_STATUS:
				log_msg(lg, " + Force Feedback Status");
				break;
			default:
				log_msg(lg, " + Unknown event type: 0x%04hx", yalv);
				break;
			}
		}
	}
#endif

	/* Check that we have EV_KEY bit set */
	if (test_bit(EV_KEY, evtype_bitmask)) return 1;

	/* device is not suitable */
	log_msg(lg, "+ evdev have no EV_KEY bit, skipped");
	return 0;
}

/* Prepare event device */
void evdev_prepare_fd(int fd)
{
#ifdef USE_EVDEV_RATE
	/* Repeat rate array (milliseconds) */
	int rep[2] = { USE_EVDEV_RATE };
	/* Set repeat rate on device */
	ioctl(fd, EVIOCSREP, rep);	/* We don't care about result */
#endif
}

/* Initialize inputs structure */
int inputs_init(kx_inputs *inputs, unsigned int size)
{
	inputs->size = size;
	inputs->count = 0;
	FD_ZERO(&(inputs->fdset));
	inputs->maxfd = -1;

	inputs->fdtypes = malloc(size * sizeof(*(inputs->fdtypes)));
	inputs->fds = malloc(size * sizeof(*(inputs->fds)));

	if ( (NULL == inputs->fdtypes) || (NULL == inputs->fds) ) {
		DPRINTF("Can't allocate memory for fd array");
		return -1;
	}

	return 0;
}

/* Cleanup inputs structure */
void inputs_clean(kx_inputs *inputs)
{
	dispose(inputs->fdtypes);
	dispose(inputs->fds);
	inputs->size = 0;
}

/* Add input */
int inputs_add_fd(kx_inputs *inputs, int fd, kx_input_type type)
{
	/* Resize arrays when needed before adding item */
	if (inputs->count >= inputs->size) {
		kx_input_type *new_fdtypes;
		int *new_fds;
		unsigned int new_size;

		new_size = inputs->size * 2;
		new_fdtypes = realloc(inputs->fdtypes, new_size * sizeof(*(inputs->fdtypes)));
		new_fds = realloc(inputs->fds, new_size * sizeof(*(inputs->fds)));
		if ( (NULL == new_fdtypes) || (NULL == new_fds) ) {
			DPRINTF("Can't resize fd's array");
			return -1;
		}

		inputs->size = new_size;
		inputs->fdtypes = new_fdtypes;
		inputs->fds = new_fds;
	}

	inputs->fdtypes[inputs->count] = type;
	inputs->fds[inputs->count] = fd;
	++inputs->count;

	if (fd > inputs->maxfd) inputs->maxfd = fd;
	FD_SET(fd, &(inputs->fdset));

	return inputs->count - 1;
}

/* Scan dir for evdev's and add them */
int inputs_open_evdir(kx_inputs *inputs, char *path)
{
	int fd, len;
	DIR *d;
	struct dirent *dp;
	char *p;
	const char *pattern = "event";
	char device[strlen(path) + 1 + strlen(pattern) + 4];

	d = opendir(path);
	if (NULL == d) {
		log_msg(lg, "+ can't open directory '%s': %s", path, ERRMSG);
		return -1;
	}

	/* Prepare placeholder */
	strcpy(device, path);
	p = device + strlen(device) + 1;
	*(p - 1) = '/';
	*p = '\0';

	len = strlen(pattern);

	/* Loop through directory and look for pattern */
	while ((dp = readdir(d)) != NULL) {
		if (0 == strncmp(dp->d_name, pattern, len)) {
			strcat(p, dp->d_name);	/* Prepare full path to device */

			log_msg(lg, "+ Trying evdev '%s'", dp->d_name);
			if ((fd = open(device, O_RDONLY)) < 0) {
				log_msg(lg, "+ can't open evdev '%s': %s", path, ERRMSG);
			} else {
				/* Check that device have right capabilities */
				if (evdev_is_suitable(fd)) {
					evdev_prepare_fd(fd);
					inputs_add_fd(inputs, fd, KX_IT_EVDEV);
					log_msg(lg, "+ Added evdev '%s'", dp->d_name);
				} else {
					close(fd);
				}
			}

			*p = '\0';	/* Reset device name from path */
		}
	}
	closedir(d);

	return 0;
}

/* Scan and open all possible inputs */
int inputs_open(kx_inputs *inputs)
{
	/* Check /dev and /dev/input for event devices */
	if (-1 == inputs_open_evdir(inputs, "/dev/input")) {
		if (-1 == inputs_open_evdir(inputs, "/dev")) {
			log_msg(lg, "No evdevs found");
			return -1;
		}
	}

	/* Here we may open other file descriptors as well (sockets e.g.) */

	return 0;
}

/* Close opened inputs */
void inputs_close(kx_inputs *inputs)
{
	int i;

	for (i=0; i < inputs->count; i++) {
		close(inputs->fds[i]);
	}
	inputs->count = 0;
}

/* Prepare inputs for processing */
int inputs_preprocess(kx_inputs *inputs)
{
	++inputs->maxfd;
	return 0;
}

int inputs_process_evdev(int fd)
{
	int nready;
	enum actions_t action = A_NONE;
	struct input_event evt;

	/* Read one event */
	nready = read(fd, &evt, sizeof(evt));
	if ( nready < (int) sizeof(evt) ) {
		log_msg(lg, "Short read of event structure (%d bytes)", nready);
		return A_ERROR;
	}
#ifdef DEBUG
	log_msg(lg, "+ Read event type %x, code %d (0x%x) value %x",
			evt.type, evt.code, evt.code, evt.value);
#endif

	/* EV_KEY event actions */
	if ((EV_KEY == evt.type) && (0 != evt.value)) {
		switch (evt.code) {
		case KEY_UP:
		case KEY_VOLUMEUP:
			action = A_UP;
			break;
		case KEY_DOWN:
		case KEY_VOLUMEDOWN:
		case BTN_TOUCH:	/* GTA02: touchscreen touch (330) */
			action = A_DOWN;
			break;
#ifndef USE_HOST_DEBUG
		case KEY_R:
			action = A_REBOOT;
			break;
#endif
		case KEY_S:	/* reScan */
			action = A_RESCAN;
			break;
		case KEY_Q:	/* Quit (when not in initmode) */
			action = A_EXIT;
			break;
		case KEY_ENTER:
		case KEY_SPACE:
		case KEY_HIRAGANA:	/* Zaurus SL-6000 */
		case KEY_HENKAN:	/* Zaurus SL-6000 */
		case 87:			/* Zaurus: OK (remove?) */
		case 63:			/* Zaurus: Enter (remove?) */
		case KEY_POWER:		/* GTA02: Power (116) */
		case KEY_PHONE:		/* GTA02: AUX (169) */
			action = A_SELECT;
			break;
#ifdef USE_NUMKEYS
		/* Return keys 0-9 */
		case KEY_0: action = A_KEY0; break;
		case KEY_1: action = A_KEY1; break;
		case KEY_2: action = A_KEY2; break;
		case KEY_3: action = A_KEY3; break;
		case KEY_4: action = A_KEY4; break;
		case KEY_5: action = A_KEY5; break;
		case KEY_6: action = A_KEY6; break;
		case KEY_7: action = A_KEY7; break;
		case KEY_8: action = A_KEY8; break;
		case KEY_9: action = A_KEY9; break;
#endif
		default:
			action = A_NONE;
			break;
		}
	}

	return action;
}


/* Read and process events */
enum actions_t inputs_process(kx_inputs *inputs)
{
	fd_set fds;
	int i, fd, nready;
	enum actions_t action = A_NONE;
	struct timeval timeout;

	timeout.tv_usec = 0;
#ifdef USE_TIMEOUT
	timeout.tv_sec = USE_TIMEOUT;
#else
	timeout.tv_sec = 60;	// exit after timeout to allow to do something above
#endif

	if (0 == inputs->count) return A_ERROR;		/* A_EXIT ? */

	fds = inputs->fdset;

	/* Wait for some input */
	nready = select(inputs->maxfd, &fds, NULL, NULL, &timeout);	/* Wait for input or timeout */

	if (-1 == nready) {
		if (errno == EINTR) return A_NONE;
		else {
			log_msg(lg, "Error occured in select() call", ERRMSG);
			return A_ERROR;
		}
	} else if (0 == nready) {	// timeout reached
#ifdef USE_TIMEOUT
		log_msg(lg, "Timeout reached!");
		return A_TIMEOUT;
#else
		return A_NONE;
#endif
	}

	/* Check fds */
	for (i = 0; i < inputs->count; i++) {
		fd = inputs->fds[i];
		if (FD_ISSET(fd, &fds)) {
			switch (inputs->fdtypes[i]) {
			case KX_IT_EVDEV:
				/* Process input from event device */
				action = inputs_process_evdev(fd);
				if (A_ERROR == action) continue; /* continue on short read */
				break;
			case KX_IT_TTY:
				/* Process input from tty */
				break;
			case KX_IT_SOCKET:
				/* Process input from sockets */
				break;
			}
		}
	}

	return action;
}
