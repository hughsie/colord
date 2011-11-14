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

#include <gio/gio.h>
#include <glib-object.h>
#include <sqlite3.h>

#include "cd-common.h"
#include "cd-mapping-db.h"

static void     cd_mapping_db_finalize	(GObject        *object);

#define CD_MAPPING_DB_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_MAPPING_DB, CdMappingDbPrivate))

struct CdMappingDbPrivate
{
	sqlite3			*db;
};

static gpointer cd_mapping_db_object = NULL;

G_DEFINE_TYPE (CdMappingDb, cd_mapping_db, G_TYPE_OBJECT)

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
	ret = cd_main_mkdir_with_parents (path, error);
	if (!ret)
		goto out;

	g_debug ("CdMappingDb: trying to open database '%s'", filename);
	rc = sqlite3_open (filename, &mdb->priv->db);
	if (rc != SQLITE_OK) {
		ret = FALSE;
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
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
		g_debug ("CdMappingDb: creating table to repair: %s", error_msg);
		sqlite3_free (error_msg);
		statement = "CREATE TABLE mappings ("
			    "timestamp INTEGER DEFAULT 0,"
			    "device TEXT,"
			    "profile TEXT);";
		sqlite3_exec (mdb->priv->db, statement, NULL, NULL, NULL);
	}

	/* check mappings has timestamp (since 0.1.8) */
	rc = sqlite3_exec (mdb->priv->db,
			   "SELECT timestamp FROM mappings LIMIT 1",
			   NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		g_debug ("CdMappingDb: altering table to repair: %s", error_msg);
		sqlite3_free (error_msg);
		statement = "ALTER TABLE mappings ADD COLUMN timestamp INTEGER DEFAULT 0;";
		sqlite3_exec (mdb->priv->db, statement, NULL, NULL, NULL);
	}
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

	statement = "DELETE FROM mappings;";
	rc = sqlite3_exec (mdb->priv->db, statement,
			   NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		ret = FALSE;
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
			     "SQL error: %s",
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
		   const gchar *device_id,
		   const gchar *profile_id,
		   GError  **error)
{
	gboolean ret = TRUE;
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;
	gint64 timestamp;

	g_return_val_if_fail (CD_IS_MAPPING_DB (mdb), FALSE);
	g_return_val_if_fail (mdb->priv->db != NULL, FALSE);

	g_debug ("CdMappingDb: add %s<=>%s",
		 device_id, profile_id);
	timestamp = g_get_real_time ();
	statement = sqlite3_mprintf ("INSERT INTO mappings (device, profile, timestamp) "
				     "VALUES ('%q', '%q', %"G_GINT64_FORMAT")",
				     device_id, profile_id, timestamp);

	/* insert the entry */
	rc = sqlite3_exec (mdb->priv->db, statement, NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
			     "SQL error: %s",
			     error_msg);
		sqlite3_free (error_msg);
		ret = FALSE;
		goto out;
	}
out:
	sqlite3_free (statement);
	return ret;
}
/**
 * cd_mapping_db_update_timestamp:
 **/
gboolean
cd_mapping_db_update_timestamp (CdMappingDb *mdb,
				const gchar *device_id,
				const gchar *profile_id,
				GError  **error)
{
	gboolean ret = TRUE;
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;
	gint64 timestamp;

	g_return_val_if_fail (CD_IS_MAPPING_DB (mdb), FALSE);
	g_return_val_if_fail (mdb->priv->db != NULL, FALSE);

	g_debug ("CdMappingDb: update timestamp %s<=>%s",
		 device_id, profile_id);
	timestamp = g_get_real_time ();
	statement = sqlite3_mprintf ("UPDATE mappings SET timestamp = %"G_GINT64_FORMAT
				     " WHERE device = '%q' AND profile = '%q';",
				     timestamp, device_id, profile_id);

	/* update the entry */
	rc = sqlite3_exec (mdb->priv->db, statement, NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
			     "SQL error: %s",
			     error_msg);
		sqlite3_free (error_msg);
		ret = FALSE;
		goto out;
	}
out:
	sqlite3_free (statement);
	return ret;
}

/**
 * cd_mapping_db_remove:
 **/
gboolean  
cd_mapping_db_remove (CdMappingDb *mdb,
		      const gchar *device_id,
		      const gchar *profile_id,
		      GError  **error)
{
	gboolean ret = TRUE;
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;

	g_return_val_if_fail (CD_IS_MAPPING_DB (mdb), FALSE);
	g_return_val_if_fail (mdb->priv->db != NULL, FALSE);

	g_debug ("CdMappingDb: remove %s<=>%s", device_id, profile_id);
	statement = sqlite3_mprintf ("DELETE FROM mappings WHERE "
				     "device = '%q' AND profile = '%q';",
				     device_id, profile_id);

	/* remove the entry */
	rc = sqlite3_exec (mdb->priv->db, statement, NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
			     "SQL error: %s",
			     error_msg);
		sqlite3_free (error_msg);
		ret = FALSE;
		goto out;
	}
out:
	sqlite3_free (statement);
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
	g_debug ("CdMappingDb: got sql entry %s", argv[0]);
	g_ptr_array_add (array, g_strdup (argv[0]));
	return 0;
}

/**
 * cd_mapping_db_get_profiles:
 *
 * The returned values are returned with the oldest assigned profile in
 * the first position and the newest assigned profile in last posision.
 **/
GPtrArray *
cd_mapping_db_get_profiles (CdMappingDb *mdb,
			    const gchar *device_id,
			    GError  **error)
{
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;
	GPtrArray *array = NULL;
	GPtrArray *array_tmp = NULL;

	g_return_val_if_fail (CD_IS_MAPPING_DB (mdb), FALSE);
	g_return_val_if_fail (mdb->priv->db != NULL, FALSE);

	g_debug ("CdMappingDb: get profiles for %s", device_id);
	statement = sqlite3_mprintf ("SELECT profile FROM mappings WHERE "
				     "device = '%q' ORDER BY timestamp ASC;", device_id);

	/* remove the entry */
	array_tmp = g_ptr_array_new_with_free_func (g_free);
	rc = sqlite3_exec (mdb->priv->db,
			   statement,
			   cd_mapping_db_sqlite_cb,
			   array_tmp,
			   &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
			     "SQL error: %s",
			     error_msg);
		sqlite3_free (error_msg);
		goto out;
	}

	/* success */
	array = g_ptr_array_ref (array_tmp);
out:
	g_ptr_array_unref (array_tmp);
	sqlite3_free (statement);
	return array;
}

/**
 * cd_mapping_db_get_devices:
 *
 * The returned values are returned with the oldest assigned profile in
 * the first position and the newest assigned profile in last posision.
 **/
GPtrArray *
cd_mapping_db_get_devices (CdMappingDb *mdb,
			   const gchar *profile_id,
			   GError  **error)
{
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;
	GPtrArray *array = NULL;
	GPtrArray *array_tmp = NULL;

	g_return_val_if_fail (CD_IS_MAPPING_DB (mdb), FALSE);
	g_return_val_if_fail (mdb->priv->db != NULL, FALSE);

	g_debug ("CdMappingDb: get devices for %s", profile_id);
	statement = sqlite3_mprintf ("SELECT device FROM mappings WHERE "
				     "profile = '%q' ORDER BY timestamp ASC;", profile_id);

	/* remove the entry */
	array_tmp = g_ptr_array_new_with_free_func (g_free);
	rc = sqlite3_exec (mdb->priv->db,
			   statement,
			   cd_mapping_db_sqlite_cb,
			   array_tmp,
			   &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
			     "SQL error: %s",
			     error_msg);
		sqlite3_free (error_msg);
		goto out;
	}

	/* success */
	array = g_ptr_array_ref (array_tmp);
out:
	g_ptr_array_unref (array_tmp);
	sqlite3_free (statement);
	return array;
}

/**
 * cd_mapping_db_sqlite_timestamp_cb:
 **/
static gint
cd_mapping_db_sqlite_timestamp_cb (void *data,
				   gint argc,
				   gchar **argv,
				   gchar **col_name)
{
	guint64 *timestamp = (guint64 *) data;

	/* should only be one entry */
	g_debug ("CdMappingDb: got sql entry %s", argv[0]);
	*timestamp = g_ascii_strtoull (argv[0], NULL, 10);
	return 0;
}

/**
 * cd_mapping_db_get_timestamp:
 *
 * Gets when the profile was added to the device.
 **/
guint64
cd_mapping_db_get_timestamp (CdMappingDb *mdb,
			     const gchar *device_id,
			     const gchar *profile_id,
			     GError  **error)
{
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;
	guint64 timestamp = G_MAXUINT64;

	g_return_val_if_fail (CD_IS_MAPPING_DB (mdb), FALSE);
	g_return_val_if_fail (mdb->priv->db != NULL, FALSE);

	g_debug ("CdMappingDb: get checksum for %s<->%s",
		 device_id, profile_id);
	statement = sqlite3_mprintf ("SELECT timestamp FROM mappings WHERE "
				     "device = '%q' AND profile = '%q' "
				     "LIMIT 1;", device_id, profile_id);

	/* query the checksum */
	rc = sqlite3_exec (mdb->priv->db,
			   statement,
			   cd_mapping_db_sqlite_timestamp_cb,
			   &timestamp,
			   &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
			     "SQL error: %s",
			     error_msg);
		sqlite3_free (error_msg);
		goto out;
	}
out:
	sqlite3_free (statement);
	return timestamp;
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

