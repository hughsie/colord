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
	colormgr

colormgr_SOURCES =					\
	cd-util.c

colormgr_LDADD =					\
	$(COLORD_LIBS)					\
	$(GLIB_LIBS)

colormgr_CFLAGS =					\
	$(WARNINGFLAGS_C)
