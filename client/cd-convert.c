/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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

#include <glib.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <locale.h>

#include "cd-sensor-sync.h"
#include "cd-device-sync.h"
#include "cd-client-sync.h"
#include "cd-it8-utils.h"
#include "cd-sample-window.h"

/**
 * cd_convert_ti3_ti3_to_ccmx:
 **/
static gboolean
cd_convert_ti3_ti3_to_ccmx (const gchar *reference_fn,
			    const gchar *measured_fn,
			    const gchar *device_fn,
			    GError **error)
{
	CdIt8 *it8_ccmx = NULL;
	CdIt8 *it8_measured = NULL;
	CdIt8 *it8_reference = NULL;
	gboolean ret;
	GFile *file_ccmx = NULL;
	GFile *file_measured = NULL;
	GFile *file_reference = NULL;

	/* load reference */
	it8_reference = cd_it8_new ();
	file_reference = g_file_new_for_path (reference_fn);
	ret = cd_it8_load_from_file (it8_reference,
				     file_reference,
				     error);
	if (!ret)
		goto out;

	/* load measured */
	it8_measured = cd_it8_new ();
	file_measured = g_file_new_for_path (measured_fn);
	ret = cd_it8_load_from_file (it8_measured,
				     file_measured,
				     error);
	if (!ret)
		goto out;

	/* calculate calibration matrix */
	it8_ccmx = cd_it8_new_with_kind (CD_IT8_KIND_CCMX);
	ret = cd_it8_utils_calculate_ccmx (it8_reference,
					   it8_measured,
					   it8_ccmx,
					   error);
	if (!ret)
		goto out;

	/* save file */
	file_ccmx = g_file_new_for_path (device_fn);
	cd_it8_set_title (it8_ccmx, "Factory Calibration");
	cd_it8_set_originator (it8_ccmx, "cd-convert");
	cd_it8_add_option (it8_ccmx, "TYPE_FACTORY");
	ret = cd_it8_save_to_file (it8_ccmx, file_ccmx, error);
	if (!ret)
		goto out;
out:
	if (file_reference != NULL)
		g_object_unref (file_reference);
	if (file_measured != NULL)
		g_object_unref (file_measured);
	if (file_ccmx != NULL)
		g_object_unref (file_ccmx);
	if (it8_reference != NULL)
		g_object_unref (it8_reference);
	if (it8_measured != NULL)
		g_object_unref (it8_measured);
	if (it8_ccmx != NULL)
		g_object_unref (it8_ccmx);
	return ret;
}

/**
 * cd_convert_setup_sensor:
 **/
static CdSensor *
cd_convert_setup_sensor (CdClient *client,
			 GError **error)
{
	CdSensor *sensor = NULL;
	CdSensor *sensor_tmp;
	gboolean ret;
	GPtrArray *sensors = NULL;

	/* get sensor */
	sensors = cd_client_get_sensors_sync (client, NULL, error);
	if (sensors == NULL) {
		ret = FALSE;
		goto out;
	}
	if (sensors->len == 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "No native sensors plugged in!");
		goto out;
	}
	sensor_tmp = g_ptr_array_index (sensors, 0);
	ret = cd_sensor_connect_sync (sensor_tmp, NULL, error);
	if (!ret)
		goto out;
	sensor = g_object_ref (sensor_tmp);
out:
	if (sensors != NULL)
		g_ptr_array_unref (sensors);
	return sensor;
}

/**
 * cd_convert_idle_delay_cb:
 **/
static gboolean
cd_convert_idle_delay_cb (gpointer user_data)
{
	GMainLoop *loop = (GMainLoop *) user_data;
	g_main_loop_quit (loop);
	return FALSE;
}

/**
 * cd_convert_idle_delay:
 **/
static void
cd_convert_idle_delay (guint ms)
{
	GMainLoop *loop;
	loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add (ms, cd_convert_idle_delay_cb, loop);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
}

/**
 * cd_convert_ti1_to_ti3:
 **/
static gboolean
cd_convert_ti1_to_ti3 (const gchar *patches_fn,
		       const gchar *measured_fn,
		       CdSensor *sensor,
		       GError **error)
{
	CdColorRGB rgb;
	CdColorXYZ *xyz_tmp;
	CdIt8 *it8_measured = NULL;
	CdIt8 *it8_patches = NULL;
	gboolean ret;
	GFile *file_measured = NULL;
	GFile *file_patches = NULL;
	GtkWindow *sample_window = NULL;
	guint i;
	guint size;

	/* load patches */
	it8_patches = cd_it8_new ();
	file_patches = g_file_new_for_path (patches_fn);
	ret = cd_it8_load_from_file (it8_patches,
				     file_patches,
				     error);
	if (!ret)
		goto out;

	/* lock the sensor */
	ret = cd_sensor_lock_sync (sensor,
				   NULL,
				   error);
	if (!ret)
		goto out;

	/* create measurement file */
	sample_window = cd_sample_window_new ();
	gtk_window_present (sample_window);
	it8_measured = cd_it8_new_with_kind (CD_IT8_KIND_TI3);
	size = cd_it8_get_data_size (it8_patches);
	for (i = 0; i < size; i++) {
		cd_it8_get_data_item (it8_patches, i, &rgb, NULL);
		cd_sample_window_set_color (CD_SAMPLE_WINDOW (sample_window), &rgb);
		cd_sample_window_set_fraction (CD_SAMPLE_WINDOW (sample_window),
					       (gdouble) i / (gdouble) size);
		cd_convert_idle_delay (200);

		/* get the sample using the default matrix */
		xyz_tmp = cd_sensor_get_sample_sync (sensor,
						     CD_SENSOR_CAP_LCD,
						     NULL,
						     error);
		if (xyz_tmp == NULL) {
			ret = FALSE;
			goto out;
		}

		/* add to measured sheet */
		cd_it8_add_data (it8_measured, &rgb, xyz_tmp);
		cd_color_xyz_free (xyz_tmp);
	}

	/* unlock the sensor */
	ret = cd_sensor_unlock_sync (sensor,
				     NULL,
				     error);
	if (!ret)
		goto out;

	/* save file */
	file_measured = g_file_new_for_path (measured_fn);
	cd_it8_set_title (it8_measured, "Calibration");
	cd_it8_set_originator (it8_measured, "cd-convert");
	cd_it8_set_instrument (it8_measured, cd_sensor_get_model (sensor));
	ret = cd_it8_save_to_file (it8_measured, file_measured, error);
	if (!ret)
		goto out;
out:
	if (file_patches != NULL)
		g_object_unref (file_patches);
	if (file_measured != NULL)
		g_object_unref (file_measured);
	if (it8_patches != NULL)
		g_object_unref (it8_patches);
	if (it8_measured != NULL)
		g_object_unref (it8_measured);
	return ret;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	CdClient *client = NULL;
	CdDevice *device = NULL;
	CdSensor *sensor = NULL;
	gboolean ret;
	gchar *device_id = NULL;
	GError *error = NULL;
	gint retval = EXIT_FAILURE;
	GOptionContext *context;
	guint xid = 0;

	const GOptionEntry options[] = {
		{ "device", '\0', 0, G_OPTION_ARG_STRING, &device_id,
			/* TRANSLATORS: command line option */
			_("Use this device for profiling"), NULL },
		{ "xid", '\0', 0, G_OPTION_ARG_INT, &xid,
			/* TRANSLATORS: command line option */
			_("Make the window modal to this XID"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	/* TRANSLATORS: just dumps the EDID to disk */
	context = g_option_context_new (_("gcm-dispread"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	ret = g_option_context_parse (context, &argc, &argv, &error);
	if (!ret)
		goto out;

	/* get client */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	if (!ret)
		goto out;

	/* check device */
	if (device_id != NULL) {
		device = cd_client_find_device_sync (client,
						     device_id,
						     NULL,
						     &error);
		if (device == NULL) {
			ret = FALSE;
			goto out;
		}
		ret = cd_device_connect_sync (device,
					      NULL,
					      &error);
		if (!ret)
			goto out;
	}

	/* create a .ccmx from two .ti3 files */
	if (argc == 4 &&
	    g_str_has_suffix (argv[1], ".ti3") &&
	    g_str_has_suffix (argv[2], ".ti3") &&
	    g_str_has_suffix (argv[3], ".ccmx")) {
		ret = cd_convert_ti3_ti3_to_ccmx (argv[1],
						  argv[2],
						  argv[3],
						  &error);
		if (!ret) {
			g_print ("failed to create ccmx: %s", error->message);
			g_error_free (error);
			goto out;
		}
	} else if (argc == 3 &&
		   g_str_has_suffix (argv[1], ".ti1") &&
		   g_str_has_suffix (argv[2], ".ti3")) {

		/* get sensor */
		sensor = cd_convert_setup_sensor (client, &error);
		if (sensor == NULL) {
			ret = FALSE;
			goto out;
		}

		/* mark device to be profiled in colord */
		if (device != NULL) {
			ret = cd_device_profiling_inhibit_sync (device,
								NULL,
								&error);
			if (!ret)
				goto out;
		}

		/* run the samples */
		ret = cd_convert_ti1_to_ti3 (argv[1],
					     argv[2],
					     sensor,
					     &error);
		if (!ret) {
			g_print ("failed to create ti3: %s", error->message);
			g_error_free (error);
			goto out;
		}
	} else {
		ret = FALSE;
		g_set_error_literal (&error, 1, 0,
				     "Specify one of:\n"
				     "patches.ti1 measured.ti3\n"
				     "reference.ti3 measured.ti3 device.ccmx");
		goto out;
	}

	/* success */
	retval = EXIT_SUCCESS;
out:
	if (!ret) {
		g_print ("%s: %s\n",
			 _("Failed to calibrate"),
			 error->message);
		g_error_free (error);
	}
	g_option_context_free (context);
	if (device != NULL)
		g_object_unref (device);
	if (client != NULL)
		g_object_unref (client);
	if (sensor != NULL)
		g_object_unref (sensor);
	g_free (device_id);
	return retval;
}
