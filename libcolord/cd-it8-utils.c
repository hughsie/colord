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
		cd_color_set_xyz (&ave_XYZ[j], 0.0f, 0.0f, 0.0f);
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
		cd_color_convert_xyz_to_yxy (&ave_XYZ[j], &tmp_Yxy[j]);

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
	gboolean ret;
	gchar *tmp = NULL;
	gdouble m_lumi = 0.0f;
	gdouble n_lumi = 0.0f;

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

	/* save to ccmx file */
	cd_it8_set_matrix (it8_ccmx, &calibration);
	cd_it8_set_instrument (it8_ccmx, cd_it8_get_instrument (it8_measured));
	cd_it8_set_reference (it8_ccmx, cd_it8_get_instrument (it8_reference));
out:
	g_free (tmp);
	return ret;
}
