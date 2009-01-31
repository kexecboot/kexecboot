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
			size *= 2;
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


struct bootlist *scan_devices()
{
	char line[80], *tmp, **p, *partinfo[4];
	int major, minor, blocks;
	unsigned int size = 4;
	unsigned int count = 0;
	char *device;
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
	// first two lines are bogus
	fgets(line, 80, f);
	fgets(line, 80, f);
	while (fgets(line, 80, f)) {
		line[strlen(line) - 1] = '\0';
		tmp = line;

		/* Split string by spaces or tabs into 4 fields - strsep() magic */
		for (p = partinfo; (*p = strsep(&tmp, " \t")) != NULL;)
			if (**p != '\0')
				if (++p >= &partinfo[4])
					break;

		tmp = partinfo[3];
		major = atoi(partinfo[0]);
		minor = atoi(partinfo[1]);
		blocks = atoi(partinfo[2])/1024;

		device = malloc( (strlen(tmp) + strlen("/dev/") + 1) * sizeof(char) );
		strcpy(device, "/dev/");
		strcat(device, tmp);

		DPRINTF("Got device %s (%d, %d) of size %dMb\n", device, major, minor, blocks);

		/* Create device node if not exists */
		if ( -1 == stat(device, &sinfo) )
			if (ENOENT == errno) {
				/* Devide node does not exists */
				DPRINTF("Device %s (%d, %d) does not exists\n", device, major, minor);
				if ( -1 == mknod( device, (S_IFBLK |
					S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH),
					makedev(major, minor) ) )
				{
					perror("mknod");
					free(device);
					continue;
				}
				DPRINTF("Device %s is created\n", device, major, minor);
			}

		DPRINTF("Probing %s\n",device);

		int fd = open(device, O_RDONLY);
		if (fd < 0) {
			perror(device);
			free(device);
			continue;
		}
		DPRINTF("+ device is opened\n");

		if (-1 == identify_fs(fd, &fstype, NULL, 0)) {
			free(device);
			continue;
		}
		close(fd);
		DPRINTF("+ device is closed\n");

		if (!fstype) {
			free(device);
			continue;
		}
		DPRINTF("+ FS on device is %s\n", fstype);

		// no unknown filesystems
		if (contains(fstype, fl) == -1) {
			free(device);
			continue;
		}
		DPRINTF("+ FS is known\n");

		// mount fs
		if (mount(device, "/mnt", fstype, MS_RDONLY, NULL)) {
			perror(device);
			free(device);
			continue;
		}
		DPRINTF("+ FS is mounted\n");

		if ( 0 == stat("/mnt/zImage", &sinfo) ) {
			kernelpath = "/mnt/zImage";
		} else if ( 0 == stat("/mnt/boot/zImage", &sinfo) ) {
			kernelpath = "/mnt/boot/zImage";
		} else {
			DPRINTF("+ no kernel found on FS, umounting\n", device);
			free(device);
			umount("/mnt");
			continue;
		}
		DPRINTF("+ found kernel\n");

		bl->list[count] = malloc(sizeof(struct boot));
		bl->list[count]->device = device;
		bl->list[count]->fstype = strdup(fstype);
		bl->list[count]->kernelpath = strdup(kernelpath);

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
			size *= 2;
			bl->list = realloc(bl->list, size * sizeof(struct boot *));
		}
		umount("/mnt");
	}
	free_fslist(fl);
	fclose(f);
	bl->size = count;
	return bl;
}
