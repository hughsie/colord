/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

#include "cd-sensor.h"

typedef struct
{
	gboolean			 done_startup;
} CdSensorDummyPrivate;

/* async state for the sensor readings */
typedef struct {
	gboolean		 ret;
	CdColorXYZ		*sample;
	GSimpleAsyncResult	*res;
	CdSensor		*sensor;
} CdSensorAsyncState;

static void
cd_sensor_get_sample_state_finish (CdSensorAsyncState *state,
				   const GError *error)
{
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

	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_slice_free (CdSensorAsyncState, state);
}

static gboolean
cd_sensor_get_ambient_wait_cb (CdSensorAsyncState *state)
{
	state->ret = TRUE;
	state->sample = cd_color_xyz_new ();
	state->sample->X = 7.7f;
	state->sample->Y = CD_SENSOR_NO_VALUE;
	state->sample->Z = CD_SENSOR_NO_VALUE;

	/* just return without a problem */
	cd_sensor_get_sample_state_finish (state, NULL);
	return FALSE;
}

static gboolean
cd_sensor_get_sample_wait_cb (CdSensorAsyncState *state)
{
	state->ret = TRUE;
	state->sample = cd_color_xyz_new ();
	state->sample->X = 0.1;
	state->sample->Y = 0.2;
	state->sample->Z = 0.3;

	/* emulate */
	cd_sensor_button_pressed (state->sensor);

	/* just return without a problem */
	cd_sensor_get_sample_state_finish (state, NULL);
	return FALSE;
}

void
cd_sensor_get_sample_async (CdSensor *sensor,
			    CdSensorCap cap,
			    GCancellable *cancellable,
			    GAsyncReadyCallback callback,
			    gpointer user_data)
{
	CdSensorAsyncState *state;

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

	/* just complete in idle */
	if (cap != CD_SENSOR_CAP_AMBIENT)
		g_timeout_add_seconds (2, (GSourceFunc) cd_sensor_get_sample_wait_cb, state);
	else
		g_timeout_add_seconds (2, (GSourceFunc) cd_sensor_get_ambient_wait_cb, state);
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
cd_sensor_unref_private (CdSensorDummyPrivate *priv)
{
	g_free (priv);
}

gboolean
cd_sensor_coldplug (CdSensor *sensor, GError **error)
{
	CdSensorDummyPrivate *priv;
	guint64 caps = cd_bitfield_from_enums (CD_SENSOR_CAP_LCD,
					       CD_SENSOR_CAP_CRT,
					       CD_SENSOR_CAP_PROJECTOR,
					       CD_SENSOR_CAP_SPOT,
					       CD_SENSOR_CAP_PRINTER,
					       CD_SENSOR_CAP_AMBIENT,
					       -1);
	g_object_set (sensor,
		      "id", "dummy",
		      "kind", CD_SENSOR_KIND_DUMMY,
		      "serial", "0123456789a",
		      "model", "Dummy Sensor #1",
		      "vendor", "Acme Corp",
		      "caps", caps,
		      "native", TRUE,
		      NULL);

	/* create private data */
	priv = g_new0 (CdSensorDummyPrivate, 1);
	g_object_set_data_full (G_OBJECT (sensor), "priv", priv,
				(GDestroyNotify) cd_sensor_unref_private);
	return TRUE;
}

