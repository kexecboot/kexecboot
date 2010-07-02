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

/*
 * TODO Show kexecboot version somewhere
 * TODO Create WARN/ERR/INFO functions in addition to DPRINTF
 * TODO Show debug info in special dialog
 * TODO Cleanup debug info
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>
#include <unistd.h>
#include <sys/mount.h>
#include <ctype.h>
#include <errno.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "util.h"
#include "cfgparser.h"
#include "devicescan.h"
#include "menu.h"
#include "kexecboot.h"

#ifdef USE_FBMENU
#include "gui.h"
#endif

#ifdef USE_ZAURUS
#include "machine/zaurus.h"
#endif

/* Don't re-create devices when executing on host */
#ifdef USE_HOST_DEBUG
#undef USE_DEVICES_RECREATING
#endif

#ifdef USE_MACHINE_KERNEL
/* Machine-dependent kernel patch */
char *machine_kernel = NULL;
#endif

/* NULL-terminated array of kernel search paths
 * First item should be filled with machine-dependent path */
char *default_kernels[] = {
#ifdef USE_ZIMAGE
	"/mnt/boot/zImage",
	"/mnt/zImage",
#endif
#ifdef USE_UIMAGE
	"/mnt/boot/uImage",
	"/mnt/uImage",
#endif
	NULL
};

/* Init mode flag */
int initmode = 0;

/* Menu/keyboard/TS actions */
enum actions_t {
	A_ERROR = -1,
	A_EXIT = 0,
	A_NONE,
	A_UP,
	A_DOWN,
	A_MAINMENU,
	A_SYSMENU,
	A_REBOOT,
	A_RESCAN,
	A_DEBUG,
	A_SELECT,
	A_DEVICES
};

/* Common parameters */
struct params_t {
	struct cfgdata_t *cfg;
	struct bootconf_t *bootcfg;
	struct menu_t *menu;
#ifdef USE_FBMENU
	struct gui_t *gui;
#endif
};

static char *kxb_ttydev = NULL;
static int kxb_echo_state = 0;

static void atexit_restore_terminal(void)
{
	setup_terminal(kxb_ttydev, &kxb_echo_state, 0);
}

#ifdef USE_MACHINE_KERNEL
/* Return lowercased and stripped machine-specific kernel path */
/* Return value should be free()'d */
char *get_machine_kernelpath() {
	FILE *f;
	char *tmp, *hw = NULL, c;
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
#endif	/* USE_MACHINE_KERNEL */


void start_kernel(struct params_t *params, int choice)
{
	/* we use var[] instead of *var because sizeof(var) using */
#ifdef USE_HOST_DEBUG
	const char kexec_path[] = "/bin/echo";
#else
	const char kexec_path[] = "/usr/sbin/kexec";
#endif
	const char mount_point[] = MOUNTPOINT;

	const char str_cmdline_start[] = "--command-line=root=";
	const char str_rootfstype[] = " rootfstype=";
	const char str_rootwait[] = " rootwait";

	/* Tags passed from host kernel cmdline to kexec'ed kernel */
	const char str_mtdparts[] = " mtdparts=";
	const char str_fbcon[] = " fbcon=";

	const char str_initrd_start[] = "--initrd=";

	/* empty environment */
	char *const envp[] = { NULL };

	const char *load_argv[] = { NULL, "-l", NULL, NULL, NULL, NULL };
	const char *exec_argv[] = { NULL, "-e", NULL, NULL};

	char *cmdline_arg = NULL, *initrd_arg = NULL;
	int n, idx;
	struct stat sinfo;
	struct boot_item_t *item;

	item = params->bootcfg->list[choice];

	exec_argv[0] = kexec_path;
	load_argv[0] = kexec_path;

	/* --command-line arg generation */
	idx = 2;	/* load_argv current option index */

	/* fill '--command-line' option */
	if (item->device) {
		/* allocate space */
		n = sizeof(str_cmdline_start) + strlen(item->device) +
				sizeof(str_rootwait) +
				sizeof(str_rootfstype) + strlen(item->fstype) +
				sizeof(str_mtdparts) + strlenn(params->cfg->mtdparts) +
				sizeof(str_fbcon) + strlenn(params->cfg->fbcon) +
				sizeof(char) + strlenn(item->cmdline);

		cmdline_arg = (char *)malloc(n);
		if (NULL == cmdline_arg) {
			perror("Can't allocate memory for cmdline_arg");
		} else {
			strcpy(cmdline_arg, str_cmdline_start);	/* --command-line=root= */
			strcat(cmdline_arg, item->device);
			if (item->fstype) {
				strcat(cmdline_arg, str_rootfstype);	/* rootfstype */
				strcat(cmdline_arg, item->fstype);
			}
			strcat(cmdline_arg, str_rootwait);			/* rootwait */

			if (params->cfg->mtdparts) {
				strcat(cmdline_arg, str_mtdparts);
				strcat(cmdline_arg, params->cfg->mtdparts);
			}

			if (params->cfg->fbcon) {
				strcat(cmdline_arg, str_fbcon);
				strcat(cmdline_arg, params->cfg->fbcon);
			}

			if (item->cmdline) {
				strcat(cmdline_arg, " ");
				strcat(cmdline_arg, item->cmdline);
			}
			load_argv[idx] = cmdline_arg;
			++idx;
		}
	}

	/* fill '--initrd' option */
	if (item->initrd) {
		/* allocate space */
		n = sizeof(str_initrd_start) + strlen(item->initrd);

		initrd_arg = (char *)malloc(n);
		if (NULL == initrd_arg) {
			perror("Can't allocate memory for initrd_arg");
		} else {
			strcpy(initrd_arg, str_initrd_start);	/* --initrd= */
			strcat(initrd_arg, item->initrd);
			load_argv[idx] = initrd_arg;
			++idx;
		}
	}

	/* Append kernelpath as last arg of kexec */
	load_argv[idx] = item->kernelpath;

	DPRINTF("load_argv: %s, %s, %s, %s, %s\n", load_argv[0],
			load_argv[1], load_argv[2],
			load_argv[3], load_argv[4]);

	/* Mount boot device */
	if ( -1 == mount(item->device, mount_point, item->fstype,
			MS_RDONLY, NULL) ) {
		perror("Can't mount boot device");
		exit(-1);
	}

	/* Load kernel */
	n = fexecw(kexec_path, (char *const *)load_argv, envp);
	if (-1 == n) {
		perror("Kexec can't load kernel");
		exit(-1);
	}

	umount(mount_point);

	dispose(cmdline_arg);
	dispose(initrd_arg);

	/* Check /proc/sys/net presence */
	if ( -1 == stat("/proc/sys/net", &sinfo) ) {
		if (ENOENT == errno) {
			/* We have no network, don't issue ifdown() while kexec'ing */
			exec_argv[2] = "-x";
			DPRINTF("No network is detected, disabling ifdown()\n");
		} else {
			perror("Can't stat /proc/sys/net");
		}
	}

	DPRINTF("exec_argv: %s, %s, %s\n", exec_argv[0],
			exec_argv[1], exec_argv[2]);

	/* Boot new kernel */
	execve(kexec_path, (char *const *)exec_argv, envp);
}


int scan_devices(struct params_t *params)
{
	struct charlist *fl;
	struct bootconf_t *bootconf;
	struct device_t dev;
	struct cfgdata_t cfgdata;
	int rc;
	FILE *f;
#ifdef USE_FBMENU
	int rows, bpp;
	char **xpm_data;
	struct xpm_parsed_t *icon = NULL;
	struct xpmlist_t *xl;
#endif

	bootconf = create_bootcfg(4);
	if (NULL == bootconf) {
		DPRINTF("Can't allocate bootconf structure\n");
		return -1;
	}

	f = devscan_open(&fl);
	if (NULL == f) {
		DPRINTF("Can't initiate device scan\n");
		return -1;
	}

#ifdef USE_FBMENU
	xl = create_xpmlist(4);
	if (NULL == xl) {
		DPRINTF("Can't allocate xpm list structure\n");
		/* We will continue but check it later */
	}

	bpp = params->gui->fb->bpp;
#endif

#ifdef USE_ZAURUS
	struct zaurus_partinfo_t pinfo;
	int zaurus_error = 0;
	zaurus_error = zaurus_read_partinfo(&pinfo);
	if (0 == zaurus_error) {
		/* Fix mtdparts tag */
		dispose(params->cfg->mtdparts);
		params->cfg->mtdparts = zaurus_mtdparts(&pinfo);
	}
#endif

	for (;;) {
		rc = devscan_next(f, fl, &dev);
		if (rc < 0) continue;	/* Error */
		if (0 == rc) break;		/* EOF */

		/* Mount device */
		if (-1 == mount(dev.device, MOUNTPOINT, dev.fstype, MS_RDONLY, NULL)) {
			perror("mount");
			goto free_device;
		}

		/* NOTE: Don't go out before umount'ing */

		/* Search boot method and return boot info */
		rc = get_bootinfo(&cfgdata);

		if (-1 == rc) {	/* Error */
			DPRINTF("Can't get device boot information\n");
			goto umount;
		}

#ifdef USE_FBMENU
		icon = NULL;

		/* Skip icons processing if have no xpm list */
		if (NULL == xl) goto umount;

		/* Load custom icon */
		if (NULL != cfgdata.iconpath) {
			rows = xpm_load_image(&xpm_data, cfgdata.iconpath);
			if (-1 == rows) {
				DPRINTF("Can't load xpm icon %s\n", cfgdata.iconpath);
				goto umount;
			}

			icon = xpm_parse_image(xpm_data, rows, bpp);
			if (NULL == icon) {
				DPRINTF("Can't parse xpm icon %s\n", cfgdata.iconpath);
				goto umount;
			}
			xpm_destroy_image(xpm_data, rows);
		}
#endif

umount:
		/* Umount device */
		if (-1 == umount(MOUNTPOINT)) {
			perror("umount");
			goto free_device;
		}

		if (-1 == rc) {	/* Error */
			goto free_device;
		}

		/* Now we have something in cfgdata */
		rc = addto_bootcfg(bootconf, &dev, &cfgdata);
		if (-1 == rc) {
			DPRINTF("Can't import config file data\n");
			goto free_device;
		}

#ifdef USE_FBMENU
		if ((NULL != xl) && (NULL != icon)) {
			/* associate custom icon with bootconf item */
			icon->tag = rc;	/* rc is No. of current bootconf item */
			addto_xpmlist(xl, icon);
			DPRINTF("Added %d icon '%s' to %s\n",rc, cfgdata.iconpath, dev.device);
		}
#endif

#ifdef USE_ZAURUS
		/* Fix partition sizes. We can have kernel in root and home partitions on NAND */
		/* HACK: mtdblock devices are hardcoded */
		if (0 == zaurus_error) {
			if (0 == strcmp(dev.device, "/dev/mtdblock2")) {	/* root */
				DPRINTF("[Zaurus] Size of %s will be changed from %u to %u\n", dev.device, bootconf->list[rc]->blocks, pinfo.root);
				bootconf->list[rc]->blocks = pinfo.root;
			} else if (0 == strcmp(dev.device, "/dev/mtdblock3")) {	/* home */
				DPRINTF("[Zaurus] Size of %s will be changed from %u to %u\n", dev.device, bootconf->list[rc]->blocks, pinfo.home);
				bootconf->list[rc]->blocks = pinfo.home;
			}
		}
#endif

		continue;

free_device:
		free(dev.device);
	}

	free_charlist(fl);
	params->bootcfg = bootconf;
#ifdef USE_FBMENU
	params->gui->loaded_icons = xl;
#endif
	return 0;
}


/* Create system menu */
struct menu_t *build_system_menu()
{
	struct menu_t *sys_menu;
	/* Initialize menu */
	sys_menu = menu_init((0 == initmode ? 5 : 4));
	if (NULL == sys_menu) {
		return NULL;
	}

	menu_add_item(sys_menu, "Back", A_MAINMENU,  NULL);
	if (!initmode) {
		menu_add_item(sys_menu, "Exit", A_EXIT,  NULL);
	}
#ifndef USE_HOST_DEBUG
	menu_add_item(sys_menu, "Reboot", A_REBOOT, NULL);
#endif
	menu_add_item(sys_menu, "Rescan", A_RESCAN, NULL);
	menu_add_item(sys_menu, "Show debug info", A_DEBUG, NULL);

	return sys_menu;
}


/* Build menu. Return pointers to menu and icons list */
/* iconlist initially should point to loaded_icons, menu to sys_menu */
int build_menu(struct params_t *params)
{
	struct menu_t *main_menu, *sys_menu;
	int i, b_items, max_pri, max_i, *a;
	struct boot_item_t *tbi;
	struct bootconf_t *bl;
	const int sizeof_label = 160;
	char *label;
#ifdef USE_FBMENU
	struct xpm_parsed_t *icon;
	struct xpmlist_t *icons;
	struct gui_t *gui;
#endif

	sys_menu = params->menu;
	bl = params->bootcfg;

	if ( (NULL != bl) && (bl->fill > 0) ) b_items = bl->fill;
	else b_items = 0;

	DPRINTF("Found %d items\n", b_items);

	/* Initialize menu */
	main_menu = menu_init(b_items + 1);
	if (NULL == main_menu) {
		DPRINTF("Can't create main menu\n");
		return -1;
	}

	/* Insert system menu */
	menu_add_item(main_menu, "System menu", A_SYSMENU, sys_menu);
#ifdef USE_FBMENU
	icons = create_xpmlist(b_items + 1);
	if (NULL == icons) {
		DPRINTF("Can't allocate memory for icons list\n");
		/* Just continue without icons */
	}

	gui = params->gui;

	addto_xpmlist(icons, gui->icons[ICON_SYSTEM]);
#endif

	label = malloc(sizeof_label);
	if (NULL == label) {
		DPRINTF("Can't allocate place for item label\n");
		goto free_icons;
	}

	a = malloc(b_items * sizeof(*a));	/* Markers array */
	if (NULL == a) {
		DPRINTF("Can't allocate markers array\n");
		goto free_label;
	}

	for (i = 0; i < b_items; i++) a[i] = 0;	/* Clean markers array */

	/* Create menu of sorted by priority boot items */
	max_i = -1;
	for(;;) {
		max_pri = -1;
		/* Search item with maximum priority */
		for (i = 0; i < b_items; i++) {
			if (0 == a[i]) {	/* Check that item is not processed yet */
				tbi = bl->list[i];
				DPRINTF("+ processing %d: %s\n", i, tbi->label);
				if (tbi->priority > max_pri) {
					max_pri = tbi->priority;	/* Max priority */
					max_i = i;					/* Max priority item index */
				}
			}
		}

		if (max_pri >= 0) {
			a[max_i] = 1;	/* Mark item as processed */
			DPRINTF("* maximum priority %d found at %d\n", max_pri, max_i);
			/* We have found new max priority - insert into menu */
			tbi = bl->list[max_i];
			snprintf(label, sizeof_label, "%s\n%s %s %luMb",
					( NULL != tbi->label ? tbi->label : tbi->kernelpath ),
					tbi->device, tbi->fstype, tbi->blocks/1024);
			DPRINTF("+ [%s]\n", label);

			menu_add_item(main_menu, label, A_DEVICES + max_i, NULL);

#ifdef USE_FBMENU
			/* Search associated with boot item icon if any */
			icon = xpm_by_tag(gui->loaded_icons, max_i);
			if (NULL == icon) {
				/* We have no custom icon - use default */
				switch (tbi->dtype) {
				case DVT_HD:
					icon = gui->icons[ICON_HD];
					break;
				case DVT_SD:
					icon = gui->icons[ICON_SD];
					break;
				case DVT_MMC:
					icon = gui->icons[ICON_MMC];
					break;
				case DVT_MTD:
					icon = gui->icons[ICON_MEMORY];
					break;
				case DVT_UNKNOWN:
				default:
					break;
				}
			}
			/* Add icon to list */
			addto_xpmlist(icons, icon);
#endif

		}

		if (-1 == max_pri) break;	/* We have no items to process */
	};

	free(a);

free_label:
	free(label);

	params->menu = main_menu;
#ifdef USE_FBMENU
	gui->menu_icons = icons;
#endif
	return 0;

free_icons:
#ifdef USE_FBMENU
	free_xpmlist(icons, 0);
#endif
	menu_destroy(main_menu);
	return -1;
}


/* Return 0 if we are ordinary app or 1 if we are init */
int do_init(void)
{
	/* When our pid is 1 we are init-process */
	if ( 1 != getpid() ) {
		return 0;
	}

	DPRINTF("I'm the init-process!\n");

	/* extra delay for initializing slow SD/CF */
	sleep(1);

	/* Mount procfs */
	if ( -1 == mount("proc", "/proc", "proc",
			0, NULL) ) {
		perror("Can't mount procfs");
		exit(-1);
	}

	FILE *f;
	/* Set up console loglevel */
	f = fopen("/proc/sys/kernel/printk", "w");
	if (NULL == f) {
		/* CONFIG_PRINTK may be disabled */
		perror("/proc/sys/kernel/printk");
	} else {
		fputs("0 4 1 7\n", f);
		fclose(f);
	}

	return 1;
}


/* Read and process events */
enum actions_t process_events(struct ev_params_t *ev)
{
	fd_set fds;
	int i, e, nready, efd;
	const int evt_size = 4;
	struct input_event evt[evt_size];
	enum actions_t action = A_NONE;

	if (0 == ev->count) return A_ERROR;		/* A_EXIT ? */

	fds = ev->fds;

	/* Wait for some input */
	nready = select(ev->maxfd, &fds, NULL, NULL, NULL);	/* Wait indefinitely */

	if (-1 == nready) {
		if (errno == EINTR) return A_NONE;
		else {
			DPRINTF("Error %d occured in select() call\n", errno);
			return A_ERROR;
		}
	}

	/* Check fds */
	for (i = 0; i < ev->count; i++) {
		efd = ev->fd[i];
		if (FD_ISSET(efd, &fds)) {
			nready = read(efd, evt, sizeof(struct input_event) * evt_size);	/* Read events */
			if ( nready < (int) sizeof(struct input_event) ) {
				DPRINTF("Short read of event structure (%d bytes)\n", nready);
				continue;
			}

			/* NOTE: debug
			if ( nready > (int) sizeof(struct input_event) )
				DPRINTF("Have more than one event here (%d bytes, %d events)\n",
						nready, (int) (nready / sizeof(struct input_event)) );
			*/

			for (e = 0; e < (int) (nready / sizeof(struct input_event)); e++) {

				/* DPRINTF("+ event on %d, type: %d, code: %d, value: %d\n",
						efd, evt[e].type, evt[e].code, evt[e].value); */

				if ((EV_KEY == evt[e].type) && (0 != evt[e].value)) {
					/* EV_KEY event actions */

					switch (evt[e].code) {
					case KEY_UP:
						action = A_UP;
						break;
					case KEY_DOWN:
					case BTN_TOUCH:	/* GTA02: touchscreen touch (330) */
						action = A_DOWN;
						break;
#ifndef USE_HOST_DEBUG
					case KEY_R:
						action = A_REBOOT;
						break;
#endif
					case KEY_S:	/* reScan */
						action = A_RESCAN;
						break;
					case KEY_Q:	/* Quit (when not in initmode) */
						if (0 == initmode) action = A_EXIT;
						break;
					case KEY_ENTER:
					case KEY_SPACE:
					case KEY_HIRAGANA:	/* Zaurus SL-6000 */
					case KEY_HENKAN:	/* Zaurus SL-6000 */
					case 87:			/* Zaurus: OK (remove?) */
					case 63:			/* Zaurus: Enter (remove?) */
					case KEY_POWER:		/* GTA02: Power (116) */
					case KEY_PHONE:		/* GTA02: AUX (169) */
						action = A_SELECT;
						break;
					default:
						action = A_NONE;
						break;
					}
#if 0
				} else if ((EV_ABS == evt.type) && (0 != evt.value)) {
					/* EV_KEY event actions */
					suitable_event = 1;
					switch (evt.code) {
					case ABS_PRESSURE:	/* Touchscreen touch */
						if (choice < (bl->fill - 1)) choice++;
						else choice = 0;
						break;
					default:
						suitable_event = 0;
					}
#endif
				}
			}
		}
	}

	return action;
}


int main(int argc, char **argv)
{
	int choice = 0;
	int is_selected = 0;
	int action = A_MAINMENU;
	struct cfgdata_t cfg;
	struct menu_t *menu, *sys_menu;
#ifdef USE_FBMENU
	struct gui_t *gui;
	struct xpmlist_t *icons;
#endif
	struct params_t params;
	struct ev_params_t ev;

	DPRINTF("%s starting\n", PACKAGE_STRING);

	initmode = do_init();

	/* Get cmdline parameters */
	init_cfgdata(&cfg);
	cfg.angle = KXB_FBANGLE;
	parse_cmdline(&cfg);

	kxb_ttydev = cfg.ttydev;
	setup_terminal(kxb_ttydev, &kxb_echo_state, 1);
	/* Setup function that will restore terminal when exit() will called */
	atexit(atexit_restore_terminal);

	DPRINTF("FB angle is %d, tty is %s\n", cfg.angle, cfg.ttydev);

#ifdef USE_MACHINE_KERNEL
	machine_kernel = get_machine_kernelpath();	/* FIXME should be passed as arg to get_bootinfo() */
#endif

	sys_menu = build_system_menu();

#ifdef USE_FBMENU
	gui = gui_init(cfg.angle);
	if (NULL == gui) {
		DPRINTF("Can't initialize GUI\n");
		exit(-1);	/* FIXME dont exit while other UI exists */
	}
	params.gui = gui;
#endif
	params.bootcfg = NULL;
	params.cfg = &cfg;

	/* In: gui.bpp */
	/* Out: bootcfg, gui.loaded_icons */
	scan_devices(&params);

	params.menu = sys_menu;
	/* In: bootcfg, menu=sys_menu, gui.loaded_icons */
	/* Out: menu, gui.menu_icons */
	if (-1 == build_menu(&params)) {
		exit(-1);
	}
	menu = params.menu;
#ifdef USE_FBMENU
	icons = gui->menu_icons;	/* HACK remember menu icons now */
#endif


	scan_evdevs(&ev);	/* Look for event devices */

	/* Event loop */
	do {
#ifdef USE_FBMENU
		if (action != A_NONE) gui_show_menu(gui, menu, choice);
#endif
		action = process_events(&ev);

		if (A_SELECT == action) action = menu->list[choice]->tag;

		switch (action) {
		case A_NONE:
			break;
		case A_UP:
			if (choice > 0) --choice;
			else choice = menu->fill - 1;
			break;
		case A_DOWN:
			if (choice < (menu->fill - 1)) ++choice;
			else choice = 0;
			break;
		case A_SYSMENU:
			menu = sys_menu;
			gui->menu_icons = NULL;	/* HACK reset menu_icons */
			break;
		case A_MAINMENU:
			menu = params.menu;
			gui->menu_icons = icons;	/* HACK restore menu_icons */
			break;
#ifndef USE_HOST_DEBUG
		case A_REBOOT:
#ifdef USE_FBMENU
			gui_show_text(gui, "Rebooting...");
#endif
			sync();
			/* if ( -1 == reboot(LINUX_REBOOT_CMD_RESTART) ) { */
			if ( -1 == reboot(RB_AUTOBOOT) ) {
				perror("Can't initiate reboot");
			}
			break;
#endif
		case A_RESCAN:
#ifdef USE_FBMENU
			gui_show_text(gui, "Rescanning devices.\nPlease wait...");
			free_xpmlist(icons, 0);		/* Free xpmlist structure only */
			free_xpmlist(gui->loaded_icons, 1);
#endif
			free_bootcfg(params.bootcfg);
			menu_destroy(params.menu);

			params.bootcfg = NULL;
			scan_devices(&params);

			params.menu = sys_menu;
			if (-1 == build_menu(&params)) {
				exit(-1);
			}
			menu = params.menu;
#ifdef USE_FBMENU
			icons = gui->menu_icons;
#endif
			choice = 0;

			break;
		case A_DEBUG:
#ifdef USE_FBMENU
			gui_show_text(gui, "Debug info dialog is not implemented yet...");
#endif
			sleep(1);
			break;
		case A_ERROR:
		case A_EXIT:
			is_selected = 1;
			break;
		default:
			if ( (action >= A_DEVICES) &&
					(NULL != params.bootcfg) && (params.bootcfg->fill > 0)
			) is_selected = 1;
			break;
		}

	} while (!is_selected);

#ifdef USE_FBMENU
	gui_destroy(gui);
#endif
	close_event_devices(ev.fd, ev.count);

	if ( (A_EXIT == action) || (A_ERROR == action) ) {
		exit(action);
	}

	action = menu->list[choice]->tag;	/* action is used as temporary variable */

	menu_destroy(params.menu);

	if (action >= A_DEVICES) {
		start_kernel(&params, action - A_DEVICES);
	}

	/* When we reach this point then some error has occured */
	DPRINTF("We should not reach this point!");
	exit(-1);
}
