/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
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

/* device constants */
#define HUEY_DEVICE_TIMEOUT			30000 /* ms */
#define HUEY_EEPROM_SIZE			0x3ff /* bytes */

#define HUEY_CMD_GET_STATUS			0x00
#define HUEY_CMD_REGISTER_READ			0x08
#define HUEY_CMD_SAMPLE_BY_PULSES		0x04
#define HUEY_CMD_SAMPLE_BY_TIME			0x53

#define HUEY_RC_SUCCESS				0x00
#define HUEY_RC_ERROR				0x80

static gboolean
huey_device_send_data (GUsbDevice *device,
		       const guint8 *request,
		       gsize request_len,
		       guint8 *reply,
		       gsize reply_len,
		       gsize *reply_read,
		       GError **error)
{
	gboolean ret;

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
					     HUEY_DEVICE_TIMEOUT,
					     NULL,
					     error);
	if (!ret)
		return FALSE;

	/* get sync response */
	ret = g_usb_device_interrupt_transfer (device,
					       0x81,
					       (guint8 *) reply,
					       reply_len,
					       reply_read,
					       HUEY_DEVICE_TIMEOUT,
					       NULL,
					       error);
	if (!ret)
		return FALSE;

	/* the second byte seems to be the command again */
	cd_buffer_debug (CD_BUFFER_KIND_RESPONSE,
			 reply, *reply_read);
	if (reply[1] != request[0]) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "wrong command reply, got 0x%02x, "
			     "expected 0x%02x",
			     reply[1],
			     request[0]);
		return FALSE;
	}

	/* the first byte is status */
	if (reply[0] != HUEY_RC_SUCCESS) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "failed to issue command: %s", &reply[2]);
		return FALSE;
	}

	/* success */
	return TRUE;
}

gchar *
huey_device_get_status (GUsbDevice *device, GError **error)
{
	guint8 request[8] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
	guint8 reply[8] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
	gboolean ret;
	gsize reply_read;
	g_autoptr(GError) error_local = NULL;

	request[0] = HUEY_CMD_GET_STATUS;
	ret = huey_device_send_data (device,
				     request, 8,
				     reply, 8,
				     &reply_read,
				     &error_local);
	if (!ret) {
		if (!g_error_matches (error_local, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED)) {
			g_propagate_error (error, error_local);
			error_local = NULL;
			return NULL;
		}
	}

	/* for error the string is also set */
	return g_strndup ((gchar *) reply + 2, 6);
}

GBytes *
huey_device_read_eeprom (GUsbDevice *device, GError **error)
{
	guint8 request[8] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
	guint8 reply[8] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
	gboolean ret;
	gsize reply_read;
	g_autofree guint8 *eeprom = NULL;

	/* get entire memory space */
	eeprom = g_malloc0 (HUEY_EEPROM_SIZE);
	request[0] = HUEY_CMD_REGISTER_READ;
	for (guint i = 0; i < HUEY_EEPROM_SIZE; i += 0x4) {
		guint16 addr_be = GUINT16_TO_BE (i);
		memcpy (&request[1], &addr_be, 0x2);
		ret = huey_device_send_data (device,
					     request, 8,
					     reply, 8,
					     &reply_read,
					     error);
		if (!ret) {
			g_prefix_error (error, "failed to read eeprom @0x%04x: ", i);
			return NULL;
		}
		memcpy (eeprom + i, reply + 0x4, 0x4);
	}
	return g_bytes_new (eeprom, HUEY_EEPROM_SIZE);
}

gboolean
huey_device_open (GUsbDevice *device, GError **error)
{
	if (!g_usb_device_open (device, error)) {
		g_prefix_error (error, "failed to open device: ");
		return FALSE;
	}
	if (!g_usb_device_set_configuration (device, 0x01, error)) {
		g_prefix_error (error, "failed to set config on device: ");
		return FALSE;
	}
	if (!g_usb_device_claim_interface (device, 0x00,
	    G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER, error)) {
		g_prefix_error (error, "failed to claim interface for device: ");
		return FALSE;
	}
	return TRUE;
}

gboolean
huey_device_take_sample (GUsbDevice *device, gdouble *val, GError **error)
{
	gboolean ret;
	gdouble val_approx;
	gdouble val_pulses;
	gsize reply_read;
	guint16 timeout_be = GUINT16_TO_BE (0x0062);
	guint32 val_be;
	guint8 request[8] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
	guint8 reply[8] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };

	/* get approx reading so we know the number of pulses to count */
	request[0] = HUEY_CMD_SAMPLE_BY_TIME;
	memcpy (&request[1], &timeout_be, 0x2);
	ret = huey_device_send_data (device,
				     request, 8,
				     reply, 8,
				     &reply_read,
				     error);
	if (!ret)
		return FALSE;
	memcpy (&val_be, reply + 2, 4);
	val_approx = (gdouble) GUINT32_FROM_BE (val_be);
	g_debug ("approximate reading=%.0f", val_approx);

	/* calculate the number of pulses we should look for */
	val_approx *= 2.9f;
	timeout_be = GUINT16_TO_BE ((guint16) val_approx);

	/* get a precise reading by counting pulses */
	request[0] = HUEY_CMD_SAMPLE_BY_PULSES;
	memcpy (&request[1], &timeout_be, 0x2);
	ret = huey_device_send_data (device,
				     request, 8,
				     reply, 8,
				     &reply_read,
				     error);
	if (!ret)
		return FALSE;

	memcpy (&val_be, reply + 2, 4);
	val_pulses = (gdouble) GUINT32_FROM_BE (val_be);
	g_debug ("number of pulses=%.0f", val_pulses);

	/* calculate luminance */
	if (val != NULL)
		*val = (val_approx * 1000.f) / val_pulses;

	return TRUE;
}
