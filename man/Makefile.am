man_MANS_DIST = 					\
	colormgr.1					\
	cd-create-profile.1				\
	colord.conf.1					\
	cd-fix-profile.1

EXTRA_DIST =						\
	colormgr.sgml					\
	cd-create-profile.sgml				\
	cd-fix-profile.sgml				\
	colord.conf.sgml				\
	$(man_MANS_DIST)

if HAVE_DOCBOOK2MAN
man_MANS =						\
	$(man_MANS_DIST)
endif

if HAVE_DOCBOOK2MAN
colormgr.1: colormgr.sgml
	$(AM_V_GEN)					\
	docbook2man $? > /dev/null
cd-create-profile.1: cd-create-profile.sgml
	$(AM_V_GEN)					\
	docbook2man $? > /dev/null
cd-fix-profile.1: cd-fix-profile.sgml
	$(AM_V_GEN)					\
	docbook2man $? > /dev/null
colord.conf.1: colord.conf.sgml
	$(AM_V_GEN)					\
	docbook2man $? > /dev/null
endif

MAINTAINERCLEANFILES =					\
	manpage.links					\
	manpage.log					\
	manpage.refs					\
	$(man_MANS)

clean-local :
	rm -f *~
	rm -f *.1
	rm -f manpage.*

-include $(top_srcdir)/git.mk
