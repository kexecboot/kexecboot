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
		addto_charlist(fl, strdup(split));	/* NOTE: strdup() may return NULL */
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


/* Import values from cfgdata and boot to bootconf */
int addto_bootcfg(struct bootconf_t *bc, struct device_t *dev,
		struct cfgfile_t *cfgdata)
{
	struct boot_item_t *bi;

	bi = malloc(sizeof(*bi));
	if (NULL == bi) {
		DPRINTF("Can't allocate memory for new bootconf item\n");
		return -1;
	}

	bi->device = dev->device;
	bi->fstype = dev->fstype;
	bi->blocks = dev->blocks;

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

	return 0;
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
		DPRINTF("FS cound not be identified\n");
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


/* Parse device line and return device parameters */
int parse_dline(char *line, struct charlist *fl, struct device_t *dev)
{
	int major, minor, blocks, len;
	char *tmp, *p;
	char *device;

	/* Get major, minor, blocks and device name */
	major = get_nni(line, &p);
	minor = get_nni(p, &p);
	blocks = get_nni(p, &p);
	tmp = get_word(p, &p);

	if (major < 0 || minor < 0 || blocks < 0 || NULL == tmp) {
		DPRINTF("Can't parse string: '%s'\n", line);
		goto free_nothing;
	}

	if (blocks < 200) {
		DPRINTF("Skipping little partition with size (%dk) < 200k\n", blocks);
		goto free_nothing;
	}

	/* Format device name */
	len = p - tmp;
	device = malloc(len + 5 + 1); /* 5 = strlen("/dev/") */
	if (NULL == device) {
		DPRINTF("Can't allocate memory for device name\n");
		goto free_nothing;
	}
	strcpy(device, "/dev/");
	strncat(device, tmp, len);

	DPRINTF("Got device '%s' (%d, %d) of size %dMb\n", device, major, minor, blocks>>10);

#ifndef KX_NO_DEVICES_CREATION
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

	dev->fstype = detect_fstype(device, fl);
	if (NULL == dev->fstype) {
		DPRINTF("Can't detect FS type\n");
		goto free_device;
	}

	dev->device = device;
	dev->blocks = blocks;

	return 0;

free_device:
	free(device);

free_nothing:
	return -1;
}


/* Check and parse config file */
int get_bootinfo(struct cfgfile_t *cfgdata)
{
	struct stat sinfo;

	/* Clean cfgdata structure */
	init_cfgfile(cfgdata);

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
		for (kp = default_kernels; NULL != kp; kp++) {
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


/*  */
struct bootconf_t *scan_devices()
{
	FILE *f;
	int rc;
	struct charlist *fl;
	struct bootconf_t *bootconf;
	struct device_t dev;
	struct cfgfile_t cfgdata;
	char line[80];

    /* Get a list of all filesystems registered in the kernel */
	fl = scan_filesystems();
	if (NULL == fl) {
		DPRINTF("Can't read filesystems\n");
		goto free_nothing;
	}

	bootconf = create_bootcfg(4);
	if (NULL == bootconf) {
		DPRINTF("Can't allocate bootconf structure\n");
		goto free_fl;
	}

	/* Get a list of available partitions on all devices
	 * See kernel/block/genhd.c for details on interface */
	f = fopen("/proc/partitions", "r");
	if (NULL == f) {
		DPRINTF("Unable to get list of available partitions\n");
		goto free_bootconf;
	}

	// First two lines are bogus.
	fgets(line, sizeof(line), f);
	fgets(line, sizeof(line), f);

	while (fgets(line, sizeof(line), f)) {
		/* Parse string and get device parameters */
		if (-1 == parse_dline(line, fl, &dev)) {
			continue;
		}

		/* Mount device */
		if (-1 == mount(dev.device, MOUNTPOINT, dev.fstype, MS_RDONLY, NULL)) {
			perror("mount");
			goto free_device;
		}

		/* NOTE: Don't go out before umount'ing */

		/* Search boot method and return boot info */
		rc = get_bootinfo(&cfgdata);

		/* Umount device */
		if (-1 == umount(MOUNTPOINT)) {
			perror("umount");
		}

		if (-1 == rc) {	/* Error */
			DPRINTF("Can't get device boot information\n");
			goto free_device;
		}

		/* Now we have something in cfgdata */
		if (-1 == addto_bootcfg(bootconf, &dev, &cfgdata)) {
			DPRINTF("Can't import config file data\n");
			goto free_device;
		}

		continue;

free_device:
		free(dev.device);
	}

	fclose(f);
	free_charlist(fl);

	return bootconf;

free_bootconf:
	free_bootcfg(bootconf);

free_fl:
	free_charlist(fl);

free_nothing:
	return NULL;
}
