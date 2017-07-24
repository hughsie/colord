SUBDIRS = 						\
	argyll						\
	colorhug					\
	dtp94						\
	dummy						\
	huey						\
	munki						\
	spark

AM_CPPFLAGS =						\
	$(GLIB_CFLAGS)					\
	$(GUSB_CFLAGS)					\
	$(GUDEV_CFLAGS)					\
	-I$(top_srcdir)/src				\
	-I$(top_srcdir)/lib				\
	-I$(top_builddir)/lib				\
	-I$(top_srcdir)/lib/colord			\
	-DLIBDIR=\"$(libdir)\"				\
	-DDATADIR=\"$(datadir)\"			\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DCD_COMPILATION				\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DG_USB_API_IS_SUBJECT_TO_CHANGE		\
	-DLOCALEDIR=\""$(localedir)"\"

COLORD_PRIVATE_LIBS =					\
	$(top_builddir)/lib/colord/libcolordprivate.la

if CD_BUILD_REVERSE

noinst_PROGRAMS =					\
	cd-sensor-dump					\
	cd-parse-beagle

cd_parse_beagle_SOURCES =				\
	cd-parse-beagle.c

cd_parse_beagle_LDADD =					\
	$(COLORD_PRIVATE_LIBS)				\
	$(GLIB_LIBS)					\
	$(top_builddir)/lib/huey/libhuey-private.la	\
	$(top_builddir)/lib/munki/libmunki-private.la

cd_parse_beagle_CFLAGS =				\
	$(WARN_CFLAGS)

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
	$(WARN_CFLAGS)

CLEANFILES = $(noinst_PROGRAMS)

endif

-include $(top_srcdir)/git.mk
