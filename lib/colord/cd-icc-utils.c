/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2020 NVIDIA CORPORATION
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
 * SECTION:cd-icc-utils
 * @short_description: Utilities for working with CdIcc objects
 *
 * Functions to do unusual things with profiles.
 */

#include "config.h"

#include <glib-object.h>
#include <lcms2.h>

#include "cd-icc-utils.h"

typedef struct {
	guint			 idx;
	cmsFloat32Number	*data;
} CdIccUtilsGamutCheckHelper;

static cmsInt32Number
cd_icc_utils_get_coverage_sample_cb (const cmsFloat32Number in[],
				     cmsFloat32Number out[],
				     void *user_data)
{
	CdIccUtilsGamutCheckHelper *helper = (CdIccUtilsGamutCheckHelper *) user_data;
	helper->data[helper->idx++] = in[0];
	helper->data[helper->idx++] = in[1];
	helper->data[helper->idx++] = in[2];
	return 1;
}

static gboolean
cd_icc_utils_get_coverage_calc (CdIcc *icc,
				CdIcc *icc_reference,
				gdouble *coverage,
				GError **error)
{
	const guint cube_size = 33;
	cmsHPROFILE profile_null = NULL;
	cmsHTRANSFORM transform = NULL;
	cmsUInt32Number dimensions[] = { cube_size, cube_size, cube_size };
	CdIccUtilsGamutCheckHelper helper;
	gboolean ret = TRUE;
	guint cnt = 0;
	guint data_len = cube_size * cube_size * cube_size;
	guint i;
	g_autofree cmsFloat32Number *data = NULL;
	g_autofree cmsUInt16Number *alarm_codes = NULL;

	/* create a proofing transform with gamut check */
	profile_null = cmsCreateNULLProfileTHR (cd_icc_get_context (icc));
	transform = cmsCreateProofingTransformTHR (cd_icc_get_context (icc),
						   cd_icc_get_handle (icc),
						   TYPE_RGB_FLT,
						   profile_null,
						   TYPE_GRAY_FLT,
						   cd_icc_get_handle (icc_reference),
						   INTENT_ABSOLUTE_COLORIMETRIC,
						   INTENT_ABSOLUTE_COLORIMETRIC,
						   cmsFLAGS_GAMUTCHECK |
						   cmsFLAGS_SOFTPROOFING);
	if (transform == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_ICC_ERROR,
			     CD_ICC_ERROR_INVALID_COLORSPACE,
			     "Failed to setup transform for %s->%s",
			     cd_icc_get_filename (icc),
			     cd_icc_get_filename (icc_reference));
		goto out;
	}

	/* set gamut alarm to 0xffff */
	alarm_codes = g_new0 (cmsUInt16Number, cmsMAXCHANNELS);
	alarm_codes[0] = 0xffff;
	cmsSetAlarmCodesTHR(cd_icc_get_context (icc), alarm_codes);

	/* slice profile in regular intervals */
	data = g_new0 (cmsFloat32Number, data_len * 3);
	helper.data = data;
	helper.idx = 0;
	ret = cmsSliceSpaceFloat (3, dimensions,
				  cd_icc_utils_get_coverage_sample_cb,
				  &helper);
	if (!ret) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_INTERNAL,
				     "Failed to slice data");
		goto out;
	}

	/* transform each one of those nodes across the proofing transform */
	cmsDoTransform (transform, helper.data, helper.data, data_len);

	/* count the nodes that gives you zero and divide by total number */
	for (i = 0; i < data_len; i++) {
		if (helper.data[i] == 0.0)
			cnt++;
	}

	/* success */
	if (coverage != NULL)
		*coverage = (gdouble) cnt / (gdouble) data_len;
out:
	cmsCloseProfile (profile_null);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	return ret;
}

/**
 * cd_icc_utils_get_coverage:
 * @icc: The profile to test
 * @icc_reference: The reference profile, e.g. sRGB
 * @coverage: The coverage of @icc on @icc_reference
 * @error: A #GError, or %NULL
 *
 * Gets the gamut coverage of two profiles where 0.5 would mean the gamut is
 * half the size, and 2.0 would indicate the gamut is twice the size.
 *
 * Return value: TRUE for success
 **/
gboolean
cd_icc_utils_get_coverage (CdIcc *icc,
			   CdIcc *icc_reference,
			   gdouble *coverage,
			   GError **error)
{
	gdouble coverage_tmp;

	/* first see if icc has a smaller gamut volume to the reference */
	if (!cd_icc_utils_get_coverage_calc (icc,
					     icc_reference,
					     &coverage_tmp,
					     error))
		return FALSE;

	/* now try the other way around */
	if (coverage_tmp >= 1.0f) {
		if (!cd_icc_utils_get_coverage_calc (icc_reference,
						     icc,
						     &coverage_tmp,
						     error))
			return FALSE;
		coverage_tmp = 1 / coverage_tmp;
	}

	/* success */
	if (coverage != NULL)
		*coverage = coverage_tmp;
	return TRUE;
}

/**
 * cd_icc_utils_get_chroma_matrix:
 * @icc: The profile to use.
 * @mat: (out): The returned matrix containing the primaries from @icc.
 *
 * Fills a 3x3 matrix with the XYZ red, green, and blue primary values from an
 * ICC profile as columns.
 */
static void
cd_icc_utils_get_chroma_matrix (CdIcc *icc, CdMat3x3 *mat)
{
	const CdColorXYZ *red = cd_icc_get_red (icc);
	const CdColorXYZ *green = cd_icc_get_green (icc);
	const CdColorXYZ *blue = cd_icc_get_blue (icc);
	CdMat3x3 matrix = {
		red->X, green->X, blue->X,
		red->Y, green->Y, blue->Y,
		red->Z, green->Z, blue->Z };

	*mat = matrix;
}

/**
 * cd_bradford_transform:
 * @reference: The white point to use as a reference.
 * @measured: The white point measurement for the target device.
 * @mat: (out): The returned Bradford color adaptation matrix.
 */
static void
cd_bradford_transform (const CdColorXYZ *reference,
		       const CdColorXYZ *measured,
		       CdMat3x3 *mat)
{
	/* see https://onlinelibrary.wiley.com/doi/pdf/10.1002/9781119021780.app3 */
	const CdMat3x3 bradford_response_matrix = {
		 0.8951,  0.2664, -0.1614,
		-0.7502,  1.7135,  0.0367,
		 0.0389, -0.0685,  1.0296 };
	CdMat3x3 bradford_inv;
	CdMat3x3 ratio;
	CdMat3x3 tmp;
	CdVec3 ref_xyz = { reference->X / reference->Y,
			   1.0,
			   reference->Z / reference->Y };
	CdVec3 meas_xyz = { measured->X / measured->Y,
			    1.0,
			    measured->Z / measured->Y };
	CdVec3 ref_rgb;
	CdVec3 meas_rgb;

	/* convert XYZ white point values to RGB */
	cd_mat33_vector_multiply (&bradford_response_matrix,
				  &ref_xyz,
				  &ref_rgb);
	cd_mat33_vector_multiply (&bradford_response_matrix,
				  &meas_xyz,
				  &meas_rgb);

	/* construct a diagonal matrix D of the ratios between the RGB values */
	cd_mat33_clear (&ratio);
	ratio.m00 = meas_rgb.v0 / ref_rgb.v0;
	ratio.m11 = meas_rgb.v1 / ref_rgb.v1;
	ratio.m22 = meas_rgb.v2 / ref_rgb.v2;

	/* transform is inv(B) * D * B */
	cd_mat33_reciprocal (&bradford_response_matrix, &bradford_inv);
	cd_mat33_matrix_multiply (&bradford_inv, &ratio, &tmp);
	cd_mat33_matrix_multiply (&tmp, &bradford_response_matrix, mat);
}

/**
 * cd_icc_utils_get_adaptation_matrix:
 * @icc: The measured ICC profile for the target device.
 * @icc_reference: The ICC profile to use as a reference (typically sRGB).
 * @mat: (out): The returned adaptation matrix.
 * @error: (out): A #GError, or %NULL
 *
 * Computes a correction matrix suitable for adjusting colors in a reference
 * color space @icc_reference (typically sRGB) to the color space of a target
 * device described by @icc.
 *
 * This function is designed to be used by desktop window systems to program the
 * color transform matrix (CTM) property of the display hardware.
 *
 * Return value: %TRUE for success
 *
 * Since: 1.4.5
 */
gboolean
cd_icc_utils_get_adaptation_matrix (CdIcc *icc,
				    CdIcc *icc_reference,
				    CdMat3x3 *mat,
				    GError **error)
{
	CdMat3x3 reference;
	CdMat3x3 measured_chroma;
	CdMat3x3 measured;
	CdMat3x3 measured_inv;
	CdMat3x3 bradford;

	cd_icc_utils_get_chroma_matrix (icc_reference, &reference);
	cd_icc_utils_get_chroma_matrix (icc, &measured_chroma);

	/* compute a Bradford color adaptation transform from the measured white
	 * point to the reference white point */
	cd_bradford_transform (cd_icc_get_white (icc_reference),
			       cd_icc_get_white (icc),
			       &bradford);

	/* use the Bradford transform to adjust the measured chroma values to
	 * match the reference luminance */
	cd_mat33_matrix_multiply (&bradford, &measured_chroma, &measured);

	/* invert the adjusted measured chroma matrix and multiply by the
	 * reference colors to compute the resulting CSC matrix */
	cd_mat33_reciprocal (&measured, &measured_inv);
	cd_mat33_matrix_multiply (&measured_inv,
				  &reference,
				  mat);

	return cd_mat33_is_finite (mat, error);
}
