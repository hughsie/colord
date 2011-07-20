/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2011 Richard Hughes <richard@hughsie.com>
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
#include <stdlib.h>

#include "cd-common.h"
#include "cd-lcms-helpers.h"

static gint lcms_error_code = 0;

/*
 * cd_fix_profile_filename:
 */
static gboolean
cd_fix_profile_filename (const gchar *filename,
			 const gchar *description,
			 const gchar *copyright,
			 const gchar *model,
			 const gchar *manufacturer,
			 const gchar *metadata,
			 gboolean clear_metadata)
{
	gboolean ret = TRUE;
	cmsHPROFILE lcms_profile = NULL;
	gchar *data = NULL;
	gsize len;
	GError *error = NULL;

	ret = g_file_get_contents (filename, &data, &len, &error);
	if (!ret) {
		g_warning ("failed to open profile: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	lcms_profile = cmsOpenProfileFromMem (data, len);
	if (lcms_profile == NULL || lcms_error_code != 0) {
		g_warning ("failed to open profile");
		ret = FALSE;
		goto out;
	}

	/* profile version to write */
	cmsSetProfileVersion (lcms_profile, 3.4);
	if (description != NULL) {
		ret = _cmsWriteTagTextAscii (lcms_profile,
					     cmsSigProfileDescriptionTag,
					     description);
		if (!ret || lcms_error_code != 0) {
			g_warning ("failed to write description");
			goto out;
		}
	}
	if (copyright != NULL) {
		ret = _cmsWriteTagTextAscii (lcms_profile,
					     cmsSigCopyrightTag,
					     copyright);
		if (!ret || lcms_error_code != 0) {
			g_warning ("failed to write copyright");
			goto out;
		}
	}
	if (model != NULL) {
		ret = _cmsWriteTagTextAscii (lcms_profile,
					     cmsSigDeviceModelDescTag,
					     model);
		if (!ret || lcms_error_code != 0) {
			g_warning ("failed to write model");
			goto out;
		}
	}
	if (manufacturer != NULL) {
		ret = _cmsWriteTagTextAscii (lcms_profile,
					     cmsSigDeviceMfgDescTag,
					     manufacturer);
		if (!ret || lcms_error_code != 0) {
			g_warning ("failed to write manufacturer");
			goto out;
		}
	}
	if (metadata != NULL) {
		ret = cd_profile_write_metadata_string (lcms_profile,
							metadata,
							clear_metadata,
							"cd-fix-profile",
							&error);
		if (!ret) {
			g_warning ("failed to write metadata: %s",
				   error->message);
			g_error_free (error);
			goto out;
		}
	}

	/* write profile id */
	ret = cmsMD5computeID (lcms_profile);
	if (!ret || lcms_error_code != 0) {
		g_warning ("failed to write profile id");
		goto out;
	}

	cmsSaveProfileToFile (lcms_profile, filename);
out:
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	g_free (data);
	return ret;
}

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

/*
 * main:
 */
int
main (int argc, char **argv)
{
	guint i;
	guint retval = 0;
	gboolean ret;
	GOptionContext *context;
	gchar **files = NULL;
	gchar *description = NULL;
	gchar *copyright = NULL;
	gchar *model = NULL;
	gchar *manufacturer = NULL;
	gchar *metadata = NULL;
	gboolean clear_metadata = FALSE;

	const GOptionEntry options[] = {
		{ "description", 'd', 0, G_OPTION_ARG_STRING, &description,
		/* TRANSLATORS: command line option */
		  _("The profile description"), NULL },
		{ "copyright", 'c', 0, G_OPTION_ARG_STRING, &copyright,
		/* TRANSLATORS: command line option */
		  _("The profile copyright"), NULL },
		{ "model", 'm', 0, G_OPTION_ARG_STRING, &model,
		/* TRANSLATORS: command line option */
		  _("The device model"), NULL },
		{ "manufacturer", 'n', 0, G_OPTION_ARG_STRING, &manufacturer,
		/* TRANSLATORS: command line option */
		  _("The device manufacturer"), NULL },
		{ "clear-metadata", '\0', 0, G_OPTION_ARG_NONE, &clear_metadata,
		/* TRANSLATORS: command line option */
		  _("Clear existing metadata in the profile"), NULL },
		{ "metadata", 'n', 0, G_OPTION_ARG_STRING, &metadata,
		/* TRANSLATORS: command line option */
		  _("Extra metadata in 'key1=value1,key2=value2' format"), NULL },
		{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &files,
		/* TRANSLATORS: command line option */
		  _("Profiles to fix"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* TRANSLATORS: command line tool */
	context = g_option_context_new (_("ICC profile fix program"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* nothing specified */
	if (files == NULL)
		goto out;

	/* setup LCMS */
	cmsSetLogErrorHandler (cd_fix_profile_error_cb);

	/* fix each profile */
	for (i=0; files[i] != NULL; i++) {
		ret = cd_fix_profile_filename (files[i],
					       description,
					       copyright,
					       model,
					       manufacturer,
					       metadata,
					       clear_metadata);
		if (!ret) {
			retval = 1;
			goto out;
		}
	}
out:
	g_free (description);
	g_free (copyright);
	g_free (model);
	g_free (manufacturer);
	g_strfreev (files);
	return retval;
}

