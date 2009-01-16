/* 
 *  kexecboot - A kexec based bootloader 
 *
 *      Copyright (c) 2008 Thomas Kunze <thommycheck@gmx.de>
 *  small parts:
 *  	Copyright (c) 2006 Matthew Allum <mallum@o-hand.com>
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

#include "kexecboot.h"

void display_slot(FB *fb, struct boot * boot,int slot, int height, int iscurrent)
{
	int margin = (height - CF_IMG_HEIGHT)/2;
	char text[100];
	if(!iscurrent)
		fb_draw_rect(fb, 0, slot*height, fb->width, height,
			0xec, 0xec, 0xe1);
	else { //draw red border
		fb_draw_rect(fb, 0, slot*height, fb->width, height,
			0xff, 0x00, 0x00);
		fb_draw_rect(fb, margin, slot*height+margin, fb->width-2*margin
			, height-2*margin, 0xec, 0xec, 0xe1);
	}
	if(!strncmp(boot->device,"/dev/hd",strlen("/dev/hd")))
		fb_draw_image(fb, margin, slot*height+margin, CF_IMG_WIDTH, 
			CF_IMG_HEIGHT, CF_IMG_BYTES_PER_PIXEL, CF_IMG_RLE_PIXEL_DATA);
	else if(!strncmp(boot->device,"/dev/mmcblk",strlen("/dev/mmcblk")))
		fb_draw_image(fb, margin, slot*height + margin, MMC_IMG_WIDTH,
			MMC_IMG_HEIGHT, MMC_IMG_BYTES_PER_PIXEL, MMC_IMG_RLE_PIXEL_DATA);
	else if(!strncmp(boot->device,"/dev/mtdblock",strlen("/dev/mtdblock")))
		fb_draw_image(fb, margin, slot*height + margin, MEMORY_IMG_WIDTH,
			MEMORY_IMG_HEIGHT, MEMORY_IMG_BYTES_PER_PIXEL, MEMORY_IMG_RLE_PIXEL_DATA);
	sprintf(text,"%s (%s)",boot->device, boot->fstype);
	fb_draw_text (fb, CF_IMG_WIDTH+margin, slot*height+margin, 0, 0, 0, 
			&radeon_font, text);
			
}

void display_menu(FB *fb, struct bootlist *bl, int current)
{
	int i,j;
	int margin = fb->width/64;
	int slotheight = LOGO_IMG_HEIGHT;
	int slots = fb->height/slotheight -1;
	// struct boot that is in fist slot
	static int firstslot=0;
	
	/* Clear the background with #ecece1 */
	fb_draw_rect(fb, 0, 0, fb->width, fb->height,0xec, 0xec, 0xe1);

	/* logo */
	fb_draw_image(fb,
		      0,
		      0,
		      LOGO_IMG_WIDTH,
		      LOGO_IMG_HEIGHT,
		      LOGO_IMG_BYTES_PER_PIXEL, LOGO_IMG_RLE_PIXEL_DATA);
	fb_draw_text (fb, LOGO_IMG_WIDTH + margin, margin, 0, 0, 0, &radeon_font,
			"Which device do\nyou want to boot\ntoday?");
	if(current < firstslot)
		firstslot=current;
	if(current > firstslot + slots -1)
		firstslot = current - (slots -1);
	for(i=1, j=firstslot; i <= slots && j< bl->size; i++, j++){
		display_slot(fb, bl->list[j], i, slotheight, j == current);
	}
	fb_render(fb);
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

/*
 * Function: kexec_execw()
 * (execve and wait)
 * kexecboot's replace of system() call without /bin/sh invocation.
 * During execution  of the command, SIGCHLD will be blocked,
 * and SIGINT and SIGQUIT will be ignored (like system() does).
 * Takes 2 args (execve()-like):
 * - path - full path to executable file
 * - argv[] - array of args for executed file (command options e.g.)
 * - envp[] - array of environment strings ("key=value")
 * Return value:
 * - command exit status on success
 * - -1 on error (e.g. fork() failed)
 */
int kexec_execw(const char *path, char *const argv[], char *const envp[])
{
	pid_t pid;
	struct sigaction ignore, old_int, old_quit;
	sigset_t masked, oldmask;
	int status;

	/* Block SIGCHLD and ignore SIGINT and SIGQUIT */
	/* Do this before the fork() to avoid races */

	ignore.sa_handler = SIG_IGN;
	sigemptyset(&ignore.sa_mask);
	ignore.sa_flags = 0;
	sigaction(SIGINT, &ignore, &old_int);
	sigaction(SIGQUIT, &ignore, &old_quit);

	sigemptyset(&masked);
	sigaddset(&masked, SIGCHLD);
	sigprocmask(SIG_BLOCK, &masked, &oldmask);

	pid = fork();

	if (pid < 0)
		/* can't fork */
		return -1;
	else if (pid == 0) {
		/* it is child */
		sigaction(SIGINT, &old_int, NULL);
		sigaction(SIGQUIT, &old_quit, NULL);
		sigprocmask(SIG_SETMASK, &oldmask, NULL);

		/* replace child with executed file */
		execve(path, (char *const *)argv, (char *const *)envp);
		/* should not happens but... */
		_exit(127);
	}

	/* it is parent */

	/* wait for our child and store status */
	waitpid(pid, &status, 0);

	/* restore signal handlers */
	sigaction(SIGINT, &old_int, NULL);
	sigaction(SIGQUIT, &old_quit, NULL);
	sigprocmask(SIG_SETMASK, &oldmask, NULL);

	return status;
}

void start_kernel(struct boot *boot)
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
			DPRINTF("No network is detected, disabling ifdown()");
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
	if (boot->cmdline || extra_cmdline || boot->device) {
		/* allocate space */
		n = strlenn(str_cmdline_start) + strlenn(boot->cmdline) +
				sizeof(char) + strlenn(extra_cmdline) +
				strlenn(str_root) + strlenn(boot->device) +
				strlenn(str_rootfstype) + strlenn(boot->fstype) +
				strlenn(str_rootwait) + sizeof(char);
		
		cmdline_arg = (char *)malloc(n);
		if (NULL == cmdline_arg) {
			perror("Can't allocate memory for cmdline_arg");
			/*  Should we exit?
			exit(-1);
			*/
		} else {
			strcat(cmdline_arg, str_cmdline_start);
			if (extra_cmdline)
				strcat(cmdline_arg, extra_cmdline);
			if (boot->cmdline && extra_cmdline)
				strcat(cmdline_arg, " ");
			if (boot->cmdline)
				strcat(cmdline_arg, boot->cmdline);
			if (boot->device) {
				strcat(cmdline_arg, str_root);
				strcat(cmdline_arg, boot->device);
				if (boot->fstype) {
					strcat(cmdline_arg, str_rootfstype);
					strcat(cmdline_arg, boot->fstype);
				}
			}
			strcat(cmdline_arg, str_rootwait);

			kexec_load_argv[idx] = cmdline_arg;
			++idx;
		}
	}

	/* Append kernelpath as last arg of kexec */
	kexec_load_argv[idx] = boot->kernelpath;

	DPRINTF("kexec_load_argv: %s, %s, %s, %s\n", kexec_load_argv[0],
			kexec_load_argv[1], kexec_load_argv[2],
			kexec_load_argv[3]);

	DPRINTF("kexec_exec_argv: %s, %s, %s\n", kexec_exec_argv[0],
			kexec_exec_argv[1], kexec_exec_argv[2]);

	/* Mount boot device */
	if ( -1 == mount(boot->device, mount_point, boot->fstype,
			MS_RDONLY, NULL) ) {
		perror("Can't mount boot device");
		exit(-1);
	}

	/* Load kernel */
	n = kexec_execw(kexec_path, (char *const *)kexec_load_argv, envp);
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
	int i = 0, choice = 0;
	int initmode = 0;
	FB *fb;
	FILE *f;
	int angle = 270;
	char *eventif = "/dev/event0";
	char *tmp;
	struct bootlist * bl;
	struct input_event evt;
	struct termios old, new;
	char line[80];

	/* Hardware Models and appropriate FB angles */
	/* Terrier and Collie/Tosa/Poodle has unknown FB angle ATM */
	struct model_angle angles_by_models [] = {
		{"Shepherd",	0},
		{"Husky",	0},
		{"Corgi",	0},

		{"Akita",	270},
		{"Borzoi",	270},
		{"Spitz",	270},

		{"Collie",	270},
		{"Tosa",	270},
		{"Poodle",	270},
		{NULL, 0}
	};

	/* When our pid is 1 we are init-process */
	if ( 1 == getpid() ) {
		initmode = 1;

		DPRINTF("I'm the init-process!\n");

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

	/* Trying to recognise FB angle by hardware model */
	f = fopen("/proc/cpuinfo", "r");
	if (!f) {
		perror("/proc/cpuinfo");
		exit(-1);
	}
	while (fgets(line, 80, f)) {
		line[strlen(line) - 1] = '\0';
		DPRINTF("Line: %s\n", line);
		/* Search string 'Hardware' */
		tmp = strstr(line, "Hardware");
		if ( NULL == tmp) continue;

		/* Search colon and skip it and space after */
		tmp = strchr(tmp, ':');
		tmp += 2;
		DPRINTF("+ model is: %s\n", tmp);

		/* Check against array of models */
		for (i = 0; angles_by_models[i].model != NULL; i++) {
			DPRINTF("+ comparing with %s\n", angles_by_models[i].model);
			if ( NULL != strstr(tmp, angles_by_models[i].model) ) {
				/* match found */
				angle = angles_by_models[i].angle;
				DPRINTF("+ angle found: %d\n", angle);
				break;
			}
		}

		break;
	}
	fclose(f);
	DPRINTF("Model is checked\n");

	/* Check command-line args while not an init-process */
	if (!initmode) {
		i = 0;
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
			[-i|--input </dev/eventX>\n",
			argv[0]);
		exit(-1);
	}
	}

	DPRINTF("FB angle is %d, input device is %s\n", angle, eventif);
	DPRINTF("Going to fb mode\n");

	if ((fb = fb_new(angle)) == NULL)
		exit(-1);

	bl = scan_devices();
/*	bl = malloc(sizeof(struct bootlist));
	bl->size = 8;
	bl->list = malloc(8 * sizeof(struct boot*));
	
	bl->list[0] = malloc(sizeof(struct boot));
	bl->list[0]->device = "/dev/mtdblock2";
	bl->list[0]->fstype = "jffs2";
	bl->list[0]->kernelpath = "/boot/zImage";
	bl->list[0]->cmdline = "cmdline0";
	
	bl->list[1] = malloc(sizeof(struct boot));
	bl->list[1]->device = "/dev/mmcblk0p1";
	bl->list[1]->fstype = "ext2";
	bl->list[1]->kernelpath = "/boot/zImage";
	bl->list[1]->cmdline = "cmdline1";
	
	bl->list[2] = malloc(sizeof(struct boot));
	bl->list[2]->device = "/dev/hda1";
	bl->list[2]->fstype = "ext3";
	bl->list[2]->kernelpath = "/boot/zImage";
	bl->list[2]->cmdline = "cmdline2";
	
	bl->list[3] = malloc(sizeof(struct boot));
	bl->list[3]->device = "/dev/mmcblk1p2";
	bl->list[3]->fstype = "ext2";
	bl->list[3]->kernelpath = "/boot/zImage";
	bl->list[3]->cmdline = "cmdline3";
	
	bl->list[4] = malloc(sizeof(struct boot));
	bl->list[4]->device = "/dev/mtdblock2";
	bl->list[4]->fstype = "jffs2";
	bl->list[4]->kernelpath = "/boot/zImage";
	bl->list[4]->cmdline = NULL;
	
	bl->list[5] = malloc(sizeof(struct boot));
	bl->list[5]->device = "/dev/mmcblk0p1";
	bl->list[5]->fstype = "ext2";
	bl->list[5]->kernelpath = "/boot/zImage";
	bl->list[5]->cmdline = NULL;
	
	bl->list[6] = malloc(sizeof(struct boot));
	bl->list[6]->device = "/dev/hda1";
	bl->list[6]->fstype = "ext3";
	bl->list[6]->kernelpath = "/boot/zImage";
	bl->list[6]->cmdline = NULL;
	
	bl->list[7] = malloc(sizeof(struct boot));
	bl->list[7]->device = "/dev/mmcblk1p2";
	bl->list[7]->fstype = "ext2";
	bl->list[7]->kernelpath = "/boot/zImage";
	bl->list[7]->cmdline = NULL;
	*/
	if(!bl->size){
		puts("No bootable device found");
		exit(-1);
	}
	
	f = fopen(eventif,"r");
	if(!f){
	    perror(eventif);
	    exit(3);
	}
	

	// deactivate terminal input

	tcgetattr(fileno(stdin), &old);
	new = old;
	new.c_lflag &= ~ECHO;
//	new.c_cflag &=~CREAD;
	tcsetattr(fileno(stdin), TCSANOW, &new);

	do{
		display_menu(fb, bl, choice);
		do
			fread(&evt, sizeof(struct input_event), 1, f);
		while(evt.type != EV_KEY || evt.value != 0);
				    
		if(evt.code == KEY_UP && choice >0)
			choice--;
		if(evt.code == KEY_DOWN && choice < bl->size-1)
			choice++;
	//	printf("%d %d\n",choice, evt.code);
	    
	}while(evt.code != 87 && evt.code != 63);
	fclose(f);
	// reset terminal
	tcsetattr(fileno(stdin), TCSANOW, &old);
	fb_destroy(fb);
	start_kernel(bl->list[choice]);
	/* When we reach this point then some error was occured */
	DPRINTF("We should not reach this point!");
	return -1;
}
