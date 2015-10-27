/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2013 Richard Hughes <richard@hughsie.com>
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
 * This object contains all the low level logic for the Hughski
 * ColorHug hardware.
 */

#include "config.h"

#include <glib-object.h>
#include <gusb.h>
#include <string.h>
#include <colorhug/colorhug.h>

#include "cd-sensor.h"

typedef struct
{
	GUsbDevice			*device;
	ChDeviceQueue			*device_queue;
} CdSensorColorhugPrivate;

/* async task for the sensor readings */
typedef struct {
	CdSensor			*sensor;
	CdColorXYZ			 xyz;
	guint32				 serial_number;
	ChSha1				 sha1;
} CdSensorTaskData;

static CdSensorColorhugPrivate *
cd_sensor_colorhug_get_private (CdSensor *sensor)
{
	return g_object_get_data (G_OBJECT (sensor), "priv");
}

static void
cd_sensor_task_data_free (CdSensorTaskData *data)
{
	if (data == NULL)
		return;
	g_object_unref (data->sensor);
	g_free (data);
}

static void
cd_sensor_colorhug_get_sample_cb (GObject *object,
				  GAsyncResult *res,
				  gpointer user_data)
{
	ChDeviceQueue *device_queue = CH_DEVICE_QUEUE (object);
	g_autoptr(GTask) task = G_TASK (user_data);
	CdSensorTaskData *data = g_task_get_task_data (task);
	g_autoptr(GError) error = NULL;

	/* get data */
	if (!ch_device_queue_process_finish (device_queue, res, &error)) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "%s", error->message);
		return;
	}
	g_debug ("finished values: red=%0.6lf, green=%0.6lf, blue=%0.6lf",
		 data->xyz.X, data->xyz.Y, data->xyz.Z);

	/* save result */
	g_task_return_pointer (task,
			       cd_color_xyz_dup (&data->xyz),
			       (GDestroyNotify) cd_color_xyz_free);
}


void
cd_sensor_get_sample_async (CdSensor *sensor,
			    CdSensorCap cap,
			    GCancellable *cancellable,
			    GAsyncReadyCallback callback,
			    gpointer user_data)
{
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (sensor);
	CdSensorTaskData *data;
	guint16 calibration_index;
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* no hardware support */
	task = g_task_new (sensor, cancellable, callback, user_data);
	switch (cap) {
	case CD_SENSOR_CAP_CALIBRATION:
		calibration_index = CH_CALIBRATION_INDEX_FACTORY_ONLY;
		break;
	case CD_SENSOR_CAP_LCD:
		calibration_index = CH_CALIBRATION_INDEX_LCD;
		break;
	case CD_SENSOR_CAP_LED:
		calibration_index = CH_CALIBRATION_INDEX_LED;
		break;
	case CD_SENSOR_CAP_CRT:
	case CD_SENSOR_CAP_PLASMA:
		calibration_index = CH_CALIBRATION_INDEX_CRT;
		break;
	case CD_SENSOR_CAP_PROJECTOR:
		calibration_index = CH_CALIBRATION_INDEX_PROJECTOR;
		break;
	default:
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_NO_SUPPORT,
					 "ColorHug cannot measure in this mode");
		return;
	}

	/* set state */
	data = g_new0 (CdSensorTaskData, 1);
	data->sensor = g_object_ref (sensor);
	g_task_set_task_data (task, data, (GDestroyNotify) cd_sensor_task_data_free);

	/* request */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_STARTING);
	ch_device_queue_take_readings_xyz (priv->device_queue,
					   priv->device,
					   calibration_index,
					   &data->xyz);
	ch_device_queue_process_async (priv->device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       g_task_get_cancellable (task),
				       cd_sensor_colorhug_get_sample_cb,
				       g_object_ref (task));
}

CdColorXYZ *
cd_sensor_get_sample_finish (CdSensor *sensor, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_sensor_colorhug_get_remote_hash_cb (GObject *object,
				       GAsyncResult *res,
				       gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	CdSensorTaskData *data = g_task_get_task_data (task);
	CdSensor *sensor = data->sensor;
	ChDeviceQueue *device_queue = CH_DEVICE_QUEUE (object);
	g_autoptr(GError) error = NULL;

	/* get data, although don't fail if it does not exist */
	if (!ch_device_queue_process_finish (device_queue, res, &error)) {
		g_warning ("ignoring error: %s", error->message);
	} else {
		g_autofree gchar *sha1 = ch_sha1_to_string (&data->sha1);
		cd_sensor_add_option (sensor,
				      "remote-profile-hash",
				      g_variant_new_string (sha1));
	}
	g_task_return_boolean (task, TRUE);
}

static void
cd_sensor_colorhug_startup_cb (GObject *object,
			       GAsyncResult *res,
			       gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	CdSensorTaskData *data = g_task_get_task_data (task);
	CdSensor *sensor = data->sensor;
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (sensor);
	ChDeviceQueue *device_queue = CH_DEVICE_QUEUE (object);
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *serial_number_tmp = NULL;

	/* get data */
	ret = ch_device_queue_process_finish (device_queue, res, &error);
	if (!ret) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "%s", error->message);
		return;
	}

	/* set this */
	serial_number_tmp = g_strdup_printf ("%" G_GUINT32_FORMAT,
					     data->serial_number);
	cd_sensor_set_serial (sensor, serial_number_tmp);
	g_debug ("Serial number: %s", serial_number_tmp);

	/* get the optional remote hash */
	ch_device_queue_get_remote_hash (priv->device_queue,
				         priv->device,
				         &data->sha1);
	ch_device_queue_process_async (priv->device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       g_task_get_cancellable (task),
				       cd_sensor_colorhug_get_remote_hash_cb,
				       task);
}

/**
 * cd_sensor_lock_async:
 *
 * What we do:
 *
 * - Connect to the USB device
 * - Flash the LEDs
 * - Get the serial number
 * - Set the integral time to 50ms
 * - Turn the sensor on to 100%
 * - Gets the remote profile hash
 **/
void
cd_sensor_lock_async (CdSensor *sensor,
		      GCancellable *cancellable,
		      GAsyncReadyCallback callback,
		      gpointer user_data)
{
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (sensor);
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = NULL;
	CdSensorTaskData *data;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* try to find the USB device */
	task = g_task_new (sensor, cancellable, callback, user_data);
	priv->device = cd_sensor_open_usb_device (sensor,
						  CH_USB_CONFIG,
						  CH_USB_INTERFACE,
						  &error);
	if (priv->device == NULL) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "%s", error->message);
		return;
	}

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_STARTING);

	/* save state */
	data = g_new0 (CdSensorTaskData, 1);
	data->sensor = g_object_ref (sensor);
	g_task_set_task_data (task, data, (GDestroyNotify) cd_sensor_task_data_free);

	/* start the color sensor */
	ch_device_queue_set_leds (priv->device_queue,
				  priv->device,
				  0x01, 0x03, 0x10, 0x20);
	ch_device_queue_get_serial_number (priv->device_queue,
					   priv->device,
					   &data->serial_number);
	if (cd_sensor_get_kind (sensor) == CD_SENSOR_KIND_COLORHUG) {
		ch_device_queue_set_integral_time (priv->device_queue,
						   priv->device,
						   CH_INTEGRAL_TIME_VALUE_MAX);
		ch_device_queue_set_multiplier (priv->device_queue,
						priv->device,
						CH_FREQ_SCALE_100);
	}
	ch_device_queue_process_async (priv->device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       g_task_get_cancellable (task),
				       cd_sensor_colorhug_startup_cb,
				       g_object_ref (task));
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
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (sensor);
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

/**********************************************************************/

static void cd_sensor_set_next_option (GTask *task);

static void
cd_sensor_colorhug_set_options_cb (GObject *object,
				   GAsyncResult *res,
				   gpointer user_data)
{
	ChDeviceQueue *device_queue = CH_DEVICE_QUEUE (object);
	GTask *task = G_TASK (user_data);
	g_autoptr(GError) error = NULL;

	/* get data */
	if (!ch_device_queue_process_finish (device_queue, res, &error)) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "%s", error->message);
		return;
	}

	/* do next option */
	cd_sensor_set_next_option (task);
}

static void
cd_sensor_colorhug_write_eeprom_cb (GObject *object,
				   GAsyncResult *res,
				   gpointer user_data)
{
	gboolean ret = FALSE;
	ChDeviceQueue *device_queue = CH_DEVICE_QUEUE (object);
	GTask *task = G_TASK (user_data);
	g_autoptr(GError) error = NULL;

	/* get data */
	ret = ch_device_queue_process_finish (device_queue, res, &error);
	if (!ret) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "%s", error->message);
		return;
	}

	/* all done */
	g_task_return_boolean (task, TRUE);
}

static void
cd_sensor_set_next_option (GTask *task)
{
	CdSensor *sensor = CD_SENSOR (g_task_get_source_object (task));
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (sensor);
	GHashTable *options = g_task_get_task_data (task);
	ChSha1 sha1;
	const gchar *key = NULL;
	const gchar *magic = "Un1c0rn2";
	gboolean ret;
	GVariant *value;
	g_autoptr(GError) error = NULL;
	g_autoptr(GList) keys = NULL;

	/* write eeprom to preserve settings */
	keys = g_hash_table_get_keys (options);
	if (keys == NULL) {
		ch_device_queue_write_eeprom (priv->device_queue,
					      priv->device,
					      magic);
		ch_device_queue_process_async (priv->device_queue,
					       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
					       g_task_get_cancellable (task),
					       cd_sensor_colorhug_write_eeprom_cb,
					       task);
		return;
	}

	/* request */
	key = (const gchar *) keys->data;
	g_debug ("trying to set key %s", key);
	value = g_hash_table_lookup (options, key);
	if (g_strcmp0 (key, "remote-profile-hash") == 0) {

		/* parse the hash */
		ret = ch_sha1_parse (g_variant_get_string (value, NULL),
				     &sha1,
				     &error);
		if (!ret) {
			g_task_return_new_error (task,
						 CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_INTERNAL,
						 "%s", error->message);
			g_hash_table_remove (options, key);
			return;
		}

		/* set the remote hash */
		g_debug ("setting remote hash value %s",
			 g_variant_get_string (value, NULL));
		cd_sensor_add_option (sensor, key, value);
		ch_device_queue_set_remote_hash (priv->device_queue,
						 priv->device,
						 &sha1);
		ch_device_queue_process_async (priv->device_queue,
					       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
					       g_task_get_cancellable (task),
					       cd_sensor_colorhug_set_options_cb,
					       task);
	} else {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_NO_SUPPORT,
					 "Sensor option %s is not supported",
					 key);
		g_hash_table_remove (options, key);
		return;
	}
}

void
cd_sensor_set_options_async (CdSensor *sensor,
			     GHashTable *options,
			     GCancellable *cancellable,
			     GAsyncReadyCallback callback,
			     gpointer user_data)
{
	g_autoptr(GTask) task = NULL;
	g_return_if_fail (CD_IS_SENSOR (sensor));
	task = g_task_new (sensor, cancellable, callback, user_data);
	g_task_set_task_data (task,
			      g_hash_table_ref (options),
			      (GDestroyNotify) g_hash_table_unref);
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_BUSY);
	cd_sensor_set_next_option (task);
}

gboolean
cd_sensor_set_options_finish (CdSensor *sensor, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_sensor_unref_private (CdSensorColorhugPrivate *priv)
{
	if (priv->device != NULL)
		g_object_unref (priv->device);
	g_object_unref (priv->device_queue);
	g_free (priv);
}

gboolean
cd_sensor_coldplug (CdSensor *sensor, GError **error)
{
	CdSensorColorhugPrivate *priv;
	guint64 caps = cd_bitfield_from_enums (CD_SENSOR_CAP_CALIBRATION,
					       CD_SENSOR_CAP_LCD, -1);
	g_object_set (sensor,
		      "native", TRUE,
		      "caps", caps,
		      NULL);
	/* create private data */
	priv = g_new0 (CdSensorColorhugPrivate, 1);
	priv->device_queue = ch_device_queue_new ();
	g_object_set_data_full (G_OBJECT (sensor), "priv", priv,
				(GDestroyNotify) cd_sensor_unref_private);
	return TRUE;
}
