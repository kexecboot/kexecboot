AC_PREREQ(2.53)
AC_INIT([kexecboot], [0.5.8], [kexecboot-devel@linuxtogo.org])
AM_INIT_AUTOMAKE([-Wall foreign])
AC_CONFIG_SRCDIR(kexecboot.c)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_ARG_ENABLE([static-linking],[AS_HELP_STRING([--enable-static-linking],[compiles kexecboot as a static executable @<:@default=no@:>@])],[],[enable_static_linking=no])
AC_ARG_ENABLE([fbui],[AS_HELP_STRING([--enable-fbui],[support framebuffer user interface @<:@default=yes@:>@])],[],[enable_fbui=yes])
AC_ARG_ENABLE([textui],[AS_HELP_STRING([--enable-textui],[support console text user interface @<:@default=no@:>@])],[],[enable_textui=no])
AC_ARG_ENABLE([cfgfiles],[AS_HELP_STRING([--enable-cfgfiles],[support config files @<:@default=yes@:>@])],[],[enable_cfgfiles=yes])
AC_ARG_ENABLE([icons],[AS_HELP_STRING([--enable-icons],[support custom icons (depends on fbui) @<:@default=yes@:>@])],[],[enable_icons=yes])
AC_ARG_ENABLE([zaurus],[AS_HELP_STRING([--enable-zaurus],[compile Sharp Zaurus specific code @<:@default=no@:>@])],[],[enable_zaurus=no])
AC_ARG_ENABLE([zimage],[AS_HELP_STRING([--enable-zimage],[compile with zImage support @<:@default=yes@:>@])],[],[enable_zimage=yes])
AC_ARG_ENABLE([uimage],[AS_HELP_STRING([--enable-uimage],[compile with uImage support @<:@default=no@:>@])],[],[enable_uimage=no])
AC_ARG_ENABLE([machine-kernel],[AS_HELP_STRING([--enable-machine-kernel],[enable looking for machine-specific zImage kernel @<:@default=no@:>@])],[],[enable_machine_kernel=no])
AC_ARG_ENABLE([devices-recreating],[AS_HELP_STRING([--enable-devices-recreating],[enable devices re-creating @<:@default=yes@:>@])], [],[enable_devices_recreating=yes])
AC_ARG_ENABLE([debug],[AS_HELP_STRING([--enable-debug],[enable debug output @<:@default=no@:>@])], [],[enable_debug=\"no\"])
AC_ARG_ENABLE([host-debug],[AS_HELP_STRING([--enable-host-debug],[allow for non-destructive executing of kexecboot on host system @<:@default=no@:>@])], [],[enable_host_debug=no])
AC_ARG_ENABLE([bg-buffer],[AS_HELP_STRING([--enable-bg-buffer],[enable special buffer to hold pre-drawed FB GUI background @<:@default=no@:>@])], [],[enable_bg_buffer=no])

AC_ARG_ENABLE([timeout],[AS_HELP_STRING([--enable-timeout@<:@=sec@:>@],[allow to boot 1st kernel after timeout in seconds @<:@default=no@:>@])], [
	test "x$enable_timeout" = xyes && enable_timeout=10
],[enable_timeout=no])

AC_ARG_ENABLE([delay],[AS_HELP_STRING([--enable-delay@<:@=sec@:>@],[specify delay before devices scanning @<:@default=1@:>@])], [
	test "x$enable_delay" = xyes && enable_delay=1
],[enable_delay=1])

AC_ARG_ENABLE([bpp], [AS_HELP_STRING([--enable-bpp@<:@=list@:>@],[enable support of specified bpp modes (all,32,24,18,16,4,2,1) @<:@default=all@:>@])],
[
	SIFS=${IFS}
	IFS=','
	for bpp in ${enable_bpp}
	do
		case ${bpp} in
			32) enable_32bpp=yes;;
			24) enable_24bpp=yes;;
			18) enable_18bpp=yes;;
			16) enable_16bpp=yes;;
			4) enable_4bpp=yes;;
			2) enable_2bpp=yes;;
			1) enable_1bpp=yes;;
			*) enable_all_bpp=yes;;
		esac
	done
	IFS=${SIFS}
],
[enable_all_bpp=yes])

AC_ARG_WITH([kexec-binary],[AS_HELP_STRING([--with-kexec-binary="path"],[look for kexec binary at path @<:@default="/usr/sbin/kexec"@:>@])],[
test "x$with_kexec_binary" = xyes && with_kexec_binary="/usr/sbin/kexec"
],[with_kexec_binary="/usr/sbin/kexec"])


AS_IF([test "x$enable_fbui" != xno],
		[
		AC_DEFINE([USE_FBMENU], [1], [Define if you use the framebuffer user interface])
		AS_IF([test "x$enable_icons" != xno],
			[
			AC_DEFINE([USE_ICONS], [1], [Define if you want to parse and use custom icons])
			],[])

		AS_IF([test "x$enable_bg_buffer" = xyes],
			[
			AC_DEFINE([USE_BG_BUFFER], [1], [Define if you want to use special buffer to hold pre-drawed background])
			],[])

		AS_IF([test "x$enable_32bpp" == xyes],
			[
			AC_DEFINE([USE_32BPP], [1], [Define if you want to support this bpp mode])
			],[])
		AS_IF([test "x$enable_24bpp" == xyes],
			[
			AC_DEFINE([USE_24BPP], [1], [Define if you want to support this bpp mode])
			],[])
		AS_IF([test "x$enable_18bpp" == xyes],
			[
			AC_DEFINE([USE_18BPP], [1], [Define if you want to support this bpp mode])
			],[])
		AS_IF([test "x$enable_16bpp" == xyes],
			[
			AC_DEFINE([USE_16BPP], [1], [Define if you want to support this bpp mode])
			],[])
		AS_IF([test "x$enable_4bpp" == xyes],
			[
			AC_DEFINE([USE_4BPP],  [1], [Define if you want to support this bpp mode])
			],[])
		AS_IF([test "x$enable_2bpp" == xyes],
			[
			AC_DEFINE([USE_2BPP],  [1], [Define if you want to support this bpp mode])
			],[])
		AS_IF([test "x$enable_1bpp" == xyes],
			[
			AC_DEFINE([USE_1BPP],  [1], [Define if you want to support this bpp mode])
			],[])
		AS_IF([test "x$enable_all_bpp" == xyes],
			[
			AC_DEFINE([USE_32BPP], [1], [Define if you want to support this bpp mode])
			AC_DEFINE([USE_24BPP], [1], [Define if you want to support this bpp mode])
			AC_DEFINE([USE_18BPP], [1], [Define if you want to support this bpp mode])
			AC_DEFINE([USE_16BPP], [1], [Define if you want to support this bpp mode])
			AC_DEFINE([USE_4BPP],  [1], [Define if you want to support this bpp mode])
			AC_DEFINE([USE_2BPP],  [1], [Define if you want to support this bpp mode])
			AC_DEFINE([USE_1BPP],  [1], [Define if you want to support this bpp mode])
			],[])
		],[])

AS_IF([test "x$enable_textui" = xyes],
		[
		AC_DEFINE([USE_TEXTUI], [1], [Define if you wish to enable text-mode UI support])
		], [])

AS_IF([test "x$enable_cfgfiles" != xno],
		[
		AC_DEFINE([USE_CFGFILES], [1],[Define if you want to parse cfgfiles])
		], [])

AS_IF([test "x$enable_static_linking" != xno],
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

AS_IF([test "x$enable_zaurus" = xyes],
		[
		AC_DEFINE([USE_ZAURUS], [1], [Define if you wish to compile Sharp Zaurus specific code])
		], [])

AS_IF([test "x$enable_zimage" = xyes],
		[
		AC_DEFINE([USE_ZIMAGE], [1], [Define if you wish to compile zImage kernel support])
		], [])

AS_IF([test "x$enable_uimage" = xyes],
		[
		AC_DEFINE([USE_UIMAGE], [1], [Define if you wish to compile uImage kernel support])
		], [])

AS_IF([test "x$enable_machine_kernel" = xyes],
		[
		AC_DEFINE([USE_MACHINE_KERNEL], [1], [Define if you wish to enable looking for machine-specific zImage kernel])
		], [])

AS_IF([test "x$enable_timeout" != xno],
		[
		AC_DEFINE_UNQUOTED([USE_TIMEOUT], [${enable_timeout}], [Define timeout in seconds to automatic boot 1st kernel after this time])
		], [])

AS_IF([test "x$enable_delay" != xno],
		[
		AC_DEFINE_UNQUOTED([USE_DELAY], [${enable_delay}], [Define delay to sleep before scanning devices])
		], [])

AS_IF([test "x$with_kexec_binary" != "xno"],
		[
		AC_DEFINE_UNQUOTED([KEXEC_PATH], ["${with_kexec_binary}"], [Where to look for kexec binary])
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
