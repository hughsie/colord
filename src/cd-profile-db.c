/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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
#include <syslog.h>

#include "cd-common.h"
#include "cd-profile-db.h"

static void cd_profile_db_finalize	(GObject *object);

#define CD_PROFILE_DB_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_PROFILE_DB, CdProfileDbPrivate))

struct CdProfileDbPrivate
{
	sqlite3			*db;
};

static gpointer cd_profile_db_object = NULL;

G_DEFINE_TYPE (CdProfileDb, cd_profile_db, G_TYPE_OBJECT)

/**
 * cd_profile_db_load:
 **/
gboolean
cd_profile_db_load (CdProfileDb *pdb,
		    const gchar *filename,
		    GError  **error)
{
	const gchar *statement;
	gboolean ret = TRUE;
	gchar *error_msg = NULL;
	gchar *path = NULL;
	gint rc;

	g_return_val_if_fail (CD_IS_PROFILE_DB (pdb), FALSE);
	g_return_val_if_fail (pdb->priv->db == NULL, FALSE);

	/* ensure the path exists */
	path = g_path_get_dirname (filename);
	ret = cd_main_mkdir_with_parents (path, error);
	if (!ret)
		goto out;

	g_debug ("CdProfileDb: trying to open database '%s'", filename);
	syslog (LOG_INFO, "Using profile database file %s", filename);
	rc = sqlite3_open (filename, &pdb->priv->db);
	if (rc != SQLITE_OK) {
		ret = FALSE;
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_INTERNAL,
			     "Can't open database: %s\n",
			     sqlite3_errmsg (pdb->priv->db));
		sqlite3_close (pdb->priv->db);
		goto out;
	}

	/* we don't need to keep doing fsync */
	sqlite3_exec (pdb->priv->db, "PRAGMA synchronous=OFF",
		      NULL, NULL, NULL);

	/* check schema */
	rc = sqlite3_exec (pdb->priv->db, "SELECT * FROM properties_pu LIMIT 1",
			   NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		statement = "CREATE TABLE properties_pu ("
			    "profile_id TEXT,"
			    "property TEXT,"
			    "uid INTEGER,"
			    "value TEXT,"
			    "PRIMARY KEY (profile_id, property, uid));";
		sqlite3_exec (pdb->priv->db, statement, NULL, NULL, NULL);
	}
out:
	g_free (path);
	return ret;
}

/**
 * cd_profile_db_empty:
 **/
gboolean
cd_profile_db_empty (CdProfileDb *pdb, GError **error)
{
	const gchar *statement;
	gboolean ret = TRUE;
	gchar *error_msg = NULL;
	gint rc;

	g_return_val_if_fail (CD_IS_PROFILE_DB (pdb), FALSE);
	g_return_val_if_fail (pdb->priv->db != NULL, FALSE);

	statement = "DELETE FROM properties_pu;";
	rc = sqlite3_exec (pdb->priv->db, statement,
			   NULL, NULL, &error_msg);
	if (rc != SQLITE_OK) {
		ret = FALSE;
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_INTERNAL,
			     "SQL error: %s",
			     error_msg);
		sqlite3_free (error_msg);
		goto out;
	}
out:
	return ret;
}

/**
 * cd_profile_db_set_property:
 **/
gboolean
cd_profile_db_set_property (CdProfileDb *pdb,
			    const gchar *profile_id,
			    const gchar *property,
			    guint uid,
			    const gchar *value,
			    GError  **error)
{
	gboolean ret = TRUE;
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;

	g_return_val_if_fail (CD_IS_PROFILE_DB (pdb), FALSE);
	g_return_val_if_fail (pdb->priv->db != NULL, FALSE);

	g_debug ("CdProfileDb: add profile property %s [%s=%s]",
		 profile_id, property, value);
	statement = sqlite3_mprintf ("INSERT OR REPLACE INTO properties_pu (profile_id, "
				     "property, uid, value) "
				     "VALUES ('%q', '%q', '%u', '%q');",
				     profile_id, property, uid, value);

	/* insert the entry */
	rc = sqlite3_exec (pdb->priv->db, statement, NULL, NULL, &error_msg);
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
 * cd_profile_db_remove:
 **/
gboolean
cd_profile_db_remove (CdProfileDb *pdb,
		      const gchar *profile_id,
		      const gchar *property,
		      guint uid,
		      GError  **error)
{
	gboolean ret = TRUE;
	gchar *error_msg = NULL;
	gchar *statement = NULL;
	gint rc;

	g_return_val_if_fail (CD_IS_PROFILE_DB (pdb), FALSE);
	g_return_val_if_fail (pdb->priv->db != NULL, FALSE);

	/* remove the entry */
	g_debug ("CdProfileDb: remove profile %s", profile_id);
	statement = sqlite3_mprintf ("DELETE FROM properties_pu WHERE "
				     "profile_id = '%q' AND "
				     "uid = '%i' AND "
				     "property = '%q' LIMIT 1;",
				     profile_id, uid, property);
	rc = sqlite3_exec (pdb->priv->db, statement, NULL, NULL, &error_msg);
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
 * cd_profile_db_sqlite_cb:
 **/
static gint
cd_profile_db_sqlite_cb (void *data,
			gint argc,
			gchar **argv,
			gchar **col_name)
{
	gchar **value = (gchar **) data;

	/* should only be one entry */
	g_debug ("CdProfileDb: got sql result %s", argv[0]);
	*value = g_strdup (argv[0]);
	return 0;
}

/**
 * cd_profile_db_get_property:
 **/
gboolean
cd_profile_db_get_property (CdProfileDb *pdb,
			   const gchar *profile_id,
			   const gchar *property,
			   guint uid,
			   gchar **value,
			   GError  **error)
{
	gboolean ret = TRUE;
	gchar *error_msg = NULL;
	gchar *statement;
	gint rc;

	g_return_val_if_fail (CD_IS_PROFILE_DB (pdb), FALSE);
	g_return_val_if_fail (pdb->priv->db != NULL, FALSE);

	g_debug ("CdProfileDb: get property %s for %s", property, profile_id);
	statement = sqlite3_mprintf ("SELECT value FROM properties_pu WHERE "
				     "profile_id = '%q' AND "
				     "uid = '%i' AND "
				     "property = '%q' LIMIT 1;",
				     profile_id, uid, property);

	/* retrieve the entry */
	rc = sqlite3_exec (pdb->priv->db,
			   statement,
			   cd_profile_db_sqlite_cb,
			   value,
			   &error_msg);
	if (rc != SQLITE_OK) {
		ret = FALSE;
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_INTERNAL,
			     "SQL error: %s",
			     error_msg);
		sqlite3_free (error_msg);
		goto out;
	}
out:
	sqlite3_free (statement);
	return ret;
}

/**
 * cd_profile_db_class_init:
 **/
static void
cd_profile_db_class_init (CdProfileDbClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_profile_db_finalize;
	g_type_class_add_private (klass, sizeof (CdProfileDbPrivate));
}

/**
 * cd_profile_db_init:
 **/
static void
cd_profile_db_init (CdProfileDb *pdb)
{
	pdb->priv = CD_PROFILE_DB_GET_PRIVATE (pdb);
}

/**
 * cd_profile_db_finalize:
 **/
static void
cd_profile_db_finalize (GObject *object)
{
	CdProfileDb *pdb;
	g_return_if_fail (CD_IS_PROFILE_DB (object));
	pdb = CD_PROFILE_DB (object);
	g_return_if_fail (pdb->priv != NULL);

	/* close the database */
	sqlite3_close (pdb->priv->db);

	G_OBJECT_CLASS (cd_profile_db_parent_class)->finalize (object);
}

/**
 * cd_profile_db_new:
 **/
CdProfileDb *
cd_profile_db_new (void)
{
	if (cd_profile_db_object != NULL) {
		g_object_ref (cd_profile_db_object);
	} else {
		cd_profile_db_object = g_object_new (CD_TYPE_PROFILE_DB, NULL);
		g_object_add_weak_pointer (cd_profile_db_object, &cd_profile_db_object);
	}
	return CD_PROFILE_DB (cd_profile_db_object);
}

