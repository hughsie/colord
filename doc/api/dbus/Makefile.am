
all : org.freedesktop.ColorManager.ref.xml org.freedesktop.ColorManager.Device.ref.xml org.freedesktop.ColorManager.Profile.ref.xml org.freedesktop.ColorManager.Sensor.ref.xml

org.freedesktop.ColorManager.ref.xml : $(top_srcdir)/src/org.freedesktop.ColorManager.xml $(top_srcdir)/doc/api/dbus/spec-to-docbook.xsl
	$(AM_V_GEN)					\
	echo "<?xml version=\"1.0\"?>""<!DOCTYPE refentry PUBLIC \"-//OASIS//DTD DocBook XML V4.1.2//EN\" \"http://www.oasis-open.org/docbook/xml/4.1.2/docbookx.dtd\">" > $@
	$(AM_V_GEN)					\
	$(XSLTPROC) $(top_srcdir)/doc/api/dbus/spec-to-docbook.xsl $< | tail -n +2 >> $@

org.freedesktop.ColorManager.Device.ref.xml : $(top_srcdir)/src/org.freedesktop.ColorManager.Device.xml $(top_srcdir)/doc/api/dbus/spec-to-docbook.xsl
	$(AM_V_GEN)					\
	echo "<?xml version=\"1.0\"?>""<!DOCTYPE refentry PUBLIC \"-//OASIS//DTD DocBook XML V4.1.2//EN\" \"http://www.oasis-open.org/docbook/xml/4.1.2/docbookx.dtd\">" > $@
	$(AM_V_GEN)					\
	$(XSLTPROC) $(top_srcdir)/doc/api/dbus/spec-to-docbook.xsl $< | tail -n +2 >> $@

org.freedesktop.ColorManager.Profile.ref.xml : $(top_srcdir)/src/org.freedesktop.ColorManager.Profile.xml $(top_srcdir)/doc/api/dbus/spec-to-docbook.xsl
	$(AM_V_GEN)					\
	echo "<?xml version=\"1.0\"?>""<!DOCTYPE refentry PUBLIC \"-//OASIS//DTD DocBook XML V4.1.2//EN\" \"http://www.oasis-open.org/docbook/xml/4.1.2/docbookx.dtd\">" > $@
	$(AM_V_GEN)					\
	$(XSLTPROC) $(top_srcdir)/doc/api/dbus/spec-to-docbook.xsl $< | tail -n +2 >> $@

org.freedesktop.ColorManager.Sensor.ref.xml : $(top_srcdir)/src/org.freedesktop.ColorManager.Sensor.xml $(top_srcdir)/doc/api/dbus/spec-to-docbook.xsl
	$(AM_V_GEN)					\
	echo "<?xml version=\"1.0\"?>""<!DOCTYPE refentry PUBLIC \"-//OASIS//DTD DocBook XML V4.1.2//EN\" \"http://www.oasis-open.org/docbook/xml/4.1.2/docbookx.dtd\">" > $@
	$(AM_V_GEN)					\
	$(XSLTPROC) $(top_srcdir)/doc/api/dbus/spec-to-docbook.xsl $< | tail -n +2 >> $@

EXTRA_DIST = spec-to-docbook.xsl dbus-introspect-docs.dtd

clean-local :
	rm -f *~ *.ref.xml

MAINTAINERCLEANFILES =					\
	*.ref.xml

-include $(top_srcdir)/git.mk
