/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2016 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <glib.h>
#include <gusb.h>
#include <string.h>
#include <lcms2.h>

#include "ch-common.h"
#include "ch-device.h"
#include "ch-math.h"

/**
 * ch_device_error_quark:
 *
 * Return value: ChDevice error quark.
 *
 * Since: 0.1.1
 **/
GQuark
ch_device_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("ch_device_error");
	return quark;
}

/**
 * ch_device_open:
 *
 * Since: 0.1.29
 **/
gboolean
ch_device_open (GUsbDevice *device, GError **error)
{
	return ch_device_open_full (device, NULL, error);
}

/**
 * ch_device_close:
 *
 * Since: 1.2.11
 **/
gboolean
ch_device_close (GUsbDevice *device, GError **error)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* unload device */
	if (!g_usb_device_release_interface (device,
					     CH_USB_INTERFACE,
					     G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
					     error))
		return FALSE;
	if (!g_usb_device_close (device, error))
		return FALSE;
	return TRUE;
}

/**
 * ch_device_is_colorhug:
 *
 * Since: 0.1.29
 **/
gboolean
ch_device_is_colorhug (GUsbDevice *device)
{
	return ch_device_get_mode (device) != CH_DEVICE_MODE_UNKNOWN;
}

/**
 * ch_device_get_mode:
 *
 * Since: 0.1.29
 **/
ChDeviceMode
ch_device_get_mode (GUsbDevice *device)
{
	ChDeviceMode state;

	/* is a legacy device */
	if (g_usb_device_get_vid (device) == CH_USB_VID_LEGACY &&
	    g_usb_device_get_pid (device) == CH_USB_PID_LEGACY) {
		return CH_DEVICE_MODE_LEGACY;
	}

	/* vendor doesn't match */
	if (g_usb_device_get_vid (device) != CH_USB_VID)
		return CH_DEVICE_MODE_UNKNOWN;

	/* use the product ID to work out the state */
	switch (g_usb_device_get_pid (device)) {
	case CH_USB_PID_BOOTLOADER:
		state = CH_DEVICE_MODE_BOOTLOADER;
		break;
	case CH_USB_PID_BOOTLOADER2:
		state = CH_DEVICE_MODE_BOOTLOADER2;
		break;
	case CH_USB_PID_BOOTLOADER_PLUS:
		state = CH_DEVICE_MODE_BOOTLOADER_PLUS;
		break;
	case CH_USB_PID_BOOTLOADER_ALS:
		state = CH_DEVICE_MODE_BOOTLOADER_ALS;
		break;
	case CH_USB_PID_FIRMWARE:
		state = CH_DEVICE_MODE_FIRMWARE;
		break;
	case CH_USB_PID_FIRMWARE2:
		state = CH_DEVICE_MODE_FIRMWARE2;
		break;
	case CH_USB_PID_FIRMWARE_PLUS:
		state = CH_DEVICE_MODE_FIRMWARE_PLUS;
		break;
	case CH_USB_PID_FIRMWARE_ALS:
	case CH_USB_PID_FIRMWARE_ALS_SENSOR_HID:
		state = CH_DEVICE_MODE_FIRMWARE_ALS;
		break;
	default:
		state = CH_DEVICE_MODE_UNKNOWN;
		break;
	}
	return state;
}

/**
 * ch_print_data_buffer:
 **/
static void
ch_print_data_buffer (const gchar *title,
		      const guint8 *data,
		      gsize length)
{
	guint i;

	if (g_strcmp0 (title, "request") == 0)
		g_print ("%c[%dm", 0x1B, 31);
	if (g_strcmp0 (title, "reply") == 0)
		g_print ("%c[%dm", 0x1B, 34);
	g_print ("%s\t", title);

	for (i = 0; i < length; i++)
		g_print ("%02x [%c]\t", data[i], g_ascii_isprint (data[i]) ? data[i] : '?');

	g_print ("%c[%dm\n", 0x1B, 0);
}

typedef struct {
	guint8			*buffer;
	guint8			*buffer_orig;
	guint8			*buffer_out;
	gsize			 buffer_out_len;
	guint8			 cmd;
	guint			 retried_cnt;
	guint8			 report_type;	/* only for Sensor HID */
	guint			 report_length;	/* only for Sensor HID */
} ChDeviceTaskData;

/**
 * ch_device_write_command_finish:
 * @device: a #GUsbDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: %TRUE if the request was fulfilled.
 *
 * Since: 0.1.29
 **/
gboolean
ch_device_write_command_finish (GUsbDevice *device,
				GAsyncResult *res,
				GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, device), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

/**
 * ch_device_task_data_free:
 **/
static void
ch_device_task_data_free (ChDeviceTaskData *tdata)
{
	g_free (tdata->buffer);
	g_free (tdata->buffer_orig);
	g_free (tdata);
}

static void ch_device_request_cb (GObject *source_object, GAsyncResult *res, gpointer user_data);

/**
 * ch_device_reply_cb:
 **/
static void
ch_device_reply_cb (GObject *source_object,
		    GAsyncResult *res,
		    gpointer user_data)
{
	ChError error_enum;
	GError *error = NULL;
	gsize actual_len;
	gchar *msg = NULL;
	GUsbDevice *device = G_USB_DEVICE (source_object);
	GTask *task = G_TASK (user_data);
	ChDeviceTaskData *tdata = g_task_get_task_data (task);

	/* get the result */
	actual_len = g_usb_device_interrupt_transfer_finish (device,
							     res,
							     &error);
	if ((gssize) actual_len < 0) {
		g_task_return_new_error (task,
					 CH_DEVICE_ERROR,
					 CH_ERROR_INVALID_VALUE,
					 "%s", error->message);
		g_object_unref (task);
		return;
	}

	/* parse the reply */
	if (g_getenv ("COLORHUG_VERBOSE") != NULL) {
		ch_print_data_buffer ("reply",
				      tdata->buffer,
				      actual_len);
	}

	/* parse */
	if (tdata->buffer[CH_BUFFER_OUTPUT_RETVAL] != CH_ERROR_NONE ||
	    tdata->buffer[CH_BUFFER_OUTPUT_CMD] != tdata->cmd ||
	    (actual_len != tdata->buffer_out_len + CH_BUFFER_OUTPUT_DATA &&
	     actual_len != CH_USB_HID_EP_SIZE)) {
		error_enum = tdata->buffer[CH_BUFFER_OUTPUT_RETVAL];

		/* handle incomplete previous request */
		if (error_enum == CH_ERROR_INCOMPLETE_REQUEST &&
		    tdata->retried_cnt == 0) {
			tdata->retried_cnt++;
			memcpy (tdata->buffer, tdata->buffer_orig, CH_USB_HID_EP_SIZE);
			if (g_getenv ("COLORHUG_VERBOSE") != NULL) {
				ch_print_data_buffer ("request",
						      tdata->buffer,
						      CH_USB_HID_EP_SIZE);
			}
			g_usb_device_interrupt_transfer_async (device,
							       CH_USB_HID_EP_OUT,
							       tdata->buffer,
							       CH_USB_HID_EP_SIZE,
							       CH_DEVICE_USB_TIMEOUT,
							       g_task_get_cancellable (task),
							       ch_device_request_cb,
							       tdata);
			/* we're re-using the tdata, so don't deallocate it */
			return;
		}

		msg = g_strdup_printf ("Invalid read: retval=0x%02x [%s] "
				       "cmd=0x%02x [%s] (expected 0x%x [%s]) "
				       "len=%" G_GSIZE_FORMAT " (expected %" G_GSIZE_FORMAT " or %i)",
				       error_enum,
				       ch_strerror (error_enum),
				       tdata->buffer[CH_BUFFER_OUTPUT_CMD],
				       ch_command_to_string (tdata->buffer[CH_BUFFER_OUTPUT_CMD]),
				       tdata->cmd,
				       ch_command_to_string (tdata->cmd),
				       actual_len,
				       tdata->buffer_out_len + CH_BUFFER_OUTPUT_DATA,
				       CH_USB_HID_EP_SIZE);
		g_task_return_new_error (task,
					 CH_DEVICE_ERROR,
					 error_enum,
					 "%s", msg);
		g_object_unref (task);
		return;
	}

	/* copy */
	if (tdata->buffer_out != NULL) {
		memcpy (tdata->buffer_out,
			tdata->buffer + CH_BUFFER_OUTPUT_DATA,
			tdata->buffer_out_len);
	}

	/* success */
	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

/**
 * ch_device_request_cb:
 **/
static void
ch_device_request_cb (GObject *source_object,
		      GAsyncResult *res,
		      gpointer user_data)
{
	GError *error = NULL;
	gssize actual_len;
	GUsbDevice *device = G_USB_DEVICE (source_object);
	GTask *task = G_TASK (user_data);
	ChDeviceTaskData *tdata = g_task_get_task_data (task);

	/* get the result */
	actual_len = g_usb_device_interrupt_transfer_finish (device,
							     res,
							     &error);
	if (actual_len < CH_USB_HID_EP_SIZE) {
		g_task_return_new_error (task,
					 CH_DEVICE_ERROR,
					 CH_ERROR_INVALID_VALUE,
					 "%s", error->message);
		g_object_unref (task);
		return;
	}

	/* request the reply */
	g_usb_device_interrupt_transfer_async (device,
					       CH_USB_HID_EP_IN,
					       tdata->buffer,
					       CH_USB_HID_EP_SIZE,
					       CH_DEVICE_USB_TIMEOUT,
					       g_task_get_cancellable (task),
					       ch_device_reply_cb,
					       task);
}

/**
 * ch_device_emulate_cb:
 **/
static gboolean
ch_device_emulate_cb (gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	ChDeviceTaskData *tdata = g_task_get_task_data (task);

	switch (tdata->cmd) {
	case CH_CMD_GET_SERIAL_NUMBER:
		tdata->buffer_out[6] = 42;
		break;
	case CH_CMD_GET_FIRMWARE_VERSION:
		tdata->buffer_out[0] = 0x01;
		tdata->buffer_out[4] = 0x01;
		break;
	case CH_CMD_GET_HARDWARE_VERSION:
		tdata->buffer_out[0] = 0xff;
		break;
	default:
		g_debug ("Ignoring command %s",
			 ch_command_to_string (tdata->cmd));
		break;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
	g_object_unref (task);

	return G_SOURCE_REMOVE;
}

#define CH_REPORT_ALS				0x00
#define CH_REPORT_HID_SENSOR			0x01
#define CH_REPORT_SENSOR_SETTINGS		0x02
#define CH_REPORT_SYSTEM_SETTINGS		0x03
#define CH_SENSOR_HID_REPORT_GET		0x01
#define CH_SENSOR_HID_REPORT_SET		0x09
#define CH_SENSOR_HID_FEATURE			0x0300

/**
 * ch_device_sensor_hid_set_cb:
 **/
static void
ch_device_sensor_hid_set_cb (GObject *source_object,
			     GAsyncResult *res,
			     gpointer user_data)
{
	GError *error = NULL;
	gssize actual_len;
	GUsbDevice *device = G_USB_DEVICE (source_object);
	GTask *task = G_TASK (user_data);
	ChDeviceTaskData *tdata = g_task_get_task_data (task);

	/* get the result */
	actual_len = g_usb_device_control_transfer_finish (device,
							   res,
							   &error);
	if (actual_len != tdata->report_length) {
		g_task_return_new_error (task,
					 CH_DEVICE_ERROR,
					 CH_ERROR_INVALID_VALUE,
					 "%s", error->message);
		g_object_unref (task);
		return;
	}
//	ch_print_data_buffer ("reply", tdata->buffer, tdata->report_length);

	/* success */
	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

/**
 * ch_device_sensor_hid_report_cb:
 **/
static void
ch_device_sensor_hid_report_cb (GObject *source_object,
				GAsyncResult *res,
				gpointer user_data)
{
	GError *error = NULL;
	gsize actual_len;
	GUsbDevice *device = G_USB_DEVICE (source_object);
	GTask *task = G_TASK (user_data);
	ChDeviceTaskData *tdata = g_task_get_task_data (task);

	/* get the result */
	actual_len = g_usb_device_interrupt_transfer_finish (device,
							     res,
							     &error);
	if ((gssize) actual_len < 0) {
		g_task_return_new_error (task,
					 CH_DEVICE_ERROR,
					 CH_ERROR_INVALID_VALUE,
					 "%s", error->message);
		g_object_unref (task);
		return;
	}

	/* copy out tdata */
	memcpy (tdata->buffer_out, tdata->buffer + 3, 4);

	/* success */
	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

/**
 * ch_device_sensor_hid_get_cb:
 **/
static void
ch_device_sensor_hid_get_cb (GObject *source_object,
			     GAsyncResult *res,
			     gpointer user_data)
{
	GError *error = NULL;
	gssize actual_len;
	gboolean another_request_required = FALSE;
	GUsbDevice *device = G_USB_DEVICE (source_object);
	GTask *task = G_TASK (user_data);
	ChDeviceTaskData *tdata = g_task_get_task_data (task);

	/* get the result */
	actual_len = g_usb_device_control_transfer_finish (device,
							   res,
							   &error);
	if (actual_len != tdata->report_length) {
		g_task_return_new_error (task,
					 CH_DEVICE_ERROR,
					 CH_ERROR_INVALID_VALUE,
					 "%s", error->message);
		g_object_unref (task);
		return;
	}
//	ch_print_data_buffer ("reply", tdata->buffer, tdata->report_length);

	switch (tdata->cmd) {
	case CH_CMD_TAKE_READING_RAW:
		memcpy (tdata->buffer_out, tdata->buffer + 3, 4);
		break;
	case CH_CMD_GET_COLOR_SELECT:
		memcpy (tdata->buffer_out, tdata->buffer + 1, 1);
		break;
	case CH_CMD_GET_INTEGRAL_TIME:
		memcpy (tdata->buffer_out, tdata->buffer + 4, 2);
		break;
	case CH_CMD_GET_LEDS:
		memcpy (tdata->buffer_out, tdata->buffer + 2, 1);
		break;
	case CH_CMD_GET_MULTIPLIER:
		memcpy (tdata->buffer_out, tdata->buffer + 3, 1);
		break;
	case CH_CMD_GET_FIRMWARE_VERSION:
		memcpy (tdata->buffer_out, tdata->buffer + 2, 6);
		break;
	case CH_CMD_GET_HARDWARE_VERSION:
		memcpy (tdata->buffer_out, tdata->buffer + 1, 1);
		break;
	case CH_CMD_GET_SERIAL_NUMBER:
		memcpy (tdata->buffer_out, tdata->buffer + 8, 4);
		break;
	case CH_CMD_SET_COLOR_SELECT:
		memcpy (tdata->buffer + 1, tdata->buffer_orig + 1, 1);
		another_request_required = TRUE;
		break;
	case CH_CMD_SET_INTEGRAL_TIME:
		memcpy (tdata->buffer + 4, tdata->buffer_orig + 1, 2);
		another_request_required = TRUE;
		break;
	case CH_CMD_SET_LEDS:
		memcpy (tdata->buffer + 2, tdata->buffer_orig + 1, 1);
		another_request_required = TRUE;
		break;
	case CH_CMD_SET_MULTIPLIER:
		memcpy (tdata->buffer + 3, tdata->buffer_orig + 1, 1);
		another_request_required = TRUE;
		break;
	case CH_CMD_SET_FLASH_SUCCESS:
		memcpy (tdata->buffer + 13, tdata->buffer_orig + 1, 1);
		another_request_required = TRUE;
		break;
	case CH_CMD_RESET:
		tdata->buffer[12] = 1;
		another_request_required = TRUE;
		break;
	case CH_CMD_SET_SERIAL_NUMBER:
		memcpy (tdata->buffer + 8, tdata->buffer_orig + 1, 4);
		another_request_required = TRUE;
		break;
	default:
		g_task_return_new_error (task,
					 CH_DEVICE_ERROR,
					 CH_ERROR_UNKNOWN_CMD,
					 "No Sensor HID support for 0x%02x",
					 tdata->cmd);
		g_object_unref (task);
		return;
	}

	/* getting the value was enough */
	if (!another_request_required) {
		g_task_return_boolean (task, TRUE);
		g_object_unref (task);
		return;
	}

//	ch_print_data_buffer ("request", tdata->buffer, tdata->report_length);
	g_usb_device_control_transfer_async (device,
					     G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
					     G_USB_DEVICE_REQUEST_TYPE_CLASS,
					     G_USB_DEVICE_RECIPIENT_INTERFACE,
					     CH_SENSOR_HID_REPORT_SET,
					     CH_SENSOR_HID_FEATURE | tdata->report_type,
					     0x0000,
					     tdata->buffer,
					     tdata->report_length,
					     CH_DEVICE_USB_TIMEOUT,
					     g_task_get_cancellable (task),
					     ch_device_sensor_hid_set_cb,
					     task);
}

/**
 * ch_device_write_command_async:
 * @device:		A #GUsbDevice
 * @cmd:		The command to use, e.g. %CH_CMD_GET_COLOR_SELECT
 * @buffer_in:		The input buffer of data, or %NULL
 * @buffer_in_len:	The input buffer length
 * @buffer_out:		The output buffer of data, or %NULL
 * @buffer_out_len:	The output buffer length
 * @cancellable:	A #GCancellable, or %NULL
 * @callback:		A #GAsyncReadyCallback that will be called when finished.
 * @user_data:		User data passed to @callback
 *
 * Sends a message to the device and waits for a reply.
 *
 * Since: 0.1.29
 **/
void
ch_device_write_command_async (GUsbDevice *device,
			       guint8 cmd,
			       const guint8 *buffer_in,
			       gsize buffer_in_len,
			       guint8 *buffer_out,
			       gsize buffer_out_len,
			       GCancellable *cancellable,
			       GAsyncReadyCallback callback,
			       gpointer user_data)
{
	ChDeviceTaskData *tdata;
	GTask *task = NULL;
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (cmd != 0);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (device, cancellable, callback, user_data);

	tdata = g_new0 (ChDeviceTaskData, 1);
	tdata->buffer_out = buffer_out;
	tdata->buffer_out_len = buffer_out_len;
	tdata->buffer = g_new0 (guint8, CH_USB_HID_EP_SIZE);
	g_task_set_task_data (task, tdata, (GDestroyNotify) ch_device_task_data_free);

	/* set command */
	tdata->cmd = cmd;
	tdata->buffer[CH_BUFFER_INPUT_CMD] = tdata->cmd;
	if (buffer_in != NULL) {
		memcpy (tdata->buffer + CH_BUFFER_INPUT_DATA,
			buffer_in,
			buffer_in_len);
	}
	tdata->buffer_orig = g_memdup (tdata->buffer, CH_USB_HID_EP_SIZE);

	/* request */
	if (g_getenv ("COLORHUG_VERBOSE") != NULL) {
		ch_print_data_buffer ("request",
				      tdata->buffer,
				      buffer_in_len + 1);
	}

	/* dummy hardware */
	if (g_getenv ("COLORHUG_EMULATE") != NULL) {
		g_timeout_add (20, ch_device_emulate_cb, tdata);
		return;
	}

	/* handle ALS in sensor-hid mode differently */
	if (g_usb_device_get_pid (device) == CH_USB_PID_FIRMWARE_ALS_SENSOR_HID) {

		/* try to map the commands to Sensor HID requests */
		switch (tdata->cmd) {
		case CH_CMD_TAKE_READING_RAW:
			tdata->report_type = CH_REPORT_ALS;
			tdata->report_length = 7;
			break;
		case CH_CMD_GET_COLOR_SELECT:
		case CH_CMD_GET_INTEGRAL_TIME:
		case CH_CMD_GET_LEDS:
		case CH_CMD_GET_MULTIPLIER:
		case CH_CMD_SET_COLOR_SELECT:
		case CH_CMD_SET_INTEGRAL_TIME:
		case CH_CMD_SET_LEDS:
		case CH_CMD_SET_MULTIPLIER:
			tdata->report_type = CH_REPORT_SENSOR_SETTINGS;
			tdata->report_length = 6;
			break;
		case CH_CMD_GET_FIRMWARE_VERSION:
		case CH_CMD_GET_HARDWARE_VERSION:
		case CH_CMD_GET_SERIAL_NUMBER:
		case CH_CMD_RESET:
		case CH_CMD_SET_FLASH_SUCCESS:
		case CH_CMD_SET_SERIAL_NUMBER:
			tdata->report_type = CH_REPORT_SYSTEM_SETTINGS;
			tdata->report_length = 14;
			break;
		default:
			g_task_return_new_error (task,
						 CH_DEVICE_ERROR,
						 CH_ERROR_UNKNOWN_CMD,
						 "No Sensor HID support for 0x%02x",
						 tdata->cmd);
			g_object_unref (task);
			return;
		}

		/* need to use this rather than feature control */
		if (tdata->report_type == CH_REPORT_ALS) {
			g_usb_device_interrupt_transfer_async (device,
							       CH_USB_HID_EP_IN,
							       tdata->buffer,
							       tdata->report_length,
							       CH_DEVICE_USB_TIMEOUT,
							       g_task_get_cancellable (task),
							       ch_device_sensor_hid_report_cb,
							       task);
			return;
		}

		/* do control transfer */
		memset(tdata->buffer, '\0', tdata->report_length);
//		ch_print_data_buffer ("request", tdata->buffer, tdata->report_length);
		g_usb_device_control_transfer_async (device,
						     G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
						     G_USB_DEVICE_REQUEST_TYPE_CLASS,
						     G_USB_DEVICE_RECIPIENT_INTERFACE,
						     CH_SENSOR_HID_REPORT_GET,
						     CH_SENSOR_HID_FEATURE | tdata->report_type,
						     0x0000,
						     tdata->buffer,
						     tdata->report_length,
						     CH_DEVICE_USB_TIMEOUT,
						     g_task_get_cancellable (task),
						     ch_device_sensor_hid_get_cb,
						     task);
		return;
	}

	/* do interrupt transfer */
	g_usb_device_interrupt_transfer_async (device,
					       CH_USB_HID_EP_OUT,
					       tdata->buffer,
					       CH_USB_HID_EP_SIZE,
					       CH_DEVICE_USB_TIMEOUT,
					       g_task_get_cancellable (task),
					       ch_device_request_cb,
					       task);
}

/* tiny helper to help us do the async operation */
typedef struct {
	GError		**error;
	GMainLoop	*loop;
	gboolean	 ret;
} ChDeviceSyncHelper;

/**
 * ch_device_write_command_finish_cb:
 **/
static void
ch_device_write_command_finish_cb (GObject *source,
				   GAsyncResult *res,
				   gpointer user_data)
{
	GUsbDevice *device = G_USB_DEVICE (source);
	ChDeviceSyncHelper *helper = (ChDeviceSyncHelper *) user_data;
	helper->ret = ch_device_write_command_finish (device, res, helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * ch_device_write_command:
 * @device:		A #GUsbDevice
 * @cmd:		The command to use, e.g. %CH_CMD_GET_COLOR_SELECT
 * @buffer_in:		The input buffer of data, or %NULL
 * @buffer_in_len:	The input buffer length
 * @buffer_out:		The output buffer of data, or %NULL
 * @buffer_out_len:	The output buffer length
 * @cancellable:	A #GCancellable or %NULL
 * @error:		A #GError, or %NULL
 *
 * Sends a message to the device and waits for a reply.
 *
 * Return value: %TRUE if the command was executed successfully.
 *
 * Since: 0.1.29
 **/
gboolean
ch_device_write_command (GUsbDevice *device,
			 guint8 cmd,
			 const guint8 *buffer_in,
			 gsize buffer_in_len,
			 guint8 *buffer_out,
			 gsize buffer_out_len,
			 GCancellable *cancellable,
			 GError **error)
{
	ChDeviceSyncHelper helper;

	/* create temp object */
	helper.ret = FALSE;
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	ch_device_write_command_async (device,
				       cmd,
				       buffer_in,
				       buffer_in_len,
				       buffer_out,
				       buffer_out_len,
				       cancellable,
				       ch_device_write_command_finish_cb,
				       &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**
 * ch_device_check_firmware:
 * @data: firmware binary data
 * @data_len: size of @data
 *
 * Checks the firmware is suitable for the ColorHug device that is attached.
 *
 * Return value: %TRUE if the command was executed successfully.
 *
 * Since: 1.2.3
 **/
gboolean
ch_device_check_firmware (GUsbDevice *device,
			  const guint8 *data,
			  gsize data_len,
			  GError **error)
{
	ChDeviceMode device_mode_fw;

	/* this is only a heuristic */
	device_mode_fw = ch_device_mode_from_firmware (data, data_len);
	switch (ch_device_get_mode (device)) {
	case CH_DEVICE_MODE_LEGACY:
	case CH_DEVICE_MODE_BOOTLOADER:
	case CH_DEVICE_MODE_FIRMWARE:
		/* fw versions < 1.2.2 has no magic bytes */
		if (device_mode_fw == CH_DEVICE_MODE_FIRMWARE2 ||
		    device_mode_fw == CH_DEVICE_MODE_FIRMWARE_ALS ||
		    device_mode_fw == CH_DEVICE_MODE_FIRMWARE_PLUS) {
			g_set_error (error,
				     CH_DEVICE_ERROR,
				     CH_ERROR_INVALID_VALUE,
				     "This firmware is not designed for "
				     "ColorHug (identifier is '%s')",
				     ch_device_mode_to_string (device_mode_fw));
			return FALSE;
		}
		break;
	case CH_DEVICE_MODE_BOOTLOADER2:
	case CH_DEVICE_MODE_FIRMWARE2:
		if (device_mode_fw != CH_DEVICE_MODE_FIRMWARE2) {
			g_set_error (error,
				     CH_DEVICE_ERROR,
				     CH_ERROR_INVALID_VALUE,
				     "This firmware is not designed for "
				     "ColorHug2 (identifier is '%s')",
				     ch_device_mode_to_string (device_mode_fw));
			return FALSE;
		}
		break;
	case CH_DEVICE_MODE_BOOTLOADER_PLUS:
	case CH_DEVICE_MODE_FIRMWARE_PLUS:
		if (device_mode_fw != CH_DEVICE_MODE_FIRMWARE_PLUS) {
			g_set_error (error,
				     CH_DEVICE_ERROR,
				     CH_ERROR_INVALID_VALUE,
				     "This firmware is not designed for "
				     "ColorHug+ (identifier is '%s')",
				     ch_device_mode_to_string (device_mode_fw));
			return FALSE;
		}
		break;
	case CH_DEVICE_MODE_BOOTLOADER_ALS:
	case CH_DEVICE_MODE_FIRMWARE_ALS:
		if (device_mode_fw != CH_DEVICE_MODE_FIRMWARE_ALS) {
			g_set_error (error,
				     CH_DEVICE_ERROR,
				     CH_ERROR_INVALID_VALUE,
				     "This firmware is not designed for "
				     "ColorHug ALS (identifier is '%s')",
				     ch_device_mode_to_string (device_mode_fw));
			return FALSE;
		}
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	return TRUE;
}

/**
 * ch_device_get_runcode_address:
 * @device:		A #GUsbDevice
 *
 * Returns the runcode address for the ColorHug device.
 *
 * Return value: the runcode address, or 0 for error
 *
 * Since: 1.2.9
 **/
guint16
ch_device_get_runcode_address (GUsbDevice *device)
{
	switch (ch_device_get_mode (device)) {
	case CH_DEVICE_MODE_LEGACY:
	case CH_DEVICE_MODE_BOOTLOADER:
	case CH_DEVICE_MODE_FIRMWARE:
	case CH_DEVICE_MODE_BOOTLOADER2:
	case CH_DEVICE_MODE_FIRMWARE2:
	case CH_DEVICE_MODE_BOOTLOADER_PLUS:
	case CH_DEVICE_MODE_FIRMWARE_PLUS:
		return CH_EEPROM_ADDR_RUNCODE;
	case CH_DEVICE_MODE_FIRMWARE_ALS:
	case CH_DEVICE_MODE_BOOTLOADER_ALS:
		return CH_EEPROM_ADDR_RUNCODE_ALS;
	default:
		break;
	}
	return 0;
}

/**
 * ch_device_get_guid:
 * @device: A #GUsbDevice
 *
 * Returns the GUID for the connected ColorHug device.
 *
 * Return value: the GUID address, or %NULL for error
 *
 * Since: 1.2.9
 **/
const gchar *
ch_device_get_guid (GUsbDevice *device)
{
	ChDeviceMode mode = ch_device_get_mode (device);
	if (mode == CH_DEVICE_MODE_LEGACY ||
	    mode == CH_DEVICE_MODE_FIRMWARE ||
	    mode == CH_DEVICE_MODE_BOOTLOADER)
		return CH_DEVICE_GUID_COLORHUG;
	if (mode == CH_DEVICE_MODE_FIRMWARE2 ||
	    mode == CH_DEVICE_MODE_BOOTLOADER2)
		return CH_DEVICE_GUID_COLORHUG2;
	if (mode == CH_DEVICE_MODE_FIRMWARE_PLUS ||
	    mode == CH_DEVICE_MODE_BOOTLOADER_PLUS)
		return CH_DEVICE_GUID_COLORHUG_PLUS;
	if (mode == CH_DEVICE_MODE_FIRMWARE_ALS ||
	    mode == CH_DEVICE_MODE_BOOTLOADER_ALS)
		return CH_DEVICE_GUID_COLORHUG_ALS;
	return NULL;
}

/**
 * ch_device_get_protocol_ver:
 **/
static guint8
ch_device_get_protocol_ver (GUsbDevice *device)
{
	switch (ch_device_get_mode (device)) {
	case CH_DEVICE_MODE_BOOTLOADER:
	case CH_DEVICE_MODE_BOOTLOADER2:
	case CH_DEVICE_MODE_BOOTLOADER_ALS:
	case CH_DEVICE_MODE_FIRMWARE:
	case CH_DEVICE_MODE_FIRMWARE2:
	case CH_DEVICE_MODE_FIRMWARE_ALS:
	case CH_DEVICE_MODE_LEGACY:
		return 0x1;
	case CH_DEVICE_MODE_FIRMWARE_PLUS:
		return 0x2;
	default:
		break;
	}
	return 0x0;
}

/**
 * ch_offset_float_to_double:
 **/
static gdouble
ch_offset_float_to_double (gint32 tmp)
{
	return (gdouble) tmp / (gdouble) 0xffff;
}

/**
 * ch_offset_float_from_double:
 **/
static gint32
ch_offset_float_from_double (gdouble tmp)
{
	return tmp * 0xffff;
}

/**
 * ch_device_check_status:
 **/
static gboolean
ch_device_check_status (GUsbDevice *device, GCancellable *cancellable, GError **error)
{
	ChError status;
	ChCmd cmd;

	/* hit hardware */
	if (!ch_device_get_error (device, &status, &cmd, cancellable, error))
		return FALSE;

	/* formulate error */
	if (status != CH_ERROR_NONE) {
		g_set_error (error,
			     G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_IO,
			     "Failed, %s(0x%02x) status was %s(0x%02x)",
			     ch_command_to_string (cmd), cmd,
			     ch_strerror (status), status);
		return FALSE;
	}
	return TRUE;
}

/**
 * ch_device_self_test:
 * @device: A #GUsbDevice
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Performs a self test on the device
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_self_test (GUsbDevice *device,
		     GCancellable *cancellable,
		     GError **error)
{
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	if (protocol_ver == 1) {
		return ch_device_write_command (device,
						CH_CMD_SELF_TEST,
						NULL,
						0,
						NULL,
						0,
						cancellable,
						error);
	}
	if (protocol_ver == 2) {
		return g_usb_device_control_transfer (device,
						      G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
						      G_USB_DEVICE_REQUEST_TYPE_CLASS,
						      G_USB_DEVICE_RECIPIENT_INTERFACE,
						      CH_CMD_SELF_TEST,
						      0,		/* wValue */
						      CH_USB_INTERFACE,	/* idx */
						      NULL,		/* data */
						      0,		/* length */
						      NULL,		/* actual_length */
						      CH_DEVICE_USB_TIMEOUT,
						      cancellable,
						      error);
	}
	g_set_error_literal (error,
			     CH_DEVICE_ERROR,
			     CH_ERROR_NOT_IMPLEMENTED,
			     "Self test is not supported");
	return FALSE;
}

/**
 * ch_device_set_serial_number:
 * @device: A #GUsbDevice
 * @value: serial number
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Sets the serial number on the device
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_set_serial_number (GUsbDevice *device, guint32 value,
			     GCancellable *cancellable, GError **error)
{
	guint32 value_le;
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	value_le = GUINT32_TO_LE (value);
	if (protocol_ver == 1) {
		return ch_device_write_command (device,
						CH_CMD_SET_SERIAL_NUMBER,
						(const guint8 *) &value_le,
						sizeof(value_le),
						NULL,
						0,
						cancellable,
						error);
	}
	if (protocol_ver == 2) {
		return g_usb_device_control_transfer (device,
						      G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
						      G_USB_DEVICE_REQUEST_TYPE_CLASS,
						      G_USB_DEVICE_RECIPIENT_INTERFACE,
						      CH_CMD_SET_SERIAL_NUMBER,
						      value_le,		/* wValue */
						      CH_USB_INTERFACE,	/* idx */
						      NULL,		/* data */
						      0,		/* length */
						      NULL,		/* actual_length */
						      CH_DEVICE_USB_TIMEOUT,
						      cancellable,
						      error);
	}
	g_set_error_literal (error,
			     CH_DEVICE_ERROR,
			     CH_ERROR_NOT_IMPLEMENTED,
			     "Setting the serial number is not supported");
	return FALSE;
}

/**
 * ch_device_get_serial_number:
 * @device: A #GUsbDevice
 * @value: (out): serial number
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Gets the serial number from the device.
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_get_serial_number (GUsbDevice *device, guint32 *value,
			     GCancellable *cancellable, GError **error)
{
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	g_return_val_if_fail (G_USB_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (protocol_ver == 1) {
		return ch_device_write_command (device,
						CH_CMD_GET_SERIAL_NUMBER,
						NULL,
						0,
						(guint8 *) value,
						sizeof(guint32),
						cancellable,
						error);
	}
	if (protocol_ver == 2) {
		guint8 buf[2];
		gsize actual_length;
		gboolean ret;
		ret = g_usb_device_control_transfer (device,
						     G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
						     G_USB_DEVICE_REQUEST_TYPE_CLASS,
						     G_USB_DEVICE_RECIPIENT_INTERFACE,
						     CH_CMD_GET_SERIAL_NUMBER,
						     0x00,		/* wValue */
						     CH_USB_INTERFACE,	/* idx */
						     buf,		/* data */
						     sizeof(buf),	/* length */
						     &actual_length,
						     CH_DEVICE_USB_TIMEOUT,
						     cancellable,
						     error);
		if (!ret)
			return FALSE;

		/* return result */
		if (actual_length != sizeof(buf)) {
			g_set_error (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_IO,
				     "Invalid size, got %li", actual_length);
			return FALSE;
		}
		if (value != NULL)
			memcpy (value, buf, sizeof(buf));
		return TRUE;
	}
	g_set_error_literal (error,
			     CH_DEVICE_ERROR,
			     CH_ERROR_NOT_IMPLEMENTED,
			     "Getting the serial number is not supported");
	return FALSE;
}

/**
 * ch_device_set_leds:
 * @device: A #GUsbDevice
 * @value: serial number
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Sets the LEDs on the device
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_set_leds (GUsbDevice *device, ChStatusLed value,
		    GCancellable *cancellable, GError **error)
{
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	if (protocol_ver == 1) {
		guint8 buffer[4] = { value, 0, 0, 0 };
		return ch_device_write_command (device,
						CH_CMD_SET_LEDS,
						(const guint8 *) buffer,
						sizeof(buffer),
						NULL,
						0,
						cancellable,
						error);
	}
	if (protocol_ver == 2) {
		return g_usb_device_control_transfer (device,
						      G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
						      G_USB_DEVICE_REQUEST_TYPE_CLASS,
						      G_USB_DEVICE_RECIPIENT_INTERFACE,
						      CH_CMD_SET_LEDS,
						      value,		/* wValue */
						      CH_USB_INTERFACE,	/* idx */
						      NULL,		/* data */
						      0,		/* length */
						      NULL,		/* actual_length */
						      CH_DEVICE_USB_TIMEOUT,
						      cancellable,
						      error);
	}
	g_set_error_literal (error,
			     CH_DEVICE_ERROR,
			     CH_ERROR_NOT_IMPLEMENTED,
			     "Setting the LEDs is not supported");
	return FALSE;
}

/**
 * ch_device_get_leds:
 * @device: A #GUsbDevice
 * @value: (out): serial number
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Gets the LEDs from the device.
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_get_leds (GUsbDevice *device, ChStatusLed *value,
		    GCancellable *cancellable, GError **error)
{
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	g_return_val_if_fail (G_USB_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (protocol_ver == 1) {
		return ch_device_write_command (device,
						CH_CMD_GET_LEDS,
						NULL,
						0,
						(guint8 *) value,
						sizeof(ChStatusLed),
						cancellable,
						error);
	}
	if (protocol_ver == 2) {
		guint8 buf[1];
		gsize actual_length;
		gboolean ret;
		ret = g_usb_device_control_transfer (device,
						     G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
						     G_USB_DEVICE_REQUEST_TYPE_CLASS,
						     G_USB_DEVICE_RECIPIENT_INTERFACE,
						     CH_CMD_GET_LEDS,
						     0x00,		/* wValue */
						     CH_USB_INTERFACE,	/* idx */
						     buf,		/* data */
						     sizeof(buf),	/* length */
						     &actual_length,
						     CH_DEVICE_USB_TIMEOUT,
						     cancellable,
						     error);
		if (!ret)
			return FALSE;

		/* return result */
		if (actual_length != sizeof(buf)) {
			g_set_error (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_IO,
				     "Invalid size, got %li", actual_length);
			return FALSE;
		}
		if (value != NULL)
			memcpy (value, buf, sizeof(buf));
		return TRUE;
	}
	g_set_error_literal (error,
			     CH_DEVICE_ERROR,
			     CH_ERROR_NOT_IMPLEMENTED,
			     "Getting the LEDs is not supported");
	return FALSE;
}

/**
 * ch_device_set_pcb_errata:
 * @device: A #GUsbDevice
 * @value: #ChPcbErrata, e.g. %CH_PCB_ERRATA_SWAPPED_LEDS
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Sets any PCB errata on the device
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_set_pcb_errata (GUsbDevice *device, ChPcbErrata value,
			  GCancellable *cancellable, GError **error)
{
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	g_return_val_if_fail (G_USB_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (protocol_ver == 1) {
		gboolean ret;
		ret = ch_device_write_command (device,
						CH_CMD_SET_PCB_ERRATA,
						(guint8 *) &value,
						sizeof(guint8),
						NULL,
						0,
						cancellable,
						error);
		if (!ret)
			return FALSE;
		return ch_device_write_command (device,
						CH_CMD_WRITE_EEPROM,
						(guint8 *) CH_WRITE_EEPROM_MAGIC,
						strlen (CH_WRITE_EEPROM_MAGIC),
						NULL,
						0,
						cancellable,
						error);
	}
	if (protocol_ver == 2) {
		return g_usb_device_control_transfer (device,
						      G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
						      G_USB_DEVICE_REQUEST_TYPE_CLASS,
						      G_USB_DEVICE_RECIPIENT_INTERFACE,
						      CH_CMD_SET_PCB_ERRATA,
						      value,		/* wValue */
						      CH_USB_INTERFACE,	/* idx */
						      NULL,		/* data */
						      0,		/* length */
						      NULL,		/* actual_length */
						      CH_DEVICE_USB_TIMEOUT,
						      cancellable,
						      error);
	}
	g_set_error_literal (error,
			     CH_DEVICE_ERROR,
			     CH_ERROR_NOT_IMPLEMENTED,
			     "Setting the PCB errata is not supported");
	return FALSE;
}

/**
 * ch_device_get_pcb_errata:
 * @device: A #GUsbDevice
 * @value: (out): #ChPcbErrata, e.g. %CH_PCB_ERRATA_SWAPPED_LEDS
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Gets any PCB errata from the device.
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_get_pcb_errata (GUsbDevice *device, ChPcbErrata *value,
			  GCancellable *cancellable, GError **error)
{
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	g_return_val_if_fail (G_USB_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (protocol_ver == 1) {
		return ch_device_write_command (device,
						CH_CMD_GET_PCB_ERRATA,
						NULL,
						0,
						NULL,
						0,
						cancellable,
						error);
	}
	if (protocol_ver == 2) {
		guint8 buf[1];
		gsize actual_length;
		gboolean ret;
		ret = g_usb_device_control_transfer (device,
						     G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
						     G_USB_DEVICE_REQUEST_TYPE_CLASS,
						     G_USB_DEVICE_RECIPIENT_INTERFACE,
						     CH_CMD_GET_PCB_ERRATA,
						     0x00,		/* wValue */
						     CH_USB_INTERFACE,	/* idx */
						     buf,		/* data */
						     sizeof(buf),	/* length */
						     &actual_length,
						     CH_DEVICE_USB_TIMEOUT,
						     cancellable,
						     error);
		if (!ret)
			return FALSE;

		/* return result */
		if (actual_length != sizeof(buf)) {
			g_set_error (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_IO,
				     "Invalid size, got %li", actual_length);
			return FALSE;
		}
		if (value != NULL)
			*value = buf[0];
		return TRUE;
	}
	g_set_error_literal (error,
			     CH_DEVICE_ERROR,
			     CH_ERROR_NOT_IMPLEMENTED,
			     "Getting the PCB errata is not supported");
	return FALSE;
}

/**
 * ch_device_set_ccd_calibration:
 * @device: A #GUsbDevice
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Sets any PCB wavelength_cal on the device
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_set_ccd_calibration (GUsbDevice *device,
			       gdouble start_nm,
			       gdouble c0,
			       gdouble c1,
			       gdouble c2,
			       GCancellable *cancellable,
			       GError **error)
{
	gboolean ret;
	gint32 buf[4];
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	g_return_val_if_fail (G_USB_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (protocol_ver != 2) {
		g_set_error_literal (error,
				     CH_DEVICE_ERROR,
				     CH_ERROR_NOT_IMPLEMENTED,
				     "Setting the CCD calibration is not supported");
		return FALSE;
	}

	/* format data */
	buf[0] = ch_offset_float_from_double (start_nm);
	buf[1] = ch_offset_float_from_double (c0);
	buf[2] = ch_offset_float_from_double (c1) * 1000.f;
	buf[3] = ch_offset_float_from_double (c2) * 1000.f;

	/* hit hardware */
	ret = g_usb_device_control_transfer (device,
					     G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
					     G_USB_DEVICE_REQUEST_TYPE_CLASS,
					     G_USB_DEVICE_RECIPIENT_INTERFACE,
					     CH_CMD_SET_CCD_CALIBRATION,
					     0,			/* wValue */
					     CH_USB_INTERFACE,	/* idx */
					     (guint8 *) buf,	/* data */
					     sizeof(buf),	/* length */
					     NULL,		/* actual_length */
					     CH_DEVICE_USB_TIMEOUT,
					     cancellable,
					     error);
	if (!ret)
		return FALSE;

	/* check status */
	return ch_device_check_status (device, cancellable, error);
}

/**
 * ch_device_set_crypto_key:
 * @device: A #GUsbDevice
 * @keys: a set of XTEA keys
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Sets the firmware signing keys on the device.
 *
 * IMPORTANT: This can only be called once until the device is unlocked.
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_set_crypto_key (GUsbDevice *device,
			  guint32 keys[4],
			  GCancellable *cancellable,
			  GError **error)
{
	gboolean ret;
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	g_return_val_if_fail (G_USB_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (protocol_ver != 2) {
		g_set_error_literal (error,
				     CH_DEVICE_ERROR,
				     CH_ERROR_NOT_IMPLEMENTED,
				     "Setting the crypto key is not supported");
		return FALSE;
	}

	/* hit hardware */
	ret = g_usb_device_control_transfer (device,
					     G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
					     G_USB_DEVICE_REQUEST_TYPE_CLASS,
					     G_USB_DEVICE_RECIPIENT_INTERFACE,
					     CH_CMD_SET_CRYPTO_KEY,
					     0,			/* wValue */
					     CH_USB_INTERFACE,	/* idx */
					     (guint8 *) keys,	/* data */
					     sizeof(guint32) * 4, /* length */
					     NULL,		/* actual_length */
					     CH_DEVICE_USB_TIMEOUT,
					     cancellable,
					     error);
	if (!ret)
		return FALSE;

	/* check status */
	return ch_device_check_status (device, cancellable, error);
}

/**
 * ch_device_get_ccd_calibration:
 * @device: A #GUsbDevice
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Gets any PCB wavelength_cal from the device.
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_get_ccd_calibration (GUsbDevice *device,
			       gdouble *start_nm,
			       gdouble *c0,
			       gdouble *c1,
			       gdouble *c2,
			       GCancellable *cancellable,
			       GError **error)
{
	gboolean ret;
	gint32 buf[4];
	gsize actual_length;
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	g_return_val_if_fail (G_USB_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (protocol_ver != 2) {
		g_set_error_literal (error,
				     CH_DEVICE_ERROR,
				     CH_ERROR_NOT_IMPLEMENTED,
				     "Getting the CCD calibration is not supported");
		return FALSE;
	}

	/* hit hardware */
	ret = g_usb_device_control_transfer (device,
					     G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
					     G_USB_DEVICE_REQUEST_TYPE_CLASS,
					     G_USB_DEVICE_RECIPIENT_INTERFACE,
					     CH_CMD_GET_CCD_CALIBRATION,
					     0x00,		/* wValue */
					     CH_USB_INTERFACE,	/* idx */
					     (guint8 *) buf,	/* data */
					     sizeof(buf),	/* length */
					     &actual_length,
					     CH_DEVICE_USB_TIMEOUT,
					     cancellable,
					     error);
	if (!ret)
		return FALSE;

	/* return result */
	if (actual_length != sizeof(buf)) {
		g_set_error (error,
			     G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_IO,
			     "Invalid size, got %li", actual_length);
		return FALSE;
	}
	if (start_nm != NULL) {
		*start_nm = ch_offset_float_to_double (buf[0]);
		*c0 = ch_offset_float_to_double (buf[1]);
		*c1 = ch_offset_float_to_double (buf[2]) / 1000.f;
		*c2 = ch_offset_float_to_double (buf[3]) / 1000.f;
	}

	/* check status */
	return ch_device_check_status (device, cancellable, error);
}

/**
 * ch_device_set_integral_time:
 * @device: A #GUsbDevice
 * @value: integration time in ms
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Sets the integration value for the next sample.
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_set_integral_time (GUsbDevice *device, guint16 value,
			     GCancellable *cancellable, GError **error)
{
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	g_return_val_if_fail (G_USB_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (protocol_ver == 1) {
		return ch_device_write_command (device,
						CH_CMD_SET_INTEGRAL_TIME,
						(guint8 *) &value,
						sizeof(guint16),
						NULL,
						0,
						cancellable,
						error);
	}
	if (protocol_ver == 2) {
		gboolean ret;
		ret = g_usb_device_control_transfer (device,
						     G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
						     G_USB_DEVICE_REQUEST_TYPE_CLASS,
						     G_USB_DEVICE_RECIPIENT_INTERFACE,
						     CH_CMD_SET_INTEGRAL_TIME,
						     value,		/* wValue */
						     CH_USB_INTERFACE,	/* idx */
						     NULL,		/* data */
						     0,			/* length */
						     NULL,		/* actual_length */
						     CH_DEVICE_USB_TIMEOUT,
						     cancellable,
						     error);
		return ret;
	}
	g_set_error_literal (error,
			     CH_DEVICE_ERROR,
			     CH_ERROR_NOT_IMPLEMENTED,
			     "Setting the integral time is not supported");
	return FALSE;
}

/**
 * ch_device_get_integral_time:
 * @device: A #GUsbDevice
 * @value: (out): integration time in ms
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Gets the integration time used for taking the next samples.
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_get_integral_time (GUsbDevice *device, guint16 *value,
			     GCancellable *cancellable, GError **error)
{
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	g_return_val_if_fail (G_USB_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (protocol_ver == 1) {
		return ch_device_write_command (device,
						CH_CMD_GET_INTEGRAL_TIME,
						NULL,
						0,
						(guint8 *) value,
						sizeof(guint16),
						cancellable,
						error);
	}
	if (protocol_ver == 2) {
		guint8 buf[2];
		gsize actual_length;
		gboolean ret;
		ret = g_usb_device_control_transfer (device,
						     G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
						     G_USB_DEVICE_REQUEST_TYPE_CLASS,
						     G_USB_DEVICE_RECIPIENT_INTERFACE,
						     CH_CMD_GET_INTEGRAL_TIME,
						     0x00,		/* wValue */
						     CH_USB_INTERFACE,	/* idx */
						     buf,		/* data */
						     sizeof(buf),	/* length */
						     &actual_length,
						     CH_DEVICE_USB_TIMEOUT,
						     cancellable,
						     error);
		if (!ret)
			return FALSE;

		/* return result */
		if (actual_length != sizeof(buf)) {
			g_set_error (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_IO,
				     "Invalid size, got %li", actual_length);
			return FALSE;
		}
		if (value != NULL)
			memcpy (value, buf, sizeof(buf));
		return TRUE;
	}
	g_set_error_literal (error,
			     CH_DEVICE_ERROR,
			     CH_ERROR_NOT_IMPLEMENTED,
			     "Getting the integral time is not supported");
	return FALSE;
}

/**
 * ch_device_get_temperature:
 * @device: A #GUsbDevice
 * @value: (out): temperature in Celcius
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Gets the PCB board temperature from the device.
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_get_temperature (GUsbDevice *device, gdouble *value,
			   GCancellable *cancellable, GError **error)
{
	gboolean ret;
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	g_return_val_if_fail (G_USB_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (protocol_ver == 1) {
		ChPackedFloat pf;
		ret = ch_device_write_command (device,
						CH_CMD_GET_TEMPERATURE,
						NULL,
						0,
						(guint8 *) &pf,
						sizeof(ChPackedFloat),
						cancellable,
						error);
		if (!ret)
			return FALSE;
		ch_packed_float_to_double (&pf, value);
		return TRUE;
	}
	if (protocol_ver == 2) {
		gint32 buf[1];
		gsize actual_length;
		ret = g_usb_device_control_transfer (device,
						     G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
						     G_USB_DEVICE_REQUEST_TYPE_CLASS,
						     G_USB_DEVICE_RECIPIENT_INTERFACE,
						     CH_CMD_GET_TEMPERATURE,
						     0x00,		/* wValue */
						     CH_USB_INTERFACE,	/* idx */
						     (guint8 *) buf,	/* data */
						     sizeof(buf),	/* length */
						     &actual_length,
						     CH_DEVICE_USB_TIMEOUT,
						     cancellable,
						     error);
		if (!ret)
			return FALSE;

		/* return result */
		if (actual_length != sizeof(buf)) {
			g_set_error (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_IO,
				     "Invalid size, got %li", actual_length);
			return FALSE;
		}
		if (value != NULL)
			*value = ch_offset_float_to_double (buf[0]);
		return TRUE;
	}
	g_set_error_literal (error,
			     CH_DEVICE_ERROR,
			     CH_ERROR_NOT_IMPLEMENTED,
			     "Getting the temperature is not supported");
	return FALSE;
}

/**
 * ch_device_get_error:
 * @device: A #GUsbDevice
 * @status: (out): a #ChError, e.g. %CH_ERROR_INVALID_ADDRESS
 * @cmd: (out): a #ChCmd, e.g. %CH_CMD_TAKE_READING_SPECTRAL
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Gets the status for the last operation.
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_get_error (GUsbDevice *device, ChError *status, ChCmd *cmd,
		     GCancellable *cancellable, GError **error)
{
	guint8 buf[2];
	gsize actual_length;
	gboolean ret;
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	g_return_val_if_fail (G_USB_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (protocol_ver != 2) {
		g_set_error_literal (error,
				     CH_DEVICE_ERROR,
				     CH_ERROR_NOT_IMPLEMENTED,
				     "Getting the last error is not supported");
		return FALSE;
	}
	ret = g_usb_device_control_transfer (device,
					     G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
					     G_USB_DEVICE_REQUEST_TYPE_CLASS,
					     G_USB_DEVICE_RECIPIENT_INTERFACE,
					     CH_CMD_GET_ERROR,
					     0x00,		/* wValue */
					     CH_USB_INTERFACE,	/* idx */
					     buf,		/* data */
					     sizeof(buf),	/* length */
					     &actual_length,
					     CH_DEVICE_USB_TIMEOUT,
					     cancellable,
					     error);
	if (!ret)
		return FALSE;

	/* return result */
	if (actual_length != sizeof(buf)) {
		g_set_error (error,
			     G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_IO,
			     "Invalid size, got %li", actual_length);
		return FALSE;
	}
	if (status != NULL)
		*status = buf[0];
	if (cmd != NULL)
		*cmd = buf[1];
	return TRUE;
}

/**
 * ch_device_open_full:
 * @device: A #GUsbDevice
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Opens the device ready for use.
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_open_full (GUsbDevice *device, GCancellable *cancellable, GError **error)
{
	gboolean ret;
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	g_return_val_if_fail (G_USB_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* open */
	if (!g_usb_device_open (device, error))
		return FALSE;

	/* claim interface */
	if (protocol_ver == 1) {
		if (!g_usb_device_set_configuration (device, CH_USB_CONFIG, error))
			return FALSE;
		if (!g_usb_device_claim_interface (device,
						   CH_USB_INTERFACE,
						   G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
						   error))
			return FALSE;
		return TRUE;
	}
	if (protocol_ver == 2) {
		if (!g_usb_device_claim_interface (device, CH_USB_INTERFACE, 0, error))
			return FALSE;
		ret = g_usb_device_control_transfer (device,
						     G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
						     G_USB_DEVICE_REQUEST_TYPE_CLASS,
						     G_USB_DEVICE_RECIPIENT_INTERFACE,
						     CH_CMD_CLEAR_ERROR,
						     0x00,		/* wValue */
						     CH_USB_INTERFACE,	/* idx */
						     NULL,		/* data */
						     0,			/* length */
						     NULL,		/* actual_length */
						     CH_DEVICE_USB_TIMEOUT,
						     cancellable,
						     error);
		if (!ret)
			return FALSE;

		/* check status */
		return ch_device_check_status (device, cancellable, error);
	}
	g_set_error_literal (error,
			     CH_DEVICE_ERROR,
			     CH_ERROR_NOT_IMPLEMENTED,
			     "Cannot open this hardware");
	return FALSE;
}

/**
 * ch_device_fixup_error:
 **/
static gboolean
ch_device_fixup_error (GUsbDevice *device, GCancellable *cancellable, GError **error)
{
	ChError status = 0xff;
	ChCmd cmd = 0xff;

	/* do we match not supported */
	if (error == NULL)
		return FALSE;
	if (!g_error_matches (*error,
			      G_USB_DEVICE_ERROR,
			      G_USB_DEVICE_ERROR_NOT_SUPPORTED))
		return FALSE;

	/* can we get a error enum from the device */
	if (!ch_device_get_error (device, &status, &cmd, cancellable, NULL))
		return FALSE;

	/* add what we tried to do */
	g_prefix_error (error,
			"Failed [%s(%02x):%s(%02x)]: ",
			ch_command_to_string (cmd), cmd,
			ch_strerror (status), status);
	return FALSE;
}

/**
 * ch_device_take_reading_spectral:
 * @device: A #GUsbDevice
 * @value: a #ChSpectrumKind, e.g. %CH_SPECTRUM_KIND_RAW
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Takes a reading from the device.
 *
 * Returns: %TRUE for success
 *
 * Since: 1.3.1
 **/
gboolean
ch_device_take_reading_spectral (GUsbDevice *device, ChSpectrumKind value,
				 GCancellable *cancellable, GError **error)
{
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	g_return_val_if_fail (G_USB_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (protocol_ver == 2) {
		gboolean ret;
		ret = g_usb_device_control_transfer (device,
						     G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
						     G_USB_DEVICE_REQUEST_TYPE_CLASS,
						     G_USB_DEVICE_RECIPIENT_INTERFACE,
						     0x51,		//FIXME: I have no idea
						     //CH_CMD_TAKE_READING_SPECTRAL,
						     value,		/* wValue */
						     CH_USB_INTERFACE,	/* idx */
						     NULL,		/* data */
						     0,			/* length */
						     NULL,		/* actual_length */
						     CH_DEVICE_USB_TIMEOUT,
						     cancellable,
						     error);
		if (!ret)
			return ch_device_fixup_error (device, cancellable, error);

		/* check status */
		return ch_device_check_status (device, cancellable, error);
	}
	g_set_error_literal (error,
			     CH_DEVICE_ERROR,
			     CH_ERROR_NOT_IMPLEMENTED,
			     "Taking spectral data is not supported");
	return FALSE;
}

/**
 * ch_device_take_reading_xyz:
 * @device: A #GUsbDevice
 * @calibration_idx: A calibration index or 0 for none
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Takes a reading from the device and returns the XYZ value.
 *
 * Returns: a #CdColorXYZ, or %NULL for error
 *
 * Since: 1.3.1
 **/
CdColorXYZ *
ch_device_take_reading_xyz (GUsbDevice *device, guint16 calibration_idx,
			    GCancellable *cancellable, GError **error)
{
	CdColorXYZ *value;
	gboolean ret;
	guint8 protocol_ver = ch_device_get_protocol_ver (device);

	g_return_val_if_fail (G_USB_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (protocol_ver == 1) {
		ChPackedFloat pf[3];
		ret = ch_device_write_command (device,
						CH_CMD_GET_TEMPERATURE,
						(guint8 *) &calibration_idx,
						sizeof(guint16),
						(guint8 *) pf,
						sizeof(pf),
						cancellable,
						error);
		if (!ret)
			return NULL;

		/* parse */
		value = cd_color_xyz_new ();
		ch_packed_float_to_double (&pf[0], &value->X);
		ch_packed_float_to_double (&pf[1], &value->Y);
		ch_packed_float_to_double (&pf[2], &value->Z);
		return value;
	}
	if (protocol_ver == 2) {
		gsize actual_length;
		gint32 buf[3];
		ret = g_usb_device_control_transfer (device,
						     G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
						     G_USB_DEVICE_REQUEST_TYPE_CLASS,
						     G_USB_DEVICE_RECIPIENT_INTERFACE,
						     CH_CMD_TAKE_READING_XYZ,
						     0,			/* wValue */
						     CH_USB_INTERFACE,	/* idx */
						     (guint8 *) buf,	/* data */
						     sizeof(buf),	/* length */
						     &actual_length,	/* actual_length */
						     CH_DEVICE_USB_TIMEOUT,
						     cancellable,
						     error);
		if (!ret)
			return NULL;

		/* return result */
		if (actual_length != sizeof(buf)) {
			g_set_error (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_IO,
				     "Invalid size, got %li", actual_length);
			return NULL;
		}

		/* check status */
		if (!ch_device_check_status (device, cancellable, error))
			return NULL;

		/* parse */
		value = cd_color_xyz_new ();
		value->X = ch_offset_float_to_double (buf[0]);
		value->Y= ch_offset_float_to_double (buf[1]);
		value->Z = ch_offset_float_to_double (buf[2]);
		return value;
	}
	g_set_error_literal (error,
			     CH_DEVICE_ERROR,
			     CH_ERROR_NOT_IMPLEMENTED,
			     "Getting an XYZ value is not supported");
	return NULL;
}

/**
 * ch_device_get_spectrum:
 * @device: A #GUsbDevice
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Gets the spectrum from the device. This queries the device multiple times
 * until the spectrum has been populated.
 *
 * The spectrum is also set up with the correct start and wavelength
 * calibration coefficients.
 *
 * Returns: a #CdSpectrum, or %NULL for error
 *
 * Since: 1.3.1
 **/
CdSpectrum *
ch_device_get_spectrum (GUsbDevice *device, GCancellable *cancellable, GError **error)
{
	gboolean ret;
	gdouble cal[4];
	gint32 buf[CH_EP0_TRANSFER_SIZE / sizeof(gint32)];
	gsize actual_length;
	guint16 i;
	guint16 j;
	guint8 protocol_ver = ch_device_get_protocol_ver (device);
	g_autoptr(CdSpectrum) sp = NULL;

	g_return_val_if_fail (G_USB_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* populate ahead of time for each chunk */
	sp = cd_spectrum_new ();

	if (protocol_ver != 2) {
		g_set_error_literal (error,
				     CH_DEVICE_ERROR,
				     CH_ERROR_NOT_IMPLEMENTED,
				     "Getting a spectrum is not supported");
		return NULL;
	}
	for (i = 0; i < 1024 * sizeof(gint32) / CH_EP0_TRANSFER_SIZE; i++) {
		ret = g_usb_device_control_transfer (device,
						     G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
						     G_USB_DEVICE_REQUEST_TYPE_CLASS,
						     G_USB_DEVICE_RECIPIENT_INTERFACE,
						     CH_CMD_READ_SRAM,
						     i,			/* wValue */
						     CH_USB_INTERFACE,	/* idx */
						     (guint8 *) buf,	/* data */
						     sizeof(buf),	/* length */
						     &actual_length,	/* actual_length */
						     CH_DEVICE_USB_TIMEOUT,
						     cancellable,
						     error);
		if (!ret)
			return NULL;
		if (actual_length != sizeof(buf)) {
			g_set_error (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_IO,
				     "Failed to get spectrum data, got %li",
				     actual_length);
			return NULL;
		}

		/* add data */
		for (j = 0; j < CH_EP0_TRANSFER_SIZE / sizeof(gint32); j++) {
			gdouble tmp = ch_offset_float_to_double (buf[j]);
			cd_spectrum_add_value (sp, tmp);
		}
	}

	/* check status */
	if (!ch_device_check_status (device, cancellable, error))
		return NULL;

	/* add the coefficients */
	if (!ch_device_get_ccd_calibration (device,
					    &cal[0], &cal[1], &cal[2], &cal[3],
					    cancellable, error))
		return NULL;
	cd_spectrum_set_start (sp, cal[0]);
	cd_spectrum_set_wavelength_cal (sp, cal[1], cal[2], cal[3]);

	/* return copy */
	return cd_spectrum_dup (sp);
}
