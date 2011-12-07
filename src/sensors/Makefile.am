INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(GUSB_CFLAGS)					\
	$(POLKIT_CFLAGS)				\
	$(LCMS_CFLAGS)					\
	$(GUDEV_CFLAGS)					\
	$(SANE_CFLAGS)					\
	$(SQLITE_CFLAGS)				\
	-I$(top_srcdir)/src				\
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
	-DG_USB_API_IS_SUBJECT_TO_CHANGE		\
	-DCD_SYSTEM_PROFILES_DIR="\"$(CD_SYSTEM_PROFILES_DIR)"\" \
	-DLOCALEDIR=\""$(localedir)"\"

COLORD_LIBS =						\
	$(top_builddir)/libcolord/libcolord.la
COLORD_PRIVATE_LIBS =					\
	$(top_builddir)/libcolord/libcolordprivate.a

check_PROGRAMS =					\
	cd-self-test

cd_self_test_SOURCES =					\
	cd-buffer.c					\
	cd-buffer.h					\
	cd-self-test.c

cd_self_test_LDADD =					\
	$(COLORD_PRIVATE_LIBS)				\
	$(POLKIT_LIBS)					\
	$(LCMS_LIBS)					\
	$(SQLITE_LIBS)					\
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
	$(GUSB_LIBS)					\
	$(GLIB_LIBS)

cd_huey_dump_values_CFLAGS =				\
	$(WARNINGFLAGS_C)

cd_sensor_dump_SOURCES =				\
	cd-buffer.c					\
	cd-buffer.h					\
	../cd-common.c					\
	../cd-common.h					\
	../cd-sensor.c					\
	../cd-sensor.h					\
	cd-sensor-huey.c				\
	cd-sensor-dump.c

cd_sensor_dump_LDADD =					\
	$(COLORD_PRIVATE_LIBS)				\
	$(POLKIT_LIBS)					\
	$(GLIB_LIBS)					\
	$(LCMS_LIBS)					\
	$(GUSB_LIBS)					\
	$(GUDEV_LIBS)					\
	-lm

cd_sensor_dump_CFLAGS =					\
	$(WARNINGFLAGS_C)

CLEANFILES = $(noinst_PROGRAMS)

endif

# sensor plugins
plugindir = $(libdir)/colord-sensors
plugin_LTLIBRARIES =					\
	libcolord_sensor_dummy.la

libcolord_sensor_dummy_la_SOURCES = cd-sensor-dummy.c
libcolord_sensor_dummy_la_LIBADD = $(GLIB_LIBS) $(COLORD_LIBS)
libcolord_sensor_dummy_la_LDFLAGS = -module -avoid-version
libcolord_sensor_dummy_la_CFLAGS = $(WARNINGFLAGS_C)

if HAVE_GUSB
plugin_LTLIBRARIES +=					\
	libcolord_sensor_colorhug.la			\
	libcolord_sensor_huey.la

libcolord_sensor_huey_la_SOURCES =			\
	cd-sensor-huey.c				\
	cd-sensor-huey.h				\
	cd-sensor-huey-private.c			\
	cd-sensor-huey-private.h			\
	cd-buffer.c
libcolord_sensor_huey_la_LIBADD = $(GLIB_LIBS) $(GUSB_LIBS) $(COLORD_LIBS)
libcolord_sensor_huey_la_LDFLAGS = -module -avoid-version
libcolord_sensor_huey_la_CFLAGS = $(WARNINGFLAGS_C)

libcolord_sensor_colorhug_la_SOURCES =			\
	cd-sensor-colorhug-private.c			\
	cd-sensor-colorhug-private.h			\
	cd-sensor-colorhug.c				\
	cd-sensor-colorhug.h
libcolord_sensor_colorhug_la_LIBADD = $(GLIB_LIBS) $(GUSB_LIBS) $(COLORD_LIBS)
libcolord_sensor_colorhug_la_LDFLAGS = -module -avoid-version
libcolord_sensor_colorhug_la_CFLAGS = $(WARNINGFLAGS_C)
endif

if FALSE
libcolord_sensor_munki_la_SOURCES = cd-sensor-munki.c
libcolord_sensor_munki_la_LIBADD = $(GLIB_LIBS) $(COLORD_LIBS)
libcolord_sensor_munki_la_LDFLAGS = -module -avoid-version
libcolord_sensor_munki_la_CFLAGS = $(WARNINGFLAGS_C)
endif

install-data-hook:
	if test -w $(DESTDIR)$(prefix)/; then \
		mkdir -p $(DESTDIR)$(CD_SYSTEM_PROFILES_DIR); \
		mkdir -p $(DESTDIR)$(localstatedir)/lib/colord; \
	fi

TESTS = cd-self-test

-include $(top_srcdir)/git.mk
