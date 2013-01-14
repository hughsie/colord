AM_CPPFLAGS =						\
	$(GLIB_CFLAGS)					\
	$(DBUS_CFLAGS)

if CD_BUILD_EXAMPLES
noinst_PROGRAMS =					\
	cd-dbus-create-device				\
	cd-dbus-use-qualifier

cd_dbus_create_device_SOURCES =				\
	cd-dbus-create-device.c

cd_dbus_create_device_LDADD =				\
	$(GLIB_LIBS)					\
	$(DBUS_LIBS)

cd_dbus_create_device_CFLAGS =				\
	$(WARNINGFLAGS_C)

cd_dbus_use_qualifier_SOURCES =				\
	cd-dbus-use-qualifier.c

cd_dbus_use_qualifier_LDADD =				\
	$(DBUS_LIBS)

cd_dbus_use_qualifier_CFLAGS =				\
	$(WARNINGFLAGS_C)
endif

-include $(top_srcdir)/git.mk
