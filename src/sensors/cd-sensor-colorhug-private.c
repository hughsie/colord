/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
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

#include "cd-sensor-colorhug-private.h"

/**
 * ch_strerror:
 **/
const gchar *
ch_strerror (ChError error_enum)
{
	const char *str = NULL;
	switch (error_enum) {
	case CH_ERROR_NONE:
		str = "Success";
		break;
	case CH_ERROR_UNKNOWN_CMD:
		str = "Unknown command";
		break;
	case CH_ERROR_WRONG_UNLOCK_CODE:
		str = "Wrong unlock code";
		break;
	case CH_ERROR_NOT_IMPLEMENTED:
		str = "Not implemented";
		break;
	case CH_ERROR_UNDERFLOW_SENSOR:
		str = "Underflow of sensor";
		break;
	case CH_ERROR_NO_SERIAL:
		str = "No serial";
		break;
	case CH_ERROR_WATCHDOG:
		str = "Watchdog";
		break;
	case CH_ERROR_INVALID_ADDRESS:
		str = "Invalid address";
		break;
	case CH_ERROR_INVALID_LENGTH:
		str = "Invalid length";
		break;
	case CH_ERROR_INVALID_CHECKSUM:
		str = "Invalid checksum";
		break;
	case CH_ERROR_INVALID_VALUE:
		str = "Invalid value";
		break;
	case CH_ERROR_UNKNOWN_CMD_FOR_BOOTLOADER:
		str = "Unknown command for bootloader";
		break;
	case CH_ERROR_OVERFLOW_MULTIPLY:
		str = "Overflow of multiply";
		break;
	case CH_ERROR_OVERFLOW_ADDITION:
		str = "Overflow of addition";
		break;
	case CH_ERROR_OVERFLOW_SENSOR:
		str = "Overflow of sensor";
		break;
	case CH_ERROR_OVERFLOW_STACK:
		str = "Overflow of stack";
		break;
	case CH_ERROR_NO_CALIBRATION:
		str = "No calibration";
		break;
	case CH_ERROR_DEVICE_DEACTIVATED:
		str = "Device deactivated";
		break;
	case CH_ERROR_INCOMPLETE_REQUEST:
		str = "Incomplete previous request";
		break;
	default:
		str = "Unknown error, please report";
		break;
	}
	return str;
}

/**
 * ch_command_to_string:
 **/
const gchar *
ch_command_to_string (guint8 cmd)
{
	const char *str = NULL;
	switch (cmd) {
	case CH_CMD_GET_COLOR_SELECT:
		str = "get-color-select";
		break;
	case CH_CMD_SET_COLOR_SELECT:
		str = "set-color-select";
		break;
	case CH_CMD_GET_MULTIPLIER:
		str = "get-multiplier";
		break;
	case CH_CMD_SET_MULTIPLIER:
		str = "set-multiplier";
		break;
	case CH_CMD_GET_INTEGRAL_TIME:
		str = "get-integral-time";
		break;
	case CH_CMD_SET_INTEGRAL_TIME:
		str = "set-integral-time";
		break;
	case CH_CMD_GET_FIRMWARE_VERSION:
		str = "get-firmare-version";
		break;
	case CH_CMD_GET_CALIBRATION:
		str = "get-calibration";
		break;
	case CH_CMD_SET_CALIBRATION:
		str = "set-calibration";
		break;
	case CH_CMD_GET_SERIAL_NUMBER:
		str = "get-serial-number";
		break;
	case CH_CMD_SET_SERIAL_NUMBER:
		str = "set-serial-number";
		break;
	case CH_CMD_GET_OWNER_NAME:
		str = "get-owner-name";
		break;
	case CH_CMD_SET_OWNER_NAME:
		str = "set-owner-name";
		break;
	case CH_CMD_GET_OWNER_EMAIL:
		str = "get-owner-name";
		break;
	case CH_CMD_SET_OWNER_EMAIL:
		str = "set-owner-email";
		break;
	case CH_CMD_GET_LEDS:
		str = "get-leds";
		break;
	case CH_CMD_SET_LEDS:
		str = "set-leds";
		break;
	case CH_CMD_GET_PCB_ERRATA:
		str = "get-pcb-errata";
		break;
	case CH_CMD_SET_PCB_ERRATA:
		str = "set-pcb-errata";
		break;
	case CH_CMD_GET_DARK_OFFSETS:
		str = "get-dark-offsets";
		break;
	case CH_CMD_SET_DARK_OFFSETS:
		str = "set-dark-offsets";
		break;
	case CH_CMD_WRITE_EEPROM:
		str = "write-eeprom";
		break;
	case CH_CMD_TAKE_READING_RAW:
		str = "take-reading-raw";
		break;
	case CH_CMD_TAKE_READINGS:
		str = "take-readings";
		break;
	case CH_CMD_TAKE_READING_XYZ:
		str = "take-reading-xyz";
		break;
	case CH_CMD_RESET:
		str = "reset";
		break;
	case CH_CMD_READ_FLASH:
		str = "read-flash";
		break;
	case CH_CMD_ERASE_FLASH:
		str = "erase-flash";
		break;
	case CH_CMD_WRITE_FLASH:
		str = "write-flash";
		break;
	case CH_CMD_BOOT_FLASH:
		str = "boot-flash";
		break;
	case CH_CMD_SET_FLASH_SUCCESS:
		str = "set-flash-success";
		break;
	case CH_CMD_GET_CALIBRATION_MAP:
		str = "get-calibration-map";
		break;
	case CH_CMD_SET_CALIBRATION_MAP:
		str = "set-calibration-map";
		break;
	case CH_CMD_GET_HARDWARE_VERSION:
		str = "get-hardware-version";
		break;
	default:
		str = "unknown-command";
		break;
	}
	return str;
}

/**********************************************************************/

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

	for (i=0; i< length; i++)
		g_print ("%02x [%c]\t", data[i], g_ascii_isprint (data[i]) ? data[i] : '?');

	g_print ("%c[%dm\n", 0x1B, 0);
}

typedef struct {
	GUsbDevice		*device;
	GCancellable		*cancellable;
	GSimpleAsyncResult	*res;
	guint8			*buffer;
	guint8			*buffer_out;
	gsize			 buffer_out_len;
	guint8			 cmd;
} ChDeviceHelper;

/**
 * ch_device_write_command_finish:
 * @device: a #GUsbDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: %TRUE if the request was fulfilled.
 **/
gboolean
ch_device_write_command_finish (GUsbDevice *device,
				GAsyncResult *res,
				GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_simple_async_result_get_op_res_gboolean (simple);
}

/**
 * ch_device_free_helper:
 **/
static void
ch_device_free_helper (ChDeviceHelper *helper)
{
	if (helper->cancellable != NULL)
		g_object_unref (helper->cancellable);
	g_object_unref (helper->device);
	g_object_unref (helper->res);
	g_free (helper->buffer);
	g_free (helper);
}

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
	ChDeviceHelper *helper = (ChDeviceHelper *) user_data;

	/* get the result */
	actual_len = g_usb_device_interrupt_transfer_finish (device,
							     res,
							     &error);
	if ((gssize) actual_len < 0) {
		g_simple_async_result_take_error (helper->res, error);
		g_simple_async_result_complete_in_idle (helper->res);
		ch_device_free_helper (helper);
		return;
	}

	/* parse the reply */
	ch_print_data_buffer ("reply", helper->buffer, actual_len);

	/* parse */
	if (helper->buffer[CH_BUFFER_OUTPUT_RETVAL] != CH_ERROR_NONE ||
	    helper->buffer[CH_BUFFER_OUTPUT_CMD] != helper->cmd ||
	    actual_len != helper->buffer_out_len + CH_BUFFER_OUTPUT_DATA) {
		error_enum = helper->buffer[CH_BUFFER_OUTPUT_RETVAL];
		msg = g_strdup_printf ("Invalid read: retval=0x%02x [%s] "
				       "cmd=0x%02x (expected 0x%x [%s]) "
				       "len=%"G_GSIZE_FORMAT" "
				       "(expected %"G_GSIZE_FORMAT")",
				       error_enum,
				       ch_strerror (error_enum),
				       helper->buffer[CH_BUFFER_OUTPUT_CMD],
				       helper->cmd,
				       ch_command_to_string (helper->cmd),
				       actual_len,
				       helper->buffer_out_len + CH_BUFFER_OUTPUT_DATA);
		g_simple_async_result_set_error (helper->res, 1, 0, "%s", msg);
		g_simple_async_result_complete_in_idle (helper->res);
		ch_device_free_helper (helper);
		return;
	}

	/* copy */
	if (helper->buffer_out != NULL) {
		memcpy (helper->buffer_out,
			helper->buffer + CH_BUFFER_OUTPUT_DATA,
			helper->buffer_out_len);
	}

	/* success */
	g_simple_async_result_set_op_res_gboolean (helper->res, TRUE);
	g_simple_async_result_complete_in_idle (helper->res);
	ch_device_free_helper (helper);
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
	ChDeviceHelper *helper = (ChDeviceHelper *) user_data;

	/* get the result */
	actual_len = g_usb_device_interrupt_transfer_finish (device,
							     res,
							     &error);
	if (actual_len < CH_USB_HID_EP_SIZE) {
		g_simple_async_result_take_error (helper->res, error);
		g_simple_async_result_complete_in_idle (helper->res);
		ch_device_free_helper (helper);
		return;
	}

	/* request the reply */
	g_usb_device_interrupt_transfer_async (helper->device,
					       CH_USB_HID_EP_IN,
					       helper->buffer,
					       CH_USB_HID_EP_SIZE,
					       CH_DEVICE_USB_TIMEOUT,
					       helper->cancellable,
					       ch_device_reply_cb,
					       helper);
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
	ChDeviceHelper *helper;

	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (cmd != 0);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	helper = g_new0 (ChDeviceHelper, 1);
	helper->device = g_object_ref (device);
	helper->buffer_out = buffer_out;
	helper->buffer_out_len = buffer_out_len;
	helper->buffer = g_new0 (guint8, CH_USB_HID_EP_SIZE);
	helper->res = g_simple_async_result_new (G_OBJECT (device),
						 callback,
						 user_data,
						 ch_device_write_command_async);
	if (cancellable != NULL)
		helper->cancellable = g_object_ref (cancellable);

	/* set command */
	helper->cmd = cmd;
	helper->buffer[CH_BUFFER_INPUT_CMD] = helper->cmd;
	if (buffer_in != NULL) {
		memcpy (helper->buffer + CH_BUFFER_INPUT_DATA,
			buffer_in,
			buffer_in_len);
	}

	/* request */
	ch_print_data_buffer ("request",
			      helper->buffer,
			      buffer_in_len + 1);
	g_usb_device_interrupt_transfer_async (helper->device,
					       CH_USB_HID_EP_OUT,
					       helper->buffer,
					       CH_USB_HID_EP_SIZE,
					       CH_DEVICE_USB_TIMEOUT,
					       helper->cancellable,
					       ch_device_request_cb,
					       helper);
}

/**
 * ch_sha1_to_string:
 * @sha1: A %ChSha1
 *
 * Gets a string representation of the SHA1 hash.
 *
 * Return value: A string, free with g_free().
 */
gchar *
ch_sha1_to_string (const ChSha1 *sha1)
{
	GString *string = NULL;
	guint i;

	g_return_val_if_fail (sha1 != NULL, NULL);

	/* read each byte and convert to hex */
	string = g_string_new ("");
	for (i = 0; i < 20; i++) {
		g_string_append_printf (string, "%02x",
					sha1->bytes[i]);
	}
	return g_string_free (string, FALSE);
}

/**
 * ch_sha1_parse:
 * @value: A string representation of the SHA1 hash
 * @sha1: A %ChSha1
 * @error: A %GError, or %NULL
 *
 * Parses a SHA1 hash from a string value.
 *
 * Return value: %TRUE for success
 */
gboolean
ch_sha1_parse (const gchar *value, ChSha1 *sha1, GError **error)
{
	gboolean ret = TRUE;
	gchar tmp[3] = { '\0', '\0', '\0'};
	guint i;
	guint len;

	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (sha1 != NULL, FALSE);

	/* not a SHA1 hash */
	len = strlen (value);
	if (len != 40) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Invalid SHA1 hash '%s'",
			     value);
		goto out;
	}

	/* parse */
	for (i = 0; i < len; i += 2) {
		tmp[0] = value[i+0];
		tmp[1] = value[i+1];
		sha1->bytes[i/2] = g_ascii_strtoull (tmp, NULL, 16);
	}
out:
	return ret;
}
