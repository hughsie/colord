INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(GTK_CFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/libcolord			\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DVERSION="\"$(VERSION)\""			\
	-DLOCALEDIR=\""$(localedir)"\"

COLORD_LIBS = $(top_builddir)/libcolord/libcolord.la

bin_PROGRAMS =						\
	colormgr					\
	cd-create-profile				\
	cd-fix-profile

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
	-lm

cd_fix_profile_CFLAGS =					\
	$(WARNINGFLAGS_C)

cd_create_profile_SOURCES =				\
	cd-create-profile.c

cd_create_profile_LDADD =				\
	$(GLIB_LIBS)					\
	$(LCMS_LIBS)					\
	-lm

cd_create_profile_CFLAGS =				\
	$(WARNINGFLAGS_C)
