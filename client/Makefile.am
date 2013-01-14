AM_CPPFLAGS =						\
	$(GLIB_CFLAGS)					\
	$(GTK_CFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/libcolord			\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DCD_COMPILATION				\
	-DCD_DISABLE_DEPRECATED				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DVERSION="\"$(VERSION)\""			\
	-DLOCALEDIR=\""$(localedir)"\"

COLORD_LIBS =						\
	$(top_builddir)/libcolord/libcolord.la

noinst_LIBRARIES = libclientprivate.a
libclientprivate_a_SOURCES =				\
	cd-common.c					\
	cd-common.h					\
	cd-lcms-helpers.c				\
	cd-lcms-helpers.h

bin_PROGRAMS =						\
	colormgr					\
	cd-create-profile				\
	cd-fix-profile

colormgr_SOURCES =					\
	cd-util.c

colormgr_LDADD =					\
	libclientprivate.a				\
	$(COLORD_LIBS)					\
	$(GLIB_LIBS)

colormgr_CFLAGS =					\
	$(WARNINGFLAGS_C)

cd_fix_profile_SOURCES =				\
	cd-fix-profile.c

cd_fix_profile_LDADD =					\
	libclientprivate.a				\
	$(GLIB_LIBS)					\
	$(LCMS_LIBS)					\
	$(COLORD_LIBS)					\
	-lm

cd_fix_profile_CFLAGS =					\
	$(WARNINGFLAGS_C)

cd_create_profile_SOURCES =				\
	cd-create-profile.c

cd_create_profile_LDADD =				\
	libclientprivate.a				\
	$(GLIB_LIBS)					\
	$(LCMS_LIBS)					\
	$(COLORD_LIBS)					\
	-lm

cd_create_profile_CFLAGS =				\
	$(WARNINGFLAGS_C)

bashcompletiondir = ${sysconfdir}/bash_completion.d
dist_bashcompletion_DATA = colormgr-completion.bash

MAINTAINERCLEANFILES = *.a

-include $(top_srcdir)/git.mk
