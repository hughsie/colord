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

//gcc -o cd-find-broken cd-find-broken.c `pkg-config --cflags --libs colord` -Wall

#include <stdlib.h>
#include <colord.h>
#include <locale.h>

static gboolean
parse_filename (const gchar *filename, GString *csv, GError **error)
{
	CdIcc *icc = NULL;
	CdProfileWarning warning;
	GArray *warnings = NULL;
	gboolean ret;
	GFile *file = NULL;
	guint i;

	/* load file */
	icc = cd_icc_new ();
	file = g_file_new_for_path (filename);
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_METADATA | CD_ICC_LOAD_FLAGS_PRIMARIES,
				NULL,
				error);
	if (!ret)
		goto out;

	/* any problems */
	warnings = cd_icc_get_warnings (icc);
	if (warnings->len == 0) 
		goto out;

	/* append to CSV file */
	g_string_append_printf (csv, "%s,\"%s\",\"%s\",",
				cd_icc_get_filename (icc),
				cd_icc_get_manufacturer (icc, NULL, NULL),
				cd_icc_get_model (icc, NULL, NULL));
	for (i = 0; i < warnings->len; i++) {
		warning = g_array_index (warnings, CdProfileWarning, i);
		g_string_append_printf (csv, "%s|",
					cd_profile_warning_to_string (warning));
	}
	csv->str[csv->len - 1] = '\n';
out:
	if (warnings != NULL)
		g_array_unref (warnings);
	g_object_unref (file);
	g_object_unref (icc);
	return ret;
}

int
main (int argc, char *argv[])
{
	const gchar *failures = "./results.csv";
	gboolean ret;
	GError *error = NULL;
	gint retval = EXIT_FAILURE;
	GString *csv = NULL;
	guint i;
	guint total = 0;
	guint total_with_warnings = 0;

	if (argc < 2) {
		g_warning ("usage: cd-find-broken.c filename, e.g. 'uploads/*'");
		goto out;
	}

	setlocale (LC_ALL, "");

	g_type_init ();

	/* create CSV header */
	csv = g_string_new ("filename,vendor,model,warnings\n");

	/* scan each file */
	for (i = 1; i < (guint) argc; i++) {
		ret = parse_filename (argv[i], csv, &error);
		if (!ret) {
			g_warning ("failed to parse %s: %s",
				   argv[i], error->message);
			g_clear_error (&error);
		}
	}

	/* print stats */
	total = argc - 1;
	for (i = 0; i < csv->len; i++) {
		if (csv->str[i] == '\n')
			total_with_warnings++;
	}
	g_print ("Total profiles scanned: %i\n",  total);
	g_print ("Profiles with invalid or unlikely primaries: %i\n",  total_with_warnings);
	g_print ("EDIDs are valid %.1f%% of the time\n", 100.0f - (gdouble) 100.0f * total_with_warnings / total);

	/* save the file */
	ret = g_file_set_contents (failures, csv->str, -1, &error);
	if (!ret) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	g_print ("Failures written to %s\n", failures);
	retval = EXIT_SUCCESS;
out:
	if (csv != NULL)
		g_string_free (csv, TRUE);
	return retval;
}
