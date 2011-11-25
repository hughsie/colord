/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#include "cd-sensor.h"
#include "cd-sensor-colorhug-private.h"

/* a 32 bit struct to hold numbers from the range -32767 to +32768
 * with a precision of at least 0.000015 */
typedef union {
	struct {
		guint16	fraction;
		gint16	offset;
	};
	struct {
		gint32	raw;
	};
} ChPackedFloat;

typedef struct
{
	GUsbContext			*usb_ctx;
	GUsbDevice			*device;
	GUsbDeviceList			*device_list;
} CdSensorColorhugPrivate;

/* async state for the sensor readings */
typedef struct {
	CdColorXYZ			*sample;
	CdSensor			*sensor;
	ChPackedFloat			 xyz_buffer[3];
	gboolean			 ret;
	GCancellable			*cancellable;
	GSimpleAsyncResult		*res;
	guint32				 serial_number;
	gulong				 cancellable_id;
} CdSensorAsyncState;

/**
 * ch_packed_float_to_double:
 *
 * @pf: A %ChPackedFloat
 * @value: a value in IEEE floating point format
 *
 * Converts a packed float to a double.
 **/
static void
ch_packed_float_to_double (const ChPackedFloat *pf, gdouble *value)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (pf != NULL);
	*value = pf->raw / (gdouble) 0xffff;
}

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
	CdColorXYZ color_result;
	gboolean ret = FALSE;
	GError *error = NULL;
	GUsbDevice *device = G_USB_DEVICE (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;

	/* get data */
	ret = ch_device_write_command_finish (device, res, &error);
	if (!ret) {
		cd_sensor_colorhug_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* convert from LE to host */
	ch_packed_float_to_double (&state->xyz_buffer[0], &color_result.X);
	ch_packed_float_to_double (&state->xyz_buffer[1], &color_result.Y);
	ch_packed_float_to_double (&state->xyz_buffer[2], &color_result.Z);

	g_debug ("finished values: red=%0.6lf, green=%0.6lf, blue=%0.6lf",
		 color_result.X, color_result.Y, color_result.Z);

	/* save result */
	state->ret = TRUE;
	state->sample = cd_color_xyz_dup (&color_result);
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
	case CD_SENSOR_CAP_CRT:
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
	ch_device_write_command_async (priv->device,
				       CH_CMD_TAKE_READING_XYZ,
				       (const guint8 *) &calibration_index,
				       sizeof(calibration_index),
				       (guint8 *) state->xyz_buffer,
				       sizeof(state->xyz_buffer),
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
cd_sensor_colorhug_set_multiplier_cb (GObject *object,
				      GAsyncResult *res,
				      gpointer user_data)
{
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;
	gboolean ret;
	GError *error = NULL;
	GUsbDevice *device = G_USB_DEVICE (object);

	/* get data */
	ret = ch_device_write_command_finish (device, res, &error);
	if (!ret) {
		cd_sensor_colorhug_lock_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* save result */
	state->ret = TRUE;
	cd_sensor_colorhug_lock_state_finish (state, NULL);
out:
	return;
}

static void
cd_sensor_colorhug_set_integral_time_cb (GObject *object,
					 GAsyncResult *res,
					 gpointer user_data)
{
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;
	gboolean ret;
	GError *error = NULL;
	ChFreqScale multiplier = CH_FREQ_SCALE_100;
	GUsbDevice *device = G_USB_DEVICE (object);
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (state->sensor);

	/* get data */
	ret = ch_device_write_command_finish (device, res, &error);
	if (!ret) {
		cd_sensor_colorhug_lock_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* set the multiplier */
	ch_device_write_command_async (priv->device,
				       CH_CMD_SET_MULTIPLIER,
				       (const guint8 *) &multiplier,
				       sizeof(multiplier),
				       NULL,	/* buffer_out */
				       0,	/* buffer_out_len */
				       state->cancellable,
				       cd_sensor_colorhug_set_multiplier_cb,
				       state);
out:
	return;
}


static void
cd_sensor_colorhug_get_serial_number_cb (GObject *object,
					 GAsyncResult *res,
					 gpointer user_data)
{
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;
	gboolean ret;
	gchar *serial_number_tmp = NULL;
	GError *error = NULL;
	guint16 integral_time = 0xffff;
	GUsbDevice *device = G_USB_DEVICE (object);
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (state->sensor);

	/* get data */
	ret = ch_device_write_command_finish (device, res, &error);
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

	/* set the integral time */
	ch_device_write_command_async (priv->device,
				       CH_CMD_SET_INTEGRAL_TIME,
				       (const guint8 *) &integral_time,
				       sizeof(integral_time),
				       NULL,	/* buffer_out */
				       0,	/* buffer_out_len */
				       state->cancellable,
				       cd_sensor_colorhug_set_integral_time_cb,
				       state);
out:
	g_free (serial_number_tmp);
	return;
}

static void
cd_sensor_colorhug_set_leds_cb (GObject *object,
				GAsyncResult *res,
				gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	GUsbDevice *device = G_USB_DEVICE (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (state->sensor);

	/* get data */
	ret = ch_device_write_command_finish (device, res, &error);
	if (!ret) {
		cd_sensor_colorhug_lock_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* get the serial number */
	ch_device_write_command_async (priv->device,
				       CH_CMD_GET_SERIAL_NUMBER,
				       NULL,	/* buffer_in */
				       0,	/* buffer_in_len */
				       (guint8 *) &state->serial_number,
				       sizeof(state->serial_number),
				       state->cancellable,
				       cd_sensor_colorhug_get_serial_number_cb,
				       state);
out:
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
 **/
void
cd_sensor_lock_async (CdSensor *sensor,
		      GCancellable *cancellable,
		      GAsyncReadyCallback callback,
		      gpointer user_data)
{
	CdSensorAsyncState *state;
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (sensor);
	gboolean ret;
	GError *error = NULL;
	guint8 buffer[4];

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* try to find the ColorHug device */
	priv->device = g_usb_device_list_find_by_vid_pid (priv->device_list,
							  CH_USB_VID,
							  CH_USB_PID,
							  &error);
	if (priv->device == NULL) {
		g_simple_async_report_gerror_in_idle (G_OBJECT (sensor),
						      callback,
						      user_data,
						      error);
		g_error_free (error);
		goto out;
	}

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_STARTING);

	/* load device */
	ret = g_usb_device_open (priv->device, &error);
	if (!ret) {
		cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
		g_simple_async_report_gerror_in_idle (G_OBJECT (sensor),
						      callback,
						      user_data,
						      error);
		g_error_free (error);
		goto out;
	}
	ret = g_usb_device_set_configuration (priv->device,
					      CH_USB_CONFIG,
					      &error);
	if (!ret) {
		cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
		g_simple_async_report_gerror_in_idle (G_OBJECT (sensor),
						      callback,
						      user_data,
						      error);
		g_error_free (error);
		goto out;
	}
	ret = g_usb_device_claim_interface (priv->device,
					    CH_USB_INTERFACE,
					    G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
					    &error);
	if (!ret) {
		cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
		g_simple_async_report_gerror_in_idle (G_OBJECT (sensor),
						      callback,
						      user_data,
						      error);
		g_error_free (error);
		goto out;
	}
	g_debug ("Claimed interface 0x%x for device", CH_USB_INTERFACE);

	/* save state */
	state = g_slice_new0 (CdSensorAsyncState);
	if (cancellable != NULL)
		state->cancellable = g_object_ref (cancellable);
	state->res = g_simple_async_result_new (G_OBJECT (sensor),
						callback,
						user_data,
						cd_sensor_lock_async);
	state->sensor = g_object_ref (sensor);

	/* set leds */
	buffer[0] = 0x01;
	buffer[1] = 0x03;
	buffer[2] = 0x10;
	buffer[3] = 0x20;
	ch_device_write_command_async (priv->device,
				       CH_CMD_SET_LEDS,
				       buffer,	/* buffer_in */
				       sizeof(buffer),	/* buffer_in_len */
				       NULL,	/* buffer_out */
				       0,	/* buffer_out_len */
				       state->cancellable,
				       cd_sensor_colorhug_set_leds_cb,
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

static void
cd_sensor_unref_private (CdSensorColorhugPrivate *priv)
{
	g_object_unref (priv->usb_ctx);
	g_object_unref (priv->device_list);
	g_free (priv);
}

gboolean
cd_sensor_coldplug (CdSensor *sensor, GError **error)
{
	CdSensorColorhugPrivate *priv;
	g_object_set (sensor,
		      "native", TRUE,
		      "kind", CD_SENSOR_KIND_COLORHUG,
		      NULL);
	/* create private data */
	priv = g_new0 (CdSensorColorhugPrivate, 1);
	g_object_set_data_full (G_OBJECT (sensor), "priv", priv,
				(GDestroyNotify) cd_sensor_unref_private);
	priv->usb_ctx = g_usb_context_new (NULL);
	priv->device_list = g_usb_device_list_new (priv->usb_ctx);
	return TRUE;
}

