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
 * This object contains all the low level logic for the HUEY hardware.
 */

#include "config.h"

#include <glib-object.h>
#include <gusb.h>
#include <colord-private.h>
#include <huey/huey.h>

#include "../src/cd-sensor.h"

typedef struct
{
	GUsbDevice			*device;
	HueyCtx				*ctx;
} CdSensorHueyPrivate;

/* async state for the sensor readings */
typedef struct {
	gboolean			 ret;
	CdColorXYZ			*sample;
	gulong				 cancellable_id;
	GCancellable			*cancellable;
	GSimpleAsyncResult		*res;
	CdSensor			*sensor;
	CdSensorCap			 current_cap;
} CdSensorAsyncState;

static CdSensorHueyPrivate *
cd_sensor_huey_get_private (CdSensor *sensor)
{
	return g_object_get_data (G_OBJECT (sensor), "priv");
}

static void
cd_sensor_huey_get_sample_state_finish (CdSensorAsyncState *state,
					const GError *error)
{
	/* set result to temp memory location */
	if (state->ret) {
		g_simple_async_result_set_op_res_gpointer (state->res,
							   state->sample,
							   (GDestroyNotify) cd_color_xyz_free);
	} else {
		g_simple_async_result_set_from_error (state->res, error);
	}

	/* deallocate */
	if (state->cancellable != NULL) {
		g_cancellable_disconnect (state->cancellable, state->cancellable_id);
		g_object_unref (state->cancellable);
	}

	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_slice_free (CdSensorAsyncState, state);
}

static void
cd_sensor_huey_cancellable_cancel_cb (GCancellable *cancellable,
				      CdSensorAsyncState *state)
{
	g_warning ("cancelled huey");
}

static void
cd_sensor_huey_get_ambient_thread_cb (GSimpleAsyncResult *res,
				      GObject *object,
				      GCancellable *cancellable)
{
	GError *error = NULL;
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorHueyPrivate *priv = cd_sensor_huey_get_private (sensor);
	CdSensorAsyncState *state = (CdSensorAsyncState *) g_object_get_data (G_OBJECT (cancellable), "state");

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_MEASURING);

	/* hit hardware */
	state->sample->X = huey_device_get_ambient (priv->device, &error);
	if (state->sample->X < 0) {
		cd_sensor_huey_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* success */
	state->ret = TRUE;
	cd_sensor_huey_get_sample_state_finish (state, NULL);
out:
	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
}

static void
cd_sensor_huey_sample_thread_cb (GSimpleAsyncResult *res,
				 GObject *object,
				 GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) g_object_get_data (G_OBJECT (cancellable), "state");
	CdSensorHueyPrivate *priv = cd_sensor_huey_get_private (sensor);
	GError *error = NULL;

	/* measure */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_MEASURING);
	state->sample = huey_ctx_take_sample (priv->ctx,
					      state->current_cap,
					      &error);
	if (state->sample == NULL) {
		cd_sensor_huey_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* save result */
	state->ret = TRUE;
	cd_sensor_huey_get_sample_state_finish (state, NULL);
out:
	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
}

void
cd_sensor_get_sample_async (CdSensor *sensor,
			    CdSensorCap cap,
			    GCancellable *cancellable,
			    GAsyncReadyCallback callback,
			    gpointer user_data)
{
	CdSensorAsyncState *state;
	GCancellable *tmp;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* save state */
	state = g_slice_new0 (CdSensorAsyncState);
	state->res = g_simple_async_result_new (G_OBJECT (sensor),
						callback,
						user_data,
						cd_sensor_get_sample_async);
	state->sensor = g_object_ref (sensor);
	if (cancellable != NULL) {
		state->cancellable = g_object_ref (cancellable);
		state->cancellable_id = g_cancellable_connect (cancellable, G_CALLBACK (cd_sensor_huey_cancellable_cancel_cb), state, NULL);
	}

	/* run in a thread */
	tmp = g_cancellable_new ();
	g_object_set_data (G_OBJECT (tmp), "state", state);
	if (cap == CD_SENSOR_CAP_AMBIENT) {
		g_simple_async_result_run_in_thread (G_SIMPLE_ASYNC_RESULT (state->res),
						     cd_sensor_huey_get_ambient_thread_cb,
						     0,
						     (GCancellable*) tmp);
	} else {
		g_simple_async_result_run_in_thread (G_SIMPLE_ASYNC_RESULT (state->res),
						     cd_sensor_huey_sample_thread_cb,
						     0,
						     (GCancellable*) tmp);
	}
	g_object_unref (tmp);
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
cd_sensor_huey_lock_thread_cb (GSimpleAsyncResult *res,
			       GObject *object,
			       GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorHueyPrivate *priv = cd_sensor_huey_get_private (sensor);
	const guint8 spin_leds[] = { 0x0, 0x1, 0x2, 0x4, 0x8, 0x4, 0x2, 0x1, 0x0, 0xff };
	gboolean ret = FALSE;
	gchar *serial_number_tmp = NULL;
	GError *error = NULL;
	guint i;

	/* try to find the USB device */
	priv->device = cd_sensor_open_usb_device (sensor,
						  0x01, /* config */
						  0x00, /* interface */
						  &error);
	if (priv->device == NULL) {
		cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}
	huey_ctx_set_device (priv->ctx, priv->device);

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_STARTING);

	/* unlock */
	ret = huey_device_unlock (priv->device, &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}

	/* get serial number */
	serial_number_tmp = huey_device_get_serial_number (priv->device, &error);
	if (serial_number_tmp == NULL) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}
	cd_sensor_set_serial (sensor, serial_number_tmp);
	g_debug ("Serial number: %s", serial_number_tmp);

	/* setup sensor */
	ret = huey_ctx_setup (priv->ctx, &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}

	/* spin the LEDs */
	for (i = 0; spin_leds[i] != 0xff; i++) {
		ret = huey_device_set_leds (priv->device, spin_leds[i], &error);
		if (!ret) {
			g_simple_async_result_set_from_error (res, error);
			g_error_free (error);
			goto out;
		}
		g_usleep (50000);
	}
out:
	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
	g_free (serial_number_tmp);
}

void
cd_sensor_lock_async (CdSensor *sensor,
		      GCancellable *cancellable,
		      GAsyncReadyCallback callback,
		      gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* run in a thread */
	res = g_simple_async_result_new (G_OBJECT (sensor),
					 callback,
					 user_data,
					 cd_sensor_lock_async);
	g_simple_async_result_run_in_thread (res,
					     cd_sensor_huey_lock_thread_cb,
					     0,
					     cancellable);
	g_object_unref (res);
}

gboolean
cd_sensor_lock_finish (CdSensor *sensor,
		       GAsyncResult *res,
		       GError **error)
{
	GSimpleAsyncResult *simple;
	gboolean ret = TRUE;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error)) {
		ret = FALSE;
		goto out;
	}
out:
	return ret;
}

static void
cd_sensor_unlock_thread_cb (GSimpleAsyncResult *res,
			    GObject *object,
			    GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorHueyPrivate *priv = cd_sensor_huey_get_private (sensor);
	gboolean ret = FALSE;
	GError *error = NULL;

	/* close */
	if (priv->device != NULL) {
		ret = g_usb_device_close (priv->device, &error);
		if (!ret) {
			g_simple_async_result_set_from_error (res, error);
			g_error_free (error);
			goto out;
		}

		/* clear */
		g_object_unref (priv->device);
		priv->device = NULL;
	}
out:
	return;
}

void
cd_sensor_unlock_async (CdSensor *sensor,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* run in a thread */
	res = g_simple_async_result_new (G_OBJECT (sensor),
					 callback,
					 user_data,
					 cd_sensor_unlock_async);
	g_simple_async_result_run_in_thread (res,
					     cd_sensor_unlock_thread_cb,
					     0,
					     cancellable);
	g_object_unref (res);
}

gboolean
cd_sensor_unlock_finish (CdSensor *sensor,
			 GAsyncResult *res,
			 GError **error)
{
	GSimpleAsyncResult *simple;
	gboolean ret = TRUE;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error)) {
		ret = FALSE;
		goto out;
	}
out:
	return ret;
}

gboolean
cd_sensor_dump_device (CdSensor *sensor, GString *data, GError **error)
{
	CdSensorHueyPrivate *priv = cd_sensor_huey_get_private (sensor);
	gboolean ret;
	guint i;
	guint8 value;
	gchar *tmp;
	const CdVec3 *vec;

	/* dump the unlock string */
	g_string_append_printf (data, "huey-dump-version:%i\n", 2);
	g_string_append_printf (data, "unlock-string:%s\n",
				huey_ctx_get_unlock_string (priv->ctx));
	g_string_append_printf (data, "calibration-value:%f\n",
				huey_ctx_get_calibration_value (priv->ctx));
	vec = huey_ctx_get_dark_offset (priv->ctx);
	g_string_append_printf (data, "dark-offset:%f,%f,%f\n",
				vec->v0, vec->v1, vec->v2);

	/* dump the DeviceRGB to XYZ matrix */
	tmp = cd_mat33_to_string (huey_ctx_get_calibration_lcd (priv->ctx));
	g_string_append_printf (data, "calibration-lcd:%s\n", tmp);
	g_free (tmp);
	tmp = cd_mat33_to_string (huey_ctx_get_calibration_crt (priv->ctx));
	g_string_append_printf (data, "calibration-crt:%s\n", tmp);
	g_free (tmp);

	/* read all the register space */
	for (i = 0; i < 0xff; i++) {
		ret = huey_device_read_register_byte (priv->device,
						      i,
						      &value,
						      error);
		if (!ret)
			goto out;

		/* write details */
		g_string_append_printf (data,
					"register[0x%02x]:0x%02x\n",
					i,
					value);
	}
out:
	return ret;
}

static void
cd_sensor_unref_private (CdSensorHueyPrivate *priv)
{
	if (priv->device != NULL)
		g_object_unref (priv->device);
	g_object_unref (priv->ctx);
	g_free (priv);
}

gboolean
cd_sensor_coldplug (CdSensor *sensor, GError **error)
{
	CdSensorHueyPrivate *priv;
	guint64 caps = cd_bitfield_from_enums (CD_SENSOR_CAP_LCD,
					       CD_SENSOR_CAP_CRT,
					       CD_SENSOR_CAP_AMBIENT,
					       -1);
	g_object_set (sensor,
		      "native", TRUE,
		      "kind", CD_SENSOR_KIND_HUEY,
		      "caps", caps,
		      NULL);
	/* create private data */
	priv = g_new0 (CdSensorHueyPrivate, 1);
	priv->ctx = huey_ctx_new ();
	g_object_set_data_full (G_OBJECT (sensor), "priv", priv,
				(GDestroyNotify) cd_sensor_unref_private);
	return TRUE;
}

