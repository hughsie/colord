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

#include "cd-common.h"
#include "cd-device-db.h"

static void     cd_device_db_finalize	(GObject        *object);

#define GET_PRIVATE(o) (cd_device_db_get_instance_private (o))

typedef struct
{
	sqlite3			*db;
} CdDeviceDbPrivate;

static gpointer cd_device_db_object = NULL;

G_DEFINE_TYPE_WITH_PRIVATE (CdDeviceDb, cd_device_db, G_TYPE_OBJECT)

/**
 * cd_device_db_load:
 **/
gboolean  
cd_device_db_load (CdDeviceDb *ddb,
		    const gchar *filename,
		    GError  **error)
{
	CdDeviceDbPrivate *priv = GET_PRIVATE (ddb);
	const gchar *statement;
	gchar *error_msg = NULL;
	gint rc;
	g_autofree gchar *path = NULL;

	g_return_val_if_fail (CD_IS_DEVICE_DB (ddb), FALSE);
	g_return_val_if_fail (priv->db == NULL, FALSE);

	/* ensure the path exists */
	path = g_path_get_dirname (filename);
	if (!cd_main_mkdir_with_parents (path, error))
		return FALSE;

	g_debug ("CdDeviceDb: trying to open database '%s'", filename);
	g_info ("Using device database file %s", filename);
	rc = sqlite3_open (filename, &priv->db);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_INTERNAL,
			     "Can't open database: %s\n",
			     sqlite3_errmsg (priv->db));
		sqlite3_close (priv->db);
		return FALSE;
	}

	/* we don't need to keep doing fsync */
	sqlite3_exec (priv->db, "PRAGMA synchronous=OFF",
		      NULL, NULL, NULL);

	/* check devices */
	rc = sqlite3_exec (priv->db, "SELECT * FROM devices LIMIT 1",
			   NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		g_debug ("CdDeviceDb: creating table to repair: %s", error_msg);
		sqlite3_free (error_msg);
		statement = "CREATE TABLE devices ("
			    "device_id TEXT PRIMARY KEY,"
			    "device TEXT);";
		sqlite3_exec (priv->db, statement, NULL, NULL, NULL);
	}

	/* check properties version 2 */
	rc = sqlite3_exec (priv->db, "SELECT * FROM properties_v2 LIMIT 1",
			   NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		statement = "CREATE TABLE properties_v2 ("
			    "device_id TEXT,"
			    "property TEXT,"
			    "value TEXT,"
			    "PRIMARY KEY (device_id, property));";
		sqlite3_exec (priv->db, statement, NULL, NULL, NULL);
	}
	return TRUE;
}

/**
 * cd_device_db_empty:
 **/
gboolean  
cd_device_db_empty (CdDeviceDb *ddb,
		     GError  **error)
{
	CdDeviceDbPrivate *priv = GET_PRIVATE (ddb);
	const gchar *statement;
	gchar *error_msg = NULL;
	gint rc;

	g_return_val_if_fail (CD_IS_DEVICE_DB (ddb), FALSE);
	g_return_val_if_fail (priv->db != NULL, FALSE);

	statement = "DELETE FROM devices;DELETE FROM properties_v2;";
	rc = sqlite3_exec (priv->db, statement,
			   NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_INTERNAL,
			     "SQL error: %s",
			     error_msg);
		sqlite3_free (error_msg);
		return FALSE;
	}
	return TRUE;
}

/**
 * cd_device_db_add:
 **/
gboolean  
cd_device_db_add (CdDeviceDb *ddb,
		  const gchar *device_id,
		  GError  **error)
{
	CdDeviceDbPrivate *priv = GET_PRIVATE (ddb);
	gboolean ret = TRUE;
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;

	g_return_val_if_fail (CD_IS_DEVICE_DB (ddb), FALSE);
	g_return_val_if_fail (priv->db != NULL, FALSE);

	g_debug ("CdDeviceDb: add device %s", device_id);
	statement = sqlite3_mprintf ("INSERT INTO devices (device_id) "
				     "VALUES ('%q')",
				     device_id);

	/* insert the entry */
	rc = sqlite3_exec (priv->db, statement, NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_INTERNAL,
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
 * cd_device_db_set_property:
 **/
gboolean  
cd_device_db_set_property (CdDeviceDb *ddb,
			   const gchar *device_id,
			   const gchar *property,
			   const gchar *value,
			   GError  **error)
{
	CdDeviceDbPrivate *priv = GET_PRIVATE (ddb);
	gboolean ret = TRUE;
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;

	g_return_val_if_fail (CD_IS_DEVICE_DB (ddb), FALSE);
	g_return_val_if_fail (priv->db != NULL, FALSE);

	g_debug ("CdDeviceDb: add device property %s [%s=%s]",
		 device_id, property, value);
	statement = sqlite3_mprintf ("INSERT OR REPLACE INTO properties_v2 (device_id, property, value) "
				     "VALUES ('%q', '%q', '%q');",
				     device_id, property, value);

	/* insert the entry */
	rc = sqlite3_exec (priv->db, statement, NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_INTERNAL,
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
 * cd_device_db_remove:
 **/
gboolean  
cd_device_db_remove (CdDeviceDb *ddb,
		     const gchar *device_id,
		     GError  **error)
{
	CdDeviceDbPrivate *priv = GET_PRIVATE (ddb);
	gboolean ret = TRUE;
	gchar *error_msg = NULL;
	gchar *statement1 = NULL;
	gchar *statement2 = NULL;
	gint rc;

	g_return_val_if_fail (CD_IS_DEVICE_DB (ddb), FALSE);
	g_return_val_if_fail (priv->db != NULL, FALSE);

	/* remove the entry */
	g_debug ("CdDeviceDb: remove device %s", device_id);
	statement1 = sqlite3_mprintf ("DELETE FROM devices WHERE "
				     "device_id = '%q';",
				     device_id);
	rc = sqlite3_exec (priv->db, statement1, NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_INTERNAL,
			     "SQL error: %s",
			     error_msg);
		sqlite3_free (error_msg);
		ret = FALSE;
		goto out;
	}
	statement2 = sqlite3_mprintf ("DELETE FROM properties_v2 WHERE "
				     "device_id = '%q';",
				     device_id);
	rc = sqlite3_exec (priv->db, statement2, NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_INTERNAL,
			     "SQL error: %s",
			     error_msg);
		sqlite3_free (error_msg);
		ret = FALSE;
		goto out;
	}
out:
	sqlite3_free (statement1);
	sqlite3_free (statement2);
	return ret;
}

/**
 * cd_device_db_sqlite_cb:
 **/
static gint
cd_device_db_sqlite_cb (void *data,
			gint argc,
			gchar **argv,
			gchar **col_name)
{
	GPtrArray *array = (GPtrArray *) data;

	/* should only be one entry */
	g_debug ("CdDeviceDb: got sql result %s", argv[0]);
	g_ptr_array_add (array, g_strdup (argv[0]));
	return 0;
}

/**
 * cd_device_db_get_property:
 **/
gchar *
cd_device_db_get_property (CdDeviceDb *ddb,
			   const gchar *device_id,
			   const gchar *property,
			   GError  **error)
{
	CdDeviceDbPrivate *priv = GET_PRIVATE (ddb);
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;
	gchar *value = NULL;
	g_autoptr(GPtrArray) array_tmp = NULL;

	g_return_val_if_fail (CD_IS_DEVICE_DB (ddb), NULL);
	g_return_val_if_fail (priv->db != NULL, NULL);

	g_debug ("CdDeviceDb: get property %s for %s", property, device_id);
	statement = sqlite3_mprintf ("SELECT value FROM properties_v2 WHERE "
				     "device_id = '%q' AND "
				     "property = '%q' LIMIT 1;",
				     device_id, property);

	/* remove the entry */
	array_tmp = g_ptr_array_new_with_free_func (g_free);
	rc = sqlite3_exec (priv->db,
			   statement,
			   cd_device_db_sqlite_cb,
			   array_tmp,
			   &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_INTERNAL,
			     "SQL error: %s",
			     error_msg);
		sqlite3_free (error_msg);
		goto out;
	}

	/* never set */
	if (array_tmp->len == 0) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_INTERNAL,
			     "no such property %s for %s",
			     property, device_id);
		goto out;
	}

	/* success */
	value = g_strdup (g_ptr_array_index (array_tmp, 0));
out:
	sqlite3_free (statement);
	return value;
}

/**
 * cd_device_db_get_devices:
 **/
GPtrArray *
cd_device_db_get_devices (CdDeviceDb *ddb,
			  GError  **error)
{
	CdDeviceDbPrivate *priv = GET_PRIVATE (ddb);
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;
	GPtrArray *array = NULL;
	g_autoptr(GPtrArray) array_tmp = NULL;

	g_return_val_if_fail (CD_IS_DEVICE_DB (ddb), NULL);
	g_return_val_if_fail (priv->db != NULL, NULL);

	/* get all the devices */
	g_debug ("CdDeviceDb: get devices");
	statement = sqlite3_mprintf ("SELECT device_id FROM devices;");
	array_tmp = g_ptr_array_new_with_free_func (g_free);
	rc = sqlite3_exec (priv->db,
			   statement,
			   cd_device_db_sqlite_cb,
			   array_tmp,
			   &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_INTERNAL,
			     "SQL error: %s",
			     error_msg);
		sqlite3_free (error_msg);
		goto out;
	}

	/* success */
	array = g_ptr_array_ref (array_tmp);
out:
	sqlite3_free (statement);
	return array;
}

/**
 * cd_device_db_get_properties:
 **/
GPtrArray *
cd_device_db_get_properties (CdDeviceDb *ddb,
			     const gchar *device_id,
			     GError  **error)
{
	CdDeviceDbPrivate *priv = GET_PRIVATE (ddb);
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;
	GPtrArray *array = NULL;
	g_autoptr(GPtrArray) array_tmp = NULL;

	g_return_val_if_fail (CD_IS_DEVICE_DB (ddb), NULL);
	g_return_val_if_fail (priv->db != NULL, NULL);

	/* get all the devices */
	g_debug ("CdDeviceDb: get properties for device %s", device_id);
	statement = sqlite3_mprintf ("SELECT property FROM properties_v2 "
				     "WHERE device_id = '%q';",
				     device_id);
	array_tmp = g_ptr_array_new_with_free_func (g_free);
	rc = sqlite3_exec (priv->db,
			   statement,
			   cd_device_db_sqlite_cb,
			   array_tmp,
			   &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_INTERNAL,
			     "SQL error: %s",
			     error_msg);
		sqlite3_free (error_msg);
		goto out;
	}

	/* success */
	array = g_ptr_array_ref (array_tmp);
out:
	sqlite3_free (statement);
	return array;
}

/**
 * cd_device_db_class_init:
 * @klass: The CdDeviceDbClass
 **/
static void
cd_device_db_class_init (CdDeviceDbClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_device_db_finalize;
}

/**
 * cd_device_db_init:
 **/
static void
cd_device_db_init (CdDeviceDb *ddb)
{
}

/**
 * cd_device_db_finalize:
 * @object: The object to finalize
 **/
static void
cd_device_db_finalize (GObject *object)
{
	CdDeviceDb *ddb = CD_DEVICE_DB (object);
	CdDeviceDbPrivate *priv = GET_PRIVATE (ddb);

	/* close the database */
	sqlite3_close (priv->db);

	G_OBJECT_CLASS (cd_device_db_parent_class)->finalize (object);
}

/**
 * cd_device_db_new:
 *
 * Return value: a new CdDeviceDb object.
 **/
CdDeviceDb *
cd_device_db_new (void)
{
	if (cd_device_db_object != NULL) {
		g_object_ref (cd_device_db_object);
	} else {
		cd_device_db_object = g_object_new (CD_TYPE_DEVICE_DB, NULL);
		g_object_add_weak_pointer (cd_device_db_object, &cd_device_db_object);
	}
	return CD_DEVICE_DB (cd_device_db_object);
}
