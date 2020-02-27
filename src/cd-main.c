/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2014 Richard Hughes <richard@hughsie.com>
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
#ifdef __unix__
#include <gio/gunixfdlist.h>
#endif
#include <glib/gi18n.h>
#include <locale.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-login.h>
#endif

#include "cd-common.h"
#include "cd-debug.h"
#include "cd-device-array.h"
#include "cd-device-db.h"
#include "cd-device.h"
#include "cd-mapping-db.h"
#include "cd-plugin.h"
#include "cd-profile-array.h"
#include "cd-profile-db.h"
#include "cd-profile.h"
#include "cd-icc-store.h"
#include "cd-sensor-client.h"

#include "colord-resources.h"

typedef struct {
	GDBusConnection		*connection;
	GDBusNodeInfo		*introspection_daemon;
	GDBusNodeInfo		*introspection_device;
	GDBusNodeInfo		*introspection_profile;
	GDBusNodeInfo		*introspection_sensor;
	CdDeviceArray		*devices_array;
	CdProfileArray		*profiles_array;
	CdIccStore		*icc_store;
	CdMappingDb		*mapping_db;
	CdDeviceDb		*device_db;
	CdProfileDb		*profile_db;
	CdSensorClient		*sensor_client;
	GPtrArray		*sensors;
	GPtrArray		*plugins;
	GMainLoop		*loop;
	gboolean		 create_dummy_sensor;
	gboolean		 always_use_xrandr_name;
	gchar			*system_vendor;
	gchar			*system_model;
} CdMainPrivate;

static void
cd_main_profile_removed (CdMainPrivate *priv, CdProfile *profile)
{
	CdDevice *device_tmp;
	gboolean ret;
	guint i;
	g_autofree gchar *object_path_tmp = NULL;
	g_autoptr(GPtrArray) devices = NULL;

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
			g_info ("Automatic remove of %s from %s",
				cd_profile_get_id (profile),
				cd_device_get_id (device_tmp));
			g_debug ("CdMain: automatically removing %s from %s as removed",
				 object_path_tmp,
				 cd_device_get_object_path (device_tmp));
		}
	}

	/* emit signal */
	g_debug ("CdMain: Emitting ProfileRemoved(%s)", object_path_tmp);
	g_info ("Profile removed: %s", cd_profile_get_id (profile));
	g_dbus_connection_emit_signal (priv->connection,
				       NULL,
				       COLORD_DBUS_PATH,
				       COLORD_DBUS_INTERFACE,
				       "ProfileRemoved",
				       g_variant_new ("(o)",
						      object_path_tmp),
				       NULL);
}

static void
cd_main_profile_invalidate_cb (CdProfile *profile,
			       gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;

	g_debug ("CdMain: profile '%s' invalidated",
		 cd_profile_get_id (profile));
	cd_main_profile_removed (priv, profile);
}

static void
cd_main_device_removed (CdMainPrivate *priv, CdDevice *device)
{
	GError *error = NULL;
	gboolean ret;
	g_autofree gchar *object_path_tmp = NULL;

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
	g_info ("device removed: %s", cd_device_get_id (device));
	g_dbus_connection_emit_signal (priv->connection,
				       NULL,
				       COLORD_DBUS_PATH,
				       COLORD_DBUS_INTERFACE,
				       "DeviceRemoved",
				       g_variant_new ("(o)",
						      object_path_tmp),
				       &error);
}

static void
cd_main_device_invalidate_cb (CdDevice *device,
			      gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;

	g_debug ("CdMain: device '%s' invalidated",
		 cd_device_get_id (device));
	cd_main_device_removed (priv, device);
}

static gboolean
cd_main_add_profile (CdMainPrivate *priv,
		     CdProfile *profile,
		     GError **error)
{
	/* add */
	cd_profile_array_add (priv->profiles_array, profile);
	g_debug ("CdMain: Adding profile %s", cd_profile_get_object_path (profile));

	/* profile is no longer valid */
	g_signal_connect (profile, "invalidate",
			  G_CALLBACK (cd_main_profile_invalidate_cb),
			  priv);
	return TRUE;
}

static CdProfile *
cd_main_create_profile (CdMainPrivate *priv,
			const gchar *sender,
			const gchar *profile_id,
			guint owner,
			CdObjectScope scope,
			GError **error)
{
	g_autoptr(CdProfile) profile_tmp = NULL;

	g_assert (priv->connection != NULL);

	/* create an object */
	profile_tmp = cd_profile_new ();
	cd_profile_set_owner (profile_tmp, owner);
	cd_profile_set_id (profile_tmp, profile_id);
	cd_profile_set_scope (profile_tmp, scope);

	/* add the profile */
	if (!cd_main_add_profile (priv, profile_tmp, error))
		return NULL;

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
		return NULL;
	default:
		g_warning ("CdMain: Unsupported scope kind: %u", scope);
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_NOT_SUPPORTED,
			     "Unsupported scope kind: %u", scope);
		return NULL;
	}

	/* success */
	return g_object_ref (profile_tmp);
}

static gboolean
cd_main_auto_add_from_md (CdMainPrivate *priv,
			  CdDevice *device,
			  CdProfile *profile)
{
	const gchar *device_id;
	const gchar *profile_id;
	const gchar *tmp;
	const gchar **warnings;
	gboolean ret;
	guint64 timestamp;
	g_autoptr(GError) error = NULL;

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
		return FALSE;
	}

	/* if the auto-EDID profile has warnings then do not add this */
	tmp = cd_profile_get_metadata_item (profile, CD_PROFILE_METADATA_DATA_SOURCE);
	if (g_strcmp0 (tmp, CD_PROFILE_METADATA_DATA_SOURCE_EDID) == 0) {
		warnings = cd_profile_get_warnings (profile);
		if (warnings != NULL && g_strv_length ((gchar **) warnings) > 0) {
			g_debug ("CdMain: NOT MD add %s to %s as profile has warnings",
				 profile_id, device_id);
			return FALSE;
		}
	}

	/* auto-add soft relationship */
	g_debug ("CdMain: Automatically MD add %s to %s", profile_id, device_id);
	g_info ("Automatic metadata add %s to %s", profile_id, device_id);
	ret = cd_device_add_profile (device,
				     CD_DEVICE_RELATION_SOFT,
				     cd_profile_get_object_path (profile),
				     g_get_real_time (),
				     &error);
	if (!ret) {
		g_debug ("CdMain: failed to assign, non-fatal: %s",
			 error->message);
		return FALSE;
	}
	return TRUE;
}

static gboolean
cd_main_auto_add_from_db (CdMainPrivate *priv,
			  CdDevice *device,
			  CdProfile *profile)
{
	gboolean ret;
	guint64 timestamp;
	g_autoptr(GError) error = NULL;

	g_debug ("CdMain: Automatically DB add %s to %s",
		 cd_profile_get_id (profile),
		 cd_device_get_object_path (device));
	g_info ("Automatic database add %s to %s",
		cd_profile_get_id (profile),
		cd_device_get_id (device));
	timestamp = cd_mapping_db_get_timestamp (priv->mapping_db,
						 cd_device_get_id (device),
						 cd_profile_get_id (profile),
						 &error);
	if (timestamp == G_MAXUINT64) {
		g_debug ("CdMain: failed to assign, non-fatal: %s",
			 error->message);
		return FALSE;
	}
	ret = cd_device_add_profile (device,
				     CD_DEVICE_RELATION_HARD,
				     cd_profile_get_object_path (profile),
				     timestamp,
				     &error);
	if (!ret) {
		g_debug ("CdMain: failed to assign, non-fatal: %s",
			 error->message);
		return FALSE;
	}
	return TRUE;
}

static void
cd_main_device_auto_add_from_md (CdMainPrivate *priv,
				 CdDevice *device)
{
	CdProfile *profile_tmp;
	guint i;
	g_autoptr(GPtrArray) array = NULL;

	/* get all the profiles, and check to see if any of them contain
	 * MAPPING_device_id that matches the device */
	array = cd_profile_array_get_by_metadata (priv->profiles_array,
						  CD_PROFILE_METADATA_MAPPING_DEVICE_ID,
						  cd_device_get_id (device));
	if (array == NULL)
		return;
	for (i = 0; i < array->len; i++) {
		profile_tmp = g_ptr_array_index (array, i);
		cd_main_auto_add_from_md (priv, device, profile_tmp);
	}
}

static void
cd_main_device_auto_add_from_db (CdMainPrivate *priv, CdDevice *device)
{
	const gchar *object_id_tmp;
	guint64 timestamp;
	guint i;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) array = NULL;

	/* get data */
	array = cd_mapping_db_get_profiles (priv->mapping_db,
					    cd_device_get_id (device),
					    &error);
	if (array == NULL) {
		g_warning ("CdMain: failed to get profiles for device from db: %s",
			   error->message);
		return;
	}

	/* try to add them */
	for (i = 0; i < array->len; i++) {
		g_autoptr(CdProfile) profile_tmp = NULL;
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
			g_debug ("CdMain: profile %s with owner %u is not (yet) available",
				 object_id_tmp, cd_device_get_owner (device));
			continue;
		}

		/* does the profile have the correct device metadata */
		cd_main_auto_add_from_db (priv, device, profile_tmp);
	}
}

static gboolean
cd_main_device_register_on_bus (CdMainPrivate *priv,
				CdDevice *device,
				GError **error)
{
	/* register object */
	if (!cd_device_register_object (device,
					priv->connection,
					priv->introspection_device->interfaces[0],
					error))
		return FALSE;

	/* emit signal */
	g_debug ("CdMain: Emitting DeviceAdded(%s)",
		 cd_device_get_object_path (device));
	g_info ("Device added: %s", cd_device_get_id (device));
	g_dbus_connection_emit_signal (priv->connection,
				       NULL,
				       COLORD_DBUS_PATH,
				       COLORD_DBUS_INTERFACE,
				       "DeviceAdded",
				       g_variant_new ("(o)",
						      cd_device_get_object_path (device)),
				       NULL);
	return TRUE;
}

static gboolean
cd_main_device_add (CdMainPrivate *priv,
		    CdDevice *device,
		    const gchar *sender,
		    GError **error)
{
	gboolean ret;
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
					error);
		if (!ret)
			return FALSE;
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
	return TRUE;
}

static gchar *
cd_main_get_seat_for_process (guint pid)
{
	gchar *seat = NULL;
#ifdef HAVE_SYSTEMD
	char *sd_seat = NULL;
	char *sd_session = NULL;
	gint rc;

	/* get session the process belongs to */
	rc = sd_pid_get_session (pid, &sd_session);
	if (rc < 0) {
		g_debug ("failed to get session [pid %u]: %s",
			 pid, strerror (-rc));
		goto out;
	}

	/* get the seat the session is on */
	rc = sd_session_get_seat (sd_session, &sd_seat);
	if (rc < 0) {
		g_debug ("failed to get seat for session %s [pid %u]: %s",
			 sd_session, pid, strerror (-rc));
		goto out;
	}
	seat = g_strdup (sd_seat);
out:
	free (sd_seat);
	free (sd_session);
#endif
	return seat;
}

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
	g_autofree gchar *seat = NULL;
	g_autoptr(CdDevice) device_tmp = NULL;

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
	if (!cd_main_device_add (priv, device_tmp, sender, error))
		return NULL;

	/* setup DBus watcher */
	if (sender != NULL && scope == CD_OBJECT_SCOPE_TEMP) {
		g_debug ("temporary device");
		cd_device_watch_sender (device_tmp, sender);
	}

	/* success */
	return g_object_ref (device_tmp);
}

static GVariant *
cd_main_device_array_to_variant (GPtrArray *array, guint uid)
{
	CdDevice *device;
	guint i;
	guint length = 0;
	g_autofree GVariant **variant_array = NULL;

	/* copy the object paths */
	variant_array = g_new0 (GVariant *, array->len + 1);
	for (i = 0; i < array->len; i++) {
		device = g_ptr_array_index (array, i);

		/* only show devices created by root and the calling
		 * user, but if called *by* root return all devices
		 * from all users */
		if (uid != 0) {
			if (cd_device_get_owner (device) != 0 &&
			    cd_device_get_owner (device) != uid)
				continue;
		}

		variant_array[length] = g_variant_new_object_path (
			cd_device_get_object_path (device));
		length++;
	}

	/* format the value */
	return g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
				    variant_array,
				    length);
}

static GVariant *
cd_main_profile_array_to_variant (GPtrArray *array)
{
	CdProfile *profile;
	guint i;
	guint length = 0;
	g_autofree GVariant **variant_array = NULL;

	/* copy the object paths */
	variant_array = g_new0 (GVariant *, array->len + 1);
	for (i = 0; i < array->len; i++) {
		profile = g_ptr_array_index (array, i);
		variant_array[length] = g_variant_new_object_path (
			cd_profile_get_object_path (profile));
		length++;
	}

	/* format the value */
	return g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
				    variant_array,
				    length);
}

static GVariant *
cd_main_sensor_array_to_variant (GPtrArray *array)
{
	CdSensor *sensor;
	guint i;
	guint length = 0;
	g_autofree GVariant **variant_array = NULL;

	/* copy the object paths */
	variant_array = g_new0 (GVariant *, array->len + 1);
	for (i = 0; i < array->len; i++) {
		sensor = g_ptr_array_index (array, i);
		variant_array[length] = g_variant_new_object_path (
			cd_sensor_get_object_path (sensor));
		length++;
	}

	/* format the value */
	return g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
				    variant_array,
				    length);
}

static void
cd_main_profile_auto_add_from_db (CdMainPrivate *priv,
				  CdProfile *profile)
{
	guint i;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) array = NULL;

	/* get data */
	array = cd_mapping_db_get_devices (priv->mapping_db,
					   cd_profile_get_id (profile),
					   &error);
	if (array == NULL) {
		g_warning ("CdMain: failed to get profiles for device from db: %s",
			   error->message);
		return;
	}

	/* nothing set */
	if (array->len == 0) {
		g_debug ("no matched device data for profile %s",
			 cd_profile_get_id (profile));
		return;
	}

	/* try to add them */
	for (i = 0; i < array->len; i++) {
		g_autoptr(CdDevice) device_tmp = NULL;
		const gchar *device_id_tmp;

		device_id_tmp = g_ptr_array_index (array, i);
		device_tmp = cd_device_array_get_by_id_owner (priv->devices_array,
							      device_id_tmp,
							      cd_profile_get_owner (profile),
							      CD_DEVICE_ARRAY_FLAG_OWNER_OPTIONAL);
		if (device_tmp == NULL)
			continue;

		/* hard add */
		cd_main_auto_add_from_db (priv, device_tmp, profile);
	}
}

static void
cd_main_profile_auto_add_from_md (CdMainPrivate *priv,
				  CdProfile *profile)
{
	g_autoptr(CdDevice) device = NULL;
	const gchar *device_id;

	/* does the device exists that matches the md */
	device_id = cd_profile_get_metadata_item (profile,
						  CD_PROFILE_METADATA_MAPPING_DEVICE_ID);
	if (device_id == NULL)
		return;
	device = cd_device_array_get_by_id_owner (priv->devices_array,
						  device_id,
						  cd_profile_get_owner (profile),
						  CD_DEVICE_ARRAY_FLAG_OWNER_OPTIONAL);
	if (device == NULL)
		return;
	cd_main_auto_add_from_md (priv, device, profile);
}

typedef enum {
	CD_LOGGING_FLAG_NONE		= 0,
	CD_LOGGING_FLAG_SYSLOG		= 1,
	CD_LOGGING_FLAG_LAST
} CdLoggingFlags;

static gboolean
cd_main_profile_register_on_bus (CdMainPrivate *priv,
				 CdProfile *profile,
				 CdLoggingFlags logging,
				 GError **error)
{
	/* register object */
	if (!cd_profile_register_object (profile,
					 priv->connection,
					 priv->introspection_profile->interfaces[0],
					 error))
		return FALSE;

	/* emit signal */
	g_debug ("CdMain: Emitting ProfileAdded(%s)",
		 cd_profile_get_object_path (profile));
	if ((logging & CD_LOGGING_FLAG_SYSLOG) > 0)
		g_info ("Profile added: %s", cd_profile_get_id (profile));
	g_dbus_connection_emit_signal (priv->connection,
				       NULL,
				       COLORD_DBUS_PATH,
				       COLORD_DBUS_INTERFACE,
				       "ProfileAdded",
				       g_variant_new ("(o)",
						      cd_profile_get_object_path (profile)),
				       NULL);
	return TRUE;
}

static CdProfile *
cd_main_get_standard_space_metadata (CdMainPrivate *priv,
				     const gchar *standard_space)
{
	CdProfile *profile_best = NULL;
	CdProfile *profile_tmp;
	guint i;
	guint score_best = 0;
	guint score_tmp;
	g_autoptr(GPtrArray) array = NULL;

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
		return NULL;

	/* success */
	return g_object_ref (profile_best);
}

static gchar *
cd_main_get_display_fallback_id (GVariant *dict)
{
	const gchar *prop_key;
	const gchar *prop_value;
	gchar *device_id = NULL;
	GVariantIter *iter;

	iter = g_variant_iter_new (dict);
	while (g_variant_iter_next (iter, "{&s&s}",
				    &prop_key, &prop_value)) {
		if (g_strcmp0 (prop_key, CD_DEVICE_METADATA_XRANDR_NAME) != 0)
			continue;
		if (prop_value == NULL || prop_value[0] == '\0')
			continue;
		device_id = g_strdup_printf ("xrandr-%s", prop_value);
		break;
	}
	g_variant_iter_free (iter);
	return device_id;
}

static gchar *
cd_main_get_cmdline_for_pid (guint pid)
{
	gboolean ret;
	gchar *cmdline = NULL;
	gsize len = 0;
	guint i;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *proc_path = NULL;

	/* just read the link */
	proc_path = g_strdup_printf ("/proc/%u/cmdline", pid);
	ret = g_file_get_contents (proc_path, &cmdline, &len, &error);
	if (!ret) {
		g_debug ("CdMain: failed to read %s: %s",
			   proc_path, error->message);
		return NULL;
	}
	if (len == 0) {
		g_debug ("CdMain: failed to read %s", proc_path);
		g_free (cmdline);
		return NULL;
	}

	/* turn all the \0's into spaces */
	for (i = 0; i < len; i++) {
		if (cmdline[i] == '\0')
			cmdline[i] = ' ';
	}
	return cmdline;
}

static void
cd_main_daemon_method_call (GDBusConnection *connection, const gchar *sender,
			    const gchar *object_path, const gchar *interface_name,
			    const gchar *method_name, GVariant *parameters,
			    GDBusMethodInvocation *invocation, gpointer user_data)
{
	CdDeviceKind device_kind;
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	CdObjectScope scope;
	GVariant *tuple = NULL;
	GVariant *value = NULL;
	const gchar *device_id = NULL;
	const gchar *metadata_key = NULL;
	const gchar *metadata_value = NULL;
	const gchar *prop_key;
	const gchar *prop_value;
	const gchar *scope_tmp = NULL;
	gboolean register_on_bus = TRUE;
	gboolean ret;
	guint i;
	guint pid;
	guint uid;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *cmdline = NULL;
	g_autofree gchar *device_id_fallback = NULL;
	g_autofree gchar *filename = NULL;
	g_autoptr(CdDevice) device = NULL;
	g_autoptr(CdProfile) profile = NULL;
	g_autoptr(GPtrArray) array = NULL;
	g_autoptr(GVariantIter) iter = NULL;
	g_autoptr(GVariant) dict = NULL;

	/* get the owner of the message */
	uid = cd_main_get_sender_uid (connection, sender, &error);
	if (uid == G_MAXUINT) {
		g_dbus_method_invocation_return_error (invocation,
						       CD_CLIENT_ERROR,
						       CD_CLIENT_ERROR_INTERNAL,
						       "failed to get owner: %s",
						       error->message);
		return;
	}

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetDevices") == 0) {

		g_debug ("CdMain: %s:GetDevices()", sender);

		/* format the value */
		array = cd_device_array_get_array (priv->devices_array);
		value = cd_main_device_array_to_variant (array, uid);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		return;
	}

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetSensors") == 0) {

		g_debug ("CdMain: %s:GetSensors()", sender);

		/* format the value */
		value = cd_main_sensor_array_to_variant (priv->sensors);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		return;
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
			return;
		}
		array = cd_device_array_get_by_kind (priv->devices_array,
						     device_kind);

		/* format the value */
		value = cd_main_device_array_to_variant (array, uid);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		return;
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
		return;
	}

	/* return 'o' */
	if (g_strcmp0 (method_name, "FindDeviceById") == 0) {

		g_variant_get (parameters, "(&s)", &device_id);
		g_debug ("CdMain: %s:FindDeviceById(%s)",
			 sender, device_id);
		device = cd_device_array_get_by_id_owner (priv->devices_array,
							  device_id,
							  uid,
							  CD_DEVICE_ARRAY_FLAG_OWNER_OPTIONAL);
		if (device == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_NOT_FOUND,
							       "device id '%s' does not exist",
							       device_id);
			return;
		}

		/* format the value */
		value = g_variant_new ("(o)", cd_device_get_object_path (device));
		g_dbus_method_invocation_return_value (invocation, value);
		return;
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
			return;
		}

		/* format the value */
		value = g_variant_new ("(o)", cd_device_get_object_path (device));
		g_dbus_method_invocation_return_value (invocation, value);
		return;
	}

	/* return 'o' */
	if (g_strcmp0 (method_name, "FindSensorById") == 0) {

		g_variant_get (parameters, "(&s)", &device_id);
		g_debug ("CdMain: %s:FindSensorById(%s)",
			 sender, device_id);

		/* find sensor */
		for (i = 0; i < priv->sensors->len; i++) {
			CdSensor *sensor_tmp;
			sensor_tmp = g_ptr_array_index (priv->sensors, i);
			if (g_strcmp0 (cd_sensor_get_id (sensor_tmp), device_id) == 0) {
				value = g_variant_new ("(o)", cd_sensor_get_object_path (sensor_tmp));
				g_dbus_method_invocation_return_value (invocation, value);
				return;
			}
		}
		g_dbus_method_invocation_return_error (invocation,
						       CD_CLIENT_ERROR,
						       CD_CLIENT_ERROR_NOT_FOUND,
						       "sensor id '%s' does not exist",
						       device_id);
		return;
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
			return;
		}

		/* format the value */
		value = g_variant_new ("(o)", cd_profile_get_object_path (profile));
		g_dbus_method_invocation_return_value (invocation, value);
		return;
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
			return;
		}

		/* format the value */
		value = g_variant_new ("(o)", cd_profile_get_object_path (profile));
		g_dbus_method_invocation_return_value (invocation, value);
		return;
	}

	/* return 'o' */
	if (g_strcmp0 (method_name, "GetStandardSpace") == 0) {

		g_variant_get (parameters, "(&s)", &device_id);
		g_debug ("CdMain: %s:GetStandardSpace(%s)",
			 sender, device_id);

		/* will also return overrides */
		profile = cd_main_get_standard_space_metadata (priv, device_id);
		if (profile == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_NOT_FOUND,
							       "profile space '%s' does not exist",
							       device_id);
			return;
		}

		/* format the value */
		value = g_variant_new ("(o)", cd_profile_get_object_path (profile));
		g_dbus_method_invocation_return_value (invocation, value);
		return;
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
			return;
		}

		/* format the value */
		value = g_variant_new ("(o)", cd_profile_get_object_path (profile));
		g_dbus_method_invocation_return_value (invocation, value);
		return;
	}

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetProfiles") == 0) {

		/* format the value */
		g_debug ("CdMain: %s:GetProfiles()", sender);
		value = cd_profile_array_get_variant (priv->profiles_array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		return;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "CreateDevice") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (connection,
						    sender,
						    "org.freedesktop.color-manager.create-device",
						    &error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror (invocation, error);
			return;
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
			return;
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
			return;
		}
		device_kind = cd_device_kind_from_string (prop_value);
		if (device_kind == CD_DEVICE_KIND_UNKNOWN) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_INPUT_INVALID,
							       "device type %s not recognised",
							       prop_value);
			return;
		}

		/* are we using the XRANDR_name property rather than the
		 * sent device-id? */
		if (priv->always_use_xrandr_name &&
		    device_kind == CD_DEVICE_KIND_DISPLAY) {
			device_id_fallback = cd_main_get_display_fallback_id (dict);
			if (device_id_fallback == NULL) {
				g_dbus_method_invocation_return_error (invocation,
								       CD_CLIENT_ERROR,
								       CD_CLIENT_ERROR_INPUT_INVALID,
								       "AlwaysUseXrandrName mode enabled and %s unset",
								       CD_DEVICE_METADATA_XRANDR_NAME);
				return;
			}
			device_id = device_id_fallback;
		}

		/* check it does not already exist */
		scope = cd_object_scope_from_string (scope_tmp);
		if (scope == CD_OBJECT_SCOPE_UNKNOWN) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_INPUT_INVALID,
							       "scope non-valid: %s",
							       scope_tmp);
			return;
		}
		device = cd_device_array_get_by_id_owner (priv->devices_array,
							  device_id,
							  uid,
							  CD_DEVICE_ARRAY_FLAG_NONE);
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
				return;
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
			return;
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
			g_dbus_method_invocation_return_gerror (invocation, error);
			return;
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
				return;
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
				return;
			}
		}

		/* register on bus */
		if (register_on_bus) {
			ret = cd_main_device_register_on_bus (priv, device, &error);
			if (!ret) {
				g_dbus_method_invocation_return_gerror (invocation,
									error);
				return;
			}
		}

		/* format the value */
		value = g_variant_new_object_path (cd_device_get_object_path (device));
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		return;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "DeleteDevice") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (connection,
						    sender,
						    "org.freedesktop.color-manager.delete-device",
						    &error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror (invocation, error);
			return;
		}

		/* does already exist */
		g_variant_get (parameters, "(&o)", &device_id);
		g_debug ("CdMain: %s:DeleteDevice(%s)",
			 sender, device_id);
		device = cd_device_array_get_by_id_owner (priv->devices_array,
							  device_id,
							  uid,
							  CD_DEVICE_ARRAY_FLAG_OWNER_OPTIONAL);
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
				return;
			}
		}

		/* remove from the array, and emit */
		cd_main_device_removed (priv, device);

		g_dbus_method_invocation_return_value (invocation, NULL);
		return;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "DeleteProfile") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (connection,
						    sender,
						    "org.freedesktop.color-manager.create-profile",
						    &error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror (invocation, error);
			return;
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
				return;
			}
		}

		/* remove from the array, and emit */
		cd_main_profile_removed (priv, profile);

		g_dbus_method_invocation_return_value (invocation, NULL);
		return;
	}

	/* return 'o' */
	if (g_strcmp0 (method_name, "CreateProfile") == 0 ||
	    g_strcmp0 (method_name, "CreateProfileWithFd") == 0) {

#ifdef __unix__
		GDBusMessage *message;
		GUnixFDList *fd_list;
#endif
		gint32 fd_handle = 0;

		/* require auth */
		ret = cd_main_sender_authenticated (connection,
						    sender,
						    "org.freedesktop.color-manager.create-profile",
						    &error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror (invocation, error);
			return;
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
			return;
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
			return;
		}

		/* create profile */
		scope = cd_object_scope_from_string (scope_tmp);
		if (scope == CD_OBJECT_SCOPE_UNKNOWN) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_INPUT_INVALID,
							       "scope non-valid: %s",
							       scope_tmp);
			return;
		}
		profile = cd_main_create_profile (priv,
						  sender,
						  device_id,
						  uid,
						  scope,
						  &error);
		if (profile == NULL) {
			g_dbus_method_invocation_return_gerror (invocation, error);
			return;
		}

		/* set the properties */
		while (g_variant_iter_next (iter, "{&s&s}",
					    &prop_key, &prop_value)) {
			if (g_strcmp0 (prop_key, CD_PROFILE_PROPERTY_FILENAME) == 0)
				filename = g_strdup (prop_value);
			ret = cd_profile_set_property_internal (profile,
								prop_key,
								prop_value,
								uid,
								&error);
			if (!ret) {
				g_dbus_method_invocation_return_gerror (invocation,
									error);
				return;
			}
		}

		/* get any file descriptor in the message */
#ifdef __unix__
		message = g_dbus_method_invocation_get_message (invocation);
		fd_list = g_dbus_message_get_unix_fd_list (message);
		if (fd_list != NULL && g_unix_fd_list_get_length (fd_list) == 1) {
			gint fd;
			fd = g_unix_fd_list_get (fd_list, fd_handle, &error);
			if (fd < 0) {
				g_warning ("CdMain: failed to get fd from message: %s",
					   error->message);
				g_dbus_method_invocation_return_gerror (invocation,
									error);
				return;
			}

			/* read from a fd, avoiding open() */
			ret = cd_profile_load_from_fd (profile, fd, &error);
			if (!ret) {
				g_warning ("CdMain: failed to profile from fd: %s",
					   error->message);
				g_dbus_method_invocation_return_gerror (invocation,
									error);
				return;
			}

		/* clients like CUPS do not use FD passing */
		} else if (filename != NULL) {
			ret = cd_profile_load_from_filename (profile,
							     filename,
							     &error);
			if (!ret) {
				g_warning ("CdMain: failed to profile from filename: %s",
					   error->message);
				g_dbus_method_invocation_return_gerror (invocation, error);
				return;
			}
		}
#else
		if (filename != NULL) {
			ret = cd_profile_load_from_filename (profile,
							     filename,
							     &error);
			if (!ret) {
				g_warning ("CdMain: failed to profile from filename: %s",
					   error->message);
				g_dbus_method_invocation_return_gerror (invocation, error);
				return;
			}
		} else {
			g_dbus_method_invocation_return_error (invocation,
							       CD_CLIENT_ERROR,
							       CD_CLIENT_ERROR_NOT_SUPPORTED,
							       "no FD support");
			return;
		}
#endif
		/* auto add profiles from the database and metadata */
		cd_main_profile_auto_add_from_db (priv, profile);
		cd_main_profile_auto_add_from_md (priv, profile);

		/* register on bus */
		ret = cd_main_profile_register_on_bus (priv,
						       profile,
						       CD_LOGGING_FLAG_SYSLOG,
						       &error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror (invocation, error);
			return;
		}

		/* format the value */
		value = g_variant_new_object_path (cd_profile_get_object_path (profile));
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		return;
	}

	/* we suck */
	g_critical ("failed to process method %s", method_name);
}

static GVariant *
cd_main_daemon_get_property (GDBusConnection *connection_, const gchar *sender,
			     const gchar *object_path, const gchar *interface_name,
			     const gchar *property_name, GError **error,
			     gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;

	if (g_strcmp0 (property_name, CD_CLIENT_PROPERTY_DAEMON_VERSION) == 0)
		return g_variant_new_string (VERSION);
	if (g_strcmp0 (property_name, CD_CLIENT_PROPERTY_SYSTEM_VENDOR) == 0)
		return g_variant_new_string (priv->system_vendor);
	if (g_strcmp0 (property_name, CD_CLIENT_PROPERTY_SYSTEM_MODEL) == 0)
		return g_variant_new_string (priv->system_model);

	/* return an error */
	g_set_error (error,
		     CD_CLIENT_ERROR,
		     CD_CLIENT_ERROR_INTERNAL,
		     "failed to get daemon property %s",
		     property_name);
	return NULL;
}

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

static void
cd_main_icc_store_added_cb (CdIccStore *icc_store,
			    CdIcc *icc,
			    gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	const gchar *checksum;
	const gchar *filename;
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *profile_id = NULL;
	g_autoptr(CdProfile) profile = NULL;

	/* create profile */
	profile = cd_profile_new ();
	filename = cd_icc_get_filename (icc);
	if (g_str_has_prefix (filename, "/usr/share/color") ||
	    g_str_has_prefix (filename, "/var/lib/color"))
		cd_profile_set_is_system_wide (profile, TRUE);

	/* parse the profile name */
	ret = cd_profile_load_from_icc (profile, icc, &error);
	if (!ret) {
		g_warning ("CdIccStore: failed to add profile '%s': %s",
			   filename, error->message);
		return;
	}

	/* ensure profiles have the checksum metadata item */
	checksum = cd_profile_get_checksum (profile);
	cd_profile_set_property_internal (profile,
					  CD_PROFILE_METADATA_FILE_CHECKSUM,
					  checksum,
					  0, /* uid unknown */
					  NULL);

	/* just add it to the bus with the title as the ID */
	profile_id = g_strdup_printf ("icc-%s", cd_icc_get_checksum (icc));
	cd_profile_set_id (profile, profile_id);
	ret = cd_main_add_profile (priv, profile, &error);
	if (!ret) {
		g_warning ("CdMain: failed to add profile: %s",
			   error->message);
		return;
	}

	/* register on bus */
	ret = cd_main_profile_register_on_bus (priv,
					       profile,
					       CD_LOGGING_FLAG_NONE,
					       &error);
	if (!ret) {
		g_warning ("CdMain: failed to emit ProfileAdded: %s",
			   error->message);
		return;
	}
}

static void
cd_main_icc_store_removed_cb (CdIccStore *icc_store,
			      CdIcc *icc,
			      gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	CdProfile *profile;

	/* check the profile should be invalidated automatically */
	profile = cd_profile_array_get_by_filename (priv->profiles_array,
						    cd_icc_get_filename (icc));
	if (profile == NULL)
		return;
	g_debug ("%s removed, so invalidating", cd_icc_get_filename (icc));
	cd_profile_array_remove (priv->profiles_array, profile);
}

static void
cd_main_add_disk_device (CdMainPrivate *priv, const gchar *device_id)
{
	const gchar *property;
	gboolean ret;
	guint i;
	g_autoptr(GError) error = NULL;
	g_autoptr(CdDevice) device = NULL;
	g_autoptr(GPtrArray) array_properties = NULL;

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
		return;
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
		return;
	}
	for (i = 0; i < array_properties->len; i++) {
		g_autofree gchar *value = NULL;
		property = g_ptr_array_index (array_properties, i);
		value = cd_device_db_get_property (priv->device_db,
						   device_id,
						   property,
						   &error);
		if (value == NULL) {
			g_warning ("CdMain: failed to get value: %s",
				   error->message);
			g_clear_error (&error);
			return;
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
			return;
		}
	}

	/* register on bus */
	ret = cd_main_device_register_on_bus (priv, device, &error);
	if (!ret) {
		g_warning ("CdMain: failed to emit DeviceAdded: %s",
			   error->message);
		return;
	}
}

static gboolean
cd_main_sensor_register_on_bus (CdMainPrivate *priv,
				CdSensor *sensor,
				GError **error)
{
	/* register object */
	if (!cd_sensor_register_object (sensor,
					priv->connection,
					priv->introspection_sensor->interfaces[0],
					error))
		return FALSE;

	/* emit signal */
	g_debug ("CdMain: Emitting SensorAdded(%s)",
		 cd_sensor_get_object_path (sensor));
	g_info ("Sensor added: %s", cd_sensor_get_id (sensor));
	g_dbus_connection_emit_signal (priv->connection,
				       NULL,
				       COLORD_DBUS_PATH,
				       COLORD_DBUS_INTERFACE,
				       "SensorAdded",
				       g_variant_new ("(o)",
						      cd_sensor_get_object_path (sensor)),
				       NULL);
	return TRUE;
}

static void
cd_main_add_sensor (CdMainPrivate *priv, CdSensor *sensor)
{
	const gchar *id;
	gboolean ret;
	g_autoptr(GError) error = NULL;

	id = cd_sensor_get_id (sensor);
	if (id == NULL) {
		g_warning ("did not get an ID from the sensor");
		return;
	}
	g_debug ("CdMain: add sensor: %s", id);
	g_ptr_array_add (priv->sensors, g_object_ref (sensor));

	/* register on bus */
	ret = cd_main_sensor_register_on_bus (priv, sensor, &error);
	if (!ret) {
		g_ptr_array_remove (priv->sensors, sensor);
		g_warning ("CdMain: failed to emit SensorAdded: %s",
			   error->message);
		return;
	}
}

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

static void
cd_main_on_name_acquired_cb (GDBusConnection *connection,
			     const gchar *name,
			     gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	const gchar *device_id;
	gboolean ret;
	guint i;
	g_autoptr(GError) error = NULL;
	g_autoptr(CdSensor) sensor = NULL;
	g_autoptr(GPtrArray) array_devices = NULL;

	g_debug ("CdMain: acquired name: %s", name);

	/* add system profiles */
	priv->icc_store = cd_icc_store_new ();
	cd_icc_store_set_load_flags (priv->icc_store, CD_ICC_LOAD_FLAGS_FALLBACK_MD5);
	cd_icc_store_set_cache (priv->icc_store, cd_get_resource ());
	g_signal_connect (priv->icc_store, "added",
			  G_CALLBACK (cd_main_icc_store_added_cb),
			  user_data);
	g_signal_connect (priv->icc_store, "removed",
			  G_CALLBACK (cd_main_icc_store_removed_cb),
			  user_data);

	/* search locations for ICC profiles */
	ret = cd_icc_store_search_kind (priv->icc_store,
					CD_ICC_STORE_SEARCH_KIND_SYSTEM,
					CD_ICC_STORE_SEARCH_FLAGS_NONE,
					NULL,
					&error);
	if (!ret) {
		g_warning ("CdMain: failed to search system directories: %s",
			    error->message);
		return;
	}
	ret = cd_icc_store_search_kind (priv->icc_store,
					CD_ICC_STORE_SEARCH_KIND_MACHINE,
					CD_ICC_STORE_SEARCH_FLAGS_NONE,
					NULL,
					&error);
	if (!ret) {
		g_warning ("CdMain: failed to search machine directories: %s",
			    error->message);
		return;
	}

	/* add disk devices */
	array_devices = cd_device_db_get_devices (priv->device_db, &error);
	if (array_devices == NULL) {
		g_warning ("CdMain: failed to get the disk devices: %s",
			    error->message);
		return;
	}
	for (i = 0; i < array_devices->len; i++) {
		device_id = g_ptr_array_index (array_devices, i);
		cd_main_add_disk_device (priv, device_id);
	}

	/* add sensor devices */
	cd_sensor_client_coldplug (priv->sensor_client);

	/* coldplug plugin devices */
	cd_main_plugin_phase (priv, CD_PLUGIN_PHASE_COLDPLUG);

	/* add dummy sensor */
	if (priv->create_dummy_sensor) {
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
}

static void
cd_main_on_name_lost_cb (GDBusConnection *connection,
			 const gchar *name,
			 gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	g_debug ("CdMain: lost name: %s", name);
	g_main_loop_quit (priv->loop);
}


static void
cd_main_client_sensor_added_cb (CdSensorClient *sensor_client_,
				CdSensor *sensor,
				gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	cd_main_add_sensor (priv, sensor);
}

static void
cd_main_client_sensor_removed_cb (CdSensorClient *sensor_client_,
				  CdSensor *sensor,
				  gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;

	/* emit signal */
	g_debug ("CdMain: Emitting SensorRemoved(%s)",
		 cd_sensor_get_object_path (sensor));
	g_info ("Sensor removed: %s", cd_sensor_get_id (sensor));
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

static gboolean
cd_main_timed_exit_cb (gpointer user_data)
{
	GMainLoop *loop = (GMainLoop *) user_data;
	g_main_loop_quit (loop);
	return G_SOURCE_REMOVE;
}

static GDBusNodeInfo *
cd_main_load_introspection (const gchar *filename, GError **error)
{
	g_autoptr(GBytes) data = NULL;
	g_autofree gchar *path = NULL;

	/* lookup data */
	path = g_build_filename ("/org/freedesktop/colord", filename, NULL);
	data = g_resource_lookup_data (cd_get_resource (),
				       path,
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       error);
	if (data == NULL)
		return NULL;

	/* build introspection from XML */
	return g_dbus_node_info_new_for_xml (g_bytes_get_data (data, NULL), error);
}

static void
cd_main_plugin_free (CdPlugin *plugin)
{
	g_free (plugin->priv);
	g_module_close (plugin->module);
	g_free (plugin);
}

static void
cd_main_plugin_device_added_cb (CdPlugin *plugin,
				CdDevice *device,
				gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	gboolean ret;
	g_autoptr(GError) error = NULL;

	cd_device_set_mode (device, CD_DEVICE_MODE_PHYSICAL);
	ret = cd_main_device_add (priv, device, NULL, &error);
	if (!ret) {
		g_warning ("CdMain: failed to add device: %s",
			   error->message);
		return;
	}

	/* register on bus */
	ret = cd_main_device_register_on_bus (priv, device, &error);
	if (!ret) {
		g_warning ("CdMain: failed to emit DeviceAdded: %s",
			   error->message);
		return;
	}
}

static void
cd_main_plugin_device_removed_cb (CdPlugin *plugin,
				  CdDevice *device,
				  gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	g_debug ("CdMain: remove device: %s", cd_device_get_id (device));
	cd_main_device_removed (priv, device);
}

static gboolean
cd_main_load_plugin (CdMainPrivate *priv,
		     const gchar *filename,
		     GError **error)
{
	CdPluginEnabledFunc plugin_enabled = NULL;
	CdPluginGetDescFunc plugin_desc = NULL;
	CdPlugin *plugin;
	GModule *module;

	/* open the plugin and import all symbols */
	module = g_module_open (filename, 0);
	if (module == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FILE_INVALID,
			     "failed to open: %s",
			     g_module_error ());
		return FALSE;
	}

	/* get description */
	if (!g_module_symbol (module, "cd_plugin_get_description",
			      (gpointer *) &plugin_desc)) {
		g_set_error_literal (error,
				     CD_CLIENT_ERROR,
				     CD_CLIENT_ERROR_INTERNAL,
				     "plugin requires description");
		g_module_close (module);
		return FALSE;
	}

	/* give the module the option to opt-out */
	if (g_module_symbol (module, "cd_plugin_enabled",
			     (gpointer *) &plugin_enabled)) {
		if (!plugin_enabled ()) {
			g_set_error_literal (error,
					     CD_CLIENT_ERROR,
					     CD_CLIENT_ERROR_NOT_SUPPORTED,
					     "plugin refused to load");
			g_module_close (module);
			return FALSE;
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
	return TRUE;
}

static void
cd_main_load_plugins (CdMainPrivate *priv)
{
	const gchar *filename_tmp;
	gboolean ret;
	g_autoptr(GDir) dir = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *path = NULL;

	/* search in the plugin directory for plugins */
	path = g_build_filename (LIBDIR, "colord-plugins", NULL);
	dir = g_dir_open (path, 0, &error);
	if (dir == NULL) {
		g_warning ("failed to open plugin directory: %s",
			   error->message);
		return;
	}

	/* try to open each plugin */
	g_debug ("searching for plugins in %s", path);
	do {
		g_autofree gchar *filename_plugin = NULL;
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
			g_info ("loaded plugin %s", filename_tmp);
		} else {
			if (g_error_matches (error,
					     CD_CLIENT_ERROR,
					     CD_CLIENT_ERROR_NOT_SUPPORTED)) {
				g_debug ("CdMain: %s", error->message);
			} else {
				g_warning ("CdMain: %s", error->message);
			}
			g_info ("plugin %s not loaded: %s",
				filename_plugin, error->message);
			g_clear_error (&error);
		}
	} while (TRUE);
}

static CdEdid *
cd_main_get_edid_for_output (const gchar *output_name)
{
	gboolean ret;
	gsize len = 0;
	g_autoptr(GBytes) data = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *edid_data = NULL;
	g_autofree gchar *edid_fn = NULL;
	g_autofree gchar *enabled_data = NULL;
	g_autofree gchar *enabled_fn = NULL;
	g_autoptr(CdEdid) edid = NULL;

	/* check output is actually an output */
	enabled_fn = g_build_filename ("/sys/class/drm",
				       output_name,
				       "enabled",
				       NULL);
	ret = g_file_test (enabled_fn, G_FILE_TEST_EXISTS);
	if (!ret)
		return NULL;

	/* check output is enabled */
	ret = g_file_get_contents (enabled_fn, &enabled_data, NULL, &error);
	if (!ret) {
		g_warning ("failed to get enabled data: %s", error->message);
		return NULL;
	}
	g_strdelimit (enabled_data, "\n", '\0');
	if (g_strcmp0 (enabled_data, "enabled") != 0)
		return NULL;

	/* get EDID data */
	edid_fn = g_build_filename ("/sys/class/drm",
				    output_name,
				    "edid",
				    NULL);
	ret = g_file_get_contents (edid_fn, &edid_data, &len, &error);
	if (!ret) {
		g_warning ("failed to get edid data: %s", error->message);
		return NULL;
	}

	/* parse EDID */
	edid = cd_edid_new ();
	data = g_bytes_new (edid_data, len);
	if (!cd_edid_parse (edid, data, &error)) {
		g_warning ("failed to get edid data: %s", error->message);
		return NULL;
	}
	return g_object_ref (edid);
}

static gchar *
cd_main_get_display_id (CdEdid *edid)
{
	GString *device_id;

	device_id = g_string_new ("xrandr");
	if (cd_edid_get_vendor_name (edid) != NULL)
		g_string_append_printf (device_id, "-%s", cd_edid_get_vendor_name (edid));
	if (cd_edid_get_monitor_name (edid) != NULL)
		g_string_append_printf (device_id, "-%s", cd_edid_get_monitor_name (edid));
	if (cd_edid_get_serial_number (edid) != NULL)
		g_string_append_printf (device_id, "-%s", cd_edid_get_serial_number (edid));
	return g_string_free (device_id, FALSE);
}

static gboolean
cd_main_check_duplicate_edids (void)
{
	const gchar *fn;
	gboolean use_xrandr_mode = FALSE;
	g_autoptr(GDir) dir = NULL;
	g_autoptr(GHashTable) hash = NULL;

	dir = g_dir_open ("/sys/class/drm", 0, NULL);
	if (dir == NULL)
		return FALSE;

	/* read all the outputs in /sys/class/drm and search for duplicates */
	hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	while (!use_xrandr_mode && (fn = g_dir_read_name (dir)) != NULL) {
		gpointer old_output;
		g_autoptr(CdEdid) edid = NULL;
		edid = cd_main_get_edid_for_output (fn);
		if (edid == NULL)
			continue;
		g_debug ("display %s has ID '%s' from MD5 %s",
			 fn, cd_main_get_display_id (edid),
			 cd_edid_get_checksum (edid));
		old_output = g_hash_table_lookup (hash, cd_edid_get_checksum (edid));
		if (old_output != NULL) {
			g_debug ("output %s has duplicate EDID", fn);
			use_xrandr_mode = TRUE;
		}
		g_hash_table_insert (hash,
				     g_strdup (cd_edid_get_checksum (edid)),
				     GINT_TO_POINTER (1));
	}
	return use_xrandr_mode;
}

static gchar *
cd_main_dmi_get_from_filename (const gchar *filename)
{
	gboolean ret;
	gchar *data = NULL;
	g_autoptr(GError) error = NULL;

	/* get the contents */
	ret = g_file_get_contents (filename, &data, NULL, &error);
	if (!ret) {
		if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
			g_warning ("failed to get contents of %s: %s",
				   filename, error->message);
	}

	/* process the random chars and trailing spaces */
	if (data != NULL) {
		g_strdelimit (data, "\t_", ' ');
		g_strdelimit (data, "\n\r", '\0');
		g_strchomp (data);
	}

	/* don't return an empty string */
	if (data != NULL && data[0] == '\0') {
		g_free (data);
		data = NULL;
	}

	return data;
}

static gchar *
cd_main_dmi_get_from_filenames (const gchar * const * filenames)
{
	guint i;
	gchar *tmp = NULL;

	/* try each one in preference order */
	for (i = 0; filenames[i] != NULL; i++) {
		tmp = cd_main_dmi_get_from_filename (filenames[i]);
		if (tmp != NULL)
			break;
	}
	return tmp;
}


static gchar *
cd_main_dmi_get_vendor (void)
{
	const gchar *sysfs_vendor[] = {
		"/sys/class/dmi/id/sys_vendor",
		"/sys/class/dmi/id/chassis_vendor",
		"/sys/class/dmi/id/board_vendor",
		NULL};
	g_autofree gchar *tmp = NULL;

	/* get vendor name */
	tmp = cd_main_dmi_get_from_filenames (sysfs_vendor);
	if (tmp == NULL)
		return g_strdup("Unknown");
	return cd_quirk_vendor_name (tmp);
}
static gchar *
cd_main_dmi_get_model (void)
{
	const gchar *sysfs_model[] = {
		"/sys/class/dmi/id/product_name",
		"/sys/class/dmi/id/board_name",
		NULL};
	gchar *model;
	g_autofree gchar *tmp = NULL;

	/* thinkpad puts the common name in the version field, urgh */
	tmp = cd_main_dmi_get_from_filename ("/sys/class/dmi/id/product_version");
	if (tmp != NULL && g_strstr_len (tmp, -1, "ThinkPad") != NULL)
		return g_strdup (tmp);

	/* get where the model should be */
	model = cd_main_dmi_get_from_filenames (sysfs_model);
	if (model == NULL)
		return g_strdup ("Unknown");
	return model;
}

static void
cd_main_dmi_setup (CdMainPrivate *priv)
{
	priv->system_vendor = cd_main_dmi_get_vendor ();
	priv->system_model = cd_main_dmi_get_model ();
}

int
main (int argc, char *argv[])
{
	CdMainPrivate *priv = NULL;
	gboolean immediate_exit = FALSE;
	gboolean create_dummy_sensor = FALSE;
	gboolean ret;
	gboolean timed_exit = FALSE;
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
		{ "create-dummy-sensor", '\0', 0, G_OPTION_ARG_NONE, &create_dummy_sensor,
		  /* TRANSLATORS: exit straight away, used for automatic profiling */
		  _("Create a dummy sensor for testing"), NULL },
		{ NULL}
	};
	g_autoptr(GError) error = NULL;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* TRANSLATORS: program name */
	g_set_application_name (_("Color Management"));
	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, cd_debug_get_option_group ());
	g_option_context_set_summary (context, _("Color Management D-Bus Service"));
	ret = g_option_context_parse (context, &argc, &argv, &error);
	if (!ret) {
		g_warning ("CdMain: failed to parse command line arguments: %s",
			   error->message);
		goto out;
	}

	/* create new objects */
	priv = g_new0 (CdMainPrivate, 1);
	priv->create_dummy_sensor = create_dummy_sensor;
	priv->loop = g_main_loop_new (NULL, FALSE);
	priv->devices_array = cd_device_array_new ();
	priv->profiles_array = cd_profile_array_new ();
	priv->sensors = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->sensor_client = cd_sensor_client_new ();
	g_signal_connect (priv->sensor_client, "sensor-added",
			  G_CALLBACK (cd_main_client_sensor_added_cb),
			  priv);
	g_signal_connect (priv->sensor_client, "sensor-removed",
			  G_CALLBACK (cd_main_client_sensor_removed_cb),
			  priv);

	/* connect to the mapping db */
	priv->mapping_db = cd_mapping_db_new ();
	ret = cd_mapping_db_load (priv->mapping_db,
				  LOCALSTATEDIR "/lib/colord/mapping.db",
				  &error);
	if (!ret) {
		g_warning ("CdMain: failed to load mapping database: %s",
			   error->message);
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
		goto out;
	}

	/* connect to the profile db */
	priv->profile_db = cd_profile_db_new ();
	ret = cd_profile_db_load (priv->profile_db,
				  LOCALSTATEDIR "/lib/colord/storage.db",
				  &error);
	if (!ret) {
		g_warning ("CdMain: failed to load profile database: %s",
			   error->message);
		goto out;
	}

	/* load introspection from file */
	priv->introspection_daemon = cd_main_load_introspection (COLORD_DBUS_INTERFACE ".xml",
								 &error);
	if (priv->introspection_daemon == NULL) {
		g_warning ("CdMain: failed to load daemon introspection: %s",
			   error->message);
		goto out;
	}
	priv->introspection_device = cd_main_load_introspection (COLORD_DBUS_INTERFACE_DEVICE ".xml",
								 &error);
	if (priv->introspection_device == NULL) {
		g_warning ("CdMain: failed to load device introspection: %s",
			   error->message);
		goto out;
	}
	priv->introspection_profile = cd_main_load_introspection (COLORD_DBUS_INTERFACE_PROFILE ".xml",
								  &error);
	if (priv->introspection_profile == NULL) {
		g_warning ("CdMain: failed to load profile introspection: %s",
			   error->message);
		goto out;
	}
	priv->introspection_sensor = cd_main_load_introspection (COLORD_DBUS_INTERFACE_SENSOR ".xml",
								 &error);
	if (priv->introspection_sensor == NULL) {
		g_warning ("CdMain: failed to load sensor introspection: %s",
			   error->message);
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

	/* If the user has two or more outputs attached with identical EDID data
	 * then the client tools cannot tell them apart. By setting this value
	 * the 'xrandr-' style device-id is always used and the monitors will
	 * show up as seporate instances.
	 * This does of course mean that the calibration is referenced to the
	 * xrandr output name, rather than the monitor itself. This means that
	 * if the monitor cables are swapped then the wrong profile would be
	 * used. */
	priv->always_use_xrandr_name = cd_main_check_duplicate_edids ();

	/* load plugins */
	priv->plugins = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_main_plugin_free);
	cd_main_load_plugins (priv);
	cd_main_plugin_phase (priv, CD_PLUGIN_PHASE_INIT);

	/* get system DMI info */
	cd_main_dmi_setup (priv);
	g_debug ("System vendor: '%s', System model: '%s'",
		 priv->system_vendor, priv->system_model);

	/* wait */
	g_info ("Daemon ready for requests");
	g_main_loop_run (priv->loop);

	/* run the plugins */
	cd_main_plugin_phase (priv, CD_PLUGIN_PHASE_DESTROY);

	/* success */
	retval = 0;
out:
	cd_debug_destroy ();
	g_option_context_free (context);
	if (owner_id > 0)
		g_bus_unown_name (owner_id);
	if (priv != NULL) {
		if (priv->loop != NULL)
			g_main_loop_unref (priv->loop);
		if (priv->sensors != NULL)
			g_ptr_array_unref (priv->sensors);
		if (priv->plugins != NULL)
			g_ptr_array_unref (priv->plugins);
		if (priv->sensor_client != NULL)
			g_object_unref (priv->sensor_client);
		if (priv->icc_store != NULL)
			g_object_unref (priv->icc_store);
		if (priv->mapping_db != NULL)
			g_object_unref (priv->mapping_db);
		if (priv->device_db != NULL)
			g_object_unref (priv->device_db);
		if (priv->profile_db != NULL)
			g_object_unref (priv->profile_db);
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
		g_free (priv->system_vendor);
		g_free (priv->system_model);
		g_free (priv);
	}
	return retval;
}

