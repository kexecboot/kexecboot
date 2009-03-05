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
#include "devicescan.h"
#include "configparser.h"
#include "util.h"

// lame search
char *contains(const char *what, struct fslist *where)
{
	char **p, **e;
	e = where->list + where->size;
	for (p = where->list; p < e; p++) {
		if (!strcmp(what, *p))
			return *p;
	}
	return NULL;
}

struct fslist *scan_filesystems()
{
	unsigned int size = 16;
	unsigned int count = 0;
	struct fslist *fl;
	char line[80];
	char *split;
	const char *filesystems = "/proc/filesystems";

	fl = malloc(sizeof(*fl));
	if (NULL == fl) {
		DPRINTF("Can't allocate memory for fslist structure\n");
		return NULL;
	}

	fl->list = malloc(size * sizeof(char *));
	if (NULL == fl->list) {
		DPRINTF("Can't allocate memory for fslist->list\n");
		return NULL;
	}

	// make a list of all known filesystems
	FILE *f = fopen(filesystems, "r");

	if (!f) {
		perror(filesystems);
		exit(-1);
	}

	while (fgets(line, sizeof(line), f)) {
		line[strlen(line) - 1] = '\0';
		split = strchr(line, '\t');
		if (NULL == split) {
			DPRINTF("%s has wrong format (no '\\t')\n", filesystems);
			continue;
		}
		fl->list[count] = strdup(split + 1);
		if (NULL == fl->list[count]) {
			DPRINTF("Can't allocate memory for fs name (%s)\n", split + 1);
			continue;
		}

		++count;
		if (count == size) {
			size <<= 1;	// size *= 2;
			fl->list = realloc(fl->list, size * sizeof(char *));
		}
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
	for (i = 0; i < bl->size; i++) {
		free_item_settings(bl->list[i]);
		free(bl->list[i]);
	}
	free(bl);
}

int check_for_kernel(char *leadin, char *append, menu_item *i)
{
	char *path;
	int result = -1, size;
	struct stat sinfo;

	if (append) {
		// Create path with machine name appended
		size = strlen(leadin);
		path = malloc(size+strlen(append)+2);

		strcpy(path, leadin);
		strcat(path, "-");
		strcat(path, append);
	} else {
		// Use path 'as is'
		path = strdup(leadin);
	}

	// Check to see if kernel exists on given path
	if (stat(path, &sinfo) == 0)
		result = 0;

	if (result == 0) {
		// Store complete kernel path
		i->kernelpath = path;
	} else {
		// Kernel not found. Free memory used
		free(path);
	}

	return result;
}

struct bootlist *scan_devices(global_settings *settings)
{
	int major, minor, blocks;
	char line[80];
	char device[80];
	char *tmp, **p, *partinfo[4], *name;
	const char *fstype;
	FILE *f;
	struct stat sinfo;
	menu_item *item = NULL;
	char procparts[] = "/proc/partitions";
	int found, max_id = 0;

    // On default allocate space for 4 boot devices
	unsigned int size = 4;
	unsigned int count = 0;
	struct bootlist *bl = malloc(sizeof(struct bootlist));
	bl->list = malloc(size * sizeof(menu_item));

    // Get a list of all filesystems registered in the kernel
	struct fslist *fl = scan_filesystems();

	// Get a list of available partitions on all devices
	// See kernel/block/genhd.c for details on interface
	f = fopen(procparts, "r");
	if (!f) {
		// This is a fatal error, so we're out of here!
		perror(procparts);
		DPRINTF("! Unable to get list of available partitions");
		exit(-1);
	}

	// First two lines are bogus.
	fgets(line, sizeof(line), f);
	fgets(line, sizeof(line), f);

	while (fgets(line, sizeof(line), f)) {
		// Trim leading and trailing spaces, tabs and newlines
		trim(line);
		tmp = line;

		// Split string by spaces or tabs into 4 fields - strsep() magic
		for (p = partinfo; (*p = strsep(&tmp, " \t")) != NULL;) {
			if (**p != '\0')
				if (++p >= &partinfo[4])
					break;
		}

		major = atoi(partinfo[0]);
		minor = atoi(partinfo[1]);
		blocks = atoi(partinfo[2]);
		tmp = partinfo[3];

		// Format device name
		device[0] = '\0';
		if ( (strlen(tmp) + strlen("/dev/") + 1) >= sizeof(device) ) {
			// We shouldn't come here, but anyway
			DPRINTF("! Maximum length of device name exceeded in '%s', skipping\n", tmp);
		} else {
			strcpy(device, "/dev/");
			strcat(device, tmp);

			DPRINTF("- Got device '%s' (%d, %d) of size %dMb\n", device, major, minor, blocks>>10);

			// Remove old device node...
			unlink(device);	// We don't care about unlink() result

			// ...Recreate device node
			DPRINTF("- Recreating device node %s (%d, %d)\n", device, major, minor);
			if ( -1 == mknod( device,
				(S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH),
				makedev(major, minor) ) )
			{
				perror("mknod");
				DPRINTF("! Error creating device node\n");
			}

			// Tests on x86 always result in error after device re-creation, so...
			// Try opening the device, regardless of the result from the node recreation
			DPRINTF("- Probing device %s\n",device);

			// Try to open the device (read-only)
			int fd = open(device, O_RDONLY);
			if (fd < 0) {
				// Unable to open device
				perror(device);
				DPRINTF("  ! Error opening device\n");
			} else {
				// Device is opened succesfully
				DPRINTF("  - Device is opened\n");

				// Try to identify the filesystem
				DPRINTF("  - Detecting filesystem\n");
				major = identify_fs(fd, &fstype, NULL, 0);	// Reused variable major

				// That's all we wanted to know, so close the device
				close(fd);
				DPRINTF("  - Device is closed\n");

				// Is this a filesystem we support?
				// major: 0=identified; 1=unknown; -1=error
				if ( (-1 == major) || (!fstype) ) {
					DPRINTF("  ! Filesystem NOT supported by kexecboot\n");
				} else {
					DPRINTF("  - Filesystem '%s' supported by kexecboot\n", fstype);

					// Is filesystem supported by kernel?
					if (NULL == contains(fstype, fl)) {
						DPRINTF("  ! Filesystem NOT supported by kernel\n");
					} else {
						DPRINTF("  - Filesystem supported by kernel\n");

						// Try mounting the filesystem (read-only)
						if (mount(device, "/mnt", fstype, MS_RDONLY, NULL)) {
							perror(device);
							DPRINTF("  ! Filesystem could not be mounted\n");
						} else {
							DPRINTF("  - Filesystem succesfully mounted\n");

							/*
							 *  - check for existance of boot.cfg
							 *    - read and parse config file
							 * Search kernel in order:
							 * 	- Kernel name as specified in boot.cfg (KERNEL option)
							 * 	- If machinename is known: /mnt/boot/zImage-machinename
							 * 	- /mnt/boot/zImage
							 *  - /mnt/zImage
							 */

							// The pessimist in us tells us we won't find a kernel
							found = -1;

							// Allocate memory for a new item, but only if needed
							if (!item) {
								max_id++;
								item = malloc(sizeof(menu_item));
								item->device = NULL;
								item->fstype = NULL;
							}

							// (Re)Initialize item with default settings
							init_item_settings(max_id, item, &device[0], &fstype[0]);

							// Try reading the 'boot.cfg' file
						    if (parse_config_file(settings, item) == -1) {
						        DPRINTF("  - No configuration file 'boot.cfg' found\n");
						    } else {
						        DPRINTF("  - Configuration file 'boot.cfg' parsed\n");
						    }

							// 1. If KERNEL parameter is used in 'boot.cfg', check for existance
							if ( (item->kernelpath) && (stat(item->kernelpath, &sinfo) == 0) ) {
								// Kernel found, we're happy!
								found = 0;
							}

							if (found == -1) {
								// 2. If machinename is known, check for zImage-machinename
								if (settings->model.hw_model_id != HW_MODEL_UNKNOWN) {
									// Build kernel name & check existance
									// If exists: store path in item->kernelpath
									name = locase(settings->model.name);

									// This checks for instance /mnt/boot/zImage-spitz
									found = check_for_kernel("/mnt/boot/zImage", name, item);
									if (found == -1)
										found = check_for_kernel("/mnt/zImage", name, item);

									// Free temporary storage
									free(name);
								}

								if (found == -1) {
									// 3. Check for kernel in /mnt/boot/zImage
									found = check_for_kernel("/mnt/boot/zImage", NULL, item);
									if (found == -1) {
										// 4. Check for kernel in /mnt/zImage
										found = check_for_kernel("/mnt/zImage", NULL, item);
									}
								}
							}

							if (found == -1) {
								DPRINTF("  ! No kernel found\n");
							} else {
								DPRINTF("  - Using kernel %s\n", item->kernelpath);

								bl->list[count] = item;
								item = NULL; // Force allocation of new item on next iteration

								count++;

								// Check to see if we have room for more items
								if (count == size) {
									// No more room. Reallocate the list and double the size
									size <<= 1; // size *= 2;
									bl->list = realloc(bl->list, size * sizeof(struct menu_item *));
								}
							}

							// Unmount the filesystem
							umount("/mnt");
						}
					}
				}
			}
		}
	}

	// Check if item has to be deallocated
	if (item)
		free(item);

	// Dispose of list with registered filesystems
	free_fslist(fl);

	// Close /proc/partitions
	fclose(f);

	bl->size = count;
	return bl;
}
