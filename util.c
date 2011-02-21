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
			DPRINTF("Can't resize menu list\n");
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

/* Log message */
void log_msg(kx_text* log, char* fmt, ...)
{
	static char buf[512];
	static va_list ap;
	static int s;

	/* Format string */
	va_start(ap, fmt);
	vsnprintf((char *)&buf, sizeof(buf), fmt, ap);
	va_end(ap);
	
	s = strlen(buf);

	/* Add to charlist */
	if (log) addto_charlist(log->rows, buf);

	/* Append '\n' */
	if ((s + 1) < sizeof(buf)) {
		buf[s] = '\n';
		buf[s+1] = '\0';
	}
	
	/* Print to stderr */
	fputs(buf, stderr);
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


/* Get non-negative integer */
int get_nni(const char *str, char **endptr)
{
	long val;

	errno = 0;
	val = strtoul(str, endptr, 10);

	/* Check for various possible errors */
	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
		|| (errno != 0 && val == 0)) {
		perror("get_nni");
		return -1;
	}

	if (val > INT_MAX) {
		DPRINTF("get_nni: Value too big for unsigned int\n");
		return -1;
	}

	if ((NULL != endptr) && (*endptr == str)) {
		DPRINTF("get_nni: No digits were found\n");
		return -1;
	}

	/* If we got here, strtol() successfully parsed a number */
	return (int)val;
}



/*
 * Change terminal settings.
 * Mode: 1 - change; 0 - restore.
 */
void setup_terminal(char *ttydev, int *echo_state, int mode)
{
	struct termios tc;
	FILE *f;

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
		DPRINTF("We have no tty\n");
		return;
	}

	/* Switch cursor off/on */
	if (NULL != ttydev) {
		f = fopen(ttydev, "r+");
		if (NULL == f) {
			DPRINTF("Can't open '%s' for writing\n", ttydev);
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

