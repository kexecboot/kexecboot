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

#include "util.h"

struct charlist *create_charlist(int size)
{
	struct charlist *cl;

	cl = malloc(sizeof(*cl));
	cl->list = malloc(size * sizeof(char *));
	cl->size = size;
	cl->fill = 0;
	return cl;
}

void free_charlist(struct charlist *cl)
{
	int i;
	for (i = 0; i < cl->size; i++)
		free(cl->list[i]);
	free(cl);
}

void addto_charlist(char *str, struct charlist *cl)
{
	cl->list[cl->fill] = str;
	++cl->fill;
	if (cl->fill == cl->size) {
		cl->size <<= 1;	/* size *= 2; */
		cl->list = realloc(cl->list, cl->size * sizeof(char *));
	}
}

int in_charlist(const char *str, struct charlist *cl)
{
	int i;
	for (i = 0; i < cl->size; i++)
		if (!strcmp(str, cl->list[i]))
			return i;
	return -1;
}


void trim(char *s)
{
    // Trim leading spaces and tabs
    int i=0, j;

    while ( (s[i]==' ') || (s[i]== '\t') )
        i++;

    if (i > 0) {
        for (j=0; j < strlen(s); j++)
            s[j]=s[j+i];

       s[j]='\0';
    }

    // Trim trailing spaces, tabs and newlines
    i = strlen(s)-1;
    while ( (s[i] == ' ') || (s[i] == '\t') || (s[i] == '\n') )
        i--;

    if (i < (strlen(s)-1))
        s[i+1]='\0';
}

enum casetype {upper, lower};

char *docase(char *s, enum casetype c)
{
  char* dest;
  int i;

  if (!s)
  	return NULL;

  dest = malloc(strlen(s)+1);

  for (i = 0; i < (int)strlen(s); i++)
  	dest[i] = ( c==upper ? toupper(s[i]) : tolower(s[i]) );

  dest[i] = '\0';

  return dest;
}

char *upcase(char *s)
{
	return docase(s, upper);
}

char *locase(char *s)
{
	return docase(s, lower);
}


/*
 * Function: strtolower()
 * Lowercase the string
 * Look in util.h for description
 */
char *strtolower(const char *src, char *dst) {
	unsigned char *c1 = (unsigned char *)src;
	unsigned char *c2 = (unsigned char *)dst;

	while ('\0' != *c1) {
		/* toupper() expects an unsigned char (implicitly cast to int)
			as input, and returns an int, which is exactly what we want. */
		*c2 = tolower(*c1);
		++c1;
		++c2;
	}
	*c2 = '\0';

	return dst;
}


char *get_word(char *str, char **endptr) {
	char *p = str;
	char *wstart;

	/* Skip space before word */
	while( isspace(*p) && ('\0' != *p) ) ++p;

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

	if (*endptr == str) {
		DPRINTF("get_nni: No digits were found\n");
		return -1;
	}

	/* If we got here, strtol() successfully parsed a number */
	return (int)val;
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

/*
 * Function: detect_hw_model()
 * Detect hardware model.
 * Look in util.h for description
 */
struct hw_model_info *detect_hw_model(struct hw_model_info model_info[]) {
	int i;
	char *tmp, line[80];
	FILE *f;

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
		if (NULL != tmp) break;
	}
	fclose(f);

	if ( NULL != tmp) {
		/* Search colon and skip it and space after */
		tmp = strchr(tmp, ':');
		tmp += 2;
		DPRINTF("+ model is: %s\n", tmp);
	}

	/* Check against array of models */
	for (i = 0; model_info[i].hw_model_id != HW_MODEL_UNKNOWN; i++) {
		DPRINTF("+ comparing with %s\n", model_info[i].name);
		if (NULL == tmp) continue;	/* Fastforwarding to unknown model */
		if ( NULL != strstr(tmp, model_info[i].name) ) {
			/* match found */
			DPRINTF("+ match found!\n");
			break;
		}
	}

	return &model_info[i];
}


