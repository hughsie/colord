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
 * SECTION:cd-sensor-huey
 * @short_description: Userspace driver for the HUEY colorimeter.
 *
 * This object contains all the low level logic for the HUEY hardware.
 */

#include "config.h"

#include <glib-object.h>
#include <libusb-1.0/libusb.h>

#include "cd-buffer.h"
#include "cd-usb.h"
#include "cd-math.h"
#include "cd-sensor-huey.h"
#include "cd-sensor-huey-private.h"

static void     cd_sensor_huey_finalize	(GObject     *object);

#define CD_SENSOR_HUEY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_SENSOR_HUEY, CdSensorHueyPrivate))

/**
 * CdSensorHueyPrivate:
 *
 * Private #CdSensorHuey data
 **/
struct _CdSensorHueyPrivate
{
	gboolean			 done_startup;
	CdUsb				*usb;
	CdMat3x3			 calibration_lcd;
	CdMat3x3			 calibration_crt;
	gfloat				 calibration_value;
	CdVec3				 dark_offset;
	gchar				 unlock_string[5];
};

/* async state for the sensor readings */
typedef struct {
	gboolean			 ret;
	CdSensorSample			 sample;
	gulong				 cancellable_id;
	GCancellable			*cancellable;
	GSimpleAsyncResult		*res;
	CdSensor			*sensor;
	CdSensorCap			 current_cap;
} CdSensorAsyncState;

G_DEFINE_TYPE (CdSensorHuey, cd_sensor_huey, CD_TYPE_SENSOR)

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

/**
 * cd_sensor_huey_print_data:
 **/
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

/**
 * cd_sensor_huey_send_data:
 **/
static gboolean
cd_sensor_huey_send_data (CdSensorHuey *sensor_huey,
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
	handle = cd_usb_get_device_handle (sensor_huey->priv->usb);
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

/**
 * cd_sensor_huey_read_register_byte:
 **/
static gboolean
cd_sensor_huey_read_register_byte (CdSensorHuey *sensor_huey,
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
	ret = cd_sensor_huey_send_data (sensor_huey,
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

/**
 * cd_sensor_huey_read_register_string:
 **/
static gboolean
cd_sensor_huey_read_register_string (CdSensorHuey *huey,
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

/**
 * cd_sensor_huey_read_register_word:
 **/
static gboolean
cd_sensor_huey_read_register_word (CdSensorHuey *huey,
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

/**
 * cd_sensor_huey_read_register_float:
 **/
static gboolean
cd_sensor_huey_read_register_float (CdSensorHuey *huey,
				    guint8 addr,
				    gfloat *value,
				    GError **error)
{
	gboolean ret;
	guint32 tmp;

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

/**
 * cd_sensor_huey_read_register_vector:
 **/
static gboolean
cd_sensor_huey_read_register_vector (CdSensorHuey *huey,
				     guint8 addr,
				     CdVec3 *value,
				     GError **error)
{
	gboolean ret = TRUE;
	guint i;
	gfloat tmp;
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

/**
 * cd_sensor_huey_read_register_matrix:
 **/
static gboolean
cd_sensor_huey_read_register_matrix (CdSensorHuey *huey,
				     guint8 addr,
				     CdMat3x3 *value,
				     GError **error)
{
	gboolean ret = TRUE;
	guint i;
	gfloat tmp;
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

/**
 * cd_sensor_huey_get_sample_state_finish:
 **/
static void
cd_sensor_huey_get_sample_state_finish (CdSensorAsyncState *state,
					const GError *error)
{
	CdSensorSample *sample;

	/* set result to temp memory location */
	if (state->ret) {
		sample = g_new0 (CdSensorSample, 1);
		cd_sensor_copy_sample (&state->sample, sample);
		g_simple_async_result_set_op_res_gpointer (state->res,
							   sample,
							   (GDestroyNotify) g_free);
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

/**
 * cd_sensor_huey_cancellable_cancel_cb:
 **/
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
	CdSensorHuey *sensor_huey = CD_SENSOR_HUEY (sensor);
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
	ret = cd_sensor_huey_send_data (sensor_huey,
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
	state->sample.luminance = (gdouble) cd_buffer_read_uint16_be (reply+5) / HUEY_AMBIENT_UNITS_TO_LUX;
	cd_sensor_huey_get_sample_state_finish (state, NULL);
out:
	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
}

/**
 * cd_sensor_huey_set_leds:
 **/
static gboolean
cd_sensor_huey_set_leds (CdSensor *sensor, guint8 value, GError **error)
{
	guchar reply[8];
	gsize reply_read;
	CdSensorHuey *sensor_huey = CD_SENSOR_HUEY (sensor);
	guchar payload[] = { CD_SENSOR_HUEY_COMMAND_SET_LEDS,
			     0x00,
			     ~value,
			     0x00,
			     0x00,
			     0x00,
			     0x00,
			     0x00 };
	return cd_sensor_huey_send_data (sensor_huey,
					  payload, 8, reply, 8,
					  &reply_read,
					  error);
}

typedef struct {
	guint16	R;
	guint16	G;
	guint16	B;
} CdSensorHueyMultiplier;

typedef struct {
	guint32	R;
	guint32	G;
	guint32	B;
} CdSensorHueyDeviceRaw;

/**
 * cd_sensor_huey_sample_for_threshold:
 **/
static gboolean
cd_sensor_huey_sample_for_threshold (CdSensorHuey *sensor_huey,
				     CdSensorHueyMultiplier *threshold,
				     CdSensorHueyDeviceRaw *raw,
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
	ret = cd_sensor_huey_send_data (sensor_huey,
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
	ret = cd_sensor_huey_send_data (sensor_huey,
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
	ret = cd_sensor_huey_send_data (sensor_huey,
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
	CdSensorHueyDeviceRaw color_native;
	CdSensorHueyMultiplier multiplier;
	CdVec3 *temp;
	GError *error = NULL;
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorHuey *sensor_huey = CD_SENSOR_HUEY (sensor);
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
	ret = cd_sensor_huey_sample_for_threshold (sensor_huey,
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
	ret = cd_sensor_huey_sample_for_threshold (sensor_huey,
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
			  &sensor_huey->priv->dark_offset,
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
		device_calibration = &sensor_huey->priv->calibration_lcd;
	} else {
		g_debug ("using CRT calibration matrix");
		device_calibration = &sensor_huey->priv->calibration_crt;
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
	state->sample.luminance = CD_SENSOR_NO_VALUE;
	cd_color_copy_xyz (&color_result, &state->sample.value);
	cd_sensor_huey_get_sample_state_finish (state, NULL);
out:
	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
}

/**
 * cd_sensor_huey_get_sample_async:
 **/
static void
cd_sensor_huey_get_sample_async (CdSensor *sensor,
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
						cd_sensor_huey_get_sample_async);
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

/**
 * cd_sensor_huey_get_sample_finish:
 **/
static gboolean
cd_sensor_huey_get_sample_finish (CdSensor *sensor,
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
 * cd_sensor_huey_send_unlock:
 **/
static gboolean
cd_sensor_huey_send_unlock (CdSensorHuey *sensor_huey, GError **error)
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
	ret = cd_sensor_huey_send_data (sensor_huey,
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
	CdSensorHuey *sensor_huey = CD_SENSOR_HUEY (sensor);
	CdSensorHueyPrivate *priv = sensor_huey->priv;
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
	ret = cd_sensor_huey_send_unlock (sensor_huey, &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}

	/* get serial number */
	ret = cd_sensor_huey_read_register_word (sensor_huey,
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
	ret = cd_sensor_huey_read_register_string (sensor_huey,
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
	ret = cd_sensor_huey_read_register_matrix (sensor_huey,
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
	ret = cd_sensor_huey_read_register_matrix (sensor_huey,
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
	ret = cd_sensor_huey_read_register_float (sensor_huey,
						  CD_SENSOR_HUEY_EEPROM_ADDR_AMBIENT_CALIB_VALUE,
						  &priv->calibration_value,
						  &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}

	/* this vector changes between sensor 1 and 3 */
	ret = cd_sensor_huey_read_register_vector (sensor_huey,
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

/**
 * cd_sensor_huey_lock_async:
 **/
static void
cd_sensor_huey_lock_async (CdSensor *sensor,
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
					 cd_sensor_huey_lock_async);
	g_simple_async_result_run_in_thread (res,
					     cd_sensor_huey_lock_thread_cb,
					     0,
					     cancellable);
	g_object_unref (res);
}

/**
 * cd_sensor_huey_lock_finish:
 **/
static gboolean
cd_sensor_huey_lock_finish (CdSensor *sensor,
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

/**
 * cd_sensor_huey_unlock_thread_cb:
 **/
static void
cd_sensor_huey_unlock_thread_cb (GSimpleAsyncResult *res,
				 GObject *object,
				 GCancellable *cancellable)
{
	CdSensorHuey *sensor_huey = CD_SENSOR_HUEY (object);
	gboolean ret = FALSE;
	GError *error = NULL;

	/* connect */
	ret = cd_usb_disconnect (sensor_huey->priv->usb,
			         &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}
out:
	return;
}

/**
 * cd_sensor_huey_unlock_async:
 **/
static void
cd_sensor_huey_unlock_async (CdSensor *sensor,
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
					 cd_sensor_huey_unlock_async);
	g_simple_async_result_run_in_thread (res,
					     cd_sensor_huey_unlock_thread_cb,
					     0,
					     cancellable);
	g_object_unref (res);
}

/**
 * cd_sensor_huey_unlock_finish:
 **/
static gboolean
cd_sensor_huey_unlock_finish (CdSensor *sensor,
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

/**
 * cd_sensor_huey_dump:
 **/
static gboolean
cd_sensor_huey_dump (CdSensor *sensor, GString *data, GError **error)
{
	CdSensorHuey *sensor_huey = CD_SENSOR_HUEY (sensor);
	CdSensorHueyPrivate *priv = sensor_huey->priv;
	gboolean ret;
	guint i;
	guint8 value;

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

	/* read all the register space */
	for (i=0; i<0xff; i++) {
		ret = cd_sensor_huey_read_register_byte (sensor_huey,
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

/**
 * cd_sensor_huey_class_init:
 **/
static void
cd_sensor_huey_class_init (CdSensorHueyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CdSensorClass *parent_class = CD_SENSOR_CLASS (klass);
	object_class->finalize = cd_sensor_huey_finalize;

	/* setup klass links */
	parent_class->get_sample_async = cd_sensor_huey_get_sample_async;
	parent_class->get_sample_finish = cd_sensor_huey_get_sample_finish;
	parent_class->lock_async = cd_sensor_huey_lock_async;
	parent_class->lock_finish = cd_sensor_huey_lock_finish;
	parent_class->unlock_async = cd_sensor_huey_unlock_async;
	parent_class->unlock_finish = cd_sensor_huey_unlock_finish;
	parent_class->dump = cd_sensor_huey_dump;

	g_type_class_add_private (klass, sizeof (CdSensorHueyPrivate));
}

/**
 * cd_sensor_huey_init:
 **/
static void
cd_sensor_huey_init (CdSensorHuey *sensor)
{
	CdSensorHueyPrivate *priv;
	priv = sensor->priv = CD_SENSOR_HUEY_GET_PRIVATE (sensor);
	priv->usb = cd_usb_new ();
	cd_mat33_clear (&priv->calibration_lcd);
	cd_mat33_clear (&priv->calibration_crt);
	priv->unlock_string[0] = '\0';
}

/**
 * cd_sensor_huey_finalize:
 **/
static void
cd_sensor_huey_finalize (GObject *object)
{
	CdSensorHuey *sensor = CD_SENSOR_HUEY (object);
	CdSensorHueyPrivate *priv = sensor->priv;

	g_object_unref (priv->usb);

	G_OBJECT_CLASS (cd_sensor_huey_parent_class)->finalize (object);
}

/**
 * cd_sensor_huey_new:
 *
 * Return value: a new #CdSensor object.
 **/
CdSensor *
cd_sensor_huey_new (void)
{
	CdSensorHuey *sensor;
	sensor = g_object_new (CD_TYPE_SENSOR_HUEY,
			       "native", TRUE,
			       "kind", CD_SENSOR_KIND_HUEY,
			       NULL);
	return CD_SENSOR (sensor);
}

