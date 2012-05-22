
SUBDIRS = 						\
	tests

dbusdir = $(sysconfdir)/dbus-1/system.d
dist_dbus_DATA = org.freedesktop.ColorManager.conf

profiledir = $(datadir)/color/icc/colord
dist_profile_DATA = x11-colors.icc crayons.icc

dbusservicemaindir = $(datadir)/dbus-1/system-services
dbusservicemain_in_files = org.freedesktop.ColorManager.service.in
dbusservicemain_DATA     = $(dbusservicemain_in_files:.service.in=.service)
$(dbusservicemain_DATA): $(dbusservicemain_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(libexecdir)|" $< | \
	sed -e "s|\@daemon_user\@|$(daemon_user)|" > $@

systemdservicemain_in_files = colord.service.in

if HAVE_SYSTEMD
systemdservicemaindir = $(systemdsystemunitdir)
systemdservicemain_DATA     = $(systemdservicemain_in_files:.service.in=.service)
$(systemdservicemain_DATA): $(systemdservicemain_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(libexecdir)|" $< | \
	sed -e "s|\@daemon_user\@|$(daemon_user)|" > $@
endif

EXTRA_DIST =						\
	$(dbusservicemain_in_files)				\
	org.freedesktop.ColorManager.conf.in

if HAVE_SYSTEMD
EXTRA_DIST += $(systemdservicemain_in_files)
endif

x11-colors.icc:
	$(top_builddir)/client/cd-create-profile --output $@ --named-color-palette ./x11-colors.csv --nc-prefix="X11" --named-color-type srgb -d "X11 Colors" -c "No copyright" -n "Richard Hughes"

crayons.icc:
	$(top_builddir)/client/cd-create-profile --output $@ --named-color-palette ./crayons.csv --nc-prefix="Crayon" --named-color-type lab -d "Crayon Colors" -c "No copyright" -n "Richard Hughes"

gamma-90.icc:
	$(top_builddir)/client/cd-create-profile --output $@ --xorg-gamma "0.9,0.9,0.9" -d "sRGB display 90% Gamma" -c "No copyright" -n "Richard Hughes"

DISTCLEANFILES =					\
	$(dbusservicemain_DATA)				\
	$(dist_profile_DATA)				\
	$(systemdservicemain_DATA)

-include $(top_srcdir)/git.mk
