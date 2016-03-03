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
 * @device: a #GUsbDevice instance
 * @error: A #GError or %NULL
 *
 * Opens the device and claims the interface
 *
 * Return value: %TRUE for success
 *
 * Since: 1.2.11
 **/
gboolean
osp_device_open (GUsbDevice *device, GError **error)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!g_usb_device_open (device, error))
		return FALSE;
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
	g_autoptr(GChecksum) csum = NULL;
	g_autofree guint8 *buffer_in = NULL;
	g_autofree guint8 *buffer_out = NULL;

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
	if (g_getenv ("SPARK_PROTOCOL_DEBUG") != NULL)
		cd_buffer_debug (CD_BUFFER_KIND_REQUEST, buffer_in, offset_wr);
	if (!g_usb_device_bulk_transfer (device, 0x01,
					 buffer_in, offset_wr,
					 &actual_length,
					 OSP_USB_TIMEOUT_MS, NULL, error))
		return FALSE;

	/* get reply */
	buffer_out = g_new0 (guint8, 64);
	if (!g_usb_device_bulk_transfer (device, 0x81,
					 buffer_out, OSP_DEVICE_EP_SIZE,
					 &actual_length,
					 OSP_USB_TIMEOUT_MS, NULL, error))
		return FALSE;
	if (g_getenv ("SPARK_PROTOCOL_DEBUG") != NULL)
		cd_buffer_debug (CD_BUFFER_KIND_RESPONSE, buffer_out, actual_length);

	/* check the error code */
	hdr = (OspProtocolHeader *) buffer_out;
	switch (hdr->error_code) {
	case OSP_ERROR_CODE_SUCCESS:
		break;
	case OSP_ERROR_CODE_MESSAGE_TOO_LARGE:
	case OSP_ERROR_CODE_UNKNOWN_CHECKSUM_TYPE:
	case OSP_ERROR_CODE_UNSUPPORTED_PROTOCOL:
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_NO_SUPPORT,
			     "Failed to %s",
			     osp_cmd_to_string (cmd));
		return FALSE;
		break;
	case OSP_ERROR_CODE_COMMAND_DATA_MISSING:
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_NO_DATA,
			     "Failed to %s",
			     osp_cmd_to_string (cmd));
		return FALSE;
		break;
	default:
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_INTERNAL,
			     "Failed to %s: %s",
			     osp_cmd_to_string (cmd),
			     osp_error_code_to_string (hdr->error_code));
		return FALSE;
		break;
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
			return FALSE;
		memcpy (*data_out + offset_wr, buffer_out, OSP_DEVICE_EP_SIZE);
		if (g_getenv ("SPARK_PROTOCOL_DEBUG") != NULL)
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
 * @device: a #GUsbDevice instance
 * @error: A #GError or %NULL
 *
 * Gets the device serial number.
 *
 * Return value: A string, or %NULL for failure
 *
 * Since: 1.2.11
 **/
gchar *
osp_device_get_serial (GUsbDevice *device, GError **error)
{
	gsize data_len;
	g_autofree guint8 *data = NULL;

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
		return NULL;
	}

	/* format value */
	return g_strndup ((const gchar *) data, data_len);
}

/**
 * osp_device_get_fw_version:
 * @device: a #GUsbDevice instance
 * @error: A #GError or %NULL
 *
 * Opens the device and claims the interface
 *
 * Return value: The firmware version, or %NULL for error
 *
 * Since: 1.2.11
 **/
gchar *
osp_device_get_fw_version (GUsbDevice *device, GError **error)
{
	gsize data_len;
	g_autofree guint8 *data = NULL;

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
		return NULL;
	}

	/* format value */
	return g_strdup_printf ("%i.%i", data[1], data[0]);
}

/**
 * osp_device_get_wavelength_cal_for_idx:
 *
 * Since: 1.3.1
 **/
static gboolean
osp_device_get_wavelength_cal_for_idx (GUsbDevice *device,
				       guint idx,
				       gfloat *cal,
				       GError **error)
{
	gsize data_len;
	guint8 idx_buf[1] = { idx };
	g_autofree guint8 *data = NULL;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* query hardware */
	if (!osp_device_query (device, OSP_CMD_GET_WAVELENGTH_COEFFICIENT,
			       idx_buf, 1, &data, &data_len, error))
		return FALSE;

	/* check values */
	if (data_len != 4) {
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_INTERNAL,
			     "Expected %i bytes, got %li", 4, data_len);
		return FALSE;
	}

	/* convert to floating point */
	if (cal != NULL)
		*cal = *((gfloat *) data);

	/* format value */
	return TRUE;
}

/**
 * osp_device_get_wavelength_start:
 * @device: a #GUsbDevice instance
 * @error: A #GError or %NULL
 *
 * Gets the starting wavelength for the sensor.
 *
 * Return value: A value in nm, or -1 for error
 *
 * Since: 1.3.1
 **/
gdouble
osp_device_get_wavelength_start (GUsbDevice *device, GError **error)
{
	gfloat tmp = -1.f;

	/* get from hardware */
	if (!osp_device_get_wavelength_cal_for_idx (device, 0, &tmp, error))
		return -1.f;

	/* check values */
	if (tmp < 0) {
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_INTERNAL,
			     "Not a valid start, got %f", tmp);
		return -1.f;
	}

	return (gdouble) tmp;
}

/**
 * osp_device_get_wavelength_cal:
 * @device: a #GUsbDevice instance
 * @length: the size of the returned array
 * @error: A #GError or %NULL
 *
 * Gets the wavelength coefficients for the sensor.
 *
 * Return value: An array of coefficients
 *
 * Since: 1.3.1
 **/
gdouble *
osp_device_get_wavelength_cal (GUsbDevice *device, guint *length, GError **error)
{
	gboolean ret;
	gdouble *coefs = NULL;
	gfloat cx;
	gsize data_len;
	guint i;
	g_autofree guint8 *data = NULL;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* query hardware */
	if (!osp_device_query (device, OSP_CMD_GET_WAVELENGTH_COEFFICIENT_COUNT,
			       NULL, 0, &data, &data_len, error))
		return NULL;

	/* check values */
	if (data_len != 1) {
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_INTERNAL,
			     "Expected 1 bytes, got %li", data_len);
		return NULL;
	}

	/* check sanity */
	if (data[0] != 4) {
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_INTERNAL,
			     "Expected 4 coefs, got %i", data[0]);
		return NULL;
	}

	/* get the coefs */
	coefs = g_new0 (gdouble, data[0] - 1);
	for (i = 0; i < (guint) data[0] - 1; i++) {
		ret = osp_device_get_wavelength_cal_for_idx (device,
							     i + 1,
							     &cx,
							     error);
		if (!ret)
			return NULL;
		coefs[i] = cx;
	}

	/* this is optional */
	if (length != NULL)
		*length = data[0] - 1;

	/* success */
	return coefs;
}

/**
 * osp_device_get_nonlinearity_cal_for_idx:
 *
 * Since: 1.3.1
 **/
static gboolean
osp_device_get_nonlinearity_cal_for_idx (GUsbDevice *device,
					 guint idx,
					 gfloat *cal,
					 GError **error)
{
	gsize data_len;
	guint8 idx_buf[1] = { idx };
	g_autofree guint8 *data = NULL;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* query hardware */
	if (!osp_device_query (device, OSP_CMD_GET_NONLINEARITY_COEFFICIENT,
			       idx_buf, 1, &data, &data_len, error))
		return FALSE;

	/* check values */
	if (data_len != 4) {
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_INTERNAL,
			     "Expected %i bytes, got %li", 4, data_len);
		return FALSE;
	}

	/* convert to floating point */
	if (cal != NULL)
		*cal = *((gfloat *) data);

	/* format value */
	return TRUE;
}

/**
 * osp_device_get_nonlinearity_cal:
 * @device: a #GUsbDevice instance
 * @length: the size of the returned array
 * @error: A #GError or %NULL
 *
 * Gets the nonlinearity values for the sensor.
 *
 * Return value: An array of coefficients
 *
 * Since: 1.3.1
 **/
gdouble *
osp_device_get_nonlinearity_cal (GUsbDevice *device, guint *length, GError **error)
{
	gboolean ret;
	gdouble *coefs = NULL;
	gfloat cx;
	gsize data_len;
	guint i;
	g_autofree guint8 *data = NULL;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* query hardware */
	if (!osp_device_query (device, OSP_CMD_GET_NONLINEARITY_COEFFICIENT_COUNT,
			       NULL, 0, &data, &data_len, error))
		return NULL;

	/* check values */
	if (data_len != 1) {
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_INTERNAL,
			     "Expected 1 bytes, got %li", data_len);
		return NULL;
	}

	/* check sanity */
	if (data[0] != 8) {
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_INTERNAL,
			     "Expected 8 coefs, got %i", data[0]);
		return NULL;
	}

	/* get the coefs */
	coefs = g_new0 (gdouble, data[0]);
	for (i = 0; i < data[0]; i++) {
		ret = osp_device_get_nonlinearity_cal_for_idx (device,
							       i,
							       &cx,
							       error);
		if (!ret)
			return NULL;
		coefs[i] = cx;
	}

	/* this is optional */
	if (length != NULL)
		*length = data[0];

	/* success */
	return coefs;
}

/**
 * osp_device_get_irradiance_cal:
 * @device: a #GUsbDevice instance
 * @length: the size of the returned array
 * @error: A #GError or %NULL
 *
 * Gets the irradiance spectrum for the sensor.
 *
 * Return value: An array of coefficients
 *
 * Since: 1.3.1
 **/
gdouble *
osp_device_get_irradiance_cal (GUsbDevice *device, guint *length, GError **error)
{
	gdouble *coefs = NULL;
	gsize data_len;
	guint i;
	g_autofree guint8 *data = NULL;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* query hardware */
	if (!osp_device_query (device, OSP_CMD_GET_IRRADIANCE_CALIBRATION,
			       NULL, 0, &data, &data_len, error))
		return NULL;

	/* check values */
	if (data_len != 4096 * 4) {
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_INTERNAL,
			     "Expected %i bytes, got %li", 4096 * 4, data_len);
		return NULL;
	}

	/* copy out the coefs */
	coefs = g_new0 (gdouble, 4096);
	for (i = 0; i < 4096; i++)
		coefs[i] = *((gfloat *) &data[i*4]);

	/* this is optional */
	if (length != NULL)
		*length = 4096;

	/* success */
	return coefs;
}

/**
 * osp_device_take_spectrum_internal:
 **/
static CdSpectrum *
osp_device_take_spectrum_internal (GUsbDevice *device,
				   guint64 sample_duration,
				   GError **error)
{
	CdSpectrum *sp;
	gdouble val;
	gsize data_len;
	guint i;
	g_autofree guint8 *data = NULL;
	g_autoptr(GTimer) t = NULL;

	/* set integral time in us */
	if (!osp_device_send_command (device, OSP_CMD_SET_INTEGRATION_TIME,
				      (guint8 *) &sample_duration, 4, error))
		return NULL;

	/* get spectrum */
	t = g_timer_new ();
	if (!osp_device_query (device, OSP_CMD_GET_AND_SEND_RAW_SPECTRUM,
			       NULL, 0, &data, &data_len, error))
		return NULL;
	g_debug ("For integration of %.0fms, sensor took %.0fms",
		 sample_duration / 1000.f, g_timer_elapsed (t, NULL) * 1000.f);

	/* check values */
	if (data_len != 2048) {
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_INTERNAL,
			     "Expected %i bytes, got %li", 2048, data_len);
		return NULL;
	}

	/* export */
	sp = cd_spectrum_sized_new (1024);
	for (i = 0; i < 1024; i++) {
		val = data[i*2+1] * 256 + data[i*2+0];
		cd_spectrum_add_value (sp, val / (gdouble) 0xffff);
	}

	/* the maximum value the hardware can return is 0x3fff */
	val = cd_spectrum_get_value_max (sp);
	if (val > 0.25) {
		g_set_error (error,
			     OSP_DEVICE_ERROR,
			     OSP_DEVICE_ERROR_INTERNAL,
			     "spectral max should be <= 0.25f, was %f",
			     val);
		cd_spectrum_free (sp);
		return NULL;
	}

	return sp;
}

/**
 * osp_device_take_spectrum_full:
 * @device: a #GUsbDevice instance
 * @sample_duration: the sample duration in µs
 * @error: A #GError or %NULL
 *
 * Returns a spectrum for a set sample duration.
 *
 * Return value: A #CdSpectrum, or %NULL for error
 *
 * Since: 1.3.1
 **/
CdSpectrum *
osp_device_take_spectrum_full (GUsbDevice *device,
			       guint64 sample_duration,
			       GError **error)
{
	CdSpectrum *sp;
	gdouble start;
	guint8 bin_factor = 0;
	g_autofree gdouble *cx = NULL;
	g_autoptr(CdSpectrum) sp_dc = NULL;
	g_autoptr(CdSpectrum) sp_raw = NULL;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* return every pixel */
	if (!osp_device_send_command (device, OSP_CMD_SET_PIXEL_BINNING_FACTOR,
				      &bin_factor, 1, error))
		return NULL;

	/* get spectrum */
	sp_raw = osp_device_take_spectrum_internal (device, sample_duration, error);
	if (sp_raw == NULL)
		return NULL;
	cd_spectrum_set_id (sp_raw, "raw");

	/* remove any DC offset from the sensor by doing a 10us reading --
	 * ideally this would be 0us, but we have to use what we have */
	sp_dc = osp_device_take_spectrum_internal (device, 10, error);
	if (sp_dc == NULL)
		return NULL;
	cd_spectrum_set_id (sp_dc, "dc");

	/* get coefficients */
	cx = osp_device_get_wavelength_cal (device, NULL, error);
	if (cx == NULL)
		return NULL;

	/* get start */
	start = osp_device_get_wavelength_start (device, error);
	if (start < 0)
		return NULL;

	/* return the reading without a DC component */
	sp = cd_spectrum_subtract (sp_raw, sp_dc, 5);
	cd_spectrum_set_start (sp, start);
	cd_spectrum_set_norm (sp, 4);
	cd_spectrum_set_wavelength_cal (sp, cx[0], cx[1], cx[2]);
	return sp;
}

/**
 * osp_device_take_spectrum:
 * @device: a #GUsbDevice instance.
 * @error: A #GError or %NULL
 *
 * Returns a spectrum. The optimal sample duration is calculated automatically.
 *
 * Return value: A #CdSpectrum, or %NULL for error
 *
 * Since: 1.2.11
 **/
CdSpectrum *
osp_device_take_spectrum (GUsbDevice *device, GError **error)
{
	const guint sample_duration_max_secs = 3;
	gboolean relax_requirements = FALSE;
	gdouble max;
	gdouble scale = 0.f;
	guint64 sample_duration = 10000; /* us */
	CdSpectrum *sp = NULL;
	guint i;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* loop until we're in 1/4 to 3/4 FSD */
	for (i = 0; i < 5; i++) {
		g_autoptr(CdSpectrum) sp_probe = NULL;

		/* for the last try, relax what we deem acceptable so we can
		 * measure very black things with a long integration time */
		if (i == 4)
			relax_requirements = TRUE;

		/* take a measurement */
		sp_probe = osp_device_take_spectrum_full (device,
							  sample_duration,
							  error);
		if (sp_probe == NULL)
			return NULL;

		/* sensor picked up nothing, take action */
		max = cd_spectrum_get_value_max (sp_probe);
		if (max < 0.001f) {
			sample_duration *= 100.f;
			g_debug ("sensor read no data, setting duration to %luus",
				 sample_duration);
			continue;
		}

		/* sensor is saturated, take action */
		if (max > 0.99f) {
			sample_duration /= 100.f;
			g_debug ("sensor saturated, setting duration to %luus",
				 sample_duration);
			continue;
		}

		/* break out if we got valid readings */
		if (max > 0.25f && max < 0.75f) {
			sp = cd_spectrum_dup (sp_probe);
			break;
		}

		/* be more accepting */
		if (relax_requirements && max > 0.01f) {
			sp = cd_spectrum_dup (sp_probe);
			break;
		}

		/* aim for FSD / 2 */
		scale = (gdouble) 0.5 / max;
		sample_duration *= scale;
		g_debug ("for max of %f, using scale=%f for duration %luus",
			 max, scale, sample_duration);

		/* limit this to something sane */
		if (sample_duration / G_USEC_PER_SEC > sample_duration_max_secs) {
			g_debug ("limiting duration from %lus to %is",
				 sample_duration / G_USEC_PER_SEC,
				 sample_duration_max_secs);
			sample_duration = sample_duration_max_secs * G_USEC_PER_SEC;
			relax_requirements = TRUE;
		}
	}

	/* no suitable readings */
	if (sp == NULL) {
		g_set_error_literal (error,
				     OSP_DEVICE_ERROR,
				     OSP_DEVICE_ERROR_NO_DATA,
				     "Got no valid data");
		return NULL;
	}

	/* scale with the new integral time */
	cd_spectrum_set_norm (sp, cd_spectrum_get_norm (sp) / scale);
	g_debug ("normalised spectral max is %f", cd_spectrum_get_value_max (sp));
	return sp;
}
