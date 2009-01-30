/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2008-2009 Yuri Bushmelev <jay4mail@gmail.com>
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

#include "util.h"

/*
 * Function: fexecw()
 * (fork, execve and wait)
 * Look in util.h for description
 */
int fexecw(const char *path, char *const argv[], char *const envp[])
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

/*
 * Function: detect_hw_model()
 * Detect hardware model.
 * Look in util.h for description
 */
struct hw_model_info *detect_hw_model(void) {
	int i;
	char *tmp, line[80];
	struct hw_model_info *found_model = NULL;
	FILE *f;

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

		{HW_MODEL_UNKNOWN,	NULL,		0	}
	};


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
		for (i = 0; model_info[i].hw_model_id != HW_MODEL_UNKNOWN; i++) {
			DPRINTF("+ comparing with %s\n", model_info[i].name);
			if ( NULL != strstr(tmp, model_info[i].name) ) {
				/* match found */
				DPRINTF("+ match found!\n");
				found_model = &model_info[i];
				break;
			}
		}
		break;
	}
	fclose(f);

	return found_model;
}


