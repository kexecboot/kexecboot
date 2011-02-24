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

/*
 * *** TODO ***
 * 1. is_suitable_evdev open and close device. But later we are opening
 * devices again. Idea is open device, do all checks, if it is suitable,
 * pass descriptor to upper layer, close otherwise.
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

#include "config.h"
#include "evdevs.h"
#include "util.h"

/* this macro is used to tell if "bit" is set in "array"
 * it selects a byte from the array, and does a boolean AND
 * operation with a byte that only has the relevant bit set.
 * eg. to check for the 12th bit, we do (array[1] & 1<<4)
 */
#define test_bit(bit, array)    (array[bit>>3] & (1<<(bit%8)))

int is_suitable_evdev(char *path)
{
	int fd;
	uint8_t evtype_bitmask[(EV_MAX>>3) + 1];

	if ((fd = open(path, O_RDONLY)) < 0) {
		log_msg(lg, "+ can't open evdev '%s': %s", path, ERRMSG);
		return 0;
	}

	/* Clean evtype_bitmask structure */
	memset(evtype_bitmask, 0, sizeof(evtype_bitmask));

	/* Ask device features */
	if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evtype_bitmask) < 0) {
		log_msg(lg, "+ can't get evdev features: %s", ERRMSG);
		return 0;
	}

	close(fd);

#ifdef DEBUG
	int yalv;
	log_msg(lg, "+ supported event types:");
	for (yalv = 0; yalv < EV_MAX; yalv++) {
		if (test_bit(yalv, evtype_bitmask)) {
			/* this means that the bit is set in the event types list */
			switch (yalv) {
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
	log_msg(lg, "+ evdev %s have no EV_KEY bit, skipped", path);
	return 0;
}


/* Search all event devices in specified directory */
struct charlist *scan_event_dir(char *path)
{
	int len;
	DIR *d;
	struct dirent *dp;
	struct charlist *evlist;
	char *p;
	const char *pattern = "event";
	char device[strlen(path) + 1 + strlen(pattern) + 3];

	d = opendir(path);
	if (NULL == d) {
		log_msg(lg, "+ can't open directory '%s': %s", path, ERRMSG);
		return NULL;
	}

	/* Prepare placeholder */
	strcpy(device, path);
	p = device + strlen(device) + 1;
	*(p - 1) = '/';
	*p = '\0';

	len = strlen(pattern);
	evlist = create_charlist(4);

	/* Loop through directory and look for pattern */
	while ((dp = readdir(d)) != NULL) {
		if (0 == strncmp(dp->d_name, pattern, len)) {
			strcat(p, dp->d_name);
			if (is_suitable_evdev(device)) {
				addto_charlist(evlist, device);
				log_msg(lg, "+ Added evdev '%s'", dp->d_name);
			}
			*p = '\0';	/* Reset device name */
		}
	}
	closedir(d);

	return evlist;
}


/* Return list of found event devices */
struct charlist *scan_event_devices()
{
	struct charlist *evlist;

	/* Check /dev for event devices */
	evlist = scan_event_dir("/dev");
	if (NULL == evlist) return NULL;
	if (0 != evlist->fill) return evlist;

	/* We have not found any device. Search in /dev/input */
	free_charlist(evlist);

	evlist = scan_event_dir("/dev/input");
	if (NULL == evlist) return NULL;
	if (0 != evlist->fill) return evlist;

	log_msg(lg, "No evdevs found");
	return NULL;
}


/* Open event devices and return array of descriptors */
int *open_event_devices(struct charlist *evlist)
{
	int i, *ev_fds;

	ev_fds = malloc(evlist->fill * sizeof(*ev_fds));
	if (NULL == ev_fds) {
		DPRINTF("Can't allocate memory for descriptors array");
		return NULL;
	}

	for(i = 0; i < evlist->fill; i++) {
		ev_fds[i] = open(evlist->list[i], O_RDONLY);
		if (-1 == ev_fds[i]) {
			log_msg(lg, "+ can't open evdev '%s': %s", evlist->list[i], ERRMSG);
		}
		log_msg(lg, "+ evdev '%s' is opened (%d)", evlist->list[i], ev_fds[i]);
	}

	return ev_fds;
}


/* Scan for event devices */
int scan_evdevs(struct ev_params_t *ev)
{
	struct charlist *evlist;
	int *ev_fds;
	fd_set fds0;
	int i, maxfd, nev, efd;

	int rep[2];		/* Repeat rate array (milliseconds) */
	rep[0] = 1000;	/* Delay before first repeat */
	rep[1] = 250;	/* Repeating delay */

	ev->count = 0;

	/* Search for keyboard/touchscreen/mouse/joystick/etc.. */
	evlist = scan_event_devices();
	if (NULL == evlist) {
		return -1;
	}

	/* Open event devices */
	ev_fds = open_event_devices(evlist);
	if (NULL == ev_fds) {
		exit(-1);
	}

	nev = evlist->fill;
	free_charlist(evlist);	/* Move this part to scan_event_devices ? */

	maxfd = -1;
	/* Fill FS set with descriptors and search maximum fd number */
	FD_ZERO(&fds0);
	for (i = 0; i < nev; i++) {
		efd = ev_fds[i];
		if (efd > 0) {
			FD_SET(efd, &fds0);
			if (efd > maxfd) maxfd = efd;	/* Find maximum fd no */
			/* Set repeat rate on device */
			ioctl(efd, EVIOCSREP, rep);	/* We don't care about result */
			/* Grab device exclusively */
			ioctl(efd, EVIOCGRAB, (void *)1);	/* We don't care about result */
		}
	}
	++maxfd;	/* Increase maxfd according to select() manual */

	ev->count = nev;
	ev->fd = ev_fds;
	ev->fds = fds0;
	ev->maxfd = maxfd;
	return 0;
}


/* Close opened devices */
void close_event_devices(int *ev_fds, int size)
{
	int i;

	if (size <= 0) return;

	for(i = 0; i < size; i++) {
		if (ev_fds > 0) close(ev_fds[i]);
	}

	free(ev_fds);
}


/* Read and process events */
enum actions_t process_events(struct ev_params_t *ev)
{
	fd_set fds;
	int i, e, nready, efd;
	const int evt_size = 4;
	struct input_event evt[evt_size];
	enum actions_t action = A_NONE;
	struct timeval timeout;

	timeout.tv_usec = 0;
#ifdef USE_TIMEOUT
	timeout.tv_sec = USE_TIMEOUT;
#else
	timeout.tv_sec = 60;	// exit after timeout to allow to do something above
#endif

	if (0 == ev->count) return A_ERROR;		/* A_EXIT ? */

	fds = ev->fds;

	/* Wait for some input */
	nready = select(ev->maxfd, &fds, NULL, NULL, &timeout);	/* Wait for input or timeout */

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
	for (i = 0; i < ev->count; i++) {
		efd = ev->fd[i];
		if (FD_ISSET(efd, &fds)) {
			nready = read(efd, evt, sizeof(struct input_event) * evt_size);	/* Read events */
			if ( nready < (int) sizeof(struct input_event) ) {
				log_msg(lg, "Short read of event structure (%d bytes)", nready);
				continue;
			}

			/* NOTE: debug
			if ( nready > (int) sizeof(struct input_event) )
				DPRINTF("Have more than one event here (%d bytes, %d events)",
						nready, (int) (nready / sizeof(struct input_event)) );
			*/

			for (e = 0; e < (int) (nready / sizeof(struct input_event)); e++) {

				/* DPRINTF("+ event on %d, type: %d, code: %d, value: %d",
						efd, evt[e].type, evt[e].code, evt[e].value); */

				if ((EV_KEY == evt[e].type) && (0 != evt[e].value)) {
					/* EV_KEY event actions */

					switch (evt[e].code) {
					case KEY_UP:
						action = A_UP;
						break;
					case KEY_DOWN:
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
					default:
						action = A_NONE;
						break;
					}
#if 0
				} else if ((EV_ABS == evt.type) && (0 != evt.value)) {
					/* EV_KEY event actions */
					suitable_event = 1;
					switch (evt.code) {
					case ABS_PRESSURE:	/* Touchscreen touch */
						if (choice < (bl->fill - 1)) choice++;
						else choice = 0;
						break;
					default:
						suitable_event = 0;
					}
#endif
				}
			}
		}
	}

	return action;
}
