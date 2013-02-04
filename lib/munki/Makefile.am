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

lib_LTLIBRARIES =						\
	libmunki.la

libmunki_la_SOURCES =						\
	munki.h							\
	munki-enum.c						\
	munki-enum.h

libmunki_la_LIBADD =						\
	$(GUSB_LIBS)						\
	$(COLORD_LIBS)						\
	$(GLIB_LIBS)

libmunki_la_LDFLAGS =						\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^munki_.*'

libmunki_la_CFLAGS =						\
	$(WARNINGFLAGS_C)

CLEANFILES = $(BUILT_SOURCES)

clean-local:
	rm -f *~

-include $(top_srcdir)/git.mk
