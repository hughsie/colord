introspectiondir = $(datadir)/dbus-1/interfaces
dist_introspection_DATA = 					\
	org.freedesktop.ColorManager.xml			\
	org.freedesktop.ColorManager.Device.xml			\
	org.freedesktop.ColorManager.Profile.xml

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(POLKIT_CFLAGS)				\
	$(LCMS_CFLAGS)					\
	$(SQLITE_CFLAGS)				\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DLOCALSTATEDIR=\""$(localstatedir)"\"		\
	-DVERSION="\"$(VERSION)\""			\
	-DCD_COMPILATION				\
	-DLOCALEDIR=\""$(localedir)"\"

libexec_PROGRAMS =					\
	colord

colord_SOURCES =					\
	cd-common.c					\
	cd-common.h					\
	cd-device-array.c				\
	cd-device-array.h				\
	cd-device.c					\
	cd-device.h					\
	cd-profile-array.c				\
	cd-profile-array.h				\
	cd-profile.c					\
	cd-profile.h					\
	cd-profile-store.c				\
	cd-profile-store.h				\
	cd-mapping-db.c					\
	cd-mapping-db.h					\
	cd-device-db.c					\
	cd-device-db.h					\
	cd-main.c

colord_LDADD =						\
	$(SQLITE_LIBS)					\
	$(POLKIT_LIBS)					\
	$(LCMS_LIBS)					\
	$(GLIB_LIBS)

colord_CFLAGS =						\
	$(WARNINGFLAGS_C)

check_PROGRAMS =					\
	cd-self-test

cd_self_test_SOURCES =					\
	cd-common.c					\
	cd-common.h					\
	cd-mapping-db.c					\
	cd-mapping-db.h					\
	cd-device-db.c					\
	cd-device-db.h					\
	cd-self-test.c

cd_self_test_LDADD =					\
	$(POLKIT_LIBS)					\
	$(SQLITE_LIBS)					\
	$(GLIB_LIBS)

cd_self_test_CFLAGS = $(WARNINGFLAGS_C)

TESTS = cd-self-test
