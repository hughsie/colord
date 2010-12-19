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
 * main:
 **/
int
main (int argc, char *argv[])
{
	const gchar **devices;
	const gchar *object_path;
	//gboolean ret;
	GDBusConnection *connection;
	GError *error = NULL;
	GOptionContext *context;
	guint i;
	guint retval = 1;
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
							G_VARIANT_TYPE ("(as)"),
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
		devices = g_variant_get_strv (response_child, NULL);
		for (i=0; devices[i] != NULL; i++) {
			g_print ("%i.\t%s\n", i+1, devices[i]);
		}

	} else if (g_strcmp0 (argv[1], "get-profiles") == 0) {

		/* execute sync method */
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							COLORD_DBUS_PATH,
							COLORD_DBUS_INTERFACE,
							"GetProfiles",
							NULL,
							G_VARIANT_TYPE ("(as)"),
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
		devices = g_variant_get_strv (response_child, NULL);
		for (i=0; devices[i] != NULL; i++) {
			g_print ("%i.\t%s\n", i+1, devices[i]);
		}

	} else if (g_strcmp0 (argv[1], "create-device") == 0) {

		if (argc < 3) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							COLORD_DBUS_PATH,
							COLORD_DBUS_INTERFACE,
							"CreateDevice",
							g_variant_new ("(s)", argv[2]),
							G_VARIANT_TYPE ("(s)"),
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

	} else if (g_strcmp0 (argv[1], "create-profile") == 0) {

		if (argc < 3) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		response = g_dbus_connection_call_sync (connection,
							COLORD_DBUS_SERVICE,
							COLORD_DBUS_PATH,
							COLORD_DBUS_INTERFACE,
							"CreateProfile",
							g_variant_new ("(s)", argv[2]),
							G_VARIANT_TYPE ("(s)"),
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
							g_variant_new ("(s)", argv[3]),
							NULL,
							G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);
		if (response == NULL) {
			/* TRANSLATORS: the DBus method failed */
			g_print ("%s %s\n", _("The request failed:"), error->message);
			g_error_free (error);
			goto out;
		}

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

