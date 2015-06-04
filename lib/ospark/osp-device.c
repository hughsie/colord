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
 * SECTION:ospark-enum
 * @short_description: Types used by libospark
 *
 * See also: #CdClient, #CdDevice
 */

#include "config.h"

#include <glib.h>
#include <string.h>
#include <colord-private.h>

#include "cd-cleanup.h"

#include "osp-device.h"
#include "osp-enum.h"

#define OSP_USB_TIMEOUT_MS		50000		/* ms */
#define OSP_DEVICE_MAX_MSG_LENGTH	(10240 + 64)	/* bytes */
#define OSP_DEVICE_EP_SIZE		64		/* bytes */

/**
 * osp_device_error_quark:
 **/
GQuark
osp_device_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("OspDeviceError");
	return quark;
}

/**
 * osp_device_open:
 *
 * Since: 1.2.11
 **/
gboolean
osp_device_open (GUsbDevice *device, GError **error)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!g_usb_device_open (device, error))
		return NULL;
	if (!g_usb_device_claim_interface (device, 0x00, 0, error)) {
		g_prefix_error (error, "Failed to claim interface: ");
		return FALSE;
	}

	return TRUE;
}

/**
 * osp_device_query:
 *
 * Since: 1.2.11
 **/
static gboolean
osp_device_query (GUsbDevice *device, OspCmd cmd,
		  const guint8 *data_in, gsize data_in_length,
		  guint8 **data_out, gsize *data_out_length,
		  GError **error)
{
	OspProtocolFooter *ftr;
	OspProtocolHeader *hdr;
	gsize actual_length;
	gsize checksum_length = 16; /* always for MD5 */
	gsize payload_length = 0;
	gsize offset_rd = 0;
	gsize offset_wr = 0;
	guint i;
	_cleanup_checksum_free_ GChecksum *csum = NULL;
	_cleanup_free_ guint8 *buffer_in = NULL;
	_cleanup_free_ guint8 *buffer_out = NULL;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	g_return_val_if_fail (data_in_length <= 16, FALSE); //FIXME
	g_assert (sizeof(OspProtocolHeader) + sizeof(OspProtocolFooter) == 64);

	/* write header to buffer */
	buffer_in = g_new0 (guint8, OSP_DEVICE_MAX_MSG_LENGTH);
	hdr = (OspProtocolHeader *) buffer_in;
	hdr->start_bytes = GUINT16_TO_BE (0xc1c0);
	hdr->protocol_version = 0x1000;
	hdr->checksum_type = OSP_HEADER_CHECKSUM_KIND_MD5;
	hdr->message_type = cmd;
	hdr->bytes_remaining = sizeof(OspProtocolFooter);
	if (data_out == NULL)
		hdr->flags = OSP_HEADER_FLAG_ACK_REQUIRED;
	if (data_in_length > 0) {
		if (data_in_length <= 16) {
			/* avoid another USB packet if we can */
			hdr->immediate_data_length = data_in_length;
			memcpy (hdr->immediate_data, data_in, data_in_length);
		} else {
			payload_length = data_in_length;
		}
	}
	offset_wr += sizeof(OspProtocolHeader);
	hdr->bytes_remaining = sizeof(OspProtocolFooter) + payload_length;

	/* write payload to buffer, if any */
	if (payload_length > 0) {
		memcpy (buffer_in + offset_wr, data_in, data_in_length);
		offset_wr += payload_length;
	}

	/* write footer to buffer */
	ftr = (OspProtocolFooter *) (buffer_in + offset_wr);
	ftr->end_bytes = GUINT32_TO_BE (0xc5c4c3c2);
	csum = g_checksum_new (G_CHECKSUM_MD5);
	g_checksum_update (csum, (const guchar *) buffer_in, offset_wr);
	g_checksum_get_digest (csum, ftr->checksum, &checksum_length);
	offset_wr += sizeof(OspProtocolFooter);

	/* send data */
	cd_buffer_debug (CD_BUFFER_KIND_REQUEST, buffer_in, offset_wr);
	if (!g_usb_device_bulk_transfer (device, 0x01,
					 buffer_in, offset_wr,
					 &actual_length,
					 OSP_USB_TIMEOUT_MS, NULL, error))
		return NULL;

	/* get reply */
	buffer_out = g_new0 (guint8, 64);
	if (!g_usb_device_bulk_transfer (device, 0x81,
					 buffer_out, OSP_DEVICE_EP_SIZE,
					 &actual_length,
					 OSP_USB_TIMEOUT_MS, NULL, error))
		return NULL;
	cd_buffer_debug (CD_BUFFER_KIND_RESPONSE, buffer_out, actual_length);

	/* check the error code */
	hdr = (OspProtocolHeader *) buffer_out;
	if (hdr->error_code != OSP_ERROR_CODE_SUCCESS) {
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_INTERNAL,
			     "Failed to %s: %s",
			     osp_cmd_to_string (cmd),
			     osp_error_code_to_string (hdr->error_code));
		return FALSE;
	}

	/* copy out the data */
	offset_rd = sizeof(OspProtocolHeader);
	if (data_out != NULL && data_out_length != NULL) {
		if (hdr->immediate_data_length > 0) {
			*data_out_length = hdr->immediate_data_length;
			*data_out = g_memdup (hdr->immediate_data,
					      hdr->immediate_data_length);
		} else if (hdr->bytes_remaining >= sizeof(OspProtocolFooter)) {
			*data_out_length = hdr->bytes_remaining - sizeof(OspProtocolFooter);
			*data_out = g_new0 (guint8, hdr->bytes_remaining);

			/* copy the first chunk of data */
			offset_wr = 64 - offset_rd;
			memcpy (*data_out, buffer_out + offset_rd, offset_wr);
		} else {
			g_assert_not_reached ();
		}
	}

	/* read the rest of the payload */
	payload_length = hdr->bytes_remaining - sizeof(OspProtocolFooter);
	for (i = 0; i < payload_length / 64; i++) {
		if (!g_usb_device_bulk_transfer (device, 0x81,
						 buffer_out, OSP_DEVICE_EP_SIZE,
						 &actual_length,
						 OSP_USB_TIMEOUT_MS, NULL, error))
			return NULL;
		memcpy (*data_out + offset_wr, buffer_out, OSP_DEVICE_EP_SIZE);
		cd_buffer_debug (CD_BUFFER_KIND_RESPONSE, buffer_out, OSP_DEVICE_EP_SIZE);
		offset_wr += 64;
	}
	offset_rd += payload_length;

	/* verify the footer is intact */
	ftr = (OspProtocolFooter *) (buffer_out + OSP_DEVICE_EP_SIZE - sizeof(OspProtocolFooter));
	if (ftr->end_bytes != GUINT32_TO_BE (0xc5c4c3c2)) {
		g_set_error_literal (error,
				     OSP_DEVICE_ERROR,
				     OSP_DEVICE_ERROR_INTERNAL,
				     "Footer invalid");
		return FALSE;
	}

	return TRUE;
}

/**
 * osp_device_send_command:
 *
 * Since: 1.2.11
 **/
static gboolean
osp_device_send_command (GUsbDevice *device, OspCmd cmd,
			 const guint8 *data_in, gsize data_in_length,
			 GError **error)
{
	return osp_device_query (device, cmd,
				 data_in, data_in_length,
				 NULL, NULL, error);
}

/**
 * osp_device_get_serial:
 *
 * Since: 1.2.11
 **/
gchar *
osp_device_get_serial (GUsbDevice *device, GError **error)
{
	gsize data_len;
	_cleanup_free_ guint8 *data = NULL;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* query hardware */
	if (!osp_device_query (device, OSP_CMD_GET_SERIAL_NUMBER,
			       NULL, 0, &data, &data_len, error))
		return NULL;

	/* check values */
	if (data_len == 0) {
		g_set_error_literal (error,
				     OSP_DEVICE_ERROR,
				     OSP_DEVICE_ERROR_INTERNAL,
				     "Expected serial number, got nothing");
		return FALSE;
	}

	/* format value */
	return g_strdup ((const gchar *) data);
}

/**
 * osp_device_get_fw_version:
 *
 * Since: 1.2.11
 **/
gchar *
osp_device_get_fw_version (GUsbDevice *device, GError **error)
{
	gsize data_len;
	_cleanup_free_ guint8 *data = NULL;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* query hardware */
	if (!osp_device_query (device, OSP_CMD_GET_FIRMWARE_VERSION,
			       NULL, 0, &data, &data_len, error))
		return NULL;

	/* check values */
	if (data_len != 2) {
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_INTERNAL,
			     "Expected %i bytes, got %li", 2, data_len);
		return FALSE;
	}

	/* format value */
	return g_strdup_printf ("%i.%i", data[1], data[0]);
}

/**
 * osp_device_take_spectrum:
 *
 * Since: 1.2.11
 **/
CdSpectrum *
osp_device_take_spectrum (GUsbDevice *device, GError **error)
{
	CdSpectrum *sp;
	gdouble val;
	gsize data_len;
	guint32 sample_duration = 100000; /* us */
	guint8 bin_factor = 0;
	guint i;
	_cleanup_free_ guint8 *data = NULL;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* return every pixel */
	if (!osp_device_send_command (device, OSP_CMD_SET_PIXEL_BINNING_FACTOR,
				      &bin_factor, 1, error))
		return NULL;

	/* set integral time in us */
	if (!osp_device_send_command (device, OSP_CMD_SET_INTEGRATION_TIME,
				      (guint8 *) &sample_duration, 4, error))
		return NULL;

	/* get spectrum */
	if (!osp_device_query (device, OSP_CMD_GET_AND_SEND_RAW_SPECTRUM,
			       NULL, 0, &data, &data_len, error))
		return NULL;

	/* check values */
	if (data_len != 2048) {
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_INTERNAL,
			     "Expected %i bytes, got %li", 2048, data_len);
		return FALSE;
	}

	/* export */
	sp = cd_spectrum_sized_new (1024);
	cd_spectrum_set_id (sp, "raw");
	cd_spectrum_set_start (sp, 380);
	cd_spectrum_set_end (sp, 700);
	cd_spectrum_set_norm (sp, FALSE);
	for (i = 0; i < 1024; i++) {
		val = data[i*2+1] * 256 + data[i*2+0];
		cd_spectrum_add_value (sp, val / (gdouble) 0xffff);
	}
	return sp;
}
