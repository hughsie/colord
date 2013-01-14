if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)
endif

AM_CPPFLAGS = \
	$(GLIB_CFLAGS)						\
	$(LCMS_CFLAGS)						\
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
	cd-enum.c						\
	cd-math.c

libcolordprivate_a_CFLAGS =					\
	$(WARNINGFLAGS_C)

lib_LTLIBRARIES =						\
	libcolord.la

libcolord_includedir = $(includedir)/colord-1
libcolord_include_HEADERS =					\
	colord.h						\
	colord-private.h

libcolordbase_includedir = $(libcolord_includedir)/colord
libcolordbase_include_HEADERS =					\
	cd-client.h						\
	cd-client-sync.h					\
	cd-color.h						\
	cd-deprecated.h						\
	cd-device.h						\
	cd-device-sync.h					\
	cd-enum.h						\
	cd-it8.h						\
	cd-it8-utils.h						\
	cd-math.h						\
	cd-profile.h						\
	cd-profile-sync.h					\
	cd-sensor.h						\
	cd-sensor-sync.h					\
	cd-version.h

libcolord_la_SOURCES =						\
	cd-client.c						\
	cd-client-sync.c					\
	cd-color.c						\
	cd-deprecated.c						\
	cd-device.c						\
	cd-device-sync.c					\
	cd-enum.c						\
	cd-it8.c						\
	cd-it8-utils.c						\
	cd-math.c						\
	cd-profile.c						\
	cd-profile-sync.c					\
	cd-sensor.c						\
	cd-sensor-sync.c

libcolord_la_LIBADD =						\
	$(LCMS_LIBS)						\
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
Colord_1_0_gir_CFLAGS = $(AM_CPPFLAGS) -DCD_DISABLE_DEPRECATED
Colord_1_0_gir_SCANNERFLAGS = --identifier-prefix=Cd \
				--symbol-prefix=cd \
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

# check-tool: Run tests under $(TOOL)
check-tool:
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) check \
	TESTS_ENVIRONMENT='$(TESTS_ENVIRONMENT) $(top_builddir)/libtool --mode=execute \
	env $(TOOL)'

# Check tests under valgrind.  Saves log to log-valgrind.txt
VALGRIND_FLAGS = \
	--tool=memcheck --suppressions=$(srcdir)/.valgrind-suppressions \
	--track-origins=yes \
	--leak-check=yes
	$(EXTRA_VALGRIND_FLAGS)
CLEANFILES +=  log-valgrind.txt
valgrind_verbose = $(valgrind_verbose_$(V))
valgrind_verbose_ = $(valgrind_verbose_$(AM_DEFAULT_VERBOSITY))
valgrind_verbose_0 = | \
	grep '\(^[^=]\|ERROR SUMMARY\|definitely lost\|indirectly lost\)' | grep -v ': 0'
# TODO: The following check does not fail if valgrind finds error.  It should.
check-valgrind:
	$(AM_V_at)$(MAKE) $(AM_MAKEFLGS) check-tool TOOL="valgrind $(VALGRIND_FLAGS)" \
	2>&1 | tee log-valgrind.txt $(valgrind_verbose)

.PHONY: check-tool check-valgrind

clean-local:
	rm -f *~

-include $(top_srcdir)/git.mk
