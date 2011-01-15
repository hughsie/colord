/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "cd-common.h"
#include "cd-device.h"
#include "cd-profile.h"
#include "cd-device-array.h"
#include "cd-profile-array.h"
#include "cd-profile-store.h"
#include "cd-device-db.h"
#include "cd-mapping-db.h"
#include "cd-udev-client.h"

static GDBusConnection *connection = NULL;
static GDBusNodeInfo *introspection_daemon = NULL;
static GDBusNodeInfo *introspection_device = NULL;
static GDBusNodeInfo *introspection_profile = NULL;
static GMainLoop *loop = NULL;
static CdDeviceArray *devices_array = NULL;
static CdProfileArray *profiles_array = NULL;
static CdProfileStore *profile_store = NULL;
static CdMappingDb *mapping_db = NULL;
static CdDeviceDb *device_db = NULL;
static CdUdevClient *udev_client = NULL;

/**
 * cd_main_profile_removed:
 **/
static void
cd_main_profile_removed (CdProfile *profile)
{
	gboolean ret;
	gchar *object_path_tmp;
	CdDevice *device_tmp;
	GError *error = NULL;
	GPtrArray *devices;
	guint i;

	/* remove from the array before emitting */
	object_path_tmp = g_strdup (cd_profile_get_object_path (profile));
	cd_profile_array_remove (profiles_array, profile);

	/* try to remove this profile from all devices */
	devices = cd_device_array_get_array (devices_array);
	for (i=0; i<devices->len; i++) {
		device_tmp = g_ptr_array_index (devices, i);
		ret = cd_device_remove_profile (device_tmp,
						object_path_tmp,
						NULL);
		if (ret) {
			g_debug ("automatically removing %s from %s as removed",
				 object_path_tmp,
				 cd_device_get_object_path (device_tmp));
		}
	}

	/* emit signal */
	g_debug ("Emitting ProfileRemoved(%s)", object_path_tmp);
	ret = g_dbus_connection_emit_signal (connection,
					     NULL,
					     COLORD_DBUS_PATH,
					     COLORD_DBUS_INTERFACE,
					     "ProfileRemoved",
					     g_variant_new ("(o)",
							    object_path_tmp),
					     &error);
	if (!ret) {
		g_warning ("failed to send signal %s", error->message);
		g_error_free (error);
	}
	g_free (object_path_tmp);
	g_ptr_array_unref (devices);
}

/**
 * cd_main_profile_invalidate_cb:
 **/
static void
cd_main_profile_invalidate_cb (CdProfile *profile,
			       gpointer user_data)
{
	g_debug ("profile '%s' invalidated",
		 cd_profile_get_id (profile));
	cd_main_profile_removed (profile);
}

/**
 * cd_main_device_removed:
 **/
static void
cd_main_device_removed (CdDevice *device)
{
	gboolean ret;
	gchar *object_path_tmp;
	GError *error = NULL;

	/* remove from the array before emitting */
	object_path_tmp = g_strdup (cd_device_get_object_path (device));
	g_debug ("Removing device %s", object_path_tmp);
	cd_device_array_remove (devices_array, device);

	/* remove from the device database */
	if (cd_device_get_scope (device) == CD_OBJECT_SCOPE_DISK) {
		ret = cd_device_db_remove (device_db,
					   cd_device_get_id (device),
					   &error);
		if (!ret) {
			g_warning ("failed to remove device %s from db: %s",
				   cd_device_get_object_path (device),
				   error->message);
			g_clear_error (&error);
		}
	}

	/* emit signal */
	g_debug ("Emitting DeviceRemoved(%s)", object_path_tmp);
	ret = g_dbus_connection_emit_signal (connection,
					     NULL,
					     COLORD_DBUS_PATH,
					     COLORD_DBUS_INTERFACE,
					     "DeviceRemoved",
					     g_variant_new ("(o)",
							    object_path_tmp),
					     &error);
	if (!ret) {
		g_warning ("failed to send signal %s", error->message);
		g_error_free (error);
	}
	g_free (object_path_tmp);
}

/**
 * cd_main_device_invalidate_cb:
 **/
static void
cd_main_device_invalidate_cb (CdDevice *device,
			      gpointer user_data)
{
	g_debug ("CdMain: device '%s' invalidated",
		 cd_device_get_id (device));
	cd_main_device_removed (device);
}

/**
 * cd_main_add_profile:
 **/
static gboolean
cd_main_add_profile (CdProfile *profile,
		     GError **error)
{
	gboolean ret;

	/* register object */
	ret = cd_profile_register_object (profile,
					  connection,
					  introspection_profile->interfaces[0],
					  error);
	if (!ret)
		goto out;

	/* add */
	cd_profile_array_add (profiles_array, profile);
	g_debug ("Adding profile %s", cd_profile_get_object_path (profile));

	/* profile is no longer valid */
	g_signal_connect (profile, "invalidate",
			  G_CALLBACK (cd_main_profile_invalidate_cb),
			  NULL);

	/* emit signal */
	g_debug ("Emitting ProfileAdded(%s)",
		 cd_profile_get_object_path (profile));
	ret = g_dbus_connection_emit_signal (connection,
					     NULL,
					     COLORD_DBUS_PATH,
					     COLORD_DBUS_INTERFACE,
					     "ProfileAdded",
					     g_variant_new ("(o)",
							    cd_profile_get_object_path (profile)),
					     error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * cd_main_create_profile:
 **/
static CdProfile *
cd_main_create_profile (const gchar *sender,
			const gchar *profile_id,
			guint options,
			GError **error)
{
	gboolean ret;
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;

	g_assert (connection != NULL);

	/* create an object */
	profile_tmp = cd_profile_new ();
	cd_profile_set_id (profile_tmp, profile_id);
	cd_profile_set_scope (profile_tmp, options);

	/* add the profile */
	ret = cd_main_add_profile (profile_tmp, error);
	if (!ret)
		goto out;

	/* different persistent options */
	if (options == CD_OBJECT_SCOPE_NORMAL) {
		g_debug ("normal profile");
	} else if ((options & CD_OBJECT_SCOPE_TEMPORARY) > 0) {
		g_debug ("temporary profile");
		/* setup DBus watcher */
		cd_profile_watch_sender (profile, sender);
	} else if ((options & CD_OBJECT_SCOPE_DISK) > 0) {
		g_debug ("persistant profile");
		//FIXME: save to disk
	} else {
		g_warning ("Unsupported options kind: %i", options);
	}

	/* success */
	profile = g_object_ref (profile_tmp);
out:
	g_object_unref (profile_tmp);
	return profile;
}

/**
 * cd_main_device_auto_add_profiles:
 **/
static void
cd_main_device_auto_add_profiles (CdDevice *device)
{
	CdProfile *profile_tmp;
	const gchar *object_path_tmp;
	gboolean ret;
	GError *error = NULL;
	GPtrArray *array;
	guint i;

	/* get data */
	array = cd_mapping_db_get_profiles (mapping_db,
					    cd_device_get_object_path (device),
					    &error);
	if (array == NULL) {
		g_warning ("failed to get profiles for device from db: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* try to add them */
	for (i=0; i<array->len; i++) {
		object_path_tmp = g_ptr_array_index (array, i);
		profile_tmp = cd_profile_array_get_by_object_path (profiles_array,
								   object_path_tmp);
		if (profile_tmp != NULL) {
			g_debug ("Automatically add %s to %s",
				 object_path_tmp,
				 cd_device_get_object_path (device));
			ret = cd_device_add_profile (device,
						     object_path_tmp,
						     &error);
			if (!ret) {
				g_debug ("failed to assign, non-fatal: %s",
					 error->message);
				g_clear_error (&error);
			}
			g_object_unref (profile_tmp);
		} else {
			g_debug ("profile %s is not (yet) available",
				 object_path_tmp);
		}
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
}

/**
 * cd_main_create_device:
 **/
static CdDevice *
cd_main_create_device (const gchar *sender,
		       const gchar *device_id,
		       guint options,
		       GError **error)
{
	gboolean ret;
	CdDevice *device;
	CdDevice *device_actual = NULL;
	GError *error_local = NULL;

	g_assert (connection != NULL);

	/* create an object */
	device = cd_device_new ();
	cd_device_set_id (device, device_id);
	cd_device_set_scope (device, options);
	cd_device_array_add (devices_array, device);
	g_debug ("Adding device %s", cd_device_get_object_path (device));

	/* register object */
	ret = cd_device_register_object (device,
					 connection,
					 introspection_device->interfaces[0],
					 error);
	if (!ret)
		goto out;

	/* different persistent options */
	if (options == CD_OBJECT_SCOPE_NORMAL) {
		g_debug ("normal device");
	} else if ((options & CD_OBJECT_SCOPE_TEMPORARY) > 0) {
		g_debug ("temporary device");
		/* setup DBus watcher */
		cd_device_watch_sender (device, sender);
	} else if ((options & CD_OBJECT_SCOPE_DISK) > 0) {
		g_debug ("persistant device");

		/* add to the device database */
		if (sender != NULL) {
			ret = cd_device_db_add (device_db,
						device_id,
						&error_local);
			if (!ret) {
				g_warning ("failed to add device %s to db: %s",
					   cd_device_get_object_path (device),
					   error_local->message);
				g_clear_error (&error_local);
			}
		}
	} else {
		g_warning ("Unsupported options kind: %i", options);
	}

	/* profile is no longer valid */
	g_signal_connect (device, "invalidate",
			  G_CALLBACK (cd_main_device_invalidate_cb),
			  NULL);

	/* emit signal */
	g_debug ("Emitting DeviceAdded(%s)",
		 cd_device_get_object_path (device));
	ret = g_dbus_connection_emit_signal (connection,
					     NULL,
					     COLORD_DBUS_PATH,
					     COLORD_DBUS_INTERFACE,
					     "DeviceAdded",
					     g_variant_new ("(o)",
							    cd_device_get_object_path (device)),
					     error);
	if (!ret)
		goto out;

	/* auto add profiles from the database */
	cd_main_device_auto_add_profiles (device);

	/* success */
	device_actual = g_object_ref (device);
out:
	g_object_unref (device);
	return device_actual;
}

/**
 * cd_main_object_path_array_to_variant:
 **/
static GVariant *
cd_main_object_path_array_to_variant (GPtrArray *array)
{
	CdDevice *device;
	guint i;
	GVariant *variant;
	GVariant **variant_array = NULL;

	/* copy the object paths */
	variant_array = g_new0 (GVariant *, array->len + 1);
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);
		variant_array[i] = g_variant_new_object_path (cd_device_get_object_path (device));
	}

	/* format the value */
	variant = g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
				       variant_array,
				       array->len);
	return variant;
}

/**
 * cd_main_profile_auto_add_to_device:
 **/
static void
cd_main_profile_auto_add_to_device (CdProfile *profile)
{
	CdDevice *device_tmp;
	const gchar *object_path_tmp;
	gboolean ret;
	GError *error = NULL;
	GPtrArray *array;
	guint i;

	/* get data */
	array = cd_mapping_db_get_devices (mapping_db,
					   cd_profile_get_object_path (profile),
					   &error);
	if (array == NULL) {
		g_warning ("failed to get profiles for device from db: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* try to add them */
	for (i=0; i<array->len; i++) {
		object_path_tmp = g_ptr_array_index (array, i);
		device_tmp = cd_device_array_get_by_object_path (devices_array,
								 object_path_tmp);
		if (device_tmp != NULL) {
			g_debug ("Automatically add %s to %s",
				 cd_profile_get_object_path (profile),
				 object_path_tmp);
			ret = cd_device_add_profile (device_tmp,
						     cd_profile_get_object_path (profile),
						     &error);
			if (!ret) {
				g_debug ("failed to assign, non-fatal: %s",
					 error->message);
				g_clear_error (&error);
			}
			g_object_unref (device_tmp);
		} else {
			g_debug ("device %s is not (yet) available",
				 object_path_tmp);
		}
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
}

/**
 * cd_main_daemon_method_call:
 **/
static void
cd_main_daemon_method_call (GDBusConnection *connection_, const gchar *sender,
			    const gchar *object_path, const gchar *interface_name,
			    const gchar *method_name, GVariant *parameters,
			    GDBusMethodInvocation *invocation, gpointer user_data)
{
	CdDevice *device = NULL;
	CdProfile *profile = NULL;
	gboolean ret;
	gchar *device_id = NULL;
	gchar *object_path_tmp = NULL;
	GError *error = NULL;
	GPtrArray *array = NULL;
	guint options;
	GVariant *tuple = NULL;
	GVariant *value = NULL;

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetDevices") == 0) {

		/* format the value */
		array = cd_device_array_get_array (devices_array);
		value = cd_main_object_path_array_to_variant (array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetDevicesByKind") == 0) {

		/* get all the devices that match this type */
		g_variant_get (parameters, "(s)", &device_id);
		array = cd_device_array_get_by_kind (devices_array,
						     device_id);

		/* format the value */
		value = cd_main_object_path_array_to_variant (array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "FindDeviceById") == 0) {

		g_variant_get (parameters, "(s)", &device_id);
		device = cd_device_array_get_by_id (devices_array, device_id);
		if (device == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "device id '%s' does not exists",
							       device_id);
			goto out;
		}

		/* format the value */
		value = g_variant_new ("(o)", cd_device_get_object_path (device));
		g_dbus_method_invocation_return_value (invocation, value);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "FindProfileById") == 0) {

		g_variant_get (parameters, "(s)", &device_id);
		profile = cd_profile_array_get_by_id (profiles_array, device_id);
		if (profile == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "profile id '%s' does not exists",
							       device_id);
			goto out;
		}

		/* format the value */
		value = g_variant_new ("(o)", cd_profile_get_object_path (profile));
		g_dbus_method_invocation_return_value (invocation, value);
		goto out;
	}

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetProfiles") == 0) {

		/* format the value */
		value = cd_profile_array_get_variant (profiles_array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "CreateDevice") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation,
						    sender,
						    "org.freedesktop.color-manager.create-device");
		if (!ret)
			goto out;

		/* does already exist */
		g_variant_get (parameters, "(su)", &device_id, &options);
		device = cd_device_array_get_by_id (devices_array, device_id);
		if (device == NULL) {
			device = cd_main_create_device (sender,
							device_id,
							options,
							&error);
			if (device == NULL) {
				g_warning ("failed to create device: %s",
					   error->message);
				g_dbus_method_invocation_return_gerror (invocation,
									error);
				g_error_free (error);
				goto out;
			}
		}

		/* format the value */
		value = g_variant_new_object_path (cd_device_get_object_path (device));
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "DeleteDevice") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation,
						    sender,
						    "org.freedesktop.color-manager.delete-device");
		if (!ret)
			goto out;

		/* does already exist */
		g_variant_get (parameters, "(s)", &device_id);
		device = cd_device_array_get_by_id (devices_array, device_id);
		if (device == NULL) {
			/* fall back to checking the object path */
			device = cd_device_array_get_by_object_path (devices_array,
								     device_id);
			if (device == NULL) {
				g_dbus_method_invocation_return_error (invocation,
								       CD_MAIN_ERROR,
								       CD_MAIN_ERROR_FAILED,
								       "device id '%s' not found",
								       device_id);
				goto out;
			}
		}

		/* remove from the array, and emit */
		cd_main_device_removed (device);

		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "DeleteProfile") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation,
						    sender,
						    "org.freedesktop.color-manager.create-profile");
		if (!ret)
			goto out;

		/* does already exist */
		g_variant_get (parameters, "(s)", &device_id);
		profile = cd_profile_array_get_by_id (profiles_array, device_id);
		if (profile == NULL) {
			/* fall back to checking the object path */
			profile = cd_profile_array_get_by_object_path (profiles_array,
								       device_id);
			if (profile == NULL) {
				g_dbus_method_invocation_return_error (invocation,
								       CD_MAIN_ERROR,
								       CD_MAIN_ERROR_FAILED,
								       "profile id '%s' not found",
								       device_id);
				goto out;
			}
		}

		/* remove from the array, and emit */
		cd_main_profile_removed (profile);

		/* profile unref'd when removed */
		profile = NULL;

		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "CreateProfile") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation,
						    sender,
						    "org.freedesktop.color-manager.create-profile");
		if (!ret)
			goto out;

		/* does already exist */
		g_variant_get (parameters, "(su)", &device_id, &options);
		profile = cd_profile_array_get_by_id (profiles_array,
						      device_id);
		if (profile != NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "profile object path '%s' already exists",
							       cd_profile_get_object_path (profile));
			goto out;
		}

		/* copy the device path */
		profile = cd_main_create_profile (sender,
						  device_id,
						  options,
						  &error);
		if (profile == NULL) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			goto out;
		}

		/* auto add profiles from the database */
		cd_main_profile_auto_add_to_device (profile);

		/* format the value */
		value = g_variant_new_object_path (cd_profile_get_object_path (profile));
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* we suck */
	g_critical ("failed to process method %s", method_name);
out:
	g_free (object_path_tmp);
	if (array != NULL)
		g_ptr_array_unref (array);
	if (device != NULL)
		g_object_unref (device);
	if (profile != NULL)
		g_object_unref (profile);
	return;
}

/**
 * cd_main_daemon_get_property:
 **/
static GVariant *
cd_main_daemon_get_property (GDBusConnection *connection_, const gchar *sender,
			     const gchar *object_path, const gchar *interface_name,
			     const gchar *property_name, GError **error,
			     gpointer user_data)
{
	GVariant *retval = NULL;

	if (g_strcmp0 (property_name, "DaemonVersion") == 0) {
		retval = g_variant_new_string (VERSION);
	} else {
		g_critical ("failed to get property %s",
			    property_name);
	}

	return retval;
}

/**
 * cd_main_on_bus_acquired_cb:
 **/
static void
cd_main_on_bus_acquired_cb (GDBusConnection *connection_,
			    const gchar *name,
			    gpointer user_data)
{
	guint registration_id;
	static const GDBusInterfaceVTable interface_vtable = {
		cd_main_daemon_method_call,
		cd_main_daemon_get_property,
		NULL
	};

	registration_id = g_dbus_connection_register_object (connection_,
							     COLORD_DBUS_PATH,
							     introspection_daemon->interfaces[0],
							     &interface_vtable,
							     NULL,  /* user_data */
							     NULL,  /* user_data_free_func */
							     NULL); /* GError** */
	g_assert (registration_id > 0);
}

/**
 * cd_main_profile_store_added_cb:
 **/
static void
cd_main_profile_store_added_cb (CdProfileStore *_profile_store,
				CdProfile *profile,
				gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;

	/* just add it to the bus with the title as the ID */
	cd_profile_set_id (profile, cd_profile_get_title (profile));
	ret = cd_main_add_profile (profile, &error);
	if (!ret) {
		g_warning ("failed to add profile: %s",
			   error->message);
		g_error_free (error);
	}
}

/**
 * cd_main_profile_store_removed_cb:
 **/
static void
cd_main_profile_store_removed_cb (CdProfileStore *_profile_store,
				  CdProfile *profile,
				  gpointer user_data)
{
	/* check the profile should be invalidated automatically */
}

/**
 * cd_main_add_disk_device:
 **/
static void
cd_main_add_disk_device (const gchar *device_id)
{
	CdDevice *device;
	const gchar *property;
	gboolean ret;
	gchar *value;
	GError *error = NULL;
	GPtrArray *array_properties;
	guint i;

	device = cd_main_create_device (NULL,
					device_id,
					CD_OBJECT_SCOPE_DISK,
					&error);
	if (device == NULL) {
		g_warning ("failed to create disk device: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	g_debug ("created permanent device %s",
		 cd_device_get_object_path (device));

	/* set properties on the device */
	array_properties = cd_device_db_get_properties (device_db,
							device_id,
							&error);
	if (array_properties == NULL) {
		g_warning ("failed to get props for device %s: %s",
			   device_id, error->message);
		g_error_free (error);
		goto out;
	}
	for (i=0; i<array_properties->len; i++) {
		property = g_ptr_array_index (array_properties, i);
		value = cd_device_db_get_property (device_db,
						   device_id,
						   property,
						   &error);
		if (value == NULL) {
			g_warning ("failed to get value: %s",
				   error->message);
			g_clear_error (&error);
			goto out;
		}
		ret = cd_device_set_property_internal (device,
						       property,
						       value,
						       FALSE,
						       &error);
		if (value == NULL) {
			g_warning ("failed to set internal prop: %s",
				   error->message);
			g_clear_error (&error);
			goto out;
		}
		g_free (value);
	}
out:
	if (device != NULL)
		g_object_unref (device);
	if (array_properties != NULL)
		g_ptr_array_unref (array_properties);
}

/**
 * cd_main_on_name_acquired_cb:
 **/
static void
cd_main_on_name_acquired_cb (GDBusConnection *connection_,
			     const gchar *name,
			     gpointer user_data)
{
	const gchar *device_id;
	GError *error = NULL;
	GPtrArray *array_devices = NULL;
	guint i;

	g_debug ("acquired name: %s", name);
	connection = g_object_ref (connection_);

	/* add system profiles */
	profile_store = cd_profile_store_new ();
	g_signal_connect (profile_store, "added",
			  G_CALLBACK (cd_main_profile_store_added_cb),
			  user_data);
	g_signal_connect (profile_store, "removed",
			  G_CALLBACK (cd_main_profile_store_removed_cb),
			  user_data);
	cd_profile_store_search (profile_store,
				 CD_PROFILE_STORE_SEARCH_SYSTEM |
				  CD_PROFILE_STORE_SEARCH_VOLUMES |
				  CD_PROFILE_STORE_SEARCH_MACHINE);	

	/* add disk devices */
	array_devices = cd_device_db_get_devices (device_db, &error);
	if (array_devices == NULL) {
		g_warning ("failed to get the disk devices: %s",
			    error->message);
		g_error_free (error);
		goto out;
	}
	for (i=0; i < array_devices->len; i++) {
		device_id = g_ptr_array_index (array_devices, i);
		cd_main_add_disk_device (device_id);
	}

	/* add GUdev devices */
	cd_udev_client_coldplug (udev_client);
out:
	if (array_devices != NULL)
		g_ptr_array_unref (array_devices);
}

/**
 * cd_main_on_name_lost_cb:
 **/
static void
cd_main_on_name_lost_cb (GDBusConnection *connection_,
			 const gchar *name,
			 gpointer user_data)
{
	g_debug ("lost name: %s", name);
	g_main_loop_quit (loop);
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	gboolean ret;
	guint retval = 1;
	guint owner_id = 0;
	GFile *file_daemon = NULL;
	GFile *file_device = NULL;
	GFile *file_profile = NULL;
	gchar *introspection_daemon_data = NULL;
	gchar *introspection_device_data = NULL;
	gchar *introspection_profile_data = NULL;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();

	/* TRANSLATORS: program name */
	g_set_application_name (_("Color Management"));
	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, _("Color Management D-Bus Service"));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* create new objects */
	loop = g_main_loop_new (NULL, FALSE);
	devices_array = cd_device_array_new ();
	profiles_array = cd_profile_array_new ();
	udev_client = cd_udev_client_new ();

	/* connect to the mapping db */
	mapping_db = cd_mapping_db_new ();
	ret = cd_mapping_db_load (mapping_db,
				  LOCALSTATEDIR "/lib/colord/mapping.db",
				  &error);
	if (!ret) {
		g_warning ("failed to load mapping database: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* connect to the device db */
	device_db = cd_device_db_new ();
	ret = cd_device_db_load (device_db,
				 LOCALSTATEDIR "/lib/colord/storage.db",
				 &error);
	if (!ret) {
		g_warning ("failed to load device database: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* load introspection from file */
	file_daemon = g_file_new_for_path (DATADIR "/dbus-1/interfaces/"
					   COLORD_DBUS_INTERFACE ".xml");
	ret = g_file_load_contents (file_daemon, NULL,
				    &introspection_daemon_data,
				    NULL, NULL, &error);
	if (!ret) {
		g_warning ("failed to load introspection: %s", error->message);
		g_error_free (error);
		goto out;
	}
	file_device = g_file_new_for_path (DATADIR "/dbus-1/interfaces/"
					   COLORD_DBUS_INTERFACE_DEVICE ".xml");
	ret = g_file_load_contents (file_device, NULL,
				    &introspection_device_data,
				    NULL, NULL, &error);
	if (!ret) {
		g_warning ("failed to load introspection: %s", error->message);
		g_error_free (error);
		goto out;
	}
	file_profile = g_file_new_for_path (DATADIR "/dbus-1/interfaces/"
					    COLORD_DBUS_INTERFACE_PROFILE ".xml");
	ret = g_file_load_contents (file_profile, NULL,
				    &introspection_profile_data,
				    NULL, NULL, &error);
	if (!ret) {
		g_warning ("failed to load introspection: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* build introspection from XML */
	introspection_daemon = g_dbus_node_info_new_for_xml (introspection_daemon_data,
							     &error);
	if (introspection_daemon == NULL) {
		g_warning ("failed to load daemon introspection: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	introspection_device = g_dbus_node_info_new_for_xml (introspection_device_data,
							     &error);
	if (introspection_device == NULL) {
		g_warning ("failed to load device introspection: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	introspection_profile = g_dbus_node_info_new_for_xml (introspection_profile_data,
							     &error);
	if (introspection_profile == NULL) {
		g_warning ("failed to load profile introspection: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* own the object */
	owner_id = g_bus_own_name (G_BUS_TYPE_SYSTEM,
				   COLORD_DBUS_SERVICE,
				   G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
				    G_BUS_NAME_OWNER_FLAGS_REPLACE,
				   cd_main_on_bus_acquired_cb,
				   cd_main_on_name_acquired_cb,
				   cd_main_on_name_lost_cb,
				   NULL, NULL);

	/* wait */
	g_main_loop_run (loop);

	/* success */
	retval = 0;
out:
	g_free (introspection_daemon_data);
	g_free (introspection_device_data);
	g_free (introspection_profile_data);
	if (udev_client != NULL)
		g_object_unref (udev_client);
	if (profile_store != NULL)
		g_object_unref (profile_store);
	if (mapping_db != NULL)
		g_object_unref (mapping_db);
	if (device_db != NULL)
		g_object_unref (device_db);
	if (devices_array != NULL)
		g_object_unref (devices_array);
	if (profiles_array != NULL)
		g_object_unref (profiles_array);
	if (file_daemon != NULL)
		g_object_unref (file_daemon);
	if (file_device != NULL)
		g_object_unref (file_device);
	if (file_profile != NULL)
		g_object_unref (file_profile);
	if (owner_id > 0)
		g_bus_unown_name (owner_id);
	if (connection != NULL)
		g_object_unref (connection);
	if (introspection_daemon != NULL)
		g_dbus_node_info_unref (introspection_daemon);
	if (introspection_device != NULL)
		g_dbus_node_info_unref (introspection_device);
	if (introspection_profile != NULL)
		g_dbus_node_info_unref (introspection_profile);
	g_main_loop_unref (loop);
	return retval;
}

