AM_CPPFLAGS =						\
	$(GLIB_CFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/lib				\
	-I$(top_builddir)/lib				\
	-DG_LOG_DOMAIN=\"CdCvs2cmf\"			\
	-DLOCALEDIR=\""$(localedir)"\"

COLORD_LIBS =						\
	$(top_builddir)/lib/colord/libcolordprivate.la

noinst_PROGRAMS =					\
	cd-csv2cmf

cd_csv2cmf_SOURCES =					\
	cd-main.c

cd_csv2cmf_LDADD =					\
	$(COLORD_LIBS)					\
	$(GLIB_LIBS)

cd_csv2cmf_CFLAGS =					\
	$(WARNINGFLAGS_C)

-include $(top_srcdir)/git.mk
