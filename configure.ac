# Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
AC_PREREQ(2.63)

m4_define([cd_major_version], [0])
m4_define([cd_minor_version], [1])
m4_define([cd_micro_version], [33])
m4_define([cd_version],
          [cd_major_version.cd_minor_version.cd_micro_version])

AC_INIT([colord],[cd_version],[http://www.freedesktop.org/software/colord/])
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz tar-ustar])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_LIBTOOL

GOBJECT_INTROSPECTION_CHECK([0.9.8])
GLIB_GSETTINGS

# set up gtk-doc
GTK_DOC_CHECK(1.9)

# use this in cd-version.h
CD_MAJOR_VERSION=cd_major_version
CD_MINOR_VERSION=cd_minor_version
CD_MICRO_VERSION=cd_micro_version
AC_SUBST(VERSION)
AC_SUBST(CD_MAJOR_VERSION)
AC_SUBST(CD_MINOR_VERSION)
AC_SUBST(CD_MICRO_VERSION)

# libtool versioning - this applies to libcolord
#
# See http://sources.redhat.com/autobook/autobook/autobook_91.html#SEC91 for details
#
# increment;
# CURRENT	If the API or ABI interface has changed (reset REVISION to 0)
# REVISION	If the API and ABI remains the same, but bugs are fixed.
# AGE		Don't use.
LT_CURRENT=1
LT_REVISION=19
LT_AGE=0
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

# enable nice build output on automake1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AS_ALL_LINGUAS
AC_PROG_CC
AC_PROG_INSTALL
LT_INIT
AM_PROG_CC_C_O
IT_PROG_INTLTOOL([0.35.0])
AC_PATH_PROG(XSLTPROC, xsltproc)

dnl ---------------------------------------------------------------------------
dnl - Use strict options (default enabled for devs, disabled in releases)
dnl ---------------------------------------------------------------------------
if test -d ".git"; then
	default_strict=yes
else
	default_strict=no
fi

AC_ARG_ENABLE(strict, AS_HELP_STRING([--enable-strict],
				     [Enable strict compilation options]),
				     enable_strict=$enableval,
				     enable_strict=$default_strict)
if test x$enable_strict != xno; then
	CPPFLAGS="$CPPFLAGS -DG_DISABLE_SINGLE_INCLUDES"
	CPPFLAGS="$CPPFLAGS -DG_DISABLE_DEPRECATED"
	CPPFLAGS="$CPPFLAGS -DGSEAL_ENABLE"
fi

dnl ---------------------------------------------------------------------------
dnl - Extra verbose warning switches
dnl ---------------------------------------------------------------------------

if test "$GCC" = "yes"; then
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wall"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wcast-align -Wno-uninitialized"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-declarations"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wpointer-arith"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wcast-align"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wwrite-strings"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Winit-self"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wreturn-type"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wformat-nonliteral"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wformat-security"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-include-dirs"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-format-attribute"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wclobbered"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wempty-body"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wignored-qualifiers"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wsign-compare"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wtype-limits"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wuninitialized"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Waggregate-return"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wdeclaration-after-statement"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wshadow"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wno-strict-aliasing"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Winline"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-parameter-type"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Woverride-init"
else
	WARNINGFLAGS_C=""
fi
AC_SUBST(WARNINGFLAGS_C)

dnl ---------------------------------------------------------------------------
dnl - gettext stuff
dnl ---------------------------------------------------------------------------
AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE(GETTEXT_PACKAGE, "AC_PACKAGE_NAME", [foo])

LT_LIB_M
AC_SUBST(LIBM)

GLIB_GSETTINGS

dnl ---------------------------------------------------------------------------
dnl - Check library dependencies
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.32.0 gobject-2.0 gthread-2.0 gio-2.0 >= 2.25.9 gio-unix-2.0 gmodule-2.0)
AC_ARG_ENABLE(gusb, AS_HELP_STRING([--enable-gusb],[Enable GUSB support]),
	      enable_gusb=$enableval, enable_gusb=yes)
if test x$enable_gusb != xno; then
	PKG_CHECK_MODULES(GUSB, gusb >= 0.1.1)
	AC_DEFINE(HAVE_GUSB,1,[whether gusb is available])
fi
AM_CONDITIONAL(HAVE_GUSB, test x$enable_gusb = xyes)

PKG_CHECK_MODULES(LCMS, lcms2 >= 2.2)
AC_CHECK_LIB(lcms2, cmsGetHeaderCreator, have_lcms_creator=yes)
if test x$have_lcms_creator != x; then
	AC_DEFINE(HAVE_LCMS_GET_HEADER_CREATOR,1,[cmsGetHeaderCreator is available])
fi
AC_CHECK_LIB(lcms2, cmsGetToneCurveEstimatedTableEntries, have_lcms_table_entries=yes)
if test x$have_lcms_table_entries != x; then
	AC_DEFINE(HAVE_LCMS_GET_TABLE_ENTRIES,1,[cmsGetToneCurveEstimatedTableEntries is available])
fi

PKG_CHECK_MODULES(SQLITE, sqlite3)

dnl **** Check for GUDEV ****
AC_ARG_ENABLE(gudev, AS_HELP_STRING([--enable-gudev],[Enable GUDEV support]),
	      enable_gudev=$enableval, enable_gudev=yes)
if test x$enable_gudev != xno; then
	PKG_CHECK_MODULES(GUDEV, gudev-1.0)
	AC_DEFINE(HAVE_GUDEV,1,[Use GUDEV support for hardware])
fi
AM_CONDITIONAL(HAVE_GUDEV, test x$enable_gudev = xyes)

dnl ---------------------------------------------------------------------------
dnl - Is docbook2man available?
dnl ---------------------------------------------------------------------------
AC_PATH_PROG(DOCBOOK2MAN, docbook2man, no)
if test "$DOCBOOK2MAN" = "no" ; then
	AC_MSG_WARN([docbook2man not found, will not be able to build man documentation (if tarball, pre-generated documentation will be installed)])
fi
AM_CONDITIONAL(HAVE_DOCBOOK2MAN, [test "$DOCBOOK2MAN" != "no"])

dnl ---------------------------------------------------------------------------
dnl - Build PolicyKit code
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(polkit, AS_HELP_STRING([--enable-polkit],[enable PolicyKit]),
	      enable_polkit=$enableval,enable_polkit=yes)
AM_CONDITIONAL(CD_BUILD_POLKIT, test x$enable_polkit = xyes)
if test x$enable_polkit = xyes; then
	PKG_CHECK_MODULES(POLKIT, polkit-gobject-1 >= 0.103)
	AC_DEFINE(USE_POLKIT, 1, [if we should use PolicyKit])
fi

dnl ---------------------------------------------------------------------------
dnl - Select whether and where to install systemd system service files
dnl ---------------------------------------------------------------------------

AC_ARG_WITH([systemdsystemunitdir],
            AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
            [],
            [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
if test "x$with_systemdsystemunitdir" != "xno"; then
	AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$systemdsystemunitdir"])



dnl ---------------------------------------------------------------------------
dnl - Use libsystemd-login to track the process seat
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(systemd-login, AS_HELP_STRING([--enable-systemd-login],[Build systemd-login support]),
	      enable_systemd_login=$enableval, enable_systemd_login=yes)
AM_CONDITIONAL(HAVE_LIBSYSTEMD_LOGIN, test x$enable_systemd_login = xyes)
if test x$enable_systemd_login != xno; then
	PKG_CHECK_MODULES(LIBSYSTEMD_LOGIN,
			  [libsystemd-login >= 44])
	AC_DEFINE([HAVE_LIBSYSTEMD_LOGIN], 1, [Define to 1 if libsystemd-login is available])
fi
AC_SUBST(HAVE_LIBSYSTEMD_LOGIN)
AC_SUBST(LIBSYSTEMD_LOGIN_CFLAGS)
AC_SUBST(LIBSYSTEMD_LOGIN_LIBS)

dnl ---------------------------------------------------------------------------
dnl - Where to install udev rules
dnl ---------------------------------------------------------------------------
AC_ARG_WITH([udevrulesdir],
            AS_HELP_STRING([--with-udevrulesdir=DIR], [Directory for udev rules files]),
            [],
            [with_udevrulesdir=$($PKG_CONFIG --variable=udevdir udev)/rules.d])
AC_SUBST([udevrulesdir], [$with_udevrulesdir])

dnl ---------------------------------------------------------------------------
dnl - Build DBus examples
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(examples, AS_HELP_STRING([--enable-examples],[enable DBus example code]),
	      enable_examples=$enableval,enable_examples=yes)
AM_CONDITIONAL(CD_BUILD_EXAMPLES, test x$enable_examples = xyes)
PKG_CHECK_MODULES(DBUS, dbus-1, have_dbus=yes, have_dbus=no)
if test x$enable_examples = xyes -a x$have_dbus = xno ; then
	AC_MSG_ERROR([DBus examples require dbus-1 package, not found])
fi

dnl ---------------------------------------------------------------------------
dnl - Build reverse engineering
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(reverse, AS_HELP_STRING([--enable-reverse],[enable reverse engineering tools]),
	      enable_reverse=$enableval,enable_reverse=no)
AM_CONDITIONAL(CD_BUILD_REVERSE, test x$enable_reverse = xyes)

dnl **** Check for SANE ****
AC_ARG_ENABLE(sane, AS_HELP_STRING([--enable-sane],[Enable SANE support]),
	      enable_sane=$enableval,enable_sane=no)
has_sane=no
if test x$enable_sane != xno; then
	PKG_CHECK_MODULES(SANE, sane-backends, has_sane=yes, has_sane=no)
	if test $has_sane = "no"; then
		# fall back to detecting the header for some distros
		AC_CHECK_HEADER(sane/sane.h, has_sane=yes, has_sane=no)
		if test $has_sane = "yes"; then
			SANE_CFLAGS=""
			SANE_LIBS="-lsane"
			AC_SUBST(SANE_CFLAGS)
			AC_SUBST(SANE_LIBS)
		fi
		if test x$enable_sane = xyes -a $has_sane = "no"; then
			AC_MSG_ERROR([--enable-sane was specified and SANE was not found])
		fi

	fi
	if test x$enable_gudev != xyes; then
		has_sane=no
		if test x$enable_sane = xyes; then
			AC_MSG_ERROR([--enable-sane was specified and SANE requires gudev support, not found])
		fi
		AC_MSG_WARN([gudev not found, SANE support will not be enabled])
	fi
	if test x$have_dbus != xyes; then
		has_sane=no
		if test x$enable_sane = xyes; then
			AC_MSG_ERROR([--enable-sane was specified and SANE requires dbus support, not found])
		fi
		AC_MSG_WARN([dbus not found, SANE support will not be enabled])
	fi
fi
AM_CONDITIONAL(HAVE_SANE, test x$has_sane = xyes)
if test x$has_sane = xyes; then
	AC_DEFINE(HAVE_SANE,1,[Use SANE support for detecting scanners])
fi

dnl **** Daemon is allowed to do FD fallback  ****
AC_ARG_ENABLE(fd_fallback, AS_HELP_STRING([--enable-fd-fallback],[Enable file descriptor fallback]),
	      enable_fd_fallback=$enableval, enable_fd_fallback=yes)
if test x$enable_fd_fallback != xno; then
	AC_DEFINE(HAVE_FD_FALLBACK,1,[Use FD fallback])
fi

dnl **** Daemon is allowed to do search external volumes for profiles ****
AC_ARG_ENABLE(volume_search, AS_HELP_STRING([--enable-volume-search],[Enable profile volume search]),
	      enable_volume_search=$enableval, enable_volume_search=yes)
tmp="false"
if test x$enable_volume_search != xno; then
	tmp="true"
fi
AC_DEFINE_UNQUOTED(ENABLE_VOLUME_SEARCH, "$tmp", [enable volume search])
AC_SUBST(ENABLE_VOLUME_SEARCH, "$tmp")

# custom daemon user
AC_ARG_WITH(
    [daemon_user],
    AC_HELP_STRING([--with-daemon-user],
                   [User for running the colord daemon (root)]),
    [daemon_user=$withval],
    [daemon_user=root]
)
AC_SUBST(daemon_user)
AC_DEFINE_UNQUOTED([DAEMON_USER], ["$daemon_user"], [Daemon user])

dnl ---------------------------------------------------------------------------
dnl - Build VALA support
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(vala, AS_HELP_STRING([--enable-vala],[build vala bindings]),
	      enable_vala=$enableval,enable_vala=no)
if test x$found_introspection != xno -a x$enable_vala != xno; then
	AC_PATH_PROG([VAPIGEN], [vapigen], [])
	if test "x$VAPIGEN" = "x"; then
		has_vapigen="no"
		AC_MSG_WARN([vapigen not found, will not build Vala binding])
	else
		has_vapigen="yes"
	fi
else
	has_vapigen="no"
	if test x$found_introspection = xno; then
		AC_MSG_WARN([introspection not found, will not build Vala binding])
	fi
fi
AM_CONDITIONAL(HAVE_VAPIGEN, [test "x$has_vapigen" = "xyes"])

dnl ---------------------------------------------------------------------------
dnl - Build session helper example
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(session-example, AS_HELP_STRING([--enable-session-example],[Enable session example]),
	      enable_session_example=$enableval, enable_session_example=no)
AM_CONDITIONAL(CD_BUILD_SESSION_EXAMPLE, test x$enable_session_example = xyes)
if test x$enable_session_example != xno; then
	PKG_CHECK_MODULES(GNOME_DESKTOP, gnome-desktop-3.0)
	PKG_CHECK_MODULES(COLORD_GTK, colord-gtk >= 0.1.24)
fi

dnl ---------------------------------------------------------------------------
dnl - Build extra print profiles (requires argyllcms)
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(print-profiles, AS_HELP_STRING([--enable-print-profiles],[Build extra print profiles]),
	      enable_print_profiles=$enableval, enable_print_profiles=no)
AM_CONDITIONAL(CD_BUILD_PRINT_PROFILES, test x$enable_print_profiles = xyes)
if test x$enable_print_profiles != xno; then
	AC_PATH_PROG(COLPROF, [colprof], false)
	if test "$COLPROF" = "false"; then
		AC_MSG_ERROR([colprof not found, please install argyllcms])
	fi
	AC_DEFINE_UNQUOTED([TOOL_COLPROF], ["$COLPROF"], [Full path of colprof binary])
fi

# per-machine directory
AC_SUBST(CD_SYSTEM_PROFILES_DIR, "\$(localstatedir)/lib/colord/icc")

dnl ---------------------------------------------------------------------------
dnl - Makefiles, etc.
dnl ---------------------------------------------------------------------------
AC_CONFIG_FILES([
Makefile
etc/Makefile
contrib/Makefile
contrib/session-helper/Makefile
contrib/colord-sane/Makefile
examples/Makefile
man/Makefile
doc/Makefile
doc/api/Makefile
doc/api/dbus/Makefile
doc/api/version.xml
data/Makefile
data/figures/Makefile
data/profiles/Makefile
data/ti1/Makefile
data/org.freedesktop.ColorManager.conf
data/tests/Makefile
policy/Makefile
rules/Makefile
rules/69-cd-sensors.rules
po/Makefile.in
src/Makefile
src/plugins/Makefile
src/sensors/Makefile
client/Makefile
lib/Makefile
lib/colord/cd-version.h
lib/colord/colord.pc
lib/colord/Makefile
lib/colorhug/Makefile
lib/colorhug/colorhug.pc
lib/colorhug/ch-version.h
lib/dtp94/Makefile
lib/huey/Makefile
lib/munki/Makefile
])
AC_OUTPUT

dnl ==========================================================================
echo "
                    colord $VERSION
                  ===================

        prefix:                    ${prefix}
        datadir:                   ${datadir}
        compiler:                  ${CC}
        cflags:                    ${CFLAGS}
        cppflags:                  ${CPPFLAGS}
        gobject-introspection:     ${found_introspection}
        PolicyKit support:         ${enable_polkit}
        Reverse engineering tools: ${enable_reverse}
        File descriptor fallback:  ${enable_fd_fallback}
        External volume searching: ${enable_volume_search}
        SANE support:              ${has_sane}
        GUdev support:             ${enable_gudev}
        GUsb support:              ${enable_gusb}
        Extra print profiles:      ${enable_print_profiles}
        Vala API generator:        ${has_vapigen}
        Daemon user:               ${daemon_user}
        udev rules.d dir:          ${with_udevrulesdir}
        using libsystemd-login:    ${enable_systemd_login}
        systemd service dir:       ${with_systemdsystemunitdir}
"


# warn that dummy is basically broken
if test x$enable_polkit = xno; then
	echo "*******************************************************************"
	echo "** YOU ARE NOT USING A SECURE DAEMON. ALL USERS CAN DO ANYTHING! **"
	echo "*******************************************************************"
fi

# warn that using the default root user isn't always a good idea
if test x$daemon_user = xroot; then
	echo "*******************************************************************"
	echo "** RUNNING THIS AS root ISN'T A GOOD IDEA SEE --with-daemon-user **"
	echo "*******************************************************************"
fi
