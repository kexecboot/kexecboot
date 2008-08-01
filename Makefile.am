bin_PROGRAMS=kexecboot

AM_CFLAGS = $(GCC_FLAGS)

kexecboot_SOURCES = kexecboot.c kexecboot.h fb.c fb.h devicescan.c devicescan.h \
		fstype/fstype.c fstype/fstype.h fstype/cramfs_fs.h fstype/ext2_fs.h fstype/ext3_fs.h \
		fstype/iso9660_sb.h fstype/jfs_superblock.h fstype/luks_fs.h fstype/lvm2_sb.h fstype/minix_fs.h \
		fstype/reiserfs_fs.h fstype/romfs_fs.h fstype/swap_fs.h fstype/xfs_sb.h \
		res/logo-img.h res/cf-img.h res/mmc-img.h res/memory-img.h res/radeon-font.h res/font.h


EXTRA_DIST = res/make-image-header.sh
 
MAINTAINERCLEANFILES = aclocal.m4 compile config.guess config.sub configure depcomp install-sh ltmain.sh Makefile.in missing config.h.in
