/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:cd-sensor-dummy
 * @short_description: Userspace driver for a dummy sensor.
 *
 * This object contains all the low level logic for imaginary hardware.
 */

#include "config.h"

#include <glib-object.h>

#include "cd-sensor-dummy.h"

G_DEFINE_TYPE (CdSensorDummy, cd_sensor_dummy, CD_TYPE_SENSOR)

#define CD_SENSOR_HUEY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_SENSOR_DUMMY, CdSensorHueyPrivate))

/**
 * CdSensorHueyPrivate:
 *
 * Private #CdSensorHuey data
 **/
struct _CdSensorHueyPrivate
{
	gboolean			 done_startup;
};

/* async state for the sensor readings */
typedef struct {
	gboolean		 ret;
	CdSensorSample		*sample;
	GSimpleAsyncResult	*res;
	CdSensor		*sensor;
} CdSensorAsyncState;

/**
 * cd_sensor_copy_sample:
 **/
static void
cd_sensor_copy_sample (const CdSensorSample *source,
		       CdSensorSample *result)
{
	result->X = source->X;
	result->Y = source->Y;
	result->Z = source->Z;
	result->luminance = source->luminance;
}

/**
 * cd_sensor_dummy_get_sample_state_finish:
 **/
static void
cd_sensor_dummy_get_sample_state_finish (CdSensorAsyncState *state,
					 const GError *error)
{
	CdSensorSample *result;

	/* set result to temp memory location */
	if (state->ret) {
		result = g_new0 (CdSensorSample, 1);
		cd_sensor_copy_sample (state->sample, result);
		g_simple_async_result_set_op_res_gpointer (state->res, result, (GDestroyNotify) g_free);
	} else {
		g_simple_async_result_set_from_error (state->res, error);
	}

	/* set state */
	cd_sensor_set_state (state->sensor, CD_SENSOR_STATE_IDLE);

	/* complete */
	g_simple_async_result_complete_in_idle (state->res);

	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_free (state->sample);
	g_slice_free (CdSensorAsyncState, state);
}

/**
 * cd_sensor_dummy_get_ambient_wait_cb:
 **/
static gboolean
cd_sensor_dummy_get_ambient_wait_cb (CdSensorAsyncState *state)
{
	state->ret = TRUE;
	state->sample->X = CD_SENSOR_NO_VALUE;
	state->sample->Y = CD_SENSOR_NO_VALUE;
	state->sample->Z = CD_SENSOR_NO_VALUE;
	state->sample->luminance = 7.7f;

	/* just return without a problem */
	cd_sensor_dummy_get_sample_state_finish (state, NULL);
	return FALSE;
}

/**
 * cd_sensor_dummy_get_sample_wait_cb:
 **/
static gboolean
cd_sensor_dummy_get_sample_wait_cb (CdSensorAsyncState *state)
{
	state->ret = TRUE;
	state->sample->X = 0.1;
	state->sample->Y = 0.2;
	state->sample->Z = 0.3;
	state->sample->luminance = CD_SENSOR_NO_VALUE;

	/* emulate */
	cd_sensor_button_pressed (state->sensor);

	/* just return without a problem */
	cd_sensor_dummy_get_sample_state_finish (state, NULL);
	return FALSE;
}

/**
 * cd_sensor_dummy_get_sample_async:
 **/
static void
cd_sensor_dummy_get_sample_async (CdSensor *sensor,
				  CdSensorCap cap,
				  GCancellable *cancellable,
				  GAsyncResult *res)
{
	CdSensorAsyncState *state;

	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_return_if_fail (res != NULL);

	/* save state */
	state = g_slice_new0 (CdSensorAsyncState);
	state->res = g_object_ref (res);
	state->sensor = g_object_ref (sensor);
	state->sample = g_new0 (CdSensorSample, 1);

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_MEASURING);

	/* just complete in idle */
	if (cap == CD_SENSOR_CAP_LCD ||
	    cap == CD_SENSOR_CAP_CRT ||
	    cap == CD_SENSOR_CAP_PROJECTOR)
		g_timeout_add_seconds (2, (GSourceFunc) cd_sensor_dummy_get_sample_wait_cb, state);
	else
		g_timeout_add_seconds (2, (GSourceFunc) cd_sensor_dummy_get_ambient_wait_cb, state);
}

/**
 * cd_sensor_dummy_get_sample_finish:
 **/
static gboolean
cd_sensor_dummy_get_sample_finish (CdSensor *sensor,
				   GAsyncResult *res,
				   CdSensorSample *value,
				   GError **error)
{
	GSimpleAsyncResult *simple;
	gboolean ret = TRUE;
	CdSensorSample *sample;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* failed */
	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error)) {
		ret = FALSE;
		goto out;
	}

	/* grab detail */
	sample = (CdSensorSample*) g_simple_async_result_get_op_res_gpointer (simple);
	cd_sensor_copy_sample (sample, value);
out:
	return ret;
}

/**
 * cd_sensor_dummy_class_init:
 **/
static void
cd_sensor_dummy_class_init (CdSensorDummyClass *klass)
{
	CdSensorClass *parent_class = CD_SENSOR_CLASS (klass);

	/* setup klass links */
	parent_class->get_sample_async = cd_sensor_dummy_get_sample_async;
	parent_class->get_sample_finish = cd_sensor_dummy_get_sample_finish;

	g_type_class_add_private (klass, sizeof (CdSensorHueyPrivate));
}

/**
 * cd_sensor_dummy_init:
 **/
static void
cd_sensor_dummy_init (CdSensorDummy *sensor)
{
	sensor->priv = CD_SENSOR_HUEY_GET_PRIVATE (sensor);
}

/**
 * cd_sensor_dummy_new:
 *
 * Return value: a new #CdSensor object.
 **/
CdSensor *
cd_sensor_dummy_new (void)
{
	CdSensorDummy *sensor;
	const gchar *caps[] = { "lcd",
				"crt",
				"projector",
				"spot",
				"printer",
				"ambient",
				NULL };
	sensor = g_object_new (CD_TYPE_SENSOR_DUMMY,
			       "id", "dummy",
			       "kind", CD_SENSOR_KIND_DUMMY,
			       "serial", "0123456789a",
			       "model", "Dummy Sensor #1",
			       "vendor", "Acme Corp",
			       "caps", caps,
			       "native", TRUE,
			       NULL);
	return CD_SENSOR (sensor);
}

