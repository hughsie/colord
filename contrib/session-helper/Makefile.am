introspectiondir = $(datadir)/dbus-1/interfaces
dist_introspection_DATA = 				\
	org.freedesktop.ColorHelper.xml

INCLUDES =						\
	$(COLORD_GTK_CFLAGS)				\
	$(GLIB_CFLAGS)					\
	$(GNOME_DESKTOP_CFLAGS)				\
	$(LCMS_CFLAGS)					\
	-I$(top_srcdir)/client				\
	-I$(top_srcdir)/libcolord			\
	-I$(top_srcdir)/src				\
	-DCD_COMPILATION				\
	-DCD_DISABLE_DEPRECATED				\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DLIBDIR=\"$(libdir)\"				\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DLOCALSTATEDIR=\""$(localstatedir)"\"		\
	-DVERSION="\"$(VERSION)\""

COLORD_LIBS =						\
	$(top_builddir)/libcolord/libcolord.la

libcolord_includedir = $(includedir)/colord-1/colord-session
libcolord_include_HEADERS =				\
	cd-session.h

libexec_PROGRAMS =					\
	colord-session

colord_session_SOURCES =				\
	$(top_srcdir)/client/cd-lcms-helpers.c		\
	$(top_srcdir)/client/cd-lcms-helpers.h		\
	$(top_srcdir)/src/cd-debug.c			\
	$(top_srcdir)/src/cd-debug.h			\
	cd-session.h					\
	cd-state.c					\
	cd-state.h					\
	cd-main.c

colord_session_LDADD =					\
	$(COLORD_LIBS)					\
	$(LCMS_LIBS)					\
	$(GLIB_LIBS)					\
	-lm

colord_session_CFLAGS =					\
	$(WARNINGFLAGS_C)

dbusservicemaindir = $(datadir)/dbus-1/services
dbusservicemain_in_files = org.freedesktop.ColorHelper.service.in
dbusservicemain_DATA     = $(dbusservicemain_in_files:.service.in=.service)
$(dbusservicemain_DATA): $(dbusservicemain_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(libexecdir)|" $< | \
	sed -e "s|\@daemon_user\@|$(daemon_user)|" > $@

if CD_BUILD_SESSION_EXAMPLE
noinst_PROGRAMS =					\
	colord-session-example
colord_session_example_SOURCES =			\
	cd-example.c
colord_session_example_LDADD =				\
	$(COLORD_GTK_LIBS)				\
	$(COLORD_LIBS)					\
	$(GLIB_LIBS)					\
	$(GNOME_DESKTOP_LIBS)				\
	$(LCMS_LIBS) -lm
colord_session_example_CFLAGS =				\
	$(WARNINGFLAGS_C)

test: colord-session-example
	./colord-session-example			\
		--device "xrandr-Lenovo Group Limited"	\
		--sensor dummy				\
		--title "This is a test profile"	\
		--quality high				\
		--whitepoint 0

endif

EXTRA_DIST =						\
	$(dbusservicemain_in_files)

DISTCLEANFILES =					\
	$(dbusservicemain_DATA)

-include $(top_srcdir)/git.mk
