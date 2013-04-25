AM_CPPFLAGS =						\
	$(GLIB_CFLAGS)					\
	$(GTK_CFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/lib				\
	-I$(top_builddir)/lib				\
	-I$(top_srcdir)/lib/colord			\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DCD_COMPILATION				\
	-DCD_DISABLE_DEPRECATED				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DVERSION="\"$(VERSION)\""			\
	-DLOCALEDIR=\""$(localedir)"\"

COLORD_LIBS =						\
	$(top_builddir)/lib/colord/libcolordprivate.la	\
	$(top_builddir)/lib/colord/libcolord.la

noinst_PROGRAMS =					\
	cd-find-broken

bin_PROGRAMS =						\
	colormgr					\
	cd-create-profile				\
	cd-iccdump					\
	cd-fix-profile

cd_find_broken_SOURCES =				\
	cd-find-broken.c

cd_find_broken_LDADD =					\
	$(COLORD_LIBS)					\
	$(GLIB_LIBS)

cd_find_broken_CFLAGS =					\
	$(WARNINGFLAGS_C)

colormgr_SOURCES =					\
	cd-util.c

colormgr_LDADD =					\
	$(COLORD_LIBS)					\
	$(GLIB_LIBS)

colormgr_CFLAGS =					\
	$(WARNINGFLAGS_C)

cd_fix_profile_SOURCES =				\
	cd-fix-profile.c

cd_fix_profile_LDADD =					\
	$(GLIB_LIBS)					\
	$(LCMS_LIBS)					\
	$(COLORD_LIBS)					\
	-lm

cd_fix_profile_CFLAGS =					\
	$(WARNINGFLAGS_C)

cd_iccdump_SOURCES =					\
	cd-iccdump.c

cd_iccdump_LDADD =					\
	$(GLIB_LIBS)					\
	$(LCMS_LIBS)					\
	$(COLORD_LIBS)					\
	-lm

cd_iccdump_CFLAGS =					\
	$(WARNINGFLAGS_C)

cd_create_profile_SOURCES =				\
	cd-create-profile.c

cd_create_profile_LDADD =				\
	$(GLIB_LIBS)					\
	$(LCMS_LIBS)					\
	$(COLORD_LIBS)					\
	-lm

cd_create_profile_CFLAGS =				\
	$(WARNINGFLAGS_C)

MAINTAINERCLEANFILES = *.a

-include $(top_srcdir)/git.mk
