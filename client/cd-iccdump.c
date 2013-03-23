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

#include "config.h"

#include <glib/gi18n.h>
#include <locale.h>
#include <lcms2.h>
#include <lcms2_plugin.h>
#include <stdlib.h>
#include <math.h>
#include <colord-private.h>

static gint lcms_error_code = 0;

/**
 * cd_fix_profile_error_cb:
 **/
static void
cd_fix_profile_error_cb (cmsContext ContextID,
			 cmsUInt32Number errorcode,
			 const char *text)
{
	g_warning ("LCMS error %i: %s", errorcode, text);

	/* copy this sytemwide */
	lcms_error_code = errorcode;
}

/**
 * cd_iccdump_print_file:
 **/
static gboolean
cd_iccdump_print_file (const gchar *filename, GError **error)
{
	CdIcc *icc;
	gboolean ret;
	gchar *str = NULL;
	GFile *file;

	/* load the profile */
	icc = cd_icc_new ();
	file = g_file_new_for_path (filename);
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_NONE,
				NULL,
				error);
	if (!ret)
		goto out;

	/* dump it to text on the console */
	str = cd_icc_to_string (icc);
	g_print ("%s\n", str);
out:
	g_free (str);
	g_object_unref (file);
	g_object_unref (icc);
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
	GOptionContext *context;
	gint i;
	guint retval = EXIT_FAILURE;

	setlocale (LC_ALL, "");
	g_type_init ();

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* setup LCMS */
	cmsSetLogErrorHandler (cd_fix_profile_error_cb);
	context = g_option_context_new (NULL);

	/* TRANSLATORS: program name */
	g_set_application_name (_("ICC profile dump program"));
	ret = g_option_context_parse (context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: the user didn't read the man page */
		g_print ("%s: %s\n",
			 _("Failed to parse arguments"),
			 error->message);
		g_error_free (error);
		goto out;
	}

	/* dump each option */
	for (i = 1; i < argc; i++) {
		ret = cd_iccdump_print_file (argv[i], &error);
		if (!ret) {
			g_warning ("Failed to dump %s: %s",
				   argv[i], error->message);
			g_error_free (error);
			goto out;
		}
	}

	/* success */
	retval = EXIT_SUCCESS;
out:
	g_option_context_free (context);
	return retval;
}

