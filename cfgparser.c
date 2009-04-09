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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/mount.h>
#include <ctype.h>
#include "util.h"
#include "cfgparser.h"

static int set_label(struct cfgfile_t *cfgdata, char *value)
{
	dispose(cfgdata->label);
	cfgdata->label = strdup(value);
	return 0;
}

static int set_kernel(struct cfgfile_t *cfgdata, char *value)
{
	dispose(cfgdata->kernelpath);
	/* Add our mountpoint, since the enduser won't know it */
	cfgdata->kernelpath = malloc(strlen(MOUNTPOINT)+strlen(value)+1);
	if (NULL == cfgdata->kernelpath) {
		DPRINTF("Can't allocate memory to store kernelpath '%s'\n", value);
		return 0;	/* This is not parsing error */
	}

	strcpy(cfgdata->kernelpath, "/mnt");
	strcat(cfgdata->kernelpath, value);
	return 0;
}

static int set_icon(struct cfgfile_t *cfgdata, char *value)
{
	dispose(cfgdata->iconpath);
	cfgdata->iconpath = strdup(value);
	return 0;
}

static int set_cmdline(struct cfgfile_t *cfgdata, char *value)
{
	dispose(cfgdata->cmdline);
	cfgdata->cmdline = strdup(value);
	return 0;
}

static int set_priority(struct cfgfile_t *cfgdata, char *value)
{
	cfgdata->priority = get_nni(value, NULL);
	if (cfgdata->priority < 0) {
		DPRINTF("Can't convert '%s' to integer\n", value);
		cfgdata->priority = 0;
		return -1;
	}
	return 0;
}

static int set_timeout(struct cfgfile_t *cfgdata, char *value)
{
	cfgdata->timeout = get_nni(value, NULL);
	if (cfgdata->timeout < 0) {
		DPRINTF("Can't convert '%s' to integer\n", value);
		cfgdata->timeout = 0;
		return -1;
	}
	return 0;
}

static int set_ui(struct cfgfile_t *cfgdata, char *value)
{
	switch (toupper(*value)) {
	case 'T':
		cfgdata->ui = TEXTUI;
		break;
	case 'G':
		cfgdata->ui = GUI;
		break;
	default:
		DPRINTF("Unknown value '%s' for UI keyword\n", value);
		return -1;
		break;
	}
	return 0;
}

static int set_debug(struct cfgfile_t *cfgdata, char *value)
{
	if (!value) {
		/* If no value is specified debugging is enabled */
		cfgdata->debug = 1;
	} else {
		char v[strlen(value)+1];

		strtoupper(value, v);

		if ( (strcmp(v, "ON") == 0) || (strcmp(v, "1") == 0) ) {
			cfgdata->debug = 1;
		} else if ( (strcmp(v, "OFF") == 0) || (strcmp(v, "0") == 0) ) {
			cfgdata->debug = 0;
		} else {
			DPRINTF("Unknown value '%s' for DEBUG keyword\n", value);
			return -1;
		}
	}
	return 0;
}

static int set_default(struct cfgfile_t *cfgdata, char *value)
{
	cfgdata->is_default = 1;
	return 0;
}


void init_cfgfile(struct cfgfile_t *cfgdata)
{
	cfgdata->timeout = 0;
	cfgdata->ui = GUI;
	cfgdata->debug = 0;

	cfgdata->is_default = 0;
	cfgdata->label = NULL;
	cfgdata->kernelpath = NULL;
	cfgdata->cmdline = NULL;
	cfgdata->iconpath = NULL;
	cfgdata->priority = 0;
}


/* Config file (keywords -> parsing functions) tuples array */
struct cfg_keyfunc_t {
	int has_value;	/* 0: option should not have value; 1: option should have value; -1: can have or not */
	char *keyword;
	int (*keyfunc)(struct cfgfile_t *, char *);
};

static struct cfg_keyfunc_t cfg_keyfunc[] = {
	/* Global bootmenu settings */
	{ 1, "TIMEOUT", set_timeout },
	{ 1, "UI", set_ui },
	{-1, "DEBUG", set_debug },
	/* Individual item settings */
	{ 0, "DEFAULT", set_default },
	{ 1, "LABEL", set_label },
	{ 1, "KERNEL", set_kernel },
	{ 1, "ICON", set_icon },
	{ 1, "APPEND", set_cmdline },
	{ 1, "PRIORITY", set_priority },
	{ 0, NULL, NULL }
};


/* Process specified keyword */
int process_keyword(struct cfgfile_t *cfgdata, char *keyword, char *value)
{
	struct cfg_keyfunc_t *kf;
	char ukey[strlen(keyword) + 1];

	/* For convenience convert keyword to lowercase */
	strtoupper(keyword, ukey);

	/* See if given keyword is known and call appropriate function */
	for (kf = cfg_keyfunc; kf->keyword != NULL; kf++) {
		if (0 == strcmp(ukey, kf->keyword)) {
			if ( (1 == kf->has_value) && (NULL == value) ) {
				DPRINTF("Keyword '%s' should have value\n", ukey);
				return -1;
			}
			return kf->keyfunc(cfgdata, value);
		}
	}

	/* Coming this far, keyword was not found */
	DPRINTF("Unknown keyword: '%s'\n", ukey);
	return -1;
}


/* Parse config file into specified structure */
/* NOTE: It will not clean cfgdata before parsing, do it yourself */
int parse_cfgfile(char *path, struct cfgfile_t *cfgdata)
{
	int linenr = 0;
	FILE *f;
	char *c;
	char *keyword;
	char *value;
	char line[256];

	/* Open the config file */
	f = fopen(path, "r");
	if (NULL == f) {
		DPRINTF("Can't open config file '%s'\n", path);
		return -1;
	}

	/* Read config file line by line */
	while (fgets(line, sizeof(line), f)) {
		++linenr;
		/* Skip white-space from beginning */
		keyword = ltrim(line);

		if ( ('\0' == keyword[0]) || ('#' == keyword[0]) ) {
			/* Skip comment or empty line */
			continue;
		}
		/* Try to split line up to key and value */
		c = strchr(keyword, '=');
		if (NULL != c) {	/* '=' was found. We have value */
			/* Split string to keyword and value */
			*c = '\0';
			++c;

			value = trim(c);	/* Strip white-space from value */
		} else {
			value = NULL;
		}
		c = rtrim(keyword);	/* Strip trailing white-space from keyword */
		*(c+1) = '\0';

		/* Process keyword and value */
		if (-1 == process_keyword(cfgdata, keyword, value)) {
			DPRINTF("Can't parse line %d (%s)!\n", linenr, keyword);
		}
	}

	fclose(f);
	return 0;
}
