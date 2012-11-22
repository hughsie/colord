
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
	$(top_builddir)/client/cd-create-profile --output $@	\
		create-named-color srgb "X11" ""		\
		./x11-colors.csv				\
		--description "X11 Colors"			\
		--manufacturer "Richard Hughes"

crayons.icc:
	$(top_builddir)/client/cd-create-profile --output $@	\
		create-named-color lab "Crayon" ""		\
		./crayons.csv					\
		--description "Crayon Colors"			\
		--manufacturer "Richard Hughes"

gamma-90.icc:
	$(top_builddir)/client/cd-create-profile --output $@	\
		create-x11-gamma 0.9 0.9 0.9			\
		--description "sRGB display 90% Gamma"		\
		--manufacturer "Richard Hughes"

CLEANFILES =						\
	$(dist_profile_DATA)

DISTCLEANFILES =					\
	$(dbusservicemain_DATA)				\
	$(systemdservicemain_DATA)

-include $(top_srcdir)/git.mk
