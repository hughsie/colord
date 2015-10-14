/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2015 Richard Hughes <richard@hughsie.com>
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
	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* load device */
	if (!g_usb_device_open (device, error))
		return FALSE;
	if (!g_usb_device_set_configuration (device, CH_USB_CONFIG, error))
		return FALSE;
	if (!g_usb_device_claim_interface (device,
					   CH_USB_INTERFACE,
					   G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
					   error))
		return FALSE;
	return TRUE;
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
		memcpy(tdata->buffer_out, tdata->buffer + 3, 4);
		break;
	case CH_CMD_GET_COLOR_SELECT:
		memcpy(tdata->buffer_out, tdata->buffer + 1, 1);
		break;
	case CH_CMD_GET_INTEGRAL_TIME:
		memcpy(tdata->buffer_out, tdata->buffer + 4, 2);
		break;
	case CH_CMD_GET_LEDS:
		memcpy(tdata->buffer_out, tdata->buffer + 2, 1);
		break;
	case CH_CMD_GET_MULTIPLIER:
		memcpy(tdata->buffer_out, tdata->buffer + 3, 1);
		break;
	case CH_CMD_GET_FIRMWARE_VERSION:
		memcpy(tdata->buffer_out, tdata->buffer + 2, 6);
		break;
	case CH_CMD_GET_HARDWARE_VERSION:
		memcpy(tdata->buffer_out, tdata->buffer + 1, 1);
		break;
	case CH_CMD_GET_SERIAL_NUMBER:
		memcpy(tdata->buffer_out, tdata->buffer + 8, 4);
		break;
	case CH_CMD_SET_COLOR_SELECT:
		memcpy(tdata->buffer + 1, tdata->buffer_orig + 1, 1);
		another_request_required = TRUE;
		break;
	case CH_CMD_SET_INTEGRAL_TIME:
		memcpy(tdata->buffer + 4, tdata->buffer_orig + 1, 2);
		another_request_required = TRUE;
		break;
	case CH_CMD_SET_LEDS:
		memcpy(tdata->buffer + 2, tdata->buffer_orig + 1, 1);
		another_request_required = TRUE;
		break;
	case CH_CMD_SET_MULTIPLIER:
		memcpy(tdata->buffer + 3, tdata->buffer_orig + 1, 1);
		another_request_required = TRUE;
		break;
	case CH_CMD_SET_FLASH_SUCCESS:
		memcpy(tdata->buffer + 13, tdata->buffer_orig + 1, 1);
		another_request_required = TRUE;
		break;
	case CH_CMD_RESET:
		tdata->buffer[12] = 1;
		another_request_required = TRUE;
		break;
	case CH_CMD_SET_SERIAL_NUMBER:
		memcpy(tdata->buffer + 8, tdata->buffer_orig + 1, 4);
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
