if HAVE_SANE
introspectiondir = $(datadir)/dbus-1/interfaces
dist_introspection_DATA = 				\
	org.freedesktop.colord.sane.xml

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(GUDEV_CFLAGS)					\
	$(SANE_CFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/libcolord			\
	-DG_LOG_DOMAIN=\"CdSane\"			\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DVERSION="\"$(VERSION)\""			\
	-DCD_COMPILATION				\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DLOCALEDIR=\""$(localedir)"\"

if HAVE_GUDEV

COLORD_LIBS =						\
	$(top_builddir)/libcolord/libcolord.la

libexec_PROGRAMS =					\
	colord-sane

colord_sane_SOURCES =					\
	cd-main.c

colord_sane_LDADD =					\
	$(COLORD_LIBS)					\
	$(GUDEV_LIBS)					\
	$(SANE_LIBS)					\
	$(GLIB_LIBS)

colord_sane_CFLAGS =					\
	$(WARNINGFLAGS_C)
endif

dbusdir = $(sysconfdir)/dbus-1/system.d
dist_dbus_DATA = org.freedesktop.colord-sane.conf

dbusservicemaindir = $(datadir)/dbus-1/system-services
dbusservicemain_in_files = org.freedesktop.colord-sane.service.in
dbusservicemain_DATA     = $(dbusservicemain_in_files:.service.in=.service)
$(dbusservicemain_DATA): $(dbusservicemain_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(libexecdir)|" $< | \
	sed -e "s|\@daemon_user\@|$(daemon_user)|" > $@

if HAVE_SYSTEMD
systemdservicemaindir = $(systemdsystemunitdir)
systemdservicemain_in_files = colord-sane.service.in
systemdservicemain_DATA     = $(systemdservicemain_in_files:.service.in=.service)
$(systemdservicemain_DATA): $(systemdservicemain_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(libexecdir)|" $< | \
	sed -e "s|\@daemon_user\@|$(daemon_user)|" > $@
endif

EXTRA_DIST =						\
	$(dbusservicemain_in_files)				\
	org.freedesktop.colord-sane.conf.in

if HAVE_SYSTEMD
EXTRA_DIST += $(systemdservicemain_in_files)
endif

DISTCLEANFILES =					\
	$(dbusservicemain_DATA)				\
	$(dist_profile_DATA)				\
	$(systemdservicemain_DATA)

-include $(top_srcdir)/git.mk
endif
