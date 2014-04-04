/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2013 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <math.h>
#include <locale.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "cd-buffer.h"
#include "cd-color.h"
#include "cd-dom.h"
#include "cd-edid.h"
#include "cd-icc.h"
#include "cd-icc-store.h"
#include "cd-icc-utils.h"
#include "cd-interp-akima.h"
#include "cd-interp.h"
#include "cd-interp-linear.h"
#include "cd-it8.h"
#include "cd-it8-utils.h"
#include "cd-math.h"
#include "cd-spectrum.h"
#include "cd-transform.h"
#include "cd-version.h"

#include "cd-test-shared.h"

static void
colord_it8_cri_util_func (void)
{
	CdIt8 *cmf;
	CdIt8 *tcs;
	CdIt8 *test;
	CdSpectrum *f4;
	GError *error = NULL;
	GFile *file;
	gboolean ret;
	gdouble value = 0.f;

	/* load a CMF */
	cmf = cd_it8_new ();
	file = g_file_new_for_path ("../../data/cmf/CIE1931-2deg-XYZ.cmf");
	ret = cd_it8_load_from_file (cmf, file, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);
	g_assert_cmpint (cd_it8_get_kind (cmf), ==, CD_IT8_KIND_CMF);

	/* load the TCS */
	tcs = cd_it8_new ();
	file = g_file_new_for_path ("../../data/ref/CIE-TCS.sp");
	ret = cd_it8_load_from_file (tcs, file, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);
	g_assert_cmpint (cd_it8_get_kind (tcs), ==, CD_IT8_KIND_SPECT);

	/* load the test spectra */
	test = cd_it8_new ();
	file = g_file_new_for_path ("../../data/illuminant/CIE-F4.sp");
	ret = cd_it8_load_from_file (test, file, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);
	g_assert_cmpint (cd_it8_get_kind (test), ==, CD_IT8_KIND_SPECT);

	/* calculate the CRI */
	f4 = cd_it8_get_spectrum_by_id (test, "1");
	g_assert (f4 != NULL);
	ret = cd_it8_utils_calculate_cri_from_cmf (cmf, tcs, f4, &value, 1.0f, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check the CRI */
	g_assert_cmpfloat (value, <, 52);
	g_assert_cmpfloat (value, >, 50);

	g_object_unref (test);
	g_object_unref (cmf);
	g_object_unref (tcs);

}

static void
colord_it8_spectra_util_func (void)
{
	CdColorXYZ value;
	CdIt8 *cmf;
	CdIt8 *spectra;
	CdSpectrum *data;
	CdSpectrum *unity;
	GError *error = NULL;
	GFile *file;
	gboolean ret;
	gchar *filename;

	/* load a CMF */
	cmf = cd_it8_new ();
	filename = cd_test_get_filename ("example.cmf");
	file = g_file_new_for_path (filename);
	ret = cd_it8_load_from_file (cmf, file, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);
	g_free (filename);
	g_assert_cmpint (cd_it8_get_kind (cmf), ==, CD_IT8_KIND_CMF);

	/* load a spectra */
	spectra = cd_it8_new ();
	filename = cd_test_get_filename ("example.sp");
	file = g_file_new_for_path (filename);
	ret = cd_it8_load_from_file (spectra, file, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);
	g_free (filename);
	g_assert_cmpint (cd_it8_get_kind (spectra), ==, CD_IT8_KIND_SPECT);

	/* calculate the XYZ value */
	data = g_ptr_array_index (cd_it8_get_spectrum_array (spectra), 0);
	unity = cd_spectrum_new ();
	ret = cd_it8_utils_calculate_xyz_from_cmf (cmf, unity, data, &value, 1.f, &error);
	g_assert_no_error (error);
	g_assert (ret);
	cd_color_xyz_normalize (&value, 1.0, &value);
	g_assert_cmpfloat (value.X, >, 0.975163f - 0.01);
	g_assert_cmpfloat (value.X, <, 0.975163f + 0.01);
	g_assert_cmpfloat (value.Y, >, 1.f - 0.01);
	g_assert_cmpfloat (value.Y, <, 1.f + 0.01);
	g_assert_cmpfloat (value.Z, >, 0.813050f - 0.01);
	g_assert_cmpfloat (value.Z, <, 0.813050f + 0.01);

	cd_spectrum_free (unity);
	g_object_unref (cmf);
	g_object_unref (spectra);
}

static void
colord_spectrum_planckian_func (void)
{
	CdSpectrum *s;
	guint i;

	s = cd_spectrum_planckian_new (2940);

	g_assert_cmpstr (cd_spectrum_get_id (s), ==, "Planckian@2940K");
	g_assert_cmpfloat (ABS (cd_spectrum_get_start (s) - 300.f), <, 0.0001f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_end (s) - 830.f), <, 0.0001f);
	g_assert_cmpint (cd_spectrum_get_size (s), ==, 531);

	/* verify */
	for (i = 0; i < cd_spectrum_get_size (s); i++) {
		g_assert_cmpfloat (cd_spectrum_get_value (s, i), >, 1.f);
		g_assert_cmpfloat (cd_spectrum_get_value (s, i), <, 241.f);
	}

	cd_spectrum_free (s);
}

static void
colord_spectrum_func (void)
{
	CdSpectrum *s;
	gdouble val;

	s = cd_spectrum_new ();
	g_assert_cmpfloat (cd_spectrum_get_start (s), <, 0.0001f);
	g_assert_cmpfloat (cd_spectrum_get_end (s), <, 0.0001f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_norm (s) - 1.f), <, 0.0001f);
	g_assert_cmpint (cd_spectrum_get_size (s), ==, 0);
	g_assert_cmpstr (cd_spectrum_get_id (s), ==, NULL);

	cd_spectrum_set_start (s, 100.f);
	cd_spectrum_set_end (s, 200.f);
	cd_spectrum_set_end (s, 300.f);
	cd_spectrum_set_id (s, "dave");
	cd_spectrum_add_value (s, 0.50f);
	cd_spectrum_add_value (s, 0.75f);
	cd_spectrum_add_value (s, 1.00f);

	g_assert_cmpfloat (ABS (cd_spectrum_get_start (s) - 100.f), <, 0.0001f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_end (s) - 300.f), <, 0.0001f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_value (s, 0) - 0.50f), <, 0.0001f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_value (s, 1) - 0.75f), <, 0.0001f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_wavelength (s, 0) - 100.f), <, 0.0001f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_wavelength (s, 1) - 200.f), <, 0.0001f);
	g_assert_cmpint (cd_spectrum_get_size (s), ==, 3);
	g_assert_cmpstr (cd_spectrum_get_id (s), ==, "dave");

	/* test interpolation */
	val = cd_spectrum_get_value_for_nm (s, 100.1f);
	g_assert_cmpfloat (ABS (val - 0.50f), <, 0.001f);
	val = cd_spectrum_get_value_for_nm (s, 199.9f);
	g_assert_cmpfloat (ABS (val - 0.75f), <, 0.001f);
	val = cd_spectrum_get_value_for_nm (s, 150.f);
	g_assert_cmpfloat (ABS (val - 0.625f), <, 0.001f);

	/* test out of bounds */
	g_assert_cmpfloat (ABS (cd_spectrum_get_value_for_nm (s, 50) - 0.5), <, 0.0001f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_value_for_nm (s, 350) - 1.0f), <, 0.0001f);

	/* test normalisation */
	cd_spectrum_normalize (s, 200, 1.f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_value (s, 0) - 0.666f), <, 0.001f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_value (s, 1) - 1.000f), <, 0.001f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_value (s, 2) - 1.333f), <, 0.001f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_norm (s) - 1.333f), <, 0.001f);

	cd_spectrum_free (s);
}

static void
colord_it8_spect_func (void)
{
	CdIt8 *it8;
	CdSpectrum *spectrum;
	GError *error = NULL;
	GFile *file;
	GPtrArray *spectral_data;
	gboolean ret;
	gchar *data;
	gchar *filename;

	/* load in file */
	filename = cd_test_get_filename ("test.sp");
	file = g_file_new_for_path (filename);
	it8 = cd_it8_new ();
	ret = cd_it8_load_from_file (it8, file, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpint (cd_it8_get_kind (it8), ==, CD_IT8_KIND_SPECT);

	/* check data */
	spectral_data = cd_it8_get_spectrum_array (it8);
	g_assert_cmpint (spectral_data->len, ==, 1);
	spectrum = g_ptr_array_index (spectral_data, 0);
	g_assert_cmpfloat (ABS (cd_spectrum_get_start (spectrum) - 350.f), <, 0.001f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_end (spectrum) - 740.f), <, 0.001f);
	g_assert_cmpint (cd_spectrum_get_size (spectrum), ==, 2);
	g_assert_cmpfloat (ABS (cd_spectrum_get_value (spectrum, 0) - 0.01f), <, 0.01f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_value (spectrum, 1) - 1.00f), <, 0.01f);
	g_ptr_array_unref (spectral_data);

	/* save to file */
	ret = cd_it8_save_to_data (it8, &data, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert (data != NULL);
	g_free (data);

	g_free (filename);
	g_object_unref (it8);
	g_object_unref (file);
}

static void
colord_it8_ccss_func (void)
{
	CdIt8 *it8;
	CdSpectrum *spectrum;
	GError *error = NULL;
	GFile *file;
	GPtrArray *spectral_data;
	gboolean ret;
	gchar *data;
	gchar *filename;

	/* load in file */
	filename = cd_test_get_filename ("test.ccss");
	file = g_file_new_for_path (filename);
	it8 = cd_it8_new ();
	ret = cd_it8_load_from_file (it8, file, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpint (cd_it8_get_kind (it8), ==, CD_IT8_KIND_CCSS);
	g_assert_cmpstr (cd_it8_get_originator (it8), ==, "cd-self-test");
	g_assert_cmpstr (cd_it8_get_title (it8), ==, "test display model");
	g_assert (!cd_it8_has_option (it8, "DISPLAY_TYPE_REFRESH"));

	/* check data */
	spectral_data = cd_it8_get_spectrum_array (it8);
	g_assert_cmpint (spectral_data->len, ==, 2);
	spectrum = g_ptr_array_index (spectral_data, 0);
	g_assert_cmpfloat (ABS (cd_spectrum_get_start (spectrum) - 350.f), <, 0.001f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_end (spectrum) - 740.f), <, 0.001f);
	g_assert_cmpint (cd_spectrum_get_size (spectrum), ==, 118);
	g_assert_cmpfloat (ABS (cd_spectrum_get_value (spectrum, 0) - 0.01f), <, 0.01f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_value (spectrum, 117) - 1.00f), <, 0.01f);
	spectrum = cd_it8_get_spectrum_by_id (it8, "2");
	g_assert_cmpint (cd_spectrum_get_size (spectrum), ==, 118);
	g_assert_cmpfloat (ABS (cd_spectrum_get_value (spectrum, 0) - 0.99f), <, 0.01f);
	g_assert_cmpfloat (ABS (cd_spectrum_get_value (spectrum, 117) - 0.00f), <, 0.01f);
	g_ptr_array_unref (spectral_data);

	/* save to file */
	ret = cd_it8_save_to_data (it8, &data, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert (data != NULL);
	g_free (data);

	g_free (filename);
	g_object_unref (it8);
	g_object_unref (file);
}

static void
colord_it8_raw_func (void)
{
	CdColorRGB rgb;
	CdColorXYZ xyz;
	CdIt8 *it8;
	gboolean ret;
	gchar *data;
	gchar *filename;
	GError *error = NULL;
	GFile *file;
	GFile *file_new;
	gsize data_len;

	it8 = cd_it8_new ();
	g_assert (it8 != NULL);

	/* load in file */
	filename = cd_test_get_filename ("raw.ti3");
	file = g_file_new_for_path (filename);
	ret = cd_it8_load_from_file (it8, file, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* write this to raw data */
	ret = cd_it8_save_to_data (it8, &data, &data_len, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert (g_str_has_prefix (data, "CTI3"));
	g_assert_cmpint (data_len, ==, strlen (data));
	g_assert (data[data_len - 1] != '\0');
	g_free (data);

	/* write this to a new file */
	file_new = g_file_new_for_path ("/tmp/test.ti3");
	ret = cd_it8_save_to_file (it8, file_new, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* load in file again to ensure we save all the required data */
	ret = cd_it8_load_from_file (it8, file_new, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* test values */
	g_assert_cmpint (cd_it8_get_kind (it8), ==, CD_IT8_KIND_TI3);
	g_assert_cmpint (cd_it8_get_data_size (it8), ==, 5);
	g_assert (!cd_it8_get_normalized (it8));
	g_assert_cmpstr (cd_it8_get_originator (it8), ==, "cd-self-test");
	g_assert (!cd_it8_get_spectral (it8));
	g_assert_cmpstr (cd_it8_get_instrument (it8), ==, "huey");
	ret = cd_it8_get_data_item (it8, 1, &rgb, &xyz);
	g_assert (ret);
	g_assert_cmpfloat (ABS (rgb.R - 1.0f), <, 0.01f);
	g_assert_cmpfloat (ABS (rgb.G - 1.0f), <, 0.01f);
	g_assert_cmpfloat (ABS (rgb.B - 1.0f), <, 0.01f);
	g_assert_cmpfloat (ABS (xyz.X - 145.46f), <, 0.01f);
	g_assert_cmpfloat (ABS (xyz.Y - 99.88f), <, 0.01f);
	g_assert_cmpfloat (ABS (xyz.Z - 116.59f), <, 0.01f);

	/* remove temp file */
	ret = g_file_delete (file_new, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_free (filename);
	g_object_unref (it8);
	g_object_unref (file);
	g_object_unref (file_new);
}

static void
colord_it8_locale_func (void)
{
	CdIt8 *ccmx;
	CdMat3x3 mat;
	const gchar *orig_locale;
	gboolean ret;
	gchar *data;
	GError *error = NULL;

	/* set to a locale with ',' as the decimal point */
	orig_locale = setlocale (LC_NUMERIC, NULL);
	setlocale (LC_NUMERIC, "nl_BE.UTF-8");

	ccmx = cd_it8_new_with_kind (CD_IT8_KIND_CCMX);
	cd_mat33_clear (&mat);
	mat.m00 = 1.234;
	cd_it8_set_matrix (ccmx, &mat);
	cd_it8_set_enable_created (ccmx, FALSE);
	ret = cd_it8_save_to_data (ccmx, &data, NULL, &error);

	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (data, ==, "CCMX   \n"
				   "DESCRIPTOR	\"Device Correction Matrix\"\n"
				   "COLOR_REP	\"XYZ\"\n"
				   "NUMBER_OF_FIELDS	3\n"
				   "NUMBER_OF_SETS	3\n"
				   "BEGIN_DATA_FORMAT\n"
				   " XYZ_X	XYZ_Y	XYZ_Z\n"
				   "END_DATA_FORMAT\n"
				   "BEGIN_DATA\n"
				   " 1.234	0.0	0.0\n"
				   " 0.0	0.0	0.0\n"
				   " 0.0	0.0	0.0\n"
				   "END_DATA\n");
	setlocale (LC_NUMERIC, orig_locale);

	g_free (data);
	g_object_unref (ccmx);
}

static void
colord_it8_normalized_func (void)
{
	CdColorRGB rgb;
	CdColorXYZ xyz;
	CdIt8 *it8;
	gboolean ret;
	gchar *filename;
	GError *error = NULL;
	GFile *file;
	GFile *file_new;

	it8 = cd_it8_new ();
	g_assert (it8 != NULL);

	/* load in file */
	filename = cd_test_get_filename ("normalised.ti3");
	file = g_file_new_for_path (filename);
	ret = cd_it8_load_from_file (it8, file, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* write this to a new file */
	file_new = g_file_new_for_path ("/tmp/test.ti3");
	ret = cd_it8_save_to_file (it8, file_new, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* load in file again to ensure we save all the required data */
	ret = cd_it8_load_from_file (it8, file_new, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* test values */
	g_assert_cmpint (cd_it8_get_data_size (it8), ==, 2);
	g_assert (!cd_it8_get_normalized (it8));
	g_assert_cmpstr (cd_it8_get_originator (it8), ==, NULL);
	g_assert (!cd_it8_get_spectral (it8));
	g_assert_cmpstr (cd_it8_get_instrument (it8), ==, NULL);
	ret = cd_it8_get_data_item (it8, 1, &rgb, &xyz);
	g_assert (ret);
	g_assert_cmpfloat (ABS (rgb.R - 1.0f), <, 0.01f);
	g_assert_cmpfloat (ABS (rgb.G - 1.0f), <, 0.01f);
	g_assert_cmpfloat (ABS (rgb.B - 1.0f), <, 0.01f);
	g_assert_cmpfloat (ABS (xyz.X - 90.21f), <, 0.01f);
	g_assert_cmpfloat (ABS (xyz.Y - 41.22f), <, 0.01f);
	g_assert_cmpfloat (ABS (xyz.Z - 56.16f), <, 0.01f);

	/* remove temp file */
	ret = g_file_delete (file_new, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_free (filename);
	g_object_unref (it8);
	g_object_unref (file);
	g_object_unref (file_new);
}

static void
colord_it8_ccmx_util_func (void)
{
	CdIt8 *ccmx;
	CdIt8 *meas;
	CdIt8 *ref;
	gboolean ret;
	gchar *filename;
	GError *error = NULL;
	GFile *file;

	/* load reference */
	filename = cd_test_get_filename ("reference.ti3");
	file = g_file_new_for_path (filename);
	ref = cd_it8_new ();
	ret = cd_it8_load_from_file (ref, file, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);

	/* load measured */
	filename = cd_test_get_filename ("measured.ti3");
	file = g_file_new_for_path (filename);
	meas = cd_it8_new ();
	ret = cd_it8_load_from_file (meas, file, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);

	/* calculate CCMX */
	ccmx = cd_it8_new_with_kind (CD_IT8_KIND_CCMX);
	ret = cd_it8_utils_calculate_ccmx (ref, meas, ccmx, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_object_unref (ref);
	g_object_unref (meas);
	g_object_unref (ccmx);
}

static void
colord_it8_ccmx_func (void)
{
	CdIt8 *it8;
	const CdMat3x3 *matrix;
	gboolean ret;
	gchar *filename;
	GError *error = NULL;
	GFile *file;
	GFile *file_new;

	it8 = cd_it8_new ();
	g_assert (it8 != NULL);

	/* load in file */
	filename = cd_test_get_filename ("calibration.ccmx");
	file = g_file_new_for_path (filename);
	ret = cd_it8_load_from_file (it8, file, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* write this to a new file */
	file_new = g_file_new_for_path ("/tmp/test.ccmx");
	ret = cd_it8_save_to_file (it8, file_new, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* load in file again to ensure we save all the required data */
	ret = cd_it8_load_from_file (it8, file_new, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* test values */
	g_assert_cmpint (cd_it8_get_data_size (it8), ==, 0);
	g_assert_cmpstr (cd_it8_get_originator (it8), ==, "cd-self-test");
	g_assert_cmpstr (cd_it8_get_title (it8), ==, "Factory Calibration");
	g_assert (!cd_it8_get_spectral (it8));
	g_assert (cd_it8_has_option (it8, "TYPE_FACTORY"));
	g_assert (!cd_it8_has_option (it8, "TYPE_XXXXXXX"));
	g_assert_cmpstr (cd_it8_get_instrument (it8), ==, "Huey");
	matrix = cd_it8_get_matrix (it8);
	g_assert_cmpfloat (ABS (matrix->m00 - 1.3139f), <, 0.01f);
	g_assert_cmpfloat (ABS (matrix->m01 - 0.21794f), <, 0.01f);
	g_assert_cmpfloat (ABS (matrix->m02 - 0.89224f), <, 0.01f);

	/* remove temp file */
	ret = g_file_delete (file_new, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_free (filename);
	g_object_unref (it8);
	g_object_unref (file);
	g_object_unref (file_new);
}

static void
colord_enum_func (void)
{
	const gchar *tmp;
	guint enum_tmp;
	guint i;

	/* CdSensorError */
	for (i = 0; i < CD_SENSOR_ERROR_LAST; i++) {
		tmp = cd_sensor_error_to_string (i);
		g_assert_cmpstr (tmp, !=, NULL);
		enum_tmp = cd_sensor_error_from_string (tmp);
		g_assert_cmpint (enum_tmp, !=, CD_SENSOR_ERROR_LAST);
	}

	/* CdProfileError */
	for (i = 0; i < CD_PROFILE_ERROR_LAST; i++) {
		tmp = cd_profile_error_to_string (i);
		g_assert_cmpstr (tmp, !=, NULL);
		enum_tmp = cd_profile_error_from_string (tmp);
		g_assert_cmpint (enum_tmp, !=, CD_PROFILE_ERROR_LAST);
	}

	/* CdDeviceError */
	for (i = 0; i < CD_DEVICE_ERROR_LAST; i++) {
		tmp = cd_device_error_to_string (i);
		g_assert_cmpstr (tmp, !=, NULL);
		enum_tmp = cd_device_error_from_string (tmp);
		g_assert_cmpint (enum_tmp, !=, CD_DEVICE_ERROR_LAST);
	}

	/* CdClientError */
	for (i = 0; i < CD_CLIENT_ERROR_LAST; i++) {
		tmp = cd_client_error_to_string (i);
		g_assert_cmpstr (tmp, !=, NULL);
		enum_tmp = cd_client_error_from_string (tmp);
		g_assert_cmpint (enum_tmp, !=, CD_CLIENT_ERROR_LAST);
	}

	/* CdSensorKind */
	for (i = CD_SENSOR_KIND_UNKNOWN + 1; i < CD_SENSOR_KIND_LAST; i++) {
		tmp = cd_sensor_kind_to_string (i);
		if (g_strcmp0 (tmp, "unknown") == 0)
			g_warning ("no enum for %i", i);
		enum_tmp = cd_sensor_kind_from_string (tmp);
		if (enum_tmp == CD_SENSOR_KIND_UNKNOWN)
			g_warning ("no enum for %s", tmp);
		g_assert_cmpint (enum_tmp, ==, i);
	}

	/* CdDeviceKind */
	for (i = CD_DEVICE_KIND_UNKNOWN + 1; i < CD_DEVICE_KIND_LAST; i++) {
		tmp = cd_device_kind_to_string (i);
		if (g_strcmp0 (tmp, "unknown") == 0)
			g_warning ("no enum for %i", i);
		enum_tmp = cd_device_kind_from_string (tmp);
		if (enum_tmp == CD_DEVICE_KIND_UNKNOWN)
			g_warning ("no enum for %s", tmp);
		g_assert_cmpint (enum_tmp, ==, i);
	}

	/* CdProfileKind */
	for (i = CD_PROFILE_KIND_UNKNOWN + 1; i < CD_PROFILE_KIND_LAST; i++) {
		tmp = cd_profile_kind_to_string (i);
		if (g_strcmp0 (tmp, "unknown") == 0)
			g_warning ("no enum for %i", i);
		enum_tmp = cd_profile_kind_from_string (tmp);
		if (enum_tmp == CD_PROFILE_KIND_UNKNOWN)
			g_warning ("no enum for %s", tmp);
		g_assert_cmpint (enum_tmp, ==, i);
	}

	/* CdRenderingIntent */
	for (i = CD_RENDERING_INTENT_UNKNOWN + 1; i < CD_RENDERING_INTENT_LAST; i++) {
		tmp = cd_rendering_intent_to_string (i);
		if (g_strcmp0 (tmp, "unknown") == 0)
			g_warning ("no enum for %i", i);
		enum_tmp = cd_rendering_intent_from_string (tmp);
		if (enum_tmp == CD_RENDERING_INTENT_UNKNOWN)
			g_warning ("no enum for %s", tmp);
		g_assert_cmpint (enum_tmp, ==, i);
	}

	/* CdColorSpace */
	for (i = CD_COLORSPACE_UNKNOWN + 1; i < CD_COLORSPACE_LAST; i++) {
		tmp = cd_colorspace_to_string (i);
		if (g_strcmp0 (tmp, "unknown") == 0)
			g_warning ("no enum for %i", i);
		enum_tmp = cd_colorspace_from_string (tmp);
		if (enum_tmp == CD_COLORSPACE_UNKNOWN)
			g_warning ("no enum for %s", tmp);
		g_assert_cmpint (enum_tmp, ==, i);
	}

	/* CdDeviceRelation */
	for (i = CD_DEVICE_RELATION_UNKNOWN + 1; i < CD_DEVICE_RELATION_LAST; i++) {
		tmp = cd_device_relation_to_string (i);
		if (g_strcmp0 (tmp, "unknown") == 0)
			g_warning ("no enum for %i", i);
		enum_tmp = cd_device_relation_from_string (tmp);
		if (enum_tmp == CD_DEVICE_RELATION_UNKNOWN)
			g_warning ("no enum for %s", tmp);
		g_assert_cmpint (enum_tmp, ==, i);
	}

	/* CdObjectScope */
	for (i = CD_OBJECT_SCOPE_UNKNOWN + 1; i < CD_OBJECT_SCOPE_LAST; i++) {
		tmp = cd_object_scope_to_string (i);
		if (g_strcmp0 (tmp, "unknown") == 0)
			g_warning ("no enum for %i", i);
		enum_tmp = cd_object_scope_from_string (tmp);
		if (enum_tmp == CD_OBJECT_SCOPE_UNKNOWN)
			g_warning ("no enum for %s", tmp);
		g_assert_cmpint (enum_tmp, ==, i);
	}

	/* CdSensorState */
	for (i = CD_SENSOR_STATE_UNKNOWN + 1; i < CD_SENSOR_STATE_LAST; i++) {
		tmp = cd_sensor_state_to_string (i);
		if (g_strcmp0 (tmp, "unknown") == 0)
			g_warning ("no enum for %i", i);
		enum_tmp = cd_sensor_state_from_string (tmp);
		if (enum_tmp == CD_SENSOR_STATE_UNKNOWN)
			g_warning ("no enum for %s", tmp);
		g_assert_cmpint (enum_tmp, ==, i);
	}

	/* CdSensorCap */
	for (i = CD_SENSOR_CAP_UNKNOWN + 1; i < CD_SENSOR_CAP_LAST; i++) {
		tmp = cd_sensor_cap_to_string (i);
		if (g_strcmp0 (tmp, "unknown") == 0)
			g_warning ("no enum for %i", i);
		enum_tmp = cd_sensor_cap_from_string (tmp);
		if (enum_tmp == CD_SENSOR_CAP_UNKNOWN)
			g_warning ("no enum for %s", tmp);
		g_assert_cmpint (enum_tmp, ==, i);
	}

	/* CdSensorCap */
	for (i = CD_STANDARD_SPACE_UNKNOWN + 1; i < CD_STANDARD_SPACE_LAST; i++) {
		tmp = cd_standard_space_to_string (i);
		if (g_strcmp0 (tmp, "unknown") == 0)
			g_warning ("no enum for %i", i);
		enum_tmp = cd_standard_space_from_string (tmp);
		if (enum_tmp == CD_STANDARD_SPACE_UNKNOWN)
			g_warning ("no enum for %s", tmp);
		g_assert_cmpint (enum_tmp, ==, i);
	}
#if 0
	/* CdProfileWarning */
	for (i = CD_PROFILE_WARNING_UNKNOWN + 1; i < CD_PROFILE_WARNING_LAST; i++) {
		tmp = cd_standard_space_to_string (i);
		if (g_strcmp0 (tmp, "unknown") == 0)
			g_warning ("no enum for %i", i);
		enum_tmp = cd_standard_space_from_string (tmp);
		if (enum_tmp == CD_PROFILE_WARNING_UNKNOWN)
			g_warning ("no enum for %s", tmp);
		g_assert_cmpint (enum_tmp, ==, i);
	}

	/* CdProfileQuality */
	for (i = CD_PROFILE_QUALITY_UNKNOWN + 1; i < CD_PROFILE_QUALITY_LAST; i++) {
		tmp = cd_profile_quality_to_string (i);
		if (g_strcmp0 (tmp, "unknown") == 0)
			g_warning ("no enum for %i", i);
		enum_tmp = cd_profile_quality_from_string (tmp);
		if (enum_tmp == CD_PROFILE_QUALITY_UNKNOWN)
			g_warning ("no enum for %s", tmp);
		g_assert_cmpint (enum_tmp, ==, i);
	}
#endif
}

static void
colord_dom_func (void)
{
	CdDom *dom;
	const gchar *markup = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?><html> <body> <p class='1'>moo1</p> <p wrap='false'>moo2</p>\n</body> </html>";
	const GNode *tmp;
	gboolean ret;
	gchar *str;
	GError *error = NULL;

	dom = cd_dom_new ();

	/* parse */
	ret = cd_dom_parse_xml_data (dom, markup, -1, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* to string */
	str = cd_dom_to_string (dom);
	g_assert_cmpstr (str, ==, "  <html> []\n   <body> []\n    <p> [moo1]\n    <p> [moo2]\n");
	g_free (str);

	/* get node */
	tmp = cd_dom_get_node (dom, NULL, "html/body");
	g_assert (tmp != NULL);
	g_assert_cmpstr (cd_dom_get_node_name (tmp), ==, "body");

	/* get children */
	tmp = tmp->children;
	g_assert_cmpstr (cd_dom_get_node_name (tmp), ==, "p");
	g_assert_cmpstr (cd_dom_get_node_data (tmp), ==, "moo1");
	g_assert_cmpstr (cd_dom_get_node_attribute (tmp, "class"), ==, "1");

	tmp = tmp->next;
	g_assert_cmpstr (cd_dom_get_node_name (tmp), ==, "p");
	g_assert_cmpstr (cd_dom_get_node_data (tmp), ==, "moo2");
	g_assert_cmpstr (cd_dom_get_node_attribute (tmp, "wrap"), ==, "false");

	g_object_unref (dom);
}

static void
colord_dom_color_func (void)
{
	CdColorLab lab;
	CdColorRGB rgb;
	CdDom *dom;
	const gchar *markup = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
		"<named>"
		" <color>"
		"  <name>Dave</name>"
		"  <L>12.34</L>"
		"  <a>0.56</a>"
		"  <b>0.78</b>"
		" </color>"
		"</named>";
	const GNode *tmp;
	gboolean ret;
	GError *error = NULL;

	dom = cd_dom_new ();

	/* parse */
	ret = cd_dom_parse_xml_data (dom, markup, -1, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get node */
	tmp = cd_dom_get_node (dom, NULL, "named/color");
	g_assert (tmp != NULL);

	/* get value */
	ret = cd_dom_get_node_lab (tmp, &lab);
	g_assert (ret);
	g_debug ("Lab = %f, %f, %f", lab.L, lab.a, lab.b);

	/* get value */
	ret = cd_dom_get_node_rgb (tmp, &rgb);
	g_assert (!ret);

	g_object_unref (dom);
}

static void
colord_dom_localized_func (void)
{
	CdDom *dom;
	const gchar *markup = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
		"<profile>"
		" <copyright>Colors cannot be copyrighted</copyright>"
		" <copyright xml:lang=\"en_GB\">Colours cannot be copyrighted</copyright>"
		"</profile>";
	const gchar *lang;
	const GNode *tmp;
	gboolean ret;
	GError *error = NULL;
	GHashTable *hash;

	dom = cd_dom_new ();

	/* parse */
	ret = cd_dom_parse_xml_data (dom, markup, -1, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get node */
	tmp = cd_dom_get_node (dom, NULL, "profile");
	g_assert (tmp != NULL);

	hash = cd_dom_get_node_localized (tmp, "copyright");
	g_assert (hash != NULL);
	lang = g_hash_table_lookup (hash, "");
	g_assert_cmpstr (lang, ==, "Colors cannot be copyrighted");
	lang = g_hash_table_lookup (hash, "en_GB");
	g_assert_cmpstr (lang, ==, "Colours cannot be copyrighted");
	lang = g_hash_table_lookup (hash, "fr");
	g_assert_cmpstr (lang, ==, NULL);
	g_hash_table_unref (hash);

	g_object_unref (dom);
}

static void
colord_color_func (void)
{
	CdColorUVW uvw;
	CdColorXYZ *xyz;
	CdColorXYZ xyz_src;
	CdColorYxy yxy;

	xyz = cd_color_xyz_new ();
	g_assert (xyz != NULL);

	/* nothing set */
	cd_color_xyz_to_yxy (xyz, &yxy);
	g_assert_cmpfloat (fabs (yxy.x - 0.0f), <, 0.001f);

	/* set dummy values */
	cd_color_xyz_set (xyz, 0.125, 0.25, 0.5);
	cd_color_xyz_to_yxy (xyz, &yxy);

	g_assert_cmpfloat (fabs (yxy.x - 0.142857143f), <, 0.001f);
	g_assert_cmpfloat (fabs (yxy.y - 0.285714286f), <, 0.001f);

	/* Planckian locus */
	cd_color_uvw_set_planckian_locus (&uvw, 4000);
	g_assert_cmpfloat (ABS (uvw.U - 0.225), <, 0.01);
	g_assert_cmpfloat (ABS (uvw.V - 0.334), <, 0.01);
	g_assert_cmpfloat (ABS (uvw.W - 1.000), <, 0.01);

	/* normalizing */
	cd_color_xyz_set (&xyz_src, 100, 50, 25);
	cd_color_xyz_normalize (&xyz_src, 1.0, xyz);
	g_assert_cmpfloat (ABS (xyz->X - 2.0), <, 0.01);
	g_assert_cmpfloat (ABS (xyz->Y - 1.0), <, 0.01);
	g_assert_cmpfloat (ABS (xyz->Z - 0.5), <, 0.01);

	cd_color_xyz_free (xyz);
}


static void
cd_test_math_func (void)
{
	CdMat3x3 mat;
	CdMat3x3 matsrc;

	/* matrix */
	mat.m00 = 1.00f;
	cd_mat33_clear (&mat);
	g_assert_cmpfloat (mat.m00, <, 0.001f);
	g_assert_cmpfloat (mat.m00, >, -0.001f);
	g_assert_cmpfloat (mat.m22, <, 0.001f);
	g_assert_cmpfloat (mat.m22, >, -0.001f);

	/* multiply two matrices */
	cd_mat33_clear (&matsrc);
	matsrc.m01 = matsrc.m10 = 2.0f;
	cd_mat33_matrix_multiply (&matsrc, &matsrc, &mat);
	g_assert_cmpfloat (mat.m00, <, 4.1f);
	g_assert_cmpfloat (mat.m00, >, 3.9f);
	g_assert_cmpfloat (mat.m11, <, 4.1f);
	g_assert_cmpfloat (mat.m11, >, 3.9f);
	g_assert_cmpfloat (mat.m22, <, 0.001f);
	g_assert_cmpfloat (mat.m22, >, -0.001f);
}

static void
colord_color_interpolate_func (void)
{
	GPtrArray *array;
	GPtrArray *result;
	guint i;
	CdColorRGB *rgb;
	gdouble test_data[] = { 0.10, 0.35, 0.40, 0.80, 1.00, -1.0 };

	/* interpolate with values that intentionally trip up Akima */
	array = cd_color_rgb_array_new ();
	for (i = 0; test_data[i] >= 0.0; i++) {
		rgb = cd_color_rgb_new ();
		cd_color_rgb_set (rgb,
				  test_data[i],
				  test_data[i] + 0.1,
				  test_data[i] + 0.2);
		g_ptr_array_add (array, rgb);
	}
	result = cd_color_rgb_array_interpolate (array, 10);
	g_assert (result != NULL);
	g_assert_cmpint (result->len, ==, 10);

	g_ptr_array_unref (result);
	g_ptr_array_unref (array);
}

static void
colord_color_blackbody_func (void)
{
	CdColorRGB rgb;
	gboolean ret;

	/* D65 */
	ret = cd_color_get_blackbody_rgb (6500, &rgb);
	g_assert (ret);
	g_assert_cmpfloat (fabs (rgb.R - 1.0000f), <, 0.01);
	g_assert_cmpfloat (fabs (rgb.G - 1.0000f), <, 0.01);
	g_assert_cmpfloat (fabs (rgb.B - 1.0000f), <, 0.01);

	/* 1000K */
	ret = cd_color_get_blackbody_rgb (1000, &rgb);
	g_assert (ret);
	g_assert_cmpfloat (fabs (rgb.R - 1.0000f), <, 0.01);
	g_assert_cmpfloat (fabs (rgb.G - 0.0425f), <, 0.01);
	g_assert_cmpfloat (fabs (rgb.B - 0.0000f), <, 0.01);

	/* 10000K */
	ret = cd_color_get_blackbody_rgb (10000, &rgb);
	g_assert (ret);
	g_assert_cmpfloat (fabs (rgb.R - 0.5944f), <, 0.01);
	g_assert_cmpfloat (fabs (rgb.G - 0.7414f), <, 0.01);
	g_assert_cmpfloat (fabs (rgb.B - 1.0000f), <, 0.01);

	/* 90K */
	ret = cd_color_get_blackbody_rgb (90, &rgb);
	g_assert (!ret);
	g_assert_cmpfloat (fabs (rgb.R - 1.0000f), <, 0.01);
	g_assert_cmpfloat (fabs (rgb.G - 0.0425f), <, 0.01);
	g_assert_cmpfloat (fabs (rgb.B - 0.0000f), <, 0.01);

	/* 100000K */
	ret = cd_color_get_blackbody_rgb (100000, &rgb);
	g_assert (!ret);
	g_assert_cmpfloat (fabs (rgb.R - 0.5944f), <, 0.01);
	g_assert_cmpfloat (fabs (rgb.G - 0.7414f), <, 0.01);
	g_assert_cmpfloat (fabs (rgb.B - 1.0000f), <, 0.01);
}

static void
colord_interp_linear_func (void)
{
	CdInterp *interp;
	GArray *array_tmp;
	gboolean ret;
	gdouble tmp;
	gdouble x;
	gdouble y;
	GError *error = NULL;
	guint i;
	guint new_length = 10;
	const gdouble data[] = { 0.100000, 0.211111, 0.322222, 0.366667,
				 0.388889, 0.488889, 0.666667, 0.822222,
				 0.911111, 1.000000 };

	/* check name */
	interp = cd_interp_linear_new ();
	g_assert_cmpint (cd_interp_get_kind (interp), ==, CD_INTERP_KIND_LINEAR);
	g_assert_cmpstr (cd_interp_kind_to_string (CD_INTERP_KIND_LINEAR), ==, "linear");

	/* insert some data */
	cd_interp_insert (interp, 0.00, 0.10);
	cd_interp_insert (interp, 0.25, 0.35);
	cd_interp_insert (interp, 0.50, 0.40);
	cd_interp_insert (interp, 0.75, 0.80);
	cd_interp_insert (interp, 1.00, 1.00);

	/* check X */
	array_tmp = cd_interp_get_x (interp);
	g_assert_cmpint (array_tmp->len, ==, 5);
	tmp = g_array_index (array_tmp, gdouble, 0);
	g_assert_cmpfloat (tmp, <, 0.01);
	g_assert_cmpfloat (tmp, >, -0.01);

	/* check Y */
	array_tmp = cd_interp_get_y (interp);
	g_assert_cmpint (array_tmp->len, ==, 5);
	tmp = g_array_index (array_tmp, gdouble, 0);
	g_assert_cmpfloat (tmp, <, 0.11);
	g_assert_cmpfloat (tmp, >, 0.09);

	/* check preparing */
	ret = cd_interp_prepare (interp, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (cd_interp_get_size (interp), ==, 5);

	/* check values */
	for (i = 0; i < new_length; i++) {
		x = (gdouble) i / (gdouble) (new_length - 1);
		y = cd_interp_eval (interp, x, &error);
		g_assert_no_error (error);
		g_assert_cmpfloat (y, <, data[i] + 0.01);
	}
	g_object_unref (interp);
}

static void
colord_interp_akima_func (void)
{
	CdInterp *interp;
	gboolean ret;
	gdouble x;
	gdouble y;
	GError *error = NULL;
	guint i;
	guint new_length = 10;
	const gdouble data[] = { 0.100000, 0.232810, 0.329704, 0.372559,
				 0.370252, 0.470252, 0.672559, 0.829704,
				 0.932810, 1.000000 };

	/* check name */
	interp = cd_interp_akima_new ();
	g_assert_cmpint (cd_interp_get_kind (interp), ==, CD_INTERP_KIND_AKIMA);
	g_assert_cmpstr (cd_interp_kind_to_string (cd_interp_get_kind (interp)), ==, "akima");

	/* insert some data */
	cd_interp_insert (interp, 0.00, 0.10);
	cd_interp_insert (interp, 0.25, 0.35);
	cd_interp_insert (interp, 0.50, 0.40);
	cd_interp_insert (interp, 0.75, 0.80);
	cd_interp_insert (interp, 1.00, 1.00);

	/* prepare */
	ret = cd_interp_prepare (interp, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check values */
	for (i = 0; i < new_length; i++) {
		x = (gdouble) i / (gdouble) (new_length - 1);
		y = cd_interp_eval (interp, x, &error);
		g_assert_no_error (error);
		g_assert_cmpfloat (y, <, data[i] + 0.01);
		g_assert_cmpfloat (y, >, data[i] - 0.01);
	}

	g_object_unref (interp);
}

static void
colord_buffer_func (void)
{
	guint8 buffer[4];

	cd_buffer_write_uint16_be (buffer, 255);
	g_assert_cmpint (buffer[0], ==, 0x00);
	g_assert_cmpint (buffer[1], ==, 0xff);
	g_assert_cmpint (cd_buffer_read_uint16_be (buffer), ==, 255);

	cd_buffer_write_uint16_le (buffer, 8192);
	g_assert_cmpint (buffer[0], ==, 0x00);
	g_assert_cmpint (buffer[1], ==, 0x20);
	g_assert_cmpint (cd_buffer_read_uint16_le (buffer), ==, 8192);
}

/* 1. create a valid profile with metadata and model and save it
 * 2. open profile, delete meta and dscm tags, and resave
 * 3. open profile and verify meta and dscm information is not present */
static void
colord_icc_clear_func (void)
{
	CdIcc *icc;
	GError *error = NULL;
	gboolean ret;
	GBytes *payload;
	const gchar *tmp;

	/* create a new file with an empty metadata store */
	icc = cd_icc_new ();
	ret = cd_icc_create_default (icc, &error);
	cd_icc_set_model (icc, NULL, "baz");
	g_assert_no_error (error);
	g_assert (ret);
	payload = cd_icc_save_data (icc, CD_ICC_SAVE_FLAGS_NONE, &error);
	g_assert_no_error (error);
	g_assert (payload != NULL);
	g_object_unref (icc);

	/* load payload, delete all meta and dscm tags, and resave */
	icc = cd_icc_new ();
	ret = cd_icc_load_data (icc,
				g_bytes_get_data (payload, NULL),
				g_bytes_get_size (payload),
				CD_ICC_LOAD_FLAGS_METADATA,
				&error);
	cd_icc_remove_metadata (icc, "DATA_source");
	cd_icc_remove_metadata (icc, "STANDARD_space");
	cd_icc_set_model (icc, NULL, NULL);
	g_assert_no_error (error);
	g_assert (ret);
	g_bytes_unref (payload);
	payload = cd_icc_save_data (icc, CD_ICC_SAVE_FLAGS_NONE, &error);
	g_assert_no_error (error);
	g_assert (payload != NULL);
	g_object_unref (icc);

	/* ensure values not set */
	icc = cd_icc_new ();
	ret = cd_icc_load_data (icc,
				g_bytes_get_data (payload, NULL),
				g_bytes_get_size (payload),
				CD_ICC_LOAD_FLAGS_METADATA,
				&error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (cd_icc_get_metadata_item (icc, "DATA_source"), ==, NULL);
	tmp = cd_icc_get_model (icc, NULL, &error);
	g_assert_error (error, CD_ICC_ERROR, CD_ICC_ERROR_NO_DATA);
	g_assert (tmp == NULL);

	g_bytes_unref (payload);
	g_object_unref (icc);
}

static void
colord_icc_func (void)
{
	CdIcc *icc;
	const CdColorRGB *rgb_tmp;
	const CdColorXYZ *xyz_tmp;
	const gchar *str;
	GArray *warnings;
	gboolean ret;
	gchar *created_str;
	gchar *filename;
	gchar *tmp;
	GDateTime *created;
	GError *error = NULL;
	GFile *file;
	GHashTable *metadata;
	gpointer handle;
	GPtrArray *array;

	/* test invalid */
	icc = cd_icc_new ();
	file = g_file_new_for_path ("not-going-to-exist.icc");
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_NONE,
				NULL,
				&error);
	g_assert_error (error, CD_ICC_ERROR, CD_ICC_ERROR_FAILED_TO_OPEN);
	g_assert (!ret);
	g_clear_error (&error);
	g_object_unref (file);

	/* test actual file */
	filename = cd_test_get_filename ("ibm-t61.icc");
	file = g_file_new_for_path (filename);
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_METADATA |
				 CD_ICC_LOAD_FLAGS_NAMED_COLORS |
				 CD_ICC_LOAD_FLAGS_PRIMARIES |
				 CD_ICC_LOAD_FLAGS_TRANSLATIONS,
				NULL,
				&error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);
	g_free (filename);

	/* get handle */
	handle = cd_icc_get_handle (icc);
	g_assert (handle != NULL);

	/* check VCGT */
	array = cd_icc_get_vcgt (icc, 256, &error);
	g_assert_no_error (error);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 256);
	rgb_tmp = g_ptr_array_index (array, 0);
	g_assert_cmpfloat (rgb_tmp->R, <, 0.02);
	g_assert_cmpfloat (rgb_tmp->G, <, 0.02);
	g_assert_cmpfloat (rgb_tmp->B, <, 0.02);
	rgb_tmp = g_ptr_array_index (array, 255);
	g_assert_cmpfloat (rgb_tmp->R, >, 0.98);
	g_assert_cmpfloat (rgb_tmp->G, >, 0.98);
	g_assert_cmpfloat (rgb_tmp->B, >, 0.08);
	g_ptr_array_unref (array);

	/* check profile properties */
	g_assert_cmpint (cd_icc_get_size (icc), ==, 25244);
	g_assert_cmpstr (cd_icc_get_checksum (icc), ==, "9ace8cce8baac8d492a93a2a232d7702");
	g_assert_cmpfloat (cd_icc_get_version (icc), ==, 3.4);
	g_assert (g_str_has_suffix (cd_icc_get_filename (icc), "ibm-t61.icc"));
	g_assert_cmpint (cd_icc_get_kind (icc), ==, CD_PROFILE_KIND_DISPLAY_DEVICE);
	g_assert_cmpint (cd_icc_get_colorspace (icc), ==, CD_COLORSPACE_RGB);
	array = cd_icc_get_named_colors (icc);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 0);
	g_ptr_array_unref (array);

	/* check profile primaries */
	xyz_tmp = cd_icc_get_red (icc);
	g_assert_cmpfloat (ABS (xyz_tmp->X - 0.405), <, 0.01);
	g_assert_cmpfloat (ABS (xyz_tmp->Y - 0.230), <, 0.01);
	g_assert_cmpfloat (ABS (xyz_tmp->Z - 0.031), <, 0.01);
	xyz_tmp = cd_icc_get_white (icc);
	g_assert_cmpfloat (ABS (xyz_tmp->X - 0.969), <, 0.01);
	g_assert_cmpfloat (ABS (xyz_tmp->Y - 1.000), <, 0.01);
	g_assert_cmpfloat (ABS (xyz_tmp->Z - 0.854), <, 0.01);
	g_assert_cmpint (cd_icc_get_temperature (icc), ==, 5000);

	/* check metadata */
	metadata = cd_icc_get_metadata (icc);
	g_assert_cmpint (g_hash_table_size (metadata), ==, 1);
	g_hash_table_unref (metadata);
	g_assert_cmpstr (cd_icc_get_metadata_item (icc, "EDID_md5"), ==, "f09e42aa86585d1bb6687d3c322ed0c1");

	/* check warnings */
	warnings = cd_icc_get_warnings (icc);
	g_assert_cmpint (warnings->len, ==, 0);
	g_array_unref (warnings);

	/* marshall to a string */
	tmp = cd_icc_to_string (icc);
	g_assert_cmpstr (tmp, !=, NULL);
	g_debug ("CdIcc: '%s'", tmp);
	g_free (tmp);

	/* check created time */
	created = cd_icc_get_created (icc);
	g_assert (created != NULL);
	created_str = g_date_time_format (created, "%F, %T");
	g_assert_cmpstr (created_str, ==, "2009-12-23, 22:20:46");
	g_free (created_str);
	g_date_time_unref (created);

	/* open a non-localized profile */
	str = cd_icc_get_description (icc, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (str, ==, "Huey, LENOVO - 6464Y1H - 15\" (2009-12-23)");
	str = cd_icc_get_description (icc, "en_GB", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (str, ==, "Huey, LENOVO - 6464Y1H - 15\" (2009-12-23)");
	str = cd_icc_get_description (icc, "fr", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (str, ==, "Huey, LENOVO - 6464Y1H - 15\" (2009-12-23)");

	g_object_unref (icc);
}

static void
colord_icc_edid_func (void)
{
	CdColorYxy blue;
	CdColorYxy green;
	CdColorYxy red;
	CdColorYxy white;
	CdIcc *icc;
	gboolean ret;
	GError *error = NULL;

	/* create a profile from the EDID data */
	icc = cd_icc_new ();
	cd_color_yxy_set (&red, 1.0f, 0.569336f, 0.332031f);
	cd_color_yxy_set (&green, 1.0f, 0.311523f, 0.543945f);
	cd_color_yxy_set (&blue, 1.0f, 0.149414f, 0.131836f);
	cd_color_yxy_set (&white, 1.0f, 0.313477f, 0.329102f);
	ret = cd_icc_create_from_edid (icc, 2.2f, &red, &green, &blue, &white, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpfloat (cd_icc_get_version (icc), >, 3.99);

	g_object_unref (icc);
}

static void
colord_icc_characterization_func (void)
{
	CdIcc *icc;
	const gchar *str;
	gchar *md5;
	gboolean ret;
	gchar *filename;
	GError *error = NULL;
	GFile *file;

	/* load source file */
	icc = cd_icc_new ();
	filename = cd_test_get_filename ("ibm-t61.icc");
	file = g_file_new_for_path (filename);
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_CHARACTERIZATION,
				NULL,
				&error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);
	g_free (filename);

	/* check original values */
	str = cd_icc_get_characterization_data (icc);
	md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, str, -1);
	g_assert_cmpstr (md5, ==, "79376a43578c5b1f7d428a62da916dab");
	g_free (md5);

	g_object_unref (icc);
}

static void
colord_icc_empty_func (void)
{
	CdIcc *icc;
	GError *error = NULL;
	GFile *file;
	gboolean ret;
	gchar *filename;

	/* load source file */
	icc = cd_icc_new ();
	filename = cd_test_get_filename ("empty.icc");
	file = g_file_new_for_path (filename);
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_NONE,
				NULL,
				&error);
	g_assert_error (error, CD_ICC_ERROR, CD_ICC_ERROR_FAILED_TO_PARSE);
	g_assert (!ret);
	g_error_free (error);
	g_free (filename);
	g_object_unref (file);
	g_object_unref (icc);
}

static void
colord_icc_corrupt_dict_func (void)
{
	CdIcc *icc;
	GError *error = NULL;
	gboolean ret;
	gchar *filename;
	int fd;

	/* load source file */
	icc = cd_icc_new ();
	filename = cd_test_get_filename ("corrupt-dict.icc");
	fd = g_open (filename, O_RDONLY, 0);
	ret = cd_icc_load_fd (icc,
			      fd,
			      CD_ICC_LOAD_FLAGS_METADATA,
			      &error);
	g_assert_error (error, CD_ICC_ERROR, CD_ICC_ERROR_CORRUPTION_DETECTED);
	g_assert (!ret);
	g_clear_error (&error);

	/* close fd */
	ret = g_close (fd, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_free (filename);
	g_object_unref (icc);
}

static void
colord_icc_save_func (void)
{
	CdIcc *icc;
	const gchar *str;
	gboolean ret;
	gchar *filename;
	GError *error = NULL;
	GFile *file;

	/* load source file */
	icc = cd_icc_new ();
	filename = cd_test_get_filename ("ibm-t61.icc");
	file = g_file_new_for_path (filename);
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_METADATA,
				NULL,
				&error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);
	g_free (filename);

	/* check original values */
	g_assert_cmpint (cd_icc_get_kind (icc), ==, CD_PROFILE_KIND_DISPLAY_DEVICE);
	g_assert_cmpint (cd_icc_get_colorspace (icc), ==, CD_COLORSPACE_RGB);

	/* modify some details about the profile */
	cd_icc_set_version (icc, 2.09);
	cd_icc_set_colorspace (icc, CD_COLORSPACE_XYZ);
	cd_icc_set_kind (icc, CD_PROFILE_KIND_OUTPUT_DEVICE);
	cd_icc_add_metadata (icc, "SelfTest", "true");
	cd_icc_remove_metadata (icc, "EDID_md5");
	cd_icc_set_characterization_data (icc, "[TI3]");
	cd_icc_set_description (icc, "fr.UTF-8", "Couleurs crayon");

	/* Save to /tmp and reparse new file */
	file = g_file_new_for_path ("/tmp/new.icc");
	ret = cd_icc_save_file (icc,
				file,
				CD_ICC_SAVE_FLAGS_NONE,
				NULL,
				&error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (icc);
	icc = cd_icc_new ();
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_METADATA |
				CD_ICC_LOAD_FLAGS_CHARACTERIZATION,
				NULL,
				&error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);

	/* verify changed values */
	g_assert_cmpfloat (cd_icc_get_version (icc), ==, 2.09);
	g_assert_cmpint (cd_icc_get_kind (icc), ==, CD_PROFILE_KIND_OUTPUT_DEVICE);
	g_assert_cmpint (cd_icc_get_colorspace (icc), ==, CD_COLORSPACE_XYZ);
	g_assert_cmpstr (cd_icc_get_metadata_item (icc, "SelfTest"), ==, "true");
	g_assert_cmpstr (cd_icc_get_metadata_item (icc, "EDID_md5"), ==, NULL);
	str = cd_icc_get_description (icc, "fr.UTF-8", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (str, ==, "Couleurs crayon");
	str = cd_icc_get_characterization_data (icc);
	g_assert_cmpstr (str, ==, "[TI3]");

	g_object_unref (icc);
}

static void
colord_icc_localized_func (void)
{
	CdIcc *icc;
	const gchar *str;
	gboolean ret;
	gchar *filename;
	gchar *tmp;
	GError *error = NULL;
	GFile *file;

	/* open a localized profile */
	icc = cd_icc_new ();
	filename = cd_test_get_filename ("crayons.icc");
	file = g_file_new_for_path (filename);
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_NONE,
				NULL,
				&error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);
	g_free (filename);

	/* marshall to a string */
	tmp = cd_icc_to_string (icc);
	g_assert_cmpstr (tmp, !=, NULL);
	g_debug ("CdIcc: '%s'", tmp);
	g_free (tmp);

	/* open a non-localized profile */
	str = cd_icc_get_description (icc, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (str, ==, "Crayon Colors");
	str = cd_icc_get_description (icc, "en_US.UTF-8", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (str, ==, "Crayon Colors");
	str = cd_icc_get_description (icc, "en_GB.UTF-8", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (str, ==, "Crayon Colours");

	/* get missing data */
	str = cd_icc_get_manufacturer (icc, NULL, &error);
	g_assert_error (error,
			CD_ICC_ERROR,
			CD_ICC_ERROR_NO_DATA);
	g_assert_cmpstr (str, ==, NULL);
	g_clear_error (&error);

	/* use an invalid locale */
	str = cd_icc_get_description (icc, "cra_ZY", &error);
	g_assert_error (error,
			CD_ICC_ERROR,
			CD_ICC_ERROR_INVALID_LOCALE);
	g_assert_cmpstr (str, ==, NULL);
	g_clear_error (&error);
	str = cd_icc_get_description (icc, "cra", &error);
	g_assert_error (error,
			CD_ICC_ERROR,
			CD_ICC_ERROR_INVALID_LOCALE);
	g_assert_cmpstr (str, ==, NULL);
	g_clear_error (&error);

	/* add localized data */
	cd_icc_set_description (icc, "fr.UTF-8", "Couleurs crayon");
	str = cd_icc_get_description (icc, "fr.UTF-8", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (str, ==, "Couleurs crayon");

	g_object_unref (icc);
}

static void
colord_transform_func (void)
{
	CdIcc *icc;
	CdTransform *transform;
	const guint height = 1080;
	const guint repeats = 10;
	const guint max_threads = 8;
	const guint width = 1920;
	gboolean ret;
	gchar *filename;
	GError *error = NULL;
	GFile *file;
	GTimer *timer;
	guint8 data_in[3] = { 127, 32, 64 };
	guint8 data_out[3];
	guint8 *img_data_in;
	guint8 *img_data_out;
	guint8 *img_data_check;
	guint i, j;

	/* setup transform with 8 bit RGB */
	transform = cd_transform_new ();
	cd_transform_set_rendering_intent (transform, CD_RENDERING_INTENT_PERCEPTUAL);
	g_assert_cmpint (cd_transform_get_rendering_intent (transform), ==, CD_RENDERING_INTENT_PERCEPTUAL);
	cd_transform_set_input_pixel_format (transform, CD_PIXEL_FORMAT_RGB24);
	g_assert_cmpint (cd_transform_get_input_pixel_format (transform), ==, CD_PIXEL_FORMAT_RGB24);
	cd_transform_set_output_pixel_format (transform, CD_PIXEL_FORMAT_RGB24);
	g_assert_cmpint (cd_transform_get_output_pixel_format (transform), ==, CD_PIXEL_FORMAT_RGB24);

	/* setup profiles */
	cd_transform_set_input_icc (transform, NULL);
	cd_transform_set_abstract_icc (transform, NULL);

	filename = cd_test_get_filename ("ibm-t61.icc");
	file = g_file_new_for_path (filename);
	icc = cd_icc_new ();
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_NONE,
				NULL,
				&error);
	g_assert_no_error (error);
	g_assert (ret);
	cd_transform_set_output_icc (transform, icc);
	g_free (filename);
	g_object_unref (file);
	g_object_unref (icc);

	/* run through profile */
	ret = cd_transform_process (transform,
				    data_in,
				    data_out,
				    1, 1, 1,
				    NULL,
				    &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpint (data_out[0], ==, 144);
	g_assert_cmpint (data_out[1], ==, 0);
	g_assert_cmpint (data_out[2], ==, 69);

	/* get a known-correct unthreaded result */
	img_data_in = g_new0 (guint8, height * width * 3);
	img_data_out = g_new0 (guint8, height * width * 3);
	img_data_check = g_new0 (guint8, height * width * 3);
	for (i = 0; i < height * width * 3; i++)
		img_data_in[i] = i % 0xff;
	cd_transform_set_max_threads (transform, 1);
	ret = cd_transform_process (transform,
				    img_data_in,
				    img_data_check,
				    width,
				    height,
				    width,
				    NULL,
				    &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get a good default */
	cd_transform_set_max_threads (transform, 0);
	ret = cd_transform_process (transform,
				    img_data_in,
				    img_data_out,
				    width,
				    height,
				    width,
				    NULL,
				    &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (cd_transform_get_max_threads (transform), >=, 1);

	/* run lots of data through the profile */
	timer = g_timer_new ();
	for (i = 1; i < max_threads; i++) {
		cd_transform_set_max_threads (transform, i);
		g_timer_reset (timer);
		for (j = 0; j < repeats; j++) {
			ret = cd_transform_process (transform,
						    img_data_in,
						    img_data_out,
						    width,
						    height,
						    width,
						    NULL,
						    &error);
			g_assert_no_error (error);
			g_assert (ret);
		}
		g_assert_cmpint (memcmp (img_data_out,
					 img_data_check,
					 height * width * 3), ==, 0);
		g_print ("%i threads = %.2fms\n", i,
			 g_timer_elapsed (timer, NULL) * 1000 / repeats);
	}
	g_timer_destroy (timer);

	g_free (img_data_in);
	g_free (img_data_out);
	g_free (img_data_check);
	g_object_unref (transform);
}

#include <glib/gstdio.h>

static void
_copy_files (const gchar *src, const gchar *dest)
{
	gboolean ret;
	gchar *data;
	GError *error = NULL;
	gsize len;

	ret = g_file_get_contents (src, &data, &len, &error);
	g_assert (ret);
	g_assert_no_error (error);
	ret = g_file_set_contents (dest, data, len, &error);
	g_assert (ret);
	g_assert_no_error (error);
	g_free (data);
}

static void
colord_icc_store_added_cb (CdIccStore *store, CdIcc *icc, guint *cnt)
{
	g_debug ("Got ::added(%s)", cd_icc_get_checksum (icc));
	(*cnt)++;
	cd_test_loop_quit ();
}

static void
colord_icc_store_removed_cb (CdIccStore *store, CdIcc *icc, guint *cnt)
{
	g_debug ("Got ::removed(%s)", cd_icc_get_checksum (icc));
	(*cnt)++;
	cd_test_loop_quit ();
}

static void
colord_icc_store_func (void)
{
	CdIccStore *store;
	CdIcc *icc;
	gboolean ret;
	gchar *file1;
	gchar *file1_dup;
	gchar *file2;
	gchar *filename1;
	gchar *filename2;
	gchar *newroot;
	gchar *root;
	GError *error = NULL;
	GPtrArray *array;
	guint added = 0;
	guint removed = 0;

	store = cd_icc_store_new ();
	g_signal_connect (store, "added",
			  G_CALLBACK (colord_icc_store_added_cb),
			  &added);
	g_signal_connect (store, "removed",
			  G_CALLBACK (colord_icc_store_removed_cb),
			  &removed);
	cd_icc_store_set_load_flags (store, CD_ICC_LOAD_FLAGS_NONE);

	filename1 = cd_test_get_filename ("ibm-t61.icc");
	filename2 = cd_test_get_filename ("crayons.icc");

	/* create test directory */
	root = g_strdup_printf ("/tmp/colord-%c%c%c%c",
				g_random_int_range ('a', 'z'),
				g_random_int_range ('a', 'z'),
				g_random_int_range ('a', 'z'),
				g_random_int_range ('a', 'z'));
	g_mkdir (root, 0777);

	file1 = g_build_filename (root, "already-exists.icc", NULL);
	_copy_files (filename1, file1);

	g_assert_cmpint (added, ==, 0);
	g_assert_cmpint (removed, ==, 0);

	/* this is done sync */
	ret = cd_icc_store_search_location (store, root,
					    CD_ICC_STORE_SEARCH_FLAGS_CREATE_LOCATION,
					    NULL, &error);
	g_assert (ret);
	g_assert_no_error (error);

	g_assert_cmpint (added, ==, 1);
	g_assert_cmpint (removed, ==, 0);

	/* find an icc by filename */
	icc = cd_icc_store_find_by_filename (store, file1);
	g_assert (icc != NULL);
	g_assert_cmpstr (cd_icc_get_checksum (icc), ==, "9ace8cce8baac8d492a93a2a232d7702");
	g_object_unref (icc);

	/* find an icc by checksum */
	icc = cd_icc_store_find_by_checksum (store, "9ace8cce8baac8d492a93a2a232d7702");
	g_assert (icc != NULL);
	g_assert_cmpstr (cd_icc_get_filename (icc), ==, file1);
	g_object_unref (icc);

	/* ensure duplicate files do not get added */
	file1_dup = g_build_filename (root, "already-exists-duplicate.icc", NULL);
	_copy_files (filename1, file1_dup);
	cd_test_loop_run_with_timeout (5000);
	cd_test_loop_quit ();
	g_assert_cmpint (added, ==, 1);
	g_assert_cmpint (removed, ==, 0);

	/* create /tmp/colord-foo/new-root/new-icc.icc which should cause a
	 * new directory notifier to be added and the new file to be
	 * discovered */
	newroot = g_build_filename (root, "new-root", NULL);
	g_mkdir (newroot, 0777);
	file2 = g_build_filename (newroot, "new-icc.icc", NULL);
	_copy_files (filename2, file2);

	/* wait for file notifier */
	cd_test_loop_run_with_timeout (5000);
	cd_test_loop_quit ();

	g_assert_cmpint (added, ==, 2);
	g_assert_cmpint (removed, ==, 0);

	/* check store size */
	array = cd_icc_store_get_all (store);
	g_assert_cmpint (array->len, ==, 2);
	g_ptr_array_unref (array);

	g_unlink (file2);

	/* wait for file notifier */
	cd_test_loop_run_with_timeout (5000);
	cd_test_loop_quit ();

	g_assert_cmpint (added, ==, 2);
	g_assert_cmpint (removed, ==, 1);

	/* remove already-exists.icc */
	g_unlink (file1);

	/* wait for file notifier */
	cd_test_loop_run_with_timeout (5000);
	cd_test_loop_quit ();

	g_assert_cmpint (added, ==, 2);
	g_assert_cmpint (removed, ==, 2);

	g_remove (newroot);
	g_remove (root);

	/* check store size */
	array = cd_icc_store_get_all (store);
	g_assert_cmpint (array->len, ==, 0);
	g_ptr_array_unref (array);

	g_free (file1);
	g_free (file1_dup);
	g_free (file2);
	g_free (filename1);
	g_free (filename2);
	g_free (newroot);
	g_free (root);
	g_object_unref (store);
}

static void
colord_icc_util_func (void)
{
	CdIcc *icc_measured;
	CdIcc *icc_reference;
	gboolean ret;
	gdouble coverage = 0;
	GError *error = NULL;

	icc_reference = cd_icc_new ();
	ret = cd_icc_create_default (icc_reference, &error);
	g_assert_no_error (error);
	g_assert (ret);

	icc_measured = cd_icc_new ();
	ret = cd_icc_create_default (icc_measured, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get coverage of one vs. the other */
	ret = cd_icc_utils_get_coverage (icc_reference,
					 icc_measured,
					 &coverage,
					 &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpfloat (coverage, >, 0.99);
	g_assert_cmpfloat (coverage, <, 1.01);

	g_object_unref (icc_reference);
	g_object_unref (icc_measured);
}

static void
colord_edid_func (void)
{
	CdEdid *edid;
	gboolean ret;
	GBytes *data_edid;
	gchar *data;
	gchar *filename;
	GError *error = NULL;
	gsize length = 0;

	edid = cd_edid_new ();
	g_assert (edid != NULL);

	/* LG 21" LCD panel */
	filename = cd_test_get_filename ("LG-L225W-External.bin");
	g_assert (filename != NULL);
	ret = g_file_get_contents (filename, &data, &length, &error);
	g_assert_no_error (error);
	g_assert (ret);
	data_edid = g_bytes_new (data, length);
	ret = cd_edid_parse (edid, data_edid, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_free (filename);
	g_bytes_unref (data_edid);

	g_assert_cmpstr (cd_edid_get_monitor_name (edid), ==, "L225W");
	g_assert_cmpstr (cd_edid_get_vendor_name (edid), ==, "LG");
	g_assert_cmpstr (cd_edid_get_serial_number (edid), ==, "34398");
	g_assert_cmpstr (cd_edid_get_eisa_id (edid), ==, NULL);
	g_assert_cmpstr (cd_edid_get_checksum (edid), ==, "0bb44865bb29984a4bae620656c31368");
	g_assert_cmpstr (cd_edid_get_pnp_id (edid), ==, "GSM");
	g_assert_cmpint (cd_edid_get_height (edid), ==, 30);
	g_assert_cmpint (cd_edid_get_width (edid), ==, 47);
	g_assert_cmpfloat (cd_edid_get_gamma (edid), >=, 2.2f - 0.01);
	g_assert_cmpfloat (cd_edid_get_gamma (edid), <, 2.2f + 0.01);
	g_free (data);

	/* Lenovo T61 internal Panel */
	filename = cd_test_get_filename ("Lenovo-T61-Internal.bin");
	g_assert (filename != NULL);
	ret = g_file_get_contents (filename, &data, &length, &error);
	g_assert_no_error (error);
	g_assert (ret);
	data_edid = g_bytes_new (data, length);
	ret = cd_edid_parse (edid, data_edid, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_free (filename);
	g_bytes_unref (data_edid);

	g_assert_cmpstr (cd_edid_get_monitor_name (edid), ==, NULL);
	g_assert_cmpstr (cd_edid_get_vendor_name (edid), ==, "IBM");
	g_assert_cmpstr (cd_edid_get_serial_number (edid), ==, NULL);
	g_assert_cmpstr (cd_edid_get_eisa_id (edid), ==, "LTN154P2-L05");
	g_assert_cmpstr (cd_edid_get_checksum (edid), ==, "e1865128c7cd5e5ed49ecfc8102f6f9c");
	g_assert_cmpstr (cd_edid_get_pnp_id (edid), ==, "IBM");
	g_assert_cmpint (cd_edid_get_height (edid), ==, 21);
	g_assert_cmpint (cd_edid_get_width (edid), ==, 33);
	g_assert_cmpfloat (cd_edid_get_gamma (edid), >=, 2.2f - 0.01);
	g_assert_cmpfloat (cd_edid_get_gamma (edid), <, 2.2f + 0.01);
	g_free (data);

	g_object_unref (edid);
}

static void
colord_icc_tags_func (void)
{
	CdIcc *icc;
	GError *error = NULL;
	GFile *file;
	gboolean ret;
	gchar **tags;
	gchar *filename;
	GBytes *data;

	/* open a localized profile */
	icc = cd_icc_new ();
	filename = cd_test_get_filename ("crayons.icc");
	file = g_file_new_for_path (filename);
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_NONE,
				NULL,
				&error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);
	g_free (filename);

	/* check tag table */
	tags = cd_icc_get_tags (icc, &error);
	g_assert_no_error (error);
	g_assert (tags != NULL);
	g_assert_cmpint (g_strv_length (tags), ==, 11);
	g_assert_cmpstr (tags[0], ==, "desc");
	g_assert_cmpstr (tags[1], ==, "cprt");
	g_strfreev (tags);

	/* get raw tag data */
	data = cd_icc_get_tag_data (icc, "xxxx", &error);
	g_assert_error (error, CD_ICC_ERROR, CD_ICC_ERROR_NO_DATA);
	g_assert (data == NULL);
	g_clear_error (&error);
	data = cd_icc_get_tag_data (icc, "desc", &error);
	g_assert_no_error (error);
	g_assert (data != NULL);
	g_assert_cmpint (g_bytes_get_size (data), ==, 98);
	g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "mluc");
	g_bytes_unref (data);

	/* set raw tag data */
	data = g_bytes_new_static ("hello", 6);
	cd_icc_set_tag_data (icc, "desc", data, &error);
	g_assert_no_error (error);
	cd_icc_set_tag_data (icc, "xxxx", data, &error);
	g_assert_no_error (error);

	/* re-get raw tag data */
	data = cd_icc_get_tag_data (icc, "desc", &error);
	g_assert (data != NULL);
	g_assert_cmpint (g_bytes_get_size (data), ==, 6);
	g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "hello");
	g_bytes_unref (data);

	g_object_unref (icc);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func ("/colord/spectrum", colord_spectrum_func);
	g_test_add_func ("/colord/spectrum{planckian}", colord_spectrum_planckian_func);
	g_test_add_func ("/colord/edid", colord_edid_func);
	g_test_add_func ("/colord/transform", colord_transform_func);
	g_test_add_func ("/colord/icc", colord_icc_func);
	g_test_add_func ("/colord/icc{util}", colord_icc_util_func);
	g_test_add_func ("/colord/icc{localized}", colord_icc_localized_func);
	g_test_add_func ("/colord/icc{edid}", colord_icc_edid_func);
	g_test_add_func ("/colord/icc{characterization}", colord_icc_characterization_func);
	g_test_add_func ("/colord/icc{save}", colord_icc_save_func);
	g_test_add_func ("/colord/icc{empty}", colord_icc_empty_func);
	g_test_add_func ("/colord/icc{corrupt-dict}", colord_icc_corrupt_dict_func);
	g_test_add_func ("/colord/icc{clear}", colord_icc_clear_func);
	g_test_add_func ("/colord/icc{tags}", colord_icc_tags_func);
	g_test_add_func ("/colord/icc-store", colord_icc_store_func);
	g_test_add_func ("/colord/buffer", colord_buffer_func);
	g_test_add_func ("/colord/enum", colord_enum_func);
	g_test_add_func ("/colord/dom", colord_dom_func);
	g_test_add_func ("/colord/dom{color}", colord_dom_color_func);
	g_test_add_func ("/colord/dom{localized}", colord_dom_localized_func);
	g_test_add_func ("/colord/interp{linear}", colord_interp_linear_func);
	g_test_add_func ("/colord/interp{akima}", colord_interp_akima_func);
	g_test_add_func ("/colord/color", colord_color_func);
	g_test_add_func ("/colord/color{interpolate}", colord_color_interpolate_func);
	g_test_add_func ("/colord/color{blackbody}", colord_color_blackbody_func);
	g_test_add_func ("/colord/math", cd_test_math_func);
	g_test_add_func ("/colord/it8{raw}", colord_it8_raw_func);
	g_test_add_func ("/colord/it8{locale}", colord_it8_locale_func);
	g_test_add_func ("/colord/it8{normalized}", colord_it8_normalized_func);
	g_test_add_func ("/colord/it8{ccmx}", colord_it8_ccmx_func);
	g_test_add_func ("/colord/it8{ccmx-util}", colord_it8_ccmx_util_func);
	g_test_add_func ("/colord/it8{spectra-util}", colord_it8_spectra_util_func);
if(0)	g_test_add_func ("/colord/it8{cri-util}", colord_it8_cri_util_func);
	g_test_add_func ("/colord/it8{ccss}", colord_it8_ccss_func);
	g_test_add_func ("/colord/it8{spect}", colord_it8_spect_func);

	return g_test_run ();
}

