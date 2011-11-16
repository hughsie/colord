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
#include <locale.h>

#include "cd-sensor.h"

static void
cd_sensor_dump_lock_cb (GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	CdSensor *sensor = CD_SENSOR (source_object);
	GMainLoop *loop = (GMainLoop *) user_data;

	ret = cd_sensor_lock_finish (sensor, res, &error);
	if (!ret) {
		g_warning ("failed to lock: %s", error->message);
		g_error_free (error);
	}
	g_main_loop_quit (loop);
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	guint retval = 0;
	gboolean ret;
	GError *error = NULL;
	GString *data = NULL;
	GOptionContext *context;
	CdSensor *sensor;
	gchar *filename = NULL;
	GMainLoop *loop = NULL;

	setlocale (LC_ALL, "");

	g_type_init ();

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new ("sensor dump program");
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* get the default sensor */
	sensor = cd_sensor_new ();
	cd_sensor_set_kind (sensor, CD_SENSOR_KIND_HUEY);
	ret = cd_sensor_load (sensor, &error);
	if (!ret) {
		g_print ("FAILED: Failed to load sensor: %s\n",
			 error->message);
		g_error_free (error);
		goto out;
	}

	/* lock the sensor */
	loop = g_main_loop_new (NULL, FALSE);
	cd_sensor_lock_async (sensor, NULL, cd_sensor_dump_lock_cb, loop);
	g_main_loop_run (loop);

	/* dump details */
	filename = g_strdup ("./sensor-dump.txt");
	g_print ("Dumping sensor details to %s... ", filename);
	data = g_string_new ("");
	ret = cd_sensor_dump (sensor, data, &error);
	if (!ret) {
		g_print ("FAILED: Failed to dump sensor: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* write to file */
	ret = g_file_set_contents (filename, data->str, data->len, &error);
	if (!ret) {
		g_print ("FAILED: Failed to write file: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	g_print ("SUCCESS!!\n");
out:
	g_free (filename);
	if (loop != NULL)
		g_main_loop_unref (loop);
	if (data != NULL)
		g_string_free (data, TRUE);
	g_object_unref (sensor);
	return retval;
}

