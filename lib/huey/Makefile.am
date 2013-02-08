AM_CPPFLAGS = \
	$(GLIB_CFLAGS)						\
	$(GUSB_CFLAGS)						\
	-I$(top_srcdir)/lib					\
	-I$(top_srcdir)/lib/colord				\
	-I$(top_builddir)/lib					\
	-DCD_COMPILATION					\
	-DHUEY_COMPILATION					\
	-DG_USB_API_IS_SUBJECT_TO_CHANGE			\
	-DG_LOG_DOMAIN=\"libhuey\"

COLORD_LIBS =							\
	$(top_builddir)/lib/colord/libcolordprivate.la

lib_LTLIBRARIES =						\
	libhuey-private.la

libhuey_private_la_SOURCES =					\
	huey.h							\
	huey-ctx.c						\
	huey-ctx.h						\
	huey-device.c						\
	huey-device.h						\
	huey-enum.c						\
	huey-enum.h

libhuey_private_la_LIBADD =					\
	$(GUSB_LIBS)						\
	$(COLORD_LIBS)						\
	$(GLIB_LIBS)

libhuey_private_la_LDFLAGS =					\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^huey_.*'

libhuey_private_la_CFLAGS =					\
	$(WARNINGFLAGS_C)

CLEANFILES = $(BUILT_SOURCES)

clean-local:
	rm -f *~

-include $(top_srcdir)/git.mk
