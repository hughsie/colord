/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2012 Richard Hughes <richard@hughsie.com>
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

#include <stdlib.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <syslog.h>

#ifdef HAVE_LIBSYSTEMD_LOGIN
#include <systemd/sd-login.h>
#endif

#include "cd-common.h"
#include "cd-config.h"
#include "cd-debug.h"
#include "cd-device-array.h"
#include "cd-device-db.h"
#include "cd-device.h"
#include "cd-mapping-db.h"
#include "cd-plugin.h"
#include "cd-profile-array.h"
#include "cd-profile.h"
#include "cd-profile-store.h"
#include "cd-resources.h"
#include "cd-sensor-client.h"

typedef struct {
	GDBusConnection		*connection;
	GDBusNodeInfo		*introspection_daemon;
	GDBusNodeInfo		*introspection_device;
	GDBusNodeInfo		*introspection_profile;
	GDBusNodeInfo		*introspection_sensor;
	CdDeviceArray		*devices_array;
	CdProfileArray		*profiles_array;
	CdProfileStore		*profile_store;
	CdMappingDb		*mapping_db;
	CdDeviceDb		*device_db;
#ifdef HAVE_GUDEV
	CdSensorClient		*sensor_client;
#endif
	CdConfig		*config;
	GPtrArray		*sensors;
	GHashTable		*standard_spaces;
	GPtrArray		*plugins;
	GMainLoop		*loop;
} CdMainPrivate;

/**
 * cd_main_profile_removed:
 **/
static void
cd_main_profile_removed (CdMainPrivate *priv, CdProfile *profile)
{
	gboolean ret;
	gchar *object_path_tmp;
	CdDevice *device_tmp;
	GPtrArray *devices;
	guint i;

	/* remove from the array before emitting */
	object_path_tmp = g_strdup (cd_profile_get_object_path (profile));
	cd_profile_array_remove (priv->profiles_array, profile);

	/* try to remove this profile from all devices */
	devices = cd_device_array_get_array (priv->devices_array);
	for (i = 0; i < devices->len; i++) {
		device_tmp = g_ptr_array_index (devices, i);
		ret = cd_device_remove_profile (device_tmp,
						object_path_tmp,
						NULL);
		if (ret) {
			syslog (LOG_INFO, "Automatic remove of %s from %s",
				cd_profile_get_id (profile),
				cd_device_get_id (device_tmp));
			g_debug ("CdMain: automatically removing %s from %s as removed",
				 object_path_tmp,
				 cd_device_get_object_path (device_tmp));
		}
	}

	/* emit signal */
	g_debug ("CdMain: Emitting ProfileRemoved(%s)", object_path_tmp);
	syslog (LOG_INFO, "Profile removed: %s",
		cd_profile_get_id (profile));
	g_dbus_connection_emit_signal (priv->connection,
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
	CdMainPrivate *priv = (CdMainPrivate *) user_data;

	g_debug ("CdMain: profile '%s' invalidated",
		 cd_profile_get_id (profile));
	cd_main_profile_removed (priv, profile);
}

/**
 * cd_main_device_removed:
 **/
static void
cd_main_device_removed (CdMainPrivate *priv, CdDevice *device)
{
	gboolean ret;
	gchar *object_path_tmp;
	GError *error = NULL;

	/* remove from the array before emitting */
	object_path_tmp = g_strdup (cd_device_get_object_path (device));
	g_debug ("CdMain: Removing device %s", object_path_tmp);
	cd_device_array_remove (priv->devices_array, device);

	/* remove from the device database */
	if (cd_device_get_scope (device) == CD_OBJECT_SCOPE_DISK) {
		ret = cd_device_db_remove (priv->device_db,
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
	syslog (LOG_INFO, "device removed: %s",
		cd_device_get_id (device));
	g_dbus_connection_emit_signal (priv->connection,
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
	CdMainPrivate *priv = (CdMainPrivate *) user_data;

	g_debug ("CdMain: device '%s' invalidated",
		 cd_device_get_id (device));
	cd_main_device_removed (priv, device);
}

/**
 * cd_main_add_profile:
 **/
static gboolean
cd_main_add_profile (CdMainPrivate *priv,
		     CdProfile *profile,
		     GError **error)
{
	gboolean ret = TRUE;

	/* add */
	cd_profile_array_add (priv->profiles_array, profile);
	g_debug ("CdMain: Adding profile %s", cd_profile_get_object_path (profile));

	/* profile is no longer valid */
	g_signal_connect (profile, "invalidate",
			  G_CALLBACK (cd_main_profile_invalidate_cb),
			  priv);

	return ret;
}

/**
 * cd_main_create_profile:
 **/
static CdProfile *
cd_main_create_profile (CdMainPrivate *priv,
			const gchar *sender,
			const gchar *profile_id,
			guint owner,
			CdObjectScope scope,
			GError **error)
{
	gboolean ret;
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;

	g_assert (priv->connection != NULL);

	/* create an object */
	profile_tmp = cd_profile_new ();
	cd_profile_set_owner (profile_tmp, owner);
	cd_profile_set_id (profile_tmp, profile_id);
	cd_profile_set_scope (profile_tmp, scope);

	/* add the profile */
	ret = cd_main_add_profile (priv, profile_tmp, error);
	if (!ret)
		goto out;

	/* different persistent scope */
	switch (scope) {
	case CD_OBJECT_SCOPE_NORMAL:
		g_debug ("CdMain: normal profile");
		break;
	case CD_OBJECT_SCOPE_TEMP:
		g_debug ("CdMain: temporary profile");
		/* setup DBus watcher */
		cd_profile_watch_sender (profile_tmp, sender);
		break;
	case CD_OBJECT_SCOPE_DISK:
		g_debug ("CdMain: persistent profile");
		g_set_error_literal (error,
				     CD_CLIENT_ERROR,
				     CD_CLIENT_ERROR_NOT_SUPPORTED,
				     "persistent profiles are no yet supported");
		goto out;
	default:
		g_warning ("CdMain: Unsupported scope kind: %i", scope);
		goto out;
	}

	/* success */
	profile = g_object_ref (profile_tmp);
out:
	g_object_unref (profile_tmp);
	return profile;
}

/**
 * cd_main_auto_add_from_md:
 **/
static gboolean
cd_main_auto_add_from_md (CdMainPrivate *priv,
			  CdDevice *device,
			  CdProfile *profile)
{
	const gchar *device_id;
	const gchar *profile_id;
	const gchar *tmp;
	const gchar **warnings;
	gboolean ret = FALSE;
	GError *error = NULL;
	guint64 timestamp;

	/* check device and profile hasn't been manually removed */
	profile_id = cd_profile_get_id (profile);
	device_id = cd_device_get_id (device);
	timestamp = cd_mapping_db_get_timestamp (priv->mapping_db,
						 device_id,
						 profile_id,
						 &error);
	if (timestamp == G_MAXUINT64) {
		g_debug ("CdMain: no existing mapping found: %s",
			 error->message);
		g_clear_error (&error);
	} else if (timestamp == 0) {
		g_debug ("CdMain: Not doing MD add %s to %s due to removal",
			 profile_id, device_id);
		goto out;
	}

	/* if the auto-EDID profile has warnings then do not add this */
	tmp = cd_profile_get_metadata_item (profile, CD_PROFILE_METADATA_DATA_SOURCE);
	if (g_strcmp0 (tmp, CD_PROFILE_METADATA_DATA_SOURCE_EDID) == 0) {
		warnings = cd_profile_get_warnings (profile);
		if (g_strv_length ((gchar **) warnings) > 0) {
			g_debug ("CdMain: NOT MD add %s to %s as profile has warnings",
				 profile_id, device_id);
			goto out;
		}
	}

	/* auto-add soft relationship */
	g_debug ("CdMain: Automatically MD add %s to %s",
		 profile_id, device_id);
	syslog (LOG_INFO, "Automatic metadata add %s to %s",
		profile_id, device_id);
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
 * cd_main_auto_add_from_db:
 **/
static gboolean
cd_main_auto_add_from_db (CdMainPrivate *priv,
			  CdDevice *device,
			  CdProfile *profile)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	guint64 timestamp;

	g_debug ("CdMain: Automatically DB add %s to %s",
		 cd_profile_get_id (profile),
		 cd_device_get_object_path (device));
	syslog (LOG_INFO, "Automatic database add %s to %s",
		cd_profile_get_id (profile),
		cd_device_get_id (device));
	timestamp = cd_mapping_db_get_timestamp (priv->mapping_db,
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
 * cd_main_device_auto_add_from_md:
 **/
static void
cd_main_device_auto_add_from_md (CdMainPrivate *priv,
				 CdDevice *device)
{
	CdProfile *profile_tmp;
	GPtrArray *array;
	guint i;

	/* get all the profiles, and check to see if any of them contain
	 * MAPPING_device_id that matches the device */
	array = cd_profile_array_get_by_metadata (priv->profiles_array,
						  CD_PROFILE_METADATA_MAPPING_DEVICE_ID,
						  cd_device_get_id (device));
	if (array == NULL)
		goto out;
	for (i = 0; i < array->len; i++) {
		profile_tmp = g_ptr_array_index (array, i);
		cd_main_auto_add_from_md (priv, device, profile_tmp);
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
}

/**
 * cd_main_device_auto_add_from_db:
 **/
static void
cd_main_device_auto_add_from_db (CdMainPrivate *priv, CdDevice *device)
{
	CdProfile *profile_tmp;
	const gchar *object_id_tmp;
	GError *error = NULL;
	GPtrArray *array;
	guint64 timestamp;
	guint i;

	/* get data */
	array = cd_mapping_db_get_profiles (priv->mapping_db,
					    cd_device_get_id (device),
					    &error);
	if (array == NULL) {
		g_warning ("CdMain: failed to get profiles for device from db: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* try to add them */
	for (i = 0; i < array->len; i++) {
		object_id_tmp = g_ptr_array_index (array, i);

		/* ensure timestamp is still valid */
		timestamp = cd_mapping_db_get_timestamp (priv->mapping_db,
							 cd_device_get_id (device),
							 object_id_tmp,
							 &error);
		if (timestamp == G_MAXUINT64) {
			g_warning ("CdMain: failed to get timestamp: %s",
				   error->message);
			g_clear_error (&error);
			continue;
		}
		if (timestamp == 0) {
			g_debug ("CdMain: timestamp zero for %s and %s",
				 cd_device_get_id (device),
				 object_id_tmp);
			continue;
		}

		profile_tmp = cd_profile_array_get_by_id_owner (priv->profiles_array,
								object_id_tmp,
								cd_device_get_owner (device));
		if (profile_tmp == NULL) {
			g_debug ("CdMain: profile %s with owner %i is not (yet) available",
				 object_id_tmp, cd_device_get_owner (device));
			continue;
		}

		/* does the profile have the correct device metadata */
		cd_main_auto_add_from_db (priv, device, profile_tmp);
		g_object_unref (profile_tmp);
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
}

/**
 * cd_main_device_register_on_bus:
 **/
static gboolean
cd_main_device_register_on_bus (CdMainPrivate *priv,
				CdDevice *device,
				GError **error)
{
	gboolean ret;

	/* register object */
	ret = cd_device_register_object (device,
					 priv->connection,
					 priv->introspection_device->interfaces[0],
					 error);
	if (!ret)
		goto out;

	/* emit signal */
	g_debug ("CdMain: Emitting DeviceAdded(%s)",
		 cd_device_get_object_path (device));
	syslog (LOG_INFO, "Device added: %s",
		cd_device_get_id (device));
	g_dbus_connection_emit_signal (priv->connection,
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
cd_main_device_add (CdMainPrivate *priv,
		    CdDevice *device,
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
		ret = cd_device_db_add (priv->device_db,
					cd_device_get_id (device),
					&error_local);
		if (!ret)
			goto out;
	}

	/* profile is no longer valid */
	if (scope == CD_OBJECT_SCOPE_TEMP) {
		g_signal_connect (device, "invalidate",
				  G_CALLBACK (cd_main_device_invalidate_cb),
				  priv);
	}

	/* add to array */
	cd_device_array_add (priv->devices_array, device);

	/* auto add profiles from the database and metadata */
	cd_main_device_auto_add_from_db (priv, device);
	cd_main_device_auto_add_from_md (priv, device);
out:
	return ret;
}

/**
 * cd_main_get_seat_for_process:
 */
static gchar *
cd_main_get_seat_for_process (guint pid)
{
	gchar *seat = NULL;
#ifdef HAVE_LIBSYSTEMD_LOGIN
	char *sd_seat = NULL;
	char *sd_session = NULL;
	gint rc;

	/* get session the process belongs to */
	rc = sd_pid_get_session (pid, &sd_session);
	if (rc != 0) {
		g_warning ("failed to get session [pid %i]: %s",
			   pid, strerror (rc));
		goto out;
	}

	/* get the seat the session is on */
	rc = sd_session_get_seat (sd_session, &sd_seat);
	if (rc != 0) {
		g_warning ("failed to get seat for session %s [pid %i]: %s",
			   sd_session, pid, strerror (rc));
		goto out;
	}
	seat = g_strdup (sd_seat);
out:
	free (sd_seat);
	free (sd_session);
#endif
	return seat;
}

/**
 * cd_main_create_device:
 **/
static CdDevice *
cd_main_create_device (CdMainPrivate *priv,
		       const gchar *sender,
		       const gchar *device_id,
		       guint owner,
		       guint process,
		       CdObjectScope scope,
		       CdDeviceMode mode,
		       GError **error)
{
	gboolean ret;
	gchar *seat;
	CdDevice *device_tmp;
	CdDevice *device = NULL;

	g_assert (priv->connection != NULL);

	/* get the seat of the process that is creating the device */
	seat = cd_main_get_seat_for_process (process);

	/* create an object */
	device_tmp = cd_device_new ();
	cd_device_set_owner (device_tmp, owner);
	cd_device_set_id (device_tmp, device_id);
	cd_device_set_scope (device_tmp, scope);
	cd_device_set_mode (device_tmp, mode);
	cd_device_set_seat (device_tmp, seat);
	ret = cd_main_device_add (priv, device_tmp, sender, error);
	if (!ret)
		goto out;

	/* setup DBus watcher */
	if (sender != NULL && scope == CD_OBJECT_SCOPE_TEMP) {
		g_debug ("temporary device");
		cd_device_watch_sender (device_tmp, sender);
	}

	/* success */
	device = g_object_ref (device_tmp);
out:
	g_free (seat);
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
	for (i = 0; i < array->len; i++) {
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
	for (i = 0; i < array->len; i++) {
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
	for (i = 0; i < array->len; i++) {
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
 * cd_main_profile_auto_add_from_db:
 **/
static void
cd_main_profile_auto_add_from_db (CdMainPrivate *priv,
				  CdProfile *profile)
{
	CdDevice *device_tmp;
	const gchar *device_id_tmp;
	GError *error = NULL;
	GPtrArray *array;
	guint i;

	/* get data */
	array = cd_mapping_db_get_devices (priv->mapping_db,
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
	for (i = 0; i < array->len; i++) {
		device_id_tmp = g_ptr_array_index (array, i);
		device_tmp = cd_device_array_get_by_id_owner (priv->devices_array,
							      device_id_tmp,
							      cd_profile_get_owner (profile));
		if (device_tmp == NULL)
			continue;

		/* hard add */
		cd_main_auto_add_from_db (priv, device_tmp, profile);
		g_object_unref (device_tmp);
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
}

/**
 * cd_main_profile_auto_add_from_md:
 **/
static void
cd_main_profile_auto_add_from_md (CdMainPrivate *priv,
				  CdProfile *profile)
{
	CdDevice *device = NULL;
	const gchar *device_id;

	/* does the device exists that matches the md */
	device_id = cd_profile_get_metadata_item (profile,
						  CD_PROFILE_METADATA_MAPPING_DEVICE_ID);
	if (device_id == NULL)
		goto out;
	device = cd_device_array_get_by_id_owner (priv->devices_array,
						  device_id,
						  cd_profile_get_owner (profile));
	if (device == NULL)
		goto out;
	cd_main_auto_add_from_md (priv, device, profile);
out:
	if (device != NULL)
		g_object_unref (device);
}

/**
 * cd_main_profile_register_on_bus:
 **/
static gboolean
cd_main_profile_register_on_bus (CdMainPrivate *priv,
				 CdProfile *profile,
				 GError **error)
{
	gboolean ret;

	/* register object */
	ret = cd_profile_register_object (profile,
					  priv->connection,
					  priv->introspection_profile->interfaces[0],
					  error);
	if (!ret)
		goto out;

	/* emit signal */
	g_debug ("CdMain: Emitting ProfileAdded(%s)",
		 cd_profile_get_object_path (profile));
	syslog (LOG_INFO, "Profile added: %s",
		cd_profile_get_id (profile));
	g_dbus_connection_emit_signal (priv->connection,
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
cd_main_get_standard_space_override (CdMainPrivate *priv,
				     const gchar *standard_space)
{
	CdProfile *profile;
	profile = g_hash_table_lookup (priv->standard_spaces,
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
cd_main_get_standard_space_metadata (CdMainPrivate *priv,
				     const gchar *standard_space)
{
	CdProfile *profile_best = NULL;
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;
	GPtrArray *array;
	guint i;
	guint score_best = 0;
	guint score_tmp;

	/* get all the profiles with this metadata */
	array = cd_profile_array_get_by_metadata (priv->profiles_array,
						  CD_PROFILE_METADATA_STANDARD_SPACE,
						  standard_space);

	/* use the profile with the largest score */
	for (i = 0; i < array->len; i++) {
		profile_tmp = g_ptr_array_index (array, i);
		score_tmp = cd_profile_get_score (profile_tmp);
		if (score_tmp > score_best) {
			score_best = score_tmp;
			profile_best = profile_tmp;
		}
	}
	if (profile_best == NULL)
		goto out;

	/* success */
	profile = g_object_ref (profile_best);
out:
	g_ptr_array_unref (array);
	return profile;
}

/**
 * cd_main_get_cmdline_for_pid:
 **/
static gchar *
cd_main_get_cmdline_for_pid (guint pid)
{
	gboolean ret;
	gchar *cmdline = NULL;
	gchar *proc_path;
	GError *error = NULL;
	gsize len = 0;
	guint i;

	/* just read the link */
	proc_path = g_strdup_printf ("/proc/%i/cmdline", pid);
	ret = g_file_get_contents (proc_path, &cmdline, &len, &error);
	if (!ret) {
		g_warning ("CdMain: failed to read %s: %s",
			   proc_path, error->message);
		g_error_free (error);
		goto out;
	}
	if (len == 0) {
		g_warning ("CdMain: failed to read %s", proc_path);
		goto out;
	}

	/* turn all the \0's into spaces */
	for (i = 0; i < len; i++) {
		if (cmdline[i] == '\0')
			cmdline[i] = ' ';
	}
out:
	g_free (proc_path);
	return cmdline;
}

/**
 * cd_main_daemon_method_call:
 **/
static void
cd_main_daemon_method_call (GDBusConnection *connection, const gchar *sender,
			    const gchar *object_path, const gchar *interface_name,
			    const gchar *method_name, GVariant *parameters,
			    GDBusMethodInvocation *invocation, gpointer user_data)
{
	CdDevice *device = NULL;
	CdDeviceKind device_kind;
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	CdObjectScope scope;
	CdProfile *profile = NULL;
	CdSensor *sensor_tmp;
	const gchar *prop_key;
	const gchar *prop_value;
	gboolean register_on_bus = TRUE;
	gboolean ret;
	const gchar *device_id = NULL;
	const gchar *scope_tmp = NULL;
	gchar *cmdline = NULL;
	GError *error = NULL;
	GPtrArray *array = NULL;
	GVariantIter *iter = NULL;
	GVariant *dict = NULL;
	GVariant *tuple = NULL;
	GVariant *value = NULL;
	gint fd = -1;
	guint i;
	guint pid;
	guint uid;
	gint32 fd_handle = 0;
	const gchar *metadata_key = NULL;
	const gchar *metadata_value = NULL;
	GDBusMessage *message;
	GUnixFDList *fd_list;

	/* get the owner of the message */
	uid = cd_main_get_sender_uid (connection, sender, &error);
	if (uid == G_MAXUINT) {
		g_dbus_method_invocation_return_error (invocation,
						       CD_CLIENT_ERROR,
						       CD_CLIENT_ERROR_INTERNAL,
						       "failed to get owner: %s",
						       error->message);
		g_error_free (error);
		goto out;
	}

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetDevices") == 0) {

		g_debug ("CdMain: %s:GetDevices()", sender);

		/* format the value */
		array = cd_device_array_get_array (priv->devices_array);
		value = cd_main_device_array_to_variant (array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetSensors") == 0) {

		g_debug ("CdMain: %s:GetSensors()", sender);

		/* format the value */
		value = cd_main_sensor_array_to_variant (priv->sensors);
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
		device_kind = cd_device_kind_from_string (device_id);
		if (device_kind == CD_DEVICE_KIND_UNKNOWN) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_INPUT_INVALID,
							       "device kind %s not recognised",
							       device_id);
			goto out;
		}
		array = cd_device_array_get_by_kind (priv->devices_array,
						     device_kind);

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
		array = cd_profile_array_get_by_kind (priv->profiles_array,
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
		device = cd_device_array_get_by_id_owner (priv->devices_array,
							  device_id,
							  uid);
		if (device == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_NOT_FOUND,
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
		device = cd_device_array_get_by_property (priv->devices_array,
							  metadata_key,
							  metadata_value);
		if (device == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_NOT_FOUND,
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
	if (g_strcmp0 (method_name, "FindSensorById") == 0) {

		g_variant_get (parameters, "(&s)", &device_id);
		g_debug ("CdMain: %s:FindSensorById(%s)",
			 sender, device_id);

		/* find sensor */
		for (i = 0; i < priv->sensors->len; i++) {
			sensor_tmp = g_ptr_array_index (priv->sensors, i);
			if (g_strcmp0 (cd_sensor_get_id (sensor_tmp), device_id) == 0) {
				value = g_variant_new ("(o)", cd_sensor_get_object_path (sensor_tmp));
				g_dbus_method_invocation_return_value (invocation, value);
				goto out;
			}
		}
		g_dbus_method_invocation_return_error (invocation,
						       CD_CLIENT_ERROR,
						       CD_CLIENT_ERROR_NOT_FOUND,
						       "sensor id '%s' does not exist",
						       device_id);
		goto out;
	}

	/* return 'o' */
	if (g_strcmp0 (method_name, "FindProfileByProperty") == 0) {

		g_variant_get (parameters, "(&s&s)",
			       &metadata_key,
			       &metadata_value);
		g_debug ("CdMain: %s:FindProfileByProperty(%s=%s)",
			 sender, metadata_key, metadata_value);
		profile = cd_profile_array_get_by_property (priv->profiles_array,
							    metadata_key,
							    metadata_value);
		if (profile == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_NOT_FOUND,
							       "property match '%s'='%s' does not exist",
							       metadata_key,
							       metadata_value);
			goto out;
		}

		/* format the value */
		value = g_variant_new ("(o)", cd_profile_get_object_path (profile));
		g_dbus_method_invocation_return_value (invocation, value);
		goto out;
	}

	/* return 'o' */
	if (g_strcmp0 (method_name, "FindProfileById") == 0) {

		g_variant_get (parameters, "(&s)", &device_id);
		g_debug ("CdMain: %s:FindProfileById(%s)",
			 sender, device_id);
		profile = cd_profile_array_get_by_id_owner (priv->profiles_array,
							    device_id,
							    uid);
		if (profile == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_NOT_FOUND,
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
		profile = cd_main_get_standard_space_override (priv, device_id);
		if (profile == NULL)
			profile = cd_main_get_standard_space_metadata (priv, device_id);
		if (profile == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_NOT_FOUND,
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
		profile = cd_profile_array_get_by_filename (priv->profiles_array,
							    device_id);
		if (profile == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_NOT_FOUND,
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
		value = cd_profile_array_get_variant (priv->profiles_array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "CreateDevice") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (connection,
						    sender,
						    "org.freedesktop.color-manager.create-device",
						    &error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			g_error_free (error);
			goto out;
		}

		/* does already exist */
		g_variant_get (parameters, "(&s&s@a{ss})",
			       &device_id,
			       &scope_tmp,
			       &dict);
		g_debug ("CdMain: %s:CreateDevice(%s)", sender, device_id);

		/* check ID is valid */
		if (g_strcmp0 (device_id, "") == 0) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_INPUT_INVALID,
							       "device id cannot be blank");
			goto out;
		}

		/* check kind is supplied and recognised */
		ret = g_variant_lookup (dict,
					CD_DEVICE_PROPERTY_KIND,
					"&s", &prop_value);
		if (!ret) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_INPUT_INVALID,
							       "required device type not specified");
			goto out;
		}
		device_kind = cd_device_kind_from_string (prop_value);
		if (device_kind == CD_DEVICE_KIND_UNKNOWN) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_INPUT_INVALID,
							       "device type %s not recognised",
							       prop_value);
			goto out;
		}

		/* check it does not already exist */
		scope = cd_object_scope_from_string (scope_tmp);
		if (scope == CD_OBJECT_SCOPE_UNKNOWN) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_INPUT_INVALID,
							       "scope non-valid: %s",
							       scope_tmp);
			goto out;
		}
		device = cd_device_array_get_by_id_owner (priv->devices_array,
							  device_id,
							  uid);
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
								       CD_CLIENT_ERROR,
								       CD_CLIENT_ERROR_ALREADY_EXISTS,
								       "device id '%s' already exists",
								       device_id);
				goto out;
			}
		}

		/* get the process that sent the message */
		pid = cd_main_get_sender_pid (connection, sender, &error);
		if (pid == G_MAXUINT) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_INTERNAL,
							       "failed to get process ID: %s",
							       error->message);
			g_error_free (error);
			goto out;
		}

		/* create device */
		device = cd_main_create_device (priv,
						sender,
						device_id,
						uid,
						pid,
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

		/* set the properties */
		cd_device_set_kind (device, device_kind);
		iter = g_variant_iter_new (dict);
		while (g_variant_iter_next (iter, "{&s&s}",
					    &prop_key, &prop_value)) {
			if (g_strcmp0 (prop_key, CD_DEVICE_PROPERTY_KIND) == 0)
				continue;
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

		/* add any extra metadata */
		cmdline = cd_main_get_cmdline_for_pid (pid);
		if (cmdline != NULL) {
			ret = cd_device_set_property_internal (device,
							       CD_DEVICE_METADATA_OWNER_CMDLINE,
							       cmdline,
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
			ret = cd_main_device_register_on_bus (priv, device, &error);
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
		ret = cd_main_sender_authenticated (connection,
						    sender,
						    "org.freedesktop.color-manager.delete-device",
						    &error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			g_error_free (error);
			goto out;
		}

		/* does already exist */
		g_variant_get (parameters, "(&o)", &device_id);
		g_debug ("CdMain: %s:DeleteDevice(%s)",
			 sender, device_id);
		device = cd_device_array_get_by_id_owner (priv->devices_array,
							  device_id,
							  uid);
		if (device == NULL) {
			/* fall back to checking the object path */
			device = cd_device_array_get_by_object_path (priv->devices_array,
								     device_id);
			if (device == NULL) {
				g_dbus_method_invocation_return_error (invocation,
								       CD_CLIENT_ERROR,
								       CD_CLIENT_ERROR_NOT_FOUND,
								       "device path '%s' not found",
								       device_id);
				goto out;
			}
		}

		/* remove from the array, and emit */
		cd_main_device_removed (priv, device);

		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "DeleteProfile") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (connection,
						    sender,
						    "org.freedesktop.color-manager.create-profile",
						    &error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			g_error_free (error);
			goto out;
		}

		/* does already exist */
		g_variant_get (parameters, "(&o)", &device_id);
		g_debug ("CdMain: %s:DeleteProfile(%s)",
			 sender, device_id);
		profile = cd_profile_array_get_by_id_owner (priv->profiles_array,
							    device_id,
							    uid);
		if (profile == NULL) {
			/* fall back to checking the object path */
			profile = cd_profile_array_get_by_object_path (priv->profiles_array,
								       device_id);
			if (profile == NULL) {
				g_dbus_method_invocation_return_error (invocation,
								       CD_CLIENT_ERROR,
								       CD_CLIENT_ERROR_NOT_FOUND,
								       "profile path '%s' not found",
								       device_id);
				goto out;
			}
		}

		/* remove from the array, and emit */
		cd_main_profile_removed (priv, profile);

		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	/* return 'o' */
	if (g_strcmp0 (method_name, "CreateProfile") == 0 ||
	    g_strcmp0 (method_name, "CreateProfileWithFd") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (connection,
						    sender,
						    "org.freedesktop.color-manager.create-profile",
						    &error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			g_error_free (error);
			goto out;
		}

		if (g_strcmp0 (g_variant_get_type_string (parameters),
			       "(ssha{ss})") == 0) {
			g_variant_get (parameters, "(&s&sha{ss})",
				       &device_id,
				       &scope_tmp,
				       &fd_handle,
				       &iter);
			g_debug ("CdMain: %s:CreateProfileWithFd(%s,%i)",
				 g_dbus_method_invocation_get_sender (invocation),
				 device_id, fd_handle);
		} else {
			g_variant_get (parameters, "(&s&sa{ss})",
				       &device_id,
				       &scope_tmp,
				       &iter);
			g_debug ("CdMain: %s:CreateProfile(%s)", sender, device_id);
		}

		/* check ID is valid */
		if (g_strcmp0 (device_id, "") == 0) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_INPUT_INVALID,
							       "profile id cannot be blank");
			goto out;
		}

		/* check it does not already exist */
		profile = cd_profile_array_get_by_id_owner (priv->profiles_array,
							    device_id,
							    uid);
		if (profile != NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_ALREADY_EXISTS,
							       "profile id '%s' already exists",
							       device_id);
			goto out;
		}

		/* create profile */
		scope = cd_object_scope_from_string (scope_tmp);
		if (scope == CD_OBJECT_SCOPE_UNKNOWN) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_INPUT_INVALID,
							       "scope non-valid: %s",
							       scope_tmp);
			goto out;
		}
		profile = cd_main_create_profile (priv,
						  sender,
						  device_id,
						  uid,
						  scope,
						  &error);
		if (profile == NULL) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			goto out;
		}

		/* get any file descriptor in the message */
		message = g_dbus_method_invocation_get_message (invocation);
		fd_list = g_dbus_message_get_unix_fd_list (message);
		if (fd_list != NULL && g_unix_fd_list_get_length (fd_list) == 1) {
			fd = g_unix_fd_list_get (fd_list, fd_handle, &error);
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

		/* auto add profiles from the database and metadata */
		cd_main_profile_auto_add_from_db (priv, profile);
		cd_main_profile_auto_add_from_md (priv, profile);

		/* register on bus */
		ret = cd_main_profile_register_on_bus (priv, profile, &error);
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
	g_free (cmdline);
	if (iter != NULL)
		g_variant_iter_free (iter);
	if (dict != NULL)
		g_variant_unref (dict);
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
		goto out;
	}

	/* return an error */
	g_set_error (error,
		     CD_CLIENT_ERROR,
		     CD_CLIENT_ERROR_INTERNAL,
		     "failed to get daemon property %s",
		     property_name);
out:
	return retval;
}

/**
 * cd_main_on_bus_acquired_cb:
 **/
static void
cd_main_on_bus_acquired_cb (GDBusConnection *connection,
			    const gchar *name,
			    gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	guint registration_id;
	static const GDBusInterfaceVTable interface_vtable = {
		cd_main_daemon_method_call,
		cd_main_daemon_get_property,
		NULL
	};

	priv->connection = g_object_ref (connection);
	registration_id = g_dbus_connection_register_object (connection,
							     COLORD_DBUS_PATH,
							     priv->introspection_daemon->interfaces[0],
							     &interface_vtable,
							     priv,  /* user_data */
							     NULL,  /* user_data_free_func */
							     NULL); /* GError** */
	g_assert (registration_id > 0);
}

/**
 * cd_main_profile_store_added_cb:
 **/
static void
cd_main_profile_store_added_cb (CdProfileStore *profile_store,
				CdProfile *profile,
				gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	gboolean ret;
	gchar *profile_id;
	GError *error = NULL;

	/* just add it to the bus with the title as the ID */
	profile_id = g_strdup_printf ("icc-%s",
				      cd_profile_get_checksum (profile));
	cd_profile_set_id (profile, profile_id);
	ret = cd_main_add_profile (priv, profile, &error);
	if (!ret) {
		g_warning ("CdMain: failed to add profile: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* register on bus */
	ret = cd_main_profile_register_on_bus (priv, profile, &error);
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
cd_main_add_disk_device (CdMainPrivate *priv, const gchar *device_id)
{
	CdDevice *device;
	const gchar *property;
	gboolean ret;
	gchar *value;
	GError *error = NULL;
	GPtrArray *array_properties = NULL;
	guint i;

	device = cd_main_create_device (priv,
					NULL,
					device_id,
					0,
					0,
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
	array_properties = cd_device_db_get_properties (priv->device_db,
							device_id,
							&error);
	if (array_properties == NULL) {
		g_warning ("CdMain: failed to get props for device %s: %s",
			   device_id, error->message);
		g_error_free (error);
		goto out;
	}
	for (i = 0; i < array_properties->len; i++) {
		property = g_ptr_array_index (array_properties, i);
		value = cd_device_db_get_property (priv->device_db,
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
	ret = cd_main_device_register_on_bus (priv, device, &error);
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
cd_main_sensor_register_on_bus (CdMainPrivate *priv,
				CdSensor *sensor,
				GError **error)
{
	gboolean ret;

	/* register object */
	ret = cd_sensor_register_object (sensor,
					 priv->connection,
					 priv->introspection_sensor->interfaces[0],
					 error);
	if (!ret)
		goto out;

	/* emit signal */
	g_debug ("CdMain: Emitting SensorAdded(%s)",
		 cd_sensor_get_object_path (sensor));
	syslog (LOG_INFO, "Sensor added: %s",
		cd_sensor_get_id (sensor));
	g_dbus_connection_emit_signal (priv->connection,
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
cd_main_add_sensor (CdMainPrivate *priv, CdSensor *sensor)
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
	g_ptr_array_add (priv->sensors, g_object_ref (sensor));

	/* register on bus */
	ret = cd_main_sensor_register_on_bus (priv, sensor, &error);
	if (!ret) {
		g_ptr_array_remove (priv->sensors, sensor);
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
cd_main_setup_standard_space (CdMainPrivate *priv,
			      const gchar *space,
			      const gchar *search)
{
	CdProfile *profile = NULL;

	/* depending on the prefix, find the profile */
	if (g_str_has_prefix (search, "icc_")) {
		profile = cd_profile_array_get_by_id_owner (priv->profiles_array,
							    search,
							    0);
	} else if (g_str_has_prefix (search, "/")) {
		profile = cd_profile_array_get_by_filename (priv->profiles_array,
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
	g_hash_table_insert (priv->standard_spaces,
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
cd_main_setup_standard_spaces (CdMainPrivate *priv)
{
	gchar **spaces;
	gchar **split;
	guint i;

	/* get overrides */
	spaces = cd_config_get_strv (priv->config, "StandardSpaces");
	if (spaces == NULL) {
		g_debug ("no standard space overrides");
		goto out;
	}

	/* parse them */
	for (i = 0; spaces[i] != NULL; i++) {
		split = g_strsplit (spaces[i], ":", 2);
		if (g_strv_length (split) == 2) {
			cd_main_setup_standard_space (priv,
						      split[0],
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
 * cd_main_plugin_phase:
 **/
static void
cd_main_plugin_phase (CdMainPrivate *priv, CdPluginPhase phase)
{
	CdPluginFunc plugin_func = NULL;
	CdPlugin *plugin;
	const gchar *function = NULL;
	gboolean ret;
	guint i;

	switch (phase) {
	case CD_PLUGIN_PHASE_INIT:
		function = "cd_plugin_initialize";
		break;
	case CD_PLUGIN_PHASE_DESTROY:
		function = "cd_plugin_destroy";
		break;
	case CD_PLUGIN_PHASE_COLDPLUG:
		function = "cd_plugin_coldplug";
		break;
	case CD_PLUGIN_PHASE_STATE_CHANGED:
		function = "cd_plugin_state_changed";
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	g_assert (function != NULL);

	/* run each plugin */
	for (i = 0; i < priv->plugins->len; i++) {
		plugin = g_ptr_array_index (priv->plugins, i);
		ret = g_module_symbol (plugin->module,
				       function,
				       (gpointer *) &plugin_func);
		if (!ret)
			continue;
		g_debug ("run %s on %s",
			 function,
			 g_module_name (plugin->module));
		plugin_func (plugin);
		g_debug ("finished %s", function);
	}
}

/**
 * cd_main_on_name_acquired_cb:
 **/
static void
cd_main_on_name_acquired_cb (GDBusConnection *connection,
			     const gchar *name,
			     gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	CdProfileSearchFlags flags;
	CdSensor *sensor = NULL;
	const gchar *device_id;
	gboolean ret;
	GError *error = NULL;
	GPtrArray *array_devices = NULL;
	guint i;

	g_debug ("CdMain: acquired name: %s", name);

	/* add system profiles */
	priv->profile_store = cd_profile_store_new ();
	g_signal_connect (priv->profile_store, "added",
			  G_CALLBACK (cd_main_profile_store_added_cb),
			  user_data);
	g_signal_connect (priv->profile_store, "removed",
			  G_CALLBACK (cd_main_profile_store_removed_cb),
			  user_data);

	/* search locations specified in the config file */
	flags = CD_PROFILE_STORE_SEARCH_SYSTEM |
		CD_PROFILE_STORE_SEARCH_MACHINE;
	ret = cd_config_get_boolean (priv->config, "SearchVolumes");
	if (ret)
		flags |= CD_PROFILE_STORE_SEARCH_VOLUMES;
	cd_profile_store_search (priv->profile_store, flags);

	/* add disk devices */
	array_devices = cd_device_db_get_devices (priv->device_db, &error);
	if (array_devices == NULL) {
		g_warning ("CdMain: failed to get the disk devices: %s",
			    error->message);
		g_error_free (error);
		goto out;
	}
	for (i = 0; i < array_devices->len; i++) {
		device_id = g_ptr_array_index (array_devices, i);
		cd_main_add_disk_device (priv, device_id);
	}

#ifdef HAVE_GUDEV
	/* add sensor devices */
	cd_sensor_client_coldplug (priv->sensor_client);
#endif

	/* coldplug plugin devices */
	cd_main_plugin_phase (priv, CD_PLUGIN_PHASE_COLDPLUG);

	/* add dummy sensor */
	ret = cd_config_get_boolean (priv->config, "CreateDummySensor");
	if (ret) {
		sensor = cd_sensor_new ();
		cd_sensor_set_kind (sensor, CD_SENSOR_KIND_DUMMY);
		ret = cd_sensor_load (sensor, &error);
		if (!ret) {
			g_warning ("CdMain: failed to load dummy sensor: %s",
				    error->message);
			g_clear_error (&error);
		} else {
			cd_main_add_sensor (priv, sensor);
		}
	}

	/* now we've got the profiles, setup the overrides */
	cd_main_setup_standard_spaces (priv);
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
cd_main_on_name_lost_cb (GDBusConnection *connection,
			 const gchar *name,
			 gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	g_debug ("CdMain: lost name: %s", name);
	g_main_loop_quit (priv->loop);
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
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	cd_main_add_sensor (priv, sensor);
}

/**
 * cd_main_client_sensor_removed_cb:
 **/
static void
cd_main_client_sensor_removed_cb (CdSensorClient *sensor_client_,
				  CdSensor *sensor,
				  gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;

	/* emit signal */
	g_debug ("CdMain: Emitting SensorRemoved(%s)",
		 cd_sensor_get_object_path (sensor));
	syslog (LOG_INFO, "Sensor removed: %s",
		cd_sensor_get_id (sensor));
	g_dbus_connection_emit_signal (priv->connection,
				       NULL,
				       COLORD_DBUS_PATH,
				       COLORD_DBUS_INTERFACE,
				       "SensorRemoved",
				       g_variant_new ("(o)",
						      cd_sensor_get_object_path (sensor)),
				       NULL);
	g_ptr_array_remove (priv->sensors, sensor);
}
#endif

/**
 * cd_main_timed_exit_cb:
 **/
static gboolean
cd_main_timed_exit_cb (gpointer user_data)
{
	GMainLoop *loop = (GMainLoop *) user_data;
	g_main_loop_quit (loop);
	return G_SOURCE_REMOVE;
}

/**
 * cd_main_load_introspection:
 **/
static GDBusNodeInfo *
cd_main_load_introspection (const gchar *filename, GError **error)
{
	GBytes *data;
	gchar *path;
	GDBusNodeInfo *info = NULL;

	/* lookup data */
	path = g_build_filename ("/org/freedesktop/colord", filename, NULL);
	data = g_resource_lookup_data (cd_get_resource (),
				       path,
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       error);
	if (data == NULL)
		goto out;

	/* build introspection from XML */
	info = g_dbus_node_info_new_for_xml (g_bytes_get_data (data, NULL), error);
	if (info == NULL)
		goto out;
out:
	g_free (path);
	if (data != NULL)
		g_bytes_unref (data);
	return info;
}

/**
 * cd_main_plugin_free:
 **/
static void
cd_main_plugin_free (CdPlugin *plugin)
{
	g_free (plugin->priv);
	g_module_close (plugin->module);
	g_free (plugin);
}

/**
 * cd_main_plugin_device_added:
 **/
static void
cd_main_plugin_device_added_cb (CdPlugin *plugin,
				CdDevice *device,
				gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	gboolean ret;
	GError *error = NULL;

	cd_device_set_mode (device, CD_DEVICE_MODE_PHYSICAL);
	ret = cd_main_device_add (priv, device, NULL, &error);
	if (!ret) {
		g_warning ("CdMain: failed to add device: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* register on bus */
	ret = cd_main_device_register_on_bus (priv, device, &error);
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
 * cd_main_plugin_device_removed_cb:
 **/
static void
cd_main_plugin_device_removed_cb (CdPlugin *plugin,
				  CdDevice *device,
				  gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	g_debug ("CdMain: remove device: %s", cd_device_get_id (device));
	cd_main_device_removed (priv, device);
}

/**
 * cd_main_load_plugin:
 */
static gboolean
cd_main_load_plugin (CdMainPrivate *priv,
		     const gchar *filename,
		     GError **error)
{
	CdPluginConfigEnabledFunc plugin_config_enabled = NULL;
	CdPluginGetDescFunc plugin_desc = NULL;
	CdPlugin *plugin;
	gboolean ret = FALSE;
	GModule *module;

	/* open the plugin and import all symbols */
	module = g_module_open (filename, 0);
	if (module == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FILE_INVALID,
			     "failed to open: %s",
			     g_module_error ());
		goto out;
	}

	/* get description */
	ret = g_module_symbol (module,
			       "cd_plugin_get_description",
			       (gpointer *) &plugin_desc);
	if (!ret) {
		g_set_error_literal (error,
				     CD_CLIENT_ERROR,
				     CD_CLIENT_ERROR_INTERNAL,
				     "plugin requires description");
		g_module_close (module);
		goto out;
	}

	/* give the module the option to opt-out */
	ret = g_module_symbol (module,
			       "cd_plugin_config_enabled",
			       (gpointer *) &plugin_config_enabled);
	if (ret) {
		if (!plugin_config_enabled (priv->config)) {
			ret = FALSE;
			g_set_error_literal (error,
					     CD_CLIENT_ERROR,
					     CD_CLIENT_ERROR_NOT_SUPPORTED,
					     "plugin refused to load");
			g_module_close (module);
			goto out;
		}
	}

	/* print what we know */
	plugin = g_new0 (CdPlugin, 1);
	plugin->user_data = priv;
	plugin->module = module;
	plugin->device_added = cd_main_plugin_device_added_cb;
	plugin->device_removed = cd_main_plugin_device_removed_cb;

	/* add to array */
	g_ptr_array_add (priv->plugins, plugin);

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cd_main_load_plugins:
 */
static void
cd_main_load_plugins (CdMainPrivate *priv)
{
	const gchar *filename_tmp;
	gboolean ret;
	gchar *filename_plugin;
	gchar *path;
	GDir *dir;
	GError *error = NULL;

	/* search in the plugin directory for plugins */
	path = g_build_filename (LIBDIR, "colord-plugins", NULL);
	dir = g_dir_open (path, 0, &error);
	if (dir == NULL) {
		g_warning ("failed to open plugin directory: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* try to open each plugin */
	g_debug ("searching for plugins in %s", path);
	do {
		filename_tmp = g_dir_read_name (dir);
		if (filename_tmp == NULL)
			break;
		if (!g_str_has_suffix (filename_tmp, ".so"))
			continue;
		filename_plugin = g_build_filename (path,
						    filename_tmp,
						    NULL);
		ret = cd_main_load_plugin (priv, filename_plugin, &error);
		if (ret) {
			syslog (LOG_INFO,
				"loaded plugin %s",
				filename_tmp);
		} else {
			if (g_error_matches (error,
					     CD_CLIENT_ERROR,
					     CD_CLIENT_ERROR_NOT_SUPPORTED)) {
				g_debug ("CdMain: %s", error->message);
			} else {
				g_warning ("CdMain: %s", error->message);
			}
			syslog (LOG_INFO,
				"plugin %s not loaded: %s",
				filename_plugin,
				error->message);
			g_clear_error (&error);
		}
		g_free (filename_plugin);
	} while (TRUE);
out:
	if (dir != NULL)
		g_dir_close (dir);
	g_free (path);
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	CdMainPrivate *priv;
	gboolean immediate_exit = FALSE;
	gboolean ret;
	gboolean timed_exit = FALSE;
	GError *error = NULL;
	GOptionContext *context;
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
	openlog ("colord", LOG_CONS, LOG_DAEMON);

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();
	priv = g_new0 (CdMainPrivate, 1);

	/* TRANSLATORS: program name */
	g_set_application_name (_("Color Management"));
	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, cd_debug_get_option_group ());
	g_option_context_set_summary (context, _("Color Management D-Bus Service"));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* get from config */
	priv->config = cd_config_new ();

	/* create new objects */
	priv->loop = g_main_loop_new (NULL, FALSE);
	priv->devices_array = cd_device_array_new ();
	priv->profiles_array = cd_profile_array_new ();
	priv->sensors = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->standard_spaces = g_hash_table_new_full (g_str_hash,
						 g_str_equal,
						 g_free,
						 (GDestroyNotify) g_object_unref);
#ifdef HAVE_GUDEV
	priv->sensor_client = cd_sensor_client_new ();
	g_signal_connect (priv->sensor_client, "sensor-added",
			  G_CALLBACK (cd_main_client_sensor_added_cb),
			  priv);
	g_signal_connect (priv->sensor_client, "sensor-removed",
			  G_CALLBACK (cd_main_client_sensor_removed_cb),
			  priv);
#endif

	/* connect to the mapping db */
	priv->mapping_db = cd_mapping_db_new ();
	ret = cd_mapping_db_load (priv->mapping_db,
				  LOCALSTATEDIR "/lib/colord/mapping.db",
				  &error);
	if (!ret) {
		g_warning ("CdMain: failed to load mapping database: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* connect to the device db */
	priv->device_db = cd_device_db_new ();
	ret = cd_device_db_load (priv->device_db,
				 LOCALSTATEDIR "/lib/colord/storage.db",
				 &error);
	if (!ret) {
		g_warning ("CdMain: failed to load device database: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* load introspection from file */
	priv->introspection_daemon = cd_main_load_introspection (COLORD_DBUS_INTERFACE ".xml",
								 &error);
	if (priv->introspection_daemon == NULL) {
		g_warning ("CdMain: failed to load daemon introspection: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	priv->introspection_device = cd_main_load_introspection (COLORD_DBUS_INTERFACE_DEVICE ".xml",
								 &error);
	if (priv->introspection_device == NULL) {
		g_warning ("CdMain: failed to load device introspection: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	priv->introspection_profile = cd_main_load_introspection (COLORD_DBUS_INTERFACE_PROFILE ".xml",
								  &error);
	if (priv->introspection_profile == NULL) {
		g_warning ("CdMain: failed to load profile introspection: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	priv->introspection_sensor = cd_main_load_introspection (COLORD_DBUS_INTERFACE_SENSOR ".xml",
								 &error);
	if (priv->introspection_sensor == NULL) {
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
				   priv, NULL);

	/* Only timeout and close the mainloop if we have specified it
	 * on the command line */
	if (immediate_exit)
		g_idle_add (cd_main_timed_exit_cb, priv->loop);
	else if (timed_exit)
		g_timeout_add_seconds (5, cd_main_timed_exit_cb, priv->loop);

	/* load plugins */
	priv->plugins = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_main_plugin_free);
	cd_main_load_plugins (priv);
	cd_main_plugin_phase (priv, CD_PLUGIN_PHASE_INIT);

	/* wait */
	syslog (LOG_INFO, "Daemon ready for requests");
	g_main_loop_run (priv->loop);

	/* run the plugins */
	cd_main_plugin_phase (priv, CD_PLUGIN_PHASE_DESTROY);
	closelog ();

	/* success */
	retval = 0;
out:
	if (owner_id > 0)
		g_bus_unown_name (owner_id);
	if (priv->loop != NULL)
		g_main_loop_unref (priv->loop);
	if (priv->sensors != NULL)
		g_ptr_array_unref (priv->sensors);
	if (priv->plugins != NULL)
		g_ptr_array_unref (priv->plugins);
	g_hash_table_destroy (priv->standard_spaces);
#ifdef HAVE_GUDEV
	if (priv->sensor_client != NULL)
		g_object_unref (priv->sensor_client);
#endif
	if (priv->config != NULL)
		g_object_unref (priv->config);
	if (priv->profile_store != NULL)
		g_object_unref (priv->profile_store);
	if (priv->mapping_db != NULL)
		g_object_unref (priv->mapping_db);
	if (priv->device_db != NULL)
		g_object_unref (priv->device_db);
	if (priv->devices_array != NULL)
		g_object_unref (priv->devices_array);
	if (priv->profiles_array != NULL)
		g_object_unref (priv->profiles_array);
	if (priv->connection != NULL)
		g_object_unref (priv->connection);
	if (priv->introspection_daemon != NULL)
		g_dbus_node_info_unref (priv->introspection_daemon);
	if (priv->introspection_device != NULL)
		g_dbus_node_info_unref (priv->introspection_device);
	if (priv->introspection_profile != NULL)
		g_dbus_node_info_unref (priv->introspection_profile);
	if (priv->introspection_sensor != NULL)
		g_dbus_node_info_unref (priv->introspection_sensor);
	g_free (priv);
	return retval;
}

