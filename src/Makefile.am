SUBDIRS = 						\
	plugins						\
	sensors

introspectiondir = $(datadir)/dbus-1/interfaces
dist_introspection_DATA = 				\
	org.freedesktop.ColorManager.xml		\
	org.freedesktop.ColorManager.Device.xml		\
	org.freedesktop.ColorManager.Sensor.xml		\
	org.freedesktop.ColorManager.Profile.xml

AM_CPPFLAGS =						\
	$(PIE_CFLAGS)					\
	$(GLIB_CFLAGS)					\
	$(POLKIT_CFLAGS)				\
	$(LCMS_CFLAGS)					\
	$(GUDEV_CFLAGS)					\
	$(GUSB_CFLAGS)					\
	$(SQLITE_CFLAGS)				\
	$(LIBSYSTEMD_LOGIN_CFLAGS)			\
	-I$(top_srcdir)/lib				\
	-I$(top_builddir)/lib				\
	-I$(top_srcdir)/lib/colord			\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DLIBDIR=\"$(libdir)\"				\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DLOCALSTATEDIR=\""$(localstatedir)"\"		\
	-DVERSION="\"$(VERSION)\""			\
	-DDAEMON_USER="\"$(daemon_user)\""		\
	-DCD_COMPILATION				\
	-DCD_DISABLE_DEPRECATED				\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DG_USB_API_IS_SUBJECT_TO_CHANGE		\
	-DCD_SYSTEM_PROFILES_DIR="\"$(CD_SYSTEM_PROFILES_DIR)"\" \
	-DLOCALEDIR=\""$(localedir)"\"

COLORD_PRIVATE_LIBS =					\
	$(top_builddir)/lib/colord/libcolordprivate.la

cd-resources.c: colord.gresource.xml $(dist_introspection_DATA)
	$(AM_V_GEN)					\
	glib-compile-resources				\
		--sourcedir=$(srcdir)			\
		--sourcedir=$(top_builddir)/data	\
		--target=$@				\
		--generate-source			\
		--c-name cd				\
		$(srcdir)/colord.gresource.xml
cd-resources.h: colord.gresource.xml
	$(AM_V_GEN)					\
	glib-compile-resources				\
		--sourcedir=$(srcdir)			\
		--sourcedir=$(top_builddir)/data	\
		--target=$@				\
		--generate-header			\
		--c-name cd				\
		$(srcdir)/colord.gresource.xml

libexec_PROGRAMS =					\
	colord

colord_SOURCES =					\
	cd-common.c					\
	cd-common.h					\
	cd-debug.c					\
	cd-debug.h					\
	cd-resources.c					\
	cd-resources.h					\
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
	cd-plugin.c					\
	cd-plugin.h					\
	cd-profile-array.c				\
	cd-profile-array.h				\
	cd-profile.c					\
	cd-profile.h					\
	cd-sensor.c					\
	cd-sensor.h					\
	cd-main.c

if HAVE_UDEV
colord_SOURCES +=					\
	cd-sensor-client.c				\
	cd-sensor-client.h
endif

colord_LDADD =						\
	$(LIBM)						\
	$(SQLITE_LIBS)					\
	$(POLKIT_LIBS)					\
	$(LCMS_LIBS)					\
	$(GUDEV_LIBS)					\
	$(GUSB_LIBS)					\
	$(LIBSYSTEMD_LOGIN_LIBS)			\
	$(COLORD_PRIVATE_LIBS)				\
	$(GLIB_LIBS)

colord_LDFLAGS =					\
	$(PIE_LDFLAGS)					\
	$(RELRO_LDFLAGS)

colord_CFLAGS =						\
	$(WARNINGFLAGS_C)

TESTS_ENVIRONMENT =					\
	libtool --mode=execute valgrind			\
	--quiet						\
	--leak-check=full				\
	--show-possibly-lost=no

check_PROGRAMS =					\
	cd-self-test

cd_self_test_SOURCES =					\
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
	cd-profile-array.c				\
	cd-profile-array.h				\
	cd-profile.c					\
	cd-profile.h					\
	cd-resources.c					\
	cd-resources.h					\
	cd-self-test.c

cd_self_test_LDADD =					\
	$(COLORD_PRIVATE_LIBS)				\
	$(POLKIT_LIBS)					\
	$(LCMS_LIBS)					\
	$(SQLITE_LIBS)					\
	$(LIBSYSTEMD_LOGIN_LIBS)			\
	$(GLIB_LIBS)

cd_self_test_CFLAGS = $(WARNINGFLAGS_C)

install-data-hook:
	if test -w $(DESTDIR)$(prefix)/; then \
		mkdir -p $(DESTDIR)$(CD_SYSTEM_PROFILES_DIR); \
		chown $(daemon_user) $(DESTDIR)$(localstatedir)/lib/colord; \
		chmod 0755 $(DESTDIR)$(localstatedir)/lib/colord; \
	fi

BUILT_SOURCES =						\
	cd-resources.c					\
	cd-resources.h

CLEANFILES = $(BUILT_SOURCES) *.log *.trs

EXTRA_DIST =						\
	colord.gresource.xml

TESTS = cd-self-test

-include $(top_srcdir)/git.mk
