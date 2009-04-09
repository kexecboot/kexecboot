bin_PROGRAMS=kexecboot

AM_CFLAGS = $(GCC_FLAGS)

kexecboot_SOURCES = xpm.c util.c devicescan.c fb.c kexecboot.c \
	cfgparser.c fstype/fstype.c

EXTRA_DIST = res/make-image-header.sh

MAINTAINERCLEANFILES = aclocal.m4 compile config.guess config.sub configure \
	depcomp install-sh ltmain.sh Makefile.in missing config.h.in
