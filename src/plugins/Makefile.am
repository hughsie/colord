## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.7

CD_GLIB2_LIBS =						\
	$(top_builddir)/lib/packagekit-glib2/libpackagekit-glib2.la

AM_CPPFLAGS =						\
	$(GIO_CFLAGS)					\
	$(GLIB_CFLAGS)					\
	$(GUDEV_CFLAGS)					\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DCD_COMPILATION				\
	-DCD_DISABLE_DEPRECATED				\
	-DCOLORD_SANE_BINARY=\""$(libexecdir)/colord-sane"\"	\
	-I$(top_srcdir)/lib				\
	-I$(top_builddir)/lib				\
	-I$(top_srcdir)/lib/colord			\
	-I$(top_srcdir)/src

# plugins
plugindir = $(libdir)/colord-plugins
noinst_LTLIBRARIES =					\
	libcd_plugin_example.la

libcd_plugin_example_la_SOURCES = cd-plugin-example.c
libcd_plugin_example_la_LIBADD = $(CD_PLUGIN_LIBS)
libcd_plugin_example_la_LDFLAGS = -module -avoid-version
libcd_plugin_example_la_CFLAGS = $(CD_PLUGIN_CFLAGS) $(WARNINGFLAGS_C)

if HAVE_UDEV

plugin_LTLIBRARIES =					\
	libcd_plugin_camera.la				\
	libcd_plugin_scanner.la

libcd_plugin_camera_la_SOURCES = cd-plugin-camera.c
libcd_plugin_camera_la_LIBADD = $(GUDEV_LIBS)
libcd_plugin_camera_la_LDFLAGS = -module -avoid-version
libcd_plugin_camera_la_CFLAGS = $(GUDEV_CFLAGS) $(WARNINGFLAGS_C)

libcd_plugin_scanner_la_SOURCES = cd-plugin-scanner.c
libcd_plugin_scanner_la_LIBADD = $(GUDEV_LIBS)
libcd_plugin_scanner_la_LDFLAGS = -module -avoid-version
libcd_plugin_scanner_la_CFLAGS = $(GUDEV_CFLAGS) $(WARNINGFLAGS_C)

if HAVE_SANE

plugin_LTLIBRARIES +=					\
	libcd_plugin_sane.la

libcd_plugin_sane_la_SOURCES = cd-plugin-sane.c
libcd_plugin_sane_la_LIBADD = $(GUDEV_LIBS)
libcd_plugin_sane_la_LDFLAGS = -module -avoid-version
libcd_plugin_sane_la_CFLAGS = $(GUDEV_CFLAGS) $(WARNINGFLAGS_C)

endif # HAVE_SANE
endif # HAVE_UDEV

-include $(top_srcdir)/git.mk
