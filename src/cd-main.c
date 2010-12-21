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

static GDBusConnection *connection = NULL;
static GDBusNodeInfo *introspection_daemon = NULL;
static GDBusNodeInfo *introspection_device = NULL;
static GDBusNodeInfo *introspection_profile = NULL;
static GMainLoop *loop = NULL;
static CdDeviceArray *devices_array = NULL;
static CdProfileArray *profiles_array = NULL;

/**
 * cd_main_create_profile:
 **/
static CdProfile *
cd_main_create_profile (const gchar *profile_id, GError **error)
{
	gboolean ret;
	CdProfile *profile;

	g_assert (connection != NULL);

	/* create an object */
	profile = cd_profile_new ();
	cd_profile_set_id (profile, profile_id);

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

	/* emit signal */
	ret = g_dbus_connection_emit_signal (connection,
					     NULL,
					     COLORD_DBUS_PATH,
					     COLORD_DBUS_INTERFACE_PROFILE,
					     "ProfileAdded",
					     g_variant_new ("(s)",
							    cd_profile_get_object_path (profile)),
					     error);
	if (!ret)
		goto out;
out:
	return profile;
}

/**
 * cd_main_create_device:
 **/
static CdDevice *
cd_main_create_device (const gchar *device_id, GError **error)
{
	gboolean ret;
	CdDevice *device;

	g_assert (connection != NULL);

	/* create an object */
	device = cd_device_new ();
	cd_device_set_id (device, device_id);
	cd_device_array_add (devices_array, device);
	g_debug ("Adding device %s", cd_device_get_object_path (device));

	/* register object */
	ret = cd_device_register_object (device,
					 connection,
					 introspection_device->interfaces[0],
					 error);
	if (!ret)
		goto out;

	/* emit signal */
	ret = g_dbus_connection_emit_signal (connection,
					     NULL,
					     COLORD_DBUS_PATH,
					     COLORD_DBUS_INTERFACE_DEVICE,
					     "DeviceAdded",
					     g_variant_new ("(o)",
							    cd_device_get_object_path (device)),
					     error);
	if (!ret)
		goto out;
out:
	return device;
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
	CdDevice *device;
	CdProfile *profile;
	gboolean ret;
	gchar *device_id = NULL;
	gchar *object_path_tmp = NULL;
	GError *error = NULL;
	GVariant *tuple = NULL;
	GVariant *value = NULL;

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetDevices") == 0) {

		/* format the value */
		value = cd_device_array_get_variant (devices_array);
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
		ret = cd_main_sender_authenticated (invocation, sender);
		if (!ret)
			goto out;

		/* does already exist */
		g_variant_get (parameters, "(s)", &device_id);
		device = cd_device_array_get_by_id (devices_array, device_id);
		if (device != NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "device object path '%s' already exists",
							       cd_device_get_object_path (device));
			goto out;
		}

		/* copy the device path */
		device = cd_main_create_device (device_id, &error);
		if (device == NULL) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			goto out;
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
		ret = cd_main_sender_authenticated (invocation, sender);
		if (!ret)
			goto out;

		/* does already exist */
		g_variant_get (parameters, "(s)", &device_id);
		device = cd_device_array_get_by_id (devices_array, device_id);
		if (device == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "device id '%s' not found",
							       device_id);
			goto out;
		}

		/* remove from the array before emitting */
		object_path_tmp = g_strdup (cd_device_get_object_path (device));
		cd_device_array_remove (devices_array, device);
		g_debug ("Removing device %s", cd_device_get_object_path (device));

		/* emit signal */
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

		g_dbus_method_invocation_return_value (invocation, NULL);
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
		profile = cd_profile_array_get_by_id (profiles_array, device_id);
		if (profile != NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "profile object path '%s' already exists",
							       cd_profile_get_object_path (profile));
			goto out;
		}

		/* copy the device path */
		profile = cd_main_create_profile (device_id, &error);
		if (profile == NULL) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
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
	g_free (object_path_tmp);
	if (tuple != NULL)
		g_variant_unref (tuple);
	if (value != NULL)
		g_variant_unref (value);
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
	devices_array = cd_device_array_new ();
	profiles_array = cd_profile_array_new ();

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

