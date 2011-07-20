/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#include "cd-color.h"
#include "cd-common.h"
#include "cd-lcms-helpers.h"

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

static gboolean
add_srgb_palette (cmsNAMEDCOLORLIST *nc2, const gchar *filename)
{
	CdColorRGB8 rgb;
	cmsCIELab lab;
	cmsHPROFILE srgb_profile;
	cmsHPROFILE lab_profile;
	cmsHTRANSFORM transform;
	cmsUInt16Number pcs[3];
	gboolean ret;
	gchar *data = NULL;
	gchar **lines = NULL;
	gchar *name;
	gchar **split = NULL;
	GError *error = NULL;
	guint i;

	lab_profile = cmsCreateLab4Profile (NULL);
	srgb_profile = cmsCreate_sRGBProfile ();
	transform = cmsCreateTransform (srgb_profile, TYPE_RGB_8,
					lab_profile, TYPE_Lab_DBL,
					INTENT_PERCEPTUAL, 0);

	ret = g_file_get_contents (filename, &data, NULL, &error);
	if (!ret)
		goto out;
	lines = g_strsplit (data, "\n", -1);

	for (i=0; lines[i] != NULL; i++) {
		/* ignore blank lines */
		if (lines[i][0] == '\0')
			continue;
		split = g_strsplit (lines[i], ",", -1);
		if (g_strv_length (split) == 4) {
			g_strdelimit (split[0], "\"", ' ');
			name = g_strstrip (split[0]);
			rgb.R = atoi (split[1]);
			rgb.G = atoi (split[2]);
			rgb.B = atoi (split[3]);
			cmsDoTransform (transform, &rgb, &lab, 1);

			g_debug ("add %s, %i,%i,%i as %f,%f,%f",
				 name,
				 rgb.R, rgb.G, rgb.B,
				 lab.L,
				 lab.a,
				 lab.b);

			/*
			 * PCS = colours in PCS colour space CIE*Lab
			 * Colorant = colours in device colour space
			 */
			cmsFloat2LabEncoded (pcs, &lab);
			ret = cmsAppendNamedColor (nc2, name, pcs, pcs);
			g_assert (ret);

		} else {
			g_warning ("invalid line: %s",
				   lines[i]);
		}
		g_strfreev (split);
	}
out:
	cmsDeleteTransform (transform);
	cmsCloseProfile (lab_profile);
	cmsCloseProfile (srgb_profile);
	g_free (data);
	g_strfreev (lines);
	return ret;
}

/*
 * main:
 */
int
main (int argc, char **argv)
{
	cmsHPROFILE lcms_profile = NULL;
	cmsNAMEDCOLORLIST *nc2 = NULL;
	gboolean ret;
	gchar *copyright = NULL;
	gchar *description = NULL;
	gchar *filename = NULL;
	gchar *manufacturer = NULL;
	gchar *metadata = NULL;
	gchar *model = NULL;
	gchar *nc_prefix = NULL;
	gchar *nc_suffix = NULL;
	gchar *srgb_palette = NULL;
	GError *error = NULL;
	GOptionContext *context;
	guint retval = EXIT_FAILURE;

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
		{ "output", 'o', 0, G_OPTION_ARG_STRING, &filename,
		/* TRANSLATORS: command line option */
		  _("Profile to create"), NULL },
		{ "srgb-palette", '\0', 0, G_OPTION_ARG_STRING, &srgb_palette,
		/* TRANSLATORS: command line option */
		  _("sRGB CSV filename"), NULL },
		{ "nc-prefix", '\0', 0, G_OPTION_ARG_STRING, &nc_prefix,
		/* TRANSLATORS: command line option */
		  _("Named color prefix"), NULL },
		{ "nc-suffix", '\0', 0, G_OPTION_ARG_STRING, &nc_suffix,
		/* TRANSLATORS: command line option */
		  _("Named color suffix"), NULL },
		{ "metadata", '\0', 0, G_OPTION_ARG_STRING, &metadata,
		/* TRANSLATORS: command line option */
		  _("The metadata in 'key1=value1,key2=value2' format"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* TRANSLATORS: command line tool */
	context = g_option_context_new (_("ICC profile creation program"));
	g_option_context_add_main_entries (context, options, NULL);
	ret = g_option_context_parse (context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: the user didn't read the man page */
		g_print ("%s: %s\n",
			 _("Failed to parse arguments"),
			 error->message);
		g_error_free (error);
		goto out;
	}
	g_option_context_free (context);

	/* nothing specified */
	if (filename == NULL) {
		/* TRANSLATORS: the user forgot to use -o */
		g_print ("%s\n", _("No output filename specified"));
		goto out;
	}

	/* setup LCMS */
	cmsSetLogErrorHandler (cd_fix_profile_error_cb);

	lcms_profile = cmsCreateNULLProfile ();
	if (lcms_profile == NULL || lcms_error_code != 0) {
		g_warning ("failed to open profile");
		goto out;
	}

	cmsSetDeviceClass(lcms_profile, cmsSigNamedColorClass);
	cmsSetPCS (lcms_profile, cmsSigLabData);
	cmsSetColorSpace (lcms_profile, cmsSigLabData);
	cmsSetProfileVersion (lcms_profile, 3.4);

	if (srgb_palette != NULL) {
		/* create a named color structure */
		nc2 = cmsAllocNamedColorList (NULL, 1, /* will realloc more as required */
					      3,
					      nc_prefix != NULL ? nc_prefix : "",
					      nc_suffix != NULL ? nc_suffix : "");
		add_srgb_palette (nc2, srgb_palette);
		cmsWriteTag (lcms_profile, cmsSigNamedColor2Tag, nc2);
	}

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

	ret = cd_profile_write_metadata_string (lcms_profile,
						metadata,
						TRUE,
						argv[0],
						&error);
	if (!ret) {
		g_warning ("failed to write metadata: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* write profile id */
	ret = cmsMD5computeID (lcms_profile);
	if (!ret || lcms_error_code != 0) {
		g_warning ("failed to write profile id");
		goto out;
	}

	/* success */
	retval = EXIT_SUCCESS;
	cmsSaveProfileToFile (lcms_profile, filename);
out:
	if (nc2 != NULL)
		cmsFreeNamedColorList (nc2);
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	g_free (description);
	g_free (copyright);
	g_free (model);
	g_free (manufacturer);
	g_free (metadata);
	g_free (filename);
	g_free (srgb_palette);
	g_free (nc_prefix);
	g_free (nc_suffix);
	return retval;
}

