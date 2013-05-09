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

#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <locale.h>

#include <string.h>
#include <glib.h>
#include <glib-object.h>

#include <sys/types.h>
#include <time.h>
#include <pwd.h>

#include "cd-buffer.h"
#include "cd-client.h"
#include "cd-client-sync.h"
#include "cd-color.h"
#include "cd-device.h"
#include "cd-device-sync.h"
#include "cd-dom.h"
#include "cd-icc.h"
#include "cd-interp-akima.h"
#include "cd-interp-linear.h"
#include "cd-interp.h"
#include "cd-it8.h"
#include "cd-it8-utils.h"
#include "cd-math.h"
#include "cd-profile.h"
#include "cd-profile-sync.h"
#include "cd-sensor.h"
#include "cd-sensor-sync.h"
#include "cd-transform.h"
#include "cd-version.h"

static gboolean has_colord_process = FALSE;

/** ver:1.0 ***********************************************************/
static GMainLoop *_test_loop = NULL;
static guint _test_loop_timeout_id = 0;

static gboolean
_g_test_hang_check_cb (gpointer user_data)
{
	g_main_loop_quit (_test_loop);
	_test_loop_timeout_id = 0;
	return G_SOURCE_REMOVE;
}

/**
 * _g_test_loop_run_with_timeout:
 **/
static void
_g_test_loop_run_with_timeout (guint timeout_ms)
{
	g_assert (_test_loop_timeout_id == 0);
	g_assert (_test_loop == NULL);
	_test_loop = g_main_loop_new (NULL, FALSE);
	_test_loop_timeout_id = g_timeout_add (timeout_ms, _g_test_hang_check_cb, NULL);
	g_main_loop_run (_test_loop);
}

/**
 * _g_test_loop_quit:
 **/
static void
_g_test_loop_quit (void)
{
	if (_test_loop_timeout_id > 0) {
		g_source_remove (_test_loop_timeout_id);
		_test_loop_timeout_id = 0;
	}
	if (_test_loop != NULL) {
		g_main_loop_quit (_test_loop);
		g_main_loop_unref (_test_loop);
		_test_loop = NULL;
	}
}

/**
 * _g_test_realpath:
 **/
static gchar *
_g_test_realpath (const gchar *relpath)
{
	gchar *full = NULL;
	gchar *tmp;
	char full_tmp[PATH_MAX];
	tmp = realpath (relpath, full_tmp);
	if (tmp == NULL)
		goto out;
	full = g_strdup (full_tmp);
out:
	return full;
}

/**********************************************************************/

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
	filename = _g_test_realpath (TESTDATADIR "/raw.ti3");
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
				   " 1.234	0	0\n"
				   " 0	0	0\n"
				   " 0	0	0\n"
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
	filename = _g_test_realpath (TESTDATADIR "/normalised.ti3");
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
	filename = _g_test_realpath (TESTDATADIR "/reference.ti3");
	file = g_file_new_for_path (filename);
	ref = cd_it8_new ();
	ret = cd_it8_load_from_file (ref, file, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);

	/* load measured */
	filename = _g_test_realpath (TESTDATADIR "/measured.ti3");
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
	filename = _g_test_realpath (TESTDATADIR "/calibration.ccmx");
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
colord_client_get_devices_cb (GObject *object,
			      GAsyncResult *res,
			      gpointer user_data)
{
	CdClient *client = CD_CLIENT (object);
	GError *error = NULL;
	GPtrArray *devices;

	/* get the result */
	devices = cd_client_get_devices_finish (client, res, &error);
	g_assert_no_error (error);
	g_assert (devices != NULL);
	g_assert_cmpint (devices->len, >=, 1);
	g_ptr_array_unref (devices);
	_g_test_loop_quit ();
}

static gchar *
colord_get_random_device_id (void)
{
	guint32 key;
	key = g_random_int_range (0x00, 0xffff);
	return g_strdup_printf ("self-test-%04x", key);
}

static void
colord_device_qualifiers_func (void)
{
	CdClient *client;
	CdDevice *device;
	CdDeviceRelation relation;
	CdProfile *profile;
	CdProfile *profile2;
	CdProfile *profile_tmp;
	gboolean ret;
	gchar *device_id;
	gchar *profile2_id;
	gchar *profile2_path;
	gchar *profile_id;
	gchar *profile_path;
	GError *error = NULL;
	GHashTable *device_props;
	GHashTable *profile_props;
	GPtrArray *array;
	guint32 key;
	const gchar *qualifier1[] = {"RGB.Plain.300dpi",
				     "RGB.Glossy.300dpi",
				     "RGB.Matte.300dpi",
				     NULL};
	const gchar *qualifier2[] = {"RGB.Transparency.*",
				     "RGB.Glossy.*",
				     NULL};
	const gchar *qualifier3[] = {"*.*.*",
				     NULL};

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	key = g_random_int_range (0x00, 0xffff);
	g_debug ("using random key %04x", key);
	profile_id = g_strdup_printf ("profile-self-test-%04x", key);
	profile2_id = g_strdup_printf ("profile-self-test-%04x-extra", key);
	device_id = g_strdup_printf ("device-self-test-%04x", key);
	profile_path = g_strdup_printf ("/org/freedesktop/ColorManager/profiles/profile_self_test_%04x", key);
	profile2_path = g_strdup_printf ("/org/freedesktop/ColorManager/profiles/profile_self_test_%04x_extra", key);

	/* connect */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create device */
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_FORMAT),
			     g_strdup ("ColorModel.OutputMode.OutputResolution"));
	device = cd_client_create_device_sync (client,
					       device_id,
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* connect */
	ret = cd_device_connect_sync (device, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (cd_device_get_id (device), ==, device_id);

	/* create profile */
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_PROPERTY_FORMAT),
			     g_strdup ("ColorSpace.Paper.Resolution"));
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_PROPERTY_QUALIFIER),
			     g_strdup ("RGB.Matte.300dpi"));
	profile = cd_client_create_profile_sync (client,
						 profile_id,
						 CD_OBJECT_SCOPE_TEMP,
						 profile_props,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);
	g_hash_table_unref (profile_props);

	/* connect */
	ret = cd_profile_connect_sync (profile, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create extra profile */
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_PROPERTY_FORMAT),
			     g_strdup ("ColorSpace.Paper.Resolution"));
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_PROPERTY_QUALIFIER),
			     g_strdup ("RGB.Glossy.1200dpi"));
	profile2 = cd_client_create_profile_sync (client,
						  profile2_id,
						  CD_OBJECT_SCOPE_TEMP,
						  profile_props,
						  NULL,
						  &error);
	g_assert_no_error (error);
	g_assert (profile2 != NULL);
	g_hash_table_unref (profile_props);

	/* wait for daemon */
	_g_test_loop_run_with_timeout (50);
	_g_test_loop_quit ();

	/* connect */
	ret = cd_profile_connect_sync (profile2, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (cd_profile_get_id (profile2), ==, profile2_id);
	g_assert_cmpstr (cd_profile_get_format (profile2), ==, "ColorSpace.Paper.Resolution");
	g_assert_cmpstr (cd_profile_get_qualifier (profile2), ==, "RGB.Glossy.1200dpi");
	g_assert_cmpstr (cd_profile_get_qualifier (profile), ==, "RGB.Matte.300dpi");

	/* check nothing matches qualifier */
	profile_tmp = cd_device_get_profile_for_qualifiers_sync (device,
								 qualifier1,
								 NULL,
								 &error);
	g_assert_error (error, CD_DEVICE_ERROR, CD_DEVICE_ERROR_NOTHING_MATCHED);
	g_assert (profile_tmp == NULL);
	g_clear_error (&error);

	/* check there is no relation */
	relation = cd_device_get_profile_relation_sync (device,
							profile,
							NULL,
							&error);
	g_assert_error (error, CD_DEVICE_ERROR, CD_DEVICE_ERROR_PROFILE_DOES_NOT_EXIST);
	g_assert (relation == CD_DEVICE_RELATION_UNKNOWN);
	g_clear_error (&error);

	/* assign profile to device */
	ret = cd_device_add_profile_sync (device,
					  CD_DEVICE_RELATION_SOFT,
					  profile,
					  NULL,
					  &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check there is now a relation */
	relation = cd_device_get_profile_relation_sync (device,
							profile,
							NULL,
							&error);
	g_assert_no_error (error);
	g_assert (relation == CD_DEVICE_RELATION_SOFT);

	/* assign extra profile to device */
	ret = cd_device_add_profile_sync (device,
					  CD_DEVICE_RELATION_HARD,
					  profile2,
					  NULL,
					  &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* connect */
	ret = cd_device_connect_sync (device, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check profile assigned */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 2);
	g_ptr_array_unref (array);

	/* check matches exact qualifier */
	profile_tmp = cd_device_get_profile_for_qualifiers_sync (device,
								 qualifier1,
								 NULL,
								 &error);
	g_assert_no_error (error);
	g_assert (profile_tmp != NULL);
	g_assert (g_str_has_prefix (cd_profile_get_object_path (profile), profile_path));
	g_object_unref (profile_tmp);

	/* check matches wildcarded qualifier */
	profile_tmp = cd_device_get_profile_for_qualifiers_sync (device,
								 qualifier2,
								 NULL,
								 &error);
	g_assert_no_error (error);
	g_assert (profile_tmp != NULL);
	g_assert (g_str_has_prefix (cd_profile_get_object_path (profile_tmp), profile_path));
	g_object_unref (profile_tmp);

	/* check hard profiles beat soft profiles */
	profile_tmp = cd_device_get_profile_for_qualifiers_sync (device,
								 qualifier3,
								 NULL,
								 &error);
	g_assert_no_error (error);
	g_assert (profile_tmp != NULL);
	g_assert (g_str_has_prefix (cd_profile_get_object_path (profile_tmp), profile2_path));
	g_object_unref (profile_tmp);

	/* uninhibit device (should fail) */
	ret = cd_device_profiling_uninhibit_sync (device,
						  NULL,
						  &error);
	g_assert_error (error, CD_DEVICE_ERROR, CD_DEVICE_ERROR_FAILED_TO_UNINHIBIT);
	g_assert (!ret);
	g_clear_error (&error);

	/* inhibit device */
	ret = cd_device_profiling_inhibit_sync (device,
						NULL,
						&error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check matches nothing */
	profile_tmp = cd_device_get_profile_for_qualifiers_sync (device,
								 qualifier2,
								 NULL,
								 &error);
	g_assert_error (error, CD_DEVICE_ERROR, CD_DEVICE_ERROR_PROFILING);
	g_assert (profile_tmp == NULL);
	g_clear_error (&error);

	/* uninhibit device */
	ret = cd_device_profiling_uninhibit_sync (device,
						  NULL,
						  &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* delete profile */
	ret = cd_client_delete_profile_sync (client,
					     profile,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* delete extra profile */
	ret = cd_client_delete_profile_sync (client,
					     profile2,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* delete device */
	ret = cd_client_delete_device_sync (client,
					    device,
					    NULL,
					    &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_free (profile_id);
	g_free (profile2_id);
	g_free (device_id);
	g_free (profile_path);
	g_free (profile2_path);
	g_hash_table_unref (device_props);
	g_object_unref (device);
	g_object_unref (profile);
	g_object_unref (profile2);
	g_object_unref (client);
}

static void
colord_profile_file_func (void)
{
	CdClient *client;
	CdProfile *profile;
	CdProfile *profile_tmp;
	gboolean ret;
	gchar *filename;
	gchar *profile_id;
	GError *error = NULL;
	GHashTable *profile_props;
	guint32 key;
	struct tm profile_created_time =
		{.tm_sec = 46,
		 .tm_min = 20,
		 .tm_hour = 22,
		 .tm_mday = 23,
		 .tm_mon = 11,
		 .tm_year = 2009 - 1900,
		 .tm_isdst = -1};

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	key = g_random_int_range (0x00, 0xffff);
	g_debug ("using random key %04x", key);
	profile_id = g_strdup_printf ("profile-self-test-%04x", key);

	/* connect */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create profile */
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	filename = _g_test_realpath (TESTDATADIR "/ibm-t61.icc");
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_PROPERTY_FILENAME),
			     g_strdup (filename));
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_PROPERTY_KIND),
			     g_strdup (cd_profile_kind_to_string (CD_PROFILE_KIND_DISPLAY_DEVICE)));
	profile = cd_client_create_profile_sync (client,
						 profile_id,
						 CD_OBJECT_SCOPE_TEMP,
						 profile_props,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);

	/* connect */
	ret = cd_profile_connect_sync (profile, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (cd_profile_get_id (profile), ==, profile_id);
	g_assert_cmpstr (cd_profile_get_format (profile), ==, "ColorSpace..");

	/* check we can find profile based on filename */
	profile_tmp = cd_client_find_profile_by_filename_sync (client,
							       filename,
							       NULL,
							       &error);
	g_assert_no_error (error);
	g_assert (profile_tmp != NULL);

	/* connect */
	ret = cd_profile_connect_sync (profile_tmp, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check id */
	g_assert_cmpstr (cd_profile_get_id (profile_tmp), ==,
			 profile_id);
	g_object_unref (profile_tmp);

	/* check profile kind */
	g_assert_cmpint (cd_profile_get_kind (profile), ==,
			 CD_PROFILE_KIND_DISPLAY_DEVICE);

	/* check profile age */
	g_assert_cmpuint (cd_profile_get_created (profile), ==,
			  mktime (&profile_created_time));

	/* check profile filename */
	g_assert (g_str_has_suffix (cd_profile_get_filename (profile),
				    "data/tests/ibm-t61.icc"));

	/* check profile title set from ICC profile */
	g_assert_cmpstr (cd_profile_get_title (profile), ==, "Huey, LENOVO - 6464Y1H - 15\" (2009-12-23)");

	/* delete profile */
	ret = cd_client_delete_profile_sync (client,
					     profile,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_free (profile_id);
	g_free (filename);
	g_hash_table_unref (profile_props);
	g_object_unref (profile);
	g_object_unref (client);
}

/*
 * Create profile with metadata MAPPING_device_id of xrandr-default
 * Create device with id xrandr-default
 * Check device has soft mapping of profile
 */
static void
colord_device_id_mapping_pd_func (void)
{
	CdClient *client;
	CdDevice *device;
	CdProfile *profile;
	CdProfile *profile_on_device;
	gboolean ret;
	gchar *device_id;
	GError *error = NULL;
	GHashTable *device_props;
	GHashTable *profile_props;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	/* connect to daemon */
	client = cd_client_new ();
	g_assert (client != NULL);
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get random device-id as we're using the mapping DB */
	device_id = colord_get_random_device_id ();

	/* create profile */
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_METADATA_MAPPING_DEVICE_ID),
			     g_strdup (device_id));
	profile = cd_client_create_profile_sync (client,
						 "profile_md_test1_id",
						 CD_OBJECT_SCOPE_TEMP,
						 profile_props,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);

	/* connect */
	ret = cd_profile_connect_sync (profile, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create a device */
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	device = cd_client_create_device_sync (client,
					       device_id,
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* connect to device */
	ret = cd_device_connect_sync (device,
				      NULL,
				      &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure profile is magically on device */
	profile_on_device = cd_device_get_default_profile (device);
	g_assert (profile_on_device != NULL);
	ret = cd_profile_connect_sync (profile_on_device, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure it's the same profile */
	g_assert_cmpstr (cd_profile_get_id (profile), ==,
			 cd_profile_get_id (profile_on_device));
	g_object_unref (profile_on_device);

	/* remove profile which should create cleared timestamp to
	 * prevent future auto-add from metadata */
	ret = cd_device_remove_profile_sync (device,
					     profile,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* delete device */
	ret = cd_client_delete_device_sync (client,
					    device,
					    NULL,
					    &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (device);

	/* create the device again and check it's not auto-added */
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	device = cd_client_create_device_sync (client,
					       device_id,
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* connect to device */
	ret = cd_device_connect_sync (device,
				      NULL,
				      &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure profile is ***NOT*** added to device even though
	 * there is metadata*/
	profile_on_device = cd_device_get_default_profile (device);
	g_assert (profile_on_device == NULL);

	/* delete profile */
	ret = cd_client_delete_profile_sync (client,
					     profile,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* delete device */
	ret = cd_client_delete_device_sync (client,
					    device,
					    NULL,
					    &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_hash_table_unref (profile_props);
	g_object_unref (profile);
	g_object_unref (device);
	g_object_unref (client);
	g_free (device_id);
}

/*
 * Create device with id xrandr-default
 * Create profile with metadata MAPPING_device_id of xrandr-default
 * Check device has soft mapping of profile
 */
static void
colord_device_id_mapping_dp_func (void)
{
	CdClient *client;
	CdDevice *device;
	CdProfile *profile;
	CdProfile *profile_on_device;
	gboolean ret;
	GError *error = NULL;
	GHashTable *device_props;
	GHashTable *profile_props;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	/* connect to daemon */
	client = cd_client_new ();
	g_assert (client != NULL);
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create a device */
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	device = cd_client_create_device_sync (client,
					       "xrandr-default",
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* connect to device */
	ret = cd_device_connect_sync (device,
				      NULL,
				      &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create profile */
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_METADATA_MAPPING_DEVICE_ID),
			     g_strdup ("xrandr-default"));
	profile = cd_client_create_profile_sync (client,
						 "profile_md_test2_id",
						 CD_OBJECT_SCOPE_TEMP,
						 profile_props,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);

	/* connect */
	ret = cd_profile_connect_sync (profile, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure profile is magically on device */
	profile_on_device = cd_device_get_default_profile (device);
	g_assert (profile_on_device != NULL);
	ret = cd_profile_connect_sync (profile_on_device, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure it's the same profile */
	g_assert_cmpstr (cd_profile_get_id (profile), ==,
			 cd_profile_get_id (profile_on_device));

	/* delete device */
	ret = cd_client_delete_device_sync (client,
					    device,
					    NULL,
					    &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* delete profile */
	ret = cd_client_delete_profile_sync (client,
					     profile,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_hash_table_unref (profile_props);
	g_object_unref (profile);
	g_object_unref (profile_on_device);
	g_object_unref (device);
	g_object_unref (client);
}

static void
colord_icc_meta_dict_func (void)
{
	gchar *filename;
	gboolean ret;
	GError *error = NULL;
	GHashTable *metadata;
	GHashTable *profile_props;
	CdProfile *profile;
	CdClient *client;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	/* create */
	client = cd_client_new ();
	g_assert (client != NULL);

	/* connect */
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create profile */
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	filename = _g_test_realpath (TESTDATADIR "/ibm-t61.icc");
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_PROPERTY_FILENAME),
			     g_strdup (filename));
	profile = cd_client_create_profile_sync (client,
						 "profile_metadata_test",
						 CD_OBJECT_SCOPE_TEMP,
						 profile_props,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);

	/* connect */
	ret = cd_profile_connect_sync (profile, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check metadata */
	metadata = cd_profile_get_metadata (profile);
	g_assert_cmpint (g_hash_table_size (metadata), ==, 1);
	g_assert_cmpstr (g_hash_table_lookup (metadata, "EDID_md5"), ==,
			 "f09e42aa86585d1bb6687d3c322ed0c1");
	g_hash_table_unref (metadata);

	/* check profile warnings */
	g_assert_cmpint (g_strv_length (cd_profile_get_warnings (profile)), ==, 0);

	/* create profile */
	ret = cd_client_delete_profile_sync (client,
					     profile,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_hash_table_unref (profile_props);
	g_object_unref (profile);
	g_object_unref (client);
	g_free (filename);
}

#if 0
static void
colord_sensor_get_sample_cb (GObject *object,
			     GAsyncResult *res,
			     gpointer user_data)
{
	CdSensor *sensor = CD_SENSOR (object);
	GError *error = NULL;
	gboolean ret;

	/* get the result */
	ret = cd_sensor_get_sample_finished (sensor, res, &error);
	g_assert_no_error (error);
	g_assert (ret);
	_g_test_loop_quit ();
}
#endif

static int _refcount = 0;

static void
colord_sensor_state_notify_cb (GObject *object,
			       GParamSpec *pspec,
			       CdClient *client)
{
	CdSensor *sensor = CD_SENSOR (object);
	g_debug ("notify::state(%s)", cd_sensor_state_to_string (cd_sensor_get_state (sensor)));
	_refcount++;
}

static void
colord_sensor_func (void)
{
	CdClient *client;
	CdColorXYZ *values;
	CdSensor *sensor;
	gboolean ret;
	GError *error = NULL;
	GHashTable *hash;
	GPtrArray *array;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	array = cd_client_get_sensors_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (array != NULL);
	if (array->len == 0) {
		g_print ("WARNING: no dummy sensor found, skipping\n");
		goto out;
	}
	g_assert_cmpint (array->len, ==, 1);

	sensor = g_ptr_array_index (array, 0);

	ret = cd_sensor_connect_sync (sensor, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpint (cd_sensor_get_kind (sensor), ==, CD_SENSOR_KIND_DUMMY);
	g_assert_cmpint (cd_sensor_get_state (sensor), ==, CD_SENSOR_STATE_UNKNOWN);
	g_assert (!cd_sensor_get_locked (sensor));
	g_assert_cmpstr (cd_sensor_get_serial (sensor), ==, "0123456789a");
	g_assert_cmpstr (cd_sensor_get_vendor (sensor), ==, "Acme Corp");
	g_assert_cmpstr (cd_sensor_get_model (sensor), ==, "Dummy Sensor #1");
	g_assert_cmpstr (cd_sensor_get_object_path (sensor), ==, "/org/freedesktop/ColorManager/sensors/dummy");
	g_assert_cmpint (cd_sensor_get_caps (sensor), ==, 126);
	g_assert (cd_sensor_has_cap (sensor, CD_SENSOR_CAP_PROJECTOR));

#if 0
	/* get a sample async */
	ret = cd_sensor_get_sample (sensor,
				    CD_SENSOR_CAP_PROJECTOR,
				    NULL,
				    colord_sensor_get_sample_cb,
				    NULL);
	_g_test_loop_run_with_timeout (5000);
#endif

	g_signal_connect (sensor,
			  "notify::state",
			  G_CALLBACK (colord_sensor_state_notify_cb),
			  NULL);

	/* lock */
	ret = cd_sensor_lock_sync (sensor,
				   NULL,
				   &error);
	g_assert_no_error (error);
	g_assert (ret);

	_g_test_loop_run_with_timeout (5);
	_g_test_loop_quit ();
	g_assert (cd_sensor_get_locked (sensor));

	/* lock again */
	ret = cd_sensor_lock_sync (sensor,
				   NULL,
				   &error);
	g_assert_error (error, CD_SENSOR_ERROR, CD_SENSOR_ERROR_ALREADY_LOCKED);
	g_assert (!ret);

	_g_test_loop_run_with_timeout (5);
	_g_test_loop_quit ();
	g_assert (cd_sensor_get_locked (sensor));
	g_clear_error (&error);

	/* setup virtual swatch */
	hash = g_hash_table_new_full (g_str_hash,
				      g_str_equal,
				      g_free,
				      (GDestroyNotify) g_variant_unref);
	g_hash_table_insert (hash,
			     g_strdup ("sample[red]"),
			     g_variant_take_ref (g_variant_new_double (0.1)));
	g_hash_table_insert (hash,
			     g_strdup ("sample[green]"),
			     g_variant_take_ref (g_variant_new_double (0.2)));
	g_hash_table_insert (hash,
			     g_strdup ("sample[blue]"),
			     g_variant_take_ref (g_variant_new_double (0.3)));
	ret = cd_sensor_set_options_sync (sensor,
					  hash,
					  NULL,
					  &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get a sample sync */
	values = cd_sensor_get_sample_sync (sensor,
					    CD_SENSOR_CAP_LCD,
					    NULL,
					    &error);
	g_assert_no_error (error);
	g_assert (values != NULL);

	/* get async events */
	_g_test_loop_run_with_timeout (5);
	_g_test_loop_quit ();
	g_assert_cmpint (_refcount, ==, 2);

	g_hash_table_unref (hash);

	g_debug ("sample was %f %f %f", values->X, values->Y, values->Z);
	g_assert_cmpfloat (values->X - 0.027599, >, -0.01);
	g_assert_cmpfloat (values->X - 0.027599, <, 0.01);
	g_assert_cmpfloat (values->Y - 0.030403, >, -0.01);
	g_assert_cmpfloat (values->Y - 0.030403, <, 0.01);
	g_assert_cmpfloat (values->Z - 0.055636, >, -0.01);
	g_assert_cmpfloat (values->Z - 0.055636, <, 0.01);
	cd_color_xyz_free (values);

	/* unlock */
	ret = cd_sensor_unlock_sync (sensor,
				     NULL,
				     &error);
	g_assert_no_error (error);
	g_assert (ret);

	_g_test_loop_run_with_timeout (5);
	_g_test_loop_quit ();
	g_assert (!cd_sensor_get_locked (sensor));

	/* lock again */
	ret = cd_sensor_unlock_sync (sensor,
				     NULL,
				     &error);
	g_assert_error (error, CD_SENSOR_ERROR, CD_SENSOR_ERROR_NOT_LOCKED);
	g_assert (!ret);

	_g_test_loop_run_with_timeout (5);
	_g_test_loop_quit ();
	g_assert (!cd_sensor_get_locked (sensor));
	g_clear_error (&error);
out:
	g_ptr_array_unref (array);
	g_object_unref (client);
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
	CdColorXYZ *xyz;
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

	cd_color_xyz_free (xyz);
}

static void
colord_client_func (void)
{
	CdClient *client;
	const gchar *version;
	gboolean ret;
	GError *error = NULL;

	/* no running colord to use */
	client = cd_client_new ();
	has_colord_process = cd_client_get_has_server (client);
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		goto out;
	}

	/* check not connected */
	g_assert (!cd_client_get_connected (client));

	/* connect once */
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check connected */
	g_assert (cd_client_get_connected (client));

	/* connect again */
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	version = cd_client_get_daemon_version (client);
	g_assert_cmpstr (version, !=, NULL);
out:
	g_object_unref (client);
}

static void
colord_device_mapping_func (void)
{
	CdClient *client;
	CdDevice *device;
	CdProfile *profile1;
	CdProfile *profile2;
	CdProfile *profile_tmp;
	gboolean ret;
	gchar *profile_id1;
	gchar *profile_id2;
	GError *error = NULL;
	GHashTable *device_props;
	gint32 key;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	key = g_random_int_range (0x00, 0xffff);
	g_debug ("using random key %04x", key);
	profile_id1 = g_strdup_printf ("profile-mapping-%04x_1", key);
	profile_id2 = g_strdup_printf ("profile-mapping-%04x_2", key);

	client = cd_client_new ();

	/* connect once */
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create a device */
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	device = cd_client_create_device_sync (client,
					       "device_mapping",
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* create a profile */
	profile1 = cd_client_create_profile_sync (client,
						  profile_id1,
						  CD_OBJECT_SCOPE_TEMP,
						  NULL,
						  NULL,
						  &error);
	g_assert_no_error (error);
	g_assert (profile1 != NULL);

	/* create another profile */
	profile2 = cd_client_create_profile_sync (client,
						  profile_id2,
						  CD_OBJECT_SCOPE_TEMP,
						  NULL,
						  NULL,
						  &error);
	g_assert_no_error (error);
	g_assert (profile2 != NULL);

	/* connect to device */
	ret = cd_device_connect_sync (device,
				      NULL,
				      &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* assign profile to device */
	ret = cd_device_add_profile_sync (device,
					  CD_DEVICE_RELATION_HARD,
					  profile1,
					  NULL,
					  &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* assign profile to device */
	ret = cd_device_add_profile_sync (device,
					  CD_DEVICE_RELATION_HARD,
					  profile2,
					  NULL,
					  &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure the second profile is the default profile */
	profile_tmp = cd_device_get_default_profile (device);
	g_assert (profile_tmp != NULL);
	g_assert_cmpstr (cd_profile_get_object_path (profile_tmp),
			 ==,
			 cd_profile_get_object_path (profile2));
	g_object_unref (profile_tmp);

	/* remove both profiles */
	ret = cd_client_delete_profile_sync (client,
					     profile1,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (profile1);
	ret = cd_client_delete_profile_sync (client,
					     profile2,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (profile2);

	/* add back the first profile */
	profile1 = cd_client_create_profile_sync (client,
						  profile_id1,
						  CD_OBJECT_SCOPE_TEMP,
						  NULL,
						  NULL,
						  &error);
	g_assert_no_error (error);
	g_assert (profile1 != NULL);

	/* ensure the first profile is selected */
	profile_tmp = cd_device_get_default_profile (device);
	g_assert (profile_tmp != NULL);
	g_assert_cmpstr (cd_profile_get_object_path (profile_tmp),
			 ==,
			 cd_profile_get_object_path (profile1));
	g_object_unref (profile_tmp);

	/* add back the second (and prefered) profile */
	profile2 = cd_client_create_profile_sync (client,
						  profile_id2,
						  CD_OBJECT_SCOPE_TEMP,
						  NULL,
						  NULL,
						  &error);
	g_assert_no_error (error);
	g_assert (profile2 != NULL);

	/* ensure the second profile is selected */
	profile_tmp = cd_device_get_default_profile (device);
	g_assert (profile_tmp != NULL);
	g_assert_cmpstr (cd_profile_get_object_path (profile_tmp),
			 ==,
			 cd_profile_get_object_path (profile2));
	g_object_unref (profile_tmp);

	/* delete the device */
	ret = cd_client_delete_device_sync (client,
					    device,
					    NULL,
					    &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (device);

	/* create a device */
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	device = cd_client_create_device_sync (client,
					       "device_mapping",
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* connect to device */
	ret = cd_device_connect_sync (device,
				      NULL,
				      &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure the second profile is the default profile */
	profile_tmp = cd_device_get_default_profile (device);
	g_assert (profile_tmp != NULL);
	g_assert_cmpstr (cd_profile_get_object_path (profile_tmp),
			 ==,
			 cd_profile_get_object_path (profile2));
	g_object_unref (profile_tmp);

	g_free (profile_id1);
	g_free (profile_id2);
	g_object_unref (profile1);
	g_object_unref (profile2);
	g_object_unref (device);
	g_object_unref (client);
}

static void
colord_client_fd_pass_func (void)
{
	CdClient *client;
	CdProfile *profile;
	GHashTable *profile_props;
	gboolean ret;
	GError *error = NULL;
	gchar full_path[PATH_MAX];
	gchar *tmp;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	/* create */
	client = cd_client_new ();
	g_assert (client != NULL);

	/* connect */
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create extra profile */
	tmp = realpath (TESTDATADIR "/ibm-t61.icc", full_path);
	g_assert (tmp != NULL);
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup ("Filename"),
			     g_strdup (full_path));
	profile = cd_client_create_profile_sync (client,
						 "icc_temp",
						 CD_OBJECT_SCOPE_TEMP,
						 profile_props,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);

	g_hash_table_unref (profile_props);
	g_object_unref (profile);
	g_object_unref (client);
}

/**
 * colord_get_profile_destination:
 **/
static GFile *
colord_get_profile_destination (GFile *file)
{
	gchar *basename;
	gchar *destination;
	GFile *dest;

	g_return_val_if_fail (file != NULL, NULL);

	/* get destination filename for this source file */
	basename = g_file_get_basename (file);
	destination = g_build_filename (g_get_user_data_dir (), "icc", basename, NULL);
	dest = g_file_new_for_path (destination);

	g_free (basename);
	g_free (destination);
	return dest;
}

static void
colord_client_import_func (void)
{
	CdClient *client;
	CdProfile *profile;
	CdProfile *profile2;
	gboolean ret;
	GFile *file;
	GFile *invalid_file;
	GFile *dest;
	GError *error = NULL;
	gchar full_path[PATH_MAX];
	gchar *dest_path;
	gchar *tmp;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	/* create */
	client = cd_client_new ();
	g_assert (client != NULL);

	/* connect */
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check we can't import random files */
	tmp = realpath (TESTDATADIR "/Makefile.am", full_path);
	g_assert (tmp != NULL);
	invalid_file = g_file_new_for_path (full_path);
	profile2 = cd_client_import_profile_sync (client,
						  invalid_file,
						  NULL,
						  &error);
	g_assert_error (error,
			CD_CLIENT_ERROR,
			CD_CLIENT_ERROR_FILE_INVALID);
	g_assert (profile2 == NULL);
	g_clear_error (&error);

	/* create extra profile */
	tmp = realpath (TESTDATADIR "/ibm-t61.icc", full_path);
	g_assert (tmp != NULL);
	file = g_file_new_for_path (full_path);

	/* ensure it's deleted */
	dest = colord_get_profile_destination (file);
	if (g_file_query_exists (dest, NULL)) {
		ret = g_file_delete (dest, NULL, &error);
		g_assert_no_error (error);
		g_assert (ret);

		/* wait for daemon to DTRT */
		_g_test_loop_run_with_timeout (2000);
	}

	/* import it */
	profile = cd_client_import_profile_sync (client,
						 file,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);

	/* connect */
	ret = cd_profile_connect_sync (profile, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* make sure it's now installed in the right place */
	dest_path = g_file_get_path (dest);
	g_assert_cmpstr (cd_profile_get_filename (profile), ==, dest_path);

	/* make sure we can't import it again */
	profile2 = cd_client_import_profile_sync (client,
						  file,
						  NULL,
						  &error);
	g_assert_error (error, CD_CLIENT_ERROR, CD_CLIENT_ERROR_ALREADY_EXISTS);
	g_assert (profile2 == NULL);
	g_clear_error (&error);

	/* delete it */
	ret = g_file_delete (dest, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_free (dest_path);
	g_object_unref (invalid_file);
	g_object_unref (file);
	g_object_unref (dest);
	g_object_unref (profile);
	g_object_unref (client);
}

static void
colord_delete_profile_good_cb (GObject *object, GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	CdClient *client = CD_CLIENT (object);

	ret = cd_client_delete_profile_finish (client, res, &error);
	g_assert_no_error (error);
	g_assert (ret);

	_g_test_loop_quit ();
}

static void
colord_delete_profile_bad_cb (GObject *object, GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	CdClient *client = CD_CLIENT (object);

	ret = cd_client_delete_profile_finish (client, res, &error);
	g_assert_error (error, CD_CLIENT_ERROR, CD_CLIENT_ERROR_NOT_FOUND);
	g_assert (!ret);

	_g_test_loop_quit ();
}

static void
colord_client_async_func (void)
{
	gboolean ret;
	GError *error = NULL;
	CdClient *client;
	CdProfile *profile;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	client = cd_client_new ();

	/* connect */
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* delete known profile */
	profile = cd_profile_new_with_object_path ("/dave");
	cd_client_delete_profile (client, profile, NULL,
				 (GAsyncReadyCallback) colord_delete_profile_bad_cb, NULL);
	_g_test_loop_run_with_timeout (1500);
	g_debug ("not deleted profile in %f", g_test_timer_elapsed ());
	g_object_unref (profile);

	/* create profile */
	profile = cd_client_create_profile_sync (client,
						 "icc_tmp",
						 CD_OBJECT_SCOPE_TEMP,
						 NULL,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);

	/* delete known profile */
	cd_client_delete_profile (client, profile, NULL,
				 (GAsyncReadyCallback) colord_delete_profile_good_cb, NULL);
	_g_test_loop_run_with_timeout (1500);
	g_debug ("deleted profile in %f", g_test_timer_elapsed ());
	g_object_unref (profile);

	g_object_unref (client);
}

static void
colord_device_connect_cb (GObject *object, GAsyncResult *res, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	CdDevice *device = CD_DEVICE (object);

	ret = cd_device_connect_finish (device, res, &error);
	g_assert_no_error (error);
	g_assert (ret);

	_g_test_loop_quit ();
}

static void
colord_device_async_func (void)
{
	CdClient *client;
	CdDevice *device;
	CdDevice *device_tmp;
	gboolean ret;
	GError *error = NULL;
	GHashTable *device_props;
	const gchar *device_name = "device_async_dave";
	gchar *device_path;
	struct passwd *user_details;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	user_details = getpwuid(getuid());
	device_path = g_strdup_printf("/org/freedesktop/ColorManager/devices/%s_%s_%d",
				      device_name,
				      user_details->pw_name,
				      user_details->pw_uid);

	client = cd_client_new ();

	/* connect */
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	device = cd_client_create_device_sync (client,
					       device_name,
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* connect */
	cd_device_connect (device, NULL, colord_device_connect_cb, NULL);

	/* unref straight away */
	g_object_unref (device);
	device = NULL;

	_g_test_loop_run_with_timeout (1500);
	g_debug ("connected to device in %f", g_test_timer_elapsed ());

	/* set a property in another instance */
	device_tmp = cd_device_new_with_object_path (device_path);
	ret = cd_device_connect_sync (device_tmp, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = cd_device_set_model_sync (device_tmp, "Cray", NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (device_tmp);

	/* delete known device */
	device_tmp = cd_device_new_with_object_path (device_path);
	ret = cd_client_delete_device_sync (client, device_tmp, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (device_tmp);

	g_object_unref (client);
	g_free(device_path);
}

static void
colord_client_systemwide_func (void)
{
	CdClient *client;
	CdProfile *profile;
	GHashTable *profile_props;
	gboolean ret;
	GError *error = NULL;
	gchar full_path[PATH_MAX];
	gchar *tmp;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	/* create */
	client = cd_client_new ();
	g_assert (client != NULL);

	/* connect */
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create extra profile */
	tmp = realpath (TESTDATADIR "/ibm-t61.icc", full_path);
	g_assert (tmp != NULL);
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup ("Filename"),
			     g_strdup (full_path));
	profile = cd_client_create_profile_sync (client,
						 "icc_temp",
						 CD_OBJECT_SCOPE_TEMP,
						 profile_props,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);

	/* set profile filename */
	ret = cd_profile_install_system_wide_sync (profile, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* delete extra profile */
	ret = cd_client_delete_profile_sync (client,
					     profile,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_hash_table_unref (profile_props);
	g_object_unref (profile);
	g_object_unref (client);
}

static void
colord_device_invalid_func (void)
{
	CdDevice *device;
	gboolean ret;
	GError *error = NULL;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	/* create a device with an invalid object path */
	device = cd_device_new_with_object_path ("/garbage");
	g_assert (device != NULL);

	/* connect */
	ret = cd_device_connect_sync (device, NULL, &error);
	g_assert_error (error, CD_DEVICE_ERROR, CD_DEVICE_ERROR_INTERNAL);
	g_assert (!ret);
	g_clear_error (&error);

	g_object_unref (device);
}

static void
colord_device_func (void)
{
	CdClient *client;
	CdDevice *device;
	gboolean ret;
	gchar *device_id;
	gchar *device_path;
	GError *error = NULL;
	GHashTable *device_props;
	GPtrArray *array;
	GPtrArray *devices;
	guint32 key;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	key = g_random_int_range (0x00, 0xffff);
	g_debug ("using random key %04x", key);
	device_id = g_strdup_printf ("device-self-test-%04x", key);
	device_path = g_strdup_printf ("/org/freedesktop/ColorManager/devices/device_self_test_%04x", key);

	/* connect */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get number of devices */
	devices = cd_client_get_devices_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (devices != NULL);

	/* create device */
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_VENDOR),
			     g_strdup ("Hewlett-Packard Ltd."));
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_MODEL),
			     g_strdup ("3000"));
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_FORMAT),
			     g_strdup ("ColorModel.OutputMode.OutputResolution"));
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_METADATA_XRANDR_NAME),
			     g_strdup ("lvds1"));
	device = cd_client_create_device_sync (client,
					       device_id,
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);
	g_assert (g_str_has_prefix (cd_device_get_object_path (device), device_path));

	/* connect */
	ret = cd_device_connect_sync (device, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (cd_device_get_id (device), ==, device_id);

	/* get new number of devices */
	array = cd_client_get_devices_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (array != NULL);
	g_assert_cmpint (devices->len + 1, ==, array->len);
	g_ptr_array_unref (array);

	/* get same data async */
	cd_client_get_devices (client,
			       NULL,
			       colord_client_get_devices_cb,
			       NULL);
	_g_test_loop_run_with_timeout (5000);

	/* set device serial */
	ret = cd_device_set_serial_sync (device, "0001", NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* set device colorspace */
	ret = cd_device_set_colorspace_sync (device,
					     CD_COLORSPACE_LAB,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* set device model */
	ret = cd_device_set_kind_sync (device, CD_DEVICE_KIND_DISPLAY,
				       NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* wait for daemon */
	_g_test_loop_run_with_timeout (50);
	_g_test_loop_quit ();

	/* check device created */
	g_assert_cmpuint (cd_device_get_created (device), >, 1295354162);

	/* check device modified */
	g_assert_cmpuint (cd_device_get_modified (device), >, 1295354162);

	/* check device model */
	g_assert_cmpstr (cd_device_get_model (device), ==, "3000");

	/* check device vendor */
	g_assert_cmpstr (cd_device_get_vendor (device), ==, "Hewlett Packard");

	/* check device serial */
	g_assert_cmpstr (cd_device_get_serial (device), ==, "0001");

	/* check device format */
	g_assert_cmpstr (cd_device_get_format (device), ==, "ColorModel.OutputMode.OutputResolution");

	/* check device metadata item */
	g_assert_cmpstr (cd_device_get_metadata_item (device, "XRANDR_name"), ==, "lvds1");

	/* check device kind */
	g_assert_cmpint (cd_device_get_kind (device), ==, CD_DEVICE_KIND_DISPLAY);

	/* check device colorspace */
	g_assert_cmpint (cd_device_get_colorspace (device), ==, CD_COLORSPACE_LAB);

	/* delete device */
	ret = cd_client_delete_device_sync (client,
					    device,
					    NULL,
					    &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get new number of devices */
	array = cd_client_get_devices_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (array != NULL);
	g_assert_cmpint (devices->len, ==, array->len);
	g_ptr_array_unref (array);

	g_free (device_id);
	g_free (device_path);
	g_ptr_array_unref (devices);
	g_hash_table_unref (device_props);
	g_object_unref (client);
	g_object_unref (device);
}

static void
colord_device_embedded_func (void)
{
	CdClient *client;
	CdDevice *device;
	gboolean ret;
	GError *error = NULL;
	GHashTable *device_props;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create device */
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_EMBEDDED),
			     NULL);
	device = cd_client_create_device_sync (client,
					       "device_embedded",
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* connect */
	ret = cd_device_connect_sync (device, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check embedded */
	g_assert (cd_device_get_embedded (device));

	g_hash_table_unref (device_props);
	g_object_unref (device);
	g_object_unref (client);
}

static void
colord_device_invalid_kind_func (void)
{
	CdClient *client;
	CdDevice *device;
	gboolean ret;
	GError *error = NULL;
	GHashTable *device_props;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create device */
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup ("thermodynamic-teapot"));
	device = cd_client_create_device_sync (client,
					       "device_kind",
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_error (error, CD_CLIENT_ERROR, CD_CLIENT_ERROR_INPUT_INVALID);
	g_assert (device == NULL);
	g_error_free (error);

	g_hash_table_unref (device_props);
	g_object_unref (client);
}

static void
colord_client_standard_space_func (void)
{
	CdClient *client;
	CdProfile *profile;
	gboolean ret;
	GError *error = NULL;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get profile */
	profile = cd_client_get_standard_space_sync (client,
						     CD_STANDARD_SPACE_SRGB,
						     NULL,
						     &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);

	/* connect */
	ret = cd_profile_connect_sync (profile, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (cd_profile_get_metadata_item (profile, CD_PROFILE_METADATA_STANDARD_SPACE), ==, "srgb");
	g_assert (cd_profile_get_is_system_wide (profile));

	g_object_unref (profile);
	g_object_unref (client);
}

static void
colord_device_modified_func (void)
{
	CdClient *client;
	CdDevice *device;
	CdProfile *profile;
	gboolean ret;
	gchar full_path[PATH_MAX];
	gchar *tmp;
	GError *error = NULL;
	GHashTable *device_props;
	GHashTable *profile_props;
	GPtrArray *array;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	/* create */
	client = cd_client_new ();
	g_assert (client != NULL);

	/* create device */
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_VENDOR),
			     g_strdup ("Hewlett-Packard Ltd."));
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_MODEL),
			     g_strdup ("3000"));
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_METADATA_XRANDR_NAME),
			     g_strdup ("lvds1"));
	device = cd_client_create_device_sync (client,
					       "device_dave",
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);
	g_assert (g_str_has_prefix (cd_device_get_object_path (device),
		  "/org/freedesktop/ColorManager/devices/device_dave"));

	/* connect */
	ret = cd_device_connect_sync (device, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (cd_device_get_id (device), ==, "device_dave");

	/* get new number of profiles */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 0);
	g_ptr_array_unref (array);

	/* create extra profile */
	tmp = realpath (TESTDATADIR "/ibm-t61.icc", full_path);
	g_assert (tmp != NULL);
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	profile = cd_client_create_profile_sync (client,
						 "icc_temp2",
						 CD_OBJECT_SCOPE_TEMP,
						 profile_props,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);

	/* assign profile to device */
	ret = cd_device_add_profile_sync (device, CD_DEVICE_RELATION_SOFT, profile, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* connect */
	ret = cd_device_connect_sync (device, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get new number of profiles */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 1);
	g_ptr_array_unref (array);

	/* delete extra profile */
	ret = cd_client_delete_profile_sync (client,
					     profile,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* wait for daemon */
	_g_test_loop_run_with_timeout (50);
	_g_test_loop_quit ();

	/* get new number of profiles */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 0);
	g_ptr_array_unref (array);

	g_hash_table_unref (profile_props);
	g_object_unref (profile);
	g_hash_table_unref (device_props);
	g_object_unref (device);
	g_object_unref (client);
}

static void
colord_device_seat_func (void)
{
	CdClient *client;
	CdDevice *device;
#ifdef HAVE_LIBSYSTEMD_LOGIN
	const gchar *tmp;
#endif
	gboolean ret;
	GError *error = NULL;
	GHashTable *device_props;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	/* ensure the seat is set */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	device = cd_client_create_device_sync (client,
					       "device_seat_test",
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* connect */
	ret = cd_device_connect_sync (device, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check the seat */
#ifdef HAVE_LIBSYSTEMD_LOGIN
	tmp = cd_device_get_seat (device);
	g_assert_cmpstr (tmp, ==, "seat0");
#endif

	/* delete device */
	ret = cd_client_delete_device_sync (client,
					    device,
					    NULL,
					    &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_hash_table_unref (device_props);
	g_object_unref (device);
	g_object_unref (client);
}

static void
colord_device_enabled_func (void)
{
	CdClient *client;
	CdDevice *device;
	gboolean ret;
	GError *error = NULL;
	GHashTable *device_props;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	/* ensure the device is enabled by default */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	device = cd_client_create_device_sync (client,
					       "device_enabled_test",
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* connect */
	ret = cd_device_connect_sync (device, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* disable the device */
	ret = cd_device_set_enabled_sync (device,
					  FALSE,
					  NULL,
					  &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert (!cd_device_get_enabled (device));

	/* disable the device (again, should be allowed) */
	ret = cd_device_set_enabled_sync (device,
					  FALSE,
					  NULL,
					  &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert (!cd_device_get_enabled (device));

	/* delete device */
	ret = cd_client_delete_device_sync (client,
					    device,
					    NULL,
					    &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (device);

	/* check the device is disabled by default */
	device = cd_client_create_device_sync (client,
					       "device_enabled_test",
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* connect */
	ret = cd_device_connect_sync (device, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* enable the device */
	ret = cd_device_set_enabled_sync (device,
					  TRUE,
					  NULL,
					  &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert (cd_device_get_enabled (device));

	/* delete device */
	ret = cd_client_delete_device_sync (client,
					    device,
					    NULL,
					    &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_hash_table_unref (device_props);
	g_object_unref (device);
	g_object_unref (client);
}

/* when we re-add profiles, ensure they are sorted so the newest
 * assigned profile is first, not the newest-added */
static void
colord_profile_ordering_func (void)
{
	CdClient *client;
	CdDevice *device;
	CdProfile *profile1;
	CdProfile *profile2;
	CdProfile *profile_tmp;
	gboolean ret;
	gchar *device_id;
	GError *error = NULL;
	GHashTable *device_props;
	GPtrArray *array;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	/* create */
	client = cd_client_new ();
	g_assert (client != NULL);

	/* create device */
	device_id = colord_get_random_device_id ();
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	device = cd_client_create_device_sync (client,
					       device_id,
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* connect */
	ret = cd_device_connect_sync (device, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (cd_device_get_id (device), ==, device_id);

	/* get new number of profiles */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 0);
	g_ptr_array_unref (array);

	/* create older profile */
	profile2 = cd_client_create_profile_sync (client,
						  "profile2",
						  CD_OBJECT_SCOPE_TEMP,
						  NULL,
						  NULL,
						  &error);
	g_assert_no_error (error);
	g_assert (profile2 != NULL);

	/* assign profile to device */
	ret = cd_device_add_profile_sync (device,
					  CD_DEVICE_RELATION_HARD,
					  profile2,
					  NULL,
					  &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create newer profile */
	profile1 = cd_client_create_profile_sync (client,
						  "profile1",
						  CD_OBJECT_SCOPE_TEMP,
						  NULL,
						  NULL,
						  &error);
	g_assert_no_error (error);
	g_assert (profile1 != NULL);

	/* assign profile to device */
	ret = cd_device_add_profile_sync (device,
					  CD_DEVICE_RELATION_HARD,
					  profile1,
					  NULL,
					  &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* wait for daemon */
	_g_test_loop_run_with_timeout (50);
	_g_test_loop_quit ();

	/* get new number of profiles */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 2);
	profile_tmp = CD_PROFILE (g_ptr_array_index (array, 0));
	g_assert (g_str_has_prefix (cd_profile_get_object_path (profile_tmp),
				    "/org/freedesktop/ColorManager/profiles/profile1"));
	profile_tmp = CD_PROFILE (g_ptr_array_index (array, 1));
	g_assert (g_str_has_prefix (cd_profile_get_object_path (profile_tmp),
				    "/org/freedesktop/ColorManager/profiles/profile2"));
	g_ptr_array_unref (array);

	/* delete profiles */
	ret = cd_client_delete_profile_sync (client, profile1, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = cd_client_delete_profile_sync (client, profile2, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* wait for daemon */
	_g_test_loop_run_with_timeout (50);
	_g_test_loop_quit ();

	/* get new number of profiles */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 0);
	g_ptr_array_unref (array);

	/* done with profiles */
	g_object_unref (profile1);
	g_object_unref (profile2);

	/* create newer profile */
	profile1 = cd_client_create_profile_sync (client,
						  "profile1",
						  CD_OBJECT_SCOPE_TEMP,
						  NULL,
						  NULL,
						  &error);
	g_assert_no_error (error);
	g_assert (profile1 != NULL);

	/* wait for daemon */
	_g_test_loop_run_with_timeout (50);
	_g_test_loop_quit ();

	/* get new number of profiles */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 1);
	profile_tmp = CD_PROFILE (g_ptr_array_index (array, 0));
	g_assert (g_str_has_prefix (cd_profile_get_object_path (profile_tmp),
				    "/org/freedesktop/ColorManager/profiles/profile1"));
	g_ptr_array_unref (array);

	/* create older profile */
	profile2 = cd_client_create_profile_sync (client,
						  "profile2",
						  CD_OBJECT_SCOPE_TEMP,
						  NULL,
						  NULL,
						  &error);
	g_assert_no_error (error);
	g_assert (profile2 != NULL);

	/* wait for daemon */
	_g_test_loop_run_with_timeout (50);
	_g_test_loop_quit ();

	/* get new number of profiles */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 2);
	profile_tmp = CD_PROFILE (g_ptr_array_index (array, 0));
	g_assert (g_str_has_prefix (cd_profile_get_object_path (profile_tmp),
				    "/org/freedesktop/ColorManager/profiles/profile1"));
	profile_tmp = CD_PROFILE (g_ptr_array_index (array, 1));
	g_assert (g_str_has_prefix (cd_profile_get_object_path (profile_tmp),
				    "/org/freedesktop/ColorManager/profiles/profile2"));
	g_ptr_array_unref (array);

	g_hash_table_unref (device_props);
	g_free (device_id);
	g_object_unref (profile1);
	g_object_unref (profile2);
	g_object_unref (device);
	g_object_unref (client);
}

/* ensure duplicate profiles have the correct error code */
static void
colord_profile_duplicate_func (void)
{
	CdClient *client;
	CdProfile *profile1;
	CdProfile *profile2;
	gboolean ret;
	gchar full_path[PATH_MAX];
	gchar *tmp;
	GError *error = NULL;
	GHashTable *profile_props;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	/* create */
	client = cd_client_new ();
	g_assert (client != NULL);

	/* create extra profile */
	tmp = realpath (TESTDATADIR "/ibm-t61.icc", full_path);
	g_assert (tmp != NULL);
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup ("Filename"),
			     g_strdup (full_path));

	/* create profile */
	profile2 = cd_client_create_profile_sync (client,
						  "profile_duplicate",
						  CD_OBJECT_SCOPE_TEMP,
						  profile_props,
						  NULL,
						  &error);
	g_assert_no_error (error);
	g_assert (profile2 != NULL);

	/* create same profile */
	profile1 = cd_client_create_profile_sync (client,
						  "profile_duplicate",
						  CD_OBJECT_SCOPE_TEMP,
						  profile_props,
						  NULL,
						  &error);
	g_assert_error (error,
			CD_CLIENT_ERROR,
			CD_CLIENT_ERROR_ALREADY_EXISTS);
	g_assert (profile1 == NULL);
	g_clear_error (&error);

	/* delete profile */
	ret = cd_client_delete_profile_sync (client,
					     profile2,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_hash_table_unref (profile_props);
	g_object_unref (profile2);
	g_object_unref (client);
}

static void
colord_device_duplicate_func (void)
{
	CdClient *client;
	CdDevice *device1;
	CdDevice *device2;
	gchar *device_id;
	GError *error = NULL;
	GHashTable *device_props;

	/* no running colord to use */
	if (!has_colord_process) {
		g_print ("[DISABLED] ");
		return;
	}

	/* create */
	client = cd_client_new ();
	g_assert (client != NULL);

	/* create device */
	device_id = colord_get_random_device_id ();
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	device1 = cd_client_create_device_sync (client,
						device_id,
						CD_OBJECT_SCOPE_TEMP,
						device_props,
						NULL,
						&error);
	g_assert_no_error (error);
	g_assert (device1 != NULL);

	/* create duplicate device */
	device2 = cd_client_create_device_sync (client,
						device_id,
						CD_OBJECT_SCOPE_TEMP,
						device_props,
						NULL,
						&error);
	g_assert_error (error,
			CD_CLIENT_ERROR,
			CD_CLIENT_ERROR_ALREADY_EXISTS);
	g_assert (device2 == NULL);
	g_clear_error (&error);

	g_hash_table_unref (device_props);
	g_free (device_id);
	g_object_unref (device1);
	g_object_unref (client);
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
	filename = _g_test_realpath (TESTDATADIR "/ibm-t61.icc");
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
	filename = _g_test_realpath (TESTDATADIR "/ibm-t61.icc");
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
	cd_icc_set_version (icc, 4.09);
	cd_icc_set_colorspace (icc, CD_COLORSPACE_XYZ);
	cd_icc_set_kind (icc, CD_PROFILE_KIND_OUTPUT_DEVICE);
	cd_icc_add_metadata (icc, "SelfTest", "true");
	cd_icc_remove_metadata (icc, "EDID_md5");
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
				CD_ICC_LOAD_FLAGS_METADATA,
				NULL,
				&error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);

	/* verify changed values */
	g_assert_cmpfloat (cd_icc_get_version (icc), ==, 4.09);
	g_assert_cmpint (cd_icc_get_kind (icc), ==, CD_PROFILE_KIND_OUTPUT_DEVICE);
	g_assert_cmpint (cd_icc_get_colorspace (icc), ==, CD_COLORSPACE_XYZ);
	g_assert_cmpstr (cd_icc_get_metadata_item (icc, "SelfTest"), ==, "true");
	g_assert_cmpstr (cd_icc_get_metadata_item (icc, "EDID_md5"), ==, NULL);
	str = cd_icc_get_description (icc, "fr.UTF-8", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (str, ==, "Couleurs crayon");

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
	filename = _g_test_realpath (TESTDATADIR "/crayons.icc");
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
	CdTransform *transform;
	gboolean ret;
	GError *error = NULL;
	guint8 data_in[3] = { 127, 32, 64 };
	guint8 data_out[3];
	CdIcc *icc;
	gchar *filename;
	GFile *file;

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

	filename = _g_test_realpath (TESTDATADIR "/ibm-t61.icc");
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

	g_object_unref (transform);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func ("/colord/transform", colord_transform_func);
	g_test_add_func ("/colord/icc", colord_icc_func);
	g_test_add_func ("/colord/icc{localized}", colord_icc_localized_func);
	g_test_add_func ("/colord/icc{edid}", colord_icc_edid_func);
	g_test_add_func ("/colord/icc{save}", colord_icc_save_func);
	g_test_add_func ("/colord/buffer", colord_buffer_func);
	g_test_add_func ("/colord/enum", colord_enum_func);
	g_test_add_func ("/colord/dom", colord_dom_func);
	g_test_add_func ("/colord/dom{color}", colord_dom_color_func);
	g_test_add_func ("/colord/dom{localized}", colord_dom_localized_func);
	g_test_add_func ("/colord/interp{linear}", colord_interp_linear_func);
	g_test_add_func ("/colord/interp{akima}", colord_interp_akima_func);
	g_test_add_func ("/colord/color", colord_color_func);
	g_test_add_func ("/colord/color{interpolate}", colord_color_interpolate_func);
	g_test_add_func ("/colord/math", cd_test_math_func);
	g_test_add_func ("/colord/it8{raw}", colord_it8_raw_func);
	g_test_add_func ("/colord/it8{locale}", colord_it8_locale_func);
	g_test_add_func ("/colord/it8{normalized}", colord_it8_normalized_func);
	g_test_add_func ("/colord/it8{ccmx}", colord_it8_ccmx_func);
	g_test_add_func ("/colord/it8{ccmx-util", colord_it8_ccmx_util_func);
	g_test_add_func ("/colord/client", colord_client_func);
	g_test_add_func ("/colord/device", colord_device_func);
	g_test_add_func ("/colord/device{embedded}", colord_device_embedded_func);
	g_test_add_func ("/colord/device{invalid-kind}", colord_device_invalid_kind_func);
	g_test_add_func ("/colord/device{duplicate}", colord_device_duplicate_func);
	g_test_add_func ("/colord/device{seat}", colord_device_seat_func);
	g_test_add_func ("/colord/device{enabled}", colord_device_enabled_func);
	g_test_add_func ("/colord/device{invalid}", colord_device_invalid_func);
	g_test_add_func ("/colord/device{qualifiers}", colord_device_qualifiers_func);
	g_test_add_func ("/colord/profile{metadata}", colord_icc_meta_dict_func);
	g_test_add_func ("/colord/profile{file}", colord_profile_file_func);
	g_test_add_func ("/colord/profile{device-id-mapping, p->d}", colord_device_id_mapping_pd_func);
	g_test_add_func ("/colord/profile{device-id-mapping, d->p}", colord_device_id_mapping_dp_func);
	g_test_add_func ("/colord/profile{ordering}", colord_profile_ordering_func);
	g_test_add_func ("/colord/profile{duplicate}", colord_profile_duplicate_func);
	g_test_add_func ("/colord/device{mapping}", colord_device_mapping_func);
	g_test_add_func ("/colord/sensor", colord_sensor_func);
	g_test_add_func ("/colord/device{modified}", colord_device_modified_func);
	g_test_add_func ("/colord/client{standard-space}", colord_client_standard_space_func);
	g_test_add_func ("/colord/client{async}", colord_client_async_func);
	g_test_add_func ("/colord/device{async}", colord_device_async_func);
	if (g_test_thorough ())
		g_test_add_func ("/colord/client{systemwide}", colord_client_systemwide_func);
	g_test_add_func ("/colord/client{fd-pass}", colord_client_fd_pass_func);
	g_test_add_func ("/colord/client{import}", colord_client_import_func);
	return g_test_run ();
}

