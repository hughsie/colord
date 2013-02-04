/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Richard Hughes <richard@hughsie.com>
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
#include <string.h>

#include "huey-device.h"
#include "huey-enum.h"

#define HUEY_MAX_READ_RETRIES		5
#define HUEY_CONTROL_MESSAGE_TIMEOUT	50000 /* ms */

/* fudge factor to convert the value of HUEY_CMD_GET_AMBIENT to Lux */
#define HUEY_AMBIENT_UNITS_TO_LUX	125.0f

/**
 * huey_device_error_quark:
 **/
GQuark
huey_device_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("HueyError");
	return quark;
}

/**
 * huey_device_send_data:
 *
 * Since: 0.1.29
 **/
gboolean
huey_device_send_data (GUsbDevice *device,
		       const guint8 *request,
		       gsize request_len,
		       guint8 *reply,
		       gsize reply_len,
		       gsize *reply_read,
		       GError **error)
{
	gboolean ret;
	guint i;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (request != NULL, FALSE);
	g_return_val_if_fail (request_len != 0, FALSE);
	g_return_val_if_fail (reply != NULL, FALSE);
	g_return_val_if_fail (reply_len != 0, FALSE);
	g_return_val_if_fail (reply_read != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* control transfer */
	cd_buffer_debug (CD_BUFFER_KIND_REQUEST,
			 request, request_len);
	ret = g_usb_device_control_transfer (device,
					     G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
					     G_USB_DEVICE_REQUEST_TYPE_CLASS,
					     G_USB_DEVICE_RECIPIENT_INTERFACE,
					     0x09,
					     0x0200,
					     0,
					     (guint8 *) request,
					     request_len,
					     NULL,
					     HUEY_CONTROL_MESSAGE_TIMEOUT,
					     NULL,
					     error);
	if (!ret)
		goto out;

	/* some commands need to retry the read */
	for (i = 0; i < HUEY_MAX_READ_RETRIES; i++) {

		/* get sync response */
		ret = g_usb_device_interrupt_transfer (device,
						       0x81,
						       (guint8 *) reply,
						       reply_len,
						       reply_read,
						       HUEY_CONTROL_MESSAGE_TIMEOUT,
						       NULL,
						       error);
		if (!ret)
			goto out;

		/* the second byte seems to be the command again */
		cd_buffer_debug (CD_BUFFER_KIND_RESPONSE,
				 reply, *reply_read);
		if (reply[1] != request[0]) {
			ret = FALSE;
			g_set_error (error,
				     HUEY_DEVICE_ERROR,
				     HUEY_DEVICE_ERROR_INTERNAL,
				     "wrong command reply, got 0x%02x, "
				     "expected 0x%02x",
				     reply[1],
				     request[0]);
			goto out;
		}

		/* the first byte is status */
		if (reply[0] == HUEY_RC_SUCCESS) {
			ret = TRUE;
			goto out;
		}

		/* failure, the return buffer is set to "Locked" */
		if (reply[0] == HUEY_RC_LOCKED) {
			ret = FALSE;
			g_set_error_literal (error,
					     HUEY_DEVICE_ERROR,
					     HUEY_DEVICE_ERROR_INTERNAL,
					     "the device is locked");
			goto out;
		}

		/* failure, the return buffer is set to "NoCmd" */
		if (reply[0] == HUEY_RC_ERROR) {
			ret = FALSE;
			g_set_error (error,
				     HUEY_DEVICE_ERROR,
				     HUEY_DEVICE_ERROR_INTERNAL,
				     "failed to issue command: %s", &reply[2]);
			goto out;
		}

		/* we ignore retry */
		if (reply[0] != HUEY_RC_RETRY) {
			ret = FALSE;
			g_set_error (error,
				     HUEY_DEVICE_ERROR,
				     HUEY_DEVICE_ERROR_INTERNAL,
				     "return value unknown: 0x%02x", reply[0]);
			goto out;
		}
	}

	/* no success */
	g_set_error (error,
		     HUEY_DEVICE_ERROR,
		     HUEY_DEVICE_ERROR_INTERNAL,
		     "gave up retrying after %i reads",
		     HUEY_MAX_READ_RETRIES);
out:
	return ret;
}

/**
 * huey_device_unlock:
 *
 * Since: 0.1.29
 **/
gboolean
huey_device_unlock (GUsbDevice *device, GError **error)
{
	guint8 request[8];
	guint8 reply[8];
	gboolean ret;
	gsize reply_read;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	request[0] = HUEY_CMD_UNLOCK;
	request[1] = 'G';
	request[2] = 'r';
	request[3] = 'M';
	request[4] = 'b';
	request[5] = 'k'; /* <- perhaps junk, need to test next time locked */
	request[6] = 'e'; /* <-         "" */
	request[7] = 'd'; /* <-         "" */

	/* no idea why the hardware gets 'locked' */
	ret = huey_device_send_data (device,
				     request, 8,
				     reply, 8,
				     &reply_read,
				     error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * huey_device_get_serial_number:
 *
 * Since: 0.1.29
 **/
gchar *
huey_device_get_serial_number (GUsbDevice *device, GError **error)
{
	gboolean ret;
	guint32 tmp;
	gchar *serial_number = NULL;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	ret = huey_device_read_register_word (device,
					      HUEY_EEPROM_ADDR_SERIAL,
					      &tmp,
					      error);
	if (!ret)
		goto out;
	serial_number = g_strdup_printf ("%i", tmp);
out:
	return serial_number;
}

/**
 * huey_device_get_unlock_string:
 *
 * Since: 0.1.29
 **/
gchar *
huey_device_get_unlock_string (GUsbDevice *device, GError **error)
{
	gboolean ret;
	gchar *str = NULL;
	gchar tmp[5];

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	ret = huey_device_read_register_string (device,
						HUEY_EEPROM_ADDR_UNLOCK,
						tmp,
						sizeof (tmp),
						error);
	if (!ret)
		goto out;
	str = g_strndup (tmp, sizeof (tmp));
out:
	return str;
}

/**
 * huey_device_set_leds:
 *
 * Since: 0.1.29
 **/
gboolean
huey_device_set_leds (GUsbDevice *device, guint8 value, GError **error)
{
	guint8 reply[8];
	gsize reply_read;
	guint8 payload[] = { HUEY_CMD_SET_LEDS,
			     0x00,
			     ~value,
			     0x00,
			     0x00,
			     0x00,
			     0x00,
			     0x00 };

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	return huey_device_send_data (device,
				      payload, 8, reply, 8,
				      &reply_read,
				      error);
}

/**
 * huey_device_read_register_byte:
 *
 * Return value: -1 for error.
 *
 * Since: 0.1.29
 **/
gdouble
huey_device_get_ambient (GUsbDevice *device, GError **error)
{
	gboolean ret = FALSE;
	gboolean retval = -1;
	gsize reply_read;
	guint8 reply[8];
	guint8 request[] = { HUEY_CMD_GET_AMBIENT,
			     0x03,
			     0x00,
			     0x00,
			     0x00,
			     0x00,
			     0x00,
			     0x00 };

	g_return_val_if_fail (G_USB_IS_DEVICE (device), -1);
	g_return_val_if_fail (error == NULL || *error == NULL, -1);

	/* just use LCD mode */
	request[2] = 0x00;
	ret = huey_device_send_data (device,
				     request, 8,
				     reply, 8,
				     &reply_read,
				     error);
	if (!ret)
		goto out;

	/* parse the value */
	retval = (gdouble) cd_buffer_read_uint16_be (reply+5) / HUEY_AMBIENT_UNITS_TO_LUX;
out:
	return retval;
}

/**
 * huey_device_read_register_byte:
 *
 * Since: 0.1.29
 **/
gboolean
huey_device_read_register_byte (GUsbDevice *device,
				guint8 addr,
				guint8 *value,
				GError **error)
{
	guint8 request[] = { HUEY_CMD_REGISTER_READ,
			     0xff,
			     0x00,
			     0x10,
			     0x3c,
			     0x06,
			     0x00,
			     0x00 };
	guint8 reply[8];
	gboolean ret;
	gsize reply_read;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* hit hardware */
	request[1] = addr;
	ret = huey_device_send_data (device,
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
 * huey_device_read_register_string:
 *
 * Since: 0.1.29
 **/
gboolean
huey_device_read_register_string (GUsbDevice *device,
				  guint8 addr,
				  gchar *value,
				  gsize len,
				  GError **error)
{
	guint8 i;
	gboolean ret = TRUE;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* get each byte of the string */
	for (i = 0; i < len; i++) {
		ret = huey_device_read_register_byte (device,
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
 * huey_device_read_register_word:
 *
 * Since: 0.1.29
 **/
gboolean
huey_device_read_register_word (GUsbDevice *device,
				guint8 addr,
				guint32 *value,
				GError **error)
{
	guint8 i;
	guint8 tmp[4];
	gboolean ret = TRUE;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* get each byte of the 32 bit number */
	for (i = 0; i < 4; i++) {
		ret = huey_device_read_register_byte (device,
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
 * huey_device_read_register_float:
 *
 * Since: 0.1.29
 **/
gboolean
huey_device_read_register_float (GUsbDevice *device,
				 guint8 addr,
				 gfloat *value,
				 GError **error)
{
	gboolean ret;
	guint32 tmp = 0;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* first read in 32 bit integer */
	ret = huey_device_read_register_word (device,
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
 * huey_device_read_register_vector:
 *
 * Since: 0.1.29
 **/
gboolean
huey_device_read_register_vector (GUsbDevice *device,
				  guint8 addr,
				  CdVec3 *value,
				  GError **error)
{
	gboolean ret = TRUE;
	guint i;
	gfloat tmp = 0.0f;
	gdouble *vector_data;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* get this to avoid casting */
	vector_data = cd_vec3_get_data (value);

	/* read in vec3 */
	for (i = 0; i < 3; i++) {
		ret = huey_device_read_register_float (device,
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
 * huey_device_read_register_matrix:
 *
 * Since: 0.1.29
 **/
gboolean
huey_device_read_register_matrix (GUsbDevice *device,
				  guint8 addr,
				  CdMat3x3 *value,
				  GError **error)
{
	gboolean ret = TRUE;
	guint i;
	gfloat tmp = 0.0f;
	gdouble *matrix_data;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* get this to avoid casting */
	matrix_data = cd_mat33_get_data (value);

	/* read in 3d matrix */
	for (i = 0; i < 9; i++) {
		ret = huey_device_read_register_float (device,
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
