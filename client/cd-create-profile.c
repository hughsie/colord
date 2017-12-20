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
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <locale.h>
#include <lcms2.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <colord-private.h>

#define LCMS_CURVE_PLUGIN_TYPE_REC709	1024

typedef struct {
	GOptionContext		*context;
	cmsHPROFILE		 lcms_profile;
	CdIcc			*icc;
} CdUtilPrivate;

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

static gboolean
cd_util_create_colprof (CdUtilPrivate *priv,
			CdDom *dom,
			const GNode *root,
			GError **error)
{
	const gchar *basename = "profile";
	const gchar *data_ti3;
	const gchar *viewcond;
	const GNode *node_enle;
	const GNode *node_enpo;
	const GNode *node_shape;
	const GNode *node_stle;
	const GNode *node_stpo;
	const GNode *tmp;
	gboolean ret = FALSE;
	gdouble enle;
	gdouble enpo;
	gdouble klimit;
	gdouble shape;
	gdouble stle;
	gdouble stpo;
	gdouble tlimit;
	gint exit_status = 0;
	gsize len = 0;
	g_autofree gchar *cmdline = NULL;
	g_autofree gchar *data = NULL;
	g_autofree gchar *debug_stderr = NULL;
	g_autofree gchar *debug_stdout = NULL;
	g_autofree gchar *output_fn = NULL;
	g_autofree gchar *ti3_fn = NULL;
	g_autofree gchar *tmpdir = NULL;
	g_autoptr(GFile) output_file = NULL;
	g_autoptr(GFile) ti3_file = NULL;
	g_autoptr(GFile) tmpdir_file = NULL;
	g_autoptr(GPtrArray) argv = NULL;

#ifndef TOOL_COLPROF
	/* no support */
	g_set_error_literal (error, 1, 0,
			     "not compiled with --enable-print-profiles");
	return FALSE;
#endif

	/* create common options */
	argv = g_ptr_array_new_with_free_func (g_free);
#ifdef TOOL_COLPROF
	g_ptr_array_add (argv, g_strdup (TOOL_COLPROF));
#endif
	g_ptr_array_add (argv, g_strdup ("-nc"));	/* no embedded ti3 */
	g_ptr_array_add (argv, g_strdup ("-qm"));	/* medium quality */
	g_ptr_array_add (argv, g_strdup ("-bm"));	/* medium quality B2A */

	/* get values */
	node_stle = cd_dom_get_node (dom, root, "stle");
	node_stpo = cd_dom_get_node (dom, root, "stpo");
	node_enpo = cd_dom_get_node (dom, root, "enpo");
	node_enle = cd_dom_get_node (dom, root, "enle");
	node_shape = cd_dom_get_node (dom, root, "shape");
	if (node_stle != NULL && node_stpo != NULL && node_enpo != NULL &&
	    node_enle != NULL && node_shape != NULL) {
		stle = cd_dom_get_node_data_as_double (node_stle);
		stpo = cd_dom_get_node_data_as_double (node_stpo);
		enpo = cd_dom_get_node_data_as_double (node_enpo);
		enle = cd_dom_get_node_data_as_double (node_enle);
		shape = cd_dom_get_node_data_as_double (node_shape);
		if (stle == G_MAXDOUBLE || stpo == G_MAXDOUBLE || enpo == G_MAXDOUBLE ||
		    enle == G_MAXDOUBLE || shape == G_MAXDOUBLE) {
			g_set_error_literal (error, 1, 0,
					     "XML error: invalid stle, stpo, enpo, enle, shape");
			return FALSE;
		}
		g_ptr_array_add (argv, g_strdup ("-kp"));
		g_ptr_array_add (argv, g_strdup_printf ("%f", stle));
		g_ptr_array_add (argv, g_strdup_printf ("%f", stpo));
		g_ptr_array_add (argv, g_strdup_printf ("%f", enpo));
		g_ptr_array_add (argv, g_strdup_printf ("%f", enle));
		g_ptr_array_add (argv, g_strdup_printf ("%f", shape));
	}

	/* total ink limit */
	tmp = cd_dom_get_node (dom, root, "tlimit");
	if (tmp != NULL) {
		tlimit = cd_dom_get_node_data_as_double (tmp);
		if (tlimit == G_MAXDOUBLE) {
			g_set_error_literal (error, 1, 0,
					     "XML error: invalid tlimit");
			return FALSE;
		}
		g_ptr_array_add (argv, g_strdup_printf ("-l%.0f", tlimit));
	}

	/* black ink limit */
	tmp = cd_dom_get_node (dom, root, "klimit");
	if (tmp != NULL) {
		klimit = cd_dom_get_node_data_as_double (tmp);
		if (klimit == G_MAXDOUBLE) {
			g_set_error_literal (error, 1, 0,
					     "XML error: invalid klimit");
			return FALSE;
		}
		g_ptr_array_add (argv, g_strdup_printf ("-L%.0f", klimit));
	}

	/* input viewing conditions */
	tmp = cd_dom_get_node (dom, root, "input_viewing_conditions");
	if (tmp != NULL) {
		viewcond = cd_dom_get_node_data (tmp);
		g_ptr_array_add (argv, g_strdup_printf ("-c%s", viewcond));
	}

	/* output viewing conditions */
	tmp = cd_dom_get_node (dom, root, "output_viewing_conditions");
	if (tmp != NULL) {
		viewcond = cd_dom_get_node_data (tmp);
		g_ptr_array_add (argv, g_strdup_printf ("-d%s", viewcond));
	}

	/* get source filename and copy into working directory */
	tmp = cd_dom_get_node (dom, root, "data_ti3");
	if (tmp == NULL) {
		g_set_error_literal (error, 1, 0,
				     "XML error: no data_ti3");
		return FALSE;
	}

	/* create temp location */
	tmpdir = g_dir_make_tmp ("cd-create-profile-XXXXXX", error);
	if (tmpdir == NULL)
		return FALSE;
	tmpdir_file = g_file_new_for_path (tmpdir);

	data_ti3 = cd_dom_get_node_data (tmp);
	ti3_fn = g_strdup_printf ("%s/%s.ti3", tmpdir, basename);
	ti3_file = g_file_new_for_path (ti3_fn);
	ret = g_file_replace_contents (ti3_file,
				       data_ti3,
				       strlen (data_ti3),
				       NULL,
				       FALSE,
				       G_FILE_CREATE_NONE,
				       NULL,
				       NULL,
				       error);
	if (!ret)
		return FALSE;

	/* ensure temporary icc profile does not already exist */
	output_fn = g_strdup_printf ("%s/%s.icc", tmpdir, basename);
	output_file = g_file_new_for_path (output_fn);
	if (g_file_query_exists (output_file, NULL)) {
		if (!g_file_delete (output_file, NULL, error))
			return FALSE;
	}

	/* run colprof in working directory */
	g_ptr_array_add (argv, g_strdup_printf ("-O%s.icc", basename));
	g_ptr_array_add (argv, g_strdup (basename));
	g_ptr_array_add (argv, NULL);
	ret = g_spawn_sync (tmpdir,
			    (gchar **) argv->pdata,
			    NULL,
			    0,
			    NULL, NULL,
			    &debug_stdout,
			    &debug_stderr,
			    &exit_status,
			    error);
	if (!ret)
		return FALSE;

	/* failed */
	if (exit_status != 0) {
		cmdline = g_strjoinv (" ", (gchar **) argv->pdata);
		g_set_error (error, 1, 0,
			     "Failed to generate %s using '%s'\nOutput: %s\nError:\t%s",
			     output_fn, cmdline, debug_stdout, debug_stderr);
		return FALSE;
	}

	/* load resulting .icc file */
	if (!g_file_load_contents (output_file, NULL, &data, &len, NULL, error))
		return FALSE;

	/* open $tmpdir/$basename.icc as hProfile */
	priv->lcms_profile = cmsOpenProfileFromMemTHR (cd_icc_get_context (priv->icc),
						       data, len);
	if (priv->lcms_profile == NULL) {
		g_set_error (error, 1, 0,
			     "Failed to open generated %s",
			     output_fn);
		return FALSE;
	}

	/* delete temp files */
	if (!g_file_delete (output_file, NULL, error))
		return FALSE;
	if (!g_file_delete (ti3_file, NULL, error))
		return FALSE;
	if (!g_file_delete (tmpdir_file, NULL, error))
		return FALSE;
	return TRUE;
}

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

	priv->lcms_profile = cmsCreateNULLProfileTHR (cd_icc_get_context (priv->icc));
	if (priv->lcms_profile == NULL) {
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
		g_set_error_literal (error, 1, 0, "XML error, expected x11_gamma");
		return FALSE;
	}
	if (!cd_dom_get_node_rgb (tmp, &rgb)) {
		g_set_error_literal (error, 1, 0, "XML error, invalid x11_gamma");
		return FALSE;
	}
	points[0] = rgb.R;
	points[1] = rgb.G;
	points[2] = rgb.B;

	/* create a bog-standard sRGB profile */
	priv->lcms_profile = cmsCreate_sRGBProfileTHR (cd_icc_get_context (priv->icc));
	if (priv->lcms_profile == NULL) {
		g_set_error_literal (error, 1, 0, "failed to create profile");
		return FALSE;
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
		return FALSE;
	}
	return TRUE;
}

static cmsToneCurve *
cd_util_build_srgb_gamma (CdUtilPrivate *priv)
{
	cmsFloat64Number params[5];
	params[0] = 2.4;
	params[1] = 1. / 1.055;
	params[2] = 0.055 / 1.055;
	params[3] = 1. / 12.92;
	params[4] = 0.04045;
	return cmsBuildParametricToneCurve (cd_icc_get_context (priv->icc), 4, params);
}

static cmsToneCurve *
cd_util_build_lstar_gamma (CdUtilPrivate *priv)
{
	cmsFloat64Number params[5];
	params[0] = 3.000000;
	params[1] = 0.862076;
	params[2] = 0.137924;
	params[3] = 0.110703;
	params[4] = 0.080002;
	return cmsBuildParametricToneCurve (cd_icc_get_context (priv->icc), 4, params);
}

static cmsToneCurve *
cd_util_build_rec709_gamma (CdUtilPrivate *priv)
{
	cmsFloat64Number params[5];
	params[0] = 1.0 / 0.45;
	params[1] = 1.099;
	params[2] = 0.099;
	params[3] = 4.500;
	params[4] = 0.018;
	return cmsBuildParametricToneCurve (cd_icc_get_context (priv->icc),
					    LCMS_CURVE_PLUGIN_TYPE_REC709, params);
}

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
	gdouble curve_gamma;

	/* parse gamma */
	tmp = cd_dom_get_node (dom, root, "gamma");
	if (tmp == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "XML error, expected gamma");
		goto out;
	}
	data = cd_dom_get_node_data (tmp);
	if (g_strcmp0 (data, "sRGB") == 0) {
		transfer[0] = cd_util_build_srgb_gamma (priv);
		transfer[1] = transfer[0];
		transfer[2] = transfer[0];
	} else if (g_strcmp0 (data, "L*") == 0) {
		transfer[0] = cd_util_build_lstar_gamma (priv);
		transfer[1] = transfer[0];
		transfer[2] = transfer[0];
	} else if (g_strcmp0 (data, "Rec709") == 0) {
		transfer[0] = cd_util_build_rec709_gamma (priv);
		transfer[1] = transfer[0];
		transfer[2] = transfer[0];
	} else {
		curve_gamma = cd_dom_get_node_data_as_double (tmp);
		if (curve_gamma == G_MAXDOUBLE) {
			ret = FALSE;
			g_set_error (error, 1, 0,
				     "failed to parse gamma: '%s'",
				     data);
			goto out;
		}
		transfer[0] = cmsBuildGamma (NULL, curve_gamma);
		transfer[1] = transfer[0];
		transfer[2] = transfer[0];
	}
	if (transfer[0] == NULL) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to set transfer function for %s",
			     data);
		goto out;
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
		white.x = 0.345703;
		white.y = 0.358539;
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
	priv->lcms_profile = cmsCreateRGBProfileTHR (cd_icc_get_context (priv->icc),
						     &white,
						     &primaries,
						     transfer);
	if (priv->lcms_profile == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "Failed to create profile");
		goto out;
	}
	ret = TRUE;
out:
	cmsFreeToneCurve (transfer[0]);
	return ret;
}

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
	gdouble curve_gamma;
	guint16 data[3][256];
	guint i;
	guint temp;

	/* create a bog-standard sRGB profile */
	priv->lcms_profile = cmsCreate_sRGBProfileTHR (cd_icc_get_context (priv->icc));
	if (priv->lcms_profile == NULL) {
		g_set_error_literal (error, 1, 0,
				     "failed to create profile");
		return FALSE;
	}

	/* parse temperature value */
	tmp = cd_dom_get_node (dom, root, "temperature");
	if (tmp == NULL) {
		g_set_error_literal (error, 1, 0, "XML error, expected temperature");
		return FALSE;
	}
	temp = atoi (cd_dom_get_node_data (tmp));

	/* parse gamma value */
	tmp = cd_dom_get_node (dom, root, "gamma");
	if (tmp == NULL) {
		g_set_error_literal (error, 1, 0, "XML error, expected gamma");
		return FALSE;
	}
	curve_gamma = cd_dom_get_node_data_as_double (tmp);
	if (curve_gamma == G_MAXDOUBLE) {
		g_set_error (error, 1, 0,
			     "failed to parse gamma: '%s'",
			     cd_dom_get_node_data (tmp));
		return FALSE;
	}

	/* generate the VCGT table */
	cd_color_get_blackbody_rgb (temp, &white_point);
	for (i = 0; i < size; i++) {
		data[0][i] = pow ((gdouble) i / size, 1.0 / curve_gamma) *
				  0xffff * white_point.R;
		data[1][i] = pow ((gdouble) i / size, 1.0 / curve_gamma) *
				  0xffff * white_point.G;
		data[2][i] = pow ((gdouble) i / size, 1.0 / curve_gamma) *
				  0xffff * white_point.B;
	}

	/* write vcgt */
	ret = set_vcgt_from_data (priv->lcms_profile,
				  data[0],
				  data[1],
				  data[2],
				  256);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "failed to write VCGT");
		return FALSE;
	}
	return TRUE;
}

static gboolean
cd_util_icc_set_metadata_coverage (CdIcc *icc, GError **error)
{
	const gchar *tmp;
	gdouble coverage = 0.0f;
	g_autofree gchar *coverage_tmp = NULL;
	g_autoptr(CdIcc) icc_srgb = NULL;

	/* is sRGB? */
	tmp = cd_icc_get_metadata_item (icc, CD_PROFILE_METADATA_STANDARD_SPACE);
	if (g_strcmp0 (tmp, "srgb") == 0)
		return TRUE;

	/* calculate coverage (quite expensive to calculate, hence metadata) */
	icc_srgb = cd_icc_new ();
	if (!cd_icc_create_default (icc_srgb, error))
		return FALSE;
	if (!cd_icc_utils_get_coverage (icc_srgb, icc, &coverage, error))
		return FALSE;
	if (coverage > 0.0) {
		coverage_tmp = g_strdup_printf ("%.2f", coverage);
		cd_icc_add_metadata (icc,
				     "GAMUT_coverage(srgb)",
				     coverage_tmp);
	}
	return TRUE;
}

static gboolean
cd_util_adjust_creation_time (CdUtilPrivate *priv, const char *source_filename, GError **error)
{
	g_autoptr(GFile) source_file;
	g_autoptr(GFileInfo) file_info;
	g_autoptr(GDateTime) creation_time;
	GTimeVal source_file_creation_time;
	gint64 build_date = 0;
	char *source_date_epoch_string;
	unsigned long long source_date_epoch;
	char *endptr;

	/* adjust creation time to match the XML one or SOURCE_DATE_EPOCH if specified and older */
	source_date_epoch_string = getenv ("SOURCE_DATE_EPOCH");
	if (source_date_epoch_string != NULL) {
		errno = 0;
		source_date_epoch = strtoull (source_date_epoch_string, &endptr, 10);
		if ((source_date_epoch == ERANGE && (source_date_epoch == ULLONG_MAX || source_date_epoch == 0))
		|| (errno != 0 && source_date_epoch == 0)) {
			g_set_error (error,
				     1,
				     0,
				     "Environment variable $SOURCE_DATE_EPOCH: strtoull: %s",
				     g_strerror (errno));
			return FALSE;
		}
		if (endptr == source_date_epoch_string) {
			g_set_error (error,
				     1,
				     0,
				     "Environment variable $SOURCE_DATE_EPOCH: No digits were found: %s",
				     endptr);
			return FALSE;
		}
		if (*endptr != '\0') {
			g_set_error (error,
				     1,
				     0,
				     "Environment variable $SOURCE_DATE_EPOCH: Trailing garbage: %s",
				     endptr);
			return FALSE;
		}
		if (source_date_epoch > ULONG_MAX) {
			g_set_error (error,
				     1,
				     0,
				     "Environment variable $SOURCE_DATE_EPOCH: value must be smaller than or equal to:"
				     " %lu but was found to be: %llu",
				     ULONG_MAX,
				     source_date_epoch);
			return FALSE;
		}
		build_date = source_date_epoch;
	}

	source_file = g_file_new_for_path (source_filename);
	file_info = g_file_query_info (source_file,
				       G_FILE_ATTRIBUTE_TIME_MODIFIED,
				       0,
				       NULL,
				       error);
	if (file_info != NULL) {
		g_file_info_get_modification_time (file_info, &source_file_creation_time);
		if (build_date > 0 &&
		    source_file_creation_time.tv_sec < build_date) {
			    build_date = source_file_creation_time.tv_sec;
		}
	} else if (build_date == 0) {
		/* No SOURCE_DATE_EPOCH and for some reason we can't stat() the source file; bail */
		return FALSE;
	}

	creation_time = g_date_time_new_from_unix_utc (build_date);
	if (creation_time == NULL) {
		/* We don't care about any previous errors, but g_file_query_info may have
		 * set some. It's safe to unconditionally clear it.
		 */
		g_clear_error (error);
		g_set_error_literal (error, 1, 0, "Build date outside of range supported by GDateTime");
		return FALSE;
	}
	cd_icc_set_created (priv->icc, creation_time);
	return TRUE;
}

static gboolean
cd_util_create_from_xml (CdUtilPrivate *priv,
			 const gchar *filename,
			 GError **error)
{
	const GNode *profile;
	const GNode *tmp;
	gboolean ret = TRUE;
	GHashTable *hash;
	gssize data_len = -1;
	g_autofree gchar *data = NULL;
	g_autoptr(CdDom) dom = NULL;

	/* parse the XML into DOM */
	if (!g_file_get_contents (filename, &data, (gsize *) &data_len, error))
		return FALSE;
	dom = cd_dom_new ();
	if (!cd_dom_parse_xml_data (dom, data, data_len, error))
		return FALSE;

	/* get root */
	profile = cd_dom_get_node (dom, NULL, "profile");
	if (profile == NULL) {
		g_set_error_literal (error, 1, 0, "invalid XML, expected profile");
		return FALSE;
	}

	/* get type */
	if (cd_dom_get_node (dom, profile, "primaries") != NULL) {
		if (!cd_util_create_standard_space (priv, dom, profile, error))
			return FALSE;
	} else if (cd_dom_get_node (dom, profile, "temperature") != NULL) {
		if (!cd_util_create_temperature (priv, dom, profile, error))
			return FALSE;
	} else if (cd_dom_get_node (dom, profile, "x11_gamma") != NULL) {
		if (!cd_util_create_x11_gamma (priv, dom, profile, error))
			return FALSE;
	} else if (cd_dom_get_node (dom, profile, "named") != NULL) {
		if (!cd_util_create_named_color (priv, dom, profile, error))
			return FALSE;
	} else if (cd_dom_get_node (dom, profile, "data_ti3") != NULL) {
		if (!cd_util_create_colprof (priv, dom, profile, error))
			return FALSE;
	} else {
		g_set_error_literal (error, 1, 0, "invalid XML, unknown type");
		return FALSE;
	}

	/* convert into a CdIcc object */
	ret = cd_icc_load_handle (priv->icc, priv->lcms_profile,
				  CD_ICC_LOAD_FLAGS_NONE, error);
	if (!ret)
		return FALSE;

	/* also write metadata */
	tmp = cd_dom_get_node (dom, profile, "license");
	if (tmp != NULL) {
		cd_icc_add_metadata (priv->icc,
				     CD_PROFILE_METADATA_LICENSE,
				     cd_dom_get_node_data (tmp));
	}
	tmp = cd_dom_get_node (dom, profile, "standard_space");
	if (tmp != NULL) {
		cd_icc_add_metadata (priv->icc,
				     CD_PROFILE_METADATA_STANDARD_SPACE,
				     cd_dom_get_node_data (tmp));
		if (!cd_util_icc_set_metadata_coverage (priv->icc, error))
			return FALSE;
	}
	tmp = cd_dom_get_node (dom, profile, "data_source");
	if (tmp != NULL) {
		cd_icc_add_metadata (priv->icc,
				     CD_PROFILE_METADATA_DATA_SOURCE,
				     cd_dom_get_node_data (tmp));
	}

	if (!cd_util_adjust_creation_time (priv, filename, error))
		return FALSE;

	/* add CMS defines */
	cd_icc_add_metadata (priv->icc,
			     CD_PROFILE_METADATA_CMF_PRODUCT,
			     PACKAGE_NAME);
	cd_icc_add_metadata (priv->icc,
			     CD_PROFILE_METADATA_CMF_BINARY,
			     "cd-create-profile");
	cd_icc_add_metadata (priv->icc,
			     CD_PROFILE_METADATA_CMF_VERSION,
			     PACKAGE_VERSION);

	/* optional localized keys */
	hash = cd_dom_get_node_localized (profile, "description");
	if (hash != NULL)
		cd_icc_set_description_items (priv->icc, hash);
	hash = cd_dom_get_node_localized (profile, "copyright");
	if (hash != NULL)
		cd_icc_set_copyright_items (priv->icc, hash);
	hash = cd_dom_get_node_localized (profile, "model");
	if (hash != NULL)
		cd_icc_set_model_items (priv->icc, hash);
	hash = cd_dom_get_node_localized (profile, "manufacturer");
	if (hash != NULL)
		cd_icc_set_manufacturer_items (priv->icc, hash);
	return TRUE;
}

int
main (int argc, char **argv)
{
	CdUtilPrivate *priv;
	gboolean ret;
	guint retval = EXIT_FAILURE;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *cmd_descriptions = NULL;
	g_autofree gchar *filename = NULL;
	g_autoptr(GFile) file = NULL;
	const GOptionEntry options[] = {
		{ "output", 'o', 0, G_OPTION_ARG_STRING, &filename,
		/* TRANSLATORS: command line option */
		  _("Profile to create"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	priv = g_new0 (CdUtilPrivate, 1);
	priv->icc = cd_icc_new ();
	priv->context = g_option_context_new (NULL);

	/* TRANSLATORS: program name */
	g_set_application_name (_("ICC profile creation program"));
	g_option_context_add_main_entries (priv->context, options, NULL);
	ret = g_option_context_parse (priv->context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: the user didn't read the man page */
		g_print ("%s: %s\n", _("Failed to parse arguments"),
			 error->message);
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
		goto out;
	}

	/* write file */
	file = g_file_new_for_path (filename);
	ret = cd_icc_save_file (priv->icc,
				file,
				CD_ICC_SAVE_FLAGS_NONE,
				NULL,
				&error);
	if (!ret) {
		g_print ("%s\n", error->message);
		goto out;
	}

	/* success */
	retval = EXIT_SUCCESS;
out:
	if (priv != NULL) {
		g_option_context_free (priv->context);
		g_object_unref (priv->icc);
		g_free (priv);
	}
	return retval;
}

