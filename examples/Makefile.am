INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(DBUS_CFLAGS)

if CD_BUILD_EXAMPLES
noinst_PROGRAMS =					\
	cd-dbus-create-device

cd_dbus_create_device_SOURCES =				\
	cd-dbus-create-device.c

cd_dbus_create_device_LDADD =				\
	$(GLIB_LIBS)					\
	$(DBUS_LIBS)

cd_dbus_create_device_CFLAGS =				\
	$(WARNINGFLAGS_C)
endif
