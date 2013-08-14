/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

//gcc -o test test.c cd-edid.c `pkg-config --cflags --libs colord`  -I../.. -Wall

#include <stdlib.h>
#include <stdio.h>

#include "cd-compat-edid.h"

int
main (int argc, char *argv[])
{
	CdEdidError rc;
	char *profile = NULL;
	gboolean ret;
	gchar *edid;
	GError *error = NULL;
	gsize edid_len = 0;

	/* load EDID blob derived from:
	 *  cat /sys/class/drm/card0-LVDS-1/edid > edid.bin */
	ret = g_file_get_contents ("edid.bin", (gchar**) &edid, &edid_len, &error);
	if (!ret) {
		g_warning ("Failed to load file: %s", error->message);
		g_error_free (error);
		return EXIT_FAILURE;
	}

	/* test libcolordcompat */
	rc = cd_edid_get_profile ((unsigned char *) edid, edid_len, &profile);
	if (rc != CD_EDID_ERROR_OK) {
		printf("Failed to get profile, error is %i\n", rc);
		return EXIT_FAILURE;
	}
	printf("Profile to use is %s\n", profile);
	free(profile);

	return EXIT_SUCCESS;
}
