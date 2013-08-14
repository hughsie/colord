AM_CPPFLAGS =							\
	$(GLIB_CFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/lib					\
	-I$(top_srcdir)/lib/colord				\
	-I$(top_builddir)/lib					\
	-I.							\
	-DCD_COMPILATION					\
	-DG_LOG_DOMAIN=\"Cd\"

COLORD_LIBS =							\
	$(top_builddir)/lib/colord/libcolord.la

lib_LTLIBRARIES =						\
	libcolordcompat.la

libcolordcompat_includedir = $(includedir)/colord-1/colord
libcolordcompat_include_HEADERS =				\
	cd-compat-edid.h

libcolordcompat_la_SOURCES =					\
	cd-compat-edid.c					\
	cd-compat-edid.h

libcolordcompat_la_LIBADD =					\
	$(GLIB_LIBS)						\
	$(COLORD_LIBS)

libcolordcompat_la_LDFLAGS =					\
	-avoid-version						\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^cd_.*'

libcolordcompat_la_CFLAGS =					\
	$(WARNINGFLAGS_C)

clean-local:
	rm -f *~
	rm -f $(CLEANFILES)

-include $(top_srcdir)/git.mk
