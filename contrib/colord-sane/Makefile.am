if HAVE_SANE

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(GUDEV_CFLAGS)					\
	$(SANE_CFLAGS)					\
	$(DBUS_CFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/libcolord			\
	-DG_LOG_DOMAIN=\"CdSane\"			\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DVERSION="\"$(VERSION)\""			\
	-DCD_COMPILATION				\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DLOCALEDIR=\""$(localedir)"\"

COLORD_LIBS =						\
	$(top_builddir)/libcolord/libcolord.la

libexec_PROGRAMS =					\
	colord-sane

colord_sane_SOURCES =					\
	cd-main.c

colord_sane_LDADD =					\
	$(COLORD_LIBS)					\
	$(GUDEV_LIBS)					\
	$(SANE_LIBS)					\
	$(GLIB_LIBS)					\
	$(DBUS_LIBS)

colord_sane_CFLAGS =					\
	$(WARNINGFLAGS_C)

DISTCLEANFILES =					\
	$(dist_profile_DATA)

-include $(top_srcdir)/git.mk
endif
