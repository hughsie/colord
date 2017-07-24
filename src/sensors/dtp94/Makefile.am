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

if HAVE_GUSB
plugindir = $(libdir)/colord-sensors
plugin_LTLIBRARIES =					\
	libcolord_sensor_dtp94.la

libcolord_sensor_dtp94_la_SOURCES =			\
	dtp94-device.c					\
	dtp94-device.h					\
	dtp94-enum.c					\
	dtp94-enum.h					\
	cd-sensor-dtp94.c
libcolord_sensor_dtp94_la_LIBADD =			\
	$(GLIB_LIBS)					\
	$(GUSB_LIBS)
libcolord_sensor_dtp94_la_LDFLAGS = -module -avoid-version
libcolord_sensor_dtp94_la_CFLAGS = $(WARN_CFLAGS)
endif

-include $(top_srcdir)/git.mk
