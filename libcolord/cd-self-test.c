/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

#include <glib.h>
#include <glib-object.h>

#include "cd-client.h"
#include "cd-client-sync.h"
#include "cd-color.h"
#include "cd-device.h"
#include "cd-device-sync.h"
#include "cd-math.h"
#include "cd-profile.h"
#include "cd-profile-sync.h"
#include "cd-sensor.h"
#include "cd-sensor-sync.h"
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
	return FALSE;
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
	gchar *full;
	char full_tmp[PATH_MAX];
	realpath (relpath, full_tmp);
	full = g_strdup (full_tmp);
	return full;
}

/**********************************************************************/

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
colord_client_random_func (void)
{
	CdClient *client;
	CdDevice *device;
	CdDeviceRelation relation;
	CdProfile *profile;
	CdProfile *profile2;
	CdProfile *profile_tmp;
	gboolean ret;
	gchar *device_id;
	gchar *device_path;
	gchar *filename;
	gchar *profile2_id;
	gchar *profile2_path;
	gchar *profile_id;
	gchar *profile_path;
	GError *error = NULL;
	GHashTable *device_props;
	GHashTable *profile_props;
	GPtrArray *array;
	GPtrArray *devices;
	GPtrArray *profiles;
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
	device_path = g_strdup_printf ("/org/freedesktop/ColorManager/devices/device_self_test_%04x", key);

	/* create */
	client = cd_client_new ();
	g_assert (client != NULL);

	/* connect */
	ret = cd_client_connect_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get number of devices */
	devices = cd_client_get_devices_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (devices != NULL);

	/* get number of profiles */
	profiles = cd_client_get_profiles_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (profiles != NULL);

	/* create device */
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
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
	g_assert_cmpstr (cd_device_get_object_path (device), ==,
			 device_path);

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

	/* create profile */
	profile = cd_client_create_profile_sync (client,
						 profile_id,
						 CD_OBJECT_SCOPE_TEMP,
						 NULL,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);

	/* connect */
	ret = cd_profile_connect_sync (profile, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (cd_profile_get_object_path (profile), ==,
			 profile_path);
	g_assert_cmpstr (cd_profile_get_id (profile), ==, profile_id);
	g_assert (!cd_profile_get_is_system_wide (profile));

	/* create extra profile */
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup ("Qualifier"),
			     g_strdup ("RGB.Glossy.1200dpi"));
	profile2 = cd_client_create_profile_sync (client,
						  profile2_id,
						  CD_OBJECT_SCOPE_TEMP,
						  profile_props,
						  NULL,
						  &error);
	g_assert_no_error (error);
	g_assert (profile2 != NULL);
	g_assert_cmpstr (cd_profile_get_object_path (profile2), ==,
			 profile2_path);

	/* connect */
	ret = cd_profile_connect_sync (profile2, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (cd_profile_get_id (profile2), ==, profile2_id);
	g_assert_cmpstr (cd_profile_get_format (profile2), ==, NULL);
	g_assert_cmpstr (cd_profile_get_qualifier (profile2), ==, "RGB.Glossy.1200dpi");

	/* get new number of profiles */
	array = cd_client_get_profiles_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (array != NULL);
	g_assert_cmpint (profiles->len + 2, ==, array->len);
	g_ptr_array_unref (array);

	/* set profile filename */
	filename = _g_test_realpath (TESTDATADIR "/ibm-t61.icc");
	ret = cd_profile_set_filename_sync (profile, filename, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* wait for daemon */
	_g_test_loop_run_with_timeout (50);
	_g_test_loop_quit ();

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

	/* set profile qualifier */
	ret = cd_profile_set_qualifier_sync (profile, "RGB.Glossy.300dpi",
					     NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* set profile qualifier */
	ret = cd_profile_set_qualifier_sync (profile2, "RGB.Matte.300dpi",
					     NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* wait for daemon */
	_g_test_loop_run_with_timeout (50);
	_g_test_loop_quit ();

	/* check profile kind */
	g_assert_cmpint (cd_profile_get_kind (profile), ==,
			 CD_PROFILE_KIND_DISPLAY_DEVICE);

	/* check profile age */
	g_assert_cmpuint (cd_profile_get_created (profile), ==,
			  1261606846);

	/* check profile filename */
	g_assert (g_str_has_suffix (cd_profile_get_filename (profile),
				    "data/tests/ibm-t61.icc"));

	/* check profile qualifier */
	g_assert_cmpstr (cd_profile_get_qualifier (profile), ==, "RGB.Glossy.300dpi");

	/* check profile title set from ICC profile */
	g_assert_cmpstr (cd_profile_get_title (profile), ==, "Huey, LENOVO - 6464Y1H - 15\" (2009-12-23)");

	/* check none assigned */
	array = cd_device_get_profiles (device);
	g_assert_cmpint (array->len, ==, 0);
	g_ptr_array_unref (array);

	/* check nothing matches qualifier */
	profile_tmp = cd_device_get_profile_for_qualifiers_sync (device,
								 qualifier1,
								 NULL,
								 &error);
	g_assert_error (error, CD_DEVICE_ERROR, CD_DEVICE_ERROR_FAILED);
	g_assert (profile_tmp == NULL);
	g_clear_error (&error);

	/* check there is no relation */
	relation = cd_device_get_profile_relation_sync (device,
						   profile,
						   NULL,
						   &error);
	g_assert_error (error, CD_DEVICE_ERROR, CD_DEVICE_ERROR_FAILED);
	g_assert (relation == CD_DEVICE_RELATION_UNKNOWN);
	g_clear_error (&error);

	/* assign profile to device */
	ret = cd_device_add_profile_sync (device, CD_DEVICE_RELATION_SOFT, profile, NULL, &error);
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
	ret = cd_device_add_profile_sync (device, CD_DEVICE_RELATION_HARD, profile2, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* connect */
	ret = cd_device_connect_sync (device, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check profile assigned */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
//	g_assert_cmpint (array->len, ==, 2);
	profile_tmp = g_ptr_array_index (array, 0);

	/* connect */
	ret = cd_profile_connect_sync (profile_tmp, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (cd_profile_get_qualifier (profile_tmp), ==, "RGB.Matte.300dpi");
	g_ptr_array_unref (array);

	/* make profile default */
	ret = cd_device_make_profile_default_sync (device, profile, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* wait for daemon */
	_g_test_loop_run_with_timeout (50);
	_g_test_loop_quit ();

	/* ensure profile is default */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 2);
	profile_tmp = g_ptr_array_index (array, 0);

	/* connect */
	ret = cd_profile_connect_sync (profile_tmp, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (cd_profile_get_id (profile_tmp), ==, profile_id);
	g_ptr_array_unref (array);

	/* make extra profile default */
	ret = cd_device_make_profile_default_sync (device, profile2, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* wait for daemon */
	_g_test_loop_run_with_timeout (50);
	_g_test_loop_quit ();

	/* ensure profile is default */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 2);
	profile_tmp = g_ptr_array_index (array, 0);

	/* connect */
	ret = cd_profile_connect_sync (profile_tmp, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (cd_profile_get_id (profile_tmp), ==, profile2_id);
	g_ptr_array_unref (array);

	/* check matches exact qualifier */
	profile_tmp = cd_device_get_profile_for_qualifiers_sync (device,
								 qualifier1,
								 NULL,
								 &error);
	g_assert_no_error (error);
	g_assert (profile_tmp != NULL);
	g_assert_cmpstr (cd_profile_get_object_path (profile), ==,
			 profile_path);
	g_object_unref (profile_tmp);

	/* check matches wildcarded qualifier */
	profile_tmp = cd_device_get_profile_for_qualifiers_sync (device,
								 qualifier2,
								 NULL,
								 &error);
	g_assert_no_error (error);
	g_assert (profile_tmp != NULL);
	g_assert_cmpstr (cd_profile_get_object_path (profile_tmp), ==,
			 profile_path);
	g_object_unref (profile_tmp);

	/* check hard profiles beat soft profiles */
	profile_tmp = cd_device_get_profile_for_qualifiers_sync (device,
								 qualifier3,
								 NULL,
								 &error);
	g_assert_no_error (error);
	g_assert (profile_tmp != NULL);
	g_assert_cmpstr (cd_profile_get_object_path (profile_tmp), ==,
			 profile2_path);
	g_object_unref (profile_tmp);

	/* uninhibit device (should fail) */
	ret = cd_device_profiling_uninhibit_sync (device,
						  NULL,
						  &error);
	g_assert_error (error, CD_DEVICE_ERROR, CD_DEVICE_ERROR_FAILED);
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
	g_assert_error (error, CD_DEVICE_ERROR, CD_DEVICE_ERROR_FAILED);
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

	/* get new number of profiles */
	array = cd_client_get_profiles_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (array != NULL);
	g_assert_cmpint (profiles->len, ==, array->len);
	g_ptr_array_unref (array);

	/* wait for daemon */
	_g_test_loop_run_with_timeout (50);
	_g_test_loop_quit ();

	/* ensure device no longer lists deleted profile */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 0);
	g_ptr_array_unref (array);

	/* add back profile, and ensure it's automatically added back
	 * to the device thanks to the db */
	g_object_unref (profile);
	profile = cd_client_create_profile_sync (client,
						 profile2_id,
						 CD_OBJECT_SCOPE_TEMP,
						 NULL,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);

	/* wait for daemon */
	_g_test_loop_run_with_timeout (50);
	_g_test_loop_quit ();

	/* ensure device has profile auto-added */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 1);
	g_ptr_array_unref (array);

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

	/* get new number of devices */
	array = cd_client_get_devices_sync (client, NULL, &error);
	g_assert_no_error (error);
	g_assert (array != NULL);
	g_assert_cmpint (devices->len, ==, array->len);
	g_ptr_array_unref (array);

	/* create profile then device and check profiles are
	 * added to the device */
	g_object_unref (profile);
	profile = cd_client_create_profile_sync (client,
						 profile2_id,
						 CD_OBJECT_SCOPE_TEMP,
						 NULL,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);

	/* create device */
	g_object_unref (device);
	device = cd_client_create_device_sync (client,
					       device_id,
					       CD_OBJECT_SCOPE_TEMP,
					       NULL,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);

	/* connect */
	ret = cd_device_connect_sync (device, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure device has profiles added */
	array = cd_device_get_profiles (device);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 1);
	g_ptr_array_unref (array);

	g_free (profile_id);
	g_free (profile2_id);
	g_free (device_id);
	g_free (profile_path);
	g_free (profile2_path);
	g_free (device_path);
	g_free (filename);
	g_ptr_array_unref (devices);
	g_ptr_array_unref (profiles);
	g_hash_table_unref (profile_props);
	g_hash_table_unref (device_props);
	g_object_unref (device);
	g_object_unref (profile);
	g_object_unref (profile2);
	g_object_unref (client);
}

static void
colord_icc_meta_dict_func (void)
{
	gchar *filename;
	gboolean ret;
	GError *error = NULL;
	GHashTable *metadata;
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
	profile = cd_client_create_profile_sync (client,
						 "profile_metadata_test",
						 CD_OBJECT_SCOPE_TEMP,
						 NULL,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);

	/* connect */
	ret = cd_profile_connect_sync (profile, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* set profile filename */
	filename = _g_test_realpath (TESTDATADIR "/ibm-t61.icc");
	ret = cd_profile_set_filename_sync (profile, filename, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check metadata */
	metadata = cd_profile_get_metadata (profile);
#ifdef HAVE_NEW_LCMS
	g_assert_cmpint (g_hash_table_size (metadata), ==, 1);
	g_assert_cmpstr (g_hash_table_lookup (metadata, "EDID_md5"), ==,
			 "f09e42aa86585d1bb6687d3c322ed0c1");
#else
	g_assert_cmpint (g_hash_table_size (metadata), ==, 0);
#endif
	g_hash_table_unref (metadata);

	/* create profile */
	ret = cd_client_delete_profile_sync (client,
					     profile,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);

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
	CdSensor *sensor;
	CdClient *client;
	gboolean ret;
	GError *error = NULL;
	GPtrArray *array;
	CdColorXYZ *values;
//	gdouble ambient = -2.0f;

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
	g_assert_error (error, CD_SENSOR_ERROR, CD_SENSOR_ERROR_FAILED);
	g_assert (!ret);

	_g_test_loop_run_with_timeout (5);
	_g_test_loop_quit ();
	g_assert (cd_sensor_get_locked (sensor));
	g_clear_error (&error);

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

	g_assert_cmpfloat (values->X - 0.1f, >, -0.01);
	g_assert_cmpfloat (values->X - 0.1f, <, 0.01);
	g_assert_cmpfloat (values->Y - 0.2f, >, -0.01);
	g_assert_cmpfloat (values->Y - 0.2f, <, 0.01);
	g_assert_cmpfloat (values->Z - 0.3f, >, -0.01);
	g_assert_cmpfloat (values->Z - 0.3f, <, 0.01);
//	g_assert_cmpfloat (ambient + 1.0f, >, -0.01);
//	g_assert_cmpfloat (ambient + 1.0f, <, 0.01);
	g_object_unref (values);

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
	g_assert_error (error, CD_SENSOR_ERROR, CD_SENSOR_ERROR_FAILED);
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
colord_color_func (void)
{
	CdColorXYZ *xyz;
	CdColorYxy yxy;

	xyz = cd_color_xyz_new ();
	g_assert (xyz != NULL);

	/* nothing set */
	cd_color_convert_xyz_to_yxy (xyz, &yxy);
	g_assert_cmpfloat (fabs (yxy.x - 0.0f), <, 0.001f);

	/* set dummy values */
	cd_color_set_xyz (xyz, 0.125, 0.25, 0.5);
	cd_color_convert_xyz_to_yxy (xyz, &yxy);

	g_assert_cmpfloat (fabs (yxy.x - 0.142857143f), <, 0.001f);
	g_assert_cmpfloat (fabs (yxy.y - 0.285714286f), <, 0.001f);

	cd_color_xyz_free (xyz);
}

static void
colord_client_func (void)
{
	CdClient *client;
	const gchar *version;
	gchar *version_str;
	gboolean ret;
	GError *error = NULL;

	client = cd_client_new ();

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

	/* is there a running colord instance? */
	has_colord_process = cd_client_get_has_server (client);

	version = cd_client_get_daemon_version (client);
	version_str = g_strdup_printf ("%i.%i.%i",
				       CD_MAJOR_VERSION,
				       CD_MINOR_VERSION,
				       CD_MICRO_VERSION);
	g_assert_cmpstr (version, ==, version_str);
	g_free (version_str);

	g_object_unref (client);
}

static void
colord_device_mapping_func (void)
{
	CdClient *client;
	gboolean ret;
	GError *error = NULL;
	CdDevice *device;
	CdProfile *profile1;
	CdProfile *profile2;
	CdProfile *profile_tmp;
	gint32 key;
	gchar *profile_id1;
	gchar *profile_id2;

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
	device = cd_client_create_device_sync (client,
					       "device_mapping",
					       CD_OBJECT_SCOPE_TEMP,
					       NULL,
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
	device = cd_client_create_device_sync (client,
					       "device_mapping",
					       CD_OBJECT_SCOPE_TEMP,
					       NULL,
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
	realpath (TESTDATADIR "/ibm-t61.icc", full_path);
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
	realpath (TESTDATADIR "/Makefile.am", full_path);
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
	realpath (TESTDATADIR "/ibm-t61.icc", full_path);
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
	g_assert_error (error, CD_CLIENT_ERROR, CD_CLIENT_ERROR_FAILED);
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
	gboolean ret;
	GError *error = NULL;
	CdClient *client;
	CdDevice *device;
	CdDevice *device_tmp;

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

	device = cd_client_create_device_sync (client,
					       "device_async_dave",
					       CD_OBJECT_SCOPE_TEMP,
					       NULL,
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
	device_tmp = cd_device_new_with_object_path ("/org/freedesktop/ColorManager/devices/device_async_dave");
	ret = cd_device_connect_sync (device_tmp, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = cd_device_set_model_sync (device_tmp, "Cray", NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (device_tmp);

	/* delete known device */
	device_tmp = cd_device_new_with_object_path ("/org/freedesktop/ColorManager/devices/device_async_dave");
	ret = cd_client_delete_device_sync (client, device_tmp, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (device_tmp);

	g_object_unref (client);
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
	realpath (TESTDATADIR "/ibm-t61.icc", full_path);
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
colord_device_func (void)
{
	CdDevice *device;
	gboolean ret;
	GError *error = NULL;

	/* create a device with an invlid object path */
	device = cd_device_new_with_object_path ("/garbage");
	g_assert (device != NULL);

	/* connect */
	ret = cd_device_connect_sync (device, NULL, &error);
	g_assert_error (error, CD_DEVICE_ERROR, CD_DEVICE_ERROR_FAILED);
	g_assert (!ret);
	g_clear_error (&error);

	g_object_unref (device);
}

static void
colord_device_modified_func (void)
{
	CdClient *client;
	CdDevice *device;
	GHashTable *device_props;
	CdProfile *profile;
	GHashTable *profile_props;
	gboolean ret;
	GError *error = NULL;
	gchar full_path[PATH_MAX];
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
			     g_strdup ("Vendor"),
			     g_strdup ("Hewlett-Packard Ltd."));
	g_hash_table_insert (device_props,
			     g_strdup ("Model"),
			     g_strdup ("3000"));
	g_hash_table_insert (device_props,
			     g_strdup ("XRANDR_name"),
			     g_strdup ("lvds1"));
	device = cd_client_create_device_sync (client,
					       "device_dave",
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	g_assert_no_error (error);
	g_assert (device != NULL);
	g_assert_cmpstr (cd_device_get_object_path (device), ==,
			 "/org/freedesktop/ColorManager/devices/device_dave");

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
	realpath (TESTDATADIR "/ibm-t61.icc", full_path);
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

/* when we re-add profiles, ensure they are sorted so the newest
 * assigned profile is first, not the newest-added */
static void
colord_profile_ordering_func (void)
{
	CdClient *client;
	CdDevice *device;
	CdProfile *profile_tmp;
	CdProfile *profile1;
	CdProfile *profile2;
	gboolean ret;
	GError *error = NULL;
	GPtrArray *array;
	gchar *device_id;

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
	device = cd_client_create_device_sync (client,
					       device_id,
					       CD_OBJECT_SCOPE_TEMP,
					       NULL,
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
	g_assert_cmpstr (cd_profile_get_object_path (profile_tmp),
			 ==,
			 "/org/freedesktop/ColorManager/profiles/profile1");
	profile_tmp = CD_PROFILE (g_ptr_array_index (array, 1));
	g_assert_cmpstr (cd_profile_get_object_path (profile_tmp),
			 ==,
			 "/org/freedesktop/ColorManager/profiles/profile2");
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
	g_assert_cmpstr (cd_profile_get_object_path (profile_tmp),
			 ==,
			 "/org/freedesktop/ColorManager/profiles/profile1");
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
	g_assert_cmpstr (cd_profile_get_object_path (profile_tmp),
			 ==,
			 "/org/freedesktop/ColorManager/profiles/profile1");
	profile_tmp = CD_PROFILE (g_ptr_array_index (array, 1));
	g_assert_cmpstr (cd_profile_get_object_path (profile_tmp),
			 ==,
			 "/org/freedesktop/ColorManager/profiles/profile2");
	g_ptr_array_unref (array);

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
	realpath (TESTDATADIR "/ibm-t61.icc", full_path);
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
	GError *error = NULL;
	gchar *device_id;

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
	device1 = cd_client_create_device_sync (client,
						device_id,
						CD_OBJECT_SCOPE_TEMP,
						NULL,
						NULL,
						&error);
	g_assert_no_error (error);
	g_assert (device1 != NULL);

	/* create duplicate device */
	device2 = cd_client_create_device_sync (client,
						device_id,
						CD_OBJECT_SCOPE_TEMP,
						NULL,
						NULL,
						&error);
	g_assert_error (error,
			CD_CLIENT_ERROR,
			CD_CLIENT_ERROR_ALREADY_EXISTS);
	g_assert (device2 == NULL);
	g_clear_error (&error);

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

int
main (int argc, char **argv)
{
	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func ("/colord/color", colord_color_func);
	g_test_add_func ("/colord/math", cd_test_math_func);
	g_test_add_func ("/colord/device", colord_device_func);
	g_test_add_func ("/colord/client", colord_client_func);
	g_test_add_func ("/colord/device-duplicate", colord_device_duplicate_func);
	g_test_add_func ("/colord/profile-metadata", colord_icc_meta_dict_func);
	g_test_add_func ("/colord/profile-ordering", colord_profile_ordering_func);
	g_test_add_func ("/colord/profile-duplicate", colord_profile_duplicate_func);
	g_test_add_func ("/colord/device-mapping", colord_device_mapping_func);
	g_test_add_func ("/colord/client-random", colord_client_random_func);
	g_test_add_func ("/colord/sensor", colord_sensor_func);
	g_test_add_func ("/colord/device-modified", colord_device_modified_func);
	g_test_add_func ("/colord/client-async", colord_client_async_func);
	g_test_add_func ("/colord/device-async", colord_device_async_func);
	if (g_test_thorough ())
		g_test_add_func ("/colord/client-systemwide", colord_client_systemwide_func);
	g_test_add_func ("/colord/client-fd-pass", colord_client_fd_pass_func);
	g_test_add_func ("/colord/client-import", colord_client_import_func);
	return g_test_run ();
}

