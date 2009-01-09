/* 
 *  kexecboot - A kexec based bootloader 
 *
 *      Copyright (c) 2008 Thomas Kunze <thommycheck@gmx.de>
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
	char line[80];
	char *split;
	unsigned int size = 4;
	unsigned int count = 0;
	struct bootlist *bl = malloc(sizeof(struct bootlist));
	bl->list = malloc(size * sizeof(struct boot));
	struct fslist *fl = scan_filesystems();

	// get list of bootable filesystems     
	FILE *f = fopen("/proc/partitions", "r");
	if (!f) {
		perror("/proc/partitions");
		exit(-1);
	}
	// first two lines are bogus
	fgets(line, 80, f);
	fgets(line, 80, f);
	while (fgets(line, 80, f)) {
		char *device;
		const char *fstype;
		const char *kernelpath;
		FILE *g;

		line[strlen(line) - 1] = '\0';
		split = line + strspn(line, " 0123456789");
		device =
		    malloc((strlen(split) + strlen("/dev/") +
			    1) * sizeof(char));
		sprintf(device, "/dev/%s", split);
		printf("Probing %s\n",device);
		int fd = open(device, O_RDONLY);
		if (fd < 0) {
			perror(device);
			free(device);
			continue;
		}
		if (-1 == identify_fs(fd, &fstype, NULL, 0)) {
			free(device);
			continue;
		}
		close(fd);
		if (!fstype) {
			free(device);
			continue;
		}
		// no unknown filesystems
		if (contains(fstype, fl) == -1) {
			free(device);
			continue;
		}
		printf("found %s (%s)\n",device, fstype);
		// mount fs
		if (mount(device, "/mnt", fstype, MS_RDONLY, NULL)) {
			printf("mount failed\n");
			perror(device);
			free(device);
			continue;
		}
		printf("mount successful\n");
		if ( (g = fopen("/mnt/zImage", "r")) )
			kernelpath = "/mnt/zImage";
		else if ( (g = fopen("/mnt/boot/zImage", "r")) )
			kernelpath = "/mnt/boot/zImage";
		else {
			printf("%s no kernel found, umounting\n", device);
			free(device);
			umount("/mnt");
			continue;
		}
		fclose(g);
		printf("found kernel\n");
		bl->list[count] = malloc(sizeof(struct boot));
		bl->list[count]->device = device;
		bl->list[count]->fstype = fstype;
		bl->list[count]->kernelpath = kernelpath;
		if ( (g = fopen("/mnt/boot/kernel-cmdline", "r")) ) {
			bl->list[count]->cmdline =
			    malloc(COMMAND_LINE_SIZE);
			fgets(bl->list[count]->cmdline, COMMAND_LINE_SIZE,
			      g);
			fclose(g);
			printf("found command line\n");
			bl->list[count]->cmdline[strlen(bl->list[count]->cmdline)-1] = '\0';
		} else
			bl->list[count]->cmdline = NULL;

		count++;
		if (count == size) {
			size *= 2;
			bl->list =
			    realloc(bl->list,
				    size * sizeof(struct boot *));
		}
		umount("/mnt");
	}
	free_fslist(fl);
	fclose(f);
	bl->size = count;
	return bl;
}
