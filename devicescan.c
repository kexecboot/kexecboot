/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2009 Omegamoon <omegamoon@gmail.com>
 *  Copyright (c) 2008-2009 Yuri Bushmelev <jay4mail@gmail.com>
 *  Copyright (c) 2008 Thomas Kunze <thommycheck@gmx.de>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <asm/types.h>
#include <stdint.h>
#include <linux/input.h>

#include <dirent.h>

#include "fstype/fstype.h"
#include "util.h"
#include "devicescan.h"


/* Create charlist of known filesystems */
struct charlist *scan_filesystems()
{
	char *split;
	struct charlist *fl;
	char line[80];

	fl = create_charlist(8);
	// make a list of all known filesystems
	FILE *f = fopen("/proc/filesystems", "r");

	if (NULL == f) {
		DPRINTF("Can't open /proc/filesystems\n");
		free_charlist(fl);
		return NULL;
	}

	while (fgets(line, sizeof(line), f)) {
		line[strlen(line) - 1] = '\0';	/* kill last '\n' */
		split = strchr(line, '\t');
		split++;
		addto_charlist(fl, split);	/* NOTE: strdup() may return NULL */
	}
	fclose(f);
	return fl;
}


/* Allocate bootconf structure */
struct bootconf_t *create_bootcfg(unsigned int size)
{
	struct bootconf_t *bc;

	bc = malloc(sizeof(*bc));
	if (NULL == bc) return NULL;

	bc->list = malloc( size * sizeof(*(bc->list)) );
	if (NULL == bc->list) {
		free(bc);
		return NULL;
	}

	bc->size = size;
	bc->fill = 0;

	bc->timeout = 0;
	bc->default_item = NULL;
	bc->ui = GUI;
	bc->debug = 0;

	return bc;
}

/* Device types */
struct dtypes_t {
	enum dtype_t dtype;
	int device_len;
	char *device;
};

static struct dtypes_t dtypes[] = {
	{ DVT_HD, sizeof("/dev/hd")-1, "/dev/hd" },
	{ DVT_SD, sizeof("/dev/sd")-1, "/dev/sd" },
	{ DVT_MMC, sizeof("/dev/mmcblk")-1, "/dev/mmcblk" },
	{ DVT_MTD, sizeof("/dev/mtdblock")-1, "/dev/mtdblock" },
	{ DVT_UNKNOWN, 0, NULL }
};

/* Import values from cfgdata and boot to bootconf */
int addto_bootcfg(struct bootconf_t *bc, struct device_t *dev,
		struct cfgdata_t *cfgdata)
{
	struct boot_item_t *bi;
	struct dtypes_t *dt;

	bi = malloc(sizeof(*bi));
	if (NULL == bi) {
		DPRINTF("Can't allocate memory for new bootconf item\n");
		return -1;
	}

	bi->device = dev->device;
	bi->fstype = dev->fstype;
	bi->blocks = dev->blocks;

	bi->dtype = DVT_UNKNOWN;
	for (dt = dtypes; dt->dtype != DVT_UNKNOWN; dt++) {
		if ( !strncmp(bi->device, dt->device, dt->device_len) ) {
			bi->dtype = dt->dtype;
		}
	}

	bi->label = cfgdata->label;
	bi->kernelpath = cfgdata->kernelpath;
	bi->cmdline = cfgdata->cmdline;
	bi->iconpath = cfgdata->iconpath;
	bi->priority = cfgdata->priority;

	bc->list[bc->fill] = bi;

	if (cfgdata->is_default)	bc->default_item = bi;
	if (cfgdata->ui != bc->ui)	bc->ui = cfgdata->ui;
	if (cfgdata->timeout > 0)	bc->timeout = cfgdata->timeout;
	if (cfgdata->debug > 0)		bc->debug = cfgdata->debug;

	++bc->fill;

	/* Resize list when needed */
	if (bc->fill >= bc->size) {
		struct boot_item_t **new_list;

		bc->size <<= 1;	/* size *= 2; */
		new_list = realloc( bc->list, bc->size * sizeof(*(bc->list)) );
		if (NULL == new_list) {
			DPRINTF("Can't resize boot structure\n");
			return -1;
		}

		bc->list = new_list;
	}

	/* Return item No. */
	return bc->fill - 1;
}


/* Free bootconf structure */
void free_bootcfg(struct bootconf_t *bc)
{
	int i;
	for (i = 0; i < bc->fill; i++) {
		free(bc->list[i]->device);
		free(bc->list[i]->kernelpath);
		dispose(bc->list[i]->cmdline);
		dispose(bc->list[i]->label);
		dispose(bc->list[i]->iconpath);
		free(bc->list[i]);
	}
	free(bc->list);
	free(bc);
}


#ifdef DEBUG
void print_bootcfg(struct bootconf_t *bc)
{
	int i;

	DPRINTF("== Bootconf (%d, %d)\n", bc->size, bc->fill);
	DPRINTF(" + ui: %d\n", bc->ui);
	DPRINTF(" + timeout: %d\n", bc->timeout);
	DPRINTF(" + debug: %d\n", bc->debug);

	for (i = 0; i < bc->fill; i++) {
		DPRINTF(" [%d] device: '%s'\n", i, bc->list[i]->device);
		DPRINTF(" [%d] fstype: '%s'\n", i, bc->list[i]->fstype);
		DPRINTF(" [%d] blocks: '%lu'\n", i, bc->list[i]->blocks);
		DPRINTF(" [%d] label: '%s'\n", i, bc->list[i]->label);
		DPRINTF(" [%d] kernelpath: '%s'\n", i, bc->list[i]->kernelpath);
		DPRINTF(" [%d] cmdline: '%s'\n", i, bc->list[i]->cmdline);
		DPRINTF(" [%d] iconpath: '%s'\n", i, bc->list[i]->iconpath);
		DPRINTF(" [%d] priority: '%d'\n", i, bc->list[i]->priority);
	}
}
#endif


/* Detect FS type on device and returt pointer to static structure from fstype.c */
const char *detect_fstype(char *device, struct charlist *fl)
{
	int fd;
	const char *fstype;

	DPRINTF("Probing %s\n",device);
	fd = open(device, O_RDONLY);
	if (fd < 0) {
		DPRINTF("Can't open device %s\n", device);
		return NULL;
	}

	if ( 0 != identify_fs(fd, &fstype, NULL, 0) ) {
		close(fd);
		DPRINTF("FS could not be identified\n");
		return NULL;
	}
	close(fd);
	DPRINTF("+ FS on device is %s\n", fstype);

	/* Check that FS is known */
	if (in_charlist(fl, fstype) < 0) {
		DPRINTF("FS is not supported by kernel\n");
		return NULL;
	}

	return(fstype);
}


/* Check and parse config file */
int get_bootinfo(struct cfgdata_t *cfgdata)
{
	struct stat sinfo;

	/* Clean cfgdata structure */
	init_cfgdata(cfgdata);

	/* Parse config file */
	if (0 == parse_cfgfile(BOOTCFG_PATH, cfgdata)) {	/* Found and parsed */
		/* Check kernel presence */
		if (0 == stat(cfgdata->kernelpath, &sinfo)) return 0;

		DPRINTF("Config file points to non-existent kernel\n");
		return -1;

	} else {	/* No config file found. Check kernels. */

		/* Check machine kernel if set */
		if (NULL != machine_kernel) {
			if (0 == stat(machine_kernel, &sinfo)) {
				cfgdata->kernelpath = machine_kernel;
				DPRINTF("+ found machine kernel\n");
				return 0;
			}
		}

		/* Check default kernels */
		char **kp;
		for (kp = default_kernels; NULL != *kp; kp++) {
			if (0 == stat(*kp, &sinfo)) {
				cfgdata->kernelpath = strdup(*kp);
				DPRINTF("+ found default kernel '%s'\n", *kp);
				return 0;
			}
		}
	}

	/* We have no kernels */
	DPRINTF("No kernels found\n");
	return -1;
}

FILE *devscan_open(struct charlist **fslist)
{
	FILE *f;
	struct charlist *fl;
	char line[80];

    /* Get a list of all filesystems registered in the kernel */
	fl = scan_filesystems();
	if (NULL == fl) {
		DPRINTF("Can't read filesystems\n");
		return NULL;
	}

	/* Get a list of available partitions on all devices
	 * See kernel/block/genhd.c for details on interface */
	f = fopen("/proc/partitions", "r");
	if (NULL == f) {
		DPRINTF("Unable to get list of available partitions\n");
		goto free_fl;
	}

	// First two lines are bogus.
	fgets(line, sizeof(line), f);
	fgets(line, sizeof(line), f);

	*fslist = fl;
	return f;

free_fl:
	free_charlist(fl);

	return NULL;
}

int devscan_next(FILE *fp, struct charlist *fslist, struct device_t *dev)
{
	int major, minor, blocks, len;
	char *tmp, *p;
	char *device;
	char line[80];

	if (NULL == fgets(line, sizeof(line), fp)) {
		DPRINTF("Can't read next device line\n");
		return 0;
	}

	/* Get major, minor, blocks and device name */
	major = get_nni(line, &p);
	minor = get_nni(p, &p);
	blocks = get_nni(p, &p);
	tmp = get_word(p, &p);

	if (major < 0 || minor < 0 || blocks < 0 || NULL == tmp) {
		DPRINTF("Can't parse string: '%s'\n", line);
		return -1;
	}

	if (blocks < 200) {
		DPRINTF("Skipping little partition with size (%dk) < 200k\n", blocks);
		return -1;
	}

	/* Format device name */
	len = p - tmp;
	device = malloc(len + 5 + 1); /* 5 = strlen("/dev/") */
	if (NULL == device) {
		DPRINTF("Can't allocate memory for device name\n");
		return -1;
	}
	strcpy(device, "/dev/");
	strncat(device, tmp, len);

	DPRINTF("Got device '%s' (%d, %d) of size %dMb\n", device, major, minor, blocks>>10);

#ifdef USE_DEVICES_RECREATING
	/* Remove old device node. We don't care about unlink() result. */
	unlink(device);

	/* Re-create device node */
	DPRINTF("Recreating device node %s (%d, %d)\n", device, major, minor);
	if ( -1 == mknod( device,
			(S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH),
			makedev(major, minor) ) )
	{
		perror("Can't create device node");
	}
#endif

	dev->fstype = detect_fstype(device, fslist);
	if (NULL == dev->fstype) {
		DPRINTF("Can't detect FS type\n");
		free(device);
		return -1;
	}

	dev->device = device;
	dev->blocks = blocks;

	return 1;
}


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
		DPRINTF("Can't open evdev device '%s'", path);
/*		perror("");*/
		return 0;
	}

	/* Clean evtype_bitmask structure */
	memset(evtype_bitmask, 0, sizeof(evtype_bitmask));

	/* Ask device features */
	if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evtype_bitmask) < 0) {
		DPRINTF("Can't get evdev features");
/*		perror("");*/
		return 0;
	}

	close(fd);

#ifdef DEBUG
	int yalv;
	DPRINTF("Supported event types:\n");
	for (yalv = 0; yalv < EV_MAX; yalv++) {
		if (test_bit(yalv, evtype_bitmask)) {
			/* this means that the bit is set in the event types list */
			DPRINTF("  Event type 0x%02x ", yalv);
			switch (yalv) {
			case EV_KEY:
				DPRINTF(" (Keys or Buttons)\n");
				break;
			case EV_REL:
				DPRINTF(" (Relative Axes)\n");
				break;
			case EV_ABS:
				DPRINTF(" (Absolute Axes)\n");
				break;
			case EV_MSC:
				DPRINTF(" (Something miscellaneous)\n");
				break;
			case EV_SW:
				DPRINTF(" (Switch)\n");
				break;
			case EV_LED:
				DPRINTF(" (LEDs)\n");
				break;
			case EV_SND:
				DPRINTF(" (Sounds)\n");
				break;
			case EV_REP:
				DPRINTF(" (Repeat)\n");
				break;
			case EV_FF:
				DPRINTF(" (Force Feedback)\n");
				break;
			case EV_PWR:
				DPRINTF(" (Power)\n");
				break;
			case EV_FF_STATUS:
				DPRINTF(" (Force Feedback Status)\n");
				break;
			default:
				DPRINTF(" (Unknown event type: 0x%04hx)\n", yalv);
				break;
			}
		}
	}
#endif

	/* Check that we have EV_KEY bit set */
	if (test_bit(EV_KEY, evtype_bitmask)) return 1;

	/* device is not suitable */
	DPRINTF("Event device %s have no EV_KEY bit\n", path);
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
		DPRINTF("Can't open directory '%s'\n", path);
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
			DPRINTF("+ found evdev: %s\n", dp->d_name);
			strcat(p, dp->d_name);
			if (is_suitable_evdev(device)) {
				addto_charlist(evlist, device);
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

	DPRINTF("We have not found any event device\n");
	return NULL;
}


/* Open event devices and return array of descriptors */
int *open_event_devices(struct charlist *evlist)
{
	int i, *ev_fds;

	ev_fds = malloc(evlist->fill * sizeof(*ev_fds));
	if (NULL == ev_fds) {
		DPRINTF("Can't allocate memory for descriptors array\n");
		return NULL;
	}

	for(i = 0; i < evlist->fill; i++) {
		ev_fds[i] = open(evlist->list[i], O_RDONLY);
		if (-1 == ev_fds[i]) {
			DPRINTF("Can't open event device '%s'", evlist->list[i]);
/*			perror("");*/
		}
		DPRINTF("+ opened event device '%s', fd: %d\n", evlist->list[i], ev_fds[i]);
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
		DPRINTF("Can't get event devices list\n");
		return -1;
	}

	/* Open event devices */
	ev_fds = open_event_devices(evlist);
	if (NULL == ev_fds) {
		DPRINTF("Can't open event devices\n");
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

	for(i = 0; i < size; i++) {
		if (ev_fds > 0) close(ev_fds[i]);
	}

	free(ev_fds);
}
