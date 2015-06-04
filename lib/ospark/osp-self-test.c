/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
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

#include <glib-object.h>
#include <colord.h>

#include "cd-cleanup.h"

#include "osp-device.h"
#include "osp-enum.h"

/**
 * osp_client_get_default:
 **/
static GUsbDevice *
osp_client_get_default (GError **error)
{
	_cleanup_object_unref_ GUsbContext *usb_ctx = NULL;
	_cleanup_object_unref_ GUsbDevice *device = NULL;

	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* try to find the Spark device */
	usb_ctx = g_usb_context_new (NULL);
	if (usb_ctx == NULL) {
		g_set_error (error,
			     G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_NO_DEVICE,
			     "No device found; USB initialisation failed");
		return NULL;
	}
	device = g_usb_context_find_by_vid_pid (usb_ctx,
						OSP_USB_VID,
						OSP_USB_PID,
						error);
	if (device == NULL)
		return NULL;
	g_debug ("Found Spark device %s",
		 g_usb_device_get_platform_id (device));
	if (!osp_device_open (device, error))
		return NULL;
	return g_object_ref (device);
}

static void
osp_test_protocol_func (void)
{
	gchar *tmp;
	guint32 cmd_val;
	guint8 cmd[4];
	guint i;
	guint j;
	_cleanup_strv_free_ gchar **lines = NULL;
	_cleanup_free_ gchar *data = NULL;
	_cleanup_error_free_ GError *error = NULL;

	if (!g_file_test ("protocol-dump.csv", G_FILE_TEST_EXISTS))
		return;
	g_file_get_contents ("protocol-dump.csv", &data, NULL, &error);
	g_assert_no_error (error);

	lines = g_strsplit (data, "\n", -1);
	for (i = 0; lines[i] != NULL; i++) {
		_cleanup_strv_free_ gchar **tokens = NULL;
		tmp = g_strstr_len (lines[i], -1, "OUT txn");
		if (tmp == NULL)
			continue;
		tokens = g_strsplit (tmp, " ", -1);
		for (j = 0; j < 4; j++)
			cmd[j] = g_ascii_strtoull (tokens[j + 9], NULL, 16);
		cmd_val = *((guint32 *) cmd);
		g_print ("%08x = %s\n", cmd_val, osp_cmd_to_string (cmd_val));
	}
}

static void
osp_test_reading_xyz_func (void)
{
	CdSpectrum *sp;
	guint i, j;
	_cleanup_error_free_ GError *error = NULL;
	_cleanup_object_unref_ GUsbDevice *device = NULL;
	_cleanup_free_ gchar *serial = NULL;
	_cleanup_free_ gchar *fwver = NULL;

	/* load the device */
	device = osp_client_get_default (&error);
	if (device == NULL && g_error_matches (error,
					       G_USB_DEVICE_ERROR,
					       G_USB_DEVICE_ERROR_NO_DEVICE)) {
		g_debug ("skipping tests: %s", error->message);
		return;
	}
	g_assert_no_error (error);
	g_assert (device != NULL);

	serial = osp_device_get_serial (device, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (serial, !=, NULL);

	fwver = osp_device_get_fw_version (device, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (fwver, ==, "0.4");

	sp = osp_device_take_spectrum (device, &error);
	g_assert_no_error (error);
	g_assert (sp != NULL);
	for (i = 0; i < 1024; i+=5) {
		g_print ("%.1fnm: ", cd_spectrum_get_wavelength (sp, i));
		for (j = 0; j < cd_spectrum_get_value_raw (sp, i) * 1000.f; j++)
			g_print ("*");
		g_print ("\n");
	}
	cd_spectrum_free (sp);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func ("/Spark/protocol", osp_test_protocol_func);
	g_test_add_func ("/Spark/reading-xyz", osp_test_reading_xyz_func);

	return g_test_run ();
}

