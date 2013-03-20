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
	cmsHPROFILE		 lcms_profile;
} CdUtilPrivate;

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
cd_util_create_named_color (CdUtilPrivate *priv,
			    CdDom *dom,
			    const GNode *root,
			    GError **error)
{
	CdColorLab lab;
	cmsNAMEDCOLORLIST *nc2 = NULL;
	cmsUInt16Number pcs[3];
	const GNode *name;
	const GNode *named;
	const GNode *prefix;
	const GNode *suffix;
	const GNode *tmp;
	gboolean ret = TRUE;

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
	prefix = cd_dom_get_node (dom, root, "prefix");
	suffix = cd_dom_get_node (dom, root, "suffix");
	nc2 = cmsAllocNamedColorList (NULL, 1, /* will realloc more as required */
				      3,
				      prefix != NULL ? cd_dom_get_node_data (prefix) : "",
				      suffix != NULL ? cd_dom_get_node_data (suffix) : "");

	named = cd_dom_get_node (dom, root, "named");
	if (named == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "XML error: missing named");
		goto out;
	}
	for (tmp = named->children; tmp != NULL; tmp = tmp->next) {
		name = cd_dom_get_node (dom, tmp, "name");
		if (name == NULL) {
			ret = FALSE;
			g_set_error_literal (error, 1, 0,
					     "XML error: missing name");
			goto out;
		}
		ret = cd_dom_get_node_lab (tmp, &lab);
		if (!ret) {
			ret = FALSE;
			g_set_error (error, 1, 0,
				     "XML error: missing Lab for %s",
				     cd_dom_get_node_data (name));
			goto out;
		}

		/* PCS = colours in PCS colour space CIE*Lab
		 * colorant = colours in device colour space */
		cmsFloat2LabEncoded (pcs, (cmsCIELab *) &lab);
		ret = cmsAppendNamedColor (nc2, cd_dom_get_node_data (name), pcs, pcs);
		g_assert (ret);
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
cd_util_create_x11_gamma (CdUtilPrivate *priv,
			  CdDom *dom,
			  const GNode *root,
			  GError **error)
{
	const GNode *tmp;
	gboolean ret;
	gdouble fraction;
	CdColorRGB rgb;
	gdouble points[3];
	guint16 data[3][256];
	guint i, j;

	/* parse gamma values */
	tmp = cd_dom_get_node (dom, root, "x11_gamma");
	if (tmp == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "XML error, expected x11_gamma");
		goto out;
	}
	ret = cd_dom_get_node_rgb (tmp, &rgb);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "XML error, invalid x11_gamma");
		goto out;
	}
	points[0] = rgb.R;
	points[1] = rgb.G;
	points[2] = rgb.B;

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
			       CdDom *dom,
			       const GNode *root,
			       GError **error)
{
	CdColorYxy yxy;
	cmsCIExyYTRIPLE primaries;
	cmsCIExyY white;
	cmsToneCurve *transfer[3] = { NULL, NULL, NULL};
	const gchar *data;
	const GNode *tmp;
	gboolean ret;
	gchar *endptr = NULL;
	gdouble tgamma;

	/* parse gamma */
	tmp = cd_dom_get_node (dom, root, "gamma");
	if (tmp == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "XML error, expected gamma");
		goto out;
	}
	data = cd_dom_get_node_data (tmp);
	if (g_strcmp0 (data, "sRGB") == 0) {
		transfer[0] = cd_util_build_srgb_gamma ();
		transfer[1] = transfer[0];
		transfer[2] = transfer[0];
	} else if (g_strcmp0 (data, "L*") == 0) {
		transfer[0] = cd_util_build_lstar_gamma ();
		transfer[1] = transfer[0];
		transfer[2] = transfer[0];
	} else if (g_strcmp0 (data, "Rec709") == 0) {
		transfer[0] = cd_util_build_rec709_gamma ();
		transfer[1] = transfer[0];
		transfer[2] = transfer[0];
	} else {
		tgamma = g_ascii_strtod (data, &endptr);
		if (endptr != NULL && endptr[0] != '\0') {
			ret = FALSE;
			g_set_error (error, 1, 0,
				     "failed to parse gamma: '%s'",
				     data);
			goto out;
		}
		transfer[0] = cmsBuildGamma (NULL, tgamma);
		transfer[1] = transfer[0];
		transfer[2] = transfer[0];
	}

	/* values taken from https://en.wikipedia.org/wiki/Standard_illuminant */
	tmp = cd_dom_get_node (dom, root, "whitepoint");
	if (tmp == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "XML error, expected whitepoint");
		goto out;
	}
	data = cd_dom_get_node_data (tmp);
	white.Y = 1.0f;
	if (g_strcmp0 (data, "C") == 0) {
		white.x = 0.31006;
		white.y = 0.31616;
	} else if (g_strcmp0 (data, "E") == 0) {
		white.x = 0.33333;
		white.y = 0.33333;
	} else if (g_strcmp0 (data, "D50") == 0) {
		cmsWhitePointFromTemp (&white, 5003);
	} else if (g_strcmp0 (data, "D65") == 0) {
		cmsWhitePointFromTemp (&white, 6504);
	} else {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "unknown illuminant, expected C, E, D50 or D65");
		goto out;
	}

	/* get red primary */
	tmp = cd_dom_get_node (dom, root, "primaries/red");
	if (tmp == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "XML error, expected primaries/red");
		goto out;
	}
	ret = cd_dom_get_node_yxy (tmp, &yxy);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "XML error, invalid primaries/red");
		goto out;
	}
	primaries.Red.x = yxy.x;
	primaries.Red.y = yxy.y;
	primaries.Red.Y = yxy.Y;

	/* get green primary */
	tmp = cd_dom_get_node (dom, root, "primaries/green");
	if (tmp == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "XML error, expected primaries/green");
		goto out;
	}
	ret = cd_dom_get_node_yxy (tmp, &yxy);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "XML error, invalid primaries/green");
		goto out;
	}
	primaries.Green.x = yxy.x;
	primaries.Green.y = yxy.y;
	primaries.Green.Y = yxy.Y;

	/* get blue primary */
	tmp = cd_dom_get_node (dom, root, "primaries/blue");
	if (tmp == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "XML error, expected primaries/blue");
		goto out;
	}
	ret = cd_dom_get_node_yxy (tmp, &yxy);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "XML error, invalid primaries/blue");
		goto out;
	}
	primaries.Blue.x = yxy.x;
	primaries.Blue.y = yxy.y;
	primaries.Blue.Y = yxy.Y;

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
			    CdDom *dom,
			    const GNode *root,
			    GError **error)
{
	CdColorRGB white_point;
	const GNode *tmp;
	const guint size = 256;
	gboolean ret;
	gchar *endptr = NULL;
	gdouble gamma;
	guint16 data[3][256];
	guint i;
	guint temp;

	/* create a bog-standard sRGB profile */
	priv->lcms_profile = cmsCreate_sRGBProfile ();
	if (priv->lcms_profile == NULL || lcms_error_code != 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "failed to create profile");
		goto out;
	}

	/* parse temperature value */
	tmp = cd_dom_get_node (dom, root, "temperature");
	if (tmp == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "XML error, expected temperature");
		goto out;
	}
	temp = atoi (cd_dom_get_node_data (tmp));

	/* parse gamma value */
	tmp = cd_dom_get_node (dom, root, "gamma");
	if (tmp == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "XML error, expected gamma");
		goto out;
	}
	gamma = g_ascii_strtod (cd_dom_get_node_data (tmp), &endptr);
	if (endptr != NULL && endptr[0] != '\0') {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to parse gamma: '%s'",
			     cd_dom_get_node_data (tmp));
		goto out;
	}

	/* generate the VCGT table */
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

/**
 * cd_util_create_from_xml:
 **/
static gboolean
cd_util_create_from_xml (CdUtilPrivate *priv,
			 const gchar *filename,
			 GError **error)
{
	CdDom *dom = NULL;
	cmsHANDLE dict = NULL;
	const GNode *profile;
	const GNode *tmp;
	gboolean ret = TRUE;
	gchar *data = NULL;
	gssize data_len = -1;

	/* parse the XML into DOM */
	ret = g_file_get_contents (filename, &data, (gsize *) &data_len, error);
	if (!ret)
		goto out;
	dom = cd_dom_new ();
	ret = cd_dom_parse_xml_data (dom, data, data_len, error);
	if (!ret)
		goto out;

	/* get root */
	profile = cd_dom_get_node (dom, NULL, "profile");
	if (profile == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid XML, expected profile");
		goto out;
	}

	/* get type */
	if (cd_dom_get_node (dom, profile, "primaries") != NULL) {
		ret = cd_util_create_standard_space (priv, dom, profile, error);
		if (!ret)
			goto out;
	} else if (cd_dom_get_node (dom, profile, "temperature") != NULL) {
		ret = cd_util_create_temperature (priv, dom, profile, error);
		if (!ret)
			goto out;
	} else if (cd_dom_get_node (dom, profile, "x11_gamma") != NULL) {
		ret = cd_util_create_x11_gamma (priv, dom, profile, error);
		if (!ret)
			goto out;
	} else if (cd_dom_get_node (dom, profile, "named") != NULL) {
		ret = cd_util_create_named_color (priv, dom, profile, error);
		if (!ret)
			goto out;
	} else {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid XML, unknown type");
		goto out;
	}

	/* also write metadata */
	dict = cmsDictAlloc (NULL);
	tmp = cd_dom_get_node (dom, profile, "license");
	if (tmp != NULL) {
		_cmsDictAddEntryAscii (dict,
				       CD_PROFILE_METADATA_LICENSE,
				       cd_dom_get_node_data (tmp));
	}
	tmp = cd_dom_get_node (dom, profile, "standard_space");
	if (tmp != NULL) {
		_cmsDictAddEntryAscii (dict,
				       CD_PROFILE_METADATA_STANDARD_SPACE,
				       cd_dom_get_node_data (tmp));
	}
	tmp = cd_dom_get_node (dom, profile, "data_source");
	if (tmp != NULL) {
		_cmsDictAddEntryAscii (dict,
				       CD_PROFILE_METADATA_DATA_SOURCE,
				       cd_dom_get_node_data (tmp));
	}

	/* add CMS defines */
	_cmsDictAddEntryAscii (dict,
			       CD_PROFILE_METADATA_CMF_PRODUCT,
			       PACKAGE_NAME);
	_cmsDictAddEntryAscii (dict,
			       CD_PROFILE_METADATA_CMF_BINARY,
			       "cd-create-profile");
	_cmsDictAddEntryAscii (dict,
			       CD_PROFILE_METADATA_CMF_VERSION,
			       PACKAGE_VERSION);

	/* just write dict */
	ret = cmsWriteTag (priv->lcms_profile, cmsSigMetaTag, dict);
	if (!ret) {
		g_set_error_literal (error, 1, 0,
				     "cannot write metadata");
		goto out;
	}

	/* optional localized keys */
	tmp = cd_dom_get_node (dom, profile, "description");
	if (tmp != NULL) {
		ret = _cmsWriteTagTextAscii (priv->lcms_profile,
					     cmsSigProfileDescriptionTag,
					     cd_dom_get_node_data (tmp));
		if (!ret) {
			g_set_error_literal (error, 1, 0,
					     "failed to write description");
			goto out;
		}
	}
	tmp = cd_dom_get_node (dom, profile, "copyright");
	if (tmp != NULL) {
		ret = _cmsWriteTagTextAscii (priv->lcms_profile,
					     cmsSigCopyrightTag,
					     cd_dom_get_node_data (tmp));
		if (!ret) {
			g_set_error_literal (error, 1, 0,
					     "failed to write copyright");
			goto out;
		}
	}
	tmp = cd_dom_get_node (dom, profile, "model");
	if (tmp != NULL) {
		ret = _cmsWriteTagTextAscii (priv->lcms_profile,
					     cmsSigDeviceModelDescTag,
					     cd_dom_get_node_data (tmp));
		if (!ret) {
			g_set_error_literal (error, 1, 0,
					     "failed to write model");
			goto out;
		}
	}
	tmp = cd_dom_get_node (dom, profile, "manufacturer");
	if (tmp != NULL) {
		ret = _cmsWriteTagTextAscii (priv->lcms_profile,
					     cmsSigDeviceMfgDescTag,
					     cd_dom_get_node_data (tmp));
		if (!ret) {
			g_set_error_literal (error, 1, 0,
					     "failed to write manufacturer");
			goto out;
		}
	}
out:
	g_free (data);
	if (dom != NULL)
		g_object_unref (dom);
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
	gchar *filename = NULL;
	GError *error = NULL;
	guint retval = EXIT_FAILURE;

	const GOptionEntry options[] = {
		{ "output", 'o', 0, G_OPTION_ARG_STRING, &filename,
		/* TRANSLATORS: command line option */
		  _("Profile to create"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");
	g_type_init ();

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* setup LCMS */
	cmsSetLogErrorHandler (cd_fix_profile_error_cb);
	ret = cmsPlugin (&cd_util_lcms_rec709_trc);
	g_assert (ret);

	priv = g_new0 (CdUtilPrivate, 1);
	priv->context = g_option_context_new (NULL);

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
	ret = cd_util_create_from_xml (priv, argv[1], &error);
	if (!ret) {
		g_print ("%s\n", error->message);
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
		if (priv->lcms_profile != NULL)
			cmsCloseProfile (priv->lcms_profile);
		g_option_context_free (priv->context);
		g_free (priv);
	}
	g_free (cmd_descriptions);
	g_free (filename);
	return retval;
}

