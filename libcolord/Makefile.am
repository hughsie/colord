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

clean-local:
	rm -f *~

-include $(top_srcdir)/git.mk
