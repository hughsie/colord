/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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

#include <stdlib.h>
#include <gio/gio.h>
#include <colord/colord.h>

typedef struct {
	guint		 nm;
	CdColorXYZ	 xyz;
} CdSpectrumData;

/**
 * cd_csv2cmf_data_free:
 **/
static void
cd_csv2cmf_data_free (CdSpectrumData *data)
{
	g_slice_free (CdSpectrumData, data);
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	CdIt8 *cmf = NULL;
	CdSpectrum *spectrum[3];
	CdSpectrumData *tmp;
	GError *error = NULL;
	GFile *file = NULL;
	GPtrArray *array = NULL;
	gboolean ret;
	gchar **lines = NULL;
	gchar **split;
	gchar *data = NULL;
	gchar *dot;
	gchar *originator = NULL;
	gchar *title = NULL;
	guint i;
	guint retval = 1;

	/* check args */
	if (argc != 3) {
		retval = 0;
		g_print ("Incorrect syntax: expected cd-cvs2cmf a.csv b.cmf\n");
		goto out;
	}

	/* get data */
	ret = g_file_get_contents (argv[1], &data, NULL, &error);
	if (!ret) {
		retval = 0;
		g_print ("Failed to get contents: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* parse lines */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_csv2cmf_data_free);
	lines = g_strsplit (data, "\n", -1);
	for (i = 0; lines[i] != NULL; i++) {
		split = g_strsplit (lines[i], ",", -1);
		if (g_strv_length (split) == 4) {
			tmp = g_slice_new0 (CdSpectrumData);
			tmp->nm = atoi (split[0]);
			cd_color_xyz_set (&tmp->xyz,
					  atof (split[1]),
					  atof (split[2]),
					  atof (split[3]));
			g_ptr_array_add (array, tmp);
		} else {
			g_print ("Ignoring data line: %s", lines[i]);
		}
		g_strfreev (split);
	}

	/* did we get enough data */
	if (array->len < 3) {
		retval = 0;
		g_print ("Not enough data in the CSV file\n");
		goto out;
	}

	for (i = 0; i < 3; i++)
		spectrum[i] = cd_spectrum_sized_new (array->len);

	cd_spectrum_set_id (spectrum[0], "X");
	cd_spectrum_set_id (spectrum[1], "Y");
	cd_spectrum_set_id (spectrum[2], "Z");

	/* get the first point */
	tmp = g_ptr_array_index (array, 0);
	for (i = 0; i < 3; i++)
		cd_spectrum_set_start (spectrum[i], tmp->nm);

	/* get the last point */
	tmp = g_ptr_array_index (array, array->len - 1);
	for (i = 0; i < 3; i++)
		cd_spectrum_set_end (spectrum[i], tmp->nm);

	/* add data to the spectra */
	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		cd_spectrum_add_value (spectrum[0], tmp->xyz.X);
		cd_spectrum_add_value (spectrum[1], tmp->xyz.Y);
		cd_spectrum_add_value (spectrum[2], tmp->xyz.Z);
	}

	/* add spectra to the CMF file */
	cmf = cd_it8_new_with_kind (CD_IT8_KIND_CMF);
	for (i = 0; i < 3; i++)
		cd_it8_add_spectrum (cmf, spectrum[i]);
	originator = g_path_get_basename (argv[0]);
	cd_it8_set_originator (cmf, originator);
	title = g_path_get_basename (argv[1]);
	dot = g_strrstr (title, ".csv");
	if (dot != NULL)
		*dot = '\0';
	cd_it8_set_title (cmf, title);

	/* save */
	file = g_file_new_for_path (argv[2]);
	ret = cd_it8_save_to_file (cmf, file, &error);
	if (!ret) {
		retval = 0;
		g_print ("Failed to save file: %s\n", error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	if (cmf != NULL)
		g_object_unref (cmf);
	if (file != NULL)
		g_object_unref (file);
	g_free (data);
	g_free (title);
	g_free (originator);
	g_strfreev (lines);
	return retval;
}
