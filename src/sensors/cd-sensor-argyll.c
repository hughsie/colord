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

/**
 * This object contains all the low level logic for imaginary hardware.
 */

#include "config.h"

#include <glib-object.h>
#include <stdlib.h>

#include "cd-sensor.h"
#include "cd-spawn.h"

#define CD_SENSOR_ARGYLL_MAX_SAMPLE_TIME	10000 /* ms */

typedef enum {
	CD_SENSOR_ARGYLL_POS_UNKNOWN,
	CD_SENSOR_ARGYLL_POS_CALIBRATE,
	CD_SENSOR_ARGYLL_POS_LAST
} CdSensorArgyllPos;

typedef struct {
	gboolean			 done_startup;
	CdSpawn				*spawn;
	guint				 communication_port;
	CdSensorArgyllPos		 pos_required;
} CdSensorArgyllPrivate;

static CdSensorArgyllPrivate *
cd_sensor_argyll_get_private (CdSensor *sensor)
{
	return g_object_get_data (G_OBJECT (sensor), "priv");
}

/* async task for the sensor readings */
typedef struct {
	gboolean		 ret;
	CdColorXYZ		*sample;
	CdSensor		*sensor;
	guint			 exit_id;
	guint			 stdout_id;
	guint			 timeout_id;
} CdSensorTaskData;

static void
cd_sensor_task_data_free (CdSensorTaskData *data)
{
	CdSensorArgyllPrivate *priv = cd_sensor_argyll_get_private (data->sensor);

	/* disconnect handlers */
	if (data->exit_id > 0)
		g_signal_handler_disconnect (priv->spawn, data->exit_id);
	if (data->stdout_id > 0)
		g_signal_handler_disconnect (priv->spawn, data->stdout_id);
	if (data->timeout_id > 0)
		g_source_remove (data->timeout_id);
	g_object_unref (data->sensor);
	g_free (data);
}

static gboolean
cd_sensor_get_sample_timeout_cb (GTask *task)
{
	g_task_return_new_error (task,
				 CD_SENSOR_ERROR,
				 CD_SENSOR_ERROR_INTERNAL,
				 "spotread timed out");
	g_object_unref (task);
	return G_SOURCE_REMOVE;
}

static void
cd_sensor_get_sample_exit_cb (CdSpawn *spawn,
			      CdSpawnExitType exit_type,
			      GTask *task)
{
	g_task_return_new_error (task,
				 CD_SENSOR_ERROR,
				 CD_SENSOR_ERROR_INTERNAL,
				 "spotread exited unexpectedly");
	g_object_unref (task);
}

static void
cd_sensor_get_sample_stdout_cb (CdSpawn *spawn, const gchar *line, GTask *task)
{
	CdSensorTaskData *data = g_task_get_task_data (task);
	CdSensorArgyllPrivate *priv = cd_sensor_argyll_get_private (data->sensor);
	g_autoptr(GError) error = NULL;
	g_auto(GStrv) parts = NULL;

	g_debug ("line='%s'", line);

	/* ready to go, no measurement */
	if (g_str_has_prefix (line, "Place instrument on spot to be measured")) {
		if (priv->pos_required == CD_SENSOR_ARGYLL_POS_UNKNOWN)
			cd_spawn_send_stdin (spawn, "");
		return;
	}

	/* got calibration */
	if (g_strcmp0 (line, "Calibration complete") == 0) {
		priv->pos_required = CD_SENSOR_ARGYLL_POS_UNKNOWN;
		return;
	}

	/* got measurement */
	if (g_str_has_prefix (line, " Result is XYZ:")) {
		CdColorXYZ *sample;
		parts = g_strsplit_set (line, " ,", -1);
		sample = cd_color_xyz_new ();
		sample->X = atof (parts[4]);
		sample->Y = atof (parts[5]);
		sample->Z = atof (parts[6]);
		g_task_return_pointer (task, sample, (GDestroyNotify) cd_color_xyz_free);
		g_object_unref (task);
		return;
	}

	/* failed */
	if (g_str_has_prefix (line, "Instrument initialisation failed")) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "failed to contact hardware (replug)");
		g_object_unref (task);
		return;
	}

	/* need surface */
	if (g_strcmp0 (line, "(Sensor should be in surface position)") == 0) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_REQUIRED_POSITION_SURFACE,
					 "Move to surface position");
		g_object_unref (task);
		return;
	}

	/* need calibrate */
	if (g_str_has_prefix (line, "Set instrument sensor to calibration position,")) {

		/* just try to read; argyllcms doesn't detect the
		 * sensor position before it asks the user to move the dial... */
		if (priv->pos_required == CD_SENSOR_ARGYLL_POS_UNKNOWN) {
			cd_spawn_send_stdin (spawn, "");
			priv->pos_required = CD_SENSOR_ARGYLL_POS_CALIBRATE;
			return;
		}
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_REQUIRED_POSITION_CALIBRATE,
					 "Move to calibration position");
		g_object_unref (task);
		return;
	}
}

static const gchar *
cd_sensor_get_y_arg_for_cap (CdSensorCap cap)
{
	const gchar *arg = NULL;

	switch (cap) {
	case CD_SENSOR_CAP_LCD:
	case CD_SENSOR_CAP_LED:
		arg = "-yl";
		break;
	case CD_SENSOR_CAP_CRT:
	case CD_SENSOR_CAP_PLASMA:
		arg = "-yc";
		break;
	case CD_SENSOR_CAP_PROJECTOR:
		arg = "-yp";
		break;
	case CD_SENSOR_CAP_LCD_CCFL:
		arg = "-yf";
		break;
	case CD_SENSOR_CAP_LCD_RGB_LED:
		arg = "-yb";
		break;
	case CD_SENSOR_CAP_WIDE_GAMUT_LCD_CCFL:
		arg = "-yL";
		break;
	case CD_SENSOR_CAP_WIDE_GAMUT_LCD_RGB_LED:
		arg = "-yB";
		break;
	case CD_SENSOR_CAP_LCD_WHITE_LED:
		arg = "-ye";
		break;
	default:
		break;
	}
	return arg;
}

void
cd_sensor_get_sample_async (CdSensor *sensor,
			    CdSensorCap cap,
			    GCancellable *cancellable,
			    GAsyncReadyCallback callback,
			    gpointer user_data)
{
	CdSensorArgyllPrivate *priv = cd_sensor_argyll_get_private (sensor);
	CdSensorTaskData *data;
	GTask *task = NULL;
	const gchar *envp[] = { "ARGYLL_NOT_INTERACTIVE=1", NULL };
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) argv = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	task = g_task_new (sensor, cancellable, callback, user_data);

	/* set state */
	data = g_new0 (CdSensorTaskData, 1);
	data->sensor = g_object_ref (sensor);
	g_task_set_task_data (task, data, (GDestroyNotify) cd_sensor_task_data_free);

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_MEASURING);

	/* connect before spotread produces values */
	data->exit_id = g_signal_connect (priv->spawn,
					  "exit",
					  G_CALLBACK (cd_sensor_get_sample_exit_cb),
					  task);
	data->stdout_id = g_signal_connect (priv->spawn,
					    "stdout",
					    G_CALLBACK (cd_sensor_get_sample_stdout_cb),
					    task);

	/* if spotread is not already running then execute */
	if (!cd_spawn_is_running (priv->spawn)) {
		argv = g_ptr_array_new_with_free_func (g_free);
		g_ptr_array_add (argv, g_strdup ("/usr/bin/spotread"));
		g_ptr_array_add (argv, g_strdup ("-d"));
		g_ptr_array_add (argv, g_strdup_printf ("-c%i", priv->communication_port));
		g_ptr_array_add (argv, g_strdup ("-N")); //no autocal
		g_ptr_array_add (argv, g_strdup (cd_sensor_get_y_arg_for_cap (cap)));
		g_ptr_array_add (argv, NULL);
		ret = cd_spawn_argv (priv->spawn,
				     (gchar **) argv->pdata,
				     (gchar **) envp,
				     &error);
		if (!ret) {
			g_task_return_new_error (task,
						 CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_INTERNAL,
						 "%s", error->message);
			return;
		}
	} else {
		cd_spawn_send_stdin (priv->spawn, "");
	}

	/* cover the case where spotread crashes */
	data->timeout_id = g_timeout_add (CD_SENSOR_ARGYLL_MAX_SAMPLE_TIME,
					  (GSourceFunc) cd_sensor_get_sample_timeout_cb,
					  task);
}

CdColorXYZ *
cd_sensor_get_sample_finish (CdSensor *sensor, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_sensor_unref_private (CdSensorArgyllPrivate *priv)
{
	g_object_unref (priv->spawn);
	g_free (priv);
}

static const gchar *
cd_sensor_to_argyll_name (CdSensor *sensor)
{
	switch (cd_sensor_get_kind (sensor)) {
	case CD_SENSOR_KIND_DTP20:
		return "Xrite DTP20";
	case CD_SENSOR_KIND_DTP22:
		return "Xrite DTP22";
	case CD_SENSOR_KIND_DTP41:
		return "Xrite DTP41";
	case CD_SENSOR_KIND_DTP51:
		return "Xrite DTP51";
	case CD_SENSOR_KIND_DTP92:
		return "Xrite DTP92";
	case CD_SENSOR_KIND_DTP94:
		return "Xrite DTP94";
	case CD_SENSOR_KIND_SPECTRO_SCAN:
		return "GretagMacbeth SpectroScan";
	case CD_SENSOR_KIND_I1_DISPLAY1:
		return "GretagMacbeth i1 Display 1";
	case CD_SENSOR_KIND_I1_DISPLAY2:
		return "GretagMacbeth i1 Display 2";
	case CD_SENSOR_KIND_I1_DISPLAY3:
		return "Xrite i1 DisplayPro, ColorMunki Display";
	case CD_SENSOR_KIND_I1_MONITOR:
		return "GretagMacbeth i1 Monitor";
	case CD_SENSOR_KIND_I1_PRO:
		return "GretagMacbeth i1 Pro";
	case CD_SENSOR_KIND_COLOR_MUNKI_PHOTO:
		return "X-Rite ColorMunki";
	case CD_SENSOR_KIND_COLOR_MUNKI_SMILE:
		return "ColorMunki Smile";
	case CD_SENSOR_KIND_COLORIMTRE_HCFR:
		return "Colorimtre HCFR";
	case CD_SENSOR_KIND_SPYDER2:
		return "ColorVision Spyder2";
	case CD_SENSOR_KIND_SPYDER3:
		return "Datacolor Spyder3";
	case CD_SENSOR_KIND_SPYDER:
		return "Datacolor Spyder4";
	case CD_SENSOR_KIND_SPYDER5:
		return "Datacolor Spyder5";
	case CD_SENSOR_KIND_HUEY:
		return "GretagMacbeth Huey";
	case CD_SENSOR_KIND_COLORHUG:
		return "Hughski ColorHug";
	case CD_SENSOR_KIND_COLORHUG2:
		return "Hughski ColorHug2";
	case CD_SENSOR_KIND_COLORHUG_PLUS:
		return "Hughski ColorHug+";
	default:
		break;
	}
	return NULL;
}

static gboolean
cd_sensor_find_device_details (CdSensor *sensor, GError **error)
{
	CdSensorArgyllPrivate *priv = cd_sensor_argyll_get_private (sensor);
	const gchar *argv[] = { "spotread", "--help", NULL };
	const gchar *argyll_name;
	const gchar *envp[] = { "ARGYLL_NOT_INTERACTIVE=1", NULL };
	gboolean ret;
	guint i;
	guint listno = 0;
	g_autofree gchar *stdout = NULL;
	g_auto(GStrv) lines = NULL;

	/* spawn the --help output to parse the comm-port */
	ret = g_spawn_sync (NULL,
			    (gchar **) argv,
			    (gchar **) envp,
			    G_SPAWN_SEARCH_PATH,
			    NULL,
			    NULL,
			    NULL,
			    &stdout,
			    NULL,
			    error);
	if (!ret)
		return FALSE;

	/* split into lines and search */
	lines = g_strsplit (stdout, "\n", -1);
	argyll_name = cd_sensor_to_argyll_name (sensor);
	if (argyll_name == NULL) {
		g_set_error_literal (error,
				     CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_INTERNAL,
				     "Failed to find sensor");
		return FALSE;
	}
	for (i = 0; lines[i] != NULL; i++) {

		/* look for the communication port listing of the
		 * device type we have plugged in */
		if (g_strstr_len (lines[i], -1, " = ") != NULL) {
			listno++;
			if (g_strstr_len (lines[i], -1, argyll_name) != NULL) {
				priv->communication_port = listno;
				break;
			}
		}
	}

	/* did we find the right device */
	if (priv->communication_port == 0) {
		g_set_error (error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_INTERNAL,
			     "Failed to find device %s",
			     argyll_name);
		return FALSE;
	}
	return TRUE;
}

static void
cd_sensor_unlock_exit_cb (CdSpawn *spawn,
			  CdSpawnExitType exit_type,
			  GTask *task)
{
	if (exit_type != CD_SPAWN_EXIT_TYPE_SIGQUIT) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "exited without sigquit");
		g_object_unref (task);
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

void
cd_sensor_unlock_async (CdSensor *sensor,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data)
{
	CdSensorArgyllPrivate *priv = cd_sensor_argyll_get_private (sensor);
	CdSensorTaskData *data;
	GTask *task = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	task = g_task_new (sensor, cancellable, callback, user_data);

	/* set state */
	data = g_new0 (CdSensorTaskData, 1);
	data->sensor = g_object_ref (sensor);
	g_task_set_task_data (task, data, (GDestroyNotify) cd_sensor_task_data_free);

	/* wait for exit */
	data->exit_id = g_signal_connect (priv->spawn,
					  "exit",
					  G_CALLBACK (cd_sensor_unlock_exit_cb),
					  task);
	/* kill spotread */
	if (!cd_spawn_kill (priv->spawn)) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "failed to kill spotread");
		g_object_unref (task);
		return;
	}
}

gboolean
cd_sensor_coldplug (CdSensor *sensor, GError **error)
{
	CdSensorArgyllPrivate *priv;
	g_object_set (sensor,
		      "native", FALSE,
		      NULL);

	/* create private data */
	priv = g_new0 (CdSensorArgyllPrivate, 1);
	priv->spawn = cd_spawn_new ();
	g_object_set_data_full (G_OBJECT (sensor), "priv", priv,
				(GDestroyNotify) cd_sensor_unref_private);

	/* try to map find the correct communication port */
	return cd_sensor_find_device_details (sensor, error);
}
