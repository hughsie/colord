if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS =	--add-include-path=$(srcdir)		\
				--add-include-path=$(top_builddir)/libcolord
INTROSPECTION_COMPILER_ARGS =	--includedir=$(srcdir)			\
				--includedir=$(top_srcdir)/libcolord	\
				--includedir=$(top_builddir)/libcolord
endif

INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(GTK_CFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/libcolord				\
	-DCD_COMPILATION					\
	-DG_LOG_DOMAIN=\"libcolord-gtk\"			\
	-DTESTDATADIR=\""$(top_srcdir)/data/tests"\"		\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"

lib_LTLIBRARIES =						\
	libcolord-gtk.la

libcolord_includedir = $(includedir)/colord-1
libcolord_include_HEADERS =					\
	colord-gtk.h

libcolordgtkbase_includedir = $(libcolord_includedir)/colord-gtk
libcolordgtkbase_include_HEADERS =				\
	cd-sample-window.h					\
	cd-window.h						\
	cd-window-sync.h

libcolord_gtk_la_SOURCES =					\
	cd-sample-window.c					\
	cd-window.c						\
	cd-window-sync.c

libcolord_gtk_la_LIBADD =					\
	$(GTK_LIBS)						\
	$(top_builddir)/libcolord/libcolord.la			\
	$(GLIB_LIBS)

libcolord_gtk_la_LDFLAGS =					\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^cd_.*'

libcolord_gtk_la_CFLAGS =					\
	$(WARNINGFLAGS_C)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = colord-gtk.pc

check_PROGRAMS =						\
	cd-self-test

cd_self_test_SOURCES =						\
	cd-self-test.c

cd_self_test_LDADD =						\
	$(GLIB_LIBS)						\
	$(GTK_LIBS)						\
	$(lib_LTLIBRARIES)

cd_self_test_CFLAGS = $(WARNINGFLAGS_C)

TESTS = cd-self-test

EXTRA_DIST =							\
	colord-gtk.pc.in

MAINTAINERCLEANFILES =						\
	$(noinst_LIBRARIES)

CLEANFILES = $(BUILT_SOURCES)

if HAVE_INTROSPECTION
introspection_sources =						\
	$(libcolordgtk_la_SOURCES)				\
	$(libcolordgtkbase_include_HEADERS)

ColordGtk-1.0.gir: libcolord-gtk.la
ColordGtk_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0 Gtk-3.0 Colord-1.0
ColordGtk_1_0_gir_CFLAGS = $(INCLUDES)
ColordGtk_1_0_gir_SCANNERFLAGS = --identifier-prefix=Cd \
				--symbol-prefix=cd_ \
				--warn-all \
				--add-include-path=$(srcdir) \
				--add-include-path=$(top_srcdir)/libcolord \
				--c-include="colord-gtk.h"
ColordGtk_1_0_gir_EXPORT_PACKAGES = colord-gtk
ColordGtk_1_0_gir_LIBS = libcolord-gtk.la
ColordGtk_1_0_gir_FILES = $(introspection_sources)
INTROSPECTION_GIRS += ColordGtk-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)

if FALSE
if HAVE_VAPIGEN
colord-gtk.vapi: ColordGtk-1.0.gir
	$(MAKE) $(AM_MAKEFLAGS) ColordGtk-1.0.gir
	$(AM_V_GEN)$(VAPIGEN) $(VAPIGENFLAGS)			\
		--library colord-gtk				\
		--pkg gio-2.0					\
		ColordGtk-1.0.gir
	touch $@
vapidir = $(datadir)/vala/vapi
dist_vapi_DATA =						\
	colord-gtk.vapi
CLEANFILES += $(dist_vapi_DATA)
endif
endif

endif

clean-local:
	rm -f *~

-include $(top_srcdir)/git.mk
