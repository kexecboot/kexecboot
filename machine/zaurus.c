/*
 *  kexecboot - A kexec based bootloader
 *
 *  Copyright (c) 2009-2011 Yuri Bushmelev <jay4mail@gmail.com>
 *  Copyright (c) 2009-2011 Andrea Adami <andrea.adami@gmail.com>
 *
 *  Based on
 *  NAND logical utility for Sharp Zaurus SL-C7x0/860/7500/Cxx00
 *  version 1.0
 *  Copyright 2006 Alexander Chukov <sash@pdaXrom.org>
 *
 *  Based on nanddump.c
 *  Copyright (c) 2000 David Woodhouse (dwmw2@infradead.org)
 *  Copyright (c) 2000 Steven J. Hill (sjhill@cotw.com)
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

#include "config.h"

#ifdef USE_ZAURUS

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// #include <linux/fs.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <mtd/mtd-user.h>
#include "zaurus.h"
#include "../util.h"

#define NAND_LOGICAL_SIZE (7 * 1024 * 1024)

#define NAND_NOOB_LOGADDR_00		8
#define NAND_NOOB_LOGADDR_01		9
#define NAND_NOOB_LOGADDR_10		10
#define NAND_NOOB_LOGADDR_11		11
#define NAND_NOOB_LOGADDR_20		12
#define NAND_NOOB_LOGADDR_21		13

static uint nand_get_logical_no(unsigned char *oob)
{
	unsigned short us, bit;
	int par, good0, good1;

	if ( oob[NAND_NOOB_LOGADDR_00] == oob[NAND_NOOB_LOGADDR_10] &&
			oob[NAND_NOOB_LOGADDR_01] == oob[NAND_NOOB_LOGADDR_11] )
	{
		good0 = NAND_NOOB_LOGADDR_00;
		good1 = NAND_NOOB_LOGADDR_01;

	} else if ( oob[NAND_NOOB_LOGADDR_10] == oob[NAND_NOOB_LOGADDR_20] &&
			oob[NAND_NOOB_LOGADDR_11] == oob[NAND_NOOB_LOGADDR_21] )
	{
		good0 = NAND_NOOB_LOGADDR_10;
		good1 = NAND_NOOB_LOGADDR_11;

	} else if ( oob[NAND_NOOB_LOGADDR_20] == oob[NAND_NOOB_LOGADDR_00] &&
			oob[NAND_NOOB_LOGADDR_21] == oob[NAND_NOOB_LOGADDR_01] )
	{
		good0 = NAND_NOOB_LOGADDR_20;
		good1 = NAND_NOOB_LOGADDR_21;

	} else {
		return (uint)-1;
	}

    us = (((unsigned short)(oob[good0]) & 0x00ff) << 0) |
			(((unsigned short)(oob[good1]) & 0x00ff) << 8);

	par = 0;
	for (bit = 0x0001; bit != 0; bit <<= 1) {
		if (us & bit) par++;
	}

	if (par & 1) return (uint)-2;

	if (us == 0xffff) {
		return 0xffff;
	} else {
		return ((us & 0x07fe) >> 1);
	}
}

void scan_logical(int fd, struct mtd_oob_buf *oob, unsigned long *log2phy, int blocks, int erasesize)
{
	int i, log_no;
	unsigned long offset;
	int ret = 1;

	for (i = 0; i < blocks; i++)
		log2phy[i] = (uint) -1;

	offset = 0;
	for (i = 0; i < blocks; i++) {
		oob->start = offset;
		ret = ioctl(fd, MEMREADOOB, oob);

		if (!ret) {
			log_no = nand_get_logical_no(oob->ptr);	/* NOTE: Here was oobbuf before */
			if ( ((int)log_no >= 0) && (log_no < blocks) ) {
				log2phy[log_no] = offset;
			}
		}
		offset += erasesize;
	}
}


/* Return mtd partition size in bytes */
unsigned int get_mtdsize(char *mtd_name)
{
	int fd;
	off_t o;

	if ((fd = open(mtd_name, O_RDONLY)) == -1) {
		perror("open flash");
		return 0;
	}
	o = lseek(fd, 0, SEEK_END);
	if (o < 0) {
		perror("lseek");
		return 0;
	}
	return (unsigned int)o;
}

/*
Sharp's bootloader (Angel) hardcoded addresses
one big read 0x60004 - 0x60028 36 bytes

0x60004 boot
0x60014 fsro
0x60024 fsrw -> seems wrong: is 0x04000000 = 64Mb while new models have 128M 0x08000000 flash

To Do: check backup copy at 0x64004 and 0x64014 for consistency ?
bkp boot on 0x64004
bkp fsro on 0x64014
bkp fsrw on 0x64024
*/

/* Read zaurus'es mtdparts offsets from paraminfo NAND area */
int zaurus_read_partinfo(struct zaurus_partinfo_t *partinfo)
{

	const unsigned long start_addr = 0x60004;
	const unsigned long length = 36;
	int blocks, bs, fd;
	unsigned int mtdsize = 0;
	unsigned long end_addr;
	unsigned long ofs;
	unsigned long *log2phy;
	unsigned char *readbuf, *p;
	mtd_info_t meminfo;
	struct mtd_oob_buf oob = {0, 16, NULL};

	/* Count overall NAND size */
	mtdsize = get_mtdsize("/dev/mtd2");	/* FSRO (Root) */
	mtdsize += get_mtdsize("/dev/mtd3");	/* FSRW (Home) */

	/* Open MTD device */
	if ((fd = open("/dev/mtd1", O_RDONLY)) == -1) {
		perror("open flash");
		return -1;
	}

	/* Fill in MTD device capability structure */
	if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
		perror("MEMGETINFO");
		goto closefd;
	}

	/* Make sure device page sizes are valid */
	if (!(meminfo.oobsize == 64 && meminfo.writesize == 2048) &&
			!(meminfo.oobsize == 16 && meminfo.writesize == 512) &&
			!(meminfo.oobsize == 8 && meminfo.writesize == 256)
	) {
		log_msg(lg, "[zaurus] Unknown flash (not normal NAND)");
		goto closefd;
	}

	//printf("erasesize %x\nwritesize %x\noobsize %x\nsize %x\n", meminfo.erasesize, meminfo.writesize, meminfo.oobsize, meminfo.size);

	blocks = NAND_LOGICAL_SIZE / meminfo.erasesize;
	log2phy = malloc(blocks * sizeof(*log2phy));
	oob.ptr = malloc(meminfo.writesize);
	if (length > meminfo.erasesize)
		readbuf = malloc(length);
	else
		readbuf = malloc(meminfo.erasesize);

	scan_logical(fd, &oob, log2phy, blocks, meminfo.erasesize);

	log_msg(lg, "[zaurus] Start: %lx, End: %lx", start_addr, length);

	end_addr = start_addr + length;
	bs = meminfo.writesize;

	/* Read data */
	p = readbuf;
	for (ofs = start_addr; ofs < end_addr; ofs += bs) {
	    int offset = log2phy[ofs / meminfo.erasesize];

	    if ((int)offset < 0) {
			log_msg(lg, "[zaurus] NAND logical - offset %08lX not found", ofs);
			goto closeall;
	    }

		offset += ofs % meminfo.erasesize;

		log_msg(lg, "[zaurus] Offset: %x", offset);

		if (pread(fd, p, bs, offset) != bs) {
			perror("pread");
			goto closeall;
		}
		p += bs;
	}
	close(fd);
	free(log2phy);
	free(oob.ptr);

	mtdsize += meminfo.size;
	log_msg(lg, "[zaurus] Total MTD size: %X", mtdsize);

	/* Calculating offsets. fd, bs and blocks are used as temporary variables */
	fd = readbuf[0] + (readbuf[1] << 8) + (readbuf[2] << 16) + (readbuf[3] << 24);
	log_msg(lg, "[zaurus] SMF offset: %X", fd);

	bs = readbuf[16] + (readbuf[17] << 8) + (readbuf[18] << 16) + (readbuf[19] << 24);
	log_msg(lg, "[zaurus] Root offset: %X", bs);

	blocks = readbuf[28] + (readbuf[29] << 8) + (readbuf[30] << 16) + (readbuf[31] << 24);
	log_msg(lg, "[zaurus] Root[2] offset: %X", blocks);

	free(readbuf);

	log_msg(lg, "[zaurus] Home size: %X", mtdsize - bs);

	/* Try to check that we have original NAND flash content */
	if (blocks != bs) {
		log_msg(lg, "[zaurus] Original NAND content was changed. We can't use mtd partition info");
		return -1;
	}

	/* Convert offsets to sizes */
	/* and convert bytes to Kbytes to be like /proc/partition */
	partinfo->home = (unsigned int)(mtdsize - bs) >> 10;
	partinfo->smf = (unsigned int)fd >> 10;
	partinfo->root = (unsigned int)(bs - fd) >> 10;

	return 0;

closeall:
	free(log2phy);
	free(readbuf);
	free(oob.ptr);
closefd:
	close(fd);

	return -1;
}


char *zaurus_mtdparts(struct zaurus_partinfo_t *partinfo)
{
	const char tag_format[] = "sharpsl-nand:%uk(smf),%uk(root),-(home)";
	const int tag_size = sizeof(tag_format) << 1;	/* to be sure.. */
	char *tag;

	tag = malloc(tag_size);
	if (NULL == tag) {
		DPRINTF("Can't allocate memory for mtdparts tag");
		return NULL;
	}

	snprintf(tag, tag_size, tag_format, partinfo->smf, partinfo->root);
	return tag;
}

#endif /* USE_ZAURUS */
