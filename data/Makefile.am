dbusdir = $(sysconfdir)/dbus-1/system.d
dist_dbus_DATA = org.freedesktop.ColorManager.conf

servicemaindir = $(datadir)/dbus-1/system-services
dist_servicemain_in_files = org.freedesktop.ColorManager.service.in
servicemain_DATA     = $(servicemain_in_files:.service.in=.service)
$(servicemain_DATA): $(servicemain_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(libexecdir)|" $< > $@
