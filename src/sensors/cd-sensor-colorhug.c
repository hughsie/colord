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

/* async state for the sensor readings */
typedef struct {
	CdColorXYZ			*sample;
	CdSensor			*sensor;
	CdColorXYZ			 xyz;
	gboolean			 ret;
	GCancellable			*cancellable;
	GSimpleAsyncResult		*res;
	guint32				 serial_number;
	gulong				 cancellable_id;
	GHashTable			*options;
	ChSha1				 sha1;
} CdSensorAsyncState;

static CdSensorColorhugPrivate *
cd_sensor_colorhug_get_private (CdSensor *sensor)
{
	return g_object_get_data (G_OBJECT (sensor), "priv");
}

static void
cd_sensor_colorhug_get_sample_state_finish (CdSensorAsyncState *state,
					const GError *error)
{
	/* set result to temp memory location */
	if (state->ret) {
		g_simple_async_result_set_op_res_gpointer (state->res,
							   state->sample,
							   cd_color_xyz_free);
	} else {
		g_simple_async_result_set_from_error (state->res, error);
	}

	/* deallocate */
	if (state->cancellable != NULL) {
		g_cancellable_disconnect (state->cancellable, state->cancellable_id);
		g_object_unref (state->cancellable);
	}

	/* set state */
	cd_sensor_set_state (state->sensor, CD_SENSOR_STATE_IDLE);

	/* complete */
	g_simple_async_result_complete_in_idle (state->res);

	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_slice_free (CdSensorAsyncState, state);
}


static void
cd_sensor_colorhug_get_sample_cb (GObject *object,
				  GAsyncResult *res,
				  gpointer user_data)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	ChDeviceQueue *device_queue = CH_DEVICE_QUEUE (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;

	/* get data */
	ret = ch_device_queue_process_finish (device_queue, res, &error);
	if (!ret) {
		cd_sensor_colorhug_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}
	g_debug ("finished values: red=%0.6lf, green=%0.6lf, blue=%0.6lf",
		 state->xyz.X, state->xyz.Y, state->xyz.Z);

	/* save result */
	state->ret = TRUE;
	state->sample = cd_color_xyz_dup (&state->xyz);
	cd_sensor_colorhug_get_sample_state_finish (state, NULL);
out:
	return;
}


void
cd_sensor_get_sample_async (CdSensor *sensor,
			    CdSensorCap cap,
			    GCancellable *cancellable,
			    GAsyncReadyCallback callback,
			    gpointer user_data)
{
	guint16 calibration_index;
	CdSensorAsyncState *state;
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (sensor);
	GError *error = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* no hardware support */
	switch (cap) {
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
		g_set_error_literal (&error, CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_NO_SUPPORT,
				     "ColorHug cannot measure in this mode");
		g_simple_async_report_gerror_in_idle (G_OBJECT (sensor),
						      callback,
						      user_data,
						      error);
		g_error_free (error);
		goto out;
	}

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_STARTING);

	/* save state */
	state = g_slice_new0 (CdSensorAsyncState);
	if (cancellable != NULL)
		state->cancellable = g_object_ref (cancellable);
	state->res = g_simple_async_result_new (G_OBJECT (sensor),
						callback,
						user_data,
						cd_sensor_get_sample_async);
	state->sensor = g_object_ref (sensor);

	/* request */
	ch_device_queue_take_readings_xyz (priv->device_queue,
					   priv->device,
					   calibration_index,
					   &state->xyz);
	ch_device_queue_process_async (priv->device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       state->cancellable,
				       cd_sensor_colorhug_get_sample_cb,
				       state);
out:
	return;
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
cd_sensor_colorhug_lock_state_finish (CdSensorAsyncState *state,
				      const GError *error)
{
	/* set result to temp memory location */
	if (state->ret) {
		g_simple_async_result_set_op_res_gboolean (state->res,
							   TRUE);
	} else {
		g_simple_async_result_set_from_error (state->res, error);
	}

	/* set state */
	cd_sensor_set_state (state->sensor, CD_SENSOR_STATE_IDLE);

	/* deallocate */
	if (state->cancellable != NULL) {
		g_cancellable_disconnect (state->cancellable,
					  state->cancellable_id);
		g_object_unref (state->cancellable);
	}

	/* complete */
	g_simple_async_result_complete_in_idle (state->res);

	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_slice_free (CdSensorAsyncState, state);
}

static void
cd_sensor_colorhug_get_remote_hash_cb (GObject *object,
				       GAsyncResult *res,
				       gpointer user_data)
{
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;
	gboolean ret;
	gchar *sha1;
	GError *error = NULL;
	ChDeviceQueue *device_queue = CH_DEVICE_QUEUE (object);

	/* get data, although don't fail if it does not exist */
	ret = ch_device_queue_process_finish (device_queue, res, &error);
	if (!ret) {
		g_warning ("ignoring error: %s", error->message);
		g_error_free (error);
	} else {
		sha1 = ch_sha1_to_string (&state->sha1);
		cd_sensor_add_option (state->sensor,
				      "remote-profile-hash",
				      g_variant_new_string (sha1));
		g_free (sha1);
	}

	/* save result */
	state->ret = TRUE;
	cd_sensor_colorhug_lock_state_finish (state, NULL);
}

static void
cd_sensor_colorhug_startup_cb (GObject *object,
			       GAsyncResult *res,
			       gpointer user_data)
{
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;
	gboolean ret;
	gchar *serial_number_tmp = NULL;
	GError *error = NULL;
	ChDeviceQueue *device_queue = CH_DEVICE_QUEUE (object);
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (state->sensor);

	/* get data */
	ret = ch_device_queue_process_finish (device_queue, res, &error);
	if (!ret) {
		cd_sensor_colorhug_lock_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* set this */
	serial_number_tmp = g_strdup_printf ("%" G_GUINT32_FORMAT,
					     state->serial_number);
	cd_sensor_set_serial (state->sensor, serial_number_tmp);
	g_debug ("Serial number: %s", serial_number_tmp);

	/* get the optional remote hash */
	ch_device_queue_get_remote_hash (priv->device_queue,
				         priv->device,
				         &state->sha1);
	ch_device_queue_process_async (priv->device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       state->cancellable,
				       cd_sensor_colorhug_get_remote_hash_cb,
				       state);
out:
	g_free (serial_number_tmp);
	return;
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
	CdSensorAsyncState *state;
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (sensor);
	GError *error = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* try to find the USB device */
	priv->device = cd_sensor_open_usb_device (sensor,
						  CH_USB_CONFIG,
						  CH_USB_INTERFACE,
						  &error);
	if (priv->device == NULL) {
		cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
		g_simple_async_report_gerror_in_idle (G_OBJECT (sensor),
						      callback,
						      user_data,
						      error);
		g_error_free (error);
		goto out;
	}

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_STARTING);

	/* save state */
	state = g_slice_new0 (CdSensorAsyncState);
	if (cancellable != NULL)
		state->cancellable = g_object_ref (cancellable);
	state->res = g_simple_async_result_new (G_OBJECT (sensor),
						callback,
						user_data,
						cd_sensor_lock_async);
	state->sensor = g_object_ref (sensor);

	/* start the color sensor */
	ch_device_queue_set_leds (priv->device_queue,
				  priv->device,
				  0x01, 0x03, 0x10, 0x20);
	ch_device_queue_get_serial_number (priv->device_queue,
					   priv->device,
					   &state->serial_number);
	ch_device_queue_set_integral_time (priv->device_queue,
					   priv->device,
					   CH_INTEGRAL_TIME_VALUE_MAX);
	ch_device_queue_set_multiplier (priv->device_queue,
					priv->device,
					CH_FREQ_SCALE_100);
	ch_device_queue_process_async (priv->device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       state->cancellable,
				       cd_sensor_colorhug_startup_cb,
				       state);
out:
	return;
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
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (sensor);
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

/**********************************************************************/

static void cd_sensor_set_next_option (CdSensorAsyncState *state);

static void
cd_sensor_colorhug_set_options_state_finish (CdSensorAsyncState *state,
					     const GError *error)
{
	/* set result to temp memory location */
	if (state->ret) {
		g_simple_async_result_set_op_res_gboolean(state->res, TRUE);
	} else {
		g_simple_async_result_set_from_error (state->res, error);
	}

	/* deallocate */
	if (state->cancellable != NULL) {
		g_cancellable_disconnect (state->cancellable, state->cancellable_id);
		g_object_unref (state->cancellable);
	}

	/* set state */
	cd_sensor_set_state (state->sensor, CD_SENSOR_STATE_IDLE);

	/* complete */
	g_simple_async_result_complete_in_idle (state->res);

	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_hash_table_unref (state->options);
	g_slice_free (CdSensorAsyncState, state);
}

static void
cd_sensor_colorhug_set_options_cb (GObject *object,
				  GAsyncResult *res,
				  gpointer user_data)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	ChDeviceQueue *device_queue = CH_DEVICE_QUEUE (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;

	/* get data */
	ret = ch_device_queue_process_finish (device_queue, res, &error);
	if (!ret) {
		cd_sensor_colorhug_set_options_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* do next option */
	cd_sensor_set_next_option (state);
out:
	return;
}

static void
cd_sensor_colorhug_write_eeprom_cb (GObject *object,
				   GAsyncResult *res,
				   gpointer user_data)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	ChDeviceQueue *device_queue = CH_DEVICE_QUEUE (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;

	/* get data */
	ret = ch_device_queue_process_finish (device_queue, res, &error);
	if (!ret) {
		cd_sensor_colorhug_set_options_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* all done */
	state->ret = TRUE;
	cd_sensor_colorhug_set_options_state_finish (state, NULL);
out:
	return;
}

static void
cd_sensor_set_next_option (CdSensorAsyncState *state)
{
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (state->sensor);
	ChSha1 sha1;
	const gchar *key = NULL;
	const gchar *magic = "Un1c0rn2";
	gboolean ret;
	GError *error = NULL;
	GList *keys;
	GVariant *value;

	/* write eeprom to preserve settings */
	keys = g_hash_table_get_keys (state->options);
	if (keys == NULL) {
		ch_device_queue_write_eeprom (priv->device_queue,
					       priv->device,
					       magic);
		ch_device_queue_process_async (priv->device_queue,
					       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
					       state->cancellable,
					       cd_sensor_colorhug_write_eeprom_cb,
					       state);
		return;
	}

	/* request */
	key = (const gchar *) keys->data;
	g_debug ("trying to set key %s", key);
	value = g_hash_table_lookup (state->options, key);
	if (g_strcmp0 (key, "remote-profile-hash") == 0) {

		/* parse the hash */
		ret = ch_sha1_parse (g_variant_get_string (value, NULL),
				     &sha1,
				     &error);
		if (!ret) {
			cd_sensor_colorhug_set_options_state_finish (state, error);
			g_error_free (error);
			goto out;
		}

		/* set the remote hash */
		g_debug ("setting remote hash value %s",
			 g_variant_get_string (value, NULL));
		cd_sensor_add_option (state->sensor, key, value);
		ch_device_queue_set_remote_hash (priv->device_queue,
						 priv->device,
						 &sha1);
		ch_device_queue_process_async (priv->device_queue,
					       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
					       state->cancellable,
					       cd_sensor_colorhug_set_options_cb,
					       state);
	} else {
		g_set_error (&error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_NO_SUPPORT,
			     "Sensor option %s is not supported",
			     key);
		cd_sensor_colorhug_set_options_state_finish (state, error);
		g_error_free (error);
		goto out;
	}
out:
	g_hash_table_remove (state->options, key);
	g_list_free (keys);
}

void
cd_sensor_set_options_async (CdSensor *sensor,
			     GHashTable *options,
			     GCancellable *cancellable,
			     GAsyncReadyCallback callback,
			     gpointer user_data)
{
	CdSensorAsyncState *state;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_BUSY);

	/* save state */
	state = g_slice_new0 (CdSensorAsyncState);
	if (cancellable != NULL)
		state->cancellable = g_object_ref (cancellable);
	state->res = g_simple_async_result_new (G_OBJECT (sensor),
						callback,
						user_data,
						cd_sensor_set_options_async);
	state->sensor = g_object_ref (sensor);
	state->options = g_hash_table_ref (options);
	cd_sensor_set_next_option (state);
}

gboolean
cd_sensor_set_options_finish (CdSensor *sensor,
			      GAsyncResult *res,
			      GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* failed */
	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	/* grab detail */
	return g_simple_async_result_get_op_res_gboolean (simple);
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
	guint64 caps = cd_bitfield_from_enums (CD_SENSOR_CAP_LCD, -1);
	g_object_set (sensor,
		      "native", TRUE,
		      "kind", CD_SENSOR_KIND_COLORHUG,
		      "caps", caps,
		      NULL);
	/* create private data */
	priv = g_new0 (CdSensorColorhugPrivate, 1);
	priv->device_queue = ch_device_queue_new ();
	g_object_set_data_full (G_OBJECT (sensor), "priv", priv,
				(GDestroyNotify) cd_sensor_unref_private);
	return TRUE;
}
