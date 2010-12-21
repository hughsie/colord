introspectiondir = $(datadir)/dbus-1/interfaces
dist_introspection_DATA = 					\
	org.freedesktop.ColorManager.xml			\
	org.freedesktop.ColorManager.Device.xml			\
	org.freedesktop.ColorManager.Profile.xml

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(POLKIT_CFLAGS)				\
	$(LCMS_CFLAGS)					\
	-DG_LOG_DOMAIN=\"Cd\"				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DVERSION="\"$(VERSION)\""			\
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
	cd-main.c

colord_LDADD =						\
	$(POLKIT_LIBS)					\
	$(LCMS_LIBS)					\
	$(GLIB_LIBS)

colord_CFLAGS =						\
	$(WARNINGFLAGS_C)

bin_PROGRAMS =						\
	colormgr

colormgr_SOURCES =					\
	cd-util.c

colormgr_LDADD =					\
	$(GLIB_LIBS)

colormgr_CFLAGS =					\
	$(WARNINGFLAGS_C)
