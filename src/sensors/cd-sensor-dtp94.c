/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2015 Richard Hughes <richard@hughsie.com>
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
 * This object contains all the low level logic for the DTP94 hardware.
 */

#include "config.h"

#include <glib-object.h>
#include <gusb.h>
#include <string.h>

#include "../src/cd-sensor.h"

#include <dtp94/dtp94.h>

typedef struct
{
	GUsbDevice			*device;
} CdSensorDtp94Private;

#define DTP94_CONTROL_MESSAGE_TIMEOUT	50000 /* ms */

static CdSensorDtp94Private *
cd_sensor_dtp94_get_private (CdSensor *sensor)
{
	return g_object_get_data (G_OBJECT (sensor), "priv");
}

static void
cd_sensor_dtp94_sample_thread_cb (GTask *task,
				  gpointer source_object,
				  gpointer task_data,
				  GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (source_object);
	CdSensorCap cap = GPOINTER_TO_UINT (task_data);
	CdSensorDtp94Private *priv = cd_sensor_dtp94_get_private (sensor);
	CdColorXYZ *sample;
	g_autoptr(GError) error = NULL;

	/* take a measurement from the sensor */
	cd_sensor_set_state_in_idle (sensor, CD_SENSOR_STATE_MEASURING);
	sample = dtp94_device_take_sample (priv->device, cap, &error);
	if (sample == NULL) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_NO_DATA,
					 "%s", error->message);
		return;
	}
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
	g_task_run_in_thread (task, cd_sensor_dtp94_sample_thread_cb);
}

CdColorXYZ *
cd_sensor_get_sample_finish (CdSensor *sensor, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_sensor_dtp94_lock_thread_cb (GTask *task,
				gpointer source_object,
				gpointer task_data,
				GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (source_object);
	CdSensorDtp94Private *priv = cd_sensor_dtp94_get_private (sensor);
	g_autoptr(GError) error = NULL;
	g_autofree gchar *serial = NULL;

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
		return;
	}

	/* set state */
	cd_sensor_set_state_in_idle (sensor, CD_SENSOR_STATE_STARTING);

	/* do startup sequence */
	if (!dtp94_device_setup (priv->device, &error)) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "%s", error->message);
		return;
	}

	/* get serial */
	serial = dtp94_device_get_serial (priv->device, &error);
	if (serial == NULL) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_NO_DATA,
					 "%s", error->message);
		return;
	}
	cd_sensor_set_serial (sensor, serial);

	/* success */
	g_task_return_boolean (task, TRUE);
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
	g_task_run_in_thread (task, cd_sensor_dtp94_lock_thread_cb);
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
	CdSensorDtp94Private *priv = cd_sensor_dtp94_get_private (sensor);
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
	g_string_append_printf (data, "dtp94-dump-version:%i\n", 1);
	return TRUE;
}

static void
cd_sensor_unref_private (CdSensorDtp94Private *priv)
{
	if (priv->device != NULL)
		g_object_unref (priv->device);
	g_free (priv);
}

gboolean
cd_sensor_coldplug (CdSensor *sensor, GError **error)
{
	CdSensorDtp94Private *priv;
	guint64 caps = cd_bitfield_from_enums (CD_SENSOR_CAP_LCD,
					       CD_SENSOR_CAP_CRT, -1);
	g_object_set (sensor,
		      "native", TRUE,
		      "kind", CD_SENSOR_KIND_DTP94,
		      "caps", caps,
		      NULL);
	/* create private data */
	priv = g_new0 (CdSensorDtp94Private, 1);
	g_object_set_data_full (G_OBJECT (sensor), "priv", priv,
				(GDestroyNotify) cd_sensor_unref_private);
	return TRUE;
}
