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
#include <libusb-1.0/libusb.h>

#include "cd-buffer.h"
#include "cd-usb.h"
#include "cd-math.h"
#include "cd-sensor.h"
#include "cd-sensor-huey-private.h"

typedef struct
{
	gboolean			 done_startup;
	CdUsb				*usb;
	CdMat3x3			 calibration_lcd;
	CdMat3x3			 calibration_crt;
	gfloat				 calibration_value;
	CdVec3				 dark_offset;
	gchar				 unlock_string[5];
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

#define HUEY_CONTROL_MESSAGE_TIMEOUT	50000 /* ms */
#define HUEY_MAX_READ_RETRIES		5

/* fudge factor to convert the value of CD_SENSOR_HUEY_COMMAND_GET_AMBIENT to Lux */
#define HUEY_AMBIENT_UNITS_TO_LUX	125.0f

/* The CY7C63001 is paired with a 6.00Mhz crystal */
#define HUEY_CLOCK_FREQUENCY		6e6

/* It takes 6 clock pulses to process a single 16bit increment (INC)
 * instruction and check for the carry so this is the fastest a loop
 * can be processed. */
#define HUEY_POLL_FREQUENCY		1e6

/* Picked out of thin air, just to try to match reality...
 * I have no idea why we need to do this, although it probably
 * indicates we doing something wrong. */
#define HUEY_XYZ_POST_MULTIPLY_SCALE_FACTOR	3.43


static CdSensorHueyPrivate *
cd_sensor_huey_get_private (CdSensor *sensor)
{
	return g_object_get_data (G_OBJECT (sensor), "priv");
}

static void
cd_sensor_huey_print_data (const gchar *title,
			   const guchar *data,
			   gsize length)
{
	guint i;

	if (g_strcmp0 (title, "request") == 0)
		g_print ("%c[%dm", 0x1B, 31);
	if (g_strcmp0 (title, "reply") == 0)
		g_print ("%c[%dm", 0x1B, 34);
	g_print ("%s\t", title);

	for (i=0; i< length; i++)
		g_print ("%02x [%c]\t", data[i], g_ascii_isprint (data[i]) ? data[i] : '?');

	g_print ("%c[%dm\n", 0x1B, 0);
}

static gboolean
cd_sensor_huey_send_data (CdSensorHueyPrivate *priv,
			  const guchar *request, gsize request_len,
			  guchar *reply, gsize reply_len,
			  gsize *reply_read, GError **error)
{
	gint retval;
	gboolean ret = FALSE;
	guint i;
	gint reply_read_raw;
	libusb_device_handle *handle;

	g_return_val_if_fail (request != NULL, FALSE);
	g_return_val_if_fail (request_len != 0, FALSE);
	g_return_val_if_fail (reply != NULL, FALSE);
	g_return_val_if_fail (reply_len != 0, FALSE);
	g_return_val_if_fail (reply_read != NULL, FALSE);

	/* show what we've got */
	cd_sensor_huey_print_data ("request", request, request_len);

	/* do sync request */
	handle = cd_usb_get_device_handle (priv->usb);
	if (handle == NULL) {
		g_set_error_literal (error, CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_INTERNAL,
				     "failed to get device handle");
		goto out;
	}
	retval = libusb_control_transfer (handle,
					  LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
					  0x09, 0x0200, 0,
					  (guchar *) request, request_len,
					  HUEY_CONTROL_MESSAGE_TIMEOUT);
	if (retval < 0) {
		g_set_error (error, CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_INTERNAL,
			     "failed to send request: %s", libusb_strerror (retval));
		goto out;
	}

	/* some commands need to retry the read */
	for (i=0; i<HUEY_MAX_READ_RETRIES; i++) {

		/* get sync response */
		retval = libusb_interrupt_transfer (handle, 0x81,
						    reply, (gint) reply_len, &reply_read_raw,
						    HUEY_CONTROL_MESSAGE_TIMEOUT);
		if (retval < 0) {
			g_set_error (error, CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_INTERNAL,
				     "failed to get reply: %s", libusb_strerror (retval));
			goto out;
		}

		/* on 64bit we can't just cast a gsize pointer to a
		 * gint pointer, we have to copy */
		*reply_read = reply_read_raw;

		/* show what we've got */
		cd_sensor_huey_print_data ("reply", reply, *reply_read);

		/* the second byte seems to be the command again */
		if (reply[1] != request[0]) {
			g_set_error (error, CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_INTERNAL,
				     "wrong command reply, got 0x%02x, expected 0x%02x", reply[1], request[0]);
			goto out;
		}

		/* the first byte is status */
		if (reply[0] == CD_SENSOR_HUEY_RETURN_SUCCESS) {
			ret = TRUE;
			break;
		}

		/* failure, the return buffer is set to "Locked" */
		if (reply[0] == CD_SENSOR_HUEY_RETURN_LOCKED) {
			g_set_error_literal (error, CD_SENSOR_ERROR,
					     CD_SENSOR_ERROR_INTERNAL,
					     "the device is locked");
			goto out;
		}

		/* failure, the return buffer is set to "NoCmd" */
		if (reply[0] == CD_SENSOR_HUEY_RETURN_ERROR) {
			g_set_error (error, CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_INTERNAL,
				     "failed to issue command: %s", &reply[2]);
			goto out;
		}

		/* we ignore retry */
		if (reply[0] != CD_SENSOR_HUEY_RETURN_RETRY) {
			g_set_error (error, CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_INTERNAL,
				     "return value unknown: 0x%02x", reply[0]);
			goto out;
		}
	}

	/* no success */
	if (!ret) {
		g_set_error (error, CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_INTERNAL,
			     "gave up retrying after %i reads", HUEY_MAX_READ_RETRIES);
		goto out;
	}
out:
	return ret;
}

static gboolean
cd_sensor_huey_read_register_byte (CdSensorHueyPrivate *priv,
				   guint8 addr,
				   guint8 *value,
				   GError **error)
{
	guchar request[] = { CD_SENSOR_HUEY_COMMAND_REGISTER_READ,
			     0xff,
			     0x00,
			     0x10,
			     0x3c,
			     0x06,
			     0x00,
			     0x00 };
	guchar reply[8];
	gboolean ret;
	gsize reply_read;

	/* hit hardware */
	request[1] = addr;
	ret = cd_sensor_huey_send_data (priv,
					request, 8,
					reply, 8,
					&reply_read,
					error);
	if (!ret)
		goto out;
	*value = reply[3];
out:
	return ret;
}

static gboolean
cd_sensor_huey_read_register_string (CdSensorHueyPrivate *huey,
				     guint8 addr,
				     gchar *value,
				     gsize len,
				     GError **error)
{
	guint8 i;
	gboolean ret = TRUE;

	/* get each byte of the string */
	for (i=0; i<len; i++) {
		ret = cd_sensor_huey_read_register_byte (huey,
							 addr+i,
							 (guint8*) &value[i],
							 error);
		if (!ret)
			goto out;
	}
out:
	return ret;
}

static gboolean
cd_sensor_huey_read_register_word (CdSensorHueyPrivate *huey,
				   guint8 addr,
				   guint32 *value,
				   GError **error)
{
	guint8 i;
	guint8 tmp[4];
	gboolean ret = TRUE;

	/* get each byte of the 32 bit number */
	for (i=0; i<4; i++) {
		ret = cd_sensor_huey_read_register_byte (huey,
							  addr+i,
							  tmp+i,
							  error);
		if (!ret)
			goto out;
	}

	/* convert to a 32 bit integer */
	*value = cd_buffer_read_uint32_be (tmp);
out:
	return ret;
}

static gboolean
cd_sensor_huey_read_register_float (CdSensorHueyPrivate *huey,
				    guint8 addr,
				    gfloat *value,
				    GError **error)
{
	gboolean ret;
	guint32 tmp = 0;

	/* first read in 32 bit integer */
	ret = cd_sensor_huey_read_register_word (huey,
						  addr,
						  &tmp,
						  error);
	if (!ret)
		goto out;

	/* convert to float */
	*((guint32 *)value) = tmp;
out:
	return ret;
}

static gboolean
cd_sensor_huey_read_register_vector (CdSensorHueyPrivate *huey,
				     guint8 addr,
				     CdVec3 *value,
				     GError **error)
{
	gboolean ret = TRUE;
	guint i;
	gfloat tmp = 0.0f;
	gdouble *vector_data;

	/* get this to avoid casting */
	vector_data = cd_vec3_get_data (value);

	/* read in vec3 */
	for (i=0; i<3; i++) {
		ret = cd_sensor_huey_read_register_float (huey,
							   addr + (i*4),
							   &tmp,
							   error);
		if (!ret)
			goto out;

		/* save in matrix */
		*(vector_data+i) = tmp;
	}
out:
	return ret;
}

static gboolean
cd_sensor_huey_read_register_matrix (CdSensorHueyPrivate *huey,
				     guint8 addr,
				     CdMat3x3 *value,
				     GError **error)
{
	gboolean ret = TRUE;
	guint i;
	gfloat tmp = 0.0f;
	gdouble *matrix_data;

	/* get this to avoid casting */
	matrix_data = cd_mat33_get_data (value);

	/* read in 3d matrix */
	for (i=0; i<9; i++) {
		ret = cd_sensor_huey_read_register_float (huey,
							  addr + (i*4),
							  &tmp,
							  error);
		if (!ret)
			goto out;

		/* save in matrix */
		*(matrix_data+i) = tmp;
	}
out:
	return ret;
}

static void
cd_sensor_huey_get_sample_state_finish (CdSensorAsyncState *state,
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
	CdSensor *sensor = CD_SENSOR (object);
	GError *error = NULL;
	guchar reply[8];
	gboolean ret = FALSE;
	gsize reply_read;
	guchar request[] = { CD_SENSOR_HUEY_COMMAND_GET_AMBIENT,
			     0x03,
			     0x00,
			     0x00,
			     0x00,
			     0x00,
			     0x00,
			     0x00 };
	CdSensorHueyPrivate *priv = cd_sensor_huey_get_private (sensor);
	CdSensorAsyncState *state = (CdSensorAsyncState *) g_object_get_data (G_OBJECT (cancellable), "state");

	/* no hardware support */
	if (state->current_cap == CD_SENSOR_CAP_PROJECTOR) {
		g_set_error_literal (&error, CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_NO_SUPPORT,
				     "HUEY cannot measure ambient light in projector mode");
		cd_sensor_huey_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_MEASURING);

	/* hit hardware */
	request[2] = (state->current_cap == CD_SENSOR_CAP_LCD) ? 0x00 : 0x02;
	ret = cd_sensor_huey_send_data (priv,
					request, 8,
					reply, 8,
					&reply_read,
					&error);
	if (!ret) {
		cd_sensor_huey_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* parse the value */
	state->ret = TRUE;
	state->sample->X = (gdouble) cd_buffer_read_uint16_be (reply+5) / HUEY_AMBIENT_UNITS_TO_LUX;
	cd_sensor_huey_get_sample_state_finish (state, NULL);
out:
	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
}

static gboolean
cd_sensor_huey_set_leds (CdSensor *sensor, guint8 value, GError **error)
{
	guchar reply[8];
	gsize reply_read;
	CdSensorHueyPrivate *priv = cd_sensor_huey_get_private (sensor);
	guchar payload[] = { CD_SENSOR_HUEY_COMMAND_SET_LEDS,
			     0x00,
			     ~value,
			     0x00,
			     0x00,
			     0x00,
			     0x00,
			     0x00 };
	return cd_sensor_huey_send_data (priv,
					 payload, 8, reply, 8,
					 &reply_read,
					 error);
}

typedef struct {
	guint16	R;
	guint16	G;
	guint16	B;
} CdSensorHueyPrivateMultiplier;

typedef struct {
	guint32	R;
	guint32	G;
	guint32	B;
} CdSensorHueyPrivateDeviceRaw;

static gboolean
cd_sensor_huey_sample_for_threshold (CdSensorHueyPrivate *priv,
				     CdSensorHueyPrivateMultiplier *threshold,
				     CdSensorHueyPrivateDeviceRaw *raw,
				     GError **error)
{
	guchar request[] = { CD_SENSOR_HUEY_COMMAND_SENSOR_MEASURE_RGB,
			     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	guchar reply[8];
	gboolean ret;
	gsize reply_read;

	/* these are 16 bit gain values */
	cd_buffer_write_uint16_be (request + 1, threshold->R);
	cd_buffer_write_uint16_be (request + 3, threshold->G);
	cd_buffer_write_uint16_be (request + 5, threshold->B);

	/* measure, and get red */
	ret = cd_sensor_huey_send_data (priv,
					request, 8,
					reply, 8,
					&reply_read,
					error);
	if (!ret)
		goto out;

	/* get value */
	raw->R = cd_buffer_read_uint32_be (reply+2);

	/* get green */
	request[0] = CD_SENSOR_HUEY_COMMAND_READ_GREEN;
	ret = cd_sensor_huey_send_data (priv,
					request, 8,
					reply, 8,
					&reply_read,
					error);
	if (!ret)
		goto out;

	/* get value */
	raw->G = cd_buffer_read_uint32_be (reply+2);

	/* get blue */
	request[0] = CD_SENSOR_HUEY_COMMAND_READ_BLUE;
	ret = cd_sensor_huey_send_data (priv,
					request, 8,
					reply, 8,
					&reply_read,
					error);
	if (!ret)
		goto out;

	/* get value */
	raw->B = cd_buffer_read_uint32_be (reply+2);
out:
	return ret;
}

/**
 * cd_sensor_huey_convert_device_RGB_to_XYZ:
 *
 * / X \   ( / R \    / c a l \ )
 * | Y | = ( | G |  * | m a t | ) x post_scale
 * \ Z /   ( \ B /    \ l c d / )
 *
 **/
static void
cd_sensor_huey_convert_device_RGB_to_XYZ (CdColorRGB *src,
					  CdColorXYZ *dest,
					  CdMat3x3 *calibration,
					  gdouble post_scale)
{
	CdVec3 *result;

	/* convolve */
	result = (CdVec3 *) dest;
	cd_mat33_vector_multiply (calibration, (CdVec3 *) src, result);

	/* post-multiply */
	cd_vec3_scalar_multiply (result,
				 post_scale,
				 result);
}

static void
cd_sensor_huey_sample_thread_cb (GSimpleAsyncResult *res,
				 GObject *object,
				 GCancellable *cancellable)
{
	gboolean ret = FALSE;
	CdColorRGB values;
	CdColorXYZ color_result;
	CdMat3x3 *device_calibration;
	CdSensorHueyPrivateDeviceRaw color_native;
	CdSensorHueyPrivateMultiplier multiplier;
	CdVec3 *temp;
	GError *error = NULL;
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorHueyPrivate *priv = cd_sensor_huey_get_private (sensor);
	CdSensorAsyncState *state = (CdSensorAsyncState *) g_object_get_data (G_OBJECT (cancellable), "state");

	/* no hardware support */
	if (state->current_cap == CD_SENSOR_CAP_PROJECTOR) {
		g_set_error_literal (&error, CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_NO_SUPPORT,
				     "HUEY cannot measure in projector mode");
		cd_sensor_huey_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_MEASURING);

	/* set this to one value for a quick approximate value */
	multiplier.R = 1;
	multiplier.G = 1;
	multiplier.B = 1;
	ret = cd_sensor_huey_sample_for_threshold (priv,
						   &multiplier,
						   &color_native,
						   &error);
	if (!ret) {
		cd_sensor_huey_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}
	g_debug ("initial values: red=%i, green=%i, blue=%i",
		 color_native.R, color_native.G, color_native.B);

	/* try to fill the 16 bit register for accuracy */
	multiplier.R = HUEY_POLL_FREQUENCY / color_native.R;
	multiplier.G = HUEY_POLL_FREQUENCY / color_native.G;
	multiplier.B = HUEY_POLL_FREQUENCY / color_native.B;

	/* don't allow a value of zero */
	if (multiplier.R == 0)
		multiplier.R = 1;
	if (multiplier.G == 0)
		multiplier.G = 1;
	if (multiplier.B == 0)
		multiplier.B = 1;
	g_debug ("using multiplier factor: red=%i, green=%i, blue=%i",
		 multiplier.R, multiplier.G, multiplier.B);
	ret = cd_sensor_huey_sample_for_threshold (priv,
						   &multiplier,
						   &color_native,
						   &error);
	if (!ret) {
		cd_sensor_huey_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	g_debug ("raw values: red=%i, green=%i, blue=%i",
		 color_native.R, color_native.G, color_native.B);

	/* get DeviceRGB values */
	values.R = (gdouble) multiplier.R * 0.5f * HUEY_POLL_FREQUENCY / ((gdouble) color_native.R);
	values.G = (gdouble) multiplier.G * 0.5f * HUEY_POLL_FREQUENCY / ((gdouble) color_native.G);
	values.B = (gdouble) multiplier.B * 0.5f * HUEY_POLL_FREQUENCY / ((gdouble) color_native.B);

	g_debug ("scaled values: red=%0.6lf, green=%0.6lf, blue=%0.6lf",
		 values.R, values.G, values.B);

	/* remove dark offset */
	temp = (CdVec3*) &values;
	cd_vec3_subtract (temp,
			  &priv->dark_offset,
			  temp);

	g_debug ("dark offset values: red=%0.6lf, green=%0.6lf, blue=%0.6lf",
		 values.R, values.G, values.B);

	/* negative values don't make sense (device needs recalibration) */
	if (values.R < 0.0f)
		values.R = 0.0f;
	if (values.G < 0.0f)
		values.G = 0.0f;
	if (values.B < 0.0f)
		values.B = 0.0f;

	/* we use different calibration matrices for each output type */
	if (state->current_cap == CD_SENSOR_CAP_LCD) {
		g_debug ("using LCD calibration matrix");
		device_calibration = &priv->calibration_lcd;
	} else {
		g_debug ("using CRT calibration matrix");
		device_calibration = &priv->calibration_crt;
	}

	/* convert from device RGB to XYZ */
	cd_sensor_huey_convert_device_RGB_to_XYZ (&values,
						   &color_result,
						   device_calibration,
						   HUEY_XYZ_POST_MULTIPLY_SCALE_FACTOR);

	g_debug ("finished values: red=%0.6lf, green=%0.6lf, blue=%0.6lf",
		 color_result.X, color_result.Y, color_result.Z);

	/* save result */
	state->ret = TRUE;
	state->sample = cd_color_xyz_dup (&color_result);
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
	} else if (cap == CD_SENSOR_CAP_LCD ||
		   cap == CD_SENSOR_CAP_CRT) {
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

static gboolean
cd_sensor_huey_send_unlock (CdSensorHueyPrivate *priv, GError **error)
{
	guchar request[8];
	guchar reply[8];
	gboolean ret;
	gsize reply_read;

	request[0] = CD_SENSOR_HUEY_COMMAND_UNLOCK;
	request[1] = 'G';
	request[2] = 'r';
	request[3] = 'M';
	request[4] = 'b';
	request[5] = 'k'; /* <- perhaps junk, need to test next time locked */
	request[6] = 'e'; /* <-         "" */
	request[7] = 'd'; /* <-         "" */

	/* no idea why the hardware gets 'locked' */
	ret = cd_sensor_huey_send_data (priv,
					request, 8,
					reply, 8,
					&reply_read,
					error);
	if (!ret)
		goto out;
out:
	return ret;
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
	guint32 serial_number;
	guint i;

	/* connect */
	ret = cd_usb_connect (priv->usb,
			      CD_SENSOR_HUEY_VENDOR_ID, CD_SENSOR_HUEY_PRODUCT_ID,
			      0x01, 0x00, &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_STARTING);

	/* unlock */
	ret = cd_sensor_huey_send_unlock (priv, &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}

	/* get serial number */
	ret = cd_sensor_huey_read_register_word (priv,
						 CD_SENSOR_HUEY_EEPROM_ADDR_SERIAL,
						 &serial_number,
						 &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}
	serial_number_tmp = g_strdup_printf ("%i", serial_number);
	cd_sensor_set_serial (sensor, serial_number_tmp);
	g_debug ("Serial number: %s", serial_number_tmp);

	/* get unlock string */
	ret = cd_sensor_huey_read_register_string (priv,
						   CD_SENSOR_HUEY_EEPROM_ADDR_UNLOCK,
						   priv->unlock_string,
						   5,
						   &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}
	g_debug ("Unlock string: %s", priv->unlock_string);

	/* get matrix */
	cd_mat33_clear (&priv->calibration_lcd);
	ret = cd_sensor_huey_read_register_matrix (priv,
						   CD_SENSOR_HUEY_EEPROM_ADDR_CALIBRATION_DATA_LCD,
						   &priv->calibration_lcd,
						   &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}
	g_debug ("device calibration LCD: %s",
		 cd_mat33_to_string (&priv->calibration_lcd));

	/* get another matrix, although this one is different... */
	cd_mat33_clear (&priv->calibration_crt);
	ret = cd_sensor_huey_read_register_matrix (priv,
						   CD_SENSOR_HUEY_EEPROM_ADDR_CALIBRATION_DATA_CRT,
						   &priv->calibration_crt,
						   &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}
	g_debug ("device calibration CRT: %s",
		 cd_mat33_to_string (&priv->calibration_crt));

	/* this number is different on all three hueys */
	ret = cd_sensor_huey_read_register_float (priv,
						  CD_SENSOR_HUEY_EEPROM_ADDR_AMBIENT_CALIB_VALUE,
						  &priv->calibration_value,
						  &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}

	/* this vector changes between sensor 1 and 3 */
	ret = cd_sensor_huey_read_register_vector (priv,
						   CD_SENSOR_HUEY_EEPROM_ADDR_DARK_OFFSET,
						   &priv->dark_offset,
						   &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}

	/* spin the LEDs */
	for (i=0; spin_leds[i] != 0xff; i++) {
		ret = cd_sensor_huey_set_leds (sensor, spin_leds[i], &error);
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

	/* connect */
	ret = cd_usb_disconnect (priv->usb,
			         &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
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

	/* dump the unlock string */
	g_string_append_printf (data, "huey-dump-version:%i\n", 2);
	g_string_append_printf (data, "unlock-string:%s\n",
				priv->unlock_string);
	g_string_append_printf (data, "calibration-value:%f\n",
				priv->calibration_value);
	g_string_append_printf (data, "dark-offset:%f,%f,%f\n",
				priv->dark_offset.v0,
				priv->dark_offset.v1,
				priv->dark_offset.v2);

	/* dump the DeviceRGB to XYZ matrix */
	tmp = cd_mat33_to_string (&priv->calibration_lcd);
	g_string_append_printf (data, "calibration-lcd:%s\n", tmp);
	g_free (tmp);
	tmp = cd_mat33_to_string (&priv->calibration_crt);
	g_string_append_printf (data, "calibration-crt:%s\n", tmp);
	g_free (tmp);
	g_string_append_printf (data, "post-scale-value:%f\n",
				HUEY_XYZ_POST_MULTIPLY_SCALE_FACTOR);

	/* read all the register space */
	for (i=0; i<0xff; i++) {
		ret = cd_sensor_huey_read_register_byte (priv,
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
	g_object_unref (priv->usb);
	g_free (priv);
}

gboolean
cd_sensor_coldplug (CdSensor *sensor, GError **error)
{
	CdSensorHueyPrivate *priv;
	g_object_set (sensor,
		      "native", TRUE,
		      "kind", CD_SENSOR_KIND_HUEY,
		      NULL);
	/* create private data */
	priv = g_new0 (CdSensorHueyPrivate, 1);
	g_object_set_data_full (G_OBJECT (sensor), "priv", priv,
				(GDestroyNotify) cd_sensor_unref_private);
	priv->usb = cd_usb_new ();
	cd_mat33_clear (&priv->calibration_lcd);
	cd_mat33_clear (&priv->calibration_crt);
	priv->unlock_string[0] = '\0';
	return TRUE;
}

