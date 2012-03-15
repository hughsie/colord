# Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
AC_PREREQ(2.63)

m4_define([cd_major_version], [0])
m4_define([cd_minor_version], [1])
m4_define([cd_micro_version], [18])
m4_define([cd_version],
          [cd_major_version.cd_minor_version.cd_micro_version])

AC_INIT([colord],[cd_version],[http://hughsie.com])
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz tar-ustar])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_LIBTOOL

GOBJECT_INTROSPECTION_CHECK([0.9.8])

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
LT_REVISION=10
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
	CPPFLAGS="$CPPFLAGS -DG_DISABLE_SINGLE_INCLUDES -DGTK_DISABLE_SINGLE_INCLUDES"
	CPPFLAGS="$CPPFLAGS -DG_DISABLE_DEPRECATED"
	CPPFLAGS="$CPPFLAGS -DGSEAL_ENABLE"
	if test "$GCC" = "yes"; then
		CPPFLAGS="$CPPFLAGS -Werror"
	fi
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

GLIB_GSETTINGS

dnl ---------------------------------------------------------------------------
dnl - Check library dependencies
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.28.0 gobject-2.0 gthread-2.0 gio-2.0 >= 2.25.9 gio-unix-2.0 gmodule-2.0)
PKG_CHECK_MODULES(GUSB, gusb >= 0.1.1, have_gusb=yes, have_gusb=no)
if test x$have_gusb = xyes; then
	AC_DEFINE(HAVE_GUSB,1,[whether gusb is available])
fi
AM_CONDITIONAL(HAVE_GUSB, test x$have_gusb = xyes)

PKG_CHECK_MODULES(LCMS, lcms2 >= 2.2)
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
AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$with_systemdsystemunitdir"])

dnl ---------------------------------------------------------------------------
dnl - Build DBus examples
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(examples, AS_HELP_STRING([--enable-examples],[enable DBus example code]),
	      enable_examples=$enableval,enable_examples=yes)
AM_CONDITIONAL(CD_BUILD_EXAMPLES, test x$enable_examples = xyes)
if test x$enable_examples = xyes; then
	PKG_CHECK_MODULES(DBUS, dbus-1)
fi

dnl ---------------------------------------------------------------------------
dnl - Build reverse engineering
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(reverse, AS_HELP_STRING([--enable-reverse],[enable reverse engineering tools]),
	      enable_reverse=$enableval,enable_reverse=no)
AM_CONDITIONAL(CD_BUILD_REVERSE, test x$enable_reverse = xyes)

dnl **** Check for SANE ****
AC_ARG_ENABLE(sane, AS_HELP_STRING([--enable-sane],[Enable SANE support]),
	      enable_sane=$enableval)
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
if test x$found_introspection != xno; then
	AC_PATH_PROG([VAPIGEN], [vapigen], [])
	if test "x$VAPIGEN" = "x"; then
		has_vapigen="no"
		AC_MSG_WARN([vapigen not found, will not build Vala binding])
	else
		has_vapigen="yes"
	fi
else
	has_vapigen="no"
	AC_MSG_WARN([Not introspection found, will not build Vala binding])
fi
AM_CONDITIONAL(HAVE_VAPIGEN, [test "x$has_vapigen" = "xyes"])

# udev rules go in /lib, not /usr/lib
if test "$prefix" = "/usr" -o "$prefix" = "/usr/local" ; then
  slashlibdir=/lib
else
  slashlibdir=$prefix/lib
fi
AC_SUBST(slashlibdir)

# per-machine directory
AC_SUBST(CD_SYSTEM_PROFILES_DIR, "\$(localstatedir)/lib/colord/icc")

dnl ---------------------------------------------------------------------------
dnl - Makefiles, etc.
dnl ---------------------------------------------------------------------------
AC_CONFIG_FILES([
Makefile
etc/Makefile
contrib/Makefile
contrib/colord-sane/Makefile
contrib/colord-sane/org.freedesktop.colord-sane.conf
examples/Makefile
man/Makefile
doc/Makefile
doc/api/Makefile
doc/api/dbus/Makefile
doc/api/version.xml
data/Makefile
data/org.freedesktop.ColorManager.conf
data/tests/Makefile
policy/Makefile
rules/Makefile
rules/69-cd-sensors.rules
po/Makefile.in
src/Makefile
src/sensors/Makefile
client/Makefile
libcolord/cd-version.h
libcolord/colord.pc
libcolord/Makefile
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
        GUDEV support:             ${enable_gudev}
        LCMS2 DICT support:        ${has_new_lcms}
        Vala API generator:        ${has_vapigen}
        Daemon user:               ${daemon_user}
"


# warn that dummy is basically broken
if test x$enable_polkit = xno; then
	echo "*******************************************************************"
	echo "** YOU ARE NOT USING A SECURE DAEMON. ALL USERS CAN DO ANYTHING! **"
	echo "*******************************************************************"
fi
