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
#include <lcms2_plugin.h>
#include <stdlib.h>
#include <math.h>
#include <colord-private.h>

#include "cd-common.h"
#include "cd-lcms-helpers.h"

typedef struct {
	GOptionContext		*context;
	GPtrArray		*cmd_array;
	cmsHPROFILE		 lcms_profile;
} CdUtilPrivate;

typedef gboolean (*CdUtilPrivateCb)	(CdUtilPrivate	*util,
					 gchar		**values,
					 GError		**error);

typedef struct {
	gchar		*name;
	gchar		*description;
	CdUtilPrivateCb	 callback;
} CdUtilItem;

/**
 * cd_util_item_free:
 **/
static void
cd_util_item_free (CdUtilItem *item)
{
	g_free (item->name);
	g_free (item->description);
	g_free (item);
}

/*
 * cd_sort_command_name_cb:
 */
static gint
cd_sort_command_name_cb (CdUtilItem **item1, CdUtilItem **item2)
{
	return g_strcmp0 ((*item1)->name, (*item2)->name);
}

/**
 * cd_util_add:
 **/
static void
cd_util_add (GPtrArray *array, const gchar *name, const gchar *description, CdUtilPrivateCb callback)
{
	CdUtilItem *item;
	gchar **names;
	guint i;

	/* add each one */
	names = g_strsplit (name, ",", -1);
	for (i = 0; names[i] != NULL; i++) {
		item = g_new0 (CdUtilItem, 1);
		item->name = g_strdup (names[i]);
		if (i == 0) {
			item->description = g_strdup (description);
		} else {
			/* TRANSLATORS: this is a command alias */
			item->description = g_strdup_printf (_("Alias to %s"),
							     names[0]);
		}
		item->callback = callback;
		g_ptr_array_add (array, item);
	}
	g_strfreev (names);
}

/**
 * cd_util_get_descriptions:
 **/
static gchar *
cd_util_get_descriptions (GPtrArray *array)
{
	CdUtilItem *item;
	GString *string;
	guint i;
	guint j;
	guint len;
	guint max_len = 0;

	/* get maximum command length */
	for (i = 0; i < array->len; i++) {
		item = g_ptr_array_index (array, i);
		len = strlen (item->name);
		if (len > max_len)
			max_len = len;
	}

	/* ensure we're spaced by at least this */
	if (max_len < 19)
		max_len = 19;

	/* print each command */
	string = g_string_new ("");
	for (i = 0; i < array->len; i++) {
		item = g_ptr_array_index (array, i);
		g_string_append (string, "  ");
		g_string_append (string, item->name);
		len = strlen (item->name);
		for (j=len; j < max_len+3; j++)
			g_string_append_c (string, ' ');
		g_string_append (string, item->description);
		g_string_append_c (string, '\n');
	}

	/* remove trailing newline */
	if (string->len > 0)
		g_string_set_size (string, string->len - 1);

	return g_string_free (string, FALSE);
}

/**
 * cd_util_run:
 **/
static gboolean
cd_util_run (CdUtilPrivate *priv, const gchar *command, gchar **values, GError **error)
{
	CdUtilItem *item;
	gboolean ret = FALSE;
	GString *string;
	guint i;

	/* find command */
	for (i = 0; i < priv->cmd_array->len; i++) {
		item = g_ptr_array_index (priv->cmd_array, i);
		if (g_strcmp0 (item->name, command) == 0) {
			ret = item->callback (priv, values, error);
			goto out;
		}
	}

	/* not found */
	string = g_string_new ("");
	/* TRANSLATORS: error message */
	g_string_append_printf (string, "%s\n",
				_("Command not found, valid commands are:"));
	for (i = 0; i < priv->cmd_array->len; i++) {
		item = g_ptr_array_index (priv->cmd_array, i);
		g_string_append_printf (string, " * %s\n", item->name);
	}
	g_set_error_literal (error, 1, 0, string->str);
	g_string_free (string, TRUE);
out:
	return ret;
}

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
add_nc_palette_srgb (cmsNAMEDCOLORLIST *nc2,
		     const gchar *filename,
		     GError **error)
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
	guint i;

	lab_profile = cmsCreateLab4Profile (NULL);
	srgb_profile = cmsCreate_sRGBProfile ();
	transform = cmsCreateTransform (srgb_profile, TYPE_RGB_8,
					lab_profile, TYPE_Lab_DBL,
					INTENT_PERCEPTUAL, 0);

	ret = g_file_get_contents (filename, &data, NULL, error);
	if (!ret)
		goto out;
	lines = g_strsplit (data, "\n", -1);

	for (i = 0; lines[i] != NULL; i++) {
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
add_nc_palette_lab (cmsNAMEDCOLORLIST *nc2,
		    const gchar *filename,
		    GError **error)
{
	cmsCIELab lab;
	cmsUInt16Number pcs[3];
	gboolean ret;
	gchar *data = NULL;
	gchar **lines = NULL;
	gchar *endptr = NULL;
	gchar *name;
	gchar **split = NULL;
	guint i;

	ret = g_file_get_contents (filename, &data, NULL, error);
	if (!ret)
		goto out;
	lines = g_strsplit (data, "\n", -1);

	for (i = 0; lines[i] != NULL; i++) {
		/* ignore blank lines */
		if (lines[i][0] == '\0')
			continue;
		split = g_strsplit (lines[i], ",", -1);
		if (g_strv_length (split) == 4) {
			g_strdelimit (split[0], "\"", ' ');
			name = g_strstrip (split[0]);
			lab.L = g_ascii_strtod (split[1], &endptr);
			if (endptr != NULL && endptr[0] != '\0') {
				ret = FALSE;
				g_set_error (error, 1, 0,
					     "failed to parse lab.L: '%s'",
					     split[1]);
				goto out;
			}
			lab.a = g_ascii_strtod (split[2], &endptr);
			if (endptr != NULL && endptr[0] != '\0') {
				ret = FALSE;
				g_set_error (error, 1, 0,
					     "failed to parse lab.a: '%s'",
					     split[2]);
				goto out;
			}
			lab.b = g_ascii_strtod (split[3], &endptr);
			if (endptr != NULL && endptr[0] != '\0') {
				ret = FALSE;
				g_set_error (error, 1, 0,
					     "failed to parse lab.b: '%s'",
					     split[3]);
				goto out;
			}

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
	for (i = 0; i < 3; i++)
		cmsSmoothToneCurve (vcgt_curve[i], 5);

	/* write the tag */
	ret = cmsWriteTag (profile, cmsSigVcgtType, vcgt_curve);

	/* free the tonecurves */
	for (i = 0; i < 3; i++)
		cmsFreeToneCurve (vcgt_curve[i]);
	return ret;
}

/**
 * cd_util_create_named_color:
 **/
static gboolean
cd_util_create_named_color (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;
	cmsNAMEDCOLORLIST *nc2 = NULL;

	/* check arguments */
	if (g_strv_length (values) != 4) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect type prefix suffix data-file, e.g. 'lab', 'X11', '', 'data.csv'");
		goto out;
	}

	priv->lcms_profile = cmsCreateNULLProfile ();
	if (priv->lcms_profile == NULL || lcms_error_code != 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "failed to create NULL profile");
		goto out;
	}

	cmsSetDeviceClass(priv->lcms_profile, cmsSigNamedColorClass);
	cmsSetPCS (priv->lcms_profile, cmsSigLabData);
	cmsSetColorSpace (priv->lcms_profile, cmsSigLabData);

	/* create a named color structure */
	nc2 = cmsAllocNamedColorList (NULL, 1, /* will realloc more as required */
				      3,
				      values[1] != NULL ? values[1] : "",
				      values[2] != NULL ? values[2] : "");
	if (g_strcmp0 (values[0], "srgb") == 0) {
		ret = add_nc_palette_srgb (nc2, values[3], error);
		if (!ret)
			goto out;
	} else if (g_strcmp0 (values[0], "lab") == 0) {
		ret = add_nc_palette_lab (nc2, values[3], error);
		if (!ret)
			goto out;
	} else {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid palette type, expected lab|srgb");
		goto out;
	}
	cmsWriteTag (priv->lcms_profile, cmsSigNamedColor2Tag, nc2);
out:
	if (nc2 != NULL)
		cmsFreeNamedColorList (nc2);
	return ret;
}

/**
 * cd_util_create_x11_gamma:
 **/
static gboolean
cd_util_create_x11_gamma (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret;
	gchar *endptr = NULL;
	gdouble fraction;
	gdouble points[3];
	guint16 data[3][256];
	guint i, j;

	/* check arguments */
	if (g_strv_length (values) != 3) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect gamma-red, gamma-green, gamma-blue, e.g. '0.8', '0.9', '1.0'");
		goto out;
	}

	/* parse floats */
	for (j = 0; j < 3; j++) {
		points[j] = g_ascii_strtod (values[j], &endptr);
		if (endptr != NULL && endptr[0] != '\0') {
			ret = FALSE;
			g_set_error (error, 1, 0,
				     "failed to parse gamma value %i: '%s'",
				     j + 1, values[j]);
			goto out;
		}
	}

	/* create a bog-standard sRGB profile */
	priv->lcms_profile = cmsCreate_sRGBProfile ();
	if (priv->lcms_profile == NULL || lcms_error_code != 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "failed to create profile");
		goto out;
	}

	/* scale all the values by the floating point values */
	for (i = 0; i < 256; i++) {
		fraction = (gdouble) i / 256.0f;
		for (j = 0; j < 3; j++)
			data[j][i] = fraction * points[j] * 0xffff;
	}

	/* write vcgt */
	ret = set_vcgt_from_data (priv->lcms_profile,
				  data[0],
				  data[1],
				  data[2],
				  256);
	if (!ret) {
		g_set_error_literal (error, 1, 0,
				     "failed to write VCGT");
		goto out;
	}
out:
	return ret;
}

/**
 * cd_util_build_srgb_gamma:
 *
 * Values taken from lcms2.
 **/
static cmsToneCurve *
cd_util_build_srgb_gamma (void)
{
	cmsFloat64Number params[5];
	params[0] = 2.4;
	params[1] = 1. / 1.055;
	params[2] = 0.055 / 1.055;
	params[3] = 1. / 12.92;
	params[4] = 0.04045;
	return cmsBuildParametricToneCurve (NULL, 4, params);
}

/**
 * cd_util_build_lstar_gamma:
 **/
static cmsToneCurve *
cd_util_build_lstar_gamma (void)
{
	cmsFloat64Number params[5];
	params[0] = 3.000000;
	params[1] = 0.862076;
	params[2] = 0.137924;
	params[3] = 0.110703;
	params[4] = 0.080002;
	return cmsBuildParametricToneCurve (NULL, 4, params);
}

#define LCMS_CURVE_PLUGIN_TYPE_REC709	1024

/**
 * cd_util_lcms_rec709_trc_cb:
 **/
static double
cd_util_lcms_rec709_trc_cb (int type, const double params[], double x)
{
	gdouble val = 0.f;

	switch (type) {
	case -LCMS_CURVE_PLUGIN_TYPE_REC709:
		if (x < params[4])
			val = x * params[3];
		else
			val = params[1] * pow (x, (1.f / params[0])) + params[2];
		break;
	case LCMS_CURVE_PLUGIN_TYPE_REC709:
		if (x <= (params[3] * params[4]))
			val = x / params[3];
		else
			val = pow (((x + params[2]) / params[1]), params[0]);
		break;
	}
	return val;
}

/* add Rec. 709 TRC curve type */
cmsPluginParametricCurves cd_util_lcms_rec709_trc = {
	{ cmsPluginMagicNumber,			/* 'acpp' */
	  2000,					/* minimum version */
	  cmsPluginParametricCurveSig,		/* type */
	  NULL },				/* no more plugins */
	1,					/* number functions */
	{LCMS_CURVE_PLUGIN_TYPE_REC709},	/* function types */
	{5},					/* parameter count */
	cd_util_lcms_rec709_trc_cb		/* evaluator */
};

/**
 * cd_util_build_rec709_gamma:
 **/
static cmsToneCurve *
cd_util_build_rec709_gamma (void)
{
	cmsFloat64Number params[5];
	params[0] = 1.0 / 0.45;
	params[1] = 1.099;
	params[2] = 0.099;
	params[3] = 4.500;
	params[4] = 0.018;
	return cmsBuildParametricToneCurve (NULL, LCMS_CURVE_PLUGIN_TYPE_REC709, params);
}

/**
 * cd_util_create_standard_space:
 **/
static gboolean
cd_util_create_standard_space (CdUtilPrivate *priv,
			       gchar **values,
			       GError **error)
{
	cmsCIExyYTRIPLE primaries;
	cmsCIExyY white;
	cmsToneCurve *transfer[3] = { NULL, NULL, NULL};
	gboolean ret;
	gchar *endptr = NULL;
	gdouble tgamma;

	/* check arguments */
	if (g_strv_length (values) != 11) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect gamma, white, xyY, xyY, xyY");
		goto out;
	}

	/* parse gamma */
	if (g_strcmp0 (values[0], "sRGB") == 0) {
		transfer[0] = cd_util_build_srgb_gamma ();
		transfer[1] = transfer[0];
		transfer[2] = transfer[0];
	} else if (g_strcmp0 (values[0], "L*") == 0) {
		transfer[0] = cd_util_build_lstar_gamma ();
		transfer[1] = transfer[0];
		transfer[2] = transfer[0];
	} else if (g_strcmp0 (values[0], "Rec709") == 0) {
		transfer[0] = cd_util_build_rec709_gamma ();
		transfer[1] = transfer[0];
		transfer[2] = transfer[0];
	} else {
		tgamma = g_ascii_strtod (values[0], &endptr);
		if (endptr != NULL && endptr[0] != '\0') {
			ret = FALSE;
			g_set_error (error, 1, 0,
				     "failed to parse gamma: '%s'",
				     values[0]);
			goto out;
		}
		transfer[0] = cmsBuildGamma (NULL, tgamma);
		transfer[1] = transfer[0];
		transfer[2] = transfer[0];
	}

	/* values taken from https://en.wikipedia.org/wiki/Standard_illuminant */
	white.Y = 1.0f;
	if (g_strcmp0 (values[1], "C") == 0) {
		white.x = 0.31006;
		white.y = 0.31616;
	} else if (g_strcmp0 (values[1], "E") == 0) {
		white.x = 0.33333;
		white.y = 0.33333;
	} else if (g_strcmp0 (values[1], "D50") == 0) {
		cmsWhitePointFromTemp (&white, 5003);
	} else if (g_strcmp0 (values[1], "D65") == 0) {
		cmsWhitePointFromTemp (&white, 6504);
	} else {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "unknown illuminant, expected C, E, D50 or D65");
		goto out;
	}

	/* get primaries */
	primaries.Red.x = g_ascii_strtod (values[2], &endptr);
	if (endptr != NULL && endptr[0] != '\0') {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to parse Red.x: '%s'",
			     values[2]);
		goto out;
	}
	primaries.Red.y = g_ascii_strtod (values[3], &endptr);
	if (endptr != NULL && endptr[0] != '\0') {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to parse Red.y: '%s'",
			     values[3]);
		goto out;
	}
	primaries.Red.Y = g_ascii_strtod (values[4], &endptr);
	if (endptr != NULL && endptr[0] != '\0') {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to parse Red.Y: '%s'",
			     values[4]);
		goto out;
	}
	primaries.Green.x = g_ascii_strtod (values[5], &endptr);
	if (endptr != NULL && endptr[0] != '\0') {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to parse Green.x: '%s'",
			     values[5]);
		goto out;
	}
	primaries.Green.y = g_ascii_strtod (values[6], &endptr);
	if (endptr != NULL && endptr[0] != '\0') {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to parse Green.y: '%s'",
			     values[6]);
		goto out;
	}
	primaries.Green.Y = g_ascii_strtod (values[7], &endptr);
	if (endptr != NULL && endptr[0] != '\0') {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to parse Green.Y: '%s'",
			     values[7]);
		goto out;
	}
	primaries.Blue.x = g_ascii_strtod (values[8], &endptr);
	if (endptr != NULL && endptr[0] != '\0') {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to parse Blue.x: '%s'",
			     values[8]);
		goto out;
	}
	primaries.Blue.y = g_ascii_strtod (values[9], &endptr);
	if (endptr != NULL && endptr[0] != '\0') {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to parse Blue.y: '%s'",
			     values[9]);
		goto out;
	}
	primaries.Blue.Y = g_ascii_strtod (values[10], &endptr);
	if (endptr != NULL && endptr[0] != '\0') {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to parse Blue.Y: '%s'",
			     values[10]);
		goto out;
	}

	/* create profile */
	priv->lcms_profile = cmsCreateRGBProfile (&white,
						  &primaries,
						  transfer);
	ret = TRUE;
out:
	cmsFreeToneCurve (transfer[0]);
	return ret;
}

/**
 * cd_util_create_temperature:
 **/
static gboolean
cd_util_create_temperature (CdUtilPrivate *priv,
			    gchar **values,
			    GError **error)
{
	CdColorRGB white_point;
	const guint size = 256;
	gboolean ret;
	gchar *endptr = NULL;
	gdouble gamma;
	guint16 data[3][256];
	guint i;
	guint temp;

	/* check arguments */
	if (g_strv_length (values) != 2) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect temperature gamma, e.g. '6500', '0.8'");
		goto out;
	}

	/* create a bog-standard sRGB profile */
	priv->lcms_profile = cmsCreate_sRGBProfile ();
	if (priv->lcms_profile == NULL || lcms_error_code != 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "failed to create profile");
		goto out;
	}

	/* generate the VCGT table */
	temp = atoi (values[0]);
	gamma = g_ascii_strtod (values[1], &endptr);
	if (endptr != NULL && endptr[0] != '\0') {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to parse gamma: '%s'",
			     values[1]);
		goto out;
	}
	cd_color_get_blackbody_rgb (temp, &white_point);
	for (i = 0; i < size; i++) {
		data[0][i] = pow ((gdouble) i / size, 1.0 / gamma) *
				  0xffff * white_point.R;
		data[1][i] = pow ((gdouble) i / size, 1.0 / gamma) *
				  0xffff * white_point.G;
		data[2][i] = pow ((gdouble) i / size, 1.0 / gamma) *
				  0xffff * white_point.B;
	}

	/* write vcgt */
	ret = set_vcgt_from_data (priv->lcms_profile,
				  data[0],
				  data[1],
				  data[2],
				  256);
	if (!ret) {
		g_set_error_literal (error, 1, 0,
				     "failed to write VCGT");
		goto out;
	}
out:
	return ret;
}

/*
 * main:
 */
int
main (int argc, char **argv)
{
	CdUtilPrivate *priv;
	gboolean ret;
	gchar *cmd_descriptions = NULL;
	gchar *copyright = NULL;
	gchar *description = NULL;
	gchar *filename = NULL;
	gchar *manufacturer = NULL;
	gchar *metadata = NULL;
	gchar *model = NULL;
	GError *error = NULL;
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
		{ "metadata", '\0', 0, G_OPTION_ARG_STRING, &metadata,
		/* TRANSLATORS: command line option */
		  _("The metadata in 'key1=value1,key2=value2' format"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* setup LCMS */
	cmsSetLogErrorHandler (cd_fix_profile_error_cb);
	ret = cmsPlugin (&cd_util_lcms_rec709_trc);
	g_assert (ret);

	/* add commands */
	priv = g_new0 (CdUtilPrivate, 1);
	priv->cmd_array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_util_item_free);
	cd_util_add (priv->cmd_array,
		     "create-named-color",
		     /* TRANSLATORS: command description */
		     _("Create a named color profile"),
		     cd_util_create_named_color);
	cd_util_add (priv->cmd_array,
		     "create-x11-gamma",
		     /* TRANSLATORS: command description */
		     _("Create an X11 gamma profile"),
		     cd_util_create_x11_gamma);
	cd_util_add (priv->cmd_array,
		     "create-standard-space",
		     /* TRANSLATORS: command description */
		     _("Create a standard working space"),
		     cd_util_create_standard_space);
	cd_util_add (priv->cmd_array,
		     "create-temperature",
		     /* TRANSLATORS: command description */
		     _("Create a profile with a temperature VCGT"),
		     cd_util_create_temperature);

	/* sort by command name */
	g_ptr_array_sort (priv->cmd_array,
			  (GCompareFunc) cd_sort_command_name_cb);

	/* get a list of the commands */
	priv->context = g_option_context_new (NULL);
	cmd_descriptions = cd_util_get_descriptions (priv->cmd_array);
	g_option_context_set_summary (priv->context, cmd_descriptions);

	/* TRANSLATORS: program name */
	g_set_application_name (_("ICC profile creation program"));
	g_option_context_add_main_entries (priv->context, options, NULL);
	ret = g_option_context_parse (priv->context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: the user didn't read the man page */
		g_print ("%s: %s\n",
			 _("Failed to parse arguments"),
			 error->message);
		g_error_free (error);
		goto out;
	}

	/* nothing specified */
	if (filename == NULL) {
		/* TRANSLATORS: the user forgot to use -o */
		g_print ("%s\n", _("No output filename specified"));
		goto out;
	}

	/* run the specified command */
	ret = cd_util_run (priv, argv[1], (gchar**) &argv[2], &error);
	if (!ret) {
		g_print ("%s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* these are default values */
	if (copyright == NULL || g_strcmp0 (copyright, "") == 0) {
		g_free (copyright);
		copyright = g_strdup (CD_PROFILE_DEFAULT_COPYRIGHT_STRING);
	}

	if (description != NULL) {
		ret = _cmsWriteTagTextAscii (priv->lcms_profile,
					     cmsSigProfileDescriptionTag,
					     description);
		if (!ret || lcms_error_code != 0) {
			g_warning ("failed to write description");
			goto out;
		}
	}
	if (copyright != NULL) {
		ret = _cmsWriteTagTextAscii (priv->lcms_profile,
					     cmsSigCopyrightTag,
					     copyright);
		if (!ret || lcms_error_code != 0) {
			g_warning ("failed to write copyright");
			goto out;
		}
	}
	if (model != NULL) {
		ret = _cmsWriteTagTextAscii (priv->lcms_profile,
					     cmsSigDeviceModelDescTag,
					     model);
		if (!ret || lcms_error_code != 0) {
			g_warning ("failed to write model");
			goto out;
		}
	}
	if (manufacturer != NULL) {
		ret = _cmsWriteTagTextAscii (priv->lcms_profile,
					     cmsSigDeviceMfgDescTag,
					     manufacturer);
		if (!ret || lcms_error_code != 0) {
			g_warning ("failed to write manufacturer");
			goto out;
		}
	}

	ret = cd_profile_write_metadata_string (priv->lcms_profile,
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
	ret = cmsMD5computeID (priv->lcms_profile);
	if (!ret || lcms_error_code != 0) {
		g_warning ("failed to write profile id");
		goto out;
	}

	/* success */
	retval = EXIT_SUCCESS;
	cmsSaveProfileToFile (priv->lcms_profile, filename);
out:
	if (priv != NULL) {
		if (priv->cmd_array != NULL)
			g_ptr_array_unref (priv->cmd_array);
		if (priv->lcms_profile != NULL)
			cmsCloseProfile (priv->lcms_profile);
		g_option_context_free (priv->context);
		g_free (priv);
	}
	g_free (cmd_descriptions);
	g_free (description);
	g_free (copyright);
	g_free (model);
	g_free (manufacturer);
	g_free (metadata);
	g_free (filename);
	return retval;
}

