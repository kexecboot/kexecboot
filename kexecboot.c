/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2008-2009 Yuri Bushmelev <jay4mail@gmail.com>
 *  Copyright (c) 2008 Thomas Kunze <thommycheck@gmx.de>
 *
 *  small parts:
 *  Copyright (c) 2006 Matthew Allum <mallum@o-hand.com>
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
#include <linux/input.h>
#include <termios.h>
#include <unistd.h>
#include <sys/mount.h>
#include <ctype.h>
#include <errno.h>
#include <sys/reboot.h>
#include <asm/setup.h> // for COMMAND_LINE_SIZE
#include "util.h"
#include "fb.h"
#include "devicescan.h"
#include "xpm.h"

#include "res/radeon-font.h"
#include "res/logo.xpm"
/* Default images data for CF, SD/MMC and NAND */
#include "res/cf.xpm"
#include "res/mmc.xpm"
#include "res/memory.xpm"

#include "kexecboot.h"

/* Global images like before */
struct xpm_parsed_t *icon_logo, *icon_cf, *icon_mmc, *icon_memory;

/* Machine-dependent kernel patch */
char *machine_kernel = NULL;

/* NULL-terminated array of kernel search paths
 * First item should be filled with machine-dependent path */
char *default_kernels[] = {
	"/mnt/boot/zImage",
	"/mnt/zImage",
	NULL
};

/* Tags we want from /proc/cmdline */
char *wanted_tags[] = {
	"mtdparts",
	"fbcon",
	NULL
};


/* Draw background with logo and text */
void draw_background(FB *fb, const char *text)
{
	int margin = fb->width/40;

	/* Clear the background with #ecece1 */
	fb_draw_rect(fb, 0, 0, fb->width, fb->height,0xec, 0xec, 0xe1);

	fb_draw_xpm_image(fb, 0, 0, icon_logo);

	fb_draw_text (fb, 32 + margin, margin, 0, 0, 0,
		&radeon_font, text);
}

/* Draw one slot in menu */
void draw_slot(FB *fb, struct boot_item_t *item, int slot, int height,
		int iscurrent, struct xpm_parsed_t *icon)
{
	int margin = (height - 32)/2;
	char text[100];
	if(!iscurrent)
		fb_draw_rect(fb, 0, slot*height, fb->width, height,
			0xec, 0xec, 0xe1);
	else { //draw red border
		fb_draw_rect(fb, 0, slot*height, fb->width, height,
			0xff, 0x00, 0x00);
		fb_draw_rect(fb, margin, slot*height+margin, fb->width-2*margin,
			height-2*margin, 0xec, 0xec, 0xe1);
	}

	if (NULL != icon) {
		fb_draw_xpm_image(fb, 0, 0, icon);
	} else if(!strncmp(item->device, "/dev/hd", strlen("/dev/hd"))) {
		fb_draw_xpm_image(fb, margin, slot * height + margin, icon_cf);
	} else if(!strncmp(item->device, "/dev/mmcblk", strlen("/dev/mmcblk"))) {
		fb_draw_xpm_image(fb, margin, slot * height + margin, icon_mmc);
	} else if(!strncmp(item->device, "/dev/mtdblock", strlen("/dev/mtdblock"))) {
		fb_draw_xpm_image(fb, margin, slot * height + margin, icon_memory);
	}
	sprintf(text, "%s\n%s (%s)", item->label, item->device, item->fstype);
	fb_draw_text (fb, 32 + margin, slot * height + 4, 0, 0, 0,
			&radeon_font, text);

}

/* Display bootlist menu with selection */
void display_menu(FB *fb, struct bootconf_t *bc, int current,
		struct xpm_parsed_t **icons_array)
{
	int i,j;
	int slotheight = 40;
	int slots = fb->height/slotheight -1;
	// struct boot that is in fist slot
	static int firstslot=0;

	if (0 == bc->fill) {
		draw_background(fb, "No bootable devices found.\nInsert bootable device!\nR: Reboot  S: Rescan devices");
	} else {
		draw_background(fb, "Make your choice by selecting\nan item with the cursor keys.\nOK/Enter: Boot selected device\nR: Reboot  S: Rescan devices");
	}

	if(current < firstslot)
		firstslot=current;
	if(current > firstslot + slots -1)
		firstslot = current - (slots -1);
	for(i=1, j=firstslot; i <= slots && j< bc->fill; i++, j++){
		draw_slot(fb, bc->list[j], i, slotheight, j == current, icons_array[j]);
	}
	fb_render(fb);
}

/* Display custom text near logo */
void display_text(FB *fb, const char *text)
{
	draw_background(fb, text);
	fb_render(fb);
}


/* Iterate through bootlist and associate icons with items */
int associate_icons(struct bootconf_t *bc, int bpp, struct xpm_parsed_t ***icons_array)
{
	int i, rows;
	char *tmp, **xpm_data;
	struct boot_item_t **pbi;
	struct xpm_parsed_t **icons, **pi, *icon;

	if (0 == bc->fill) return 0;	/* may be (-1)? */

	/* Allocate array of bl->fill num icons */
	icons = malloc(sizeof(icons) * bc->fill);
	if (NULL == icons) {
		DPRINTF("Can't allocate memory for icons array\n");
		*icons_array = NULL;
		return -1;
	}

	pbi = bc->list;
	pi = icons;
	for (i = 0; i < bc->fill; i++) {

		icon = NULL;

		tmp = (*pbi)->iconpath;
		if (NULL != tmp) {	/* Load and parse image */
			rows = xpm_load_image(&xpm_data, tmp);
			if (rows > 0) {
				icon = xpm_parse_image(xpm_data, rows, bpp);
				/* destroy loaded data - not needed anymore */
				xpm_destroy_image(xpm_data, rows);
			}
		}

		/* Store and go to next item */
		*pi = icon;
		++pbi;
		++pi;
	}
	*icons_array = icons;

	return i;
}


void free_associated_icons(struct xpm_parsed_t **icons_array, int ia_size)
{
	if ( (NULL == icons_array) || (0 == ia_size) ) return;

	int i;
	struct xpm_parsed_t **p = icons_array;

	for (i = 0; i < ia_size; i++) {
		if (NULL != *p) xpm_destroy_parsed(*p);
		++p;
	}
}


/* Return lowercased and stripped machine-specific kernel path */
/* Return value should be free()'d */
char *get_machine_kernelpath() {
	FILE *f;
	char *tmp, *hw, c;
	char line[80];

	f = fopen("/proc/cpuinfo", "r");
	if (!f) {
		perror("/proc/cpuinfo");
		exit(-1);
	}

	/* Search string 'Hardware' */
	while (fgets(line, sizeof(line), f)) {
		line[strlen(line) - 1] = '\0';
		hw = strstr(line, "Hardware");
		if (NULL != hw) break;
	}
	fclose(f);

	if ( NULL != hw) {
		/* Search colon then skip it and space after */
		hw = strchr(hw, ':');
		if (NULL == hw) {	/* Should not happens but anyway */
			DPRINTF("Can't find ':' in 'Hardware' line\n");
			return NULL;
		}
		hw += 2;	/* May be ltrim()? */

		/* Lowercase name and replace spaces with '_' */
		tmp = hw;
		while('\0' != *tmp) {
			c = *tmp;
			if (isspace(c)) {
				*tmp = '_';
			} else {
				*tmp = tolower(c);
			}
			++tmp;
		}

		/* Prepend "/mnt/boot/zImage-" to hw */
		tmp = malloc(strlen(hw) + 17 + 1);	/* strlen("/mnt/boot/zImage-") */
		if (NULL == tmp) {
			DPRINTF("Can't allocate memory for machine-specific kernel path\n");
			return NULL;
		}

		strcpy(tmp, "/mnt/boot/zImage-");
		strcat(tmp, hw);

		return tmp;
	}

	DPRINTF("Can't find 'Hardware' line in cpuinfo\n");
	return NULL;
}


/*
 * Function: get_extra_cmdline()
 * It gets wanted tags from original cmdline.
 * Takes 2 args:
 * - extra_cmdline - buffer to store cmdline parameters;
 * - extra_cmdline_size - size of buffer
 *   (inc. terminating '\0').
 * Return values:
 * - length of extra_cmdline on success (w/o term. zero);
 * - -1 on error;
 * - (- length of extra_cmdline - 1)  on insufficient buffer space.
 * REFACTOR: do something with wanted_tags and this function (may be move to util)
 */

int get_extra_cmdline(char *const extra_cmdline, const size_t extra_cmdline_size)
{
	char *p, *t, *tag = NULL;
	char line[COMMAND_LINE_SIZE];
	int i, len, sp_size;
	int sum_len = 0;
	int wanted_tag_found = 0;
	int overflow = 0;

	const char proc_cmdline_path[] = "/proc/cmdline";

	/* Open /proc/cmdline and read cmdline */
	FILE *f = fopen(proc_cmdline_path, "r");
	if (NULL == f) {
		perror("Can't open /proc/cmdline");
		return -1;
	}

	if ( NULL == fgets(line, sizeof(line), f) ) {
		perror("Can't read /proc/cmdline");
		fclose(f);
		return -1;
	}

	fclose(f);

	/* clean up buffer before parsing */
	t = extra_cmdline;
	*t = '\0';

	p = line-1;		/* because of ++p below */

	sp_size = 0;	/* size of (first) space */

	do {
		++p;

		if ( (NULL == tag) && (isalnum(*p)) ) {
			/* new tag found */
			tag = p;
		} else if (tag) {
			/* we are working on some tag */

			if (isspace(*p) || ('\0' == *p) || ('=' == *p) ) {
				/* end of tag or '=' found */

				if (!wanted_tag_found) {
					/* search in wanted_tags */
					for (i = 0; wanted_tags[i] != NULL; i++) {
						if ( 0 == strncmp(wanted_tags[i], tag, p-tag) ) {
							/* match found */
							wanted_tag_found = 1;
							break;
						}
					}
				}

				if ( ('=' != *p) && wanted_tag_found ) {
					/* end of wanted tag found -> copy */

					len = p - tag;
					if ( (sum_len + len + sp_size) < extra_cmdline_size ) {
						if (sp_size) {
							/* prepend space when have tags already */
							*t = ' ';
							++t;
							*t = '\0';
							++sum_len;
						} else {
							sp_size = sizeof(char);
						}

						/* NOTE: tag is not null-terminated so copy only
						 * len chars and terminate it directly
						 */
						strncpy(t, tag, len);
						/* move pointer to position after copied tag */
						t += len ;
						/* null-terminate */
						*t = '\0';
						/* update summary length */
						sum_len += len;
					} else {
						/* we have no space - skip this tag */
						overflow = 1;
					}

					/* reset wanted_tag_found */
					wanted_tag_found = 0;
				}

				/* reset tag */
				if (!wanted_tag_found) tag = NULL;

			}
		}

	} while ('\0' != *p);

	if (overflow) return -sum_len-1;
	return sum_len;
}

void start_kernel(struct boot_item_t *item)
{
	/* we use var[] instead of *var because sizeof(var) using */
	const char mount_point[] = "/mnt";
	const char kexec_path[] = "/usr/sbin/kexec";

	const char str_cmdline_start[] = "--command-line=";
	const char str_root[] = " root=";
	const char str_rootfstype[] = " rootfstype=";
	const char str_rootwait[] = " rootwait";

	/* empty environment */
	char *const envp[] = { NULL };

	const char *kexec_load_argv[] = { NULL, "-l", NULL, NULL, NULL };
	const char *kexec_exec_argv[] = { NULL, "-e", NULL, NULL};

	char extra_cmdline_buffer[COMMAND_LINE_SIZE];
	char *extra_cmdline, *cmdline_arg = NULL;
	int n, idx;
	struct stat *sinfo = malloc(sizeof(struct stat));

	kexec_exec_argv[0] = kexec_path;

	/* Check /proc/sys/net presence */
	if ( -1 == stat("/proc/sys/net", sinfo) ) {
		if (ENOENT == errno) {
			/* We have no network, don't issue ifdown() while kexec'ing */
			kexec_exec_argv[2] = "-x";
			DPRINTF("No network is detected, disabling ifdown()\n");
		} else {
			perror("Can't stat /proc/sys/net");
		}
	}
	free(sinfo);

	kexec_load_argv[0] = kexec_path;

	/* --command-line arg generation */
	idx = 2;	/* kexec_load_argv current option index */

	/* get some parts of cmdline from boot loader (e.g. mtdparts) */
	n = get_extra_cmdline( extra_cmdline_buffer,
			sizeof(extra_cmdline_buffer) );
	if ( -1 == n ) {
		/* clean up extra_cmdline on error */
		extra_cmdline = NULL;
/*	} else if ( n < -1 ) { */
		/* Do something when we have no space to get all wanted tags */
		/* Now do nothing ;) */
	} else {
		extra_cmdline = extra_cmdline_buffer;
	}

	/* fill '--command-line' option */
	if (item->cmdline || extra_cmdline || item->device) {
		/* allocate space */
		n = strlenn(str_cmdline_start) + strlenn(item->cmdline) +
				sizeof(char) + strlenn(extra_cmdline) +
				strlenn(str_root) + strlenn(item->device) +
				strlenn(str_rootfstype) + strlenn(item->fstype) +
				strlenn(str_rootwait) + sizeof(char);

		cmdline_arg = (char *)malloc(n);
		if (NULL == cmdline_arg) {
			perror("Can't allocate memory for cmdline_arg");
			/*  Should we exit?
			exit(-1);
			*/
		} else {
			strcpy(cmdline_arg, str_cmdline_start);
			if (extra_cmdline)
				strcat(cmdline_arg, extra_cmdline);
			if (item->cmdline && extra_cmdline)
				strcat(cmdline_arg, " ");
			if (item->cmdline)
				strcat(cmdline_arg, item->cmdline);
			if (item->device) {
				strcat(cmdline_arg, str_root);
				strcat(cmdline_arg, item->device);
				if (item->fstype) {
					strcat(cmdline_arg, str_rootfstype);
					strcat(cmdline_arg, item->fstype);
				}
			}
			strcat(cmdline_arg, str_rootwait);

			kexec_load_argv[idx] = cmdline_arg;
			++idx;
		}
	}

	/* Append kernelpath as last arg of kexec */
	kexec_load_argv[idx] = item->kernelpath;

	DPRINTF("kexec_load_argv: %s, %s, %s, %s\n", kexec_load_argv[0],
			kexec_load_argv[1], kexec_load_argv[2],
			kexec_load_argv[3]);

	DPRINTF("kexec_exec_argv: %s, %s, %s\n", kexec_exec_argv[0],
			kexec_exec_argv[1], kexec_exec_argv[2]);

	/* Mount boot device */
	if ( -1 == mount(item->device, mount_point, item->fstype,
			MS_RDONLY, NULL) ) {
		perror("Can't mount boot device");
		exit(-1);
	}

	/* Load kernel */
	n = fexecw(kexec_path, (char *const *)kexec_load_argv, envp);
	if (-1 == n) {
		perror("Kexec can't load kernel");
		exit(-1);
	}

	if (cmdline_arg)
		free(cmdline_arg);

	umount(mount_point);

	/* Boot new kernel */
	execve(kexec_path, (char *const *)kexec_exec_argv, envp);
}


int main(int argc, char **argv)
{
	int choice = 0;
	int initmode = 0;
	FB *fb;
	FILE *f;
	int angle = KXB_FBANGLE;
	char *eventif = KXB_EVENTIF;
	struct bootconf_t *bl;
	struct input_event evt;
	struct termios old, new;
	struct xpm_parsed_t **icons;
	int nicons = 0;

	/* When our pid is 1 we are init-process */
	if ( 1 == getpid() ) {
		initmode = 1;

		DPRINTF("I'm the init-process!\n");

		/* extra delay for initializing slow SD/CF */
		sleep(1);

		/* Mount procfs */
		if ( -1 == mount("proc", "/proc", "proc",
				0, NULL) ) {
			perror("Can't mount procfs");
			exit(-1);
		}

		DPRINTF("Procfs mounted\n");

		/* Set up console loglevel */
		f = fopen("/proc/sys/kernel/printk", "w");
		if (NULL == f) {
			perror("/proc/sys/kernel/printk");
			exit(-1);
		}
		fputs("0 4 1 7\n", f);
		fclose(f);

		DPRINTF("Console loglevel is set\n");

	}

/* FIXME: Read fbcon tag from cmdline and fill angle
4. fbcon=rotate:<n>

This option changes the orientation angle of the console display. The
value 'n' accepts the following:

0 - normal orientation (0 degree)
1 - clockwise orientation (90 degrees)
2 - upside down orientation (180 degrees)
3 - counterclockwise orientation (270 degrees)
*/

	/* Check command-line args when not an init-process */
	if (!initmode) {
		int i = 0;
		while (++i < argc) {
			if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--angle")) {
				if (++i > argc)
					goto fail;
				angle = atoi(argv[i]);
				continue;
			}

			if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--input")) {
				if (++i > argc)
					goto fail;
				eventif = argv[i];
				continue;
			}

			fail:
			fprintf(stderr,	"Usage: %s [-a|--angle <0|90|180|270>] \
				[-i|--input </dev/input/eventX>\n",
				argv[0]);
			exit(-1);
		}
	}

	DPRINTF("FB angle is %d, input device is %s\n", angle, eventif);
	DPRINTF("Going to fb mode\n");

	if ((fb = fb_new(angle)) == NULL)
		exit(-1);

	f = fopen(eventif,"r");
	if(!f){
	    perror(eventif);
	    exit(3);
	}

	/* Parse compiled images.
	 * We don't care about result because drawing code is aware
	 * FIXME: it should be done for GUI only */
	icon_logo	= xpm_parse_image(logo_xpm, XPM_ROWS(logo_xpm), fb->bpp);
	icon_cf		= xpm_parse_image(cf_xpm, XPM_ROWS(cf_xpm), fb->bpp);
	icon_mmc	= xpm_parse_image(mmc_xpm, XPM_ROWS(mmc_xpm), fb->bpp);
	icon_memory	= xpm_parse_image(memory_xpm, XPM_ROWS(memory_xpm), fb->bpp);

	machine_kernel = get_machine_kernelpath();

	bl = scan_devices();

	/* FIXME: sort_bootlist(bl, 0, bl->size-1); */

	nicons = associate_icons(bl, fb->bpp, &icons);
	if (-1 == nicons) {
		DPRINTF("Can't associate icons\n");
	}

	/* Switch cursor off. FIXME: works only when master-console is tty */
	//printf("\033[?25l\n");

	// deactivate terminal input
	tcgetattr(fileno(stdin), &old);
	new = old;
	new.c_lflag &= ~ECHO;
//	new.c_cflag &=~CREAD;
	tcsetattr(fileno(stdin), TCSANOW, &new);

	do {
		display_menu(fb, bl, choice, icons);
		do
			fread(&evt, sizeof(struct input_event), 1, f);
		while(evt.type != EV_KEY || evt.value != 0);

		switch (evt.code) {
		case KEY_UP:
			if (choice > 0) choice--;
			break;
		case KEY_DOWN:
			if ( choice < (bl->fill - 1) ) choice++;
			break;
		case KEY_R:
			display_text(fb, "Rebooting...");
			sync();
			/* if ( -1 == reboot(LINUX_REBOOT_CMD_RESTART) ) { */
			if ( -1 == reboot(RB_AUTOBOOT) ) {
				perror("Can't initiate reboot");
			}
			break;
		case KEY_S:	/* reScan */
			display_text(fb, "Rescanning devices.\nPlease wait...");
			free_bootcfg(bl);
			free_associated_icons(icons, nicons);

			bl = scan_devices();
			/* FIXME sort_bootlist(bl, 0, bl->size-1); */
			nicons = associate_icons(bl, fb->bpp, &icons);
			break;
		}

	} while( (bl->fill == 0) || (evt.code != 87 && evt.code != 63 &&
		evt.code != KEY_SPACE && evt.code != KEY_ENTER &&
		evt.code != KEY_HIRAGANA && evt.code != KEY_HENKAN) );
	fclose(f);
	// reset terminal
	tcsetattr(fileno(stdin), TCSANOW, &old);

	fb_destroy(fb);
	free_associated_icons(icons, nicons);
	xpm_destroy_parsed(icon_logo);

	start_kernel(bl->list[choice]);
	/* When we reach this point then some error has occured */
	DPRINTF("We should not reach this point!");
	return -1;
}
