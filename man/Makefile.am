EXTRA_DIST =						\
	colormgr.sgml					\
	cd-create-profile.sgml				\
	cd-fix-profile.sgml

if HAVE_DOCBOOK2MAN
man_MANS =						\
	colormgr.1					\
	cd-create-profile.1
	cd-fix-profile.1
endif

if HAVE_DOCBOOK2MAN
colormgr.1: colormgr.sgml
	docbook2man $? > /dev/null
cd-create-profile.1: cd-create-profile.sgml
	docbook2man $? > /dev/null
cd-fix-profile.1: cd-fix-profile.sgml
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
