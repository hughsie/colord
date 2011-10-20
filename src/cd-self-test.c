/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

#include <limits.h>
#include <stdlib.h>

#include <glib.h>
#include <glib-object.h>

#include "cd-common.h"
#include "cd-device-array.h"
#include "cd-device-db.h"
#include "cd-device.h"
#include "cd-mapping-db.h"
#include "cd-profile-array.h"
#include "cd-profile.h"

static void
colord_profile_func (void)
{
	CdProfile *profile;

	profile = cd_profile_new ();
	g_assert (profile != NULL);

	cd_profile_set_id (profile, "dave");
	g_assert_cmpstr (cd_profile_get_id (profile), ==, "dave");

	g_object_unref (profile);
}

static void
colord_device_func (void)
{
	CdDevice *device;
	CdProfile *profile;
	CdProfileArray *profile_array;
	gboolean ret;
	GError *error = NULL;

	profile_array = cd_profile_array_new ();
	device = cd_device_new ();
	g_assert (device != NULL);

	cd_device_set_id (device, "dave");
	g_assert_cmpstr (cd_device_get_id (device), ==, "dave");

	profile = cd_profile_new ();
	cd_profile_set_id (profile, "dave");
	cd_profile_array_add (profile_array, profile);

	/* add profile */
	ret = cd_device_add_profile (device,
				     CD_DEVICE_RELATION_SOFT,
				     cd_profile_get_object_path (profile),
				     0,
				     &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* add profile again */
	ret = cd_device_add_profile (device,
				     CD_DEVICE_RELATION_HARD,
				     cd_profile_get_object_path (profile),
				     0,
				     &error);
	g_assert_error (error, CD_MAIN_ERROR, CD_MAIN_ERROR_FAILED);
	g_assert (!ret);
	g_clear_error (&error);

	/* add profile that does not exist */
	ret = cd_device_add_profile (device,
				     CD_DEVICE_RELATION_SOFT,
				     "/dave",
				     0,
				     &error);
	g_assert_error (error, CD_MAIN_ERROR, CD_MAIN_ERROR_FAILED);
	g_assert (!ret);
	g_clear_error (&error);

	g_object_unref (device);
	g_object_unref (profile);
	g_object_unref (profile_array);
}

static void
colord_device_array_func (void)
{
	CdDevice *device;
	CdDeviceArray *device_array;

	device_array = cd_device_array_new ();
	g_assert (device_array != NULL);

	device = cd_device_new ();
	cd_device_set_id (device, "dave");
	cd_device_array_add (device_array, device);
	g_object_unref (device);

	device = cd_device_array_get_by_id (device_array, "does not exist");
	g_assert (device == NULL);

	device = cd_device_array_get_by_id (device_array, "dave");
	g_assert (device != NULL);
	g_assert_cmpstr (cd_device_get_id (device), ==, "dave");
	g_object_unref (device);

	device = cd_device_array_get_by_object_path (device_array,
						     "/org/freedesktop/ColorManager/devices/dave");
	g_assert (device != NULL);
	g_assert_cmpstr (cd_device_get_id (device), ==, "dave");
	g_object_unref (device);

	g_object_unref (device_array);
}

static void
cd_mapping_db_func (void)
{
	CdMappingDb *mdb;
	GError *error = NULL;
	gboolean ret;
	GPtrArray *array;

	/* create */
	mdb = cd_mapping_db_new ();
	g_assert (mdb != NULL);

	/* connect, which should create it for us */
	ret = cd_mapping_db_load (mdb, "/tmp/mapping.db", &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure empty */
	ret = cd_mapping_db_empty (mdb, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* add a few entries */
	ret = cd_mapping_db_add (mdb, "device1", "profile1", &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = cd_mapping_db_add (mdb, "device1", "profile2", &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = cd_mapping_db_add (mdb, "device1", "profile3", &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* remove one */
	ret = cd_mapping_db_remove (mdb, "device1", "profile2", &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get the profiles for a device */
	array = cd_mapping_db_get_profiles (mdb, "device1", &error);
	g_assert_no_error (error);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 2);
	g_assert_cmpstr (g_ptr_array_index (array, 0), ==, "profile1");
	g_assert_cmpstr (g_ptr_array_index (array, 1), ==, "profile3");
	g_ptr_array_unref (array);

	/* get the devices for a profile */
	array = cd_mapping_db_get_devices (mdb, "profile1", &error);
	g_assert_no_error (error);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 1);
	g_assert_cmpstr (g_ptr_array_index (array, 0), ==, "device1");
	g_ptr_array_unref (array);

	g_object_unref (mdb);
}

static void
cd_device_db_func (void)
{
	CdDeviceDb *ddb;
	GError *error = NULL;
	gboolean ret;
	GPtrArray *array;
	gchar *value;

	/* create */
	ddb = cd_device_db_new ();
	g_assert (ddb != NULL);

	/* connect, which should create it for us */
	ret = cd_device_db_load (ddb, "/tmp/device.db", &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure empty */
	ret = cd_device_db_empty (ddb, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* add a few entries */
	ret = cd_device_db_add (ddb, "device1", &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = cd_device_db_add (ddb, "device2", &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = cd_device_db_add (ddb, "device3", &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* remove one */
	ret = cd_device_db_remove (ddb, "device1", &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get all the devices */
	array = cd_device_db_get_devices (ddb, &error);
	g_assert_no_error (error);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 2);
	g_assert_cmpstr (g_ptr_array_index (array, 0), ==, "device2");
	g_assert_cmpstr (g_ptr_array_index (array, 1), ==, "device3");
	g_ptr_array_unref (array);

	/* set a property */
	ret = cd_device_db_set_property (ddb,
					 "device2",
					 "kind",
					 "display",
					 &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get a property that does not exist */
	value = cd_device_db_get_property (ddb,
					   "device2",
					   "xxx",
					   &error);
	g_assert_error (error, CD_MAIN_ERROR, CD_MAIN_ERROR_FAILED);
	g_assert (value == NULL);
	g_clear_error (&error);
	g_free (value);

	/* get a property that does exist */
	value = cd_device_db_get_property (ddb,
					   "device2",
					   "kind",
					   &error);
	g_assert_no_error (error);
	g_assert_cmpstr (value, ==, "display");
	g_free (value);

	/* check the correct number of properties are stored */
	array = cd_device_db_get_properties (ddb, "device2", &error);
	g_assert_no_error (error);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 1);
	g_ptr_array_unref (array);

	/* remove devices */
	ret = cd_device_db_remove (ddb, "device2", &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = cd_device_db_remove (ddb, "device3", &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get all the devices */
	array = cd_device_db_get_devices (ddb, &error);
	g_assert_no_error (error);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 0);
	g_ptr_array_unref (array);

	/* get a property that should be deleted */
	value = cd_device_db_get_property (ddb,
					   "device2",
					   "kind",
					   &error);
	g_assert_error (error, CD_MAIN_ERROR, CD_MAIN_ERROR_FAILED);
	g_assert (value == NULL);
	g_clear_error (&error);
	g_free (value);

	g_object_unref (ddb);
}

int
main (int argc, char **argv)
{
	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func ("/colord/mapping-db", cd_mapping_db_func);
	g_test_add_func ("/colord/device-db", cd_device_db_func);
	g_test_add_func ("/colord/profile", colord_profile_func);
	g_test_add_func ("/colord/device", colord_device_func);
	g_test_add_func ("/colord/device-array", colord_device_array_func);
	return g_test_run ();
}

