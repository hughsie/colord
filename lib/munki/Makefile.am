AM_CPPFLAGS = \
	$(GLIB_CFLAGS)						\
	$(GUSB_CFLAGS)						\
	-I$(top_srcdir)/lib					\
	-I$(top_srcdir)/lib/colord				\
	-I$(top_builddir)/lib					\
	-DCD_COMPILATION					\
	-DMUNKI_COMPILATION					\
	-DG_USB_API_IS_SUBJECT_TO_CHANGE			\
	-DG_LOG_DOMAIN=\"libmunki\"

COLORD_LIBS =							\
	$(top_builddir)/lib/colord/libcolordprivate.la

privlibdir=$(libdir)/colord-sensors
privlib_LTLIBRARIES =						\
	libmunki-private.la

libmunki_private_la_SOURCES =					\
	munki.h							\
	munki-enum.c						\
	munki-enum.h

libmunki_private_la_LIBADD =					\
	$(GUSB_LIBS)						\
	$(COLORD_LIBS)						\
	$(GLIB_LIBS)

libmunki_private_la_LDFLAGS =					\
	-export-dynamic						\
	-no-undefined						\
	-avoid-version                                          \
	-export-symbols-regex '^munki_.*'

libmunki_private_la_CFLAGS =					\
	$(WARNINGFLAGS_C)

CLEANFILES = $(BUILT_SOURCES)

clean-local:
	rm -f *~

-include $(top_srcdir)/git.mk
