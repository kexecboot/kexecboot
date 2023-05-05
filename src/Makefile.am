bin_PROGRAMS=kexecboot

kexecboot_CFLAGS = -I$(top_srcdir) $(AM_CFLAGS) -I$(shell $(CC) -print-file-name=include)

kexecboot_SOURCES = \
	global.c \
	util.c \
	cfgparser.c \
	devicescan.c \
	evdevs.c \
	fb.c \
	gui.c \
	menu.c \
	xpm.c \
	rgb.c \
	tui.c \
	kexecboot.c \
	fstype/fstype.c
