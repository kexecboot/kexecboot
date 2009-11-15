AC_PREREQ(2.53)
AC_INIT([kexecboot], [0.5], [thommycheck@gmx.de])
AM_INIT_AUTOMAKE([-Wall foreign])
AC_CONFIG_SRCDIR(kexecboot.c)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_ARG_WITH([static-linking],[AS_HELP_STRING([--with-static-linking],[compiles kexecboot as a static executable @<:@default=no@:>@])],[],[with_static_linking=no])
AC_ARG_WITH([fbmenu],[AS_HELP_STRING([--with-fbmenu],[support framebuffer menu @<:@default=yes@:>@])],[],[with_fbmenu=yes])
AC_ARG_WITH([cfgfiles],[AS_HELP_STRING([--with-cfgfiles],[support config files @<:@default=yes@:>@])],[],[with_cfgfiles=yes])
AC_ARG_WITH([icons],[AS_HELP_STRING([--with-icons],[support custom icons (depends on fbmenu) @<:@default=yes@:>@])],[],[with_icons=yes])
AC_ARG_WITH([zaurus],[AS_HELP_STRING([--with-zaurus],[compile Sharp Zaurus specific code @<:@default=yes@:>@])],[],[with_zaurus=yes])
AC_ARG_ENABLE([devices-recreating],[AS_HELP_STRING([--enable-devices-recreating],[enable devices re-creating @<:@default=yes@:>@])], [],[enable_devices_recreating=\"yes\"])
AC_ARG_ENABLE([debug],[AS_HELP_STRING([--enable-debug],[enable debug output @<:@default=no@:>@])], [],[enable_debug=\"no\"])
AC_ARG_ENABLE([host-debug],[AS_HELP_STRING([--enable-host-debug],[allow for non-destructive executing of kexecboot on host system@<:@default=no@:>@])], [],[enable_host_debug=\"no\"])

AS_IF([test "x$with_fbmenu" != xno],
		[
		AC_DEFINE([USE_FBMENU], [1],[Define if you use the framebuffer menu])
		AS_IF([test "x$with_icons" != xno],
			[
			AC_DEFINE([USE_ICONS], [1],[Define if you want to parse and use custom icons])
			],[])
		],[])

AS_IF([test "x$with_cfgfiles" != xno],
		[
		AC_DEFINE([USE_CFGFILES], [1],[Define if you want to parse cfgfiles])
		], [])

AS_IF([test "x$with_static_linking" != xno],
		[
		GCC_FLAGS="$GCC_FLAGS -static"
		], [])

AS_IF([test "x$enable_debug" = xyes],
		[
		GCC_FLAGS="$GCC_FLAGS -DDEBUG -O0 -g"
		], [])

AS_IF([test "x$enable_devices_recreating" != xno],
		[
		AC_DEFINE([USE_DEVICES_RECREATING], [1], [Define to enable devices re-creating])
		], [])

AS_IF([test "x$enable_host_debug" = xyes],
		[
		AC_DEFINE([USE_HOST_DEBUG], [1], [Define if you wish to debug kexecboot right on host system])
		], [])

AS_IF([test "x$with_zaurus" = xyes],
		[
		AC_DEFINE([USE_ZAURUS], [1], [Define if you wish to compile Sharp Zaurus specific code])
		], [])

AC_ISC_POSIX
AC_PROG_CC
AC_STDC_HEADERS

if test "x$GCC" = "xyes"; then
        GCC_FLAGS="$GCC_FLAGS -Wall"
fi

AC_SUBST(GCC_FLAGS)

AC_OUTPUT([
Makefile
])
