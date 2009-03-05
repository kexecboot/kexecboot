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
 *  TODO:
 *  * Submenu's.
 * 		- Add keyword "GROUP". Each group adds a new submenu.
 * 		  Only one nesting level is supported.
 * 		- When in submenu, add option "Back to main menu"
 * 		  Assign (Zaurus) left cursor key to "Back to main menu"
 * 		- Automatically add a submenu "System" supporting:
 * 			- System information
 * 			- Debug information
 * 			- Reboot & Rescan
 *  * Paging / Scrolling
 * 		- Different handling for GUI and Text modus
 *  * Advanced handling of "DEFAULT".
 * 		- Situation where default item is in submenu
 *
 */
#include "configparser.h"

// Global variables (I hate them, they're ugly, but... I'm lazy, so I don't care!)
int linenr;

/* Known hardware models */
/* FIXME: better is to have this array in kexecboot.c/h */
struct hw_model_info model_info[] = {
	{HW_SHARP_POODLE,	"Poodle",	270	},
	{HW_SHARP_COLLIE,	"Collie",	270	},
	{HW_SHARP_CORGI,	"Corgi",	0	},
	{HW_SHARP_SHEPHERD,	"Shepherd",	0	},
	{HW_SHARP_HUSKY,	"Husky",	0	},
	{HW_SHARP_TOSA,		"Tosa",		0	},
	{HW_SHARP_AKITA,	"Akita",	270	},
	{HW_SHARP_SPITZ,	"Spitz",	270	},
	{HW_SHARP_BORZOI,	"Borzoi",	270	},
	{HW_SHARP_TERRIER,	"Terrier",	270	},

	{HW_MODEL_UNKNOWN,	"Unknown",	0	}
};

enum cfg_keyword_id keyword_id(char *key)
{
    int i;
    char* ukey;
    enum cfg_keyword_id result = CFG_UNKNOWN;
	struct cfg_keyword_rec cfg_keywords[] = {
		// Global bootmenu settings
		{ CFG_TIMEOUT,	"TIMEOUT" },
		{ CFG_GUI, 		"GUI" },
		{ CFG_NOGUI,	"NOGUI" },
		{ CFG_LOGO,		"LOGO" },
		{ CFG_DEBUG,	"DEBUG" },
		// Individual item settings
		{ CFG_DEFAULT,	"DEFAULT" },
		{ CFG_LABEL,	"LABEL" },
		{ CFG_KERNEL,	"KERNEL" },
		{ CFG_ICON,		"ICON" },
		{ CFG_APPEND,	"APPEND" },
		{ CFG_ORDER,	"ORDER" }
	};

    // For convenience convert key to uppercase
    ukey = upcase(key);

    // See if given key is known and return it's id when found
    for (i = 0; i < MAX_CFG_KEYWORDS; i++) {
        if (strcmp(ukey, cfg_keywords[i].keyword) == 0) {
            result = cfg_keywords[i].keyword_id;
            break;
        }
    }

    // Coming this far, key was not found
    free(ukey);
    return result;
}

void set_label(menu_item *i, char *value)
{
	if (!value) {
		// Description has to be specified
   		printf("** Syntax error in LABEL on line %d\n", linenr);
	} else {
		dispose(i->description);

		i->description = strdup(value);
	}
}

void set_kernel(menu_item *i, char *value)
{
	if (!value) {
		// Kernel path has to be specified
   		printf("** Syntax error in KERNEL on line %d\n", linenr);
	} else {
		dispose(i->kernelpath);

		// Add our mountpoint, since the enduser won't know it
		i->kernelpath = malloc(strlen(value)+5);
		i->kernelpath[0] = '\0';
		strcat(i->kernelpath, "/mnt");
		strcat(i->kernelpath, value);
	}
}

void set_icon(menu_item *i, char *value)
{
	if (!value) {
		// Icon path has to be specified
   		printf("** Syntax error in ICON on line %d\n", linenr);
	} else {
		dispose(i->iconpath);
		i->iconpath = strdup(value);

		// TODO: Load image into memory
	}
}

void set_cmdline(menu_item *i, char *value)
{
	if (!value) {
		// Command line has to be specified
   		printf("** Syntax error in APPEND on line %d\n", linenr);
	} else {
		dispose(i->cmdline);
		i->cmdline = strdup(value);
	}
}

void set_order(menu_item *i, char *value)
{
	if (!value) {
		// Order value has to be specified
		printf("** Syntax error in ORDER on line %d\n", linenr);
	} else {
		i->order = (int)strtol(value, NULL, 0);
	}
}

void set_timeout(global_settings *settings, char *value)
{
	if (!value) {
		// Timeout value has to be specified
		printf("** Syntax error in TIMEOUT on line %d\n", linenr);
	} else {
		settings->timeout = (int)strtol(value, NULL, 0);
	}
}

void set_interface(global_settings *settings, enum cfg_keyword_id gui)
{
	settings->gui = (gui == CFG_GUI) ? GUIMODUS : TEXTMODUS;
}

void set_logo(global_settings *settings, char *value)
{
	if (!value) {
		// Logo path & filename has to be specified
		printf("** Syntax error in LOGO on line %d\n", linenr);
	} else {
		dispose(settings->logopath);
		settings->logopath = strdup(value);
	}
}

void set_debug(global_settings *settings, char *value)
{
	if (!value) {
		// If no value is specified debugging is enabled
		settings->debug = DEBUG_ON;
	} else {
		upcase(value);

		if ( (strcmp(value, "ON") == 0) || (strcmp(value, "1") == 0) ) {
			settings->debug = DEBUG_ON;
		} else {
			if ( (strcmp(value, "OFF") == 0) || (strcmp(value, "0") == 0) ) {
				settings->debug = DEBUG_OFF;
			} else {
				// Syntax error
				printf("** Syntax error in DEBUG on line %d\n", linenr);
			}
		}
	}
}

void set_default_item(global_settings *settings, menu_item *item)
{
	settings->default_item = item;
}

void init_item_settings(int id, menu_item *item, char *device, const char *fstype)
{
	item->id = id;

	dispose(item->device);
	item->device = strdup(device);

	dispose(item->fstype);

	item->fstype = strdup(fstype);
	item->description = NULL;
	item->kernelpath = NULL;
	item->iconpath = NULL;
	item->icondata = NULL;
	item->cmdline = NULL;
	item->order = INT_MAX;  // Unordered
}

void free_item_settings(menu_item *item)
{
	dispose(item->device);
	dispose(item->fstype);
	dispose(item->description);
	dispose(item->kernelpath);
	dispose(item->iconpath);
	dispose(item->icondata);
	dispose(item->cmdline);
}

void init_global_settings(global_settings *settings)
{
	struct hw_model_info *model;
	// Disable the autostart function on default
	set_timeout(settings, "0");
	// GUI modus on default
	set_interface(settings, GUIMODUS);

	// Use build-in logo on default
	settings->logopath = NULL;
	settings->logodata = NULL;
	//set_logo(settings, "");

	settings->default_item = NULL;

	// Enable debugging on default
	set_debug(settings, "ON");

	// Try to detect the device we're running on
	model = detect_hw_model(model_info);
	settings->model = *model;
	if (settings->model.hw_model_id == HW_MODEL_UNKNOWN) {
		// Unknown model, use default fb angle
		settings->model.angle = KXB_FBANGLE;
	}
}

void free_global_settings(global_settings *settings)
{
	dispose(settings->logopath);
	dispose(settings->logodata);
}

void sort_bootlist(struct bootlist *bl, int low, int high)
{
    // Sort everything inbetween `low' <-> `high'
	int i = low;
	int j = high;
	menu_item *y;
	int z;

	// Bail out if nothing to do
	if (low >= high)
		return;

	// Compare value
	z = bl->list[(low + high) / 2]->order;

	// Partition
	do {
		// Find member above...
		while(bl->list[i]->order < z)
			i++;

		// Find element below...
		while(bl->list[j]->order > z)
			j--;

		if (i <= j) {
			// Swap two elements
			y = bl->list[i];
			bl->list[i] = bl->list[j];
			bl->list[j] = y;
			i++;
			j--;
		}
	} while(i <= j);

	// I just love recursion...
	if (low < j)
		sort_bootlist(bl, low, j);

	if (i < high)
		sort_bootlist(bl, i, high);
}

int parse_config_file(global_settings *settings, menu_item *item)
{
    FILE *bootcfg;
    char *filename = &BOOTCFG_FILENAME[0];
    struct stat info;
    char line[MAX_LINE_LENGTH];
    char *c;
    int result = -1;
    enum cfg_keyword_id key;
    int hasvalue;

	// Initialize (global variable) linenr
    linenr = 0;

    // Get file info
    if(stat(filename, &info) == -1)
        return -1;

    // Open the config file
    bootcfg = fopen(filename, "r");
    if (bootcfg) {
        // Read config file line by line
        while ( fgets( line, sizeof line, bootcfg ) != NULL ) {
        	// Keep track of the current line number
            linenr++;
            // Trim leading and trailing spaces
            trim(line);

            if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') {
                // Skip comment or empty line
                continue;
            } else {
                // Split line up in key and value
                c = strstr(line, "=");

                if (!c) {
                    // If '=' was not found set position to end of line
                    c = &line[strlen(line)];
                    hasvalue = 0;
                } else {
                	// When here, the '=' was found, indicating there is a key-value
                    hasvalue = 1;
                }

                if (!c) {
                    // Syntax error
                    printf("** Syntax error in line %d", linenr);
                } else {
                    // Replace the '=' by string terminator
                    *c = '\0';

					// When here, 'line' contains only the key
                    // Trim leading and trailing spaces from key
                    trim(line);

                    // Get key value if specified
                    if (hasvalue == 1) {
                        // make c point to key value
                        c++;
                        // Trim leading and trailing spaces from key-value
                        trim(c);
                    } else {
                    	c = NULL;
                    }

                    // Get keyword id
                    key = keyword_id(line);

                    switch (key) {
                        case CFG_UNKNOWN:    // Unknown option
                            printf("** Unknown option '%s'\n", line);
                            break;
                        case CFG_TIMEOUT:   // Syntax: TIMEOUT=<seconds>
                    		set_timeout(settings, c);
                            break;
						case CFG_GUI:		// Syntax: GUI
							set_interface(settings, CFG_GUI);
							break;
                        case CFG_NOGUI:     // Syntax: NOGUI
                        	set_interface(settings, CFG_NOGUI);
                        	break;
                    	case CFG_LOGO:		// Syntax: LOGO=<logo path & filename>
                        	set_logo(settings, c);
                        	break;
                    	case CFG_DEBUG:		// Syntax: DEBUG=<ON/OFF> or DEBUG=<0/1> or DEBUG (means ON)
                    		set_debug(settings, c);
                    		break;
                        case CFG_DEFAULT: 	// Syntax: DEFAULT
                            set_default_item(settings, item);
                            break;
                        case CFG_LABEL: 	// Syntax: LABEL=<Description>
                        	set_label(item, c);
                            break;
                        case CFG_KERNEL:   	// Syntax: KERNEL=<Kernel path & filename>
                        	set_kernel(item, c);
                        	break;
                    	case CFG_ICON:		// Syntax: ICON=<icon path & filename>
                    		set_icon(item, c);
                    		break;
                		case CFG_APPEND:	// Syntax: APPEND=<kernel cmdline addition>
                			set_cmdline(item, c);
                			break;
            			case CFG_ORDER:		// Syntax: ORDER=<item number>
            				set_order(item, c);
            				break;
                    }
                }
            }
        }

        fclose(bootcfg);
        result = 0;
    }

    return result;
}
