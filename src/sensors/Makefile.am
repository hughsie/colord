AM_CPPFLAGS =						\
	$(GLIB_CFLAGS)					\
	$(GUSB_CFLAGS)					\
	$(POLKIT_CFLAGS)				\
	$(LCMS_CFLAGS)					\
	$(GUDEV_CFLAGS)					\
	$(SANE_CFLAGS)					\
	$(SQLITE_CFLAGS)				\
	-I$(top_srcdir)/src				\
	-I$(top_srcdir)/lib				\
	-I$(top_builddir)/lib				\
	-I$(top_srcdir)/lib/colord			\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DLIBDIR=\"$(libdir)\"				\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DLOCALSTATEDIR=\""$(localstatedir)"\"		\
	-DVERSION="\"$(VERSION)\""			\
	-DCD_COMPILATION				\
	-DCD_DISABLE_DEPRECATED				\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DG_USB_API_IS_SUBJECT_TO_CHANGE		\
	-DLOCALEDIR=\""$(localedir)"\"

COLORD_PRIVATE_LIBS =					\
	$(top_builddir)/lib/colord/libcolordprivate.la

if CD_BUILD_REVERSE

noinst_PROGRAMS =					\
	cd-huey-parse					\
	cd-huey-dump-values				\
	cd-sensor-dump					\
	cd-parse-beagle

cd_parse_beagle_SOURCES =				\
	cd-parse-beagle.c

cd_parse_beagle_LDADD =					\
	$(COLORD_PRIVATE_LIBS)				\
	$(GLIB_LIBS)					\
	$(top_builddir)/lib/huey/libhuey.la		\
	$(top_builddir)/lib/munki/libmunki.la

cd_parse_beagle_CFLAGS =				\
	$(WARNINGFLAGS_C)

cd_huey_parse_SOURCES =					\
	cd-huey-parse.c

cd_huey_parse_LDADD =					\
	$(COLORD_PRIVATE_LIBS)				\
	$(GLIB_LIBS)					\
	$(top_builddir)/lib/huey/libhuey.la

cd_huey_parse_CFLAGS =					\
	$(WARNINGFLAGS_C)

cd_huey_dump_values_SOURCES =				\
	cd-huey-dump-values.c

cd_huey_dump_values_LDADD =				\
	$(COLORD_PRIVATE_LIBS)				\
	$(GUSB_LIBS)					\
	$(GLIB_LIBS)

cd_huey_dump_values_CFLAGS =				\
	$(WARNINGFLAGS_C)

cd_sensor_dump_SOURCES =				\
	../cd-common.c					\
	../cd-common.h					\
	../cd-sensor.c					\
	../cd-sensor.h					\
	../cd-debug.c					\
	../cd-debug.h					\
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
libcolord_sensor_dummy_la_LIBADD = $(GLIB_LIBS)
libcolord_sensor_dummy_la_LDFLAGS = -module -avoid-version
libcolord_sensor_dummy_la_CFLAGS = $(WARNINGFLAGS_C)

if HAVE_GUSB
plugin_LTLIBRARIES +=					\
	libcolord_sensor_colorhug.la			\
	libcolord_sensor_dtp94.la			\
	libcolord_sensor_huey.la

libcolord_sensor_huey_la_SOURCES =			\
	cd-sensor-huey.c				\
	cd-sensor-huey.h
libcolord_sensor_huey_la_LIBADD =			\
	$(GLIB_LIBS)					\
	$(GUSB_LIBS)					\
	$(top_builddir)/lib/huey/libhuey.la
libcolord_sensor_huey_la_LDFLAGS = -module -avoid-version
libcolord_sensor_huey_la_CFLAGS = $(WARNINGFLAGS_C)

libcolord_sensor_dtp94_la_SOURCES =			\
	cd-sensor-dtp94.c				\
	cd-sensor-dtp94.h
libcolord_sensor_dtp94_la_LIBADD =			\
	$(GLIB_LIBS)					\
	$(GUSB_LIBS)					\
	$(top_builddir)/lib/dtp94/libdtp94.la
libcolord_sensor_dtp94_la_LDFLAGS = -module -avoid-version
libcolord_sensor_dtp94_la_CFLAGS = $(WARNINGFLAGS_C)

libcolord_sensor_colorhug_la_SOURCES =			\
	cd-sensor-colorhug.c				\
	cd-sensor-colorhug.h
libcolord_sensor_colorhug_la_LIBADD =			\
	$(GLIB_LIBS)					\
	$(GUSB_LIBS)					\
	$(top_builddir)/lib/colorhug/libcolorhug.la
libcolord_sensor_colorhug_la_LDFLAGS = -module -avoid-version
libcolord_sensor_colorhug_la_CFLAGS = $(WARNINGFLAGS_C)
endif

plugin_LTLIBRARIES +=					\
	libcolord_sensor_argyll.la
libcolord_sensor_argyll_la_SOURCES =			\
	cd-spawn.c					\
	cd-spawn.h					\
	cd-sensor-argyll.c
libcolord_sensor_argyll_la_LIBADD = $(GLIB_LIBS) $(GUSB_LIBS)
libcolord_sensor_argyll_la_LDFLAGS = -module -avoid-version
libcolord_sensor_argyll_la_CFLAGS = $(WARNINGFLAGS_C)

if FALSE
libcolord_sensor_munki_la_SOURCES = cd-sensor-munki.c
libcolord_sensor_munki_la_LIBADD = $(GLIB_LIBS)
libcolord_sensor_munki_la_LDFLAGS = -module -avoid-version
libcolord_sensor_munki_la_CFLAGS = $(WARNINGFLAGS_C)
endif

-include $(top_srcdir)/git.mk
