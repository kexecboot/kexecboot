#ifndef BOOTMAN_H_
#define BOOTMAN_H_

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <limits.h>
#include "devicescan.h"
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

int parse_config_file(global_settings *settings, menu_item *item);
void init_global_settings(global_settings *settings);
void free_global_settings(global_settings *settings);
void init_item_settings(int id, menu_item *item, char *device, const char *fstype);
void free_item_settings(menu_item *item);
void sort_bootlist(struct bootlist *bl, int low, int high);

#endif /*BOOTMAN_H_*/
