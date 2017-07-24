AM_CPPFLAGS =						\
	$(GLIB_CFLAGS)					\
	$(GUSB_CFLAGS)					\
	$(LCMS_CFLAGS)					\
	$(GUDEV_CFLAGS)					\
	-I$(top_srcdir)/src				\
	-I$(top_srcdir)/lib				\
	-I$(top_builddir)/lib				\
	-I$(top_srcdir)/lib/colord			\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DCD_COMPILATION				\
	-DLOCALEDIR=\""$(localedir)"\"

# sensor plugins
plugindir = $(libdir)/colord-sensors
plugin_LTLIBRARIES =					\
	libcolord_sensor_dummy.la

libcolord_sensor_dummy_la_SOURCES = cd-sensor-dummy.c
libcolord_sensor_dummy_la_LIBADD = $(GLIB_LIBS)
libcolord_sensor_dummy_la_LDFLAGS = -module -avoid-version
libcolord_sensor_dummy_la_CFLAGS = $(WARN_CFLAGS)

-include $(top_srcdir)/git.mk
