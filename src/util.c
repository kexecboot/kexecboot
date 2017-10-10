/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2008-2011 Yuri Bushmelev <jay4mail@gmail.com>
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
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <termios.h>
#include <limits.h>		/* LONG_MAX, INT_MAX */
#include <stdarg.h>		/* va_start/va_end */

#include "config.h"
#include "util.h"


/* Create charlist structure */
struct charlist *create_charlist(int size)
{
	struct charlist *cl;

	cl = malloc(sizeof(*cl));
	cl->list = malloc(size * sizeof(char *));
	cl->size = size;
	cl->fill = 0;
	return cl;
}


/* Free charlist structure */
void free_charlist(struct charlist *cl)
{
	int i;
	for (i = 0; i < cl->fill; i++)
		free(cl->list[i]);
	free(cl->list);
	free(cl);
}


/* Add item to charlist structure */
void addto_charlist(struct charlist *cl, const char *str)
{
	cl->list[cl->fill] = strdup(str);
	++cl->fill;

	/* Resize list when needed */
	if (cl->fill >= cl->size) {
		char **new_list;

		cl->size <<= 1;	/* size *= 2; */
		new_list = realloc(cl->list, cl->size * sizeof(char *));
		if (NULL == new_list) {
			DPRINTF("Can't resize menu list");
			return;
		}

		cl->list = new_list;
	}
}


/* Search item in charlist structure */
int in_charlist(struct charlist *cl, const char *str)
{
	int i;
	char **p = cl->list;

	for (i = 0; i < cl->fill; i++) {
		if (!strcmp(str, *p)) return i;
		++p;
	}
	return -1;
}


kx_text *log_open(unsigned int size)
{
	kx_text *log;

	log = malloc(sizeof(*log));
	log->rows = create_charlist(size);
	log->current_line_no = 0;

	return log;
}

void log_plain_msg(kx_text *log, char *msg)
{
	/* Add to charlist */
	if (log) addto_charlist(log->rows, msg);

	/* Print to stderr */
	fputs(msg, stderr);
	fputs("\n", stderr);
}

/* Log message */
void log_msg(kx_text *log, char *fmt, ...)
{
	static char *b, *e, buf[512];
	static va_list ap;

	/* Format string */
	va_start(ap, fmt);
	vsnprintf((char *)&buf, sizeof(buf), fmt, ap);
	va_end(ap);
	
	/* Split strings by '\n' and add to charlist */
	b = buf;
	while (NULL != (e = strchr(b, '\n'))) {
		*e = '\0'; /* Terminate this part of string */
		log_plain_msg(log, b);
		b = e+1;
	}

	/* Process latest part of string if any */
	if (*b != '\0') log_plain_msg(log, b);
}

void log_close(kx_text *log)
{
	if (!log) return;

	free_charlist(log->rows);
	free(log);
}

/* Change string case. 'u' - uppercase, 'l'/'d' - lowercase */
char *chcase(char ul, const char *src, char *dst)
{
	unsigned char *c1 = (unsigned char *)src;
	unsigned char *c2 = (unsigned char *)dst;

	while ('\0' != *c1) {
		switch (ul) {
		case 'u':
			*c2 = toupper(*c1);
			break;
		case 'l':
		case 'd':
			*c2 = tolower(*c1);
			break;
		default:
			break;
		}
		++c1;
		++c2;
	}
	*c2 = '\0';

	return dst;
}


/* Return pointer to word in string 'str' with end of word in 'endptr' */
char *get_word(char *str, char **endptr)
{
	char *p = str;
	char *wstart;

	/* Skip space before word */
	p = ltrim(str);

	if ('\0' != *p) {
		wstart = p;

		/* Search end of word */
		while ( !isspace(*p) && ('\0' != *p) ) ++p;
		if (NULL != endptr) *endptr = p;
		return wstart;
	} else {
		if (NULL != endptr) *endptr = str;
		return NULL;
	}
}


/* Strip leading and trailing spaces, tabs and newlines
 * NOTE: this will modify str */
char *trim(char *str)
{
	char *s, *e;

	s = ltrim(str);
	e = rtrim(str);

	if (s <= e) {
		*(e+1) = '\0';
		return s;
	}

	return NULL;
}


/* Skip trailing white-space */
char *rtrim(char *str)
{
	char *p = str + strlen(str) - 1;
	while( isspace(*p) && (p > str) ) --p;
	return p;
}


/* Skip leading white-space */
char *ltrim(char *str)
{
	char *p = str;
	while( isspace(*p) && ('\0' != *p) ) ++p;
	return p;
}


/* Get unsigned long-long integer */
unsigned long long get_nnll(const char *str, char **endptr, int *error_flag)
{
	static unsigned long long val;

	errno = 0;
	val = strtoull(str, endptr, 10);

	/* Check for various possible errors */
	if ((errno == ERANGE && (val == ULLONG_MAX))
		|| (errno != 0 && val == 0)
	) {
		if (error_flag) *error_flag = -2;
		return 0;
	}

	/* Check that we have found any digits */
	if ((NULL != endptr) && (*endptr == str)) {
		if (error_flag) *error_flag = -3;
		return 0;
	}

	/* If we got here, strtoll() successfully parsed a number */
	if (error_flag) *error_flag = 0;
	return val;
}

/* Get non-negative integer */
int get_nni(const char *str, char **endptr)
{
	static unsigned long long val;
	static int eflag;

	eflag = 0;
	val = get_nnll(str, endptr, &eflag);

	switch (eflag) {
	case 0:
		if (val > INT_MAX)	/* Too big to return as int */
			return -3;
		else
			return (int)val;
	case -2:
	case -3:
		return eflag;
	default:
		return -1;
	}
}


/*
 * Change terminal settings.
 * Mode: 1 - change; 0 - restore.
 */
void setup_terminal(char *ttydev, int *echo_state, int mode)
{
	struct termios tc;
	FILE *f;

	/* Flush unread input */
	tcflush(fileno(stdin), TCIFLUSH);

	/* Deactivate/Activate terminal input */
	tcgetattr(fileno(stdin), &tc);

	if (0 == mode) {	/* Restore state */
		tc.c_lflag = (tc.c_lflag & ~ECHO) | *echo_state;
	} else {			/* reset ECHO */
		*echo_state = tc.c_lflag & ECHO;	/* Save state (ECHO or 0) */
		tc.c_lflag &= ~ECHO;
	}

	tcsetattr(fileno(stdin), TCSANOW, &tc);

	if (NULL == ttydev) {
		log_msg(lg, "We have no tty");
		return;
	}

	/* Switch cursor off/on */
	if (NULL != ttydev) {
		f = fopen(ttydev, "r+");
		if (NULL == f) {
			log_msg(lg, "Can't open '%s' for writing: %s", ttydev, ERRMSG);
			return;
		}
	} else {
		f = stdout;
	}

	if (0 == mode) {	/* Show cursor */
		fputs("\033[?25h", f);	/* Applied to *term */
	} else {			/* Hide cursor */
		fputs("\033[?25l", f);
	}

	if (NULL != ttydev) fclose(f);
}


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

/* wrapper around fexecw ubiattach          */
/* returns ubi_id attached to mtd_id        */
/* on error, returns -1 so that mount fails */
int ubi_attach(const char *mtd_id)
{
	/* prepare ubiattach_argv[] */
	const char *ubiattach_argv[] = { NULL, NULL, NULL, NULL };
	char *const envp[] = { NULL };
	int n,res;

	ubiattach_argv[0] = UBIATTACH_PATH;
	ubiattach_argv[1] = "-m";
	ubiattach_argv[2] = mtd_id;
#ifdef UBI_VID_HDR_OFFSET
	ubiattach_argv[3] = "-O" UBI_VID_HDR_OFFSET;
#endif

	/* fexecw ubiattach */
	n = fexecw(UBIATTACH_PATH, (char *const *)ubiattach_argv, envp);
	if (-1 == n) {
		log_msg(lg, "+ ubiattach failed: %s", ERRMSG);
		res = -1;
	} else {
		res = find_attached_ubi_device(mtd_id);
	}
	return res;
}


/* loop until /sys/class/ubi/ubiX/mtd_num == mtd_id */
/* kernel: max 32 ubi devices (0-31)                */
/* kernel: max 16 mtd char devices (0-15)           */
int find_attached_ubi_device(const char *mtd_id)
{
	char sys_class_ubi[32]; /* max 26 + 2 + 1 */
	int ubi_id;
	char line[3];
	int res = -1;
	FILE *f;

	for (ubi_id = 0; ubi_id < 32; ubi_id++)
	{
		snprintf(sys_class_ubi, sizeof(sys_class_ubi), "/sys/class/ubi/ubi%d/mtd_num", ubi_id);
		f = fopen(sys_class_ubi, "r");
		if (NULL == f) {
			/* log_msg(lg, "+ missing ubi%d in sysfs: %s", ubi_id, ERRMSG); */
		} else {
			/* We have only one line in that file */
			if (fgets(line, sizeof(line), f)) {
				line[strlen(line) - 1] = '\0';
				if (!strncmp(line, mtd_id, 2)) {
					log_msg(lg, "+ map /dev/ubi%d on /dev/mtd%s", ubi_id, mtd_id);
					res = ubi_id;
				}
			}
			fclose(f);
			if (res >= 0) break;
		}
	}
	return res;
}
