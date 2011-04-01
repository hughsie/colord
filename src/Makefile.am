introspectiondir = $(datadir)/dbus-1/interfaces
dist_introspection_DATA = 					\
	org.freedesktop.ColorManager.xml			\
	org.freedesktop.ColorManager.Device.xml			\
	org.freedesktop.ColorManager.Profile.xml

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(POLKIT_CFLAGS)				\
	$(LCMS_CFLAGS)					\
	$(GUDEV_CFLAGS)					\
	$(SANE_CFLAGS)					\
	$(SQLITE_CFLAGS)				\
	$(USB_CFLAGS)					\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DLOCALSTATEDIR=\""$(localstatedir)"\"		\
	-DVERSION="\"$(VERSION)\""			\
	-DCD_COMPILATION				\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DCD_SYSTEM_PROFILES_DIR="\"$(CD_SYSTEM_PROFILES_DIR)"\" \
	-DLOCALEDIR=\""$(localedir)"\"

libexec_PROGRAMS =					\
	colord

colord_SOURCES =					\
	cd-buffer.c					\
	cd-buffer.h					\
	cd-common.c					\
	cd-common.h					\
	cd-config.c					\
	cd-config.h					\
	cd-device-array.c				\
	cd-device-array.h				\
	cd-device.c					\
	cd-device-db.c					\
	cd-device-db.h					\
	cd-device.h					\
	cd-inhibit.c					\
	cd-inhibit.h					\
	cd-mapping-db.c					\
	cd-mapping-db.h					\
	cd-math.c					\
	cd-math.h					\
	cd-profile-array.c				\
	cd-profile-array.h				\
	cd-profile.c					\
	cd-profile.h					\
	cd-profile-store.c				\
	cd-profile-store.h				\
	cd-sane-client.c				\
	cd-sane-client.h				\
	cd-udev-client.c				\
	cd-udev-client.h				\
	cd-usb.c					\
	cd-usb.h					\
	../libcolord/cd-enum.c				\
	cd-main.c

colord_LDADD =						\
	$(SQLITE_LIBS)					\
	$(POLKIT_LIBS)					\
	$(LCMS_LIBS)					\
	$(GUDEV_LIBS)					\
	$(SANE_LIBS)					\
	$(USB_LIBS)					\
	$(GLIB_LIBS)

colord_CFLAGS =						\
	$(WARNINGFLAGS_C)

check_PROGRAMS =					\
	cd-self-test

cd_self_test_SOURCES =					\
	cd-buffer.c					\
	cd-buffer.h					\
	cd-common.c					\
	cd-common.h					\
	cd-device-array.c				\
	cd-device-array.h				\
	cd-device.c					\
	cd-device-db.c					\
	cd-device-db.h					\
	cd-device.h					\
	cd-inhibit.c					\
	cd-inhibit.h					\
	cd-mapping-db.c					\
	cd-mapping-db.h					\
	cd-math.c					\
	cd-math.h					\
	cd-profile-array.c				\
	cd-profile-array.h				\
	cd-profile.c					\
	cd-profile.h					\
	cd-usb.c					\
	cd-usb.h					\
	../libcolord/cd-enum.c				\
	cd-self-test.c

cd_self_test_LDADD =					\
	$(POLKIT_LIBS)					\
	$(LCMS_LIBS)					\
	$(SQLITE_LIBS)					\
	$(USB_LIBS)					\
	$(GLIB_LIBS)

cd_self_test_CFLAGS = $(WARNINGFLAGS_C)

install-data-hook:
	if test -w $(DESTDIR)$(prefix)/; then \
		mkdir -p $(DESTDIR)$(CD_SYSTEM_PROFILES_DIR); \
	fi

TESTS = cd-self-test
