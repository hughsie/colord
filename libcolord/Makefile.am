if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)
endif

INCLUDES = \
	$(GLIB_CFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/libcolord				\
	-DCD_COMPILATION					\
	-DG_LOG_DOMAIN=\"libcolord\"				\
	-DTESTDATADIR=\""$(top_srcdir)/data/tests"\"		\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"

# only stuff we can use in the daemon
noinst_LIBRARIES = libcolordprivate.a
libcolordprivate_a_SOURCES =					\
	cd-color.c						\
	cd-enum.c

libcolordprivate_a_CFLAGS =					\
	$(WARNINGFLAGS_C)

lib_LTLIBRARIES =						\
	libcolord.la

libcolord_includedir = $(includedir)/colord-1
libcolord_include_HEADERS =					\
	colord.h

libcolordbase_includedir = $(libcolord_includedir)/colord
libcolordbase_include_HEADERS =					\
	cd-version.h						\
	cd-enum.h						\
	cd-device.h						\
	cd-device-sync.h					\
	cd-sensor.h						\
	cd-sensor-sync.h					\
	cd-profile.h						\
	cd-profile-sync.h					\
	cd-color.h						\
	cd-client.h						\
	cd-client-sync.h

libcolord_la_SOURCES =						\
	cd-enum.c						\
	cd-client.c						\
	cd-client-sync.c					\
	cd-profile.c						\
	cd-profile-sync.c					\
	cd-sensor.c						\
	cd-sensor-sync.c					\
	cd-color.c						\
	cd-device.c						\
	cd-device-sync.c

libcolord_la_LIBADD =						\
	$(GLIB_LIBS)

libcolord_la_LDFLAGS =						\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^cd_.*'

libcolord_la_CFLAGS =						\
	$(WARNINGFLAGS_C)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = colord.pc

check_PROGRAMS =						\
	cd-self-test

cd_self_test_SOURCES =						\
	cd-self-test.c

cd_self_test_LDADD =						\
	$(GLIB_LIBS)						\
	$(lib_LTLIBRARIES)

cd_self_test_CFLAGS = $(WARNINGFLAGS_C)

TESTS = cd-self-test

EXTRA_DIST =							\
	cd-version.h.in						\
	colord.pc.in

MAINTAINERCLEANFILES =						\
	$(noinst_LIBRARIES)

CLEANFILES = $(BUILT_SOURCES)

if HAVE_INTROSPECTION
introspection_sources =						\
	$(libcolord_la_SOURCES)					\
	$(libcolordbase_include_HEADERS)

Colord-1.0.gir: libcolord.la
Colord_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0
Colord_1_0_gir_CFLAGS = $(INCLUDES)
Colord_1_0_gir_SCANNERFLAGS = --identifier-prefix=Cd \
				--symbol-prefix=cd_ \
				--warn-all \
				--add-include-path=$(srcdir) \
				--c-include="colord.h"
Colord_1_0_gir_EXPORT_PACKAGES = colord
Colord_1_0_gir_LIBS = libcolord.la
Colord_1_0_gir_FILES = $(introspection_sources)
INTROSPECTION_GIRS += Colord-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)

if HAVE_VAPIGEN
colord.vapi: Colord-1.0.gir
	$(MAKE) $(AM_MAKEFLAGS) Colord-1.0.gir
	$(AM_V_GEN)$(VAPIGEN) $(VAPIGENFLAGS)			\
		--library colord \
		--pkg gio-2.0					\
		Colord-1.0.gir
	touch $@
vapidir = $(datadir)/vala/vapi
dist_vapi_DATA =						\
	colord.vapi
CLEANFILES += $(dist_vapi_DATA)
endif

endif

clean-local:
	rm -f *~

-include $(top_srcdir)/git.mk
