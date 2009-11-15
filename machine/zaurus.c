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

#include "../config.h"

#ifdef USE_ZAURUS

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "mtd-user.h"
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

void scan_logical(int fd, struct mtd_oob_buf *oob, unsigned long *log2phy, unsigned char *oobbuf, int blocks, int erasesize)
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
			log_no = nand_get_logical_no(oobbuf);	/* NOTE: Here was oobbuf before */
			if ( ((int)log_no >= 0) && (log_no < blocks) ) {
				log2phy[log_no] = offset;
				DPRINTF("NAND logical - %08X -> %04X\n", offset, log_no * erasesize);
			} else {
				DPRINTF("NAND logical - %08X - skip (%x)\n", offset, log_no);
			}
		} else {
			DPRINTF("NAND logical - offset %x read OOB problem\n", offset);
		}
		offset += erasesize;
	}
}


/*
Sharp's bootloader (Angel) hardcoded addresses
one big read 0x60004 - 0x60028 36 bytes

0x60004 boot
0x60014 fsro
0x60024 fsrw -> seems wrong: is 0x04000000 = 64Mb while new models have 128M 0x08000000 flash

bkp boot on 0x64004
bkp fsro on 0x64014
bkp fsrw on 0x64024

To Do: check backup copy at 0x64004 and 0x64014 for consistency ?
*/

/* Read zaurus'es mtdparts from paraminfo NAND area */
int zaurus_read_partinfo(struct zaurus_partinfo_t *partinfo)
{

	int blocks, bs, fd;
	unsigned long start_addr = 0x60004;
	unsigned long length = 36;
	unsigned long end_addr;
	unsigned long ofs;
	unsigned long *log2phy;
	unsigned char *readbuf;
	unsigned char *oobbuf;
	mtd_info_t meminfo;
	struct mtd_oob_buf oob = {0, 16, NULL};

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
		DPRINTF("Unknown flash (not normal NAND)\n");
		goto closefd;
	}

	//printf("erasesize %x\nwritesize %x\noobsize %x\nsize %x\n", meminfo.erasesize, meminfo.writesize, meminfo.oobsize, meminfo.size);

	blocks = NAND_LOGICAL_SIZE / meminfo.erasesize;
	log2phy = malloc(blocks * sizeof(unsigned long));
	readbuf = malloc(meminfo.erasesize);
	oobbuf = malloc(meminfo.writesize);
	oob.ptr = oobbuf;

	scan_logical(fd, &oob, log2phy, oobbuf, blocks, meminfo.erasesize);

	DPRINTF("Start: %x, End: %x\n", start_addr, length);

	end_addr = start_addr + length;
	bs = meminfo.writesize;

	for (ofs = start_addr; ofs < end_addr ; ofs+=bs) {
	    int offset = log2phy[ofs / meminfo.erasesize];

	    if ((int)offset < 0) {
			DPRINTF("NAND logical - offset %08X not found\n", ofs);
			goto closeall;
	    }

		offset += ofs % meminfo.erasesize;

	    DPRINTF("Offset: %x\n", offset);

		if (pread(fd, readbuf, bs, offset) != bs) {
			perror("pread");
			goto closeall;
		}


		/* reversed byte order (on little endian?) */

		DPRINTF("SMF: %02X%02X%02X%02X\n", readbuf[3], readbuf[2], readbuf[1], readbuf[0]);
		DPRINTF("Root: %02X%02X%02X%02X\n", readbuf[19], readbuf[18], readbuf[17], readbuf[16]);
		// seems wrong: is 0x04000000 = 64Mb while new models have 128M 0x08000000 flash
		DPRINTF("Home: %02X%02X%02X%02X\n", readbuf[35], readbuf[34], readbuf[33], readbuf[32]);

	}

	partinfo->smf = 0x700000;
	partinfo->root = 0x4100000;
	partinfo->home = 0x8000000;

closeall:
	free(log2phy);
	free(readbuf);
	free(oobbuf);
closefd:
	close(fd);

	return -1;
}

#endif /* USE_ZAURUS */