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

/* async state for the sensor readings */
typedef struct {
	gboolean		 ret;
	CdColorXYZ		*sample;
	GSimpleAsyncResult	*res;
	CdSensor		*sensor;
	guint			 exit_id;
	guint			 stdout_id;
	guint			 timeout_id;
} CdSensorAsyncState;

static void
cd_sensor_get_sample_state_finish (CdSensorAsyncState *state,
				   const GError *error)
{
	CdSensorArgyllPrivate *priv = cd_sensor_argyll_get_private (state->sensor);

	if (state->ret) {
		g_simple_async_result_set_op_res_gpointer (state->res,
							   state->sample,
							   cd_color_xyz_free);
	} else {
		g_simple_async_result_set_from_error (state->res, error);
	}

	/* set state */
	cd_sensor_set_state (state->sensor, CD_SENSOR_STATE_IDLE);

	/* complete */
	g_simple_async_result_complete_in_idle (state->res);

	/* disconnect handlers */
	g_signal_handler_disconnect (priv->spawn, state->exit_id);
	g_signal_handler_disconnect (priv->spawn, state->stdout_id);
	g_source_remove (state->timeout_id);

	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_slice_free (CdSensorAsyncState, state);
}

static gboolean
cd_sensor_get_sample_timeout_cb (CdSensorAsyncState *state)
{
	GError *error;
	error = g_error_new (CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_INTERNAL,
			     "spotread timed out");
	cd_sensor_get_sample_state_finish (state, error);
	g_error_free (error);
	return FALSE;
}

static void
cd_sensor_get_sample_exit_cb (CdSpawn *spawn,
			      CdSpawnExitType exit_type,
			      CdSensorAsyncState *state)
{
	GError *error;
	error = g_error_new (CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_INTERNAL,
			     "spotread exited unexpectedly");
	cd_sensor_get_sample_state_finish (state, error);
	g_error_free (error);
}

static void
cd_sensor_get_sample_stdout_cb (CdSpawn *spawn, const gchar *line, CdSensorAsyncState *state)
{
	CdSensorArgyllPrivate *priv = cd_sensor_argyll_get_private (state->sensor);
	gchar **parts = NULL;
	GError *error;

	g_debug ("line='%s'", line);

	/* ready to go, no measurement */
	if (g_str_has_prefix (line, "Place instrument on spot to be measured")) {
		if (priv->pos_required == CD_SENSOR_ARGYLL_POS_UNKNOWN)
			cd_spawn_send_stdin (spawn, "");
		goto out;
	}

	/* got calibration */
	if (g_strcmp0 (line, "Calibration complete") == 0) {
		priv->pos_required = CD_SENSOR_ARGYLL_POS_UNKNOWN;
		goto out;
	}

	/* got measurement */
	if (g_str_has_prefix (line, " Result is XYZ:")) {
		parts = g_strsplit_set (line, " ,", -1);
		state->ret = TRUE;
		state->sample = cd_color_xyz_new ();
		state->sample->X = atof (parts[4]);
		state->sample->Y = atof (parts[5]);
		state->sample->Z = atof (parts[6]);
		cd_sensor_get_sample_state_finish (state, NULL);
		goto out;
	}

	/* failed */
	if (g_str_has_prefix (line, "Instrument initialisation failed")) {
		error = g_error_new (CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_INTERNAL,
				     "failed to contact hardware (replug)");
		cd_sensor_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* need surface */
	if (g_strcmp0 (line, "(Sensor should be in surface position)") == 0) {
		error = g_error_new (CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_REQUIRED_POSITION_SURFACE,
				     "Move to surface position");
		cd_sensor_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* need calibrate */
	if (g_str_has_prefix (line, "Set instrument sensor to calibration position,")) {

		/* just try to read; argyllcms doesn't detect the
		 * sensor position before it asks the user to move the dial... */
		if (priv->pos_required == CD_SENSOR_ARGYLL_POS_UNKNOWN) {
			cd_spawn_send_stdin (spawn, "");
			priv->pos_required = CD_SENSOR_ARGYLL_POS_CALIBRATE;
			goto out;
		}
		error = g_error_new (CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_REQUIRED_POSITION_CALIBRATE,
				     "Move to calibration position");
		cd_sensor_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}
out:
	g_strfreev (parts);
	return;
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
	CdSensorAsyncState *state;
	const gchar *envp[] = { "ARGYLL_NOT_INTERACTIVE=1", NULL };
	gboolean ret;
	GError *error = NULL;
	GPtrArray *argv = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* save state */
	state = g_slice_new0 (CdSensorAsyncState);
	state->res = g_simple_async_result_new (G_OBJECT (sensor),
						callback,
						user_data,
						cd_sensor_get_sample_async);
	state->sensor = g_object_ref (sensor);

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_MEASURING);

	/* connect before spotread produces values */
	state->exit_id = g_signal_connect (priv->spawn,
					   "exit",
					   G_CALLBACK (cd_sensor_get_sample_exit_cb),
					   state);
	state->stdout_id = g_signal_connect (priv->spawn,
					     "stdout",
					     G_CALLBACK (cd_sensor_get_sample_stdout_cb),
					     state);

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
			cd_sensor_get_sample_state_finish (state, error);
			g_error_free (error);
			goto out;
		}
	} else {
		cd_spawn_send_stdin (priv->spawn, "");
	}

	/* cover the case where spotread crashes */
	state->timeout_id = g_timeout_add (CD_SENSOR_ARGYLL_MAX_SAMPLE_TIME,
					     (GSourceFunc) cd_sensor_get_sample_timeout_cb,
					     state);
out:
	if (argv != NULL)
		g_ptr_array_unref (argv);
}

CdColorXYZ *
cd_sensor_get_sample_finish (CdSensor *sensor,
			     GAsyncResult *res,
			     GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* failed */
	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	/* grab detail */
	return cd_color_xyz_dup (g_simple_async_result_get_op_res_gpointer (simple));
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
	case CD_SENSOR_KIND_COLORIMTRE_HCFR:
		return "Colorimtre HCFR";
	case CD_SENSOR_KIND_SPYDER2:
		return "ColorVision Spyder2";
	case CD_SENSOR_KIND_SPYDER3:
		return "Datacolor Spyder3";
	case CD_SENSOR_KIND_SPYDER:
		return "Datacolor Spyder4";
	case CD_SENSOR_KIND_HUEY:
		return "GretagMacbeth Huey";
	case CD_SENSOR_KIND_COLORHUG:
		return "Hughski ColorHug";
	case CD_SENSOR_KIND_COLORHUG_SPECTRO:
		return "Hughski ColorHug Spectro";
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
	gchar **lines = NULL;
	gchar *stdout = NULL;
	guint i;
	guint listno = 0;

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
		goto out;

	/* split into lines and search */
	lines = g_strsplit (stdout, "\n", -1);
	argyll_name = cd_sensor_to_argyll_name (sensor);
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
		ret = FALSE;
		g_set_error (error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_INTERNAL,
			     "Failed to find device %s",
			     argyll_name);
		goto out;
	}
out:
	g_strfreev (lines);
	g_free (stdout);
	return ret;
}

static void
cd_sensor_unlock_state_finish (CdSensorAsyncState *state,
			       const GError *error)
{
	CdSensorArgyllPrivate *priv = cd_sensor_argyll_get_private (state->sensor);

	if (state->ret) {
		g_simple_async_result_set_op_res_gboolean (state->res,
							   state->ret);
	} else {
		g_simple_async_result_set_from_error (state->res, error);
	}

	/* set state */
	cd_sensor_set_state (state->sensor, CD_SENSOR_STATE_IDLE);

	/* complete */
	g_simple_async_result_complete_in_idle (state->res);

	/* disconnect handlers */
	if (state->exit_id != 0)
		g_signal_handler_disconnect (priv->spawn, state->exit_id);
	if (state->timeout_id != 0)
		g_source_remove (state->timeout_id);

	/* this is no longer valid (put in ::Lock() also?) */
	priv->pos_required = CD_SENSOR_ARGYLL_POS_UNKNOWN;

	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_slice_free (CdSensorAsyncState, state);
}

static void
cd_sensor_unlock_exit_cb (CdSpawn *spawn,
			  CdSpawnExitType exit_type,
			  CdSensorAsyncState *state)
{
	GError *error;

	if (exit_type == CD_SPAWN_EXIT_TYPE_SIGQUIT) {
		state->ret = TRUE;
		cd_sensor_unlock_state_finish (state, NULL);
	} else {
		error = g_error_new (CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_INTERNAL,
				     "exited without sigquit");
		cd_sensor_unlock_state_finish (state, error);
		g_error_free (error);
	}
}

void
cd_sensor_unlock_async (CdSensor *sensor,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data)
{
	CdSensorArgyllPrivate *priv = cd_sensor_argyll_get_private (sensor);
	CdSensorAsyncState *state;
	gboolean ret;
	GError *error = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* save state */
	state = g_slice_new0 (CdSensorAsyncState);
	state->res = g_simple_async_result_new (G_OBJECT (sensor),
						callback,
						user_data,
						cd_sensor_unlock_async);
	state->sensor = g_object_ref (sensor);

	/* wait for exit */
	state->exit_id = g_signal_connect (priv->spawn,
					   "exit",
					   G_CALLBACK (cd_sensor_unlock_exit_cb),
					   state);
	/* kill spotread */
	ret = cd_spawn_kill (priv->spawn);
	if (!ret) {
		g_set_error (&error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_INTERNAL,
			     "failed to kill spotread");
		cd_sensor_unlock_state_finish (state, error);
		g_error_free (error);
		goto out;
	}
out:
	return;
}

gboolean
cd_sensor_coldplug (CdSensor *sensor, GError **error)
{
	gboolean ret;
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
	ret = cd_sensor_find_device_details (sensor, error);
	if (!ret)
		goto out;
out:
	return ret;
}
