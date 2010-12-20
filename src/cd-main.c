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

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <locale.h>
#include <polkit/polkit.h>

#include "cd-common.h"

static GDBusConnection *connection = NULL;
static GDBusNodeInfo *introspection_daemon = NULL;
static GDBusNodeInfo *introspection_device = NULL;
static GDBusNodeInfo *introspection_profile = NULL;
static GMainLoop *loop = NULL;
static GPtrArray *devices_array = NULL;
static GPtrArray *profiles_array = NULL;
static PolkitAuthority *authority = NULL;

typedef struct {
	gchar		*device_id;
	gchar		*object_path;
	guint		 registration_id;
	GPtrArray	*profiles;
} CdDeviceItem;

typedef struct {
	gchar		*filename;
	gchar		*object_path;
	gchar		*profile_id;
	gchar		*qualifier;
	gchar		*title;
	guint		 registration_id;
} CdProfileItem;

/**
 * cd_main_device_item_free:
 **/
static void
cd_main_device_item_free (CdDeviceItem *item)
{
	if (item->registration_id > 0) {
		g_dbus_connection_unregister_object (connection,
						     item->registration_id);
	}
	g_ptr_array_unref (item->profiles);
	g_free (item->object_path);
	g_free (item->device_id);
	g_free (item);
}

/**
 * cd_main_profile_item_free:
 **/
static void
cd_main_profile_item_free (CdProfileItem *item)
{
	if (item->registration_id > 0) {
		g_dbus_connection_unregister_object (connection,
						     item->registration_id);
	}
	g_free (item->qualifier);
	g_free (item->title);
	g_free (item->object_path);
	g_free (item->profile_id);
	g_free (item->filename);
	g_free (item);
}

/**
 * cd_main_device_find_by_object_path:
 **/
static CdDeviceItem *
cd_main_device_find_by_object_path (const gchar *object_path)
{
	CdDeviceItem *item = NULL;
	CdDeviceItem *item_tmp;
	guint i;

	/* find item */
	for (i=0; i<devices_array->len; i++) {
		item_tmp = g_ptr_array_index (devices_array, i);
		if (g_strcmp0 (item_tmp->object_path, object_path) == 0) {
			item = item_tmp;
			break;
		}
	}
	return item;
}

/**
 * cd_main_profile_find_by_object_path:
 **/
static CdProfileItem *
cd_main_profile_find_by_object_path (const gchar *object_path)
{
	CdProfileItem *item = NULL;
	CdProfileItem *item_tmp;
	guint i;

	/* find item */
	for (i=0; i<profiles_array->len; i++) {
		item_tmp = g_ptr_array_index (profiles_array, i);
		if (g_strcmp0 (item_tmp->object_path, object_path) == 0) {
			item = item_tmp;
			break;
		}
	}
	return item;
}

/**
 * cd_main_device_find_by_id:
 **/
static CdDeviceItem *
cd_main_device_find_by_id (const gchar *device_id)
{
	CdDeviceItem *item = NULL;
	CdDeviceItem *item_tmp;
	guint i;

	/* find item */
	for (i=0; i<devices_array->len; i++) {
		item_tmp = g_ptr_array_index (devices_array, i);
		if (g_strcmp0 (item_tmp->device_id, device_id) == 0) {
			item = item_tmp;
			break;
		}
	}
	return item;
}

/**
 * cd_main_profile_find_by_id:
 **/
static CdProfileItem *
cd_main_profile_find_by_id (const gchar *profile_id)
{
	CdProfileItem *item = NULL;
	CdProfileItem *item_tmp;
	guint i;

	/* find item */
	for (i=0; i<profiles_array->len; i++) {
		item_tmp = g_ptr_array_index (profiles_array, i);
		if (g_strcmp0 (item_tmp->profile_id, profile_id) == 0) {
			item = item_tmp;
			break;
		}
	}
	return item;
}

typedef enum {
	CD_MAIN_ERROR_FAILED,
	CD_MAIN_ERROR_LAST
} CdMainError;

/**
 * cd_main_error_quark:
 **/
static GQuark
cd_main_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark) {
		quark = g_quark_from_static_string ("colord");
		g_dbus_error_register_error (quark,
					     CD_MAIN_ERROR_FAILED,
					     COLORD_DBUS_SERVICE ".Failed");
	}
	return quark;
}

#define CD_MAIN_ERROR			cd_main_error_quark()

/**
 * cd_main_profile_emit_changed:
 **/
static void
cd_main_profile_emit_changed (CdProfileItem *item)
{
	gboolean ret;
	GError *error = NULL;

	/* emit signal */
	ret = g_dbus_connection_emit_signal (connection,
					     NULL,
					     item->object_path,
					     COLORD_DBUS_INTERFACE_PROFILE,
					     "Changed",
					     NULL,
					     &error);
	if (!ret) {
		g_warning ("failed to send signal %s", error->message);
		g_error_free (error);
	}
}

/**
 * cd_main_sender_authenticated:
 **/
static gboolean
cd_main_sender_authenticated (GDBusMethodInvocation *invocation, const gchar *sender)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	PolkitAuthorizationResult *result = NULL;
	PolkitSubject *subject;

	/* do authorization async */
	subject = polkit_system_bus_name_new (sender);
	result = polkit_authority_check_authorization_sync (authority, subject,
			"org.freedesktop.color-manager.add-device",
			NULL,
			POLKIT_CHECK_AUTHORIZATION_FLAGS_NONE,
			NULL,
			&error);

	/* failed */
	if (result == NULL) {
		g_dbus_method_invocation_return_error (invocation,
						       CD_MAIN_ERROR,
						       CD_MAIN_ERROR_FAILED,
						       "could not check for auth: %s",
						       error->message);
		g_error_free (error);
		goto out;
	}

	/* did not auth */
	if (!polkit_authorization_result_get_is_authorized (result)) {
		g_dbus_method_invocation_return_error (invocation,
						       CD_MAIN_ERROR,
						       CD_MAIN_ERROR_FAILED,
						       "failed to obtain auth");
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	if (result != NULL)
		g_object_unref (result);
	g_object_unref (subject);
	return ret;
}

/**
 * cd_main_profile_method_call:
 **/
static void
cd_main_profile_method_call (GDBusConnection *connection_, const gchar *sender,
			    const gchar *object_path, const gchar *interface_name,
			    const gchar *method_name, GVariant *parameters,
			    GDBusMethodInvocation *invocation, gpointer user_data)
{
	CdProfileItem *item;
	gboolean ret;
	gchar *filename = NULL;
	gchar **profiles = NULL;
	gchar *qualifier = NULL;
	GVariant *tuple = NULL;
	GVariant *value = NULL;

	/* return '' */
	if (g_strcmp0 (method_name, "SetFilename") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation, sender);
		if (!ret)
			goto out;

		/* copy the profile path */
		item = cd_main_profile_find_by_object_path (object_path);
		if (item == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "profile object path '%s' does not exist",
							       object_path);
			goto out;
		}

		/* check the profile_object_path exists */
		g_variant_get (parameters, "(s)",
			       &filename);

		/* add to the array */
		item->filename = g_strdup (filename);
		item->title = g_strdup ("This is a parsed profile title");

		/* emit */
		cd_main_profile_emit_changed (item);

		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	/* return '' */
	if (g_strcmp0 (method_name, "SetQualifier") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation, sender);
		if (!ret)
			goto out;

		/* copy the profile path */
		item = cd_main_profile_find_by_object_path (object_path);
		if (item == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "profile object path '%s' does not exist",
							       object_path);
			goto out;
		}

		/* check the profile_object_path exists */
		g_variant_get (parameters, "(s)",
			       &qualifier);

		/* save in the struct */
		item->qualifier = g_strdup (qualifier);

		/* emit */
		cd_main_profile_emit_changed (item);

		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	/* we suck */
	g_critical ("failed to process method %s", method_name);
out:
	g_free (filename);
	g_free (qualifier);
	if (tuple != NULL)
		g_variant_unref (tuple);
	if (value != NULL)
		g_variant_unref (value);
	g_strfreev (profiles);
	return;
}

/**
 * cd_main_profile_get_property:
 **/
static GVariant *
cd_main_profile_get_property (GDBusConnection *connection_, const gchar *sender,
			     const gchar *object_path, const gchar *interface_name,
			     const gchar *property_name, GError **error,
			     gpointer user_data)
{
	CdProfileItem *item;
	gchar **profiles = NULL;
	GVariant *retval = NULL;

	if (g_strcmp0 (property_name, "Title") == 0) {
		item = cd_main_profile_find_by_object_path (object_path);
		if (item->title != NULL)
			retval = g_variant_new_string (item->title);
		goto out;
	}
	if (g_strcmp0 (property_name, "ProfileId") == 0) {
		item = cd_main_profile_find_by_object_path (object_path);
		if (item->profile_id != NULL)
			retval = g_variant_new_string (item->profile_id);
		goto out;
	}
	if (g_strcmp0 (property_name, "Qualifier") == 0) {
		item = cd_main_profile_find_by_object_path (object_path);
		if (item->qualifier != NULL)
			retval = g_variant_new_string (item->qualifier);
		goto out;
	}
	if (g_strcmp0 (property_name, "Filename") == 0) {
		item = cd_main_profile_find_by_object_path (object_path);
		if (item->filename != NULL)
			retval = g_variant_new_string (item->filename);
		goto out;
	}

	g_critical ("failed to set property %s", property_name);
out:
	g_strfreev (profiles);
	return retval;
}

/**
 * cd_main_create_profile:
 **/
static CdProfileItem *
cd_main_create_profile (const gchar *profile_id, GError **error)
{
	gboolean ret;
	CdProfileItem *item;
	static const GDBusInterfaceVTable interface_vtable = {
		cd_main_profile_method_call,
		cd_main_profile_get_property,
		NULL
	};

	g_assert (connection != NULL);

	/* create an object */
	item = g_new0 (CdProfileItem, 1);
	item->profile_id = g_strdup (profile_id);
	item->object_path = g_build_filename (COLORD_DBUS_PATH, profile_id, NULL);
	g_ptr_array_add (profiles_array, item);
	g_debug ("Adding profile %s", item->object_path);
	item->registration_id = g_dbus_connection_register_object (connection,
							     item->object_path,
							     introspection_profile->interfaces[0],
							     &interface_vtable,
							     NULL,  /* user_data */
							     NULL,  /* user_data_free_func */
							     NULL); /* GError** */
	g_assert (item->registration_id > 0);

	/* emit signal */
	ret = g_dbus_connection_emit_signal (connection,
					     NULL,
					     COLORD_DBUS_PATH,
					     COLORD_DBUS_INTERFACE_PROFILE,
					     "ProfileAdded",
					     g_variant_new ("(s)",
							    item->object_path),
					     error);
	if (!ret)
		goto out;
out:
	return item;
}

/**
 * cd_main_device_emit_changed:
 **/
static void
cd_main_device_emit_changed (CdDeviceItem *item)
{
	gboolean ret;
	GError *error = NULL;

	/* emit signal */
	ret = g_dbus_connection_emit_signal (connection,
					     NULL,
					     item->object_path,
					     COLORD_DBUS_INTERFACE_DEVICE,
					     "Changed",
					     NULL,
					     &error);
	if (!ret) {
		g_warning ("failed to send signal %s", error->message);
		g_error_free (error);
	}
}

/**
 * cd_main_device_method_call:
 **/
static void
cd_main_device_method_call (GDBusConnection *connection_, const gchar *sender,
			    const gchar *object_path, const gchar *interface_name,
			    const gchar *method_name, GVariant *parameters,
			    GDBusMethodInvocation *invocation, gpointer user_data)
{
	CdDeviceItem *item;
	CdProfileItem *item_profile;
	gboolean ret;
	gchar **devices = NULL;
	gchar *profile_object_path = NULL;
	GVariant *tuple = NULL;
	GVariant *value = NULL;

	/* return '' */
	if (g_strcmp0 (method_name, "AddProfile") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation, sender);
		if (!ret)
			goto out;

		/* copy the device path */
		item = cd_main_device_find_by_object_path (object_path);
		if (item == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "device object path '%s' does not exist",
							       object_path);
			goto out;
		}

		/* check the profile_object_path exists */
		g_variant_get (parameters, "(o)",
			       &profile_object_path);
		item_profile = cd_main_profile_find_by_object_path (profile_object_path);
		if (item_profile == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "profile object path '%s' does not exist",
							       profile_object_path);
			goto out;
		}

		/* add to the array */
		g_ptr_array_add (item->profiles, g_strdup (profile_object_path));

		/* emit */
		cd_main_device_emit_changed (item);

		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	/* we suck */
	g_critical ("failed to process method %s", method_name);
out:
	g_free (profile_object_path);
	if (tuple != NULL)
		g_variant_unref (tuple);
	if (value != NULL)
		g_variant_unref (value);
	g_strfreev (devices);
	return;
}

/**
 * cd_main_device_get_property:
 **/
static GVariant *
cd_main_device_get_property (GDBusConnection *connection_, const gchar *sender,
			     const gchar *object_path, const gchar *interface_name,
			     const gchar *property_name, GError **error,
			     gpointer user_data)
{
	CdDeviceItem *item;
	const gchar *profile;
	guint i;
	GVariant **profiles = NULL;
	GVariant *retval = NULL;

	if (g_strcmp0 (property_name, "Created") == 0) {
		retval = g_variant_new_uint64 (0);
		goto out;
	}
	if (g_strcmp0 (property_name, "Model") == 0) {
		retval = g_variant_new_string ("hello dave");
		goto out;
	}
	if (g_strcmp0 (property_name, "DeviceId") == 0) {
		item = cd_main_device_find_by_object_path (object_path);
		retval = g_variant_new_string (item->device_id);
		goto out;
	}
	if (g_strcmp0 (property_name, "Profiles") == 0) {
		item = cd_main_device_find_by_object_path (object_path);

		/* copy the object paths */
		profiles = g_new0 (GVariant *, item->profiles->len + 1);
		for (i=0; i<item->profiles->len; i++) {
			profile = g_ptr_array_index (item->profiles, i);
			profiles[i] = g_variant_new_object_path (profile);
		}

		/* format the value */
		retval = g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
					      profiles,
					      item->profiles->len);
		goto out;
	}

	g_critical ("failed to set property %s", property_name);
out:
	return retval;
}

/**
 * cd_main_create_device:
 **/
static CdDeviceItem *
cd_main_create_device (const gchar *device_id, GError **error)
{
	gboolean ret;
	CdDeviceItem *item;
	static const GDBusInterfaceVTable interface_vtable = {
		cd_main_device_method_call,
		cd_main_device_get_property,
		NULL
	};

	g_assert (connection != NULL);

	/* create an object */
	item = g_new0 (CdDeviceItem, 1);
	item->device_id = g_strdup (device_id);
	item->object_path = g_build_filename (COLORD_DBUS_PATH, device_id, NULL);
	item->profiles = g_ptr_array_new_with_free_func (g_free);
	g_ptr_array_add (devices_array, item);
	g_debug ("Adding device %s", item->object_path);
	item->registration_id = g_dbus_connection_register_object (connection,
							     item->object_path,
							     introspection_device->interfaces[0],
							     &interface_vtable,
							     NULL,  /* user_data */
							     NULL,  /* user_data_free_func */
							     NULL); /* GError** */
	g_assert (item->registration_id > 0);

	/* emit signal */
	ret = g_dbus_connection_emit_signal (connection,
					     NULL,
					     COLORD_DBUS_PATH,
					     COLORD_DBUS_INTERFACE_DEVICE,
					     "DeviceAdded",
					     g_variant_new ("(o)",
							    item->object_path),
					     error);
	if (!ret)
		goto out;
out:
	return item;
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
	CdDeviceItem *item_device;
	CdProfileItem *item_profile;
	gboolean ret;
	gchar *device_id = NULL;
	GError *error = NULL;
	guint i;
	GVariant *tuple = NULL;
	GVariant *value = NULL;
	GVariant **variant_array = NULL;

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetDevices") == 0) {

		/* copy the object paths */
		variant_array = g_new0 (GVariant *, devices_array->len + 1);
		for (i=0; i<devices_array->len; i++) {
			item_device = g_ptr_array_index (devices_array, i);
			variant_array[i] = g_variant_new_object_path (item_device->object_path);
		}

		/* format the value */
		value = g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
					     variant_array,
					     devices_array->len);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "FindDeviceById") == 0) {

		g_variant_get (parameters, "(s)", &device_id);
		item_device = cd_main_device_find_by_id (device_id);
		if (item_device == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "device id '%s' does not exists",
							       device_id);
			goto out;
		}

		/* format the value */
		value = g_variant_new ("(o)", item_device->object_path);
		g_dbus_method_invocation_return_value (invocation, value);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "FindProfileById") == 0) {

		g_variant_get (parameters, "(s)", &device_id);
		item_profile = cd_main_profile_find_by_id (device_id);
		if (item_profile == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "profile id '%s' does not exists",
							       device_id);
			goto out;
		}

		/* format the value */
		value = g_variant_new ("(o)", item_profile->object_path);
		g_dbus_method_invocation_return_value (invocation, value);
		goto out;
	}

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetProfiles") == 0) {

		/* copy the object paths */
		variant_array = g_new0 (GVariant *, profiles_array->len + 1);
		for (i=0; i<profiles_array->len; i++) {
			item_profile = g_ptr_array_index (profiles_array, i);
			variant_array[i] = g_variant_new_object_path (item_profile->object_path);
		}

		/* format the value */
		value = g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
					     variant_array,
					     profiles_array->len);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "CreateDevice") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation, sender);
		if (!ret)
			goto out;

		/* does already exist */
		g_variant_get (parameters, "(s)", &device_id);
		item_device = cd_main_device_find_by_id (device_id);
		if (item_device != NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "device object path '%s' already exists",
							       item_device->object_path);
			goto out;
		}

		/* copy the device path */
		item_device = cd_main_create_device (device_id, &error);
		if (item_device == NULL) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			goto out;
		}

		/* format the value */
		value = g_variant_new_object_path (item_device->object_path);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "CreateProfile") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation, sender);
		if (!ret)
			goto out;

		/* does already exist */
		g_variant_get (parameters, "(s)", &device_id);
		item_profile = cd_main_profile_find_by_id (device_id);
		if (item_profile != NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "profile object path '%s' already exists",
							       item_profile->object_path);
			goto out;
		}

		/* copy the device path */
		item_profile = cd_main_create_profile (device_id, &error);
		if (item_profile == NULL) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			goto out;
		}

		/* format the value */
		value = g_variant_new_object_path (item_profile->object_path);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* we suck */
	g_critical ("failed to process method %s", method_name);
out:
	if (tuple != NULL)
		g_variant_unref (tuple);
	if (value != NULL)
		g_variant_unref (value);
	if (variant_array != NULL) {
		for (i=0; variant_array[i] != NULL; i++)
			g_variant_unref (variant_array[i]);
	}
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
 * cd_main_on_name_acquired_cb:
 **/
static void
cd_main_on_name_acquired_cb (GDBusConnection *connection_,
			     const gchar *name,
			     gpointer user_data)
{
	g_debug ("acquired name: %s", name);
	connection = g_object_ref (connection_);
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
	devices_array = g_ptr_array_new_with_free_func ((GDestroyNotify)
							cd_main_device_item_free);
	profiles_array = g_ptr_array_new_with_free_func ((GDestroyNotify)
							 cd_main_profile_item_free);
	authority = polkit_authority_get_sync (NULL, &error);
	if (authority == NULL) {
		g_error ("failed to get pokit authority: %s", error->message);
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
				   G_BUS_NAME_OWNER_FLAGS_NONE,
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
	if (devices_array != NULL)
		g_ptr_array_unref (devices_array);
	if (profiles_array != NULL)
		g_ptr_array_unref (profiles_array);
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
	if (authority != NULL)
		g_object_unref (authority);
	g_main_loop_unref (loop);
	return retval;
}

