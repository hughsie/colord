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

//g++ -o cd-libcolord++ cd-libcolord++.cpp `pkg-config --cflags --libs colord` -Wall

#include <colord.h>
#include <iostream>
using namespace std;

int
main (int argc, char *argv[])
{
	CdIcc *icc = NULL;
	gboolean ret;
	GError *error = NULL;
	GFile *file = NULL;
	gint retval = 1;

	if (argc != 2) {
		g_warning ("usage: cd-libcolord++ filename, e.g. /usr/share/color/icc/colord/sRGB.icc");
		goto out;
	}

	icc = cd_icc_new ();
	file = g_file_new_for_path (argv[1]);
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_METADATA,
				NULL, /* GCancellable */
				&error);
	if (!ret) {
		cerr << "failed to parse " << argv[1] << error->message << endl;
		g_error_free (error);
		goto out;
	}

	/* get details about the profile */
	cout << "Filename:\t" << cd_icc_get_filename (icc) << endl;
	cout << "License:\t" << cd_icc_get_metadata_item (icc, "License") << endl;
	cout << "LCMS hProfile:\t" << cd_icc_get_handle (icc) << endl;

	retval = 0;
out:
	if (file != NULL)
		g_object_unref (file);
	if (icc != NULL)
		g_object_unref (icc);
	return retval;
}
