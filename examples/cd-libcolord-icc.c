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

//gcc -o cd-libcolord-icc cd-libcolord-icc.c `pkg-config --cflags --libs colord` -Wall

#include <stdlib.h>
#include <colord.h>
#include <locale.h>

int
main (int argc, char *argv[])
{
	CdIcc *icc = NULL;
	const gchar *locale;
	gboolean ret;
	GError *error = NULL;
	GFile *file = NULL;
	gint retval = EXIT_FAILURE;

	if (argc != 2) {
		g_warning ("usage: cd-libcolord-icc.c filename, e.g. /usr/share/color/icc/colord/sRGB.icc");
		goto out;
	}

	setlocale (LC_ALL, "");

	g_type_init ();

	icc = cd_icc_new ();
	file = g_file_new_for_path (argv[1]);
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_METADATA,
				NULL, /* GCancellable */
				&error);
	if (!ret) {
		g_warning ("failed to parse %s: %s", argv[1], error->message);
		g_error_free (error);
		goto out;
	}

	/* get details about the profile */
	g_print ("Filename:\t%s\n", cd_icc_get_filename (icc));
	g_print ("License:\t%s\n", cd_icc_get_metadata_item (icc, "License"));
	g_print ("LCMS hProfile:\t%p\n", cd_icc_get_handle (icc));

	/* get translated UTF-8 strings where available */
	locale = g_getenv ("LANG");
	g_print ("Description:\t%s\n", cd_icc_get_description (icc, locale, NULL));
	g_print ("Model:\t\t%s\n", cd_icc_get_model (icc, locale, NULL));
	g_print ("Copyright:\t%s\n", cd_icc_get_copyright (icc, locale, NULL));

	retval = EXIT_SUCCESS;
out:
	if (file != NULL)
		g_object_unref (file);
	if (icc != NULL)
		g_object_unref (icc);
	return retval;
}
