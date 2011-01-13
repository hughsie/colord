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

#include <gio/gio.h>
#include <glib-object.h>
#include <sqlite3.h>

#include "cd-mapping-db.h"

static void     cd_mapping_db_finalize	(GObject        *object);

#define CD_MAPPING_DB_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_MAPPING_DB, CdMappingDbPrivate))

struct CdMappingDbPrivate
{
	sqlite3			*db;
};

enum {
	SIGNAL_MAPPING,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };
static gpointer cd_mapping_db_object = NULL;

G_DEFINE_TYPE (CdMappingDb, cd_mapping_db, G_TYPE_OBJECT)

/**
 * cd_mapping_db_mkdir_with_parents:
 **/
static gboolean
cd_mapping_db_mkdir_with_parents (const gchar *filename, GError **error)
{
	gboolean ret;
	GFile *file = NULL;

	/* ensure desination exists */
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (!ret) {
		file = g_file_new_for_path (filename);
		ret = g_file_make_directory_with_parents (file, NULL, error);
		if (!ret)
			goto out;
	}
out:
	if (file != NULL)
		g_object_unref (file);
	return ret;
}

/**
 * cd_mapping_db_load:
 **/
gboolean  
cd_mapping_db_load (CdMappingDb *mdb,
		    const gchar *filename,
		    GError  **error)
{
	const gchar *statement;
	gboolean ret = TRUE;
	gchar *error_msg = NULL;
	gchar *path = NULL;
	gint rc;

	g_return_val_if_fail (CD_IS_MAPPING_DB (mdb), FALSE);
	g_return_val_if_fail (mdb->priv->db == NULL, FALSE);

	/* ensure the path exists */
	path = g_path_get_dirname (filename);
	ret = cd_mapping_db_mkdir_with_parents (path, error);
	if (!ret)
		goto out;

	g_debug ("trying to open database '%s'", filename);
	rc = sqlite3_open (filename, &mdb->priv->db);
	if (rc != SQLITE_OK) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Can't open database: %s\n",
			     sqlite3_errmsg (mdb->priv->db));
		sqlite3_close (mdb->priv->db);
		goto out;
	}

	/* we don't need to keep doing fsync */
	sqlite3_exec (mdb->priv->db, "PRAGMA synchronous=OFF",
		      NULL, NULL, NULL);

	/* check mappings */
	rc = sqlite3_exec (mdb->priv->db, "SELECT * FROM mappings LIMIT 1",
			   NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		g_debug ("creating table to repair: %s", error_msg);
		sqlite3_free (error_msg);
		statement = "CREATE TABLE mappings ("
			    "id TEXT PRIMARY KEY,"
			    "device TEXT,"
			    "profile TEXT);";
		sqlite3_exec (mdb->priv->db, statement, NULL, NULL, NULL);
	}
#if 0
	/* check mappings has enough data (since 0.1.99999) */
	rc = sqlite3_exec (mdb->priv->db,
			   "SELECT default FROM mappings LIMIT 1",
			   NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		g_debug ("altering table to repair: %s", error_msg);
		sqlite3_free (error_msg);
		statement = "ALTER TABLE mappings ADD COLUMN default INTEGER DEFAULT 0;";
		sqlite3_exec (mdb->priv->db, statement, NULL, NULL, NULL);
	}
#endif
out:
	g_free (path);
	return ret;
}

/**
 * cd_mapping_db_empty:
 **/
gboolean  
cd_mapping_db_empty (CdMappingDb *mdb,
		     GError  **error)
{
	const gchar *statement;
	gboolean ret = TRUE;
	gchar *error_msg = NULL;
	gint rc;

	g_return_val_if_fail (CD_IS_MAPPING_DB (mdb), FALSE);
	g_return_val_if_fail (mdb->priv->db != NULL, FALSE);

	statement = "TRUNCATE TABLE mappings;";
	statement = "DELETE FROM mappings;";
	rc = sqlite3_exec (mdb->priv->db, statement,
			   NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "SQL error: %s\n",
			     error_msg);
		sqlite3_free (error_msg);
		goto out;
	}
out:
	return ret;
}

/**
 * cd_mapping_db_add:
 **/
gboolean  
cd_mapping_db_add (CdMappingDb *mdb,
		   const gchar *device,
		   const gchar *profile,
		   GError  **error)
{
	gboolean ret = TRUE;
	gchar *device_id;
	gchar *error_msg = NULL;
	gchar *profile_id;
	gchar *statement;
	gint rc;

	g_return_val_if_fail (CD_IS_MAPPING_DB (mdb), FALSE);
	g_return_val_if_fail (mdb->priv->db != NULL, FALSE);

	device_id = g_path_get_basename (device);
	profile_id = g_path_get_basename (profile);
	g_debug ("add %s<->%s with id %s-%s",
		 device, profile,
		 device_id, profile_id);
	statement = g_strdup_printf ("INSERT INTO mappings (id, device, profile) "
				     "VALUES ('%s-%s', '%s', '%s')",
				     device_id, profile_id,
				     device, profile);

	/* insert the entry */
	rc = sqlite3_exec (mdb->priv->db, statement, NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     1, 0,
			     "SQL error: %s\n",
			     error_msg);
		sqlite3_free (error_msg);
		ret = FALSE;
		goto out;
	}
out:
	g_free (device_id);
	g_free (profile_id);
	g_free (statement);
	return ret;
}

/**
 * cd_mapping_db_remove:
 **/
gboolean  
cd_mapping_db_remove (CdMappingDb *mdb,
		      const gchar *device,
		      const gchar *profile,
		      GError  **error)
{
	gboolean ret = TRUE;
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;

	g_return_val_if_fail (CD_IS_MAPPING_DB (mdb), FALSE);
	g_return_val_if_fail (mdb->priv->db != NULL, FALSE);

	g_debug ("remove %s<->%s", device, profile);
	statement = g_strdup_printf ("DELETE FROM mappings WHERE "
				     "device = '%s' AND profile = '%s';",
				     device, profile);

	/* remove the entry */
	rc = sqlite3_exec (mdb->priv->db, statement, NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     1, 0,
			     "SQL error: %s\n",
			     error_msg);
		sqlite3_free (error_msg);
		ret = FALSE;
		goto out;
	}
out:
	g_free (statement);
	return ret;
}

/**
 * cd_mapping_db_sqlite_cb:
 **/
static gint
cd_mapping_db_sqlite_cb (void *data,
			 gint argc,
			 gchar **argv,
			 gchar **col_name)
{
	GPtrArray *array = (GPtrArray *) data;

	/* should only be one entry */
	g_debug ("adding %s", argv[0]);
	g_ptr_array_add (array, g_strdup (argv[0]));
	return 0;
}

/**
 * cd_mapping_db_get_profiles:
 **/
GPtrArray *
cd_mapping_db_get_profiles (CdMappingDb *mdb,
			    const gchar *device,
			    GError  **error)
{
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;
	GPtrArray *array = NULL;
	GPtrArray *array_tmp = NULL;

	g_return_val_if_fail (CD_IS_MAPPING_DB (mdb), FALSE);
	g_return_val_if_fail (mdb->priv->db != NULL, FALSE);

	g_debug ("get profiles for %s", device);
	statement = g_strdup_printf ("SELECT profile FROM mappings WHERE "
				     "device = '%s';", device);

	/* remove the entry */
	array_tmp = g_ptr_array_new_with_free_func (g_free);
	rc = sqlite3_exec (mdb->priv->db,
			   statement,
			   cd_mapping_db_sqlite_cb,
			   array_tmp,
			   &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     1, 0,
			     "SQL error: %s\n",
			     error_msg);
		sqlite3_free (error_msg);
		goto out;
	}

	/* success */
	array = g_ptr_array_ref (array_tmp);
out:
	g_ptr_array_unref (array_tmp);
	g_free (statement);
	return array;
}

/**
 * cd_mapping_db_get_devices:
 **/
GPtrArray *
cd_mapping_db_get_devices (CdMappingDb *mdb,
			   const gchar *profile,
			   GError  **error)
{
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;
	GPtrArray *array = NULL;
	GPtrArray *array_tmp = NULL;

	g_return_val_if_fail (CD_IS_MAPPING_DB (mdb), FALSE);
	g_return_val_if_fail (mdb->priv->db != NULL, FALSE);

	g_debug ("get devices for %s", profile);
	statement = g_strdup_printf ("SELECT device FROM mappings WHERE "
				     "profile = '%s';", profile);

	/* remove the entry */
	array_tmp = g_ptr_array_new_with_free_func (g_free);
	rc = sqlite3_exec (mdb->priv->db,
			   statement,
			   cd_mapping_db_sqlite_cb,
			   array_tmp,
			   &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     1, 0,
			     "SQL error: %s\n",
			     error_msg);
		sqlite3_free (error_msg);
		goto out;
	}

	/* success */
	array = g_ptr_array_ref (array_tmp);
out:
	g_ptr_array_unref (array_tmp);
	g_free (statement);
	return array;
}

/**
 * cd_mapping_db_class_init:
 * @klass: The CdMappingDbClass
 **/
static void
cd_mapping_db_class_init (CdMappingDbClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_mapping_db_finalize;
	signals [SIGNAL_MAPPING] =
		g_signal_new ("mapping",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      0, NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	g_type_class_add_private (klass, sizeof (CdMappingDbPrivate));
}

/**
 * cd_mapping_db_init:
 **/
static void
cd_mapping_db_init (CdMappingDb *mdb)
{
	mdb->priv = CD_MAPPING_DB_GET_PRIVATE (mdb);
}

/**
 * cd_mapping_db_finalize:
 * @object: The object to finalize
 **/
static void
cd_mapping_db_finalize (GObject *object)
{
	CdMappingDb *mdb;
	g_return_if_fail (CD_IS_MAPPING_DB (object));
	mdb = CD_MAPPING_DB (object);
	g_return_if_fail (mdb->priv != NULL);

	/* close the database */
	sqlite3_close (mdb->priv->db);

	G_OBJECT_CLASS (cd_mapping_db_parent_class)->finalize (object);
}

/**
 * cd_mapping_db_new:
 *
 * Return value: a new CdMappingDb object.
 **/
CdMappingDb *
cd_mapping_db_new (void)
{
	if (cd_mapping_db_object != NULL) {
		g_object_ref (cd_mapping_db_object);
	} else {
		cd_mapping_db_object = g_object_new (CD_TYPE_MAPPING_DB, NULL);
		g_object_add_weak_pointer (cd_mapping_db_object, &cd_mapping_db_object);
	}
	return CD_MAPPING_DB (cd_mapping_db_object);
}

