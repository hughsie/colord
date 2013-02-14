if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS =					\
	--add-include-path=$(srcdir)				\
	--add-include-path=$(top_builddir)/lib/colord
INTROSPECTION_COMPILER_ARGS =					\
	--includedir=$(srcdir)					\
	--includedir=$(top_builddir)/lib/colord
endif

AM_CPPFLAGS =							\
	$(GLIB_CFLAGS)						\
	$(GUSB_CFLAGS)						\
	$(LCMS_CFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/lib					\
	-I$(top_srcdir)/lib/colord				\
	-I$(top_builddir)/lib					\
	-I.							\
	-DCD_COMPILATION					\
	-DCH_COMPILATION					\
	-DG_USB_API_IS_SUBJECT_TO_CHANGE			\
	-DG_LOG_DOMAIN=\"Ch\"					\
	-DTOP_SRCDIR=\""$(top_srcdir)"\" 			\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"

COLORD_LIBS =							\
	$(top_builddir)/lib/colord/libcolordprivate.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 						\
	colorhug.pc

lib_LTLIBRARIES =						\
	libcolorhug.la

libcolorhug_includedir = $(includedir)/colord-1/colorhug
libcolorhug_include_HEADERS =					\
	ch-common.h						\
	ch-device.h						\
	ch-device-queue.h					\
	ch-hash.h						\
	ch-inhx32.h						\
	ch-math.h						\
	ch-version.h						\
	colorhug.h

libcolorhug_la_SOURCES =					\
	ch-common.c						\
	ch-common.h						\
	ch-device.c						\
	ch-device.h						\
	ch-device-queue.c					\
	ch-device-queue.h					\
	ch-hash.c						\
	ch-hash.h						\
	ch-inhx32.c						\
	ch-inhx32.h						\
	ch-math.c						\
	ch-math.h						\
	ch-version.h						\
	colorhug.h

libcolorhug_la_LIBADD =						\
	$(GLIB_LIBS)						\
	$(GUSB_LIBS)						\
	$(COLORD_LIBS)						\
	$(GLIB_LIBS)						\
	$(LCMS_LIBS)

libcolorhug_la_LDFLAGS =					\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^ch_.*'

libcolorhug_la_CFLAGS =						\
	$(WARNINGFLAGS_C)

check_PROGRAMS =						\
	ch-self-test

ch_self_test_SOURCES =						\
	ch-self-test.c

ch_self_test_LDADD =						\
	$(top_builddir)/lib/colord/libcolordprivate.la		\
	$(GLIB_LIBS)						\
	$(SELFTEST_LIBS)					\
	$(LCMS_LIBS)						\
	$(lib_LTLIBRARIES)					\
	$(GUSB_LIBS)

ch_self_test_CFLAGS = -DEGG_TEST $(AM_CFLAGS) $(WARNINGFLAGS_C)

TESTS = ch-self-test

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
CLEANFILES =  log-valgrind.txt
valgrind_verbose = $(valgrind_verbose_$(V))
valgrind_verbose_ = $(valgrind_verbose_$(AM_DEFAULT_VERBOSITY))
valgrind_verbose_0 = | \
	grep '\(^[^=]\|ERROR SUMMARY\|definitely lost\|indirectly lost\)' | grep -v ': 0'
# TODO: The following check does not fail if valgrind finds error.  It should.
check-valgrind:
	$(AM_V_at)$(MAKE) $(AM_MAKEFLGS) check-tool TOOL="valgrind $(VALGRIND_FLAGS)" \
	2>&1 | tee log-valgrind.txt $(valgrind_verbose)

.PHONY: check-tool check-valgrind

if HAVE_INTROSPECTION
introspection_sources =						\
	ch-common.c						\
	ch-common.h						\
	ch-device.c						\
	ch-device.h						\
	ch-hash.c						\
	ch-hash.h						\
	ch-device-queue.c					\
	ch-device-queue.h

ColorHug-1.0.gir: libcolorhug.la
ColorHug_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0 GUsb-1.0 Colord-1.0
ColorHug_1_0_gir_CFLAGS = $(AM_CPPFLAGS)
ColorHug_1_0_gir_SCANNERFLAGS = --identifier-prefix=Ch \
				--symbol-prefix=ch \
				--warn-all \
				--add-include-path=$(srcdir) \
				--c-include="colorhug.h"
ColorHug_1_0_gir_EXPORT_PACKAGES = colorhug
ColorHug_1_0_gir_LIBS = libcolorhug.la
ColorHug_1_0_gir_FILES = $(introspection_sources)
INTROSPECTION_GIRS += ColorHug-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

EXTRA_DIST =							\
	ch-version.h.in						\
	colorhug.pc.in

clean-local:
	rm -f *~
	rm -f $(CLEANFILES)

-include $(top_srcdir)/git.mk
