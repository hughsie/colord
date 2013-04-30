SUBDIRS =						\
	colord

if BUILD_LIBCOLORDCOMPAT
SUBDIRS +=						\
	compat
endif

if HAVE_GUSB
SUBDIRS +=						\
	colorhug					\
	dtp94						\
	huey						\
	munki
endif

-include $(top_srcdir)/git.mk
