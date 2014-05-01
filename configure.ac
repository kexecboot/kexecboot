AC_PREREQ(2.53)
AC_INIT([kexecboot], [0.6], [kexecboot-devel@linuxtogo.org])
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([-Wall foreign] [subdir-objects])
AC_CONFIG_SRCDIR(kexecboot.c)
AC_CONFIG_HEADERS(config.h)
AM_MAINTAINER_MODE

AC_ARG_ENABLE([static-linking],[AS_HELP_STRING([--enable-static-linking],[compiles kexecboot as a static executable @<:@default=no@:>@])],[],[enable_static_linking=no])
AC_ARG_ENABLE([fbui],[AS_HELP_STRING([--enable-fbui],[support framebuffer user interface @<:@default=yes@:>@])],[],[enable_fbui=yes])
AC_ARG_ENABLE([fbui-width],[AS_HELP_STRING([--enable-fbui-width],[limit FB UI width to specified value @<:@default=no@:>@])],[],[enable_fbui_width=no])
AC_ARG_ENABLE([fbui-height],[AS_HELP_STRING([--enable-fbui-height],[limit FB UI height to specified value @<:@default=no@:>@])],[],[enable_fbui_height=no])
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
AC_ARG_ENABLE([numkeys],[AS_HELP_STRING([--enable-numkeys],[allow to choose menu item by 0-9 keys @<:@default=yes@:>@])], [],[enable_numkeys=yes])
AC_ARG_ENABLE([devtmpfs],[AS_HELP_STRING([--enable-devtmpfs],[mount devtmpfs at startup in init-mode @<:@default=yes@:>@])], [],[enable_devtmpfs=yes])

AC_ARG_ENABLE([timeout],[AS_HELP_STRING([--enable-timeout@<:@=sec@:>@],[allow to boot 1st kernel after timeout in seconds @<:@default=no@:>@])], [
	test "x$enable_timeout" = xyes && enable_timeout=10
],[enable_timeout=no])

AC_ARG_ENABLE([delay],[AS_HELP_STRING([--enable-delay@<:@=sec@:>@],[specify delay before devices scanning @<:@default=1@:>@])], [
	test "x$enable_delay" = xyes && enable_delay=1
],[enable_delay=1])

AC_ARG_ENABLE([bpp], [AS_HELP_STRING([--enable-bpp@<:@=list@:>@],[enable support of specified bpp modes (all,32,24,18,16) @<:@default=all@:>@])],
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
			*) enable_all_bpp=yes;;
		esac
	done
	IFS=${SIFS}
],
[enable_all_bpp=yes])

AC_ARG_ENABLE([fb-transfer-width], [AS_HELP_STRING([--enable-fb-transfer-width@<:@=bits@:>@],[choose RAM to framebuffer transfer width (32/16/8 bits) @<:@default=32@:>@])],
[
	case ${enable_fb_transfer_width} in
		32 | 16 | 8) ;;
		*) echo "ERROR: $enable_fb_transfer_width is wrong value for --enable-fb-transfer-width!"; exit 1;;
	esac
],
[enable_fb_transfer_width=32])

AC_ARG_ENABLE([evdev-rate],[AS_HELP_STRING([--enable-evdev-rate@<:@=first_delay,repeat_delay@:>@],[change evdev (keyboard/mouse) repeat rate (in milliseconds) @<:@default=no@:>@])], [
	test "x$enable_evdev_rate" = xyes && enable_evdev_rate="1000,250"
],[enable_evdev_rate=no])


AC_ARG_WITH([kexec-binary],[AS_HELP_STRING([--with-kexec-binary="path"],[look for kexec binary at path @<:@default="/usr/sbin/kexec"@:>@])],[
test "x$with_kexec_binary" = xyes && with_kexec_binary="/usr/sbin/kexec"
],[with_kexec_binary="/usr/sbin/kexec"])

AC_ARG_WITH([ubiattach-binary],[AS_HELP_STRING([--with-ubiattach-binary="path"],[look for ubiattach binary at path @<:@default="/usr/sbin/ubiattach"@:>@])],[
test "x$with_ubiattach_binary" = xyes && with_ubiattach_binary="/usr/sbin/ubiattach"
],[with_ubiattach_binary="/usr/sbin/ubiattach"])

AC_ARG_ENABLE([ubi-vid-hdr-offset],[AS_HELP_STRING([--enable-ubi-vid-hdr-offset@<:@=bytes@:>@],[UBI VID header offset @<:@default=no@:>@])], [],[enable_ubi_vid_hdr_offset=no])


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

		AS_IF([test "x$enable_fbui_width" != xno],
			[
			AC_DEFINE_UNQUOTED([USE_FBUI_WIDTH], [${enable_fbui_width}], [Define if you want to limit FB UI width to specified value])
			],[])
		AS_IF([test "x$enable_fbui_height" != xno],
			[
			AC_DEFINE_UNQUOTED([USE_FBUI_HEIGHT], [${enable_fbui_height}], [Define if you want to limit FB UI height to specified value])
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
		AS_IF([test "x$enable_all_bpp" == xyes],
			[
			AC_DEFINE([USE_32BPP], [1], [Define if you want to support this bpp mode])
			AC_DEFINE([USE_24BPP], [1], [Define if you want to support this bpp mode])
			AC_DEFINE([USE_18BPP], [1], [Define if you want to support this bpp mode])
			AC_DEFINE([USE_16BPP], [1], [Define if you want to support this bpp mode])
			],[])

		AS_IF([test "x$enable_fb_transfer_width" == x32],
			[
			AC_DEFINE([USE_FB_TRANS_TYPE], [uint32_t], [Data type for RAM-to-FB transfers])
			AC_DEFINE([USE_FB_TRANS_LENGTH(x)], [(x >> 2)], [How to change data length for RAM-to-FB transfers])
			],[])
		AS_IF([test "x$enable_fb_transfer_width" == x16],
			[
			AC_DEFINE([USE_FB_TRANS_TYPE], [uint16_t], [Data type for RAM-to-FB transfers])
			AC_DEFINE([USE_FB_TRANS_LENGTH(x)], [(x >> 1)], [How to change data length for RAM-to-FB transfers])
			],[])
		AS_IF([test "x$enable_fb_transfer_width" == x8],
			[
			AC_DEFINE([USE_FB_TRANS_TYPE], [uint8_t], [Data type for RAM-to-FB transfers])
			AC_DEFINE([USE_FB_TRANS_LENGTH(x)], [(x)], [How to change data length for RAM-to-FB transfers])
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

AS_IF([test "x$with_ubiattach_binary" != "xno"],
		[
		AC_DEFINE_UNQUOTED([UBIATTACH_PATH], ["${with_ubiattach_binary}"], [Where to look for ubiattach binary])
		], [])

AS_IF([test "x$enable_ubi_vid_hdr_offset" != "xno"],
		[
		AC_DEFINE_UNQUOTED([UBI_VID_HDR_OFFSET], ["${enable_ubi_vid_hdr_offset}"], [UBI VID header offset])
		], [])

AS_IF([test "x$enable_evdev_rate" != xno],
		[
		AC_DEFINE_UNQUOTED([USE_EVDEV_RATE], [${enable_evdev_rate}], [Define evdev (keyboard/mouse) repeat rate to use in milliseconds (first_delay, repeat_delay)])
		], [])

AS_IF([test "x$enable_numkeys" = xyes],
		[
		AC_DEFINE([USE_NUMKEYS], [1], [Define if you wish to allow to choose menu items by 0-9 keys])
		], [])

AS_IF([test "x$enable_devtmpfs" = xyes],
		[
		AC_DEFINE([USE_DEVTMPFS], [1], [Define if you wish to mount devtmpfs at startup in init-mode])
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
