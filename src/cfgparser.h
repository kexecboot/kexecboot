/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2009-2011 Yuri Bushmelev <jay4mail@gmail.com>
 *  Copyright (c) 2009 Omegamoon <omegamoon@gmail.com>
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

#ifndef _HAVE_CONFIGPARSER_H
#define _HAVE_CONFIGPARSER_H

#include "config.h"
#include "util.h"

#define MOUNTPOINT	"/mnt"
#define BOOTCFG_PATH MOUNTPOINT "/boot/boot.cfg"

enum ui_type_t { GUI, TEXTUI };

typedef struct {
	char *label;		/* Partition label (name) */
	char *dtbpath;		/* Found dtb (/boot/dtb) */
	char *kernelpath;	/* Found kernel (/boot/zImage) */
	char *cmdline_append;	/* Appended kernel cmdline (logo.nologo debug) */
	char *cmdline;		/* Kernel cmdline */
	char *initrd;		/* Initial ramdisk file */
	char *iconpath;		/* Custom partition icon path */
	void *icondata;		/* Icon data */
	int is_default;		/* Use section as default? */
	int priority;		/* Priority of item in menu */
} kx_cfg_section;

/* Config file data structure */
struct cfgdata_t {
	int timeout;		/* Seconds before default item autobooting (0 - disabled) */
	enum ui_type_t ui;	/* UI (graphics/text) */
	int debug;			/* Use debugging */

	unsigned int size;	/* Size of sections array allocated */
	unsigned int count;	/* Sections count */
	kx_cfg_section **list;	/* List of sections */
	kx_cfg_section *current;	/* Latest allocated section */

	/* cmdline parameters */
	int angle;			/* FB angle */
	char *fbcon;		/* fbcon tag */
	char *mtdparts;		/* MTD partitioning */
	char *ttydev;		/* Console tty device name */
};

/* Clean config file structure */
void init_cfgdata(struct cfgdata_t *cfgdata);

/* Free config file sections */
void destroy_cfgdata(struct cfgdata_t *cfgdata);

/* Set kernelpath only (may be used when no config file found) */
int cfgdata_add_kernel(struct cfgdata_t *cfgdata, char *kernelpath);

/* Parse config file into specified structure */
/* NOTE: It will not clean cfgdata before parsing, do it yourself */
int parse_cfgfile(char *cfgpath, struct cfgdata_t *cfgdata);

int parse_cmdline(struct cfgdata_t *cfgdata);

#endif /* _HAVE_CONFIGPARSER_H */
