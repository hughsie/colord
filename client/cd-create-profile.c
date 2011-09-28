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
#include <math.h>

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
add_nc_palette_srgb (cmsNAMEDCOLORLIST *nc2, const gchar *filename)
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

static gboolean
add_nc_palette_lab (cmsNAMEDCOLORLIST *nc2, const gchar *filename)
{
	cmsCIELab lab;
	cmsUInt16Number pcs[3];
	gboolean ret;
	gchar *data = NULL;
	gchar **lines = NULL;
	gchar *name;
	gchar **split = NULL;
	GError *error = NULL;
	guint i;

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
			lab.L = atof (split[1]);
			lab.a = atof (split[2]);
			lab.b = atof (split[3]);

			g_debug ("add %s, %f,%f,%f",
				 name,
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
			g_warning ("invalid line: %s", lines[i]);
		}
		g_strfreev (split);
	}
out:
	g_free (data);
	g_strfreev (lines);
	return ret;
}

/* create a Lab profile of named colors */
static cmsHPROFILE
create_nc_palette (const gchar *filename,
		   const gchar *nc_prefix,
		   const gchar *nc_suffix,
		   const gchar *nc_type)
{
	cmsHPROFILE profile;
	cmsNAMEDCOLORLIST *nc2 = NULL;

	profile = cmsCreateNULLProfile ();
	if (profile == NULL || lcms_error_code != 0) {
		g_warning ("failed to open profile");
		goto out;
	}

	cmsSetDeviceClass(profile, cmsSigNamedColorClass);
	cmsSetPCS (profile, cmsSigLabData);
	cmsSetColorSpace (profile, cmsSigLabData);
	cmsSetProfileVersion (profile, 3.4);

	/* create a named color structure */
	nc2 = cmsAllocNamedColorList (NULL, 1, /* will realloc more as required */
				      3,
				      nc_prefix != NULL ? nc_prefix : "",
				      nc_suffix != NULL ? nc_suffix : "");
	if (g_strcmp0 (nc_type, "srgb") == 0)
		add_nc_palette_srgb (nc2, filename);
	else if (g_strcmp0 (nc_type, "lab") == 0)
		add_nc_palette_lab (nc2, filename);
	cmsWriteTag (profile, cmsSigNamedColor2Tag, nc2);
out:
	if (nc2 != NULL)
		cmsFreeNamedColorList (nc2);
	return profile;
}

static gboolean
set_vcgt_from_data (cmsHPROFILE profile,
		    const guint16 *red,
		    const guint16 *green,
		    const guint16 *blue,
		    guint size)
{
	guint i;
	gboolean ret = FALSE;
	cmsToneCurve *vcgt_curve[3];

	/* build tone curve */
	vcgt_curve[0] = cmsBuildTabulatedToneCurve16 (NULL, size, red);
	vcgt_curve[1] = cmsBuildTabulatedToneCurve16 (NULL, size, green);
	vcgt_curve[2] = cmsBuildTabulatedToneCurve16 (NULL, size, blue);

	/* smooth it */
	for (i=0; i<3; i++)
		cmsSmoothToneCurve (vcgt_curve[i], 5);

	/* write the tag */
	ret = cmsWriteTag (profile, cmsSigVcgtType, vcgt_curve);

	/* free the tonecurves */
	for (i=0; i<3; i++)
		cmsFreeToneCurve (vcgt_curve[i]);
	return ret;
}

/* create a Lab profile of named colors */
static cmsHPROFILE
create_xorg_gamma (const gchar *points_str)
{
	cmsHPROFILE profile = NULL;
	gboolean ret;
	gchar **points_split = NULL;
	gdouble fraction;
	gdouble points[3];
	guint16 data[3][256];
	guint i, j;

	/* split into parts */
	points_split = g_strsplit (points_str, ",", -1);
	if (g_strv_length (points_split) != 3) {
		g_warning ("incorrect points string");
		goto out;
	}

	/* parse floats */
	for (j=0; j<3; j++)
		points[j] = atof (points_split[j]);

	/* create a bog-standard sRGB profile */
	profile = cmsCreate_sRGBProfile ();
	if (profile == NULL || lcms_error_code != 0) {
		g_warning ("failed to open profile");
		goto out;
	}

	/* write header */
	cmsSetDeviceClass (profile, cmsSigDisplayClass);
	cmsSetPCS (profile, cmsSigXYZData);
	cmsSetColorSpace (profile, cmsSigRgbData);
	cmsSetProfileVersion (profile, 3.4);
	cmsSetHeaderRenderingIntent (profile,
				     INTENT_RELATIVE_COLORIMETRIC);

	/* scale all the values by the floating point values */
	for (i=0; i<256; i++) {
		fraction = (gdouble)i / 256.0f;
		for (j=0; j<3; j++) {
			data[j][i] = pow (fraction, 1.0f / points[j]) * 0xffff;
		}
	}

	/* write vcgt */
	ret = set_vcgt_from_data (profile,
				  data[0],
				  data[1],
				  data[2],
				  256);
	if (!ret) {
		g_warning ("failed to write VCGT");
		goto out;
	}
out:
	g_strfreev (points_split);
	return profile;
}

/*
 * main:
 */
int
main (int argc, char **argv)
{
	cmsHPROFILE lcms_profile = NULL;
	gboolean ret;
	gchar *copyright = NULL;
	gchar *description = NULL;
	gchar *filename = NULL;
	gchar *manufacturer = NULL;
	gchar *metadata = NULL;
	gchar *model = NULL;
	gchar *nc_prefix = NULL;
	gchar *nc_suffix = NULL;
	gchar *nc_palette = NULL;
	gchar *nc_type = NULL;
	gchar *xorg_gamma = NULL;
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
		{ "named-color-palette", '\0', 0, G_OPTION_ARG_STRING, &nc_palette,
		/* TRANSLATORS: command line option */
		  _("Named color CSV filename"), NULL },
		{ "named-color-type", '\0', 0, G_OPTION_ARG_STRING, &nc_type,
		/* TRANSLATORS: command line option */
		  _("Named color type, e.g. 'lab' or 'srgb'"), NULL },
		{ "xorg-gamma", '\0', 0, G_OPTION_ARG_STRING, &xorg_gamma,
		/* TRANSLATORS: command line option */
		  _("A gamma string, e.g. '0.8,0.8,0.6'"), NULL },
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

	if (nc_palette != NULL) {
		lcms_profile = create_nc_palette (nc_palette,
						  nc_prefix,
						  nc_suffix,
						  nc_type);
	} else if (xorg_gamma != NULL) {
		lcms_profile = create_xorg_gamma (xorg_gamma);
	} else {
		/* TRANSLATORS: the user forgot to use an action */
		g_print ("%s\n", _("No data to create profile"));
		goto out;
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
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	g_free (description);
	g_free (copyright);
	g_free (model);
	g_free (manufacturer);
	g_free (metadata);
	g_free (filename);
	g_free (nc_palette);
	g_free (nc_type);
	g_free (xorg_gamma);
	g_free (nc_prefix);
	g_free (nc_suffix);
	return retval;
}

