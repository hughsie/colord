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

/* All colorhugs have this VID/PID */
#define	CH_USB_VID				0x04d8
#define	CH_USB_PID				0xf8da

#define	CH_USB_CONFIG				0x0001
#define	CH_USB_INTERFACE			0x0000
#define	CH_USB_HID_EP				0x0001
#define	CH_USB_HID_EP_IN			(CH_USB_HID_EP | 0x80)
#define	CH_USB_HID_EP_OUT			(CH_USB_HID_EP | 0x00)
#define	CH_USB_HID_EP_SIZE			64

/* commands */
#define	CH_CMD_SET_MULTIPLIER			0x04
#define	CH_CMD_SET_INTERGRAL_TIME		0x06
#define	CH_CMD_GET_SERIAL_NUMBER		0x0b
#define	CH_CMD_TAKE_READING_XYZ			0x23

#define	CH_USB_TIMEOUT_DEFAULT			2000 /* ms */

/* what frequency divider to use */
typedef enum {
	CH_FREQ_SCALE_0,
	CH_FREQ_SCALE_20,
	CH_FREQ_SCALE_2,
	CH_FREQ_SCALE_100
} ChFreqScale;

/* fatal error morse code */
typedef enum {
	CH_FATAL_ERROR_NONE,
	CH_FATAL_ERROR_UNKNOWN_CMD,
	CH_FATAL_ERROR_WRONG_UNLOCK_CODE,
	CH_FATAL_ERROR_NOT_IMPLEMENTED,
	CH_FATAL_ERROR_UNDERFLOW,
	CH_FATAL_ERROR_NO_SERIAL,
	CH_FATAL_ERROR_LAST
} ChFatalError;

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
	gboolean			 ret;
	GCancellable			*cancellable;
	gulong				 cancellable_id;
	GSimpleAsyncResult		*res;
	guint8				*buffer;
} CdSensorAsyncState;

/* input and output buffer offsets */
#define	CH_BUFFER_INPUT_CMD			0x00
#define	CH_BUFFER_INPUT_DATA			0x01
#define	CH_BUFFER_OUTPUT_RETVAL			0x00
#define	CH_BUFFER_OUTPUT_CMD			0x01
#define	CH_BUFFER_OUTPUT_DATA			0x02

/**
 * ch_client_strerror:
 **/
static const gchar *
ch_client_strerror (ChFatalError fatal_error)
{
	const char *str = NULL;
	switch (fatal_error) {
	case CH_FATAL_ERROR_NONE:
		str = "Success";
		break;
	case CH_FATAL_ERROR_UNKNOWN_CMD:
		str = "Unknown command";
		break;
	case CH_FATAL_ERROR_WRONG_UNLOCK_CODE:
		str = "Wrong unlock code";
		break;
	case CH_FATAL_ERROR_NOT_IMPLEMENTED:
		str = "Not implemented";
		break;
	case CH_FATAL_ERROR_UNDERFLOW:
		str = "Underflow";
		break;
	case CH_FATAL_ERROR_NO_SERIAL:
		str = "No serial";
		break;
	default:
		str = "Unknown error, please report";
		break;
	}
	return str;
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

	g_free (state->buffer);
	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_slice_free (CdSensorAsyncState, state);
}

/**
 * ch_client_packet_parse:
 **/
static gboolean
ch_client_packet_parse (CdSensorAsyncState *state,
			guint8 *buffer_out,
			gsize buffer_out_length,
			GError **error)
{
	ChFatalError fatal_error;
	gboolean ret = TRUE;

	/* parse */
	if (state->buffer[CH_BUFFER_OUTPUT_RETVAL] != CH_FATAL_ERROR_NONE) {
		ret = FALSE;
		fatal_error = state->buffer[CH_BUFFER_OUTPUT_RETVAL];
		g_set_error (error, 1, 0,
			     "Invalid read: retval=0x%02x [%s] "
			     "cmd=0x%02x",
			     fatal_error,
			     ch_client_strerror (fatal_error),
			     state->buffer[CH_BUFFER_OUTPUT_CMD]);
		goto out;
	}

	/* copy */
	if (buffer_out != NULL) {
		memcpy (buffer_out,
			state->buffer + CH_BUFFER_OUTPUT_DATA,
			buffer_out_length);
	}
out:
	return ret;
}

static void
cd_sensor_colorhug_get_sample_reply_cb (GObject *object,
					GAsyncResult *res,
					gpointer user_data)
{
	CdColorXYZ color_result;
	gboolean ret = FALSE;
	GError *error = NULL;
	gfloat buffer[3];
	gsize len;
	GUsbDevice *device = G_USB_DEVICE (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;

	/* get result of request */
	len = g_usb_device_interrupt_transfer_finish (device, res, &error);
	if (len == (gsize) -1) {
		cd_sensor_colorhug_get_sample_state_finish (state, error);
		goto out;
	}

	/* parse output packet */
	ret = ch_client_packet_parse (state,
				      (guint8 *) buffer,
				      sizeof(buffer),
				      &error);
	if (!ret) {
		cd_sensor_colorhug_get_sample_state_finish (state, error);
		goto out;
	}

	/* this is only possible as the PIC has the same floating point
	 * layout as i386 */
	color_result.X = buffer[0];
	color_result.Y = buffer[1];
	color_result.Z = buffer[2];

	g_debug ("finished values: red=%0.6lf, green=%0.6lf, blue=%0.6lf",
		 color_result.X, color_result.Y, color_result.Z);

	/* save result */
	state->ret = TRUE;
	state->sample = cd_color_xyz_dup (&color_result);
	cd_sensor_colorhug_get_sample_state_finish (state, NULL);
out:
	return;
}

static void
cd_sensor_colorhug_get_sample_request_cb (GObject *object,
					  GAsyncResult *res,
					  gpointer user_data)
{
	GError *error = NULL;
	gsize len;
	GUsbDevice *device = G_USB_DEVICE (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;

	/* get result of request */
	len = g_usb_device_interrupt_transfer_finish (device, res, &error);
	if (len == (gsize) -1) {
		cd_sensor_colorhug_get_sample_state_finish (state, error);
		goto out;
	}

	/* now waiting for sample */
	cd_sensor_set_state (state->sensor, CD_SENSOR_STATE_MEASURING);

	/* read reply */
	g_usb_device_interrupt_transfer_async (device,
					       CH_USB_HID_EP_IN,
					       state->buffer,
					       CH_USB_HID_EP_SIZE,
					       CH_USB_TIMEOUT_DEFAULT,
					       state->cancellable,
					       cd_sensor_colorhug_get_sample_reply_cb,
					       state);
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
	CdSensorAsyncState *state;
	CdSensorColorhugPrivate *priv = cd_sensor_colorhug_get_private (sensor);
	GError *error = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* no hardware support */
	if (cap == CD_SENSOR_CAP_PROJECTOR) {
		g_set_error_literal (&error, CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_NO_SUPPORT,
				     "ColorHug cannot measure in projector mode");
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
	state->res = g_simple_async_result_new (G_OBJECT (sensor),
						callback,
						user_data,
						cd_sensor_get_sample_async);
	state->sensor = g_object_ref (sensor);
	state->buffer = g_new0 (guint8, CH_USB_HID_EP_SIZE);

	/* request */
	state->buffer[CH_BUFFER_INPUT_CMD] = CH_CMD_TAKE_READING_XYZ;
	g_usb_device_interrupt_transfer_async (priv->device,
					       CH_USB_HID_EP_OUT,
					       state->buffer,
					       CH_USB_HID_EP_SIZE,
					       CH_USB_TIMEOUT_DEFAULT,
					       cancellable,
					       cd_sensor_colorhug_get_sample_request_cb,
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

	g_free (state->buffer);
	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_slice_free (CdSensorAsyncState, state);
}

static void
cd_sensor_colorhug_set_multiplier_reply_cb (GObject *object,
					    GAsyncResult *res,
					    gpointer user_data)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	gsize len;
	GUsbDevice *device = G_USB_DEVICE (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;

	/* get result of request */
	len = g_usb_device_interrupt_transfer_finish (device, res, &error);
	if (len == (gsize) -1) {
		cd_sensor_colorhug_lock_state_finish (state, error);
		goto out;
	}

	/* parse output packet */
	ret = ch_client_packet_parse (state,
				      NULL,
				      0,
				      &error);
	if (!ret) {
		cd_sensor_colorhug_lock_state_finish (state, error);
		goto out;
	}

	/* save result */
	state->ret = TRUE;
	cd_sensor_colorhug_lock_state_finish (state, NULL);
out:
	return;
}

static void
cd_sensor_colorhug_set_multiplier_request_cb (GObject *object,
					      GAsyncResult *res,
					      gpointer user_data)
{
	GError *error = NULL;
	gsize len;
	GUsbDevice *device = G_USB_DEVICE (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;

	/* get result of request */
	len = g_usb_device_interrupt_transfer_finish (device, res, &error);
	if (len == (gsize) -1) {
		cd_sensor_colorhug_lock_state_finish (state, error);
		goto out;
	}

	/* now waiting for sample */
	cd_sensor_set_state (state->sensor, CD_SENSOR_STATE_MEASURING);

	/* read reply */
	g_usb_device_interrupt_transfer_async (device,
					       CH_USB_HID_EP_IN,
					       state->buffer,
					       CH_USB_HID_EP_SIZE,
					       CH_USB_TIMEOUT_DEFAULT,
					       state->cancellable,
					       cd_sensor_colorhug_set_multiplier_reply_cb,
					       state);
out:
	return;
}

static void
cd_sensor_colorhug_set_integral_time_reply_cb (GObject *object,
					       GAsyncResult *res,
					       gpointer user_data)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	gsize len;
	GUsbDevice *device = G_USB_DEVICE (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;

	/* get result of request */
	len = g_usb_device_interrupt_transfer_finish (device, res, &error);
	if (len == (gsize) -1) {
		cd_sensor_colorhug_lock_state_finish (state, error);
		goto out;
	}

	/* parse output packet */
	ret = ch_client_packet_parse (state,
				      NULL,
				      0,
				      &error);
	if (!ret) {
		cd_sensor_colorhug_lock_state_finish (state, error);
		goto out;
	}

	/* request, set multiplier */
	state->buffer[CH_BUFFER_INPUT_CMD] = CH_CMD_SET_MULTIPLIER;
	state->buffer[CH_BUFFER_INPUT_DATA] = CH_FREQ_SCALE_100;
	g_usb_device_interrupt_transfer_async (device,
					       CH_USB_HID_EP_OUT,
					       state->buffer,
					       CH_USB_HID_EP_SIZE,
					       CH_USB_TIMEOUT_DEFAULT,
					       state->cancellable,
					       cd_sensor_colorhug_set_multiplier_request_cb,
					       state);
out:
	return;
}

static void
cd_sensor_colorhug_set_integral_time_request_cb (GObject *object,
						 GAsyncResult *res,
						 gpointer user_data)
{
	GError *error = NULL;
	gsize len;
	GUsbDevice *device = G_USB_DEVICE (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;

	/* get result of request */
	len = g_usb_device_interrupt_transfer_finish (device, res, &error);
	if (len == (gsize) -1) {
		cd_sensor_colorhug_lock_state_finish (state, error);
		goto out;
	}

	/* now waiting for sample */
	cd_sensor_set_state (state->sensor, CD_SENSOR_STATE_MEASURING);

	/* read reply */
	g_usb_device_interrupt_transfer_async (device,
					       CH_USB_HID_EP_IN,
					       state->buffer,
					       CH_USB_HID_EP_SIZE,
					       CH_USB_TIMEOUT_DEFAULT,
					       state->cancellable,
					       cd_sensor_colorhug_set_integral_time_reply_cb,
					       state);
out:
	return;
}

static void
cd_sensor_colorhug_get_serial_number_reply_cb (GObject *object,
					       GAsyncResult *res,
					       gpointer user_data)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	gchar *serial_number_tmp = NULL;
	guint64 serial_number;
	gsize len;
	GUsbDevice *device = G_USB_DEVICE (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;

	/* get result of request */
	len = g_usb_device_interrupt_transfer_finish (device, res, &error);
	if (len == (gsize) -1) {
		cd_sensor_colorhug_lock_state_finish (state, error);
		goto out;
	}

	/* parse output packet */
	ret = ch_client_packet_parse (state,
				      (guint8 *) &serial_number,
				      4,
				      &error);
	if (!ret) {
		cd_sensor_colorhug_lock_state_finish (state, error);
		goto out;
	}

	serial_number_tmp = g_strdup_printf ("%" G_GUINT64_FORMAT,
					     serial_number);
	cd_sensor_set_serial (state->sensor, serial_number_tmp);
	g_debug ("Serial number: %s", serial_number_tmp);

	/* request, set integral time */
	state->buffer[CH_BUFFER_INPUT_CMD] = CH_CMD_SET_INTERGRAL_TIME;
	state->buffer[CH_BUFFER_INPUT_DATA+0] = 0xff;
	state->buffer[CH_BUFFER_INPUT_DATA+1] = 0xff;
	g_usb_device_interrupt_transfer_async (device,
					       CH_USB_HID_EP_OUT,
					       state->buffer,
					       CH_USB_HID_EP_SIZE,
					       CH_USB_TIMEOUT_DEFAULT,
					       state->cancellable,
					       cd_sensor_colorhug_set_integral_time_request_cb,
					       state);
out:
	return;
}

static void
cd_sensor_colorhug_get_serial_number_request_cb (GObject *object,
						 GAsyncResult *res,
						 gpointer user_data)
{
	GError *error = NULL;
	gsize len;
	GUsbDevice *device = G_USB_DEVICE (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) user_data;

	/* get result of request */
	len = g_usb_device_interrupt_transfer_finish (device, res, &error);
	if (len == (gsize) -1) {
		cd_sensor_colorhug_lock_state_finish (state, error);
		goto out;
	}

	/* now waiting for sample */
	cd_sensor_set_state (state->sensor, CD_SENSOR_STATE_MEASURING);

	/* read reply */
	g_usb_device_interrupt_transfer_async (device,
					       CH_USB_HID_EP_IN,
					       state->buffer,
					       CH_USB_HID_EP_SIZE,
					       CH_USB_TIMEOUT_DEFAULT,
					       state->cancellable,
					       cd_sensor_colorhug_get_serial_number_reply_cb,
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
	state->res = g_simple_async_result_new (G_OBJECT (sensor),
						callback,
						user_data,
						cd_sensor_lock_async);
	state->sensor = g_object_ref (sensor);
	state->buffer = g_new0 (guint8, CH_USB_HID_EP_SIZE);

	/* request serial number */
	state->buffer[CH_BUFFER_INPUT_CMD] = CH_CMD_GET_SERIAL_NUMBER;
	g_usb_device_interrupt_transfer_async (priv->device,
					       CH_USB_HID_EP_OUT,
					       state->buffer,
					       CH_USB_HID_EP_SIZE,
					       CH_USB_TIMEOUT_DEFAULT,
					       cancellable,
					       cd_sensor_colorhug_get_serial_number_request_cb,
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

