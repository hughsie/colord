AM_CPPFLAGS = \
	$(GLIB_CFLAGS)						\
	$(GUSB_CFLAGS)						\
	-I$(top_srcdir)/lib					\
	-I$(top_srcdir)/lib/colord				\
	-I$(top_builddir)/lib					\
	-DCD_COMPILATION					\
	-DDTP94_COMPILATION					\
	-DG_USB_API_IS_SUBJECT_TO_CHANGE			\
	-DG_LOG_DOMAIN=\"libdtp94\"

COLORD_LIBS =							\
	$(top_builddir)/lib/colord/libcolordprivate.la

lib_LTLIBRARIES =						\
	libdtp94.la

libdtp94_la_SOURCES =						\
	dtp94.h							\
	dtp94-device.c						\
	dtp94-device.h						\
	dtp94-enum.c						\
	dtp94-enum.h

libdtp94_la_LIBADD =						\
	$(GUSB_LIBS)						\
	$(COLORD_LIBS)						\
	$(GLIB_LIBS)

libdtp94_la_LDFLAGS =						\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^dtp94_.*'

libdtp94_la_CFLAGS =						\
	$(WARNINGFLAGS_C)

CLEANFILES = $(BUILT_SOURCES)

clean-local:
	rm -f *~

-include $(top_srcdir)/git.mk
