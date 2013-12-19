/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <gusb.h>
#include <math.h>
#include <colord.h>

#include "ch-math.h"
#include "ch-hash.h"
#include "ch-device.h"
#include "ch-device-queue.h"

static void
ch_test_hash_func (void)
{
	ChSha1 sha1;
	gboolean ret;
	gchar *str;
	GError *error = NULL;

	/* parse into structure */
	ret = ch_sha1_parse ("f18973b4ebaeab527dc15d5dd246debfbff20324",
			     &sha1, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (sha1.bytes[0], == ,241);
	g_assert_cmpint (sha1.bytes[1], ==, 137);

	/* print back to string */
	str = ch_sha1_to_string (&sha1);
	g_assert_cmpstr (str, ==, "f18973b4ebaeab527dc15d5dd246debfbff20324");
	g_free (str);
}

static guint device_failed_cnt = 0;
static guint progress_changed_cnt = 0;

static void
ch_test_device_queue_device_failed_cb (ChDeviceQueue	*device_queue,
				       GUsbDevice	*device,
				       const gchar	*error_message,
				       gpointer		 user_data)
{
	device_failed_cnt++;
	g_debug ("device %s down, error: %s",
		 g_usb_device_get_platform_id (device),
		 error_message);
}

static void
ch_test_device_queue_progress_changed_cb (ChDeviceQueue	*device_queue,
					  guint		 percentage,
					  gpointer	 user_data)
{
	progress_changed_cnt++;
	g_debug ("queue complete %i%%",
		 percentage);
}

static void
ch_test_device_queue_func (void)
{
	ChDeviceQueue *device_queue;
	gboolean ret;
	GError *error = NULL;
	GPtrArray *devices;
	guint i;
	guint valid_devices = 0;
	GUsbContext *usb_ctx;
	GUsbDevice *device;
	GUsbDeviceList *list;

	/* try to find any ColorHug devices */
	usb_ctx = g_usb_context_new (NULL);
	if (usb_ctx == NULL)
		return;

	list = g_usb_device_list_new (usb_ctx);
	g_usb_device_list_coldplug (list);
	devices = g_usb_device_list_get_devices (list);

	/* watch for any failed devices */
	device_queue = ch_device_queue_new ();
	g_signal_connect (device_queue,
			  "device-failed",
			  G_CALLBACK (ch_test_device_queue_device_failed_cb),
			  NULL);
	g_signal_connect (device_queue,
			  "progress-changed",
			  G_CALLBACK (ch_test_device_queue_progress_changed_cb),
			  NULL);
	for (i = 0; i < devices->len; i++) {
		device = g_ptr_array_index (devices, i);
		if (g_usb_device_get_vid (device) != CH_USB_VID)
			continue;
		if (g_usb_device_get_pid (device) != CH_USB_PID_FIRMWARE &&
		    g_usb_device_get_pid (device) != CH_USB_PID_FIRMWARE_SPECTRO)
			continue;

		valid_devices++;
		g_debug ("Found ColorHug device %s",
			 g_usb_device_get_platform_id (device));

		/* load device */
		ret = ch_device_open (device, &error);
		g_assert_no_error (error);
		g_assert (ret);

		/* set RED to queue */
		ch_device_queue_set_leds (device_queue,
				          device,
				          CH_STATUS_LED_RED,
				          50,
				          100,
				          5);

		/* set GREEN to queue */
		ch_device_queue_set_leds (device_queue,
				          device,
				          CH_STATUS_LED_GREEN,
				          50,
				          100,
				          5);

		/* do unknown command */
		ch_device_queue_add (device_queue,
				     device,
				     0xff,
				     NULL,
				     0,
				     NULL,
				     0);

		/* set ALL to queue */
		ch_device_queue_set_leds (device_queue,
				          device,
				          CH_STATUS_LED_RED |
				          CH_STATUS_LED_GREEN |
				          CH_STATUS_LED_BLUE,
				          50,
				          100,
				          5);
	}

	/* fix make check with no hardware attached */
	if (valid_devices == 0)
		goto out;

	/* process queue */
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONFATAL_ERRORS,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check we failed both devices */
	g_assert_cmpint (device_failed_cnt, ==, valid_devices);

	/* chekc we got enough progress updates */
	if (valid_devices > 0)
		g_assert_cmpint (progress_changed_cnt, ==, valid_devices * 3 + 1);

	/* fail on unknown command */
	for (i = 0; i < devices->len; i++) {
		device = g_ptr_array_index (devices, i);
		if (g_usb_device_get_vid (device) != CH_USB_VID)
			continue;
		if (g_usb_device_get_pid (device) != CH_USB_PID_FIRMWARE)
			continue;
		ch_device_queue_add (device_queue,
				     device,
				     0xff,
				     NULL,
				     0,
				     NULL,
				     0);
	}

	/* process queue */
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_error (error, 1, 0);
	g_debug ("error was: %s", error->message);
	g_assert (!ret);
	g_error_free (error);
out:
	g_ptr_array_unref (devices);
	g_object_unref (device_queue);
	g_object_unref (list);
	g_object_unref (usb_ctx);
}

static void
ch_test_math_convert_func (void)
{
	ChPackedFloat pf;
	gdouble value = 0.0f;

	/* test packing */
	g_assert_cmpint (sizeof (ChPackedFloat), ==, 4);

	/* test converting to packed struct */
	value = 3.1415927f;
	ch_double_to_packed_float (value, &pf);
	pf.raw = ch_packed_float_get_value (&pf);
	g_assert_cmpint (pf.offset, ==, 3);
	g_assert_cmpint (pf.fraction, <, 0x249f);
	g_assert_cmpint (pf.fraction, >, 0x240f);

	/* test converting to packed struct */
	value = -3.1415927f;
	ch_double_to_packed_float (value, &pf);
	pf.raw = ch_packed_float_get_value (&pf);
	g_assert_cmpint (pf.offset, ==, -4);
	g_assert_cmpint (pf.fraction, <, (0x240f ^ 0xffff));
	g_assert_cmpint (pf.fraction, >, (0x249f ^ 0xffff));

	/* test converting positive to float */
	pf.offset = 3;
	pf.fraction = 0x243c;
	pf.raw = ch_packed_float_get_value (&pf);
	ch_packed_float_to_double (&pf, &value);
	g_assert_cmpfloat (value, >, 3.1415);
	g_assert_cmpfloat (value, <, 3.1416);

	/* test converting negative to float */
	pf.offset = -4;
	pf.fraction = 0x243b ^ 0xffff;
	ch_packed_float_to_double (&pf, &value);
	pf.raw = ch_packed_float_get_value (&pf);
	g_assert_cmpfloat (value, >, -3.1416);
	g_assert_cmpfloat (value, <, -3.1415);

	/* test converting zero */
	value = 0.0f;
	ch_double_to_packed_float (value, &pf);
	pf.raw = ch_packed_float_get_value (&pf);
	g_assert_cmpint (pf.offset, ==, 0);
	g_assert_cmpint (pf.fraction, ==, 0);
	ch_packed_float_set_value (&pf, pf.raw);
	ch_packed_float_to_double (&pf, &value);
	g_assert_cmpfloat (value, >, -0.001f);
	g_assert_cmpfloat (value, <, +0.001f);

	/* test converting positive */
	value = +1.4f;
	ch_double_to_packed_float (value, &pf);
	pf.raw = ch_packed_float_get_value (&pf);
	g_assert_cmpint (pf.offset, ==, 1);
	g_assert_cmpint (pf.fraction, <, 0x6668);
	g_assert_cmpint (pf.fraction, >, 0x6663);
	ch_packed_float_set_value (&pf, pf.raw);
	ch_packed_float_to_double (&pf, &value);
	g_assert_cmpfloat (value, <, 1.41);
	g_assert_cmpfloat (value, >, 1.39);

	/* test converting negative */
	value = -1.4f;
	ch_double_to_packed_float (value, &pf);
	pf.raw = ch_packed_float_get_value (&pf);
	g_assert_cmpint (pf.offset, ==, -2);
	g_assert_cmpint (pf.fraction, <, (0x6662 ^ 0xffff));
	g_assert_cmpint (pf.fraction, >, (0x6668 ^ 0xffff));
	ch_packed_float_set_value (&pf, pf.raw);
	ch_packed_float_to_double (&pf, &value);
	g_assert_cmpfloat (value, <, -1.39);
	g_assert_cmpfloat (value, >, -1.41);

	/* test converting negative max */
	value = -0x7fff;
	ch_double_to_packed_float (value, &pf);
	pf.raw = ch_packed_float_get_value (&pf);
	g_assert_cmpint (pf.offset, ==, -32767);
	g_assert_cmpint (pf.fraction, ==, 0);
	ch_packed_float_set_value (&pf, pf.raw);
	ch_packed_float_to_double (&pf, &value);
	g_assert_cmpfloat (value, >, -32768.0001);
	g_assert_cmpfloat (value, <, +32767.9999);
}

static void
ch_test_math_add_func (void)
{
	ChPackedFloat pf;
	ChPackedFloat pf_tmp;
	ChPackedFloat pf_result;
	gdouble value = 0.0f;
	guint8 rc;

	/* test addition */
	ch_double_to_packed_float (3.90f, &pf);
	ch_double_to_packed_float (1.40f, &pf_tmp);
	rc = ch_packed_float_add (&pf, &pf_tmp, &pf_result);
	g_assert_cmpint (rc, ==, CH_ERROR_NONE);
	ch_packed_float_to_double (&pf_result, &value);
	g_assert_cmpfloat (value, >, 5.299);
	g_assert_cmpfloat (value, <, 5.310);

	/* test addition with both negative */
	ch_double_to_packed_float (-3.90f, &pf);
	ch_double_to_packed_float (-1.40f, &pf_tmp);
	rc = ch_packed_float_add (&pf, &pf_tmp, &pf_result);
	g_assert_cmpint (rc, ==, CH_ERROR_NONE);
	ch_packed_float_to_double (&pf_result, &value);
	g_assert_cmpfloat (value, >, -5.301);
	g_assert_cmpfloat (value, <, -5.299);

	/* test addition with negative */
	ch_double_to_packed_float (3.20f, &pf);
	ch_double_to_packed_float (-1.50f, &pf_tmp);
	rc = ch_packed_float_add (&pf, &pf_tmp, &pf_result);
	g_assert_cmpint (rc, ==, CH_ERROR_NONE);
	ch_packed_float_to_double (&pf_result, &value);
	g_assert_cmpfloat (value, <, 1.701);
	g_assert_cmpfloat (value, >, 1.699);

	/* test addition with negative */
	ch_double_to_packed_float (3.20f, &pf);
	ch_double_to_packed_float (-10.50f, &pf_tmp);
	rc = ch_packed_float_add (&pf, &pf_tmp, &pf_result);
	g_assert_cmpint (rc, ==, CH_ERROR_NONE);
	ch_packed_float_to_double (&pf_result, &value);
	g_assert_cmpfloat (value, >, -7.301);
	g_assert_cmpfloat (value, <, -7.299);

	/* test addition overflow */
	ch_double_to_packed_float (0x7fff, &pf);
	ch_double_to_packed_float (0x7fff, &pf_tmp);
	rc = ch_packed_float_add (&pf, &pf_tmp, &pf_result);
//	g_assert_cmpint (rc, ==, CH_ERROR_OVERFLOW_ADDITION);
}


static void
ch_test_math_multiply_func (void)
{
	ChPackedFloat pf;
	ChPackedFloat pf_tmp;
	ChPackedFloat pf_result;
	gdouble value = 0.0f;
	gdouble value1;
	gdouble value2;
	guint8 rc;

	/* test safe multiplication */
	ch_double_to_packed_float (0.25f, &pf);
	ch_double_to_packed_float (0.50f, &pf_tmp);
	rc = ch_packed_float_multiply (&pf, &pf_tmp, &pf_result);
	g_assert_cmpint (rc, ==, CH_ERROR_NONE);
	ch_packed_float_to_double (&pf_result, &value);
	g_assert_cmpfloat (value, >, 0.1249);
	g_assert_cmpfloat (value, <, 0.1251);

	/* test multiplication we have to scale */
	ch_double_to_packed_float (3.90f, &pf);
	ch_double_to_packed_float (1.40f, &pf_tmp);
	rc = ch_packed_float_multiply (&pf, &pf_tmp, &pf_result);
	g_assert_cmpint (rc, ==, CH_ERROR_NONE);
	ch_packed_float_to_double (&pf_result, &value);
	g_assert_cmpfloat (value, >, 5.45);
	g_assert_cmpfloat (value, <, 5.47);

	/* test multiplication we have to scale a lot */
	ch_double_to_packed_float (3.90f, &pf);
	ch_double_to_packed_float (200.0f, &pf_tmp);
	rc = ch_packed_float_multiply (&pf, &pf_tmp, &pf_result);
	g_assert_cmpint (rc, ==, CH_ERROR_NONE);
	ch_packed_float_to_double (&pf_result, &value);
	g_assert_cmpfloat (value, >, 778.9);
	g_assert_cmpfloat (value, <, 780.1);

	/* test multiplication of negative */
	ch_double_to_packed_float (3.90f, &pf);
	ch_double_to_packed_float (-1.4f, &pf_tmp);
	rc = ch_packed_float_multiply (&pf, &pf_tmp, &pf_result);
	g_assert_cmpint (rc, ==, CH_ERROR_NONE);
	ch_packed_float_to_double (&pf_result, &value);
	g_assert_cmpfloat (value, <, -5.45);
	g_assert_cmpfloat (value, >, -5.47);

	/* test multiplication of double negative */
	ch_double_to_packed_float (-3.90f, &pf);
	ch_double_to_packed_float (-1.4f, &pf_tmp);
	rc = ch_packed_float_multiply (&pf, &pf_tmp, &pf_result);
	g_assert_cmpint (rc, ==, CH_ERROR_NONE);
	ch_packed_float_to_double (&pf_result, &value);
	g_assert_cmpfloat (value, >, 5.45);
	g_assert_cmpfloat (value, <, 5.47);

	/* test multiplication of very different numbers */
	ch_double_to_packed_float (0.072587f, &pf);
	ch_double_to_packed_float (80.0f, &pf_tmp);
	rc = ch_packed_float_multiply (&pf, &pf_tmp, &pf_result);
	g_assert_cmpint (rc, ==, CH_ERROR_NONE);
	ch_packed_float_to_double (&pf_result, &value);
	g_assert_cmpfloat (value, >, 5.79);
	g_assert_cmpfloat (value, <, 5.81);

	/* be evil */
	for (value1 = -127; value1 < +127; value1 += 0.5f) {
		for (value2 = -127; value2 < +127; value2 += 0.5f) {
			ch_double_to_packed_float (value1, &pf);
			ch_double_to_packed_float (value2, &pf_tmp);
			rc = ch_packed_float_multiply (&pf, &pf_tmp, &pf_result);
			g_assert_cmpint (rc, ==, CH_ERROR_NONE);
			ch_packed_float_to_double (&pf_result, &value);
			g_assert_cmpfloat (value, >, (value1 * value2) - 0.01);
			g_assert_cmpfloat (value, <, (value1 * value2) + 0.01);
		}
	}

	/* test multiplication overflow */
	ch_double_to_packed_float (0x4fff, &pf);
	ch_double_to_packed_float (0x4, &pf_tmp);
	rc = ch_packed_float_multiply (&pf, &pf_tmp, &pf_result);
	g_assert_cmpint (rc, ==, CH_ERROR_OVERFLOW_MULTIPLY);
}

/**
 * ch_client_get_default:
 **/
static GUsbDevice *
ch_client_get_default (GError **error)
{
	gboolean ret;
	GUsbContext *usb_ctx;
	GUsbDevice *device = NULL;
	GUsbDeviceList *list;

	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* try to find the ColorHug device */
	usb_ctx = g_usb_context_new (NULL);
	if (usb_ctx == NULL) {
		g_set_error(error,
			    G_USB_DEVICE_ERROR,
			    G_USB_DEVICE_ERROR_NO_DEVICE,
			    "No device found; USB initialisation failed");
		return NULL;
	}
	list = g_usb_device_list_new (usb_ctx);
	g_usb_device_list_coldplug (list);
	device = g_usb_device_list_find_by_vid_pid (list,
						    CH_USB_VID,
						    CH_USB_PID_FIRMWARE,
						    NULL);
	if (device == NULL) {
		device = g_usb_device_list_find_by_vid_pid (list,
							    CH_USB_VID,
							    CH_USB_PID_FIRMWARE_SPECTRO,
							    error);
	}
	if (device == NULL)
		goto out;
	g_debug ("Found ColorHug device %s",
		 g_usb_device_get_platform_id (device));
	ret = ch_device_open (device, error);
	if (!ret)
		goto out;
out:
	g_object_unref (usb_ctx);
	if (list != NULL)
		g_object_unref (list);
	return device;
}

static void
ch_test_state_func (void)
{
	ChColorSelect color_select = 0;
	ChDeviceQueue *device_queue;
	ChFreqScale multiplier = 0;
	gboolean ret;
	gdouble elapsed;
	GError *error = NULL;
	GTimer *timer;
	guint16 integral_time = 0;
	guint8 leds = 0;
	guint i;
	GUsbDevice *device;

	/* load the device */
	device = ch_client_get_default (&error);
	if (device == NULL && g_error_matches (error,
					       G_USB_DEVICE_ERROR,
					       G_USB_DEVICE_ERROR_NO_DEVICE)) {
		g_debug ("no device, skipping tests");
		g_error_free (error);
		return;
	}
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* verify LEDs */
	device_queue = ch_device_queue_new ();
	ch_device_queue_set_leds (device_queue,
				  device,
				  3,
				  0,
				  0x00,
				  0x00);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	ch_device_queue_get_leds (device_queue,
				  device,
				  &leds);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (leds, ==, 3);

	/* verify color select */
	if (ch_device_get_mode (device) == CH_DEVICE_MODE_FIRMWARE) {
		ch_device_queue_set_color_select (device_queue,
						  device,
						  CH_COLOR_SELECT_BLUE);
		ret = ch_device_queue_process (device_queue,
					       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
					       NULL,
					       &error);
		g_assert_no_error (error);
		g_assert (ret);
		ch_device_queue_get_color_select (device_queue,
						  device,
						  &color_select);
		ret = ch_device_queue_process (device_queue,
					       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
					       NULL,
					       &error);
		g_assert_no_error (error);
		g_assert (ret);
		g_assert_cmpint (color_select, ==, CH_COLOR_SELECT_BLUE);

		/* verify multiplier */
		ch_device_queue_set_multiplier (device_queue,
						device,
						CH_FREQ_SCALE_2);
		ret = ch_device_queue_process (device_queue,
					       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
					       NULL,
					       &error);
		g_assert_no_error (error);
		g_assert (ret);
		ch_device_queue_get_multiplier (device_queue,
						device,
						&multiplier);
		ret = ch_device_queue_process (device_queue,
					       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
					       NULL,
					       &error);
		g_assert_no_error (error);
		g_assert (ret);
		g_assert_cmpint (multiplier, ==, CH_FREQ_SCALE_2);
	}

	/* verify integral */
	ch_device_queue_set_integral_time (device_queue,
					   device,
					   100);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	ch_device_queue_get_integral_time (device_queue,
					   device,
					   &integral_time);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (integral_time, ==, 100);

	/* verify sram access time */
	if (ch_device_get_mode (device) == CH_DEVICE_MODE_FIRMWARE_SPECTRO) {
		guint8 data[3500*2];
		for (i = 0; i < sizeof(data); i++)
			data[i] = i;

		/* test writing */
		timer = g_timer_new ();
		ch_device_queue_write_sram (device_queue,
					    device,
					    0x0000,
					    data,
					    sizeof(data));
		ret = ch_device_queue_process (device_queue,
					       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
					       NULL,
					       &error);
		g_assert_no_error (error);
		g_assert (ret);
		elapsed = g_timer_elapsed (timer, NULL);
		g_debug ("%" G_GSIZE_FORMAT " writes in %.1fms",
			 sizeof(data) / 60, elapsed * 1000);
		g_assert_cmpfloat (elapsed, <, 0.75);

		/* test reading */
		g_timer_reset (timer);
		ch_device_queue_read_sram (device_queue,
					   device,
					   0x0000,
					   data,
					   sizeof(data));
		ret = ch_device_queue_process (device_queue,
					       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
					       NULL,
					       &error);
		g_assert_no_error (error);
		g_assert (ret);
		elapsed = g_timer_elapsed (timer, NULL);
		g_debug ("%" G_GSIZE_FORMAT " reads in %.1fms",
			 sizeof(data) / 60, elapsed * 1000);
		g_assert_cmpfloat (elapsed, <, 0.75);
		g_timer_destroy (timer);
	}

	g_object_unref (device_queue);
}

static void
ch_test_eeprom_func (void)
{
	gboolean ret;
	GError *error = NULL;
	guint16 major = 0;
	guint16 micro = 0;
	guint16 minor = 0;
	guint8 types = 0;
	CdColorRGB value;
	gdouble post_scale = 0;
	gdouble post_scale_tmp = 0;
	gdouble pre_scale = 0;
	gdouble pre_scale_tmp = 0;
	guint32 serial_number = 0;
	CdMat3x3 calibration;
	CdMat3x3 calibration_tmp;
	gchar desc[24];
	GUsbDevice *device;
	ChDeviceQueue *device_queue;

	/* load the device */
	device = ch_client_get_default (&error);
	if (device == NULL && g_error_matches (error,
					       G_USB_DEVICE_ERROR,
					       G_USB_DEVICE_ERROR_NO_DEVICE)) {
		g_debug ("no device, skipping tests");
		g_error_free (error);
		return;
	}
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* only run the destructive tests on a device that is blank */
	device_queue = ch_device_queue_new ();
	ch_device_queue_get_serial_number (device_queue,
					   device,
					   &serial_number);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	if (serial_number != 0) {
		g_debug ("not resetting device as bad serial, skipping tests");
		return;
	}

	/* write eeprom with wrong code */
	ch_device_queue_write_eeprom (device_queue,
				      device,
				     "hello dave");
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_error (error, 1, 0);
	g_assert (!ret);
	g_clear_error (&error);

	/* verify serial number */
	ch_device_queue_set_serial_number (device_queue,
					   device,
					   12345678);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	ch_device_queue_get_serial_number (device_queue,
					   device,
					   &serial_number);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (serial_number, ==, 12345678);

	/* verify firmware */
	ch_device_queue_get_firmware_ver (device_queue,
					  device,
					  &major,
					  &minor,
					  &micro);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (major, ==, 1);
	g_assert_cmpint (minor, ==, 0);
	g_assert_cmpint (micro, >, 0);

	/* verify dark offsets */
	value.R = 0.12;
	value.G = 0.34;
	value.B = 0.56;
	ch_device_queue_set_dark_offsets (device_queue,
					  device,
					  &value);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	ch_device_queue_get_dark_offsets (device_queue,
					  device,
					  &value);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (value.R, ==, 0.12);
	g_assert_cmpint (value.G, ==, 0.34);
	g_assert_cmpint (value.B, ==, 0.56);

	/* verify calibration */
	calibration.m00 = 1.0f;
	calibration.m01 = 2.0f;
	calibration.m02 = 3.0f;
	calibration.m10 = 4.0f;
	calibration.m11 = 5.0f;
	calibration.m12 = 6.0f;
	calibration.m20 = 7.0f;
	calibration.m21 = 8.0f;
	calibration.m22 = 9.0f;
	ch_device_queue_set_calibration (device_queue,
					 device,
					 60,
					 &calibration,
					 CH_CALIBRATION_TYPE_CRT,
					 "test0");
	ch_device_queue_set_calibration (device_queue,
					 device,
					 61,
					 &calibration,
					 CH_CALIBRATION_TYPE_PROJECTOR,
					 "test1");
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	ch_device_queue_set_calibration (device_queue,
					 device,
					 60,
					 &calibration,
					 CH_CALIBRATION_TYPE_CRT,
					 "test0");
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* read back data */
	ch_device_queue_get_calibration (device_queue,
					 device,
					 60,
					 &calibration_tmp,
					 &types,
					 desc);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert (memcmp (&calibration_tmp,
			  &calibration,
			  sizeof (gfloat) * 9) == 0);
	g_assert_cmpint (types, ==, CH_CALIBRATION_TYPE_CRT);
	g_assert_cmpstr (desc, ==, "test0");
	ch_device_queue_get_calibration (device_queue,
					 device,
					 61,
					 &calibration_tmp,
					 &types,
					 desc);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert (memcmp (&calibration_tmp,
			  &calibration,
			  sizeof (gfloat) * 9) == 0);
	g_assert_cmpint (types, ==, CH_CALIBRATION_TYPE_PROJECTOR);
	g_assert_cmpstr (desc, ==, "test1");

	/* verify post scale */
	post_scale = 127.8f;
	ch_device_queue_set_post_scale (device_queue,
					device,
					post_scale);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);

	ch_device_queue_get_post_scale (device_queue,
					device,
					&post_scale_tmp);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpfloat (fabs (post_scale - post_scale_tmp), <, 0.0001);

	/* verify pre scale */
	pre_scale = 1.23f;
	ch_device_queue_set_pre_scale (device_queue,
				       device,
				       pre_scale);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);

	ch_device_queue_get_pre_scale (device_queue,
				       device,
				       &pre_scale_tmp);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpfloat (fabs (pre_scale - pre_scale_tmp), <, 0.0001);

#if 0
	/* write eeprom */
	ch_device_queue_write_eeprom (device_queue,
				      device,
				      CH_WRITE_EEPROM_MAGIC);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
#endif

	g_object_unref (device);
	g_object_unref (device_queue);
}

static void
ch_test_reading_func (void)
{
	gboolean ret;
	GError *error = NULL;
	guint32 take_reading = 0;
	GUsbDevice *device;
	ChDeviceQueue *device_queue;

	/* load the device */
	device = ch_client_get_default (&error);
	if (device == NULL && g_error_matches (error,
					       G_USB_DEVICE_ERROR,
					       G_USB_DEVICE_ERROR_NO_DEVICE)) {
		g_debug ("no device, skipping tests");
		g_error_free (error);
		return;
	}
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* set color select */
	device_queue = ch_device_queue_new ();
	if (ch_device_get_mode (device) == CH_DEVICE_MODE_FIRMWARE) {
		ch_device_queue_set_color_select (device_queue,
						  device,
						  CH_COLOR_SELECT_WHITE);
		ret = ch_device_queue_process (device_queue,
					       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
					       NULL,
					       &error);
		g_assert_no_error (error);
		g_assert (ret);

		/* set multiplier */
		ch_device_queue_set_multiplier (device_queue,
						device,
						CH_FREQ_SCALE_100);
		ret = ch_device_queue_process (device_queue,
					       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
					       NULL,
					       &error);
		g_assert_no_error (error);
		g_assert (ret);
	}

	/* set integral and take a reading from the hardware */
	ch_device_queue_set_integral_time (device_queue,
					   device,
					   0xffff);
	ch_device_queue_take_reading_raw (device_queue,
					  device,
					  &take_reading);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (take_reading, >, 0);

	g_object_unref (device);
	g_object_unref (device_queue);
}

static void
ch_test_reading_xyz_func (void)
{
	gboolean ret;
	CdMat3x3 calibration;
	CdColorXYZ reading1;
	CdColorXYZ reading2;
	gdouble scaling_factor_actual;
	GError *error = NULL;
	guint16 calibration_map[6];
	guint16 post_scale;
	guint i;
	GUsbDevice *device;
	CdColorRGB value;
	ChDeviceQueue *device_queue;

	/* load the device */
	device = ch_client_get_default (&error);
	if (device == NULL && g_error_matches (error,
					       G_USB_DEVICE_ERROR,
					       G_USB_DEVICE_ERROR_NO_DEVICE)) {
		g_debug ("no device, skipping tests");
		g_error_free (error);
		return;
	}
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* set unity calibration */
	device_queue = ch_device_queue_new ();
	cd_mat33_set_identity (&calibration);
	ch_device_queue_set_calibration (device_queue,
					 device,
					 60,
					 &calibration,
					 CH_CALIBRATION_TYPE_ALL,
					 "test0");
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* set everything to use the unity values */
	for (i = 0; i < 6; i++)
		calibration_map[i] = 60;
	ch_device_queue_set_calibration_map (device_queue,
					     device,
					     calibration_map);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* set dark offsets and scale */
	value.R = 0.0f;
	value.G = 0.0f;
	value.B = 0.0f;
	ch_device_queue_set_dark_offsets (device_queue,
					  device,
					  &value);
	ch_device_queue_set_pre_scale (device_queue,
				       device,
				       5.0f);
	ch_device_queue_set_post_scale (device_queue,
					device,
					1.0f);

	/* take a reading from the hardware */
	ch_device_queue_take_readings_xyz (device_queue,
					   device,
					   0,
					   &reading1);
	ret = ch_device_queue_process (device_queue,
				       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
				       NULL,
				       &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpfloat (reading1.X, >, 0.0f);
	g_assert_cmpfloat (reading1.Y, >, 0.0f);
	g_assert_cmpfloat (reading1.Z, >, 0.0f);

	/* set post scale much higher */
	for (post_scale = 1; post_scale < 2000; post_scale *= 2) {
		g_debug ("Setting post-scale %i", post_scale);
		ch_device_queue_set_post_scale (device_queue,
						device,
						post_scale);
		ch_device_queue_take_readings_xyz (device_queue,
						   device,
						   0,
						   &reading2);
		ret = ch_device_queue_process (device_queue,
					       CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE,
					       NULL,
					       &error);
		g_assert_no_error (error);
		g_assert (ret);

		/* X */
		scaling_factor_actual = reading2.X / reading1.X;
		g_debug ("scale %i: %f, scale 1: %f so effective %f",
			 post_scale, reading2.X, reading1.X,
			 scaling_factor_actual);
		g_assert_cmpfloat (scaling_factor_actual, >, 0.9);
		g_assert_cmpfloat (scaling_factor_actual, <, 1.1);
		reading1.X = reading2.X * 2;

		/* Y */
		scaling_factor_actual = reading2.Y / reading1.Y;
		g_debug ("scale %i: %f, scale 1: %f so effective %f",
			 post_scale, reading2.Y, reading1.Y,
			 scaling_factor_actual);
		g_assert_cmpfloat (scaling_factor_actual, >, 0.9);
		g_assert_cmpfloat (scaling_factor_actual, <, 1.1);
		reading1.Y = reading2.Y * 2;

		/* Z */
		scaling_factor_actual = reading2.Z / reading1.Z;
		g_debug ("scale %i: %f, scale 1: %f so effective %f",
			 post_scale, reading2.Z, reading1.Z,
			 scaling_factor_actual);
		g_assert_cmpfloat (scaling_factor_actual, >, 0.9);
		g_assert_cmpfloat (scaling_factor_actual, <, 1.1);
		reading1.Z = reading2.Z * 2;
	}
	g_object_unref (device);
	g_object_unref (device_queue);
}

/**
 * ch_test_incomplete_request_func:
 *
 * This tests what happens when we do request,request,read on the device
 * rather than just request,read. With new firmare versions we should
 * return a %CH_ERROR_INCOMPLETE_REQUEST error value and the original
 * command ID rather than just the device re-enumerating on the USB bus.
 */
static void
ch_test_incomplete_request_func (void)
{
	gboolean ret;
	GError *error = NULL;
	guint8 buffer[CH_USB_HID_EP_SIZE];
	GUsbDevice *device = NULL;


	/* load the device */
	device = ch_client_get_default (&error);
	if (device == NULL && g_error_matches (error,
					       G_USB_DEVICE_ERROR,
					       G_USB_DEVICE_ERROR_NO_DEVICE)) {
		g_debug ("no device, skipping tests");
		g_error_free (error);
		return;
	}
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* sending first tx packet */
	memset (buffer, 0x00, CH_USB_HID_EP_SIZE);
	buffer[0] = CH_CMD_GET_FIRMWARE_VERSION;
	ret = g_usb_device_interrupt_transfer (device,
					       CH_USB_HID_EP_OUT,
					       buffer,
					       CH_USB_HID_EP_SIZE,
					       NULL,
					       CH_DEVICE_USB_TIMEOUT,
					       NULL,
					       &error);
	if (!ret) {
		g_warning ("Error: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* oops, the calling program crashed */
	g_usleep (G_USEC_PER_SEC);

	/* sending second tx packet */
	memset (buffer, 0x00, CH_USB_HID_EP_SIZE);
	buffer[0] = CH_CMD_GET_CALIBRATION;
	ret = g_usb_device_interrupt_transfer (device,
					       CH_USB_HID_EP_OUT,
					       buffer,
					       CH_USB_HID_EP_SIZE,
					       NULL,
					       CH_DEVICE_USB_TIMEOUT,
					       NULL,
					       &error);
	if (!ret) {
		g_warning ("Error: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* get rx packet */
	memset (buffer, 0x00, CH_USB_HID_EP_SIZE);
	ret = g_usb_device_interrupt_transfer (device,
					       CH_USB_HID_EP_IN,
					       buffer,
					       CH_USB_HID_EP_SIZE,
					       NULL,
					       CH_DEVICE_USB_TIMEOUT,
					       NULL,
					       &error);
	if (!ret) {
		g_warning ("Error: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_assert_cmpint (buffer[0], ==, CH_ERROR_INCOMPLETE_REQUEST);
	g_assert_cmpint (buffer[1], ==, CH_CMD_GET_FIRMWARE_VERSION);
out:
	if (device != NULL)
		g_object_unref (device);
}

int
main (int argc, char **argv)
{
	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func ("/ColorHug/hash", ch_test_hash_func);
	g_test_add_func ("/ColorHug/device-queue", ch_test_device_queue_func);
	g_test_add_func ("/ColorHug/math-convert", ch_test_math_convert_func);
	g_test_add_func ("/ColorHug/math-add", ch_test_math_add_func);
	g_test_add_func ("/ColorHug/math-multiply", ch_test_math_multiply_func);
	g_test_add_func ("/ColorHug/state", ch_test_state_func);
	g_test_add_func ("/ColorHug/eeprom", ch_test_eeprom_func);
	g_test_add_func ("/ColorHug/reading", ch_test_reading_func);
	g_test_add_func ("/ColorHug/reading-xyz", ch_test_reading_xyz_func);
	g_test_add_func ("/ColorHug/device-incomplete-request", ch_test_incomplete_request_func);

	return g_test_run ();
}

