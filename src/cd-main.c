/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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
#include <gio/gunixfdlist.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "cd-debug.h"
#include "cd-common.h"
#include "cd-config.h"
#include "cd-device-array.h"
#include "cd-device-db.h"
#include "cd-device.h"
#include "cd-mapping-db.h"
#include "cd-profile-array.h"
#include "cd-profile.h"
#include "cd-profile-store.h"
#include "cd-sane-client.h"
#include "cd-sensor-client.h"
#include "cd-udev-client.h"

static GDBusConnection *connection = NULL;
static GDBusNodeInfo *introspection_daemon = NULL;
static GDBusNodeInfo *introspection_device = NULL;
static GDBusNodeInfo *introspection_profile = NULL;
static GDBusNodeInfo *introspection_sensor = NULL;
static GMainLoop *loop = NULL;
static CdDeviceArray *devices_array = NULL;
static CdProfileArray *profiles_array = NULL;
static CdProfileStore *profile_store = NULL;
static CdMappingDb *mapping_db = NULL;
static CdDeviceDb *device_db = NULL;
#ifdef HAVE_GUDEV
static CdUdevClient *udev_client = NULL;
static CdSensorClient *sensor_client = NULL;
#endif
static CdSaneClient *sane_client = NULL;
static CdConfig *config = NULL;
static GPtrArray *sensors = NULL;
static GHashTable *standard_spaces = NULL;

/**
 * cd_main_profile_removed:
 **/
static void
cd_main_profile_removed (CdProfile *profile)
{
	gboolean ret;
	gchar *object_path_tmp;
	CdDevice *device_tmp;
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
			g_debug ("CdMain: automatically removing %s from %s as removed",
				 object_path_tmp,
				 cd_device_get_object_path (device_tmp));
		}
	}

	/* emit signal */
	g_debug ("CdMain: Emitting ProfileRemoved(%s)", object_path_tmp);
	g_dbus_connection_emit_signal (connection,
				       NULL,
				       COLORD_DBUS_PATH,
				       COLORD_DBUS_INTERFACE,
				       "ProfileRemoved",
				       g_variant_new ("(o)",
							    object_path_tmp),
				       NULL);
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
	g_debug ("CdMain: profile '%s' invalidated",
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
	g_debug ("CdMain: Removing device %s", object_path_tmp);
	cd_device_array_remove (devices_array, device);

	/* remove from the device database */
	if (cd_device_get_scope (device) == CD_OBJECT_SCOPE_DISK) {
		ret = cd_device_db_remove (device_db,
					   cd_device_get_id (device),
					   &error);
		if (!ret) {
			g_warning ("CdMain: failed to remove device %s from db: %s",
				   cd_device_get_object_path (device),
				   error->message);
			g_clear_error (&error);
		}
	}

	/* emit signal */
	g_debug ("CdMain: Emitting DeviceRemoved(%s)", object_path_tmp);
	g_dbus_connection_emit_signal (connection,
				       NULL,
				       COLORD_DBUS_PATH,
				       COLORD_DBUS_INTERFACE,
				       "DeviceRemoved",
				       g_variant_new ("(o)",
							    object_path_tmp),
				       &error);
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
	gboolean ret = TRUE;

	/* add */
	cd_profile_array_add (profiles_array, profile);
	g_debug ("CdMain: Adding profile %s", cd_profile_get_object_path (profile));

	/* profile is no longer valid */
	g_signal_connect (profile, "invalidate",
			  G_CALLBACK (cd_main_profile_invalidate_cb),
			  NULL);

	return ret;
}

/**
 * cd_main_create_profile:
 **/
static CdProfile *
cd_main_create_profile (const gchar *sender,
			const gchar *profile_id,
			CdObjectScope scope,
			GError **error)
{
	gboolean ret;
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;

	g_assert (connection != NULL);

	/* create an object */
	profile_tmp = cd_profile_new ();
	cd_profile_set_id (profile_tmp, profile_id);
	cd_profile_set_scope (profile_tmp, scope);

	/* add the profile */
	ret = cd_main_add_profile (profile_tmp, error);
	if (!ret)
		goto out;

	/* different persistent scope */
	if (scope == CD_OBJECT_SCOPE_NORMAL) {
		g_debug ("CdMain: normal profile");
	} else if ((scope & CD_OBJECT_SCOPE_TEMP) > 0) {
		g_debug ("CdMain: temporary profile");
		/* setup DBus watcher */
		cd_profile_watch_sender (profile_tmp, sender);
	} else if ((scope & CD_OBJECT_SCOPE_DISK) > 0) {
		g_debug ("CdMain: persistent profile");
		g_set_error_literal (error,
				     CD_MAIN_ERROR,
				     CD_MAIN_ERROR_FAILED,
				     "persistent profiles are no yet supported");
		goto out;
	} else {
		g_warning ("CdMain: Unsupported scope kind: %i", scope);
	}

	/* success */
	profile = g_object_ref (profile_tmp);
out:
	g_object_unref (profile_tmp);
	return profile;
}

/**
 * cd_main_device_auto_add_profile_md:
 **/
static gboolean
cd_main_device_auto_add_profile_md (CdDevice *device, CdProfile *profile)
{
	const gchar *device_id;
	gboolean ret = FALSE;
	GError *error = NULL;

	/* does the profile have device metadata */
	device_id = cd_profile_get_metadata_item (profile,
						  CD_PROFILE_METADATA_MAPPING_DEVICE_ID);
	if (device_id == NULL)
		goto out;

	/* does this device match? */
	if (g_strcmp0 (cd_device_get_id (device), device_id) != 0)
		goto out;

	/* auto-add soft relationship */
	g_debug ("CdMain: Automatically MD add %s to %s",
		 cd_profile_get_id (profile),
		 cd_device_get_object_path (device));
	ret = cd_device_add_profile (device,
				     CD_DEVICE_RELATION_SOFT,
				     cd_profile_get_object_path (profile),
				     g_get_real_time (),
				     &error);
	if (!ret) {
		g_debug ("CdMain: failed to assign, non-fatal: %s",
			 error->message);
		g_error_free (error);
		goto out;
	}
out:
	return ret;
}

/**
 * cd_main_device_auto_add_profile_db:
 **/
static gboolean
cd_main_device_auto_add_profile_db (CdDevice *device, CdProfile *profile)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	guint64 timestamp;

	g_debug ("CdMain: Automatically DB add %s to %s",
		 cd_profile_get_id (profile),
		 cd_device_get_object_path (device));
	timestamp = cd_mapping_db_get_timestamp (mapping_db,
						 cd_device_get_id (device),
						 cd_profile_get_id (profile),
						 &error);
	if (timestamp == G_MAXUINT64) {
		g_debug ("CdMain: failed to assign, non-fatal: %s",
			 error->message);
		g_error_free (error);
		goto out;
	}
	ret = cd_device_add_profile (device,
				     CD_DEVICE_RELATION_HARD,
				     cd_profile_get_object_path (profile),
				     timestamp,
				     &error);
	if (!ret) {
		g_debug ("CdMain: failed to assign, non-fatal: %s",
			 error->message);
		g_error_free (error);
		goto out;
	}
out:
	return ret;
}

/**
 * cd_main_device_auto_add_profile:
 **/
static gboolean
cd_main_device_auto_add_profile (CdDevice *device, CdProfile *profile)
{
	gboolean ret;

	/* try adding devices from the mapping db -- we do this first
	 * as the database entries might be hard */
	ret = cd_main_device_auto_add_profile_db (device, profile);
	if (ret)
		goto out;

	/* first try finding profile metadata to the device,
	 * which will be added soft */
	ret = cd_main_device_auto_add_profile_md (device, profile);
	if (ret)
		goto out;
out:
	return ret;
}

/**
 * cd_main_device_auto_add_profiles:
 **/
static void
cd_main_device_auto_add_profiles (CdDevice *device)
{
	CdProfile *profile_tmp;
	const gchar *object_id_tmp;
	GError *error = NULL;
	GPtrArray *array;
	guint i;

	/* get data */
	array = cd_mapping_db_get_profiles (mapping_db,
					    cd_device_get_id (device),
					    &error);
	if (array == NULL) {
		g_warning ("CdMain: failed to get profiles for device from db: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* try to add them */
	for (i=0; i<array->len; i++) {
		object_id_tmp = g_ptr_array_index (array, i);
		profile_tmp = cd_profile_array_get_by_id (profiles_array,
							  object_id_tmp);
		if (profile_tmp != NULL) {
			cd_main_device_auto_add_profile (device, profile_tmp);
			g_object_unref (profile_tmp);
		} else {
			g_debug ("CdMain: profile %s is not (yet) available",
				 object_id_tmp);
		}
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
}

/**
 * cd_main_device_register_on_bus:
 **/
static gboolean
cd_main_device_register_on_bus (CdDevice *device,
				GError **error)
{
	gboolean ret;

	/* register object */
	ret = cd_device_register_object (device,
					 connection,
					 introspection_device->interfaces[0],
					 error);
	if (!ret)
		goto out;

	/* emit signal */
	g_debug ("CdMain: Emitting DeviceAdded(%s)",
		 cd_device_get_object_path (device));
	g_dbus_connection_emit_signal (connection,
				       NULL,
				       COLORD_DBUS_PATH,
				       COLORD_DBUS_INTERFACE,
				       "DeviceAdded",
				       g_variant_new ("(o)",
						      cd_device_get_object_path (device)),
				       NULL);
out:
	return ret;
}

/**
 * cd_main_device_add:
 **/
static gboolean
cd_main_device_add (CdDevice *device,
		    const gchar *sender,
		    GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	CdObjectScope scope;

	/* create an object */
	g_debug ("CdMain: Adding device %s", cd_device_get_object_path (device));

	/* different persistent scope */
	scope = cd_device_get_scope (device);
	if (scope == CD_OBJECT_SCOPE_DISK && sender != NULL) {
		g_debug ("CdMain: persistent device");

		/* add to the device database */
		ret = cd_device_db_add (device_db,
					cd_device_get_id (device),
					&error_local);
		if (!ret)
			goto out;
	}

	/* profile is no longer valid */
	g_signal_connect (device, "invalidate",
			  G_CALLBACK (cd_main_device_invalidate_cb),
			  NULL);

	/* add to array */
	cd_device_array_add (devices_array, device);

	/* auto add profiles from the database */
	cd_main_device_auto_add_profiles (device);
out:
	return ret;
}

/**
 * cd_main_create_device:
 **/
static CdDevice *
cd_main_create_device (const gchar *sender,
		       const gchar *device_id,
		       CdObjectScope scope,
		       CdDeviceMode mode,
		       GError **error)
{
	gboolean ret;
	CdDevice *device_tmp;
	CdDevice *device = NULL;

	g_assert (connection != NULL);

	/* create an object */
	device_tmp = cd_device_new ();
	cd_device_set_id (device_tmp, device_id);
	cd_device_set_scope (device_tmp, scope);
	cd_device_set_mode (device_tmp, mode);
	ret = cd_main_device_add (device_tmp, sender, error);
	if (!ret)
		goto out;

	/* setup DBus watcher */
	if ((scope & CD_OBJECT_SCOPE_TEMP) > 0) {
		g_debug ("temporary device");
		cd_device_watch_sender (device_tmp, sender);
	}

	/* success */
	device = g_object_ref (device_tmp);
out:
	g_object_unref (device_tmp);
	return device;
}

/**
 * cd_main_device_array_to_variant:
 **/
static GVariant *
cd_main_device_array_to_variant (GPtrArray *array)
{
	CdDevice *device;
	guint i;
	guint length = 0;
	GVariant *variant;
	GVariant **variant_array = NULL;

	/* copy the object paths */
	variant_array = g_new0 (GVariant *, array->len + 1);
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);
		variant_array[length] = g_variant_new_object_path (
			cd_device_get_object_path (device));
		length++;
	}

	/* format the value */
	variant = g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
				       variant_array,
				       length);
	g_free (variant_array);
	return variant;
}

/**
 * cd_main_profile_array_to_variant:
 **/
static GVariant *
cd_main_profile_array_to_variant (GPtrArray *array)
{
	CdProfile *profile;
	guint i;
	guint length = 0;
	GVariant *variant;
	GVariant **variant_array = NULL;

	/* copy the object paths */
	variant_array = g_new0 (GVariant *, array->len + 1);
	for (i=0; i<array->len; i++) {
		profile = g_ptr_array_index (array, i);
		variant_array[length] = g_variant_new_object_path (
			cd_profile_get_object_path (profile));
		length++;
	}

	/* format the value */
	variant = g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
				       variant_array,
				       length);
	g_free (variant_array);
	return variant;
}

/**
 * cd_main_sensor_array_to_variant:
 **/
static GVariant *
cd_main_sensor_array_to_variant (GPtrArray *array)
{
	CdSensor *sensor;
	guint i;
	guint length = 0;
	GVariant *variant;
	GVariant **variant_array = NULL;

	/* copy the object paths */
	variant_array = g_new0 (GVariant *, array->len + 1);
	for (i=0; i<array->len; i++) {
		sensor = g_ptr_array_index (array, i);
		variant_array[length] = g_variant_new_object_path (
			cd_sensor_get_object_path (sensor));
		length++;
	}

	/* format the value */
	variant = g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
				       variant_array,
				       length);
	g_free (variant_array);
	return variant;
}

/**
 * cd_main_profile_auto_add_to_device:
 **/
static void
cd_main_profile_auto_add_to_device (CdProfile *profile)
{
	CdDevice *device_tmp;
	const gchar *device_id_tmp;
	GError *error = NULL;
	GPtrArray *array;
	guint i;

	/* get data */
	array = cd_mapping_db_get_devices (mapping_db,
					   cd_profile_get_id (profile),
					   &error);
	if (array == NULL) {
		g_warning ("CdMain: failed to get profiles for device from db: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* nothing set */
	if (array->len == 0) {
		g_debug ("no matched device data for profile %s",
			 cd_profile_get_id (profile));
		goto out;
	}

	/* try to add them */
	for (i=0; i<array->len; i++) {
		device_id_tmp = g_ptr_array_index (array, i);
		device_tmp = cd_device_array_get_by_id (devices_array,
							device_id_tmp);
		if (device_tmp != NULL) {
			cd_main_device_auto_add_profile (device_tmp, profile);
			g_object_unref (device_tmp);
		} else {
			g_debug ("CdMain: device %s is not (yet) available",
				 device_id_tmp);
		}
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
}

/**
 * cd_main_profile_register_on_bus:
 **/
static gboolean
cd_main_profile_register_on_bus (CdProfile *profile,
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

	/* emit signal */
	g_debug ("CdMain: Emitting ProfileAdded(%s)",
		 cd_profile_get_object_path (profile));
	g_dbus_connection_emit_signal (connection,
				       NULL,
				       COLORD_DBUS_PATH,
				       COLORD_DBUS_INTERFACE,
				       "ProfileAdded",
				       g_variant_new ("(o)",
							    cd_profile_get_object_path (profile)),
				       NULL);
out:
	return ret;
}

/**
 * cd_main_get_standard_space_override:
 **/
static CdProfile *
cd_main_get_standard_space_override (const gchar *standard_space)
{
	CdProfile *profile;
	profile = g_hash_table_lookup (standard_spaces,
				       standard_space);
	if (profile == NULL)
		goto out;
	g_object_ref (profile);
out:
	return profile;
}

/**
 * cd_main_get_standard_space_metadata:
 **/
static CdProfile *
cd_main_get_standard_space_metadata (const gchar *standard_space)
{
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;
	gboolean ret;
	GPtrArray *array;
	guint i;

	/* get all the profiles with this metadata */
	array = cd_profile_array_get_by_metadata (profiles_array,
						  "STANDARD_space",
						  standard_space);

	/* just use the first system-wide profile */
	for (i=0; i<array->len; i++) {
		profile_tmp = g_ptr_array_index (array, i);
		ret = cd_profile_get_is_system_wide (profile_tmp);
		if (ret) {
			profile = g_object_ref (profile_tmp);
			goto out;
		}
	}
out:
	g_ptr_array_unref (array);
	return profile;
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
	CdObjectScope scope;
	CdProfile *profile = NULL;
	const gchar *prop_key;
	const gchar *prop_value;
	gboolean register_on_bus = TRUE;
	gboolean ret;
	const gchar *device_id = NULL;
	const gchar *scope_tmp = NULL;
	GError *error = NULL;
	GPtrArray *array = NULL;
	GVariantIter *iter = NULL;
	GVariant *tuple = NULL;
	GVariant *value = NULL;
	gint fd = -1;
	guint uid;
	const gchar *metadata_key = NULL;
	const gchar *metadata_value = NULL;
	GDBusMessage *message;
	GUnixFDList *fd_list;

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetDevices") == 0) {

		g_debug ("CdMain: %s:GetDevices()", sender);

		/* format the value */
		array = cd_device_array_get_array (devices_array);
		value = cd_main_device_array_to_variant (array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetSensors") == 0) {

		g_debug ("CdMain: %s:GetSensors()", sender);

		/* format the value */
		value = cd_main_sensor_array_to_variant (sensors);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetDevicesByKind") == 0) {

		/* get all the devices that match this type */
		g_variant_get (parameters, "(&s)", &device_id);
		g_debug ("CdMain: %s:GetDevicesByKind(%s)",
			 sender, device_id);
		array = cd_device_array_get_by_kind (devices_array,
						     device_id);

		/* format the value */
		value = cd_main_device_array_to_variant (array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetProfilesByKind") == 0) {

		/* get all the devices that match this type */
		g_variant_get (parameters, "(&s)", &scope_tmp);
		g_debug ("CdMain: %s:GetProfilesByKind(%s)",
			 sender, scope_tmp);
		array = cd_profile_array_get_by_kind (profiles_array,
						      cd_profile_kind_from_string (scope_tmp));

		/* format the value */
		value = cd_main_profile_array_to_variant (array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 'o' */
	if (g_strcmp0 (method_name, "FindDeviceById") == 0) {

		g_variant_get (parameters, "(&s)", &device_id);
		g_debug ("CdMain: %s:FindDeviceById(%s)",
			 sender, device_id);
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

	/* return 'o' */
	if (g_strcmp0 (method_name, "FindDeviceByProperty") == 0) {

		g_variant_get (parameters, "(&s&s)",
			       &metadata_key,
			       &metadata_value);
		g_debug ("CdMain: %s:FindDeviceByProperty(%s=%s)",
			 sender, metadata_key, metadata_value);
		device = cd_device_array_get_by_property (devices_array,
							  metadata_key,
							  metadata_value);
		if (device == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "property match '%s'='%s' does not exist",
							       metadata_key,
							       metadata_value);
			goto out;
		}

		/* format the value */
		value = g_variant_new ("(o)", cd_device_get_object_path (device));
		g_dbus_method_invocation_return_value (invocation, value);
		goto out;
	}

	/* return 'o' */
	if (g_strcmp0 (method_name, "FindProfileById") == 0) {

		g_variant_get (parameters, "(&s)", &device_id);
		g_debug ("CdMain: %s:FindProfileById(%s)",
			 sender, device_id);
		profile = cd_profile_array_get_by_id (profiles_array, device_id);
		if (profile == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "profile id '%s' does not exist",
							       device_id);
			goto out;
		}

		/* format the value */
		value = g_variant_new ("(o)", cd_profile_get_object_path (profile));
		g_dbus_method_invocation_return_value (invocation, value);
		goto out;
	}

	/* return 'o' */
	if (g_strcmp0 (method_name, "GetStandardSpace") == 0) {

		g_variant_get (parameters, "(&s)", &device_id);
		g_debug ("CdMain: %s:GetStandardSpace(%s)",
			 sender, device_id);

		/* first search overrides */
		profile = cd_main_get_standard_space_override (device_id);
		if (profile == NULL)
			profile = cd_main_get_standard_space_metadata (device_id);
		if (profile == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "profile space '%s' does not exist",
							       device_id);
			goto out;
		}

		/* format the value */
		value = g_variant_new ("(o)", cd_profile_get_object_path (profile));
		g_dbus_method_invocation_return_value (invocation, value);
		goto out;
	}

	/* return 'o' */
	if (g_strcmp0 (method_name, "FindProfileByFilename") == 0) {

		g_variant_get (parameters, "(&s)", &device_id);
		g_debug ("CdMain: %s:FindProfileByFilename(%s)",
			 sender, device_id);
		profile = cd_profile_array_get_by_filename (profiles_array,
							    device_id);
		if (profile == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "profile filename '%s' does not exist",
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
		g_debug ("CdMain: %s:GetProfiles()", sender);
		value = cd_profile_array_get_variant (profiles_array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "CreateDevice") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation,
						    "org.freedesktop.color-manager.create-device");
		if (!ret)
			goto out;

		/* does already exist */
		g_variant_get (parameters, "(&s&sa{ss})",
			       &device_id,
			       &scope_tmp,
			       &iter);

		g_debug ("CdMain: %s:CreateDevice(%s)", sender, device_id);
		scope = cd_object_scope_from_string (scope_tmp);
		device = cd_device_array_get_by_id (devices_array, device_id);
		if (device != NULL) {
			/* where we try to manually add an existing
			 * virtual device, which means promoting it to
			 * an actual physical device */
			if (cd_device_get_mode (device) == CD_DEVICE_MODE_VIRTUAL) {
				cd_device_set_mode (device,
						    CD_DEVICE_MODE_PHYSICAL);
				register_on_bus = FALSE;
			} else {
				g_dbus_method_invocation_return_error (invocation,
								       CD_MAIN_ERROR,
								       CD_MAIN_ERROR_ALREADY_EXISTS,
								       "device id '%s' already exists",
								       device_id);
				goto out;
			}
		}

		/* create device */
		device = cd_main_create_device (sender,
						device_id,
						scope,
						CD_DEVICE_MODE_UNKNOWN,
						&error);
		if (device == NULL) {
			g_warning ("CdMain: failed to create device: %s",
				   error->message);
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			g_error_free (error);
			goto out;
		}

		/* set the owner */
		uid = cd_main_get_sender_uid (invocation, &error);
		if (uid == G_MAXUINT) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "failed to get owner: %s",
							       error->message);
			g_error_free (error);
			goto out;
		}
		cd_device_set_owner (device, uid);

		/* set the properties */
		while (g_variant_iter_next (iter, "{&s&s}",
					    &prop_key, &prop_value)) {
			ret = cd_device_set_property_internal (device,
							       prop_key,
							       prop_value,
							       (scope == CD_OBJECT_SCOPE_DISK),
							       &error);
			if (!ret) {
				g_warning ("CdMain: failed to set property on device: %s",
					   error->message);
				g_dbus_method_invocation_return_gerror (invocation,
									error);
				g_error_free (error);
				goto out;
			}
		}

		/* register on bus */
		if (register_on_bus) {
			ret = cd_main_device_register_on_bus (device, &error);
			if (!ret) {
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
						    "org.freedesktop.color-manager.delete-device");
		if (!ret)
			goto out;

		/* does already exist */
		g_variant_get (parameters, "(&o)", &device_id);
		g_debug ("CdMain: %s:DeleteDevice(%s)",
			 sender, device_id);
		device = cd_device_array_get_by_id (devices_array, device_id);
		if (device == NULL) {
			/* fall back to checking the object path */
			device = cd_device_array_get_by_object_path (devices_array,
								     device_id);
			if (device == NULL) {
				g_dbus_method_invocation_return_error (invocation,
								       CD_MAIN_ERROR,
								       CD_MAIN_ERROR_FAILED,
								       "device path '%s' not found",
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
						    "org.freedesktop.color-manager.create-profile");
		if (!ret)
			goto out;

		/* does already exist */
		g_variant_get (parameters, "(&o)", &device_id);
		g_debug ("CdMain: %s:DeleteProfile(%s)",
			 sender, device_id);
		profile = cd_profile_array_get_by_object_path (profiles_array, device_id);
		if (profile == NULL) {
			/* fall back to checking the object path */
			profile = cd_profile_array_get_by_object_path (profiles_array,
								       device_id);
			if (profile == NULL) {
				g_dbus_method_invocation_return_error (invocation,
								       CD_MAIN_ERROR,
								       CD_MAIN_ERROR_FAILED,
								       "profile path '%s' not found",
								       device_id);
				goto out;
			}
		}

		/* remove from the array, and emit */
		cd_main_profile_removed (profile);

		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "CreateProfile") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation,
						    "org.freedesktop.color-manager.create-profile");
		if (!ret)
			goto out;

		/* does already exist */
		g_variant_get (parameters, "(&s&sa{ss})",
			       &device_id,
			       &scope_tmp,
			       &iter);
		g_debug ("CdMain: %s:CreateProfile(%s)", sender, device_id);
		profile = cd_profile_array_get_by_id (profiles_array,
						      device_id);
		if (profile != NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_ALREADY_EXISTS,
							       "profile id '%s' already exists",
							       device_id);
			goto out;
		}
		scope = cd_object_scope_from_string (scope_tmp);
		profile = cd_main_create_profile (sender,
						  device_id,
						  scope,
						  &error);
		if (profile == NULL) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			goto out;
		}

		/* set the owner */
		uid = cd_main_get_sender_uid (invocation, &error);
		if (uid == G_MAXUINT) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "failed to get owner: %s",
							       error->message);
			g_error_free (error);
			goto out;
		}
		cd_profile_set_owner (profile, uid);

		/* auto add profiles from the database */
		cd_main_profile_auto_add_to_device (profile);

		/* get any file descriptor in the message */
		message = g_dbus_method_invocation_get_message (invocation);
		fd_list = g_dbus_message_get_unix_fd_list (message);
		if (fd_list != NULL && g_unix_fd_list_get_length (fd_list) == 1) {
			fd = g_unix_fd_list_get (fd_list, 0, &error);
			if (fd < 0) {
				g_warning ("CdMain: failed to get fd from message: %s",
					   error->message);
				g_dbus_method_invocation_return_gerror (invocation,
									error);
				g_error_free (error);
				goto out;
			}

			/* read from a fd, avoiding open() */
			ret = cd_profile_set_fd (profile, fd, &error);
			if (!ret) {
				g_warning ("CdMain: failed to profile from fd: %s",
					   error->message);
				g_dbus_method_invocation_return_gerror (invocation,
									error);
				g_error_free (error);
				goto out;
			}
		}

		/* set the properties */
		while (g_variant_iter_next (iter, "{&s&s}",
					    &prop_key, &prop_value)) {
			ret = cd_profile_set_property_internal (profile,
								prop_key,
								prop_value,
								&error);
			if (!ret) {
				g_warning ("CdMain: failed to set property on profile: %s",
					   error->message);
				g_dbus_method_invocation_return_gerror (invocation,
									error);
				g_error_free (error);
				goto out;
			}
		}

		/* register on bus */
		ret = cd_main_profile_register_on_bus (profile, &error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			g_error_free (error);
			goto out;
		}

		/* format the value */
		value = g_variant_new_object_path (cd_profile_get_object_path (profile));
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* we suck */
	g_critical ("failed to process method %s", method_name);
out:
	if (iter != NULL)
		g_variant_iter_free (iter);
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
	gchar *profile_id;
	GError *error = NULL;

	/* just add it to the bus with the title as the ID */
	profile_id = g_strdup_printf ("icc-%s",
				      cd_profile_get_checksum (profile));
	cd_profile_set_id (profile, profile_id);
	ret = cd_main_add_profile (profile, &error);
	if (!ret) {
		g_warning ("CdMain: failed to add profile: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* register on bus */
	ret = cd_main_profile_register_on_bus (profile, &error);
	if (!ret) {
		g_warning ("CdMain: failed to emit ProfileAdded: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_free (profile_id);
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
	GPtrArray *array_properties = NULL;
	guint i;

	device = cd_main_create_device (NULL,
					device_id,
					CD_OBJECT_SCOPE_DISK,
					CD_DEVICE_MODE_VIRTUAL,
					&error);
	if (device == NULL) {
		g_warning ("CdMain: failed to create disk device: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	g_debug ("CdMain: created permanent device %s",
		 cd_device_get_object_path (device));

	/* set properties on the device */
	array_properties = cd_device_db_get_properties (device_db,
							device_id,
							&error);
	if (array_properties == NULL) {
		g_warning ("CdMain: failed to get props for device %s: %s",
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
			g_warning ("CdMain: failed to get value: %s",
				   error->message);
			g_clear_error (&error);
			goto out;
		}
		ret = cd_device_set_property_internal (device,
						       property,
						       value,
						       FALSE,
						       &error);
		if (!ret) {
			g_warning ("CdMain: failed to set internal prop: %s",
				   error->message);
			g_clear_error (&error);
			goto out;
		}
		g_free (value);
	}

	/* register on bus */
	ret = cd_main_device_register_on_bus (device, &error);
	if (!ret) {
		g_warning ("CdMain: failed to emit DeviceAdded: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (device != NULL)
		g_object_unref (device);
	if (array_properties != NULL)
		g_ptr_array_unref (array_properties);
}

/**
 * cd_main_sensor_register_on_bus:
 **/
static gboolean
cd_main_sensor_register_on_bus (CdSensor *sensor,
				GError **error)
{
	gboolean ret;

	/* register object */
	ret = cd_sensor_register_object (sensor,
					 connection,
					 introspection_sensor->interfaces[0],
					 error);
	if (!ret)
		goto out;

	/* emit signal */
	g_debug ("CdMain: Emitting SensorAdded(%s)",
		 cd_sensor_get_object_path (sensor));
	g_dbus_connection_emit_signal (connection,
				       NULL,
				       COLORD_DBUS_PATH,
				       COLORD_DBUS_INTERFACE,
				       "SensorAdded",
				       g_variant_new ("(o)",
						      cd_sensor_get_object_path (sensor)),
				       NULL);
out:
	return ret;
}

/**
 * cd_main_add_sensor:
 **/
static void
cd_main_add_sensor (CdSensor *sensor)
{
	const gchar *id;
	gboolean ret;
	GError *error = NULL;

	id = cd_sensor_get_id (sensor);
	if (id == NULL) {
		g_warning ("did not get an ID from the sensor");
		goto out;
	}
	g_debug ("CdMain: add sensor: %s", id);
	g_ptr_array_add (sensors, g_object_ref (sensor));

	/* register on bus */
	ret = cd_main_sensor_register_on_bus (sensor, &error);
	if (!ret) {
		g_ptr_array_remove (sensors, sensor);
		g_warning ("CdMain: failed to emit SensorAdded: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
out:
	return;
}

/**
 * cd_main_setup_standard_space:
 **/
static void
cd_main_setup_standard_space (const gchar *space,
			      const gchar *search)
{
	CdProfile *profile = NULL;

	/* depending on the prefix, find the profile */
	if (g_str_has_prefix (search, "icc_")) {
		profile = cd_profile_array_get_by_id (profiles_array,
						      search);
	} else if (g_str_has_prefix (search, "/")) {
		profile = cd_profile_array_get_by_filename (profiles_array,
							    search);
	} else  {
		g_warning ("unknown prefix for override search: %s",
			   search);
		goto out;
	}
	if (profile == NULL) {
		g_warning ("failed to find profile %s for override",
			   search);
		goto out;
	}

	/* add override */
	g_debug ("CdMain: adding profile override %s=%s",
		 space, search);
	g_hash_table_insert (standard_spaces,
			     g_strdup (space),
			     g_object_ref (profile));
out:
	if (profile != NULL)
		g_object_unref (profile);
}

/**
 * cd_main_setup_standard_spaces:
 **/
static void
cd_main_setup_standard_spaces (void)
{
	gchar **spaces;
	gchar **split;
	guint i;

	/* get overrides */
	spaces = cd_config_get_strv (config, "StandardSpaces");
	if (spaces == NULL) {
		g_debug ("no standard space overrides");
		goto out;
	}

	/* parse them */
	for (i=0; spaces[i] != NULL; i++) {
		split = g_strsplit (spaces[i], ":", 2);
		if (g_strv_length (split) == 2) {
			cd_main_setup_standard_space (split[0],
						      split[1]);
		} else {
			g_warning ("invalid spaces override '%s', "
				   "expected name:value",
				   spaces[i]);
		}
		g_strfreev (split);
	}
out:
	g_strfreev (spaces);
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
	gboolean ret;
	GError *error = NULL;
	GPtrArray *array_devices = NULL;
	guint i;
	CdProfileSearchFlags flags;
	CdSensor *sensor = NULL;

	g_debug ("CdMain: acquired name: %s", name);
	connection = g_object_ref (connection_);

	/* add system profiles */
	profile_store = cd_profile_store_new ();
	g_signal_connect (profile_store, "added",
			  G_CALLBACK (cd_main_profile_store_added_cb),
			  user_data);
	g_signal_connect (profile_store, "removed",
			  G_CALLBACK (cd_main_profile_store_removed_cb),
			  user_data);

	/* search locations specified in the config file */
	flags = CD_PROFILE_STORE_SEARCH_SYSTEM |
		CD_PROFILE_STORE_SEARCH_MACHINE;
	ret = cd_config_get_boolean (config, "SearchVolumes");
	if (ret)
		flags |= CD_PROFILE_STORE_SEARCH_VOLUMES;
	cd_profile_store_search (profile_store, flags);

	/* add disk devices */
	array_devices = cd_device_db_get_devices (device_db, &error);
	if (array_devices == NULL) {
		g_warning ("CdMain: failed to get the disk devices: %s",
			    error->message);
		g_error_free (error);
		goto out;
	}
	for (i=0; i < array_devices->len; i++) {
		device_id = g_ptr_array_index (array_devices, i);
		cd_main_add_disk_device (device_id);
	}

#ifdef HAVE_GUDEV
	/* add GUdev devices */
	cd_udev_client_coldplug (udev_client);

	/* add sensor devices */
	cd_sensor_client_coldplug (sensor_client);
#endif

	/* add dummy sensor */
	ret = cd_config_get_boolean (config, "CreateDummySensor");
	if (ret) {
		sensor = cd_sensor_new ();
		cd_sensor_set_kind (sensor, CD_SENSOR_KIND_DUMMY);
		ret = cd_sensor_load (sensor, &error);
		if (!ret) {
			g_warning ("CdMain: failed to load dummy sensor: %s",
				    error->message);
			g_clear_error (&error);
		} else {
			cd_main_add_sensor (sensor);
		}
	}

	/* add SANE devices */
	ret = cd_config_get_boolean (config, "UseSANE");
	if (ret) {
		ret = cd_sane_client_refresh (sane_client, &error);
		if (!ret) {
			g_warning ("CdMain: failed to refresh SANE devices: %s",
				    error->message);
			g_error_free (error);
		}
	}

	/* now we've got the profiles, setup the overrides */
	cd_main_setup_standard_spaces ();
out:
	if (array_devices != NULL)
		g_ptr_array_unref (array_devices);
	if (sensor != NULL)
		g_object_unref (sensor);
}

/**
 * cd_main_on_name_lost_cb:
 **/
static void
cd_main_on_name_lost_cb (GDBusConnection *connection_,
			 const gchar *name,
			 gpointer user_data)
{
	g_debug ("CdMain: lost name: %s", name);
	g_main_loop_quit (loop);
}

/**
 * cd_main_client_device_added_cb:
 **/
static void
cd_main_client_device_added_cb (GObject *source,
				CdDevice *device,
				gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	cd_device_set_mode (device, CD_DEVICE_MODE_PHYSICAL);
	ret = cd_main_device_add (device, NULL, &error);
	if (!ret) {
		g_warning ("CdMain: failed to add device: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* register on bus */
	ret = cd_main_device_register_on_bus (device, &error);
	if (!ret) {
		g_warning ("CdMain: failed to emit DeviceAdded: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
out:
	return;
}

/**
 * cd_main_client_device_removed_cb:
 **/
static void
cd_main_client_device_removed_cb (GObject *source,
				  CdDevice *device,
				  gpointer user_data)
{
	g_debug ("CdMain: remove device: %s",
		 cd_device_get_id (device));
	cd_main_device_removed (device);
}

#ifdef HAVE_GUDEV

/**
 * cd_main_client_sensor_added_cb:
 **/
static void
cd_main_client_sensor_added_cb (CdSensorClient *sensor_client_,
				CdSensor *sensor,
				gpointer user_data)
{
	cd_main_add_sensor (sensor);
}

/**
 * cd_main_client_sensor_removed_cb:
 **/
static void
cd_main_client_sensor_removed_cb (CdSensorClient *sensor_client_,
				  CdSensor *sensor,
				  gpointer user_data)
{
	/* emit signal */
	g_debug ("CdMain: Emitting SensorRemoved(%s)",
		 cd_sensor_get_object_path (sensor));
	g_dbus_connection_emit_signal (connection,
				       NULL,
				       COLORD_DBUS_PATH,
				       COLORD_DBUS_INTERFACE,
				       "SensorRemoved",
				       g_variant_new ("(o)",
						      cd_sensor_get_object_path (sensor)),
				       NULL);
	g_ptr_array_remove (sensors, sensor);
}
#endif

/**
 * cd_main_timed_exit_cb:
 **/
static gboolean
cd_main_timed_exit_cb (gpointer user_data)
{
	g_main_loop_quit (loop);
	return FALSE;
}

/**
 * cd_main_load_introspection:
 **/
static GDBusNodeInfo *
cd_main_load_introspection (const gchar *filename, GError **error)
{
	gboolean ret;
	gchar *data = NULL;
	GDBusNodeInfo *info = NULL;
	GFile *file;

	/* load file */
	file = g_file_new_for_path (filename);
	ret = g_file_load_contents (file, NULL, &data,
				    NULL, NULL, error);
	if (!ret)
		goto out;

	/* build introspection from XML */
	info = g_dbus_node_info_new_for_xml (data, error);
	if (info == NULL)
		goto out;
out:
	g_object_unref (file);
	g_free (data);
	return info;
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	gboolean immediate_exit = FALSE;
	gboolean ret;
	gboolean timed_exit = FALSE;
	guint owner_id = 0;
	guint retval = 1;
	const GOptionEntry options[] = {
		{ "timed-exit", '\0', 0, G_OPTION_ARG_NONE, &timed_exit,
		  /* TRANSLATORS: exit after we've started up, used for user profiling */
		  _("Exit after a small delay"), NULL },
		{ "immediate-exit", '\0', 0, G_OPTION_ARG_NONE, &immediate_exit,
		  /* TRANSLATORS: exit straight away, used for automatic profiling */
		  _("Exit after the engine has loaded"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();

	/* TRANSLATORS: program name */
	g_set_application_name (_("Color Management"));
	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, cd_debug_get_option_group ());
	g_option_context_set_summary (context, _("Color Management D-Bus Service"));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* get from config */
	config = cd_config_new ();

	/* create new objects */
	loop = g_main_loop_new (NULL, FALSE);
	devices_array = cd_device_array_new ();
	profiles_array = cd_profile_array_new ();
	sensors = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	standard_spaces = g_hash_table_new_full (g_str_hash,
						 g_str_equal,
						 g_free,
						 (GDestroyNotify) g_object_unref);

	sane_client = cd_sane_client_new ();
	g_signal_connect (sane_client, "added",
			  G_CALLBACK (cd_main_client_device_added_cb),
			  NULL);
	g_signal_connect (sane_client, "removed",
			  G_CALLBACK (cd_main_client_device_removed_cb),
			  NULL);
#ifdef HAVE_GUDEV
	udev_client = cd_udev_client_new ();
	g_signal_connect (udev_client, "device-added",
			  G_CALLBACK (cd_main_client_device_added_cb),
			  NULL);
	g_signal_connect (udev_client, "device-removed",
			  G_CALLBACK (cd_main_client_device_removed_cb),
			  NULL);
	sensor_client = cd_sensor_client_new ();
	g_signal_connect (sensor_client, "sensor-added",
			  G_CALLBACK (cd_main_client_sensor_added_cb),
			  NULL);
	g_signal_connect (sensor_client, "sensor-removed",
			  G_CALLBACK (cd_main_client_sensor_removed_cb),
			  NULL);
#endif

	/* connect to the mapping db */
	mapping_db = cd_mapping_db_new ();
	ret = cd_mapping_db_load (mapping_db,
				  LOCALSTATEDIR "/lib/colord/mapping.db",
				  &error);
	if (!ret) {
		g_warning ("CdMain: failed to load mapping database: %s",
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
		g_warning ("CdMain: failed to load device database: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* load introspection from file */
	introspection_daemon = cd_main_load_introspection (DATADIR "/dbus-1/interfaces/"
							   COLORD_DBUS_INTERFACE ".xml",
							   &error);
	if (introspection_daemon == NULL) {
		g_warning ("CdMain: failed to load daemon introspection: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	introspection_device = cd_main_load_introspection (DATADIR "/dbus-1/interfaces/"
							   COLORD_DBUS_INTERFACE_DEVICE ".xml",
							   &error);
	if (introspection_device == NULL) {
		g_warning ("CdMain: failed to load device introspection: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	introspection_profile = cd_main_load_introspection (DATADIR "/dbus-1/interfaces/"
							   COLORD_DBUS_INTERFACE_PROFILE ".xml",
							   &error);
	if (introspection_profile == NULL) {
		g_warning ("CdMain: failed to load profile introspection: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	introspection_sensor = cd_main_load_introspection (DATADIR "/dbus-1/interfaces/"
							   COLORD_DBUS_INTERFACE_SENSOR ".xml",
							   &error);
	if (introspection_sensor == NULL) {
		g_warning ("CdMain: failed to load sensor introspection: %s",
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

	/* Only timeout and close the mainloop if we have specified it
	 * on the command line */
	if (immediate_exit)
		g_idle_add (cd_main_timed_exit_cb, loop);
	else if (timed_exit)
		g_timeout_add_seconds (5, cd_main_timed_exit_cb, loop);

	/* wait */
	g_main_loop_run (loop);

	/* success */
	retval = 0;
out:
	g_ptr_array_unref (sensors);
	g_hash_table_destroy (standard_spaces);
#ifdef HAVE_GUDEV
	if (udev_client != NULL)
		g_object_unref (udev_client);
	if (sensor_client != NULL)
		g_object_unref (sensor_client);
#endif
	if (config != NULL)
		g_object_unref (config);
	if (sane_client != NULL)
		g_object_unref (sane_client);
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
	if (introspection_sensor != NULL)
		g_dbus_node_info_unref (introspection_sensor);
	g_main_loop_unref (loop);
	return retval;
}

