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

#include <stdlib.h>

#include "huey-device.h"

int
main (int argc, char *argv[])
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GUsbContext) ctx = NULL;
	g_autoptr(GUsbDevice) device = NULL;

	/* check arguments */
	if (argc != 2) {
		g_printerr ("command required, e.g. status, eeprom, sample\n");
		return EXIT_FAILURE;
	}

	/* find and open device */
	ctx = g_usb_context_new (&error);
	if (ctx == NULL) {
		g_printerr ("%s\n", error->message);
		return EXIT_FAILURE;
	}
	device = g_usb_context_find_by_vid_pid (ctx, HUEY_USB_VID, HUEY_USB_PID, &error);
	if (device == NULL) {
		g_printerr ("%s\n", error->message);
		return EXIT_FAILURE;
	}
	if (!huey_device_open (device, &error)) {
		g_printerr ("%s\n", error->message);
		return EXIT_FAILURE;
	}

	/* device status */
	if (g_strcmp0 (argv[1], "status") == 0) {
		g_autofree gchar *status = NULL;
		status = huey_device_get_status (device, &error);
		if (status == NULL) {
			g_printerr ("failed to get status: %s\n", error->message);
			return EXIT_FAILURE;
		}
		g_print ("status = '%s'\n", status);
		return EXIT_SUCCESS;
	}

	/* sample the colors again and again */
	if (g_strcmp0 (argv[1], "sample") == 0) {
		for (guint i = 0; i < 0xff; i++) {
			gdouble val = 0.f;
			if (!huey_device_take_sample (device, &val, &error)) {
				g_printerr ("failed to take sample: %s\n", error->message);
				return EXIT_FAILURE;
			}
			g_print ("val=%.3f\n", val);
		}
		return EXIT_SUCCESS;
	}

	/* get all the EEPROM */
	if (g_strcmp0 (argv[1], "eeprom") == 0) {
		const gchar *data;
		gsize sz = 0;
		g_autoptr(GBytes) blob = NULL;

		/* read entire EEPROM space */
		blob = huey_device_read_eeprom (device, &error);
		if (blob == NULL) {
			g_printerr ("failed to get EEPROM: %s\n", error->message);
			return EXIT_FAILURE;
		}
		/* write it out to a file */
		data = g_bytes_get_data (blob, &sz);
		if (!g_file_set_contents ("huey2.bin", data, sz, &error)) {
			g_printerr ("failed to save file: %s\n", error->message);
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}

	/* command invalid */
	g_printerr ("command not known\n");
	return EXIT_FAILURE;
}
