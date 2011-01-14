/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
#include "cd-mapping-db.h"

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
	ret = cd_mapping_db_add (mdb, "/device1", "/profile1", &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = cd_mapping_db_add (mdb, "/device1", "/profile2", &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = cd_mapping_db_add (mdb, "/device1", "/profile3", &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* remove one */
	ret = cd_mapping_db_remove (mdb, "/device1", "/profile2", &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get the profiles for a device */
	array = cd_mapping_db_get_profiles (mdb, "/device1", &error);
	g_assert_no_error (error);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 2);
	g_assert_cmpstr (g_ptr_array_index (array, 0), ==, "/profile1");
	g_assert_cmpstr (g_ptr_array_index (array, 1), ==, "/profile3");
	g_ptr_array_unref (array);

	/* get the devices for a profile */
	array = cd_mapping_db_get_devices (mdb, "/profile1", &error);
	g_assert_no_error (error);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 1);
	g_assert_cmpstr (g_ptr_array_index (array, 0), ==, "/device1");
	g_ptr_array_unref (array);

	g_object_unref (mdb);
}

int
main (int argc, char **argv)
{
	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func ("/colord/mapping-db", cd_mapping_db_func);
	return g_test_run ();
}

