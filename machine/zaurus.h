/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2009 Yuri Bushmelev <jay4mail@gmail.com>
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

#ifndef _HAVE_ZAURUS_H
#define _HAVE_ZAURUS_H

struct zaurus_partinfo_t {
	unsigned int smf;
	unsigned int root;
	unsigned int home;
};

/* Read zaurus'es mtdparts from paraminfo NAND area */
int zaurus_read_partinfo(struct zaurus_partinfo_t *partinfo);
char *zaurus_mtdparts(struct zaurus_partinfo_t *partinfo);

#endif /* _HAVE_ZAURUS_H */