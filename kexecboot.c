/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2008-2011 Yuri Bushmelev <jay4mail@gmail.com>
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
#include "evdevs.h"
#include "menu.h"
#include "kexecboot.h"

#ifdef USE_FBMENU
#include "gui.h"
#endif

#ifdef USE_TEXTUI
#include "tui.h"
#endif

#ifdef USE_ZAURUS
#include "machine/zaurus.h"
#endif

/* Don't re-create devices when executing on host */
#ifdef USE_HOST_DEBUG
#undef USE_DEVICES_RECREATING
#endif

#ifdef USE_MACHINE_KERNEL
/* Machine-dependent kernel path */
char *machine_kernel = NULL;
#endif

#define PREPEND_MOUNTPATH(string) MOUNTPOINT""string

/* NULL-terminated array of kernel search paths
 * First item should be filled with machine-dependent path */
char *default_kernels[] = {
#ifdef USE_ZIMAGE
	PREPEND_MOUNTPATH("/boot/zImage"),
	PREPEND_MOUNTPATH("/zImage"),
#endif
#ifdef USE_UIMAGE
	PREPEND_MOUNTPATH("/boot/uImage"),
	PREPEND_MOUNTPATH("/uImage"),
#endif
	NULL
};

/* Init mode flag */
int initmode = 0;

/* Contexts available - menu and textview */
typedef enum {
	KX_CTX_MENU,
	KX_CTX_TEXTVIEW,
} kx_context;

/* Common parameters */
struct params_t {
	struct cfgdata_t *cfg;
	struct bootconf_t *bootcfg;
	kx_menu *menu;
	kx_context context;
#ifdef USE_FBMENU
	struct gui_t *gui;
#endif
#ifdef USE_TEXTUI
	kx_tui *tui;
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
			log_msg(lg, "Can't find ':' in 'Hardware' line");
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

		/* Prepend  MOUNTPOINT"/boot/zImage-" to hw */
		tmp = malloc(strlen(PREPEND_MOUNTPATH("/boot/zImage-")) + strlen(hw) + 1);
		if (NULL == tmp) {
			DPRINTF("Can't allocate memory for machine-specific kernel path");
			return NULL;
		}

		strcpy(tmp, PREPEND_MOUNTPATH("/boot/zImage-"));
		strcat(tmp, hw);

		return tmp;
	}

	log_msg(lg, "Can't find 'Hardware' line in cpuinfo");
	return NULL;
}
#endif	/* USE_MACHINE_KERNEL */


void start_kernel(struct params_t *params, int choice)
{
	/* we use var[] instead of *var because sizeof(var) using */
#ifdef USE_HOST_DEBUG
	const char kexec_path[] = "/bin/echo";
#else
	const char kexec_path[] = KEXEC_PATH;
#endif
	const char mount_point[] = MOUNTPOINT;

	const char str_cmdline_start[] = "--command-line=root=";
	const char str_rootfstype[] = " rootfstype=";
	const char str_rootwait[] = " rootwait";
	const char str_ubirootdev[] = "ubi0";
	const char str_ubimtd[] = " ubi.mtd="; /* max ' ubi.mtd=15' len 11 +1 = 12 */

#ifdef UBI_VID_HDR_OFFSET
	const char str_ubimtd_off[] = UBI_VID_HDR_OFFSET;
#else
	const char str_ubimtd_off[] = "";
#endif

	char mount_dev[16];
	char mount_fstype[16];
	char str_mtd_id[3];

	/* Tags passed from host kernel cmdline to kexec'ed kernel */
	const char str_mtdparts[] = " mtdparts=";
	const char str_fbcon[] = " fbcon=";

	const char str_initrd_start[] = "--initrd=";

	/* empty environment */
	char *const envp[] = { NULL };

	const char *load_argv[] = { NULL, "-l", NULL, NULL, NULL, NULL };
	const char *exec_argv[] = { NULL, "-e", NULL, NULL};

	char *cmdline_arg = NULL, *initrd_arg = NULL;
	int n, idx, u;
	struct stat sinfo;
	struct boot_item_t *item;

	item = params->bootcfg->list[choice];

	exec_argv[0] = kexec_path;
	load_argv[0] = kexec_path;

	/* --command-line arg generation */
	idx = 2;	/* load_argv current option index */

	/* fill '--command-line' option */
	if (item->device) {
		/* default device to mount */
		strcpy(mount_dev, item->device);

		/* allocate space */
		n = sizeof(str_cmdline_start) + strlen(item->device) +
				sizeof(str_ubirootdev) + 2 +
				sizeof(str_ubimtd) + 2 + sizeof(str_ubimtd_off) + 1 +
				sizeof(str_rootwait) +
				sizeof(str_rootfstype) + strlen(item->fstype) + 2 +
				sizeof(str_mtdparts) + strlenn(params->cfg->mtdparts) +
				sizeof(str_fbcon) + strlenn(params->cfg->fbcon) +
				sizeof(char) + strlenn(item->cmdline);

		cmdline_arg = (char *)malloc(n);
		if (NULL == cmdline_arg) {
			perror("Can't allocate memory for cmdline_arg");
		} else {

			strcpy(cmdline_arg, str_cmdline_start);	/* --command-line=root= */

			if (item->fstype) {

				/* default fstype to mount */
				strcpy(mount_fstype, item->fstype);

				/* extra tags when we detect UBI */
				if (!strncmp(item->fstype,"ubi",3)) {

					/* mtd id [0-15] - one or two digits */
					if(isdigit(atoi(item->device+strlen(item->device)-2))) {
						strcpy(str_mtd_id, item->device+strlen(item->device)-2);
						strcat(str_mtd_id, item->device+strlen(item->device)-1);
					} else {
						strcpy(str_mtd_id, item->device+strlen(item->device)-1);
					}
					/* get corresponding ubi dev to mount */
					u = find_attached_ubi_device(str_mtd_id);

					sprintf(mount_dev, "/dev/ubi%d", u);
					 /* FIXME: first volume is hardcoded */
					strcat(mount_dev, "_0");

					/* HARDCODED: we assume it's ubifs */
					strcpy(mount_fstype,"ubifs");

					/* extra cmdline tags when we detect ubi */
					strcat(cmdline_arg, str_ubirootdev);
					 /* FIXME: first volume is hardcoded */
					strcat(cmdline_arg, "_0");

					strcat(cmdline_arg, str_ubimtd);
					strcat(cmdline_arg, str_mtd_id);
#ifdef UBI_VID_HDR_OFFSET
					strcat(cmdline_arg, ",");
					strcat(cmdline_arg, str_ubimtd_off);
#endif
				} else {
					strcat(cmdline_arg, item->device); /* root=item->device */
				}
				strcat(cmdline_arg, str_rootfstype);
				strcat(cmdline_arg, mount_fstype);
			}
			strcat(cmdline_arg, str_rootwait);

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

	DPRINTF("load_argv: %s, %s, %s, %s, %s", load_argv[0],
			load_argv[1], load_argv[2],
			load_argv[3], load_argv[4]);

	/* Mount boot device */
	if ( -1 == mount(mount_dev, mount_point, mount_fstype,
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
			DPRINTF("No network is detected, disabling ifdown()");
		} else {
			perror("Can't stat /proc/sys/net");
		}
	}

	DPRINTF("exec_argv: %s, %s, %s", exec_argv[0],
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
	int rc,n;
	FILE *f;

	char mount_dev[16];
	char mount_fstype[16];
	char str_mtd_id[3];

#ifdef USE_ICONS
	kx_cfg_section *sc;
	int i;
	int rows;
	char **xpm_data;

#endif

	bootconf = create_bootcfg(4);
	if (NULL == bootconf) {
		DPRINTF("Can't allocate bootconf structure");
		return -1;
	}

	f = devscan_open(&fl);
	if (NULL == f) {
		log_msg(lg, "Can't initiate device scan");
		return -1;
	}

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

		/* initialize with defaults */
		strcpy(mount_dev, dev.device);
		strcpy(mount_fstype, dev.fstype);

		/* We found an ubi erase counter */
		if (!strncmp(dev.fstype, "ubi",3)) {

			/* attach ubi boot device - mtd id [0-15] */
			if(isdigit(atoi(dev.device+strlen(dev.device)-2))) {
				strcpy(str_mtd_id, dev.device+strlen(dev.device)-2);
				strcat(str_mtd_id, dev.device+strlen(dev.device)-1);
			} else {
				strcpy(str_mtd_id, dev.device+strlen(dev.device)-1);
			}
			n = ubi_attach(str_mtd_id);

			/* we have attached ubiX and we mount /dev/ubiX_0  */
			sprintf(mount_dev, "/dev/ubi%d", n);
			 /* HARDCODED: first volume */
			strcat(mount_dev, "_0");

			/* HARDCODED: we assume it's ubifs */
			strcpy(mount_fstype, "ubifs");
		}

		/* Mount device */
		if (-1 == mount(mount_dev, MOUNTPOINT, mount_fstype, MS_RDONLY, NULL)) {
			log_msg(lg, "+ can't mount device %s: %s", mount_dev, ERRMSG);
			goto free_device;
		}

		/* NOTE: Don't go out before umount'ing */

		/* Search boot method and return boot info */
		rc = get_bootinfo(&cfgdata);

		if (-1 == rc) {	/* Error */
			goto umount;
		}

#ifdef USE_ICONS
		/* Iterate over sections found */
		if (params->gui) {
			for (i = 0; i < cfgdata.count; i++) {
				sc = cfgdata.list[i];
				if (!sc) continue;

				/* Load custom icon */
				if (sc->iconpath) {
					rows = xpm_load_image(&xpm_data, sc->iconpath);
					if (-1 == rows) {
						log_msg(lg, "+ can't load xpm icon %s", sc->iconpath);
						continue;
					}

					sc->icondata = xpm_parse_image(xpm_data, rows);
					if (!sc->icondata) {
						log_msg(lg, "+ can't parse xpm icon %s", sc->iconpath);
						continue;
					}
					xpm_destroy_image(xpm_data, rows);
				}
			}
		}
#endif

umount:
		/* Umount device */
		if (-1 == umount(MOUNTPOINT)) {
			log_msg(lg, "+ can't umount device: %s", ERRMSG);
			goto free_cfgdata;
		}

		if (-1 == rc) {	/* Error */
			goto free_cfgdata;
		}

#ifdef USE_ZAURUS
		/* Fix partition sizes. We can have kernel in root and home partitions on NAND */
		/* HACK: mtdblock devices are hardcoded */
		if (0 == zaurus_error) {
			if (0 == strcmp(dev.device, "/dev/mtdblock2")) {	/* root */
				log_msg(lg, "+ [zaurus root] size of %s will be changed from %llu to %lu",
						dev.device, dev.blocks, pinfo.root);
				dev.blocks = pinfo.root;
			} else if (0 == strcmp(dev.device, "/dev/mtdblock3")) {	/* home */
				log_msg(lg, "+ [zaurus home] size of %s will be changed from %llu to %lu",
						dev.device, dev.blocks, pinfo.home);
				dev.blocks = pinfo.home;
			}
		}
#endif

		/* Now we have something in cfgdata */
		rc = addto_bootcfg(bootconf, &dev, &cfgdata);

free_cfgdata:
		destroy_cfgdata(&cfgdata);
free_device:
		free(dev.device);
	}

	free_charlist(fl);
	params->bootcfg = bootconf;
	return 0;
}


/* Create system menu */
kx_menu *build_menu(struct params_t *params)
{
	kx_menu *menu;
	kx_menu_level *ml;
	kx_menu_item *mi;
	
#ifdef USE_ICONS
	kx_picture **icons;
	
	if (params->gui) icons = params->gui->icons;
	else icons = NULL;
#endif
	
	/* Create menu with 2 levels (main and system) */
	menu = menu_create(2);
	if (!menu) {
		DPRINTF("Can't create menu");
		return NULL;
	}
	
	/* Create main menu level */
	menu->top = menu_level_create(menu, 4, NULL);
	
	/* Create system menu level */
	ml = menu_level_create(menu, 6, menu->top);
	if (!ml) {
		DPRINTF("Can't create system menu");
		return menu;
	}

	mi = menu_item_add(menu->top, A_SUBMENU, "System menu", NULL, ml);
#ifdef USE_ICONS
	if (icons) menu_item_set_data(mi, icons[ICON_SYSTEM]);
#endif

	mi = menu_item_add(ml, A_PARENTMENU, "Back", NULL, NULL);
#ifdef USE_ICONS
	if (icons) menu_item_set_data(mi, icons[ICON_BACK]);
#endif

	mi = menu_item_add(ml, A_RESCAN, "Rescan", NULL, NULL);
#ifdef USE_ICONS
	if (icons) menu_item_set_data(mi, icons[ICON_RESCAN]);
#endif

	mi = menu_item_add(ml, A_DEBUG, "Show debug info", NULL, NULL);
#ifdef USE_ICONS
	if (icons) menu_item_set_data(mi, icons[ICON_DEBUG]);
#endif

	mi = menu_item_add(ml, A_REBOOT, "Reboot", NULL, NULL);
#ifdef USE_ICONS
	if (icons) menu_item_set_data(mi, icons[ICON_REBOOT]);
#endif

	mi = menu_item_add(ml, A_SHUTDOWN, "Shutdown", NULL, NULL);
#ifdef USE_ICONS
	if (icons) menu_item_set_data(mi, icons[ICON_SHUTDOWN]);
#endif

	if (!initmode) {
		mi = menu_item_add(ml, A_EXIT, "Exit", NULL, NULL);
#ifdef USE_ICONS
		if (icons) menu_item_set_data(mi, icons[ICON_EXIT]);
#endif
	}

	menu->current = menu->top;
	menu_item_select(menu, 0);
	return menu;
}


/* Fill main menu with boot items */
int fill_menu(struct params_t *params)
{
	kx_menu_item *mi;
	int i, b_items, max_pri, max_i, *a;
	struct boot_item_t *tbi;
	struct bootconf_t *bl;
	const int sizeof_desc = 160;
	char *desc, *label;
#ifdef USE_ICONS
	kx_picture *icon;
	struct gui_t *gui;

	gui = params->gui;
#endif

	bl = params->bootcfg;

	if ( (NULL != bl) && (bl->fill > 0) ) b_items = bl->fill;
	else {
		log_msg(lg, "No items for menu found");
		return 0;
	}

	log_msg(lg, "Populating menu: %d item(s)", b_items);

	desc = malloc(sizeof_desc);
	if (NULL == desc) {
		DPRINTF("Can't allocate item description");
		goto dirty_exit;
	}

	a = malloc(b_items * sizeof(*a));	/* Markers array */
	if (NULL == a) {
		DPRINTF("Can't allocate markers array");
		goto dirty_exit;
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
				if (tbi->priority > max_pri) {
					max_pri = tbi->priority;	/* Max priority */
					max_i = i;					/* Max priority item index */
				}
			}
		}

		if (max_pri >= 0) {
			a[max_i] = 1;	/* Mark item as processed */
			/* We have found new max priority - insert into menu */
			tbi = bl->list[max_i];
			snprintf(desc, sizeof_desc, "%s %s %lluMb",
					tbi->device, tbi->fstype, tbi->blocks/1024);

			if (tbi->label)
				label = tbi->label;
			else
				label = tbi->kernelpath + sizeof(MOUNTPOINT) - 1;

			log_msg(lg, "+ [%s]", label);
			mi = menu_item_add(params->menu->top, A_DEVICES + max_i,
					label, desc, NULL);

#ifdef USE_ICONS
			if (gui) {
				/* Search associated with boot item icon if any */
				icon = tbi->icondata;
				if (!icon && (gui->icons)) {
					/* We have no custom icon - use default */
					switch (tbi->dtype) {
					case DVT_STORAGE:
						icon = gui->icons[ICON_STORAGE];
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

				/* Add icon to menu */
				if (mi) mi->data = icon;
			}
#endif
		}

		if (-1 == max_pri) break;	/* We have no items to process */
	}

	free(a);
	free(desc);
	return 0;

dirty_exit:
	dispose(desc);
	return -1;
}


/* Return 0 if we are ordinary app or 1 if we are init */
int do_init(void)
{
	/* When our pid is 1 we are init-process */
	if ( 1 != getpid() ) {
		return 0;
	}

	log_msg(lg, "I'm the init-process!");

#ifdef USE_DEVTMPFS
	if (-1 == mount("devtmpfs", "/dev", "devtmpfs",
			0, NULL) ) {
		perror("Can't mount devtmpfs");
	}
#endif

	/* Mount procfs */
	if ( -1 == mount("proc", "/proc", "proc",
			0, NULL) ) {
		perror("Can't mount procfs");
		exit(-1);
	}

	/* Mount sysfs */
	if ( -1 == mount("sysfs", "/sys", "sysfs",
			0, NULL) ) {
		perror("Can't mount sysfs");
		exit(-1);
	}

	FILE *f;
	/* Set up console loglevel */
	f = fopen("/proc/sys/kernel/printk", "w");
	if (NULL == f) {
		/* CONFIG_PRINTK may be disabled */
		log_msg(lg, "/proc/sys/kernel/printk", ERRMSG);
	} else {
		fputs("0 4 1 7\n", f);
		fclose(f);
	}

	return 1;
}


int do_rescan(struct params_t *params)
{
	int i;

	/* Clean top menu level except system menu item */
	/* FIXME should be done by some function from menu module */
	kx_menu_item *mi;
	for (i = 1; i < params->menu->top->count; i++) {
		mi = params->menu->top->list[i];
		if (mi) {
			dispose(mi->label);
			dispose(mi->description);
			free(mi);
		}
		params->menu->top->list[i] = NULL;
	}
	params->menu->top->count = 1;

#ifdef USE_ICONS
	/* Destroy icons */
	/* FIXME should be done by some function from devicescan module */
	for (i = 0; i < params->bootcfg->fill; i++) {
		fb_destroy_picture(params->bootcfg->list[i]->icondata);
	}
#endif

	free_bootcfg(params->bootcfg);
	params->bootcfg = NULL;
	scan_devices(params);

	return fill_menu(params);
}


/* Process menu context 
 * Return 0 to select, <0 to raise error, >0 to continue
 */
int process_ctx_menu(struct params_t *params, int action) {
	static int rc;
	static int menu_action;
	static kx_menu *menu;
	menu = params->menu;

#ifdef USE_NUMKEYS
	/* Some hacks to allow menu items selection by keys 0-9 */
	if ((action >= A_KEY0) && (action <= A_KEY9)) {
		rc = action - A_KEY0;
		if (-1 == menu_item_select_by_no(menu, rc)) {
			/* There is no item with such number - do nothing */
			return 1;
		} else {
			action = A_SELECT;
		}
	}
#endif

	menu_action = (A_SELECT == action ? menu->current->current->id : action);
	rc = 1;

	switch (menu_action) {
	case A_UP:
		menu_item_select(menu, -1);
		break;
	case A_DOWN:
		menu_item_select(menu, 1);
		break;
	case A_SUBMENU:
		menu->current = menu->current->current->submenu;
		break;
	case A_PARENTMENU:
		menu->current = menu->current->parent;
		break;

	case A_REBOOT:
#ifdef USE_FBMENU
		gui_show_msg(params->gui, "Rebooting...");
#endif
#ifdef USE_TEXTUI
		tui_show_msg(params->tui, "Rebooting...");
#endif
#ifdef USE_HOST_DEBUG
		sleep(1);
#else
		sync();
		/* if ( -1 == reboot(LINUX_REBOOT_CMD_RESTART) ) { */
		if ( -1 == reboot(RB_AUTOBOOT) ) {
			log_msg(lg, "Can't initiate reboot: %s", ERRMSG);
		}
#endif
		break;
	case A_SHUTDOWN:
#ifdef USE_FBMENU
		gui_show_msg(params->gui, "Shutting down...");
#endif
#ifdef USE_TEXTUI
		tui_show_msg(params->tui, "Shutting down...");
#endif
#ifdef USE_HOST_DEBUG
		sleep(1);
#else
		sync();
		/* if ( -1 == reboot(LINUX_REBOOT_CMD_POWER_OFF) ) { */
		if ( -1 == reboot(RB_POWER_OFF) ) {
			log_msg(lg, "Can't initiate shutdown: %s", ERRMSG);
		}
#endif
		break;

	case A_RESCAN:
#ifdef USE_FBMENU
		gui_show_msg(params->gui, "Rescanning devices.\nPlease wait...");
#endif
#ifdef USE_TEXTUI
		tui_show_msg(params->tui, "Rescanning devices.\nPlease wait...");
#endif
		if (-1 == do_rescan(params)) {
			log_msg(lg, "Rescan failed");
			return -1;
		}
		menu = params->menu;
		break;

	case A_DEBUG:
		params->context = KX_CTX_TEXTVIEW;
		break;

	case A_EXIT:
		if (initmode) break;	// don't exit if we are init
	case A_ERROR:
		rc = -1;
		break;

#ifdef USE_TIMEOUT
	case A_TIMEOUT:		// timeout was reached - boot 1st kernel if exists
		if (menu->current->count > 1) {
			menu_item_select(menu, 0);	/* choose first item */
			menu_item_select(menu, 1);	/* and switch to next item */
			rc = 0;
		}
		break;
#endif

	default:
		if (menu_action >= A_DEVICES) rc = 0;
		break;
	}

	return rc;
}

/* Draw menu context */
void draw_ctx_menu(struct params_t *params)
{
#ifdef USE_FBMENU
	gui_show_menu(params->gui, params->menu);
#endif
#ifdef USE_TEXTUI
	tui_show_menu(params->tui, params->menu);
#endif
}


/* Process text view context
 * Return 0 to select, <0 to raise error, >0 to continue
 */
int process_ctx_textview(struct params_t *params, int action) {
	static int rc;

	rc = 1;
	switch (action) {
	case A_UP:
		if (lg->current_line_no > 0) --lg->current_line_no;
		break;
	case A_DOWN:
		if (lg->current_line_no + 1 < lg->rows->fill) ++lg->current_line_no;
		break;
	case A_SELECT:
		/* Rewind log view to top. This should make log view usable
		 * on devices with 2 buttons only (DOWN and SELECT)
		 */
		lg->current_line_no = 0;
		params->context = KX_CTX_MENU;
		break;
	case A_EXIT:
		if (initmode) break;	// don't exit if we are init
	case A_ERROR:
		rc = -1;
		break;
	}
	return rc;
}

/* Draw text view context */
void draw_ctx_textview(struct params_t *params)
{
#ifdef USE_FBMENU
	gui_show_text(params->gui, lg);
#endif
#ifdef USE_TEXTUI
	tui_show_text(params->tui, lg);
#endif
}


/* Main event loop */
int do_main_loop(struct params_t *params, kx_inputs *inputs)
{
	int rc = 0;
	int action;

	/* Start with menu context */
	params->context = KX_CTX_MENU;
	draw_ctx_menu(params);

	/* Event loop */
	do {
		/* Read events */
		action = inputs_process(inputs);
		if (action != A_NONE) {

			/* Process events in current context */
			switch (params->context) {
			case KX_CTX_MENU:
				rc = process_ctx_menu(params, action);
				break;
			case KX_CTX_TEXTVIEW:
				rc = process_ctx_textview(params, action);
			}

			/* Draw current context */
			if (rc > 0) {
				switch (params->context) {
				case KX_CTX_MENU:
					draw_ctx_menu(params);
					break;
				case KX_CTX_TEXTVIEW:
					draw_ctx_textview(params);
					break;
				}
			}
		}
		else
			rc = 1;

	/* rc: 0 - select, <0 - raise error, >0 - continue */
	} while (rc > 0);

	/* If item is selected then return his id */
	if (0 == rc) rc = params->menu->current->current->id;

	return rc;
}


int main(int argc, char **argv)
{
	int rc = 0;
	struct cfgdata_t cfg;
	struct params_t params;
	kx_inputs inputs;

	lg = log_open(16);
	log_msg(lg, "%s starting", PACKAGE_STRING);

	initmode = do_init();

	/* Get cmdline parameters */
	params.cfg = &cfg;
	init_cfgdata(&cfg);
	cfg.angle = 0;	/* No rotation by default */
	parse_cmdline(&cfg);

	kxb_ttydev = cfg.ttydev;
	setup_terminal(kxb_ttydev, &kxb_echo_state, 1);
	/* Setup function that will restore terminal when exit() will called */
	atexit(atexit_restore_terminal);

	log_msg(lg, "FB angle is %d, tty is %s", cfg.angle, cfg.ttydev);

#ifdef USE_MACHINE_KERNEL
	machine_kernel = get_machine_kernelpath();	/* FIXME should be passed as arg to get_bootinfo() */
#endif

#ifdef USE_DELAY
	/* extra delay for initializing slow SD/CF */
	sleep(USE_DELAY);
#endif

	int no_ui = 1;	/* UI presence flag */
#ifdef USE_FBMENU
	params.gui = NULL;
	if (no_ui) {
		params.gui = gui_init(cfg.angle);
		if (NULL == params.gui) {
			log_msg(lg, "Can't initialize GUI");
		} else no_ui = 0;
	}
#endif
#ifdef USE_TEXTUI
	FILE *ttyfp;
	params.tui = NULL;
	if (no_ui) {

		if (cfg.ttydev) ttyfp = fopen(cfg.ttydev, "w");
		else ttyfp = stdout;

		params.tui = tui_init(ttyfp);
		if (NULL == params.tui) {
			log_msg(lg, "Can't initialize TUI");
			if (ttyfp != stdout) fclose(ttyfp);
		} else no_ui = 0;
	}
#endif
	if (no_ui) exit(-1); /* Exit if no one UI was initialized */
	
	params.menu = build_menu(&params);
	params.bootcfg = NULL;
	scan_devices(&params);

	if (-1 == fill_menu(&params)) {
		exit(-1);
	}

	/* Collect input devices */
	inputs_init(&inputs, 8);
	inputs_open(&inputs);
	inputs_preprocess(&inputs);

	/* Run main event loop
	 * Return values: <0 - error, >=0 - selected item id */
	rc = do_main_loop(&params, &inputs);

#ifdef USE_FBMENU
	if (params.gui) {
		if (rc < 0) gui_clear(params.gui);
		gui_destroy(params.gui);
	}
#endif
#ifdef USE_TEXTUI
	if (params.tui) {
		tui_destroy(params.tui);
		if (ttyfp != stdout) fclose(ttyfp);
	}
#endif
	inputs_close(&inputs);
	inputs_clean(&inputs);

	log_close(lg);
	lg = NULL;

	/* rc < 0 indicate error */
	if (rc < 0) exit(rc);

	menu_destroy(params.menu, 0);

	if (rc >= A_DEVICES) {
		start_kernel(&params, rc - A_DEVICES);
	}

	/* When we reach this point then some error has occured */
	DPRINTF("We should not reach this point!");
	exit(-1);
}
