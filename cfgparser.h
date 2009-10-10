/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2009 Yuri Bushmelev <jay4mail@gmail.com>
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

#define MOUNTPOINT	"/mnt"
#define BOOTCFG_PATH MOUNTPOINT "/boot/boot.cfg"

enum ui_type_t { GUI, TEXTUI };

/* Config file data structure */
struct cfgdata_t {
	int timeout;		/* Seconds before default item autobooting (0 - disabled) */
	enum ui_type_t ui;	/* UI (graphics/text) */
	int debug;			/* Use debugging */

	int is_default;		/* Default item */
	char *label;		/* Partition label (name) */
	char *kernelpath;	/* Found kernel (/boot/zImage) */
	char *cmdline;		/* Kernel cmdline (logo.nologo debug) */
	char *iconpath;		/* Custom partition icon path */
	int priority;		/* Priority of item in menu */

	/* cmdline parameters */
	int angle;			/* FB angle */
	char *mtdparts;		/* MTD partitioning */
	char *ttydev;		/* Console tty device name */
};

/* Clean config file structure */
void init_cfgdata(struct cfgdata_t *cfgdata);

/* Parse config file into specified structure */
/* NOTE: It will not clean cfgdata before parsing, do it yourself */
int parse_cfgfile(char *cfgpath, struct cfgdata_t *cfgdata);

int parse_cmdline(struct cfgdata_t *cfgdata);

#endif /* _HAVE_CONFIGPARSER_H */
