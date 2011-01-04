INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(GTK_CFLAGS)					\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DVERSION="\"$(VERSION)\""			\
	-DLOCALEDIR=\""$(localedir)"\"

bin_PROGRAMS =						\
	colormgr

colormgr_SOURCES =					\
	cd-util.c

colormgr_LDADD =					\
	$(GLIB_LIBS)

colormgr_CFLAGS =					\
	$(WARNINGFLAGS_C)

if CD_BUILD_GUI
noinst_PROGRAMS = colorgui

colorgui_SOURCES =					\
	cd-common.h					\
	cd-gui.c
colorgui_LDADD =					\
	$(GLIB_LIBS)					\
	$(GTK_LIBS)
colorgui_CFLAGS = $(WARNINGFLAGS_C)
endif

EXTRA_DIST =						\
	cd-gui.ui
