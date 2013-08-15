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

#include <locale.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <pwd.h>

#include "cd-client.h"
#include "cd-client-sync.h"
#include "cd-device.h"
#include "cd-device-sync.h"
#include "cd-profile.h"
#include "cd-profile-sync.h"
#include "cd-sensor.h"
#include "cd-sensor-sync.h"

#include "cd-test-shared.h"

static gboolean has_colord_process = FALSE;

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
	cd_test_loop_quit ();
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
	gchar *filename;
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
	filename = cd_test_get_filename ("ibm-t61.icc");
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_PROPERTY_FILENAME),
			     g_strdup (filename));
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
			     g_strdup (CD_PROFILE_PROPERTY_FILENAME),
			     g_strdup (filename));
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
	g_free (filename);

	/* wait for daemon */
	cd_test_loop_run_with_timeout (50);
	cd_test_loop_quit ();

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
	gchar *basename;
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
	filename = cd_test_get_filename ("ibm-t61.icc");
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

	/* check we can find profile based on basename */
	basename = g_path_get_basename (filename);
	profile_tmp = cd_client_find_profile_by_filename_sync (client,
							       basename,
							       NULL,
							       &error);
	g_assert_no_error (error);
	g_assert (profile_tmp != NULL);
	g_object_unref (profile_tmp);

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
				    "ibm-t61.icc"));

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
	g_free (basename);
	g_free (filename);
	g_hash_table_unref (profile_props);
	g_object_unref (profile);
	g_object_unref (client);
}

/*
 * Create profile with metadata MAPPING_device_id
 * Create device with id matching the profile MD
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
	gchar *filename;
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
	filename = cd_test_get_filename ("ibm-t61.icc");
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_METADATA_MAPPING_DEVICE_ID),
			     g_strdup (device_id));
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_PROPERTY_FILENAME),
			     g_strdup (filename));
	profile = cd_client_create_profile_sync (client,
						 "profile_md_test1_id",
						 CD_OBJECT_SCOPE_TEMP,
						 profile_props,
						 NULL,
						 &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);
	g_free (filename);

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
 * 1. Add soft profile with MAPPING_device_id=foo, DATA_source=calib
 * 2. Add soft profile with MAPPING_device_id=foo, DATA_source=edid
 *
 * We should prefer the calibration profile over the EDID profile every time
 */
static void
colord_device_id_mapping_edid_func (void)
{
	CdClient *client;
	CdDevice *device;
	CdProfile *profile_calib;
	CdProfile *profile_edid;
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

	/* create a device */
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

	/* connect to device */
	ret = cd_device_connect_sync (device,
				      NULL,
				      &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create calibration profile that matches device */
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_METADATA_MAPPING_DEVICE_ID),
			     g_strdup (device_id));
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_METADATA_DATA_SOURCE),
			     g_strdup (CD_PROFILE_METADATA_DATA_SOURCE_CALIB));
	profile_calib = cd_client_create_profile_sync (client,
						       "profile_calib",
						       CD_OBJECT_SCOPE_TEMP,
						       profile_props,
						       NULL,
						       &error);
	g_hash_table_unref (profile_props);
	g_assert_no_error (error);
	g_assert (profile_calib != NULL);

	/* create EDID profile that matches device */
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_METADATA_MAPPING_DEVICE_ID),
			     g_strdup (device_id));
	g_hash_table_insert (profile_props,
			     g_strdup (CD_PROFILE_METADATA_DATA_SOURCE),
			     g_strdup (CD_PROFILE_METADATA_DATA_SOURCE_EDID));
	profile_edid = cd_client_create_profile_sync (client,
						      "profile_edid",
						      CD_OBJECT_SCOPE_TEMP,
						      profile_props,
						      NULL,
						      &error);
	g_hash_table_unref (profile_props);
	g_assert_no_error (error);
	g_assert (profile_edid != NULL);

	/* connect */
	ret = cd_profile_connect_sync (profile_calib, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure it's the *calibration* profile not the *edid* profile */
	profile_on_device = cd_device_get_default_profile (device);
	g_assert_cmpstr (cd_profile_get_object_path (profile_on_device), ==,
			 cd_profile_get_object_path (profile_calib));
	g_object_unref (profile_on_device);

	/* delete device */
	ret = cd_client_delete_device_sync (client,
					    device,
					    NULL,
					    &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* delete profiles */
	ret = cd_client_delete_profile_sync (client,
					     profile_calib,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = cd_client_delete_profile_sync (client,
					     profile_edid,
					     NULL,
					     &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_free (device_id);
	g_object_unref (profile_calib);
	g_object_unref (profile_edid);
	g_object_unref (device);
	g_object_unref (client);
}

/*
 * Create device with known id
 * Create profile with metadata MAPPING_device_id of the same ID
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

	/* create a device */
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
			     g_strdup (device_id));
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

	g_free (device_id);
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
	filename = cd_test_get_filename ("ibm-t61.icc");
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
	cd_test_loop_quit ();
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
	cd_test_loop_run_with_timeout (5000);
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

	cd_test_loop_run_with_timeout (5);
	cd_test_loop_quit ();
	g_assert (cd_sensor_get_locked (sensor));

	/* lock again */
	ret = cd_sensor_lock_sync (sensor,
				   NULL,
				   &error);
	g_assert_error (error, CD_SENSOR_ERROR, CD_SENSOR_ERROR_ALREADY_LOCKED);
	g_assert (!ret);

	cd_test_loop_run_with_timeout (5);
	cd_test_loop_quit ();
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
	cd_test_loop_run_with_timeout (5);
	cd_test_loop_quit ();
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

	cd_test_loop_run_with_timeout (5);
	cd_test_loop_quit ();
	g_assert (!cd_sensor_get_locked (sensor));

	/* lock again */
	ret = cd_sensor_unlock_sync (sensor,
				     NULL,
				     &error);
	g_assert_error (error, CD_SENSOR_ERROR, CD_SENSOR_ERROR_NOT_LOCKED);
	g_assert (!ret);

	cd_test_loop_run_with_timeout (5);
	cd_test_loop_quit ();
	g_assert (!cd_sensor_get_locked (sensor));
	g_clear_error (&error);
out:
	g_ptr_array_unref (array);
	g_object_unref (client);
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
	gchar *filename;

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
	filename = cd_test_get_filename ("ibm-t61.icc");
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup ("Filename"),
			     g_strdup (filename));
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
	g_free (filename);
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
	gchar *dest_path;
	gchar *filename;

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
	filename = cd_test_get_filename ("raw.ti3");
	invalid_file = g_file_new_for_path (filename);
	profile2 = cd_client_import_profile_sync (client,
						  invalid_file,
						  NULL,
						  &error);
	g_assert_error (error,
			CD_CLIENT_ERROR,
			CD_CLIENT_ERROR_FILE_INVALID);
	g_assert (profile2 == NULL);
	g_clear_error (&error);
	g_free (filename);

	/* create extra profile */
	filename = cd_test_get_filename ("ibm-t61.icc");
	file = g_file_new_for_path (filename);

	/* ensure it's deleted */
	dest = colord_get_profile_destination (file);
	if (g_file_query_exists (dest, NULL)) {
		ret = g_file_delete (dest, NULL, &error);
		g_assert_no_error (error);
		g_assert (ret);

		/* wait for daemon to DTRT */
		cd_test_loop_run_with_timeout (2000);
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
	g_free (filename);
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

	cd_test_loop_quit ();
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

	cd_test_loop_quit ();
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
	cd_test_loop_run_with_timeout (1500);
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
	cd_test_loop_run_with_timeout (1500);
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

	cd_test_loop_quit ();
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

	cd_test_loop_run_with_timeout (1500);
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
	gchar *filename;

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
	filename = cd_test_get_filename ("ibm-t61.icc");
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup ("Filename"),
			     g_strdup (filename));
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
	g_free (filename);
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
	cd_test_loop_run_with_timeout (5000);

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
	cd_test_loop_run_with_timeout (50);
	cd_test_loop_quit ();

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
	cd_test_loop_run_with_timeout (50);
	cd_test_loop_quit ();

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
	cd_test_loop_run_with_timeout (50);
	cd_test_loop_quit ();

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
	cd_test_loop_run_with_timeout (50);
	cd_test_loop_quit ();

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
	cd_test_loop_run_with_timeout (50);
	cd_test_loop_quit ();

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
	cd_test_loop_run_with_timeout (50);
	cd_test_loop_quit ();

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
	gchar *filename;
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
	filename = cd_test_get_filename ("ibm-t61.icc");
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup ("Filename"),
			     g_strdup (filename));

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
	g_free (filename);
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

int
main (int argc, char **argv)
{
	gint retval;

	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
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
	g_test_add_func ("/colord/profile{device-id-mapping, edid}", colord_device_id_mapping_edid_func);
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

	/* run the tests */
	retval = g_test_run ();

	/* if there is no daemon, mark these tests as skipped */
	if (!has_colord_process)
		retval = 77;

	return retval;
}
