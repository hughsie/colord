/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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
#include <stdlib.h>

#include "cd-it8-utils.h"

/**
 * cd_convert_ti3_ti3_to_ccmx:
 **/
static gboolean
cd_convert_ti3_ti3_to_ccmx (const gchar *reference_fn,
			    const gchar *measured_fn,
			    const gchar *device_fn,
			    GError **error)
{
	CdIt8 *it8_ccmx = NULL;
	CdIt8 *it8_measured = NULL;
	CdIt8 *it8_reference = NULL;
	gboolean ret;
	GFile *file_ccmx = NULL;
	GFile *file_measured = NULL;
	GFile *file_reference = NULL;

	/* load reference */
	it8_reference = cd_it8_new ();
	file_reference = g_file_new_for_path (reference_fn);
	ret = cd_it8_load_from_file (it8_reference,
				     file_reference,
				     error);
	if (!ret)
		goto out;

	/* load measured */
	it8_measured = cd_it8_new ();
	file_measured = g_file_new_for_path (measured_fn);
	ret = cd_it8_load_from_file (it8_measured,
				     file_measured,
				     error);
	if (!ret)
		goto out;

	/* calculate calibration matrix */
	it8_ccmx = cd_it8_new_with_kind (CD_IT8_KIND_CCMX);
	ret = cd_it8_utils_calculate_ccmx (it8_reference,
					   it8_measured,
					   it8_ccmx,
					   error);
	if (!ret)
		goto out;

	/* save file */
	file_ccmx = g_file_new_for_path (device_fn);
	cd_it8_set_title (it8_ccmx, "Factory Calibration");
	cd_it8_set_originator (it8_ccmx, "cd-calibration");
	cd_it8_add_option (it8_ccmx, "TYPE_FACTORY");
	ret = cd_it8_save_to_file (it8_ccmx, file_ccmx, error);
	if (!ret)
		goto out;
out:
	if (file_reference != NULL)
		g_object_unref (file_reference);
	if (file_measured != NULL)
		g_object_unref (file_measured);
	if (file_ccmx != NULL)
		g_object_unref (file_ccmx);
	if (it8_reference != NULL)
		g_object_unref (it8_reference);
	if (it8_measured != NULL)
		g_object_unref (it8_measured);
	if (it8_ccmx != NULL)
		g_object_unref (it8_ccmx);
	return ret;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean ret;
	GError *error = NULL;
	gint retval = EXIT_FAILURE;

	g_type_init ();

	/* create a .ccmx from two .ti3 files */
	if (argc == 4 &&
	    g_str_has_suffix (argv[1], ".ti3") &&
	    g_str_has_suffix (argv[2], ".ti3") &&
	    g_str_has_suffix (argv[3], ".ccmx")) {
		ret = cd_convert_ti3_ti3_to_ccmx (argv[1],
						  argv[2],
						  argv[3],
						  &error);
		if (!ret) {
			g_print ("failed to create ccmx: %s", error->message);
			g_error_free (error);
			goto out;
		}
	} else {
		g_print ("Usage: reference.ti3 measured.ti3 device.ccmx\n");
	}

	/* success */
	retval = EXIT_SUCCESS;
out:
	return retval;
}
