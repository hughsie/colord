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
 * This object contains all the low level logic for the MUNKI hardware.
 */

#include "config.h"

#include <glib-object.h>
#include <libusb-1.0/libusb.h>

#include "cd-buffer.h"
#include "cd-usb.h"
#include "cd-sensor.h"
#include "cd-sensor-munki-private.h"

typedef struct
{
	gboolean			 done_startup;
	CdUsb				*usb;
	struct libusb_transfer		*transfer_interrupt;
	struct libusb_transfer		*transfer_state;
	gchar				*version_string;
	gchar				*chip_id;
	gchar				*firmware_revision;
	guint32				 tick_duration;
	guint32				 min_int;
	guint32				 eeprom_blocks;
	guint32				 eeprom_blocksize;
} CdSensorMunkiPrivate;

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

#define CD_SENSOR_MUNKI_VENDOR_ID				0x0971
#define CD_SENSOR_MUNKI_PRODUCT_ID				0x2007

static void
cd_sensor_munki_submit_transfer (CdSensor *sensor);

static CdSensorMunkiPrivate *
cd_sensor_munki_get_private (CdSensor *sensor)
{
	return g_object_get_data (G_OBJECT (sensor), "priv");
}

static void
cd_sensor_munki_print_data (const gchar *title,
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
 * cd_sensor_munki_refresh_state_transfer_cb:
 **/
static void
cd_sensor_munki_refresh_state_transfer_cb (struct libusb_transfer *transfer)
{
	CdSensor *sensor = CD_SENSOR (transfer->user_data);
	guint8 *reply = transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		g_warning ("did not succeed");
		goto out;
	}

	/* sensor position and button state */
	switch (reply[0]) {
	case CD_SENSOR_MUNKI_DIAL_POSITION_PROJECTOR:
		cd_sensor_set_mode (sensor, CD_SENSOR_CAP_PROJECTOR);
		break;
	case CD_SENSOR_MUNKI_DIAL_POSITION_SURFACE:
		cd_sensor_set_mode (sensor, CD_SENSOR_CAP_PRINTER);
		break;
	case CD_SENSOR_MUNKI_DIAL_POSITION_CALIBRATION:
		cd_sensor_set_mode (sensor, CD_SENSOR_CAP_CALIBRATION);
		break;
	case CD_SENSOR_MUNKI_DIAL_POSITION_AMBIENT:
		cd_sensor_set_mode (sensor, CD_SENSOR_CAP_AMBIENT);
		break;
	case CD_SENSOR_MUNKI_DIAL_POSITION_UNKNOWN:
		cd_sensor_set_mode (sensor, CD_SENSOR_CAP_UNKNOWN);
		break;
	default:
		break;
	}

	g_debug ("dial now %s, button now %s",
		 cd_sensor_cap_to_string (cd_sensor_get_mode (sensor)),
		 cd_sensor_munki_button_state_to_string (reply[1]));

	cd_sensor_munki_print_data ("reply", transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE, transfer->actual_length);
out:
	g_free (transfer->buffer);
}

/**
 * cd_sensor_munki_refresh_state:
 **/
static gboolean
cd_sensor_munki_refresh_state (CdSensor *sensor, GError **error)
{
	gint retval;
	gboolean ret = FALSE;
	static guchar *request;
	libusb_device_handle *handle;
	CdSensorMunkiPrivate *priv = cd_sensor_munki_get_private (sensor);

	/* do sync request */
	handle = cd_usb_get_device_handle (priv->usb);

	/* request new button state */
	request = g_new0 (guchar, LIBUSB_CONTROL_SETUP_SIZE + 2);
	libusb_fill_control_setup (request,
				   LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
				   CD_SENSOR_MUNKI_REQUEST_GET_STATUS,
				   0x00, 0, 2);
	libusb_fill_control_transfer (priv->transfer_state, handle, request,
				      &cd_sensor_munki_refresh_state_transfer_cb,
				      sensor, 2000);

	/* submit transfer */
	retval = libusb_submit_transfer (priv->transfer_state);
	if (retval < 0) {
		g_set_error (error, 1, 0,
			     "failed to submit transfer: %s",
			     libusb_strerror (retval));
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cd_sensor_munki_transfer_cb:
 **/
static void
cd_sensor_munki_transfer_cb (struct libusb_transfer *transfer)
{
	guint32 timestamp;
	CdSensor *sensor = CD_SENSOR (transfer->user_data);
	guint8 *reply = transfer->buffer;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		g_warning ("did not succeed");
		return;
	}

	cd_sensor_munki_print_data ("reply", transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE, transfer->actual_length);
	timestamp = (reply[7] << 24) + (reply[6] << 16) + (reply[5] << 8) + (reply[4] << 0);
	/* we only care when the button is pressed */
	if (reply[0] == CD_SENSOR_MUNKI_COMMAND_BUTTON_RELEASED) {
		g_debug ("ignoring button released");
		goto out;
	}

	if (reply[0] == CD_SENSOR_MUNKI_COMMAND_DIAL_ROTATE) {
		g_warning ("dial rotate at %ims", timestamp);
	} else if (reply[0] == CD_SENSOR_MUNKI_COMMAND_BUTTON_PRESSED) {
		g_debug ("button pressed at %ims", timestamp);
		cd_sensor_button_pressed (sensor);
	}

	/* get the device state */
	cd_sensor_munki_refresh_state (sensor, NULL);

out:
	/* get the next bit of data */
	g_free (transfer->buffer);
	cd_sensor_munki_submit_transfer (sensor);
}

/**
 * cd_sensor_munki_submit_transfer:
 **/
static void
cd_sensor_munki_submit_transfer (CdSensor *sensor)
{
	gint retval;
	guchar *reply;
	libusb_device_handle *handle;
	CdSensorMunkiPrivate *priv = cd_sensor_munki_get_private (sensor);

	reply = g_new0 (guchar, 8);
	handle = cd_usb_get_device_handle (priv->usb);
	libusb_fill_interrupt_transfer (priv->transfer_interrupt, handle,
					CD_SENSOR_MUNKI_REQUEST_INTERRUPT,
					reply, 8,
					cd_sensor_munki_transfer_cb,
					sensor, -1);

	g_debug ("submitting transfer");
	retval = libusb_submit_transfer (priv->transfer_interrupt);
	if (retval < 0)
		g_warning ("failed to submit transfer: %s", libusb_strerror (retval));
}

/**
 * cd_sensor_munki_get_eeprom_data:
 **/
static gboolean
cd_sensor_munki_get_eeprom_data (CdSensor *sensor,
				 guint32 address, guchar *data,
				 guint32 size, GError **error)
{
	gint retval;
	libusb_device_handle *handle;
	guchar request[8];
	gsize reply_read = 0;
	gboolean ret = FALSE;
	CdSensorMunkiPrivate *priv = cd_sensor_munki_get_private (sensor);

	/* do EEPROM request */
	g_debug ("get EEPROM at 0x%04x for %i", address, size);
	cd_buffer_write_uint32_le (request, address);
	cd_buffer_write_uint32_le (request + 4, size);
	cd_sensor_munki_print_data ("request", request, 8);
	handle = cd_usb_get_device_handle (priv->usb);
	retval = libusb_control_transfer (handle,
					  LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					  CD_SENSOR_MUNKI_REQUEST_EEPROM_DATA,
					  0, 0, request, 8, 2000);
	if (retval < 0) {
		g_set_error (error, CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_NO_SUPPORT,
			     "failed to request eeprom: %s",
			     libusb_strerror (retval));
		goto out;
	}

	/* read EEPROM */
	retval = libusb_bulk_transfer (handle,
				       CD_SENSOR_MUNKI_REQUEST_EEPROM_DATA,
				       data, (gint) size, (gint*)&reply_read,
				       5000);
	if (retval < 0) {
		g_set_error (error, CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_NO_SUPPORT,
			     "failed to get eeprom data: %s",
			     libusb_strerror (retval));
		goto out;
	}
	if (reply_read != size) {
		g_set_error_literal (error, CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_NO_SUPPORT,
				     "did not get the correct number of bytes");
		goto out;
	}
	cd_sensor_munki_print_data ("reply", data, size);

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cd_sensor_munki_random:
 **/
static gboolean
cd_sensor_munki_random (CdSensor *sensor, GError **error)
{
	gboolean ret = FALSE;
//	CdSensorMunkiPrivate *priv = cd_sensor_munki_get_private (sensor);

	g_debug ("submit transfer");
	cd_sensor_munki_submit_transfer (sensor);
	ret = cd_sensor_munki_refresh_state (sensor, error);
	if (!ret)
		goto out;
out:
	return ret;
}

static void
cd_sensor_munki_get_sample_state_finish (CdSensorAsyncState *state,
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
cd_sensor_munki_cancellable_cancel_cb (GCancellable *cancellable,
				      CdSensorAsyncState *state)
{
	g_warning ("cancelled munki");
}

static void
cd_sensor_munki_get_ambient_thread_cb (GSimpleAsyncResult *res,
				      GObject *object,
				      GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (object);
	GError *error = NULL;
//	CdSensorMunkiPrivate *priv = cd_sensor_munki_get_private (sensor);
	CdSensorAsyncState *state = (CdSensorAsyncState *) g_object_get_data (G_OBJECT (cancellable), "state");

	/* no hardware support */
	if (cd_sensor_get_mode (sensor) != CD_SENSOR_CAP_AMBIENT) {
		g_set_error_literal (&error, CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_NO_SUPPORT,
				     "Cannot measure ambient light in this mode (turn dial!)");
		cd_sensor_munki_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

/*
 * ioctl(3, USBDEVFS_SUBMITURB or USBDEVFS_SUBMITURB32, {type=3, endpoint=129, status=0, flags=0, buffer_length=1096, actual_length=0, start_frame=0, number_of_packets=0, error_count=0, signr=0, usercontext=(nil), buffer=00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ) = 0
 * ioctl(3, USBDEVFS_CONTROL or USBDEVFS_CONTROL32, {requesttype=64, request=128, value=0, index=0, length=12, timeout=2000, data=00 00 01 00 b7 3e 00 00 02 00 00 00 ) = 12
 * 
 * ioctl(3, USBDEVFS_SUBMITURB or USBDEVFS_SUBMITURB32, {type=3, endpoint=129, status=0, flags=0, buffer_length=548, actual_length=0, start_frame=0, number_of_packets=0, error_count=0, signr=0, usercontext=(nil), buffer=d0 a3 9d 00 d0 a3 9d 00 00 00 00 00 00 00 00 00 00 d0 86 40 bf c6 fa 21 a4 4b 61 40 0b 24 0c d6 7a 29 04 40 91 3a 0e c7 f9 28 04 40 c0 b1 55 bc 9b 28 04 40 b9 d3 41 53 86 6a 07 40 df 23 db 4d 0c e3 06 40 20 5c bf 4d b2 53 05 40 5f 28 38 74 26 44 07 40 e9 45 b7 e4 2f a5 08 40 bb a2 87 d7 8c db 07 40 34 90 30 b1 f3 a1 06 40 b0 8f fa 63 84 98 05 40 35 1f 09 07 97 47 04 40 53 ac 8a be ) = 0
 * 
 * ioctl(3, USBDEVFS_REAPURBNDELAY or USBDEVFS_REAPURBNDELAY32, {type=3, endpoint=129, status=0, flags=0, buffer_length=548, actual_length=548, start_frame=0, number_of_packets=0, error_count=0, signr=0, usercontext=(nil), buffer=de 07 da 07 d6 07 d8 07 d6 07 16 08 29 0b 79 0d 22 12 f2 17 b4 1c 31 20 4b 22 e2 22 7b 22 a8 21 93 20 eb 1e 2d 1d fe 1b 1c 1b e5 19 69 19 c8 19 b5 19 8a 18 16 17 a4 15 86 14 ac 13 e8 12 22 12 20 12 bf 12 8e 13 d2 13 de 13 ea 13 fb 13 39 14 89 14 bd 14 ec 14 8b 15 78 16 69 17 99 18 ca 19 97 1a 14 1b 6f 1b b5 1b 7f 1c 98 1d 59 1e a9 1e af 1e 71 1e d2 1d db 1c c1 1b d4 1a 50 1a 46 1a }) = 0
 * write(1, " Result is XYZ: 126.685284 136.9"..., 91 Result is XYZ: 126.685284 136.946975 206.789116, D50 Lab: 112.817679 -7.615524 -49.589593
) = 91
* write(1, " Ambient = 430.2 Lux, CCT = 1115"..., 54 Ambient = 430.2 Lux, CCT = 11152K (Delta E 9.399372)
 */

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_MEASURING);

	/* save result */
	state->ret = TRUE;
	cd_sensor_munki_get_sample_state_finish (state, NULL);
out:
	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
}

static void
cd_sensor_munki_sample_thread_cb (GSimpleAsyncResult *res,
				 GObject *object,
				 GCancellable *cancellable)
{
	GError *error = NULL;
	CdSensor *sensor = CD_SENSOR (object);
//	CdSensorMunkiPrivate *priv = cd_sensor_munki_get_private (sensor);
	CdSensorAsyncState *state = (CdSensorAsyncState *) g_object_get_data (G_OBJECT (cancellable), "state");

	/* no hardware support */
	if (state->current_cap == CD_SENSOR_CAP_PROJECTOR) {
		g_set_error_literal (&error, CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_NO_SUPPORT,
				     "MUNKI cannot measure in projector mode");
		cd_sensor_munki_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_MEASURING);

	/* save result */
	state->ret = TRUE;
	state->sample = cd_color_xyz_new ();
	cd_sensor_munki_get_sample_state_finish (state, NULL);
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
		state->cancellable_id = g_cancellable_connect (cancellable, G_CALLBACK (cd_sensor_munki_cancellable_cancel_cb), state, NULL);
	}

	/* run in a thread */
	tmp = g_cancellable_new ();
	g_object_set_data (G_OBJECT (tmp), "state", state);
	if (cap == CD_SENSOR_CAP_AMBIENT) {
		g_simple_async_result_run_in_thread (G_SIMPLE_ASYNC_RESULT (state->res),
						     cd_sensor_munki_get_ambient_thread_cb,
						     0,
						     (GCancellable*) tmp);
	} else if (cap == CD_SENSOR_CAP_LCD ||
		   cap == CD_SENSOR_CAP_CRT) {
		g_simple_async_result_run_in_thread (G_SIMPLE_ASYNC_RESULT (state->res),
						     cd_sensor_munki_sample_thread_cb,
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
cd_sensor_munki_lock_thread_cb (GSimpleAsyncResult *res,
			       GObject *object,
			       GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorMunkiPrivate *priv = cd_sensor_munki_get_private (sensor);
	gboolean ret = FALSE;
	GError *error = NULL;
	gint retval;
	guchar buffer[36];
	libusb_device_handle *handle;

	/* connect */
	ret = cd_usb_connect (priv->usb,
			       CD_SENSOR_MUNKI_VENDOR_ID,
			       CD_SENSOR_MUNKI_PRODUCT_ID,
			       0x01, 0x00, &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}

	/* attach to the default mainloop */
	ret = cd_usb_attach_to_context (priv->usb, NULL, &error);
	if (!ret) {
		g_simple_async_result_set_error (res, CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_NO_SUPPORT,
						 "failed to attach to mainloop: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* get firmware parameters */
	handle = cd_usb_get_device_handle (priv->usb);
	retval = libusb_control_transfer (handle,
					  LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					  CD_SENSOR_MUNKI_REQUEST_FIRMWARE_PARAMS,
					  0, 0, buffer, 24, 2000);
	if (retval < 0) {
		g_simple_async_result_set_error (res, CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_NO_SUPPORT,
						 "failed to get firmware parameters: %s",
						 libusb_strerror (retval));
		goto out;
	}
	priv->firmware_revision = g_strdup_printf ("%i.%i",
						   cd_buffer_read_uint32_le (buffer),
						   cd_buffer_read_uint32_le (buffer+4));
	priv->tick_duration = cd_buffer_read_uint32_le (buffer+8);
	priv->min_int = cd_buffer_read_uint32_le (buffer+0x0c);
	priv->eeprom_blocks = cd_buffer_read_uint32_le (buffer+0x10);
	priv->eeprom_blocksize = cd_buffer_read_uint32_le (buffer+0x14);

	/* get chip ID */
	retval = libusb_control_transfer (handle,
					  LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					  CD_SENSOR_MUNKI_REQUEST_CHIP_ID,
					  0, 0, buffer, 8, 2000);
	if (retval < 0) {
		g_simple_async_result_set_error (res, CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_NO_SUPPORT,
						 "failed to get chip id parameters: %s",
						 libusb_strerror (retval));
		goto out;
	}
	priv->chip_id = g_strdup_printf ("%02x-%02x%02x%02x%02x%02x%02x%02x",
					 buffer[0], buffer[1], buffer[2], buffer[3],
					 buffer[4], buffer[5], buffer[6], buffer[7]);

	/* get version string */
	priv->version_string = g_new0 (gchar, 36);
	retval = libusb_control_transfer (handle,
					  LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					  CD_SENSOR_MUNKI_REQUEST_VERSION_STRING,
					  0, 0, (guchar*) priv->version_string, 36, 2000);
	if (retval < 0) {
		g_simple_async_result_set_error (res, CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_NO_SUPPORT,
						 "failed to get version string: %s",
						 libusb_strerror (retval));
		goto out;
	}

	/* get serial number */
	ret = cd_sensor_munki_get_eeprom_data (sensor,
					       COLORMUNKI_EEPROM_OFFSET_SERIAL_NUMBER,
					       buffer, 10, &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}
	cd_sensor_set_serial (sensor, (const gchar*) buffer);

	/* print details */
	g_debug ("Chip ID\t%s", priv->chip_id);
	g_debug ("Serial number\t%s", buffer);
	g_debug ("Version\t%s", priv->version_string);
	g_debug ("Firmware\tfirmware_revision=%s, tick_duration=%i, min_int=%i, eeprom_blocks=%i, eeprom_blocksize=%i",
		   priv->firmware_revision, priv->tick_duration, priv->min_int, priv->eeprom_blocks, priv->eeprom_blocksize);

	/* do unknown cool stuff */
	ret = cd_sensor_munki_random (sensor, &error);
	if (!ret) {
g_assert (error != NULL);
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}
out:
	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
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
					     cd_sensor_munki_lock_thread_cb,
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
	CdSensorMunkiPrivate *priv = cd_sensor_munki_get_private (sensor);
	gboolean ret = FALSE;
	GError *error = NULL;

	/* stop watching the dial */
	libusb_cancel_transfer (priv->transfer_interrupt);
	libusb_cancel_transfer (priv->transfer_state);

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
	guchar *buffer;
	guint i, j;
	gboolean ret = TRUE;
	CdSensorMunkiPrivate *priv = cd_sensor_munki_get_private (sensor);

	/* dump the unlock string */
	g_string_append_printf (data, "colormunki-dump-version: %i\n", 1);
	g_string_append_printf (data, "chip-id:%s\n", priv->chip_id);
	g_string_append_printf (data, "version:%s\n", priv->version_string);
	g_string_append_printf (data, "firmware-revision:%s\n", priv->firmware_revision);
	g_string_append_printf (data, "tick-duration:%i\n", priv->tick_duration);
	g_string_append_printf (data, "min-int:%i\n", priv->min_int);
	g_string_append_printf (data, "eeprom-blocks:%i\n", priv->eeprom_blocks);
	g_string_append_printf (data, "eeprom-blocksize:%i\n", priv->eeprom_blocksize);

	/* allocate a big chunk o' memory */
	buffer = g_new0 (guchar, priv->eeprom_blocksize);

	/* get all banks of EEPROM */
	for (i=0; i<priv->eeprom_blocks; i++) {
		ret = cd_sensor_munki_get_eeprom_data (sensor,
						       i*priv->eeprom_blocksize,
						       buffer, priv->eeprom_blocksize,
						       error);
		if (!ret)
			goto out;

		/* write details */
		for (j=0; j<priv->eeprom_blocksize; j++)
			g_string_append_printf (data, "eeprom[0x%04x]:0x%02x\n", (i*priv->eeprom_blocksize) + j, buffer[j]);
	}
out:
	g_free (buffer);
	return ret;
}

static void
cd_sensor_unref_private (CdSensorMunkiPrivate *priv)
{
	/* FIXME: cancel transfers if in progress */

	/* detach from the main loop */
	g_object_unref (priv->usb);

	/* free transfers */
	libusb_free_transfer (priv->transfer_interrupt);
	libusb_free_transfer (priv->transfer_state);
	g_free (priv->version_string);
	g_free (priv->chip_id);
	g_free (priv->firmware_revision);

	g_free (priv);
}

gboolean
cd_sensor_coldplug (CdSensor *sensor, GError **error)
{
	CdSensorMunkiPrivate *priv;
	g_object_set (sensor,
		      "native", TRUE,
		      "kind", CD_SENSOR_KIND_COLOR_MUNKI,
		      NULL);
	/* create private data */
	priv = g_new0 (CdSensorMunkiPrivate, 1);
	g_object_set_data_full (G_OBJECT (sensor), "priv", priv,
				(GDestroyNotify) cd_sensor_unref_private);
	priv->usb = cd_usb_new ();
	priv->transfer_interrupt = libusb_alloc_transfer (0);
	priv->transfer_state = libusb_alloc_transfer (0);
	return TRUE;
}

