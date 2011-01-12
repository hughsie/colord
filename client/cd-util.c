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

#include "cd-common.h"

/**
 * cd_util_show_profile:
 **/
static void
cd_util_show_profile (const gchar *object_path)
{
	const gchar *filename = NULL;
	const gchar *profile_id = NULL;
	const gchar *qualifier = NULL;
	const gchar *title = NULL;
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *variant_filename = NULL;
	GVariant *variant_profile_id = NULL;
	GVariant *variant_qualifier = NULL;
	GVariant *variant_title = NULL;

	g_print ("Object Path: %s\n", object_path);

	/* get proxy */
	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					       G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
					       NULL,
					       COLORD_DBUS_SERVICE,
					       object_path,
					       COLORD_DBUS_INTERFACE_PROFILE,
					       NULL,
					       &error);
	if (proxy == NULL) {
		g_print ("Failed to get profile properties: %s",
			 error->message);
		g_error_free (error);
		goto out;
	}

	/* print profile id */
	variant_profile_id = g_dbus_proxy_get_cached_property (proxy, "ProfileId");
	if (variant_profile_id != NULL)
		profile_id = g_variant_get_string (variant_profile_id, NULL);
	g_print ("ProfileId:\t\t%s\n", profile_id);

	/* print title */
	variant_title = g_dbus_proxy_get_cached_property (proxy, "Title");
	if (variant_title != NULL)
		title = g_variant_get_string (variant_title, NULL);
	g_print ("Title:\t\t%s\n", title);

	/* print qualifier */
	variant_qualifier = g_dbus_proxy_get_cached_property (proxy, "Qualifier");
	if (variant_qualifier != NULL)
		qualifier = g_variant_get_string (variant_qualifier, NULL);
	g_print ("Qualifier:\t\t%s\n", qualifier);

	/* print filename */
	variant_filename = g_dbus_proxy_get_cached_property (proxy, "Filename");
	if (variant_filename != NULL)
		filename = g_variant_get_string (variant_filename, NULL);
	g_print ("Filename:\t\t%s\n", filename);
out:
	if (variant_profile_id != NULL)
		g_variant_unref (variant_profile_id);
	if (variant_title != NULL)
		g_variant_unref (variant_title);
	if (variant_qualifier != NULL)
		g_variant_unref (variant_qualifier);
	if (variant_filename != NULL)
		g_variant_unref (variant_filename);
	if (proxy != NULL)
		g_object_unref (proxy);
}

/**
 * cd_util_show_device:
 **/
static void
cd_util_show_device (const gchar *object_path)
{
	const gchar *device_id;
	const gchar *kind;
	const gchar *model;
	gchar *profile_tmp;
	GDBusProxy *proxy;
	GError *error = NULL;
	gsize len;
	guint64 created;
	guint i;
	GVariantIter iter;
	GVariant *variant_created = NULL;
	GVariant *variant_device_id = NULL;
	GVariant *variant_kind = NULL;
	GVariant *variant_model = NULL;
	GVariant *variant_profiles = NULL;

	g_print ("Object Path: %s\n", object_path);

	/* get proxy */
	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					       G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
					       NULL,
					       COLORD_DBUS_SERVICE,
					       object_path,
					       COLORD_DBUS_INTERFACE_DEVICE,
					       NULL,
					       &error);
	if (proxy == NULL) {
		g_print ("Failed to get device properties: %s",
			 error->message);
		g_error_free (error);
		goto out;
	}

	/* print created date */
	variant_created = g_dbus_proxy_get_cached_property (proxy, "Created");
	created = g_variant_get_uint64 (variant_created);
	g_print ("Created:\t%" G_GUINT64_FORMAT "\n", created);

	/* print kind */
	variant_kind = g_dbus_proxy_get_cached_property (proxy, "Kind");
	if (variant_kind != NULL) {
		kind = g_variant_get_string (variant_kind, NULL);
		g_print ("Kind:\t\t%s\n", kind);
	}

	/* print model */
	variant_model = g_dbus_proxy_get_cached_property (proxy, "Model");
	model = g_variant_get_string (variant_model, NULL);
	g_print ("Model:\t\t%s\n", model);

	/* print device id */
	variant_device_id = g_dbus_proxy_get_cached_property (proxy, "DeviceId");
	device_id = g_variant_get_string (variant_device_id, NULL);
	g_print ("Device ID:\t%s\n", device_id);

	/* print profiles */
	variant_profiles = g_dbus_proxy_get_cached_property (proxy, "Profiles");
	len = g_variant_iter_init (&iter, variant_profiles);
	if (len == 0)
		g_print ("No assigned profiles!\n");
	for (i=0; i<len; i++) {
		g_variant_get_child (variant_profiles, i,
				     "o", &profile_tmp);
		g_print ("Profile %i:\t%s\n", i+1, profile_tmp);
		g_free (profile_tmp);
	}
out:
	if (variant_created != NULL)
		g_variant_unref (variant_created);
	if (variant_model != NULL)
		g_variant_unref (variant_model);
	if (variant_device_id != NULL)
		g_variant_unref (variant_device_id);
	if (variant_profiles != NULL)
		g_variant_unref (variant_profiles);
	if (proxy != NULL)
		g_object_unref (proxy);
}

/**
 * cd_util_mask_from_string:
 **/
static guint
cd_util_mask_from_string (const gchar *value)
{
	if (g_strcmp0 (value, "normal") == 0)
		return CD_DBUS_OPTIONS_MASK_NORMAL;
	if (g_strcmp0 (value, "temp") == 0)
		return CD_DBUS_OPTIONS_MASK_TEMP;
	if (g_strcmp0 (value, "disk") == 0)
		return CD_DBUS_OPTIONS_MASK_DISK;
	g_warning ("mask string '%s' unknown", value);
	return CD_DBUS_OPTIONS_MASK_NORMAL;
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	const gchar *object_path;
	gchar *object_path_tmp = NULL;
	GDBusConnection *connection;
	GError *error = NULL;
	GOptionContext *context;
	gsize len;
	guint i;
	guint mask;
	guint retval = 1;
	GVariantIter iter;
	GVariant *response_child = NULL;
	GVariant *response = NULL;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();

	/* TRANSLATORS: program name */
	g_set_application_name (_("Color Management"));
	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, _("Color Management Utility"));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* get a session bus connection */
	connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
	if (connection == NULL) {
		/* TRANSLATORS: no DBus system bus */
		g_print ("%s %s\n", _("Failed to connect to system bus:"), error->message);
		g_error_free (error);
		goto out;
	}

	if (argc < 2) {
		g_print ("Not enough arguments\n");
		goto out;
	}

	/* find the commands */
	if (g_strcmp0 (argv[1], "get-devices") == 0) {

		/* execute sync method */
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							COLORD_DBUS_PATH,
							COLORD_DBUS_INTERFACE,
							"GetDevices",
							NULL,
							G_VARIANT_TYPE ("(ao)"),
							G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);
		if (response == NULL) {
			/* TRANSLATORS: the DBus method failed */
			g_print ("%s %s\n", _("The request failed:"), error->message);
			g_error_free (error);
			goto out;
		}

		/* print each device */
		response_child = g_variant_get_child_value (response, 0);
		len = g_variant_iter_init (&iter, response_child);
		for (i=0; i < len; i++) {
			g_variant_get_child (response_child, i,
					     "o", &object_path_tmp);
			cd_util_show_device (object_path_tmp);
			g_free (object_path_tmp);
		}

	} else if (g_strcmp0 (argv[1], "get-devices-by-kind") == 0) {

		if (argc < 2) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							COLORD_DBUS_PATH,
							COLORD_DBUS_INTERFACE,
							"GetDevicesByKind",
							g_variant_new ("(s)", argv[2]),
							NULL,
							G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);
		if (response == NULL) {
			/* TRANSLATORS: the DBus method failed */
			g_print ("%s %s\n", _("The request failed:"), error->message);
			g_error_free (error);
			goto out;
		}

		/* print each device */
		response_child = g_variant_get_child_value (response, 0);
		len = g_variant_iter_init (&iter, response_child);
		for (i=0; i < len; i++) {
			g_variant_get_child (response_child, i,
					     "o", &object_path_tmp);
			cd_util_show_device (object_path_tmp);
			g_free (object_path_tmp);
		}

	} else if (g_strcmp0 (argv[1], "get-profiles") == 0) {

		/* execute sync method */
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							COLORD_DBUS_PATH,
							COLORD_DBUS_INTERFACE,
							"GetProfiles",
							NULL,
							G_VARIANT_TYPE ("(ao)"),
							G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);
		if (response == NULL) {
			/* TRANSLATORS: the DBus method failed */
			g_print ("%s %s\n", _("The request failed:"), error->message);
			g_error_free (error);
			goto out;
		}

		/* print each device */
		response_child = g_variant_get_child_value (response, 0);
		len = g_variant_iter_init (&iter, response_child);
		for (i=0; i < len; i++) {
			g_variant_get_child (response_child, i,
					     "o", &object_path_tmp);
			cd_util_show_profile (object_path_tmp);
			g_free (object_path_tmp);
		}

	} else if (g_strcmp0 (argv[1], "create-device") == 0) {

		if (argc < 4) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		mask = cd_util_mask_from_string (argv[3]);
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							COLORD_DBUS_PATH,
							COLORD_DBUS_INTERFACE,
							"CreateDevice",
							g_variant_new ("(su)",
								       argv[2],
								       mask),
							G_VARIANT_TYPE ("(o)"),
							G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);
		if (response == NULL) {
			/* TRANSLATORS: the DBus method failed */
			g_print ("%s %s\n", _("The request failed:"), error->message);
			g_error_free (error);
			goto out;
		}

		/* print the device */
		response_child = g_variant_get_child_value (response, 0);
		object_path = g_variant_get_string (response_child, NULL);
		g_print ("Created device %s\n", object_path);

	} else if (g_strcmp0 (argv[1], "find-device") == 0) {

		if (argc < 3) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							COLORD_DBUS_PATH,
							COLORD_DBUS_INTERFACE,
							"FindDeviceById",
							g_variant_new ("(s)",
								       argv[2]),
							G_VARIANT_TYPE ("(o)"),
							G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);
		if (response == NULL) {
			/* TRANSLATORS: the DBus method failed */
			g_print ("%s %s\n", _("The request failed:"), error->message);
			g_error_free (error);
			goto out;
		}

		/* print the device */
		response_child = g_variant_get_child_value (response, 0);
		object_path = g_variant_get_string (response_child, NULL);
		g_print ("Got device %s\n", object_path);

	} else if (g_strcmp0 (argv[1], "find-profile") == 0) {

		if (argc < 3) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							COLORD_DBUS_PATH,
							COLORD_DBUS_INTERFACE,
							"FindProfileById",
							g_variant_new ("(s)", argv[2]),
							G_VARIANT_TYPE ("(o)"),
							G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);
		if (response == NULL) {
			/* TRANSLATORS: the DBus method failed */
			g_print ("%s %s\n", _("The request failed:"), error->message);
			g_error_free (error);
			goto out;
		}

		/* print the device */
		response_child = g_variant_get_child_value (response, 0);
		object_path = g_variant_get_string (response_child, NULL);
		g_print ("Got profile %s\n", object_path);

	} else if (g_strcmp0 (argv[1], "create-profile") == 0) {

		if (argc < 4) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		mask = cd_util_mask_from_string (argv[3]);
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							COLORD_DBUS_PATH,
							COLORD_DBUS_INTERFACE,
							"CreateProfile",
							g_variant_new ("(su)",
								       argv[2],
								       mask),
							G_VARIANT_TYPE ("(o)"),
							G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);
		if (response == NULL) {
			/* TRANSLATORS: the DBus method failed */
			g_print ("%s %s\n", _("The request failed:"), error->message);
			g_error_free (error);
			goto out;
		}

		/* print the device */
		response_child = g_variant_get_child_value (response, 0);
		object_path = g_variant_get_string (response_child, NULL);
		g_print ("Created profile %s\n", object_path);

	} else if (g_strcmp0 (argv[1], "delete-device") == 0) {

		if (argc < 2) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							COLORD_DBUS_PATH,
							COLORD_DBUS_INTERFACE,
							"DeleteDevice",
							g_variant_new ("(s)", argv[2]),
							NULL,
							G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);
		if (response == NULL) {
			/* TRANSLATORS: the DBus method failed */
			g_print ("%s %s\n", _("The request failed:"), error->message);
			g_error_free (error);
			goto out;
		}

	} else if (g_strcmp0 (argv[1], "device-add-profile") == 0) {

		if (argc < 3) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							argv[2],
							COLORD_DBUS_INTERFACE_DEVICE,
							"AddProfile",
							g_variant_new ("(o)", argv[3]),
							NULL,
							G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);
		if (response == NULL) {
			/* TRANSLATORS: the DBus method failed */
			g_print ("%s %s\n", _("The request failed:"), error->message);
			g_error_free (error);
			goto out;
		}

	} else if (g_strcmp0 (argv[1], "profile-set-property") == 0) {

		if (argc < 5) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							argv[2],
							COLORD_DBUS_INTERFACE_PROFILE,
							"SetProperty",
							g_variant_new ("(ss)",
								       argv[3],
								       argv[4]),
							NULL,
							G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);
		if (response == NULL) {
			/* TRANSLATORS: the DBus method failed */
			g_print ("%s %s\n", _("The request failed:"), error->message);
			g_error_free (error);
			goto out;
		}

	} else if (g_strcmp0 (argv[1], "device-set-property") == 0) {

		if (argc < 5) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							argv[2],
							COLORD_DBUS_INTERFACE_DEVICE,
							"SetProperty",
							g_variant_new ("(ss)",
								       argv[3],
								       argv[4]),
							NULL,
							G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);
		if (response == NULL) {
			/* TRANSLATORS: the DBus method failed */
			g_print ("%s %s\n", _("The request failed:"), error->message);
			g_error_free (error);
			goto out;
		}

	} else if (g_strcmp0 (argv[1], "device-get-profile-for-qualifier") == 0) {

		if (argc < 4) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							argv[2],
							COLORD_DBUS_INTERFACE_DEVICE,
							"GetProfileForQualifier",
							g_variant_new ("(s)", argv[3]),
							G_VARIANT_TYPE ("(o)"),
							G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);
		if (response == NULL) {
			/* TRANSLATORS: the DBus method failed */
			g_print ("%s %s\n", _("The request failed:"), error->message);
			g_error_free (error);
			goto out;
		}

		/* print the device */
		response_child = g_variant_get_child_value (response, 0);
		object_path = g_variant_get_string (response_child, NULL);
		cd_util_show_profile (object_path);

	} else {

		g_print ("Command '%s' not known\n", argv[1]);
		goto out;
	}

	/* success */
	retval = 0;
out:
	if (response != NULL)
		g_variant_unref (response);
	return retval;
}

