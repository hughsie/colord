
SUBDIRS = 						\
	tests

dbusdir = $(sysconfdir)/dbus-1/system.d
dist_dbus_DATA = org.freedesktop.ColorManager.conf

profiledir = $(datadir)/color/icc/colord
dist_profile_DATA = x11-colors.icc crayons.icc

servicemaindir = $(datadir)/dbus-1/system-services
servicemain_in_files = org.freedesktop.ColorManager.service.in
servicemain_DATA     = $(servicemain_in_files:.service.in=.service)
$(servicemain_DATA): $(servicemain_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(libexecdir)|" $< | \
	sed -e "s|\@daemon_user\@|$(daemon_user)|" > $@

EXTRA_DIST =						\
	$(servicemain_in_files)				\
	org.freedesktop.ColorManager.conf.in

x11-colors.icc:
	$(top_builddir)/client/cd-create-profile --output $@ --named-color-palette ./x11-colors.csv --nc-prefix="X11" --named-color-type srgb -d "X11 Colors" -c "No copyright" -n "Richard Hughes"

crayons.icc:
	$(top_builddir)/client/cd-create-profile --output $@ --named-color-palette ./crayons.csv --nc-prefix="Crayon" --named-color-type lab -d "Crayon Colors" -c "No copyright" -n "Richard Hughes"

gamma-90.icc:
	$(top_builddir)/client/cd-create-profile --output $@ --xorg-gamma "0.9,0.9,0.9" -d "sRGB display 90% Gamma" -c "No copyright" -n "Richard Hughes"

DISTCLEANFILES =					\
	org.freedesktop.ColorManager.service		\
	$(dist_profile_DATA)

-include $(top_srcdir)/git.mk
