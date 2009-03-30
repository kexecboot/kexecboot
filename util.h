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

#ifndef _HAVE_UTIL_H_
#define _HAVE_UTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>     /* uint's below */
#include <errno.h>
#include <limits.h>		/* LONG_MAX, INT_MAX */

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

/* Macro for dealing with NULL strings */
#define strlenn(s)	( (NULL != s) ? (strlen(s)) : 0 )

/* Debug macro */
/* #define DEBUG */
#ifdef DEBUG
#define DPRINTF(fmt, args...)	do { \
		fprintf(stderr, fmt, ##args); \
	} while (0)
#else
#define DPRINTF(fmt, args...)	do { } while (0)
#endif

/* Known hardware models */
enum hw_model_ids {
	HW_MODEL_UNKNOWN,

	HW_SHARP_POODLE,
	HW_SHARP_COLLIE,
	HW_SHARP_CORGI,
	HW_SHARP_SHEPHERD,
	HW_SHARP_HUSKY,
	HW_SHARP_TOSA,
	HW_SHARP_AKITA,
	HW_SHARP_SPITZ,
	HW_SHARP_BORZOI,
	HW_SHARP_TERRIER
};

/*
 * Hardware models info (id, name, parameters...)
 * Name is searched in 'Hardware' line of /proc/cpuinfo to detect model.
 */
struct hw_model_info {
	enum hw_model_ids hw_model_id;
	char *name;
	// parameters
	int angle;
};


/*
 * FUNCTIONS
 */

/*
 * Function: strtolower()
 * Lowercase the string.
 * Takes 2 arg:
 * - source string;
 * - destination buffer to store lowercased string.
 * Return value:
 * - pointer to destination string.
 */
char *strtolower(const char *src, char *dst);

/* Get word from string */
//int get_word(char *str, char **word);
char *get_word(char *str, char **endptr);

/* Get non-negative integer */
int get_nni(const char *str, char **endptr);

/*
 * Function: fexecw()
 * (fork, execve and wait)
 * kexecboot's replace of system() call without /bin/sh invocation.
 * During execution  of the command, SIGCHLD will be blocked,
 * and SIGINT and SIGQUIT will be ignored (like system() does).
 * Takes 3 args (execve()-like):
 * - path - full path to executable file
 * - argv[] - array of args for executed file (command options e.g.)
 * - envp[] - array of environment strings ("key=value")
 * Return value:
 * - command exit status on success
 * - -1 on error (e.g. fork() failed)
 */
int fexecw(const char *path, char *const argv[], char *const envp[]);

/*
 * Function: detect_hw_model()
 * Detect hardware model.
 * Return value:
 * - pointer to hw_model_info structure from model_info array
 */
struct hw_model_info *detect_hw_model(struct hw_model_info model_info[]);


#endif //_HAVE_UTIL_H_
