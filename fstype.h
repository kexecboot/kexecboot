/*
 * by rmk
 *
 * Detect filesystem type (on stdin) and output strings for two
 * environment variables:
 *  FSTYPE - filesystem type
 *  FSSIZE - filesystem size (if known)
 *
 * We currently detect (in order):
 *  gzip, cramfs, romfs, xfs, minix, ext3, ext2, reiserfs, jfs
 *
 * MINIX, ext3 and Reiserfs bits are currently untested.
 */

#ifndef FSTYPE_H
#define FSTYPE_H

#include <unistd.h>

int identify_fs(int fd, const char **fstype,
		unsigned long long *bytes, off_t offset);

#endif
