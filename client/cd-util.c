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
#include <colord.h>

/**
 * cd_util_show_profile:
 **/
static void
cd_util_show_profile (CdProfile *profile)
{
	const gchar *tmp;
	g_print ("Object Path: %s\n",
		 cd_profile_get_object_path (profile));
	tmp = cd_profile_get_qualifier (profile);
	if (tmp != NULL && tmp[0] != '\0')
		g_print ("Qualifier:\t\t%s\n", tmp);
	g_print ("Filename:\t\t%s\n",
		 cd_profile_get_filename (profile));
	g_print ("Profile ID:\t%s\n",
		 cd_profile_get_id (profile));
}

/**
 * cd_util_show_device:
 **/
static void
cd_util_show_device (CdDevice *device)
{
	CdProfile *profile_tmp;
	GPtrArray *profiles;
	guint i;

	g_print ("Object Path: %s\n",
		 cd_device_get_object_path (device));
	g_print ("Created:\t%" G_GUINT64_FORMAT "\n",
		 cd_device_get_created (device));
	g_print ("Kind:\t\t%s\n",
		 cd_device_kind_to_string (cd_device_get_kind (device)));
	g_print ("Model:\t\t%s\n",
		 cd_device_get_model (device));
	g_print ("Device ID:\t%s\n",
		 cd_device_get_id (device));

	/* print profiles */
	profiles = cd_device_get_profiles (device);
	for (i=0; i<profiles->len; i++) {
		profile_tmp = g_ptr_array_index (profiles, i);
		g_print ("Profile %i:\t%s\n",
			 i+1,
			 cd_profile_get_object_path (profile_tmp));
	}
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
	CdClient *client = NULL;
	CdDevice *device = NULL;
	CdDevice *device_tmp;
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;
	gboolean ret;
	GError *error = NULL;
	GOptionContext *context;
	GPtrArray *array = NULL;
	guint i;
	guint mask;
	guint retval = 1;

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

	/* get connection to colord */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	if (!ret) {
		/* TRANSLATORS: no colord available */
		g_print ("%s %s\n", _("No connection to colord:"), error->message);
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
		array = cd_client_get_devices_sync (client, NULL, &error);
		if (array == NULL) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to get devices:"), error->message);
			g_error_free (error);
			goto out;
		}
		for (i=0; i < array->len; i++) {
			device_tmp = g_ptr_array_index (array, i);
			cd_util_show_device (device_tmp);
		}

	} else if (g_strcmp0 (argv[1], "get-devices-by-kind") == 0) {

		if (argc < 2) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		array = cd_client_get_devices_by_kind_sync (client,
				cd_device_kind_from_string (argv[2]),
				NULL,
				&error);
		if (array == NULL) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to get devices:"), error->message);
			g_error_free (error);
			goto out;
		}
		for (i=0; i < array->len; i++) {
			device_tmp = g_ptr_array_index (array, i);
			cd_util_show_device (device_tmp);
		}

	} else if (g_strcmp0 (argv[1], "get-profiles") == 0) {

		/* execute sync method */
		array = cd_client_get_profiles_sync (client, NULL, &error);
		if (array == NULL) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to get profiles:"), error->message);
			g_error_free (error);
			goto out;
		}
		for (i=0; i < array->len; i++) {
			profile_tmp = g_ptr_array_index (array, i);
			cd_util_show_profile (profile_tmp);
		}

	} else if (g_strcmp0 (argv[1], "create-device") == 0) {

		if (argc < 4) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		mask = cd_util_mask_from_string (argv[3]);
		device = cd_client_create_device_sync (client, argv[2],
						       mask, NULL, &error);
		if (device == NULL) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to create device:"), error->message);
			g_error_free (error);
			goto out;
		}
		g_print ("Created device:\n");
		cd_util_show_device (device);

	} else if (g_strcmp0 (argv[1], "find-device") == 0) {

		if (argc < 3) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		device = cd_client_find_device_sync (client, argv[2],
						     NULL, &error);
		if (device == NULL) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to find device:"), error->message);
			g_error_free (error);
			goto out;
		}
		cd_util_show_device (device);

	} else if (g_strcmp0 (argv[1], "find-profile") == 0) {

		if (argc < 3) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		profile = cd_client_find_profile_sync (client, argv[2],
						       NULL, &error);
		if (profile == NULL) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to find profile:"), error->message);
			g_error_free (error);
			goto out;
		}
		cd_util_show_profile (profile);

	} else if (g_strcmp0 (argv[1], "create-profile") == 0) {

		if (argc < 4) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		/* execute sync method */
		mask = cd_util_mask_from_string (argv[3]);
		profile = cd_client_create_profile_sync (client, argv[2],
							 mask, NULL, &error);
		if (profile == NULL) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to create profile:"), error->message);
			g_error_free (error);
			goto out;
		}
		g_print ("Created profile:\n");
		cd_util_show_profile (profile);

	} else if (g_strcmp0 (argv[1], "device-add-profile") == 0) {

		if (argc < 3) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		device = cd_device_new ();
		ret = cd_device_set_object_path_sync (device,
						      argv[2],
						      NULL,
						      &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to create device:"),
				 error->message);
			g_error_free (error);
			goto out;
		}
		profile = cd_profile_new ();
		ret = cd_profile_set_object_path_sync (profile,
						       argv[3],
						       NULL,
						       &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to create profile:"),
				 error->message);
			g_error_free (error);
			goto out;
		}
		ret = cd_device_add_profile_sync (device, profile, NULL, &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to add profile to device:"),
				 error->message);
			g_error_free (error);
			goto out;
		}

	} else if (g_strcmp0 (argv[1], "device-make-profile-default") == 0) {

		if (argc < 3) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		device = cd_device_new ();
		ret = cd_device_set_object_path_sync (device,
						      argv[2],
						      NULL,
						      &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to create device:"),
				 error->message);
			g_error_free (error);
			goto out;
		}
		profile = cd_profile_new ();
		ret = cd_profile_set_object_path_sync (profile,
						       argv[3],
						       NULL,
						       &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to create profile:"),
				 error->message);
			g_error_free (error);
			goto out;
		}
		ret = cd_device_make_profile_default_sync (device, profile,
							   NULL, &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to add profile to device:"),
				 error->message);
			g_error_free (error);
			goto out;
		}

	} else if (g_strcmp0 (argv[1], "delete-device") == 0) {

		if (argc < 2) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		device = cd_device_new ();
		ret = cd_device_set_object_path_sync (device,
						      argv[2],
						      NULL,
						      &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to create device:"),
				 error->message);
			g_error_free (error);
			goto out;
		}
		ret = cd_client_delete_device_sync (client, device,
						    NULL, &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to delete device:"),
				 error->message);
			g_error_free (error);
			goto out;
		}

	} else if (g_strcmp0 (argv[1], "delete-profile") == 0) {

		if (argc < 2) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		profile = cd_profile_new ();
		ret = cd_profile_set_object_path_sync (profile,
						       argv[2],
						       NULL,
						       &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to create profile:"),
				 error->message);
			g_error_free (error);
			goto out;
		}
		ret = cd_client_delete_profile_sync (client, profile,
						     NULL, &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to delete profile:"),
				 error->message);
			g_error_free (error);
			goto out;
		}

	} else if (g_strcmp0 (argv[1], "profile-set-qualifier") == 0) {

		if (argc < 3) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		profile = cd_profile_new ();
		ret = cd_profile_set_object_path_sync (profile,
						       argv[2],
						       NULL,
						       &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to create profile:"),
				 error->message);
			g_error_free (error);
			goto out;
		}
		ret = cd_profile_set_qualifier_sync (profile, argv[3],
						     NULL, &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to set property on profile:"),
				 error->message);
			g_error_free (error);
			goto out;
		}

	} else if (g_strcmp0 (argv[1], "profile-set-filename") == 0) {

		if (argc < 3) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		profile = cd_profile_new ();
		ret = cd_profile_set_object_path_sync (profile,
						       argv[2],
						       NULL,
						       &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to create profile:"),
				 error->message);
			g_error_free (error);
			goto out;
		}
		ret = cd_profile_set_filename_sync (profile, argv[3],
						    NULL, &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to set property on profile:"),
				 error->message);
			g_error_free (error);
			goto out;
		}

	} else if (g_strcmp0 (argv[1], "device-set-model") == 0) {

		if (argc < 3) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		device = cd_device_new ();
		ret = cd_device_set_object_path_sync (device,
						      argv[2],
						      NULL,
						      &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to create device:"),
				 error->message);
			g_error_free (error);
			goto out;
		}
		ret = cd_device_set_model_sync (device, argv[3],
						NULL, &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to set property on device:"),
				 error->message);
			g_error_free (error);
			goto out;
		}

	} else if (g_strcmp0 (argv[1], "device-set-kind") == 0) {

		if (argc < 3) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		device = cd_device_new ();
		ret = cd_device_set_object_path_sync (device,
						      argv[2],
						      NULL,
						      &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to create device:"),
				 error->message);
			g_error_free (error);
			goto out;
		}
		ret = cd_device_set_kind_sync (device, cd_device_kind_from_string (argv[3]),
					       NULL, &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to set property on device:"),
				 error->message);
			g_error_free (error);
			goto out;
		}

	} else if (g_strcmp0 (argv[1], "device-get-profile-for-qualifier") == 0) {

		if (argc < 3) {
			g_print ("Not enough arguments\n");
			goto out;
		}

		device = cd_device_new ();
		ret = cd_device_set_object_path_sync (device,
						      argv[2],
						      NULL,
						      &error);
		if (!ret) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to create device:"),
				 error->message);
			g_error_free (error);
			goto out;
		}
		profile = cd_device_get_profile_for_qualifier_sync (device,
								    argv[3],
								    NULL,
								    &error);
		if (profile == NULL) {
			/* TRANSLATORS: no colord available */
			g_print ("%s %s\n", _("Failed to get a profile for the qualifier:"),
				 error->message);
			g_error_free (error);
			goto out;
		}
		cd_util_show_profile (profile);

	} else {

		g_print ("Command '%s' not known\n", argv[1]);
		goto out;
	}

	/* success */
	retval = 0;
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	if (client != NULL)
		g_object_unref (client);
	if (device != NULL)
		g_object_unref (device);
	if (profile != NULL)
		g_object_unref (profile);
	return retval;
}

