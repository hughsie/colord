SUBDIRS = 						\
	sensors

introspectiondir = $(datadir)/dbus-1/interfaces
dist_introspection_DATA = 				\
	org.freedesktop.ColorManager.xml		\
	org.freedesktop.ColorManager.Device.xml		\
	org.freedesktop.ColorManager.Sensor.xml		\
	org.freedesktop.ColorManager.Profile.xml

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(POLKIT_CFLAGS)				\
	$(LCMS_CFLAGS)					\
	$(GUDEV_CFLAGS)					\
	$(SQLITE_CFLAGS)				\
	-I$(top_srcdir)/libcolord			\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DLIBDIR=\"$(libdir)\"				\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DLOCALSTATEDIR=\""$(localstatedir)"\"		\
	-DVERSION="\"$(VERSION)\""			\
	-DDAEMON_USER="\"$(daemon_user)\""		\
	-DCD_COMPILATION				\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DCD_SYSTEM_PROFILES_DIR="\"$(CD_SYSTEM_PROFILES_DIR)"\" \
	-DLOCALEDIR=\""$(localedir)"\"

COLORD_LIBS =						\
	$(top_builddir)/libcolord/libcolord.la
COLORD_PRIVATE_LIBS =					\
	$(top_builddir)/libcolord/libcolordprivate.a

libexec_PROGRAMS =					\
	colord

colord_SOURCES =					\
	cd-common.c					\
	cd-common.h					\
	cd-config.c					\
	cd-config.h					\
	cd-debug.c					\
	cd-debug.h					\
	cd-device-array.c				\
	cd-device-array.h				\
	cd-device.c					\
	cd-device-db.c					\
	cd-device-db.h					\
	cd-device.h					\
	cd-inhibit.c					\
	cd-inhibit.h					\
	cd-mapping-db.c					\
	cd-mapping-db.h					\
	cd-profile-array.c				\
	cd-profile-array.h				\
	cd-profile.c					\
	cd-profile.h					\
	cd-profile-store.c				\
	cd-profile-store.h				\
	cd-sensor.c					\
	cd-sensor.h					\
	cd-main.c

if HAVE_GUDEV
colord_SOURCES +=					\
	cd-sensor-client.c				\
	cd-sensor-client.h				\
	cd-udev-client.c				\
	cd-udev-client.h
endif

colord_LDADD =						\
	$(COLORD_PRIVATE_LIBS)				\
	$(SQLITE_LIBS)					\
	$(POLKIT_LIBS)					\
	$(LCMS_LIBS)					\
	$(GUDEV_LIBS)					\
	$(GLIB_LIBS)

colord_CFLAGS =						\
	$(WARNINGFLAGS_C)

check_PROGRAMS =					\
	cd-self-test

cd_self_test_SOURCES =					\
	cd-common.c					\
	cd-common.h					\
	cd-device-array.c				\
	cd-device-array.h				\
	cd-device.c					\
	cd-device-db.c					\
	cd-device-db.h					\
	cd-device.h					\
	cd-inhibit.c					\
	cd-inhibit.h					\
	cd-mapping-db.c					\
	cd-mapping-db.h					\
	cd-profile-array.c				\
	cd-profile-array.h				\
	cd-profile.c					\
	cd-profile.h					\
	cd-self-test.c

cd_self_test_LDADD =					\
	$(COLORD_PRIVATE_LIBS)				\
	$(POLKIT_LIBS)					\
	$(LCMS_LIBS)					\
	$(SQLITE_LIBS)					\
	$(GLIB_LIBS)

cd_self_test_CFLAGS = $(WARNINGFLAGS_C)

install-data-hook:
	if test -w $(DESTDIR)$(prefix)/; then \
		mkdir -p $(DESTDIR)$(CD_SYSTEM_PROFILES_DIR); \
		chown $(daemon_user) $(DESTDIR)$(localstatedir)/lib/colord; \
		chmod 0755 $(DESTDIR)$(localstatedir)/lib/colord; \
	fi

TESTS = cd-self-test

-include $(top_srcdir)/git.mk
