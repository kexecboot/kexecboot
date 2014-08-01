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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/mount.h>
#include <ctype.h>
#include <errno.h>

#include "config.h"
#include "util.h"
#include "cfgparser.h"

kx_cfg_section *cfg_section_new(struct cfgdata_t *cfgdata)
{
	kx_cfg_section *sc;

	if (!cfgdata) return NULL;

	/* Resize list when needed before adding item */
	if (cfgdata->count >= cfgdata->size) {
		kx_cfg_section **new_list;
		unsigned int new_size;

		new_size = cfgdata->size * 2;
		new_list = realloc(cfgdata->list, new_size * sizeof(*(cfgdata->list)));
		if (NULL == new_list) {
			DPRINTF("Can't resize cfgdata sections list");
			return NULL;
		}

		cfgdata->size = new_size;
		cfgdata->list = new_list;
	}

	sc = malloc(sizeof(*sc));
	if (NULL == sc) {
		DPRINTF("Can't allocate new cfgdata section");
		return NULL;
	}

	sc->label = NULL;
	sc->kernelpath = NULL;
	sc->cmdline = NULL;
	sc->initrd = NULL;
	sc->iconpath = NULL;
	sc->icondata = NULL;
	sc->priority = 0;
	sc->is_default = 0;

	cfgdata->list[cfgdata->count++] = sc;
	cfgdata->current = sc;

	return sc;
}

void init_cfgdata(struct cfgdata_t *cfgdata)
{
	cfgdata->timeout = 0;
	cfgdata->ui = GUI;
	cfgdata->debug = 0;
	cfgdata->current = NULL;

	cfgdata->size = 2;	/* NOTE: hardcoded value */
	cfgdata->count = 0;

	cfgdata->list = malloc(cfgdata->size * sizeof(*(cfgdata->list)));
	if (NULL == cfgdata->list) {
		DPRINTF("Can't allocate cfgdata sections array");
		cfgdata->list = NULL;
		return;
	}

	cfgdata->angle = 0;
	cfgdata->mtdparts = NULL;
	cfgdata->fbcon = NULL;
	cfgdata->ttydev = NULL;
}

void destroy_cfgdata(struct cfgdata_t *cfgdata)
{
	int i;

	if (!cfgdata) return;
	if (!cfgdata->list) return;

	for(i = 0; i < cfgdata->count; i++) {
		if (cfgdata->list[i]) {
			dispose(cfgdata->list[i]->iconpath);
			free(cfgdata->list[i]);
		}
	}
	free(cfgdata->list);

	cfgdata->current = NULL;
}


static int set_label(struct cfgdata_t *cfgdata, char *value)
{
	kx_cfg_section *sc;

	/* Allocate new section */
	sc = cfg_section_new(cfgdata);

	if (!sc) return -1;

	sc->label = strdup(value);
	return 0;
}

static int set_kernel(struct cfgdata_t *cfgdata, char *value)
{
	kx_cfg_section *sc;

	sc = cfgdata->current;
	if (!sc) return -1;

	dispose(sc->kernelpath);
	/* Add our mountpoint, since the enduser won't know it */
	sc->kernelpath = malloc(strlen(MOUNTPOINT)+strlen(value)+1);
	if (NULL == sc->kernelpath) {
		DPRINTF("Can't allocate memory to store kernelpath '%s'", value);
		return -1;
	}

	strcpy(sc->kernelpath, MOUNTPOINT);
	strcat(sc->kernelpath, value);
	return 0;
}

static int set_icon(struct cfgdata_t *cfgdata, char *value)
{
	kx_cfg_section *sc;

	sc = cfgdata->current;
	if (!sc) return -1;

	dispose(sc->iconpath);
	/* Add our mountpoint, since the enduser won't know it */
	sc->iconpath = malloc(strlen(MOUNTPOINT)+strlen(value)+1);
	if (NULL == sc->iconpath) {
		DPRINTF("Can't allocate memory to store iconpath '%s'", value);
		return -1;
	}

	strcpy(sc->iconpath, MOUNTPOINT);
	strcat(sc->iconpath, value);

	return 0;
}

static int set_cmdline(struct cfgdata_t *cfgdata, char *value)
{
	kx_cfg_section *sc;

	sc = cfgdata->current;
	if (!sc) return -1;

	dispose(sc->cmdline);
	sc->cmdline = strdup(value);
	return 0;
}

static int set_initrd(struct cfgdata_t *cfgdata, char *value)
{
	kx_cfg_section *sc;

	sc = cfgdata->current;
	if (!sc) return -1;

	dispose(sc->initrd);
	/* Add our mountpoint, since the enduser won't know it */
	sc->initrd = malloc(strlen(MOUNTPOINT)+strlen(value)+1);
	if (NULL == sc->initrd) {
		DPRINTF("Can't allocate memory to store initrd '%s'", value);
		return -1;
	}

	strcpy(sc->initrd, MOUNTPOINT);
	strcat(sc->initrd, value);
	return 0;
}

static int set_priority(struct cfgdata_t *cfgdata, char *value)
{
	kx_cfg_section *sc;

	sc = cfgdata->current;
	if (!sc) return -1;

	sc->priority = get_nni(value, NULL);
	if (sc->priority < 0) {
		log_msg(lg, "Can't convert '%s' to integer", value);
		sc->priority = 0;
		return -1;
	}
	return 0;
}

static int set_default(struct cfgdata_t *cfgdata, char *value)
{
	kx_cfg_section *sc;

	sc = cfgdata->current;
	if (!sc) return -1;

	sc->is_default = 1;
	return 0;
}

static int set_timeout(struct cfgdata_t *cfgdata, char *value)
{
	cfgdata->timeout = get_nni(value, NULL);
	if (cfgdata->timeout < 0) {
		log_msg(lg, "Can't convert '%s' to integer", value);
		cfgdata->timeout = 0;
		return -1;
	}
	return 0;
}

static int set_ui(struct cfgdata_t *cfgdata, char *value)
{
	switch (toupper(*value)) {
	case 'T':
		cfgdata->ui = TEXTUI;
		break;
	case 'G':
		cfgdata->ui = GUI;
		break;
	default:
		log_msg(lg, "Unknown value '%s' for UI keyword", value);
		return -1;
		break;
	}
	return 0;
}

static int set_debug(struct cfgdata_t *cfgdata, char *value)
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
			log_msg(lg, "Unknown value '%s' for DEBUG keyword", value);
			return -1;
		}
	}
	return 0;
}

static int set_fbcon(struct cfgdata_t *cfgdata, char *value)
{
	const char *str_rotate = "rotate:";
	char *c;
	int i;

	/* Value should be 'rotate:<n>' */
	if (0 != strncmp(str_rotate, value, strlen(str_rotate))) {
		/* This is not 'rotate' parameter. Skip it */
		return 0;
	}

	c = strchr(value, ':');
	if (NULL == c) {
		log_msg(lg, "Wrong 'rotate' value: %s", value);
		return -1;
	}

	dispose(cfgdata->fbcon);
	cfgdata->fbcon = strdup(value);

	++c;	/* Skip ':' */
	i = get_nni(c, NULL);
	if (i < 0) {
		log_msg(lg, "Can't convert '%s' to integer", c);
		return -1;
	}

	switch(i) {
	case 3:
		//cfgdata->angle = 270;
		cfgdata->angle = 90;
		break;
	case 2:
		cfgdata->angle = 180;
		break;
	case 1:
		//cfgdata->angle = 90;
		cfgdata->angle = 270;
		break;
	case 0:
	default:
		cfgdata->angle = 0;
	}

	return 0;
}

static int set_mtdparts(struct cfgdata_t *cfgdata, char *value)
{
	dispose(cfgdata->mtdparts);
	cfgdata->mtdparts = strdup(value);
	return 0;
}

static int set_ttydev(struct cfgdata_t *cfgdata, char *value)
{
	const char str_dev[] = "/dev/";
	char *p;

	/* Check value for 'tty[0-9]' */
	if ( ! ( ('t' == value[0]) && ('t' == value[1]) && ('y' == value[2]) &&
			(value[3] > '0') && (value[3] < '9') )
	) return 0;

	/* Prepend '/dev/' to tty name (value) */
	p = malloc(sizeof(str_dev)+strlen(value));
	if (NULL == p) {
		DPRINTF("Can't allocate memory to store tty device name '/dev/%s'", value);
		return -1;
	}

	dispose(cfgdata->ttydev);
	cfgdata->ttydev = p;
	strcpy(cfgdata->ttydev, str_dev);
	strcat(cfgdata->ttydev, value);

	return 0;
}

enum cfg_type_t { CFG_NONE, CFG_FILE, CFG_CMDLINE };

/* Config file (keywords -> parsing functions) tuples array */
struct cfg_keyfunc_t {
	enum cfg_type_t type;		/* cfgfile or cmdline */
	int has_value;	/* 0: option should not have value; 1: option should have value; -1: can have or not */
	char *keyword;
	int (*keyfunc)(struct cfgdata_t *, char *);
};

static struct cfg_keyfunc_t cfg_keyfunc[] = {
	/* Global bootmenu settings */
	{ CFG_FILE, 1, "TIMEOUT", set_timeout },
	{ CFG_FILE, 1, "UI", set_ui },
	{ CFG_FILE,-1, "DEBUG", set_debug },
	/* Individual item settings */
	{ CFG_FILE, 0, "DEFAULT", set_default },
	{ CFG_FILE, 1, "LABEL", set_label },
	{ CFG_FILE, 1, "KERNEL", set_kernel },
	{ CFG_FILE, 1, "ICON", set_icon },
	{ CFG_FILE, 1, "APPEND", set_cmdline },
	{ CFG_FILE, 1, "INITRD", set_initrd },
	{ CFG_FILE, 1, "PRIORITY", set_priority },
	{ CFG_CMDLINE, 1, "FBCON", set_fbcon },
	{ CFG_CMDLINE, 1, "MTDPARTS", set_mtdparts },
	{ CFG_CMDLINE, 1, "CONSOLE", set_ttydev },

	{ CFG_NONE, 0, NULL, NULL }
};


/* Process specified keyword */
int process_keyword(enum cfg_type_t cfg_type, struct cfgdata_t *cfgdata, char *keyword, char *value)
{
	struct cfg_keyfunc_t *kf;
	char ukey[strlen(keyword) + 1];

	/* For convenience convert keyword to lowercase */
	strtoupper(keyword, ukey);

	/* See if given keyword is known and call appropriate function */
	for (kf = cfg_keyfunc; kf->keyword != NULL; kf++) {
		if (cfg_type != kf->type) continue;
		if (0 == strcmp(ukey, kf->keyword)) {
			if ( (1 == kf->has_value) && (NULL == value) ) {
				log_msg(lg, "+ keyword '%s' should have value", ukey);
				return -1;
			}
			return kf->keyfunc(cfgdata, value);
		}
	}

	/* Coming this far, keyword was not found */
	return -1;
}


/* Parse config file into specified structure */
/* NOTE: It will not clean cfgdata before parsing, do it yourself */
int parse_cfgfile(char *path, struct cfgdata_t *cfgdata)
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
		log_msg(lg, "+ can't open config file: %s", ERRMSG);
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
		if (-1 == process_keyword(CFG_FILE, cfgdata, keyword, value)) {
			log_msg(lg, "Can't parse keyword '%s'", keyword);
		}
	}

	fclose(f);
	return 0;
}

int parse_cmdline(struct cfgdata_t *cfgdata)
{
	FILE *f;
	char *c;
	char *keyword;
	char *value;
	char line[COMMAND_LINE_SIZE];

	/* Open /proc/cmdline and read cmdline */
	f = fopen("/proc/cmdline", "r");
	if (NULL == f) {
		log_msg(lg, "Can't open /proc/cmdline: %s", ERRMSG);
		return -1;
	}

	if ( NULL == fgets(line, sizeof(line), f) ) {
		log_msg(lg, "Can't read /proc/cmdline: %s", ERRMSG);
		fclose(f);
		return -1;
	}

	fclose(f);

	log_msg(lg, "Kernel cmdline: %s", line);

	c = line;
	/* Split string to words */
	while ( NULL != (keyword = get_word(c, &c)) ) {
		if ( ('\0' == keyword[0]) || ('#' == keyword[0]) ) {
			/* Skip comment or empty line */
			continue;
		}

		*c = '\0';	/* NULL-terminate keyword-value pair */
		++c;

		/* Try to split line up to key and value */
		value = strchr(keyword, '=');
		if (NULL != value) {	/* '=' was found. We have value */
			/* Split string to keyword and value */
			*value = '\0';
			++value;
		}

		/* Process keyword and value */
		process_keyword(CFG_CMDLINE, cfgdata, keyword, value);
	}

	return 0;
}

/* Set kernelpath only (may be used when no config file found) */
int cfgdata_add_kernel(struct cfgdata_t *cfgdata, char *kernelpath)
{
	kx_cfg_section *sc;

	/* Allocate new section */
	sc = cfg_section_new(cfgdata);
	if (!sc) return -1;

	sc->kernelpath = strdup(kernelpath);
	return 0;
}
