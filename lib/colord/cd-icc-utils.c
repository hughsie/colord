/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Richard Hughes <richard@hughsie.com>
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

/**
 * cd_icc_utils_get_coverage_sample_cb:
 **/
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

/**
 * cd_icc_utils_get_coverage_calc:
 **/
static gboolean
cd_icc_utils_get_coverage_calc (CdIcc *icc,
				CdIcc *icc_reference,
				gdouble *coverage,
				GError **error)
{
	const guint cube_size = 33;
	cmsFloat32Number *data = NULL;
	cmsHPROFILE profile_null = NULL;
	cmsHTRANSFORM transform = NULL;
	cmsUInt16Number *alarm_codes = NULL;
	cmsUInt32Number dimensions[] = { cube_size, cube_size, cube_size };
	CdIccUtilsGamutCheckHelper helper;
	gboolean ret = TRUE;
	guint cnt = 0;
	guint data_len = cube_size * cube_size * cube_size;
	guint i;

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
	cmsSetAlarmCodes (alarm_codes);

	/* slice profile in regular intevals */
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
	g_free (data);
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
	gboolean ret;
	gdouble coverage_tmp;

	/* first see if icc has a smaller gamut volume to the reference */
	ret = cd_icc_utils_get_coverage_calc (icc,
					      icc_reference,
					      &coverage_tmp,
					      error);
	if (!ret)
		goto out;

	/* now try the other way around */
	if (coverage_tmp >= 1.0f) {
		ret = cd_icc_utils_get_coverage_calc (icc_reference,
						      icc,
						      &coverage_tmp,
						      error);
		if (!ret)
			goto out;
		coverage_tmp = 1 / coverage_tmp;
	}

	/* success */
	if (coverage != NULL)
		*coverage = coverage_tmp;
out:
	return ret;
}
