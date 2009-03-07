/*
 *  bootman - A kexec based bootloader
 *
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

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <limits.h>
#include "util.h"

#define BOOTCFG_FILENAME "/mnt/boot/boot.cfg"
#define MAX_LINE_LENGTH 255

// Default event interface. Can be redefined
#ifndef KXB_EVENTIF
#define KXB_EVENTIF "/dev/event0"
#endif

// Default FB angle. Can be redefined
#ifndef KXB_FBANGLE
#define KXB_FBANGLE 0
#endif

#define MAX_CFG_KEYWORDS 11 // <- Make sure you adjust this when adding keyword(s)
enum cfg_keyword_id {
	CFG_UNKNOWN,
    // Global settings
	CFG_TIMEOUT,
	CFG_GUI,
	CFG_NOGUI,
	CFG_LOGO,
	CFG_DEBUG,
	// Individual settings
	CFG_DEFAULT,
	CFG_LABEL,
	CFG_KERNEL,
	CFG_ICON,
	CFG_APPEND,
	CFG_ORDER
};

struct cfg_keyword_rec {
	enum cfg_keyword_id keyword_id;
	char *keyword;
};

enum gui_type { GUIMODUS, TEXTMODUS };
enum debug_modus { DEBUG_ON, DEBUG_OFF };

typedef struct _menu_item_ {
	int  id;				// Unique menu ID
	char *device;			// eg. /dev/mmcblk0p1
	char *fstype;			// eg. ext2, ext4, jffs2
	char *description;		// Description of menu item as displayed
	char *kernelpath;		// Complete path to kernel image
	char *iconpath;			// Complete path to custom icon
	char *cmdline;			// Command line to be added on execution
	int  order;				// Sort order position
} menu_item;

typedef struct _global_settings_ {
	struct hw_model_info model;	// Device model
	int timeout;				// # Seconds before autostart default item (0=autostart disabled)
	enum gui_type gui;			// GUI/Textmodus
	menu_item *default_item;	// Default menu item (NULL=none selected)
	enum debug_modus debug;		// Debug modus
	char *logopath;				// Complete path to custom logo (NULL=use build-in logo)
} global_settings;

struct bootlist {
	menu_item **list;
	unsigned int size;
};

int parse_config_file(global_settings *settings, menu_item *item);
void init_global_settings(global_settings *settings);
void free_global_settings(global_settings *settings);
void init_item_settings(int id, menu_item *item, char *device, const char *fstype);
void free_item_settings(menu_item *item);
void sort_bootlist(struct bootlist *bl, int low, int high);

#endif /* _HAVE_CONFIGPARSER_H */
