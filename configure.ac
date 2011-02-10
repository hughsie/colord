# Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
AC_PREREQ(2.63)

m4_define([cd_major_version], [0])
m4_define([cd_minor_version], [1])
m4_define([cd_micro_version], [3])
m4_define([cd_version],
          [cd_major_version.cd_minor_version.cd_micro_version])

AC_INIT([colord],[cd_version],[http://hughsie.com])
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE([1.9 tar-ustar dist-bzip2])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

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
LT_REVISION=1
LT_AGE=0
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

# enable nice build output on automake1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

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
GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE(GETTEXT_PACKAGE, "AC_PACKAGE_NAME", [foo])

AM_GLIB_GNU_GETTEXT
GLIB_GSETTINGS

dnl ---------------------------------------------------------------------------
dnl - Check library dependencies
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.14.0 gobject-2.0 gthread-2.0 gio-2.0 >= 2.25.9)
PKG_CHECK_MODULES(LCMS, lcms2)
PKG_CHECK_MODULES(SQLITE, sqlite3)
PKG_CHECK_MODULES(GUDEV, gudev-1.0)

dnl ---------------------------------------------------------------------------
dnl - Is docbook2man available?
dnl ---------------------------------------------------------------------------
AC_PATH_PROG(DOCBOOK2MAN, docbook2man, no)
if test "$DOCBOOK2MAN" = "no" ; then
	AC_MSG_WARN([docbook2man not found, will not be able to build man documentation])
fi
AM_CONDITIONAL(HAVE_DOCBOOK2MAN, [test "$DOCBOOK2MAN" != "no"])

dnl ---------------------------------------------------------------------------
dnl - Build PolicyKit code
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(polkit, AS_HELP_STRING([--enable-polkit],[enable PolicyKit]),
	      enable_polkit=$enableval,enable_polkit=yes)
AM_CONDITIONAL(CD_BUILD_POLKIT, test x$enable_polkit = xyes)
if test x$enable_polkit = xyes; then
	PKG_CHECK_MODULES(POLKIT, polkit-gobject-1 >= 0.97)
	AC_DEFINE(USE_POLKIT, 1, [if we should use PolicyKit])
fi

dnl ---------------------------------------------------------------------------
dnl - Build GUI gui
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(gui, AS_HELP_STRING([--enable-gui],[enable GUI gui]),
	      enable_gui=$enableval,enable_gui=yes)
AM_CONDITIONAL(CD_BUILD_GUI, test x$enable_gui = xyes)
if test x$enable_gui = xyes; then
	PKG_CHECK_MODULES(GTK, gtk+-3.0 >= 2.91.5)
fi

dnl **** Check for SANE ****
AC_ARG_ENABLE(sane, AS_HELP_STRING([--enable-sane],[Enable SANE support]),
	      enable_sane=$enableval, enable_sane=yes)
if test x$enable_sane != xno; then
	PKG_CHECK_MODULES(SANE, sane-backends, has_sane=yes, has_sane=no)
	if test $has_sane = "no"; then
		AC_CHECK_HEADER(sane/sane.h, has_sane=yes, has_sane=no)
		if test $has_sane = "no"; then
			AC_MSG_ERROR([Cannot fine SANE])
		fi
		SANE_CFLAGS=""
		SANE_LIBS="-lsane"
		AC_SUBST(SANE_CFLAGS)
		AC_SUBST(SANE_LIBS)
	fi
fi
AM_CONDITIONAL(HAVE_SANE, test x$enable_sane = xyes)
if test x$enable_sane = xyes; then
	AC_DEFINE(HAVE_SANE,1,[Use SANE support for detecting scanners])
fi

# udev rules go in /lib, not /usr/lib
if test "$prefix" = "/usr" -o "$prefix" = "/usr/local" ; then
  slashlibdir=/lib
else
  slashlibdir=$prefix/lib
fi
AC_SUBST(slashlibdir)

# per-machine directory
AC_SUBST(CD_SYSTEM_PROFILES_DIR, "\$(localstatedir)/lib/color")

dnl ---------------------------------------------------------------------------
dnl - Makefiles, etc.
dnl ---------------------------------------------------------------------------
AC_CONFIG_FILES([
Makefile
etc/Makefile
man/Makefile
doc/Makefile
doc/api/Makefile
doc/api/dbus/Makefile
doc/api/version.xml
data/Makefile
data/tests/Makefile
policy/Makefile
rules/Makefile
po/Makefile.in
src/Makefile
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
        PolicyKit support:         ${enable_polkit}
        GUI support:               ${enable_gui}
        SANE support:              ${enable_sane}
"


# warn that dummy is basically broken
if test x$enable_polkit = xno; then
	echo "*******************************************************************"
	echo "** YOU ARE NOT USING A SECURE DAEMON. ALL USERS CAN DO ANYTHING! **"
	echo "*******************************************************************"
fi
