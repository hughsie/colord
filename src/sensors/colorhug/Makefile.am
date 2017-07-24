AM_CPPFLAGS =						\
	$(GLIB_CFLAGS)					\
	$(GUSB_CFLAGS)					\
	$(GUDEV_CFLAGS)					\
	-I$(top_srcdir)/src				\
	-I$(top_srcdir)/lib				\
	-I$(top_builddir)/lib				\
	-I$(top_srcdir)/lib/colord			\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DCD_COMPILATION				\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DG_USB_API_IS_SUBJECT_TO_CHANGE		\
	-DLOCALEDIR=\""$(localedir)"\"

plugindir = $(libdir)/colord-sensors
plugin_LTLIBRARIES =					\
	libcolord_sensor_colorhug.la

libcolord_sensor_colorhug_la_SOURCES =			\
	cd-sensor-colorhug.c
libcolord_sensor_colorhug_la_LIBADD =			\
	$(GLIB_LIBS)					\
	$(GUSB_LIBS)					\
	$(top_builddir)/lib/colorhug/libcolorhug.la
libcolord_sensor_colorhug_la_LDFLAGS = -module -avoid-version
libcolord_sensor_colorhug_la_CFLAGS = $(WARN_CFLAGS)

-include $(top_srcdir)/git.mk
