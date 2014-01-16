/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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

/**
 * SECTION:cd-it8-utils
 * @short_description: Utilities for working with CdIt8 objects
 *
 * Functions to do cool things with sets of reference and measurements.
 */

#include "config.h"

#include <glib-object.h>
#include <math.h>

#include "cd-color.h"
#include "cd-it8-utils.h"
#include "cd-math.h"

/**
 * ch_it8_utils_4color_read_data:
 **/
static gboolean
ch_it8_utils_4color_read_data (CdIt8 *it8,
			       CdMat3x3 *mat_xyz,
			       CdVec3 *vec_w,
			       gdouble *abs_lumi,
			       GError **error)
{
	CdColorXYZ ave_XYZ[5];
	CdColorXYZ tmp_XYZ;
	CdColorYxy tmp_Yxy[5];
	gboolean ret = TRUE;
	guint i, j;
	guint len;

	/* ensur we have multiple of 5s */
	len = cd_it8_get_data_size (it8);
	if (len % 5 != 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "expected black, white, red, green, blue");
		goto out;
	}

	/* find patches */
	for (j = 0; j < 5; j++)
		cd_color_xyz_set (&ave_XYZ[j], 0.0f, 0.0f, 0.0f);
	for (i = 0; i < len; i += 5) {
		/* black, white, red, green, blue */
		for (j = 0; j < 5; j++) {
			cd_it8_get_data_item (it8, i + j, NULL, &tmp_XYZ);
			ave_XYZ[j].X += tmp_XYZ.X;
			ave_XYZ[j].Y += tmp_XYZ.Y;
			ave_XYZ[j].Z += tmp_XYZ.Z;
		}
	}

	/* average */
	for (j = 0; j < 5; j++) {
		ave_XYZ[j].X /= len / 5;
		ave_XYZ[j].Y /= len / 5;
		ave_XYZ[j].Z /= len / 5;
	}

	/* save luminance */
	if (abs_lumi != NULL)
		*abs_lumi = ave_XYZ[1].Y;

	g_debug ("black XYZ = %f %f %f", ave_XYZ[0].X, ave_XYZ[0].Y, ave_XYZ[0].Z);
	g_debug ("white XYZ = %f %f %f", ave_XYZ[1].X, ave_XYZ[1].Y, ave_XYZ[1].Z);
	g_debug ("red XYZ = %f %f %f", ave_XYZ[2].X, ave_XYZ[2].Y, ave_XYZ[2].Z);
	g_debug ("green XYZ = %f %f %f", ave_XYZ[3].X, ave_XYZ[3].Y, ave_XYZ[3].Z);
	g_debug ("blue XYZ = %f %f %f", ave_XYZ[4].X, ave_XYZ[4].Y, ave_XYZ[4].Z);

	/* Convert XYZ to Yxy */
	for (j = 0; j < 5; j++)
		cd_color_xyz_to_yxy (&ave_XYZ[j], &tmp_Yxy[j]);

	/* create chroma of M_RGB */
	mat_xyz->m00 = tmp_Yxy[2].x;
	mat_xyz->m10 = tmp_Yxy[2].y;
	mat_xyz->m20 = 1 - tmp_Yxy[2].x - tmp_Yxy[2].y;
	mat_xyz->m01 = tmp_Yxy[3].x;
	mat_xyz->m11 = tmp_Yxy[3].y;
	mat_xyz->m21 = 1 - tmp_Yxy[3].x - tmp_Yxy[3].y;
	mat_xyz->m02 = tmp_Yxy[4].x;
	mat_xyz->m12 = tmp_Yxy[4].y;
	mat_xyz->m22 = 1 - tmp_Yxy[4].x - tmp_Yxy[4].y;

	/* create white */
	vec_w->v0 = tmp_Yxy[1].x;
	vec_w->v1 = tmp_Yxy[1].y;
	vec_w->v2 = 1 - tmp_Yxy[1].x - tmp_Yxy[1].y;
out:
	return ret;
}

/**
 * ch_it8_utils_4color_decompose:
 **/
static gboolean
ch_it8_utils_4color_decompose (CdIt8 *it8,
			       CdMat3x3 *rgb,
			       gdouble *abs_lumi,
			       GError **error)
{
	CdMat3x3 chroma;
	CdMat3x3 chroma_inv;
	CdMat3x3 lumi;
	CdVec3 lumi_v;
	CdVec3 white_v;
	gboolean ret;
	gchar *tmp;

	/* read reference matrix */
	ret = ch_it8_utils_4color_read_data (it8,
				   &chroma,
				   &white_v,
				   abs_lumi,
				   error);
	if (!ret)
		goto out;

	/* print what we've got */
	tmp = cd_mat33_to_string (&chroma);
	g_debug ("chroma = %s", tmp);
	g_free (tmp);
	tmp = cd_vec3_to_string (&white_v);
	g_debug ("lumi = %s", tmp);
	g_free (tmp);

	/* invert chroma of M_RGB and multiply it with white */
	ret = cd_mat33_reciprocal (&chroma, &chroma_inv);
	if (!ret) {
		ret = FALSE;
		tmp = cd_mat33_to_string (&chroma);
		g_set_error (error, 1, 0,
			     "failed to invert %s", tmp);
		g_free (tmp);
		goto out;
	}
	cd_mat33_vector_multiply (&chroma_inv, &white_v, &lumi_v);

	/* create luminance of M_RGB (k) */
	cd_mat33_clear (&lumi);
	lumi.m00 = lumi_v.v0;
	lumi.m11 = lumi_v.v1;
	lumi.m22 = lumi_v.v2;

	/* create RGB */
	cd_mat33_matrix_multiply (&chroma, &lumi, rgb);
out:
	return ret;
}

/**
 * cd_it8_utils_calculate_ccmx:
 * @it8_reference: The reference data
 * @it8_measured: The measured data
 * @it8_ccmx: The calculated correction matrix
 * @error: A #GError, or %NULL
 *
 * This calculates the colorimter correction matrix using the Four-Color
 * Matrix Method by Yoshihiro Ohno and Jonathan E. Hardis, 1997.
 *
 * Return value: %TRUE if a correction matrix was found.
 **/
gboolean
cd_it8_utils_calculate_ccmx (CdIt8 *it8_reference,
			     CdIt8 *it8_measured,
			     CdIt8 *it8_ccmx,
			     GError **error)
{
	CdMat3x3 calibration;
	CdMat3x3 m_rgb;
	CdMat3x3 m_rgb_inv;
	CdMat3x3 n_rgb;
	const gdouble *data;
	gboolean ret;
	gchar *tmp = NULL;
	gdouble m_lumi = 0.0f;
	gdouble n_lumi = 0.0f;
	guint i;

	/* read reference matrix */
	ret = ch_it8_utils_4color_decompose (it8_reference, &n_rgb, &n_lumi, error);
	if (!ret)
		goto out;

	/* read measured matrix */
	ret = ch_it8_utils_4color_decompose (it8_measured, &m_rgb, &m_lumi, error);
	if (!ret)
		goto out;

	/* create m_RGB^-1 */
	ret = cd_mat33_reciprocal (&m_rgb, &m_rgb_inv);
	if (!ret) {
		tmp = cd_mat33_to_string (&m_rgb);
		g_set_error (error, 1, 0,
			     "failed to invert %s", tmp);
		goto out;
	}

	/* create M */
	cd_mat33_matrix_multiply (&n_rgb, &m_rgb_inv, &calibration);

	/* scale up to absolute values */
	g_debug ("m_lumi=%f, n_lumi=%f", m_lumi, n_lumi);
	cd_mat33_scalar_multiply (&calibration,
				  n_lumi / m_lumi,
				  &calibration);
	tmp = cd_mat33_to_string (&calibration);
	g_debug ("device calibration = %s", tmp);

	/* check there are no nan's or inf's */
	data = cd_mat33_get_data (&calibration);
	for (i = 0; i < 9; i++) {
		if (fpclassify (data[i]) != FP_NORMAL) {
			ret = FALSE;
			g_set_error (error, 1, 0,
				     "Matrix value %i non-normal: %f", i, data[i]);
			goto out;
		}
	}

	/* save to ccmx file */
	cd_it8_set_matrix (it8_ccmx, &calibration);
	cd_it8_set_instrument (it8_ccmx, cd_it8_get_instrument (it8_measured));
	cd_it8_set_reference (it8_ccmx, cd_it8_get_instrument (it8_reference));
out:
	g_free (tmp);
	return ret;
}

/**
 * cd_it8_utils_calculate_xyz_from_cmf:
 * @cmf: The color match function
 * @illuminant: The illuminant (you can use cd_spectrum_new() for type E)
 * @spectrum: The #CdSpectrum input data
 * @value: The #CdColorXYZ result
 * @resolution: The resolution in nm, typically 1.0
 * @error: A #GError, or %NULL
 *
 * This calculates the XYZ from a CMF, illuminant and input spectrum.
 *
 * Return value: %TRUE if a XYZ value was set.
 **/
gboolean
cd_it8_utils_calculate_xyz_from_cmf (CdIt8 *cmf,
				     CdSpectrum *illuminant,
				     CdSpectrum *spectrum,
				     CdColorXYZ *value,
				     gdouble resolution,
				     GError **error)
{
	CdSpectrum *observer[3];
	gboolean ret = TRUE;
	gdouble end;
	gdouble i_val;
	gdouble o_val;
	gdouble s_val;
	gdouble scale = 0.f;
	gdouble start;
	gdouble wl;

	g_return_val_if_fail (CD_IS_IT8 (cmf), FALSE);
	g_return_val_if_fail (illuminant != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	/* check this is a CMF */
	if (cd_it8_get_kind (cmf) != CD_IT8_KIND_CMF) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_IT8_ERROR,
				     CD_IT8_ERROR_FAILED,
				     "not a CMF IT8 object");
		goto out;
	}
	observer[0] = cd_it8_get_spectrum_by_id (cmf, "X");
	observer[1] = cd_it8_get_spectrum_by_id (cmf, "Y");
	observer[2] = cd_it8_get_spectrum_by_id (cmf, "Z");
	if (observer[0] == NULL || observer[1] == NULL || observer[2] == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_IT8_ERROR,
				     CD_IT8_ERROR_FAILED,
				     "CMF IT8 object has no X,Y,Y channel");
		goto out;
	}

	/* calculate the integrals */
	start = cd_spectrum_get_start (observer[0]);
	end = cd_spectrum_get_end (observer[0]);
	cd_color_xyz_clear (value);
	for (wl = start; wl <= end; wl += resolution) {
		i_val = cd_spectrum_get_value_for_nm (illuminant, wl);
		s_val = cd_spectrum_get_value_for_nm (spectrum, wl);
		o_val = cd_spectrum_get_value_for_nm (observer[0], wl);
		value->X += i_val * o_val * s_val;
		o_val = cd_spectrum_get_value_for_nm (observer[1], wl);
		scale += i_val * o_val;
		value->Y += i_val * o_val * s_val;
		o_val = cd_spectrum_get_value_for_nm (observer[2], wl);
		value->Z += i_val * o_val * s_val;
	}

	/* scale by Y */
	value->X /= scale;
	value->Y /= scale;
	value->Z /= scale;
out:
	return ret;
}

/**
 * cd_it8_utils_calculate_cri_from_cmf:
 * @cmf: The color match function
 * @tcs: The CIE TCS test patches
 * @illuminant: The illuminant
 * @value: The CRI result
 * @resolution: The resolution in nm, typically 1.0
 * @error: A #GError, or %NULL
 *
 * This calculates the CRI for a specific illuminant.
 *
 * Return value: %TRUE if a XYZ value was set.
 **/
gboolean
cd_it8_utils_calculate_cri_from_cmf (CdIt8 *cmf,
				     CdIt8 *tcs,
				     CdSpectrum *illuminant,
				     gdouble *value,
				     gdouble resolution,
				     GError **error)
{
	CdColorUVW d1;
	CdColorUVW d2;
	CdColorUVW reference_uvw[8];
	CdColorUVW unknown_uvw[8];
	CdColorXYZ illuminant_xyz;
	CdColorXYZ reference_illuminant_xyz;
	CdColorXYZ sample_xyz;
	CdColorYxy yxy;
	CdSpectrum *reference_illuminant = NULL;
	CdSpectrum *sample;
	CdSpectrum *unity;
	GPtrArray *samples;
	gboolean ret;
	gdouble cct;
	gdouble ri_sum = 0.f;
	gdouble val;
	guint i;

	/* get the illuminant CCT */
	unity = cd_spectrum_new ();
	ret = cd_it8_utils_calculate_xyz_from_cmf (cmf,
						   unity,
						   illuminant,
						   &illuminant_xyz,
						   resolution,
						   error);
	if (!ret)
		goto out;
	cct = cd_color_xyz_to_cct (&illuminant_xyz);
	cd_color_xyz_normalize (&illuminant_xyz, 1.0, &illuminant_xyz);

	/* get the reference illuminant */
	if (cct < 5000) {
		reference_illuminant = cd_spectrum_planckian_new (cct);
	} else {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_IT8_ERROR,
				     CD_IT8_ERROR_FAILED,
				     "need to use CIE standard illuminant D");
		goto out;
	}
	cd_spectrum_normalize (reference_illuminant, 560, 1.0);
	ret = cd_it8_utils_calculate_xyz_from_cmf (cmf,
						   unity,
						   reference_illuminant,
						   &reference_illuminant_xyz,
						   resolution,
						   error);
	if (!ret)
		goto out;

	/* check the source is white enough */
	cd_color_uvw_set_planckian_locus (&d1, cct);
	cd_color_xyz_to_yxy (&illuminant_xyz, &yxy);
	cd_color_yxy_to_uvw (&yxy, &d2);
	val = cd_color_uvw_get_chroma_difference (&d1, &d2);
	if (val > 5.4e-3) {
		ret = FALSE;
		g_set_error (error,
			     CD_IT8_ERROR,
			     CD_IT8_ERROR_FAILED,
			     "result not meaningful, DC=%f", val);
		goto out;
	}

	/* get the XYZ for each color sample under the reference illuminant */
	samples = cd_it8_get_spectrum_array (tcs);
	for (i = 0; i < 8; i++) {
		sample = g_ptr_array_index (samples, i);
		ret = cd_it8_utils_calculate_xyz_from_cmf (cmf,
							   reference_illuminant,
							   sample,
							   &sample_xyz,
							   1.f,
							   error);
		if (!ret)
			goto out;
		cd_color_xyz_to_uvw (&sample_xyz,
				     &illuminant_xyz,
				     &reference_uvw[i]);
	}

	/* get the XYZ for each color sample under the unknown illuminant */
	samples = cd_it8_get_spectrum_array (tcs);
	for (i = 0; i < 8; i++) {
		sample = g_ptr_array_index (samples, i);
		ret = cd_it8_utils_calculate_xyz_from_cmf (cmf,
							   illuminant,
							   sample,
							   &sample_xyz,
							   resolution,
							   error);
		if (!ret)
			goto out;
		cd_color_xyz_to_uvw (&sample_xyz,
				     &illuminant_xyz,
				     &unknown_uvw[i]);
	}

	/* add up all the Ri's and take the average to get the CRI */
	for (i = 0; i < 8; i++) {
		val = cd_color_uvw_get_chroma_difference (&reference_uvw[i],
							  &unknown_uvw[i]);
		ri_sum += 100 - (4.6 * val);
	}
	*value = ri_sum / 8;
out:
	if (reference_illuminant != NULL)
		cd_spectrum_free (reference_illuminant);
	return ret;
}
