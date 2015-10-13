/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2015 Richard Hughes <richard@hughsie.com>
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

static CdSensorHueyPrivate *
cd_sensor_huey_get_private (CdSensor *sensor)
{
	return g_object_get_data (G_OBJECT (sensor), "priv");
}

static void
cd_sensor_huey_get_ambient_thread_cb (GTask *task,
				      gpointer source_object,
				      gpointer task_data,
				      GCancellable *cancellable)
{
	g_autoptr(GError) error = NULL;
	CdSensor *sensor = CD_SENSOR (source_object);
	CdSensorHueyPrivate *priv = cd_sensor_huey_get_private (sensor);
	CdColorXYZ sample;

	/* set state */
	cd_sensor_set_state_in_idle (sensor, CD_SENSOR_STATE_MEASURING);

	/* hit hardware */
	cd_color_xyz_clear (&sample);
	sample.X = huey_device_get_ambient (priv->device, &error);
	if (sample.X < 0) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_NO_DATA,
					 "%s", error->message);
		return;
	}

	/* success */
	g_task_return_pointer (task,
			       cd_color_xyz_dup (&sample),
			       (GDestroyNotify) cd_color_xyz_free);
}

static void
cd_sensor_huey_sample_thread_cb (GTask *task,
				 gpointer source_object,
				 gpointer task_data,
				 GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (source_object);
	CdSensorHueyPrivate *priv = cd_sensor_huey_get_private (sensor);
	CdSensorCap cap = GPOINTER_TO_UINT (task_data);
	CdColorXYZ *sample;
	g_autoptr(GError) error = NULL;

	/* measure */
	cd_sensor_set_state_in_idle (sensor, CD_SENSOR_STATE_MEASURING);
	sample = huey_ctx_take_sample (priv->ctx, cap, &error);
	if (sample == NULL) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_NO_DATA,
					 "%s", error->message);
		return;
	}

	/* save result */
	g_task_return_pointer (task, sample, NULL);
}

void
cd_sensor_get_sample_async (CdSensor *sensor,
			    CdSensorCap cap,
			    GCancellable *cancellable,
			    GAsyncReadyCallback callback,
			    gpointer user_data)
{
	g_autoptr(GTask) task = NULL;
	g_return_if_fail (CD_IS_SENSOR (sensor));
	task = g_task_new (sensor, cancellable, callback, user_data);
	g_task_set_task_data (task, GUINT_TO_POINTER (cap), NULL);
	if (cap == CD_SENSOR_CAP_AMBIENT) {
		g_task_run_in_thread (task, cd_sensor_huey_get_ambient_thread_cb);
	} else {
		g_task_run_in_thread (task, cd_sensor_huey_sample_thread_cb);
	}
}

CdColorXYZ *
cd_sensor_get_sample_finish (CdSensor *sensor, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_sensor_huey_lock_thread_cb (GTask *task,
			       gpointer source_object,
			       gpointer task_data,
			       GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (source_object);
	CdSensorHueyPrivate *priv = cd_sensor_huey_get_private (sensor);
	const guint8 spin_leds[] = { 0x0, 0x1, 0x2, 0x4, 0x8, 0x4, 0x2, 0x1, 0x0, 0xff };
	guint i;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *serial_number_tmp = NULL;

	/* try to find the USB device */
	priv->device = cd_sensor_open_usb_device (sensor,
						  0x01, /* config */
						  0x00, /* interface */
						  &error);
	if (priv->device == NULL) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "%s", error->message);
		goto out;
	}
	huey_ctx_set_device (priv->ctx, priv->device);

	/* set state */
	cd_sensor_set_state_in_idle (sensor, CD_SENSOR_STATE_STARTING);

	/* unlock */
	if (!huey_device_unlock (priv->device, &error)) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "%s", error->message);
		goto out;
	}

	/* get serial number */
	serial_number_tmp = huey_device_get_serial_number (priv->device, &error);
	if (serial_number_tmp == NULL) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_NO_DATA,
					 "%s", error->message);
		goto out;
	}
	cd_sensor_set_serial (sensor, serial_number_tmp);
	g_debug ("Serial number: %s", serial_number_tmp);

	/* setup sensor */
	if (!huey_ctx_setup (priv->ctx, &error)) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "%s", error->message);
		goto out;
	}

	/* spin the LEDs */
	for (i = 0; spin_leds[i] != 0xff; i++) {
		if (!huey_device_set_leds (priv->device, spin_leds[i], &error)) {
			g_task_return_new_error (task,
						 CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_INTERNAL,
						 "%s", error->message);
			goto out;
		}
		g_usleep (50000);
	}

	/* success */
	g_task_return_boolean (task, TRUE);
out:
	/* set state */
	cd_sensor_set_state_in_idle (sensor, CD_SENSOR_STATE_IDLE);
}

void
cd_sensor_lock_async (CdSensor *sensor,
		      GCancellable *cancellable,
		      GAsyncReadyCallback callback,
		      gpointer user_data)
{
	g_autoptr(GTask) task = NULL;
	g_return_if_fail (CD_IS_SENSOR (sensor));
	task = g_task_new (sensor, cancellable, callback, user_data);
	g_task_run_in_thread (task, cd_sensor_huey_lock_thread_cb);
}

gboolean
cd_sensor_lock_finish (CdSensor *sensor, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_sensor_unlock_thread_cb (GTask *task,
			    gpointer source_object,
			    gpointer task_data,
			    GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (source_object);
	CdSensorHueyPrivate *priv = cd_sensor_huey_get_private (sensor);
	g_autoptr(GError) error = NULL;

	/* close */
	if (priv->device != NULL) {
		if (!g_usb_device_close (priv->device, &error)) {
			g_task_return_new_error (task,
						 CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_INTERNAL,
						 "%s", error->message);
			return;
		}
		g_clear_object (&priv->device);
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

void
cd_sensor_unlock_async (CdSensor *sensor,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data)
{
	g_autoptr(GTask) task = NULL;
	g_return_if_fail (CD_IS_SENSOR (sensor));
	task = g_task_new (sensor, cancellable, callback, user_data);
	g_task_run_in_thread (task, cd_sensor_unlock_thread_cb);
}

gboolean
cd_sensor_unlock_finish (CdSensor *sensor, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
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
			return FALSE;

		/* write details */
		g_string_append_printf (data,
					"register[0x%02x]:0x%02x\n",
					i,
					value);
	}
	return TRUE;
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

