if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)
endif

AM_CPPFLAGS = \
	$(GLIB_CFLAGS)						\
	$(LCMS_CFLAGS)						\
	$(UDEV_CFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/lib					\
	-DCD_COMPILATION					\
	-DG_LOG_DOMAIN=\"libcolord\"				\
	-DTESTDATADIR=\""$(top_srcdir)/data/tests"\"		\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	-DCD_SYSTEM_PROFILES_DIR="\"$(CD_SYSTEM_PROFILES_DIR)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"

lib_LTLIBRARIES =						\
	libcolordprivate.la					\
	libcolord.la

libcolord_includedir = $(includedir)/colord-1
libcolord_include_HEADERS =					\
	colord.h						\
	colord-private.h

libcolordbase_includedir = $(libcolord_includedir)/colord
libcolordbase_include_HEADERS =					\
	cd-buffer.h						\
	cd-client.h						\
	cd-client-sync.h					\
	cd-color.h						\
	cd-device.h						\
	cd-device-sync.h					\
	cd-dom.h						\
	cd-edid.h						\
	cd-enum.h						\
	cd-icc.h						\
	cd-icc-store.h						\
	cd-icc-utils.h						\
	cd-interp-akima.h					\
	cd-interp-linear.h					\
	cd-interp.h						\
	cd-it8.h						\
	cd-it8-utils.h						\
	cd-math.h						\
	cd-profile.h						\
	cd-profile-sync.h					\
	cd-quirk.h						\
	cd-sensor.h						\
	cd-sensor-sync.h					\
	cd-spectrum.h						\
	cd-transform.h						\
	cd-version.h

libcolordprivate_la_SOURCES =					\
	cd-context-lcms.c					\
	cd-context-lcms.h					\
	cd-buffer.c						\
	cd-color.c						\
	cd-dom.c						\
	cd-edid.c						\
	cd-enum.c						\
	cd-icc.c						\
	cd-icc-store.c						\
	cd-icc-utils.c						\
	cd-interp-akima.c					\
	cd-interp-linear.c					\
	cd-interp.c						\
	cd-it8.c						\
	cd-it8-utils.c						\
	cd-math.c						\
	cd-quirk.c						\
	cd-spectrum.c						\
	cd-transform.c
libcolordprivate_la_LIBADD =					\
	$(LIBM)							\
	$(UDEV_LIBS)						\
	$(LCMS_LIBS)						\
	$(GLIB_LIBS)
libcolordprivate_la_LDFLAGS =					\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^cd_.*'
libcolordprivate_la_CFLAGS =					\
	$(WARNINGFLAGS_C)

libcolord_la_SOURCES =						\
	cd-client.c						\
	cd-client-sync.c					\
	cd-device.c						\
	cd-device-sync.c					\
	cd-profile.c						\
	cd-profile-sync.c					\
	cd-sensor.c						\
	cd-sensor-sync.c
libcolord_la_LIBADD =						\
	$(UDEV_LIBS)						\
	$(GLIB_LIBS)						\
	libcolordprivate.la
libcolord_la_LDFLAGS =						\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^cd_.*'
libcolord_la_CFLAGS =						\
	$(WARNINGFLAGS_C)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = colord.pc

TESTS_ENVIRONMENT =						\
	libtool --mode=execute valgrind				\
	--quiet							\
	--leak-check=full					\
	--show-possibly-lost=no

TESTS =								\
	cd-test-daemon						\
	cd-test-private

check_PROGRAMS = $(TESTS)

cd_test_daemon_SOURCES =					\
	cd-test-shared.h					\
	cd-test-shared.c					\
	cd-test-daemon.c

cd_test_daemon_LDADD =						\
	$(GLIB_LIBS)						\
	$(lib_LTLIBRARIES)

cd_test_daemon_CFLAGS = $(WARNINGFLAGS_C)

cd_test_private_SOURCES =					\
	cd-test-shared.h					\
	cd-test-shared.c					\
	cd-test-private.c

cd_test_private_LDADD =						\
	$(GLIB_LIBS)						\
	$(UDEV_LIBS)						\
	$(lib_LTLIBRARIES)

cd_test_private_CFLAGS = $(WARNINGFLAGS_C)

if BUILDOPT_INSTALL_TESTS
insttestdir = $(libexecdir)/installed-tests/$(PACKAGE)
insttest_PROGRAMS = $(TESTS)
testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_in_files =						\
	colord-daemon.test.in					\
	colord-private.test.in
testmeta_DATA = $(testmeta_in_files:.test.in=.test)
colord-daemon.test: colord-daemon.test.in Makefile
	@sed -e "s|\@libexecdir\@|$(libexecdir)|g" $< > $@
colord-private.test: colord-private.test.in Makefile
	@sed -e "s|\@libexecdir\@|$(libexecdir)|g" $< > $@
endif

EXTRA_DIST =							\
	cd-version.h.in						\
	colord-daemon.test.in					\
	colord.pc.in						\
	colord-private.test.in

MAINTAINERCLEANFILES =						\
	$(noinst_LIBRARIES)

CLEANFILES = $(BUILT_SOURCES) *.log *.trs

if HAVE_INTROSPECTION
introspection_sources =						\
	$(libcolordprivate_la_SOURCES)				\
	$(libcolord_la_SOURCES)					\
	$(libcolordbase_include_HEADERS)

Colord-1.0.gir: libcolord.la libcolordprivate.la
Colord_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0
Colord_1_0_gir_CFLAGS = $(AM_CPPFLAGS) -DCD_DISABLE_DEPRECATED
Colord_1_0_gir_SCANNERFLAGS = --identifier-prefix=Cd \
				--symbol-prefix=cd \
				--warn-all \
				--add-include-path=$(srcdir) \
				--c-include="colord.h"
Colord_1_0_gir_EXPORT_PACKAGES = colord
Colord_1_0_gir_LIBS = libcolord.la libcolordprivate.la
Colord_1_0_gir_FILES = $(introspection_sources)
INTROSPECTION_GIRS += Colord-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA) *.log *.trs *.test

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
