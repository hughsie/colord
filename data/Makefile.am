
SUBDIRS = 						\
	tests

dbusdir = $(sysconfdir)/dbus-1/system.d
dist_dbus_DATA = org.freedesktop.ColorManager.conf

servicemaindir = $(datadir)/dbus-1/system-services
servicemain_in_files = org.freedesktop.ColorManager.service.in
servicemain_DATA     = $(servicemain_in_files:.service.in=.service)
$(servicemain_DATA): $(servicemain_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(libexecdir)|" $< > $@

EXTRA_DIST =						\
	$(servicemain_in_files)

DISTCLEANFILES =					\
	org.freedesktop.ColorManager.service

x11-colors.icc:
	$(top_builddir)/client/cd-create-profile --output $@ --srgb-palette ./x11-colors.csv --nc-prefix="X11" -d "X11 Colors" -c "No copyright" -n "Richard Hughes"
