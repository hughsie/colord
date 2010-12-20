EXTRA_DIST =						\
	colormgr.sgml

if HAVE_DOCBOOK2MAN
man_MANS =						\
	colormgr.1
endif

if HAVE_DOCBOOK2MAN
colormgr.1: colormgr.sgml
	docbook2man $? > /dev/null
endif

clean-local :
	rm -f *~
	rm -f *.1
	rm -f manpage.*
