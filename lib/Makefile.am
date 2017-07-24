SUBDIRS =						\
	colord

if BUILD_LIBCOLORDCOMPAT
SUBDIRS +=						\
	compat
endif

if HAVE_GUSB
SUBDIRS +=						\
	colorhug
endif

-include $(top_srcdir)/git.mk
