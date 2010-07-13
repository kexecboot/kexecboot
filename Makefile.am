bin_PROGRAMS=kexecboot

AM_CFLAGS = $(GCC_FLAGS)

kexecboot_SOURCES = cfgparser.c devicescan.c fb.c fstype/fstype.c gui.c \
	kexecboot.c menu.c util.c xpm.c rgb.c machine/zaurus.c

EXTRA_DIST = res/make-image-header.sh

MAINTAINERCLEANFILES = aclocal.m4 compile config.guess config.sub configure \
	depcomp install-sh ltmain.sh Makefile.in missing config.h.in
