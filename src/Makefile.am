introspectiondir = $(datadir)/dbus-1/interfaces
dist_introspection_DATA = 					\
	org.freedesktop.ColorManager.xml			\
	org.freedesktop.ColorManager.Device.xml			\
	org.freedesktop.ColorManager.Sensor.xml			\
	org.freedesktop.ColorManager.Profile.xml

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(POLKIT_CFLAGS)				\
	$(LCMS_CFLAGS)					\
	$(GUDEV_CFLAGS)					\
	$(SANE_CFLAGS)					\
	$(SQLITE_CFLAGS)				\
	$(USB_CFLAGS)					\
	-I$(top_srcdir)/libcolord			\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DLIBDIR=\"$(libdir)\"				\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DLOCALSTATEDIR=\""$(localstatedir)"\"		\
	-DVERSION="\"$(VERSION)\""			\
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
	cd-buffer.c					\
	cd-buffer.h					\
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
	cd-math.c					\
	cd-math.h					\
	cd-profile-array.c				\
	cd-profile-array.h				\
	cd-profile.c					\
	cd-profile.h					\
	cd-profile-store.c				\
	cd-profile-store.h				\
	cd-sane-client.c				\
	cd-sane-client.h				\
	cd-sensor.c					\
	cd-sensor.h					\
	cd-udev-client.c				\
	cd-udev-client.h				\
	cd-usb.c					\
	cd-usb.h					\
	cd-main.c

colord_LDADD =						\
	$(COLORD_PRIVATE_LIBS)				\
	$(SQLITE_LIBS)					\
	$(POLKIT_LIBS)					\
	$(LCMS_LIBS)					\
	$(GUDEV_LIBS)					\
	$(SANE_LIBS)					\
	$(USB_LIBS)					\
	$(GLIB_LIBS)

colord_CFLAGS =						\
	$(WARNINGFLAGS_C)

check_PROGRAMS =					\
	cd-self-test

cd_self_test_SOURCES =					\
	cd-buffer.c					\
	cd-buffer.h					\
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
	cd-math.c					\
	cd-math.h					\
	cd-profile-array.c				\
	cd-profile-array.h				\
	cd-profile.c					\
	cd-profile.h					\
	cd-usb.c					\
	cd-usb.h					\
	cd-self-test.c

cd_self_test_LDADD =					\
	$(COLORD_PRIVATE_LIBS)				\
	$(POLKIT_LIBS)					\
	$(LCMS_LIBS)					\
	$(SQLITE_LIBS)					\
	$(USB_LIBS)					\
	$(GLIB_LIBS)

cd_self_test_CFLAGS = $(WARNINGFLAGS_C)

if CD_BUILD_REVERSE

noinst_PROGRAMS =					\
	cd-huey-parse					\
	cd-huey-dump-values				\
	cd-sensor-dump					\
	cd-parse-beagle

cd_parse_beagle_SOURCES =				\
	cd-sensor-munki-private.c			\
	cd-sensor-munki-private.h			\
	cd-sensor-huey-private.c			\
	cd-sensor-huey-private.h			\
	cd-parse-beagle.c

cd_parse_beagle_LDADD =					\
	$(COLORD_PRIVATE_LIBS)				\
	$(GLIB_LIBS)

cd_parse_beagle_CFLAGS =				\
	$(WARNINGFLAGS_C)

cd_huey_parse_SOURCES =					\
	cd-sensor-huey-private.c			\
	cd-sensor-huey-private.h			\
	cd-huey-parse.c

cd_huey_parse_LDADD =					\
	$(COLORD_PRIVATE_LIBS)				\
	$(GLIB_LIBS)

cd_huey_parse_CFLAGS =					\
	$(WARNINGFLAGS_C)

cd_huey_dump_values_SOURCES =				\
	cd-buffer.c					\
	cd-buffer.h					\
	cd-huey-dump-values.c

cd_huey_dump_values_LDADD =				\
	$(COLORD_PRIVATE_LIBS)				\
	$(GLIB_LIBS)

cd_huey_dump_values_CFLAGS =				\
	$(WARNINGFLAGS_C)

cd_sensor_dump_SOURCES =				\
	cd-buffer.c					\
	cd-buffer.h					\
	cd-common.c					\
	cd-common.h					\
	cd-math.c					\
	cd-math.h					\
	cd-sensor.c					\
	cd-sensor.h					\
	cd-sensor-huey.c				\
	cd-usb.c					\
	cd-usb.h					\
	cd-sensor-dump.c

cd_sensor_dump_LDADD =					\
	$(COLORD_PRIVATE_LIBS)				\
	$(POLKIT_LIBS)					\
	$(GLIB_LIBS)					\
	$(LCMS_LIBS)					\
	$(USB_LIBS)					\
	$(GUDEV_LIBS)					\
	-lm

cd_sensor_dump_CFLAGS =					\
	$(WARNINGFLAGS_C)

endif

# sensor plugins
plugindir = $(libdir)/colord-sensors
plugin_LTLIBRARIES =					\
	libcolord_sensor_dummy.la			\
	libcolord_sensor_munki.la			\
	libcolord_sensor_huey.la

libcolord_sensor_dummy_la_SOURCES = cd-sensor-dummy.c
libcolord_sensor_dummy_la_LIBADD = $(GLIB_LIBS) $(COLORD_LIBS)
libcolord_sensor_dummy_la_LDFLAGS = -module -avoid-version
libcolord_sensor_dummy_la_CFLAGS = $(WARNINGFLAGS_C)

libcolord_sensor_huey_la_SOURCES = cd-sensor-huey.c
libcolord_sensor_huey_la_LIBADD = $(GLIB_LIBS) $(COLORD_LIBS)
libcolord_sensor_huey_la_LDFLAGS = -module -avoid-version
libcolord_sensor_huey_la_CFLAGS = $(WARNINGFLAGS_C)

libcolord_sensor_munki_la_SOURCES = cd-sensor-munki.c
libcolord_sensor_munki_la_LIBADD = $(GLIB_LIBS) $(COLORD_LIBS)
libcolord_sensor_munki_la_LDFLAGS = -module -avoid-version
libcolord_sensor_munki_la_CFLAGS = $(WARNINGFLAGS_C)

install-data-hook:
	if test -w $(DESTDIR)$(prefix)/; then \
		mkdir -p $(DESTDIR)$(CD_SYSTEM_PROFILES_DIR); \
		mkdir -p $(DESTDIR)$(localstatedir)/lib/colord; \
	fi

TESTS = cd-self-test
