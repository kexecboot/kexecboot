/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2009 Omegamoon <omegamoon@gmail.com>
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

#include <stdint.h>     /* uint's below */

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


/* Charlist structure */
struct charlist {
	char **list;
	unsigned int size;
	unsigned int fill;
};


/*
 * FUNCTIONS
 */

/* Allocate charlist structure of 'size' items initial */
struct charlist *create_charlist(int size);

/* Destroy specified charlist structure 'cl' */
void free_charlist(struct charlist *cl);

/* Append string 'str' to end of charlist 'cl' */
void addto_charlist(char *str, struct charlist *cl);

/* Return position of string 'str' in charlist 'cl' or (-1) when not found */
int in_charlist(const char *str, struct charlist *cl);

/* Strip leading and trailing white-space */
/* NOTE: this will modify str */
char *trim(char *str);

/* Skip trailing white-space */
char *rtrim(char *str);

/* Skip leading white-space */
char *ltrim(char *str);

/* Uppercase the string 'src' and place result into 'dst' */
#define strtoupper(src, dst) do { chcase('u', src, dst); } while (0)

/* Lowercase the string 'src' and place result into 'dst' */
#define strtolower(src, dst) do { chcase('l', src, dst); } while (0)

/* Change case of 'str' and place result into 'dst' */
/* ul value can be: 'u' for uppercase, 'l'/'d' for lowercase */
char *chcase(char ul, const char *src, char *dst);

/* Return pointer to word in string 'str' and end of word in 'endptr' */
char *get_word(char *str, char **endptr);

/* Return non-negative integer from string 'str' and end of number in 'endptr' */
int get_nni(const char *str, char **endptr);

/* Check pointer for NULL value and free() if not */
#define dispose(ptr) do { if (NULL != ptr) free(ptr); } while (0)

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

#endif //_HAVE_UTIL_H_
