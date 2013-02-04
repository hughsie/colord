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

/**
 * SECTION:dtp94-enum
 * @short_description: Types used by dtp94 and libdtp94
 *
 * These helper functions provide a way to marshal enumerated values to
 * text and back again.
 *
 * See also: #CdClient, #CdDevice
 */

#include "config.h"

#include <glib.h>
#include <string.h>
#include <colord-private.h>

#include "dtp94-device.h"
#include "dtp94-enum.h"

#define DTP94_MAX_READ_RETRIES		5
#define DTP94_CONTROL_MESSAGE_TIMEOUT	50000 /* ms */

/**
 * dtp94_device_error_quark:
 **/
GQuark
dtp94_device_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("Dtp94DeviceError");
	return quark;
}

/**
 * dtp94_device_send_data:
 *
 * Since: 0.1.29
 **/
gboolean
dtp94_device_send_data (GUsbDevice *device,
			const guint8 *request,
			gsize request_len,
			guint8 *reply,
			gsize reply_len,
			gsize *reply_read,
			GError **error)
{
	gboolean ret;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (request != NULL, FALSE);
	g_return_val_if_fail (request_len != 0, FALSE);
	g_return_val_if_fail (reply != NULL, FALSE);
	g_return_val_if_fail (reply_len != 0, FALSE);
	g_return_val_if_fail (reply_read != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* request data from device */
	cd_buffer_debug (CD_BUFFER_KIND_REQUEST,
			 request, request_len);
	ret = g_usb_device_interrupt_transfer (device,
					       0x2,
					       (guint8 *) request,
					       request_len,
					       NULL,
					       DTP94_CONTROL_MESSAGE_TIMEOUT,
					       NULL,
					       error);
	if (!ret)
		goto out;

	/* get sync response */
	ret = g_usb_device_interrupt_transfer (device,
					       0x81,
					       (guint8 *) reply,
					       reply_len,
					       reply_read,
					       DTP94_CONTROL_MESSAGE_TIMEOUT,
					       NULL,
					       error);
	if (!ret)
		goto out;
	if (reply_read == 0) {
		ret = FALSE;
		g_set_error_literal (error,
				     DTP94_DEVICE_ERROR,
				     DTP94_DEVICE_ERROR_INTERNAL,
				     "failed to get data from device");
		goto out;
	}
	cd_buffer_debug (CD_BUFFER_KIND_RESPONSE,
			 reply, *reply_read);
out:
	return ret;
}

static gboolean
dtp94_device_send_cmd_issue (GUsbDevice *device,
			     const gchar *command,
			     GError **error)
{
	gboolean ret;
	gsize reply_read;
	guint8 buffer[128];
	guint8 rc;
	guint command_len;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* sent command raw */
	command_len = strlen (command);
	ret = dtp94_device_send_data (device,
			       (const guint8 *) command,
			       command_len,
			       buffer,
			       sizeof (buffer),
			       &reply_read,
			       error);
	if (!ret)
		goto out;

	/* device busy */
	rc = dtp94_rc_parse (buffer, reply_read);
	if (rc == DTP94_RC_BAD_COMMAND) {
		ret = FALSE;
		g_set_error_literal (error,
				     DTP94_DEVICE_ERROR,
				     DTP94_DEVICE_ERROR_NO_DATA,
				     "device busy");
		goto out;
	}

	/* no success */
	if (rc != DTP94_RC_OK) {
		ret = FALSE;
		buffer[reply_read] = '\0';
		g_set_error (error,
			     DTP94_DEVICE_ERROR,
			     DTP94_DEVICE_ERROR_INTERNAL,
			     "unexpected response from device: %s [%s]",
			     (const gchar *) buffer,
			     dtp94_rc_to_string (rc));
		goto out;
	}
out:
	return ret;
}

/**
 * dtp94_device_send_cmd:
 *
 * Since: 0.1.29
 **/
gboolean
dtp94_device_send_cmd (GUsbDevice *device,
		       const gchar *command,
		       GError **error)
{
	gboolean ret = FALSE;
	GError *error_local = NULL;
	guint error_cnt = 0;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (command != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* repeat until the device is ready */
	for (error_cnt = 0; ret != TRUE; error_cnt++) {
		ret = dtp94_device_send_cmd_issue (device, command, &error_local);
		if (!ret) {
			if (error_cnt < DTP94_MAX_READ_RETRIES &&
			    g_error_matches (error_local,
					     DTP94_DEVICE_ERROR,
					     DTP94_DEVICE_ERROR_NO_DATA)) {
				g_debug ("ignoring %s", error_local->message);
				g_clear_error (&error_local);
				continue;
			}
			g_propagate_error (error, error_local);
			break;
		}
	};
	return ret;
}

/**
 * dtp94_device_setup:
 *
 * Since: 0.1.29
 **/
gboolean
dtp94_device_setup (GUsbDevice *device, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* reset device */
	ret = dtp94_device_send_cmd (device, "0PR\r", error);
	if (!ret)
		goto out;

	/* reset device again */
	ret = dtp94_device_send_cmd (device, "0PR\r", error);
	if (!ret)
		goto out;

	/* set color data separator to '\t' */
	ret = dtp94_device_send_cmd (device, "0207CF\r", error);
	if (!ret)
		goto out;

	/* set delimeter to CR */
	ret = dtp94_device_send_cmd (device, "0008CF\r", error);
	if (!ret)
		goto out;

	/* set extra digit resolution */
	ret = dtp94_device_send_cmd (device, "010ACF\r", error);
	if (!ret)
		goto out;

	/* no black point subtraction */
	ret = dtp94_device_send_cmd (device, "0019CF\r", error);
	if (!ret)
		goto out;

	/* set to factory calibration */
	ret = dtp94_device_send_cmd (device, "EFC\r", error);
	if (!ret)
		goto out;

	/* unknown command */
	ret = dtp94_device_send_cmd (device, "0117CF\r", error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * dtp94_device_take_sample:
 *
 * Since: 0.1.29
 **/
CdColorXYZ *
dtp94_device_take_sample (GUsbDevice *device, CdSensorCap cap, GError **error)
{
	CdColorXYZ *result = NULL;
	gboolean ret = FALSE;
	gchar *tmp;
	gsize reply_read;
	guint8 buffer[128];

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* set hardware support */
	switch (cap) {
	case CD_SENSOR_CAP_CRT:
	case CD_SENSOR_CAP_PLASMA:
		/* CRT = 01 */
		ret = dtp94_device_send_cmd (device, "0116CF\r", error);
		break;
	case CD_SENSOR_CAP_LCD:
		/* LCD = 02 */
		ret = dtp94_device_send_cmd (device, "0216CF\r", error);
		break;
	default:
		g_set_error (error,
			     DTP94_DEVICE_ERROR,
			     DTP94_DEVICE_ERROR_NO_SUPPORT,
			     "DTP94 cannot measure in %s mode",
			     cd_sensor_cap_to_string (cap));
		break;
	}
	if (!ret)
		goto out;

	/* get sample */
	ret = dtp94_device_send_data (device,
			       (const guint8 *) "RM\r", 3,
			       buffer, sizeof (buffer),
			       &reply_read,
			       error);
	if (!ret)
		goto out;
	tmp = g_strstr_len ((const gchar *) buffer, reply_read, "\r");
	if (tmp == NULL || memcmp (tmp + 1, "<00>", 4) != 0) {
		buffer[reply_read] = '\0';
		g_set_error (error,
			     DTP94_DEVICE_ERROR,
			     DTP94_DEVICE_ERROR_INTERNAL,
			     "unexpected response from device: %s",
			     (const gchar *) buffer);
		goto out;
	}

	/* format is raw ASCII with fixed formatting:
	 * 'X     10.29	Y     10.33	Z      4.65\u000d<00>' */
	tmp = (gchar *) buffer;
	g_strdelimit (tmp, "\t\r", '\0');

	/* success */
	result = cd_color_xyz_new ();
	cd_color_xyz_set (result,
			  g_ascii_strtod (tmp + 1, NULL),
			  g_ascii_strtod (tmp + 13, NULL),
			  g_ascii_strtod (tmp + 25, NULL));
out:
	return result;
}

/**
 * dtp94_device_get_serial:
 *
 * Since: 0.1.29
 **/
gchar *
dtp94_device_get_serial (GUsbDevice *device, GError **error)
{
	gboolean ret;
	gchar *serial = NULL;
	gchar *tmp;
	gsize reply_read;
	guint8 buffer[128];

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	ret = dtp94_device_send_data (device,
			       (const guint8 *) "SV\r", 3,
			       buffer, sizeof (buffer),
			       &reply_read,
			       error);
	if (!ret)
		goto out;
	tmp = g_strstr_len ((const gchar *) buffer, reply_read, "\r");
	if (tmp == NULL || memcmp (tmp + 1, "<00>", 4) != 0) {
		buffer[reply_read] = '\0';
		g_set_error (error,
			     DTP94_DEVICE_ERROR,
			     DTP94_DEVICE_ERROR_INTERNAL,
			     "unexpected response from device: %s",
			     (const gchar *) buffer);
		goto out;
	}
	tmp[0] = '\0';
	serial = g_strdup (tmp);
out:
	return serial;
}
