/*
 *  kexecboot - A kexec based bootloader
 *
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
#include "devicescan.h"

struct fslist {
	char **list;
	unsigned int size;
};

// lame search
int contains(const char *what, struct fslist *where)
{
	int i;
	for (i = 0; i < where->size; i++)
		if (!strcmp(what, where->list[i]))
			return i;
	return -1;
}

struct fslist *scan_filesystems()
{
	char line[80];
	char *split;
	unsigned int size = 8;
	struct fslist *fl = malloc(sizeof(struct fslist));
	fl->list = malloc(size * sizeof(char *));
	unsigned int count = 0;
	// make a list of all known filesystems
	FILE *f = fopen("/proc/filesystems", "r");

	if (!f) {
		perror("/proc/filesystems");
		exit(-1);
	}

	while (fgets(line, 80, f)) {
		line[strlen(line) - 1] = '\0';
		split = strchr(line, '\t');
		split++;
		fl->list[count] = malloc(strlen(split) + 1);
		strcpy(fl->list[count], split);
		count++;
		if (count == size) {
			size <<= 1;	/* size *= 2; */
			fl->list =
			    realloc(fl->list, size * sizeof(char *));
		}
		//printf("filesystem supported: %s\n",split);
	}
	fclose(f);
	fl->size = count;
	return fl;
}

void free_fslist(struct fslist *fl)
{
	int i;
	for (i = 0; i < fl->size; i++)
		free(fl->list[i]);
	free(fl);
}

void free_bootlist(struct bootlist *bl)
{
	int i;
	for (i = 0; i < bl->size; i++){
		free(bl->list[i]->device);
		free(bl->list[i]->fstype);
		free(bl->list[i]->kernelpath);
		if(bl->list[i]->cmdline)
			free(bl->list[i]->cmdline);
		free(bl->list[i]);
	}
	free(bl);
}


struct bootlist *scan_devices(struct hw_model_info *model)
{
	unsigned int size = 4;
	unsigned int count = 0;
	int major, minor, blocks;
	char line[80], device[80];
	char *tmp, **p, **kernels, *partinfo[4];
	const char *fstype;
	const char *kernelpath;
	FILE *g, *f;
	struct stat sinfo;
	struct bootlist *bl = malloc(sizeof(struct bootlist));
	bl->list = malloc(size * sizeof(struct boot));
	struct fslist *fl = scan_filesystems();

	// get list of bootable filesystems
	f = fopen("/proc/partitions", "r");
	if (!f) {
		perror("/proc/partitions");
		exit(-1);
	}

	char str_kernpath[] = "/mnt/boot/zImage";

	/* Where to look for kernels */
	char *default_kernel_paths[] = {
		NULL,
		str_kernpath,
		"/mnt/zImage",
		NULL
	};

	kernels = default_kernel_paths;

	/* Fill first kernel path with /mnt/boot/zImage-<machine> */
	tmp = malloc( strlen(model->name) * sizeof(char) );
	if (tmp) {
		strtolower(model->name, tmp);
		kernelpath = malloc( ( strlen(str_kernpath) +
			strlen(tmp) + 2 ) * sizeof(char) );

		strcpy(kernelpath, str_kernpath);
		strcat(kernelpath, "-");
		strcat(kernelpath, tmp);
		free(tmp);
		*kernels = kernelpath;
		DPRINTF("Kernelpath filled with %s\n", kernelpath);
	} else {
		/* We can't fill fist kernel path, skip it */
		++kernels;
	}

	// first two lines are bogus
	fgets(line, sizeof(line), f);
	fgets(line, sizeof(line), f);
	while (fgets(line, sizeof(line), f)) {
		line[strlen(line) - 1] = '\0';	/* Kill '\n' in the end of string */
		tmp = line;

		/* Split string by spaces or tabs into 4 fields - strsep() magic */
		for (p = partinfo; (*p = strsep(&tmp, " \t")) != NULL;)
			if (**p != '\0')
				if (++p >= &partinfo[4])
					break;

		major = atoi(partinfo[0]);
		minor = atoi(partinfo[1]);
		blocks = atoi(partinfo[2]);
		tmp = partinfo[3];

		/* Format device name */
		device[0] = '\0';
		if ( (strlen(tmp) + strlen("/dev/") + 1) >= sizeof(device) ) {
			DPRINTF("No enough memory for device name '%s', skipping\n", tmp);
			continue;
		}
		strcpy(device, "/dev/");
		strcat(device, tmp);
		DPRINTF("Got device %s (%d, %d) of size %dMb\n", device, major, minor, blocks>>10);

		/* Remove old device node and create new */
		DPRINTF("Re-creating device node %s (%d, %d)\n", device, major, minor);
		unlink(device);	/* We don't care about unlink() result */
		if ( -1 == mknod( device,
			(S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH),
			makedev(major, minor) ) )
		{
			perror("mknod");
			continue;
		}

		DPRINTF("Probing %s\n",device);
		int fd = open(device, O_RDONLY);
		if (fd < 0) {
			perror(device);
			continue;
		}
		DPRINTF("+ device is opened\n");

		if (-1 == identify_fs(fd, &fstype, NULL, 0)) {
			continue;
		}
		close(fd);
		DPRINTF("+ device is closed\n");

		if (!fstype) {
			continue;
		}
		DPRINTF("+ FS on device is %s\n", fstype);

		// no unknown filesystems
		if (contains(fstype, fl) == -1) {
			continue;
		}
		DPRINTF("+ FS is known\n");

		// mount fs
		if (mount(device, "/mnt", fstype, MS_RDONLY, NULL)) {
			perror(device);
			continue;
		}
		DPRINTF("+ FS is mounted\n");

		/* Check that kernel is exists on FS */
		p = kernels;
		while (*p) {
			DPRINTF("+ looking for %s\n", *p);
			if ( 0 == stat(*p, &sinfo) ) break; /* Break when found */
			++p; /* Go to next kernel path */
		}

		/* *p will be NULL when end of default_kernels is reached */
		if (NULL == *p) {
			DPRINTF("+ no kernel found on FS, umounting\n");
			umount("/mnt");
			continue;
		}

		DPRINTF("+ found kernel %s\n", *p);

		bl->list[count] = malloc(sizeof(struct boot));
		bl->list[count]->device = strdup(device);
		bl->list[count]->fstype = strdup(fstype);
		bl->list[count]->kernelpath = strdup(*p);

		if ( (g = fopen("/mnt/boot/kernel-cmdline", "r")) ) {
			bl->list[count]->cmdline = malloc(COMMAND_LINE_SIZE);
			fgets(bl->list[count]->cmdline, COMMAND_LINE_SIZE, g);
			fclose(g);
			DPRINTF("+ found command line\n");
			bl->list[count]->cmdline[strlen(bl->list[count]->cmdline)-1] = '\0';
		} else
			bl->list[count]->cmdline = NULL;

		count++;
		if (count == size) {
			size <<= 1;	/* size *= 2; */
			bl->list = realloc(bl->list, size * sizeof(struct boot *));
		}
		umount("/mnt");
	}
	free_fslist(fl);
	fclose(f);
	if (NULL != default_kernel_paths[0]) free(default_kernel_paths[0]);
	bl->size = count;
	return bl;
}
