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
#include <sqlite3.h>
#include <glib/gstdio.h>

#include "cd-common.h"
#include "cd-device-array.h"
#include "cd-device-db.h"
#include "cd-device.h"
#include "cd-mapping-db.h"
#include "cd-profile-array.h"
#include "cd-profile-db.h"
#include "cd-profile.h"

static void
colord_common_func (void)
{
	gchar *tmp;

	/* exact match in quirk db */
	tmp = cd_quirk_vendor_name ("NIKON");
	g_assert_cmpstr (tmp, ==, "Nikon");
	g_free (tmp);

	/* suffix that needs removing */
	tmp = cd_quirk_vendor_name ("Hughski Ltd.");
	g_assert_cmpstr (tmp, ==, "Hughski");
	g_free (tmp);

	/* suffix that needs removing */
	tmp = cd_quirk_vendor_name ("Acme Inc");
	g_assert_cmpstr (tmp, ==, "Acme");
	g_free (tmp);
}

static void
colord_profile_func (void)
{
	CdProfile *profile;

	profile = cd_profile_new ();
	g_assert (profile != NULL);

	cd_profile_set_id (profile, "dave");
	g_assert_cmpstr (cd_profile_get_id (profile), ==, "dave");
	g_assert_cmpint (cd_profile_get_score (profile), ==, 1);

	/* system-wide profiles have a larger importance */
	cd_profile_set_is_system_wide (profile, TRUE);
	g_assert_cmpint (cd_profile_get_score (profile), ==, 2);

	g_object_unref (profile);
}

static void
colord_device_func (void)
{
	CdDeviceDb *ddb;
	CdDevice *device;
	CdProfile *profile;
	CdProfileArray *profile_array;
	gboolean ret;
	GError *error = NULL;

	profile_array = cd_profile_array_new ();
	device = cd_device_new ();
	g_assert (device != NULL);

	/* create device database */
	ddb = cd_device_db_new ();
	ret = cd_device_db_load (ddb, "/tmp/device.db", &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = cd_device_db_empty (ddb, &error);
	g_assert_no_error (error);
	g_assert (ret);

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
				     CD_DEVICE_RELATION_SOFT,
				     cd_profile_get_object_path (profile),
				     0,
				     &error);
	g_assert_error (error, CD_DEVICE_ERROR, CD_DEVICE_ERROR_PROFILE_ALREADY_ADDED);
	g_assert (!ret);
	g_clear_error (&error);

	/* add profile again */
	ret = cd_device_add_profile (device,
				     CD_DEVICE_RELATION_HARD,
				     cd_profile_get_object_path (profile),
				     0,
				     &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* add profile that does not exist */
	ret = cd_device_add_profile (device,
				     CD_DEVICE_RELATION_SOFT,
				     "/dave",
				     0,
				     &error);
	g_assert_error (error, CD_DEVICE_ERROR, CD_DEVICE_ERROR_PROFILE_DOES_NOT_EXIST);
	g_assert (!ret);
	g_clear_error (&error);

	g_object_unref (device);
	g_object_unref (ddb);
	g_object_unref (profile);
	g_object_unref (profile_array);
}

static void
colord_device_array_func (void)
{
	CdDeviceArray *device_array;
	CdDeviceDb *ddb;
	CdDevice *device;
	gboolean ret;
	GError *error = NULL;

	/* create device database */
	ddb = cd_device_db_new ();
	ret = cd_device_db_load (ddb, "/tmp/device.db", &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = cd_device_db_empty (ddb, &error);
	g_assert_no_error (error);
	g_assert (ret);

	device_array = cd_device_array_new ();
	g_assert (device_array != NULL);

	device = cd_device_new ();
	cd_device_set_id (device, "dave");
	cd_device_array_add (device_array, device);
	g_object_unref (device);

	device = cd_device_array_get_by_id_owner (device_array, "does not exist", 0);
	g_assert (device == NULL);

	device = cd_device_array_get_by_id_owner (device_array, "dave", 0);
	g_assert (device != NULL);
	g_assert_cmpstr (cd_device_get_id (device), ==, "dave");
	g_object_unref (device);

	device = cd_device_array_get_by_object_path (device_array,
						     "/org/freedesktop/ColorManager/devices/dave");
	g_assert (device != NULL);
	g_assert_cmpstr (cd_device_get_id (device), ==, "dave");
	g_object_unref (device);

	g_object_unref (device_array);
	g_object_unref (ddb);
}

static void
cd_mapping_db_alter_func (void)
{
	CdMappingDb *mdb;
	const gchar *db_filename = "/tmp/mapping.db";
	const gchar *statement;
	gboolean ret;
	GError *error = NULL;
	gint rc;
	sqlite3 *db;

	/* create */
	mdb = cd_mapping_db_new ();
	g_assert (mdb != NULL);

	/* setup v0.1.0 database for altering */
	g_unlink (db_filename);
	rc = sqlite3_open (db_filename, &db);
	g_assert_cmpint (rc, ==, SQLITE_OK);
	statement = "CREATE TABLE mappings ("
		    "device TEXT,"
		    "profile TEXT);";
	sqlite3_exec (db, statement, NULL, NULL, NULL);
	statement = "INSERT INTO mappings (device, profile) VALUES ('dev1', 'prof1')";
	sqlite3_exec (db, statement, NULL, NULL, NULL);

	/* connect */
	ret = cd_mapping_db_load (mdb, db_filename, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check we ALTERed it correctly */
	statement = "SELECT timestamp FROM mappings LIMIT 1";
	rc = sqlite3_exec (db, statement, NULL, NULL, NULL);
	g_assert_cmpint (rc, ==, SQLITE_OK);

	sqlite3_close (db);
	g_object_unref (mdb);
}

/**
 * cd_mapping_db_test_cb:
 **/
static gint
cd_mapping_db_test_cb (void *data, gint argc, gchar **argv, gchar **col_name)
{
	gchar **tmp = (gchar **) data;
	*tmp = g_strdup (argv[0]);
	return 0;
}

static void
cd_mapping_db_convert_func (void)
{
	CdMappingDb *mdb;
	const gchar *db_filename = "/tmp/mapping.db";
	const gchar *statement;
	gboolean ret;
	gchar *device_id = NULL;
	GError *error = NULL;
	gint rc;
	sqlite3 *db;

	/* create */
	mdb = cd_mapping_db_new ();
	g_assert (mdb != NULL);

	/* setup v0.1.8 database for converting */
	g_unlink (db_filename);
	rc = sqlite3_open (db_filename, &db);
	g_assert_cmpint (rc, ==, SQLITE_OK);
	statement = "CREATE TABLE mappings ("
		    "device TEXT,"
		    "profile TEXT,"
		    "timestamp INTEGER DEFAULT 0);";
	sqlite3_exec (db, statement, NULL, NULL, NULL);
	statement = "INSERT INTO mappings (device, profile, timestamp) VALUES ('dev1', 'prof1', 12345)";
	sqlite3_exec (db, statement, NULL, NULL, NULL);

	/* connect, which should convert it for us */
	ret = cd_mapping_db_load (mdb, db_filename, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check we converted it correctly */
	statement = "SELECT timestamp FROM mappings_v2 LIMIT 1";
	rc = sqlite3_exec (db, statement,
			   cd_mapping_db_test_cb, &device_id, NULL);
	g_assert_cmpint (rc, ==, SQLITE_OK);
	g_assert_cmpstr (device_id, ==, "12345");
	g_free (device_id);

	sqlite3_close (db);
	g_object_unref (mdb);
}

static void
cd_mapping_db_func (void)
{
	CdMappingDb *mdb;
	const gchar *db_filename = "/tmp/mapping.db";
	gboolean ret;
	GError *error = NULL;
	GPtrArray *array;
	guint64 timestamp;

	/* create */
	mdb = cd_mapping_db_new ();
	g_assert (mdb != NULL);

	/* connect, which should create a v2 table for us */
	g_unlink (db_filename);
	ret = cd_mapping_db_load (mdb, db_filename, &error);
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
	ret = cd_mapping_db_clear_timestamp (mdb, "device1", "profile2", &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure timestamp really is zero */
	timestamp = cd_mapping_db_get_timestamp (mdb,
						 "device1",
						 "profile2",
						 &error);
	g_assert_no_error (error);
	g_assert (timestamp != G_MAXUINT64);
	g_assert_cmpint (timestamp, ==, 0);

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
	g_assert_error (error, CD_CLIENT_ERROR, CD_CLIENT_ERROR_INTERNAL);
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
	g_assert_error (error, CD_CLIENT_ERROR, CD_CLIENT_ERROR_INTERNAL);
	g_assert (value == NULL);
	g_clear_error (&error);
	g_free (value);

	g_object_unref (ddb);
}

static void
cd_profile_db_func (void)
{
	CdProfileDb *pdb;
	GError *error = NULL;
	gboolean ret;
	gchar *value = NULL;

	/* create */
	pdb = cd_profile_db_new ();
	g_assert (pdb != NULL);

	/* connect, which should create it for us */
	ret = cd_profile_db_load (pdb, "/tmp/profile.db", &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure empty */
	ret = cd_profile_db_empty (pdb, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* set a property */
	ret = cd_profile_db_set_property (pdb,
					 "profile-test",
					 "Title",
					 500,
					 "My Display Profile",
					 &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get a property that does not exist */
	ret = cd_profile_db_get_property (pdb,
					  "profile-test",
					  "Modified",
					  500,
					  &value,
					  &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (value, ==, NULL);

	/* get a property for a different user */
	ret = cd_profile_db_get_property (pdb,
					  "profile-test",
					  "Title",
					  501,
					  &value,
					  &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (value, ==, NULL);

	/* get a property that does exist */
	ret = cd_profile_db_get_property (pdb,
					  "profile-test",
					  "Title",
					  500,
					  &value,
					  &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (value, ==, "My Display Profile");
	g_free (value);

	g_object_unref (pdb);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func ("/colord/common", colord_common_func);
	g_test_add_func ("/colord/mapping-db{alter}", cd_mapping_db_alter_func);
	g_test_add_func ("/colord/mapping-db{convert}", cd_mapping_db_convert_func);
	g_test_add_func ("/colord/mapping-db", cd_mapping_db_func);
	g_test_add_func ("/colord/device-db", cd_device_db_func);
	g_test_add_func ("/colord/profile", colord_profile_func);
	g_test_add_func ("/colord/profile-db", cd_profile_db_func);
	g_test_add_func ("/colord/device", colord_device_func);
	g_test_add_func ("/colord/device-array", colord_device_array_func);
	return g_test_run ();
}

