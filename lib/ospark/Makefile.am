AM_CPPFLAGS = \
	$(GLIB_CFLAGS)						\
	$(GUSB_CFLAGS)						\
	-I$(top_srcdir)/lib					\
	-I$(top_srcdir)/lib/colord				\
	-I$(top_builddir)/lib					\
	-DCD_COMPILATION					\
	-DOSPARK_COMPILATION					\
	-DG_USB_API_IS_SUBJECT_TO_CHANGE			\
	-DG_LOG_DOMAIN=\"libospark\"

COLORD_LIBS =							\
	$(top_builddir)/lib/colord/libcolordprivate.la

privlibdir=$(libdir)/colord-sensors
privlib_LTLIBRARIES =						\
	libospark-private.la

libospark_private_la_SOURCES =					\
	ospark.h						\
	osp-device.c						\
	osp-device.h						\
	osp-enum.c						\
	osp-enum.h

libospark_private_la_LIBADD =					\
	$(GUSB_LIBS)						\
	$(COLORD_LIBS)						\
	$(GLIB_LIBS)

libospark_private_la_LDFLAGS =					\
	-export-dynamic						\
	-no-undefined						\
	-avoid-version						\
	-export-symbols-regex '^osp_.*'

libospark_private_la_CFLAGS =					\
	$(WARNINGFLAGS_C)

TESTS_ENVIRONMENT =						\
	libtool --mode=execute valgrind				\
	--quiet							\
	--leak-check=full					\
	--show-possibly-lost=no

check_PROGRAMS =						\
	osp-self-test

osp_self_test_SOURCES =						\
	osp-self-test.c

osp_self_test_LDADD =						\
	$(top_builddir)/lib/colord/libcolordprivate.la		\
	$(GLIB_LIBS)						\
	$(SELFTEST_LIBS)					\
	$(LCMS_LIBS)						\
	$(privlib_LTLIBRARIES)					\
	$(GUSB_LIBS)

osp_self_test_CFLAGS = -DEGG_TEST $(AM_CFLAGS) $(WARNINGFLAGS_C)

TESTS = osp-self-test

CLEANFILES = *.log *.trs $(BUILT_SOURCES)

clean-local:
	rm -f *~

-include $(top_srcdir)/git.mk
