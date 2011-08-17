/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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

#include <glib-object.h>
#include <gio/gio.h>

#include "cd-profile-store.h"

static void     cd_profile_store_finalize	(GObject	*object);
static gboolean	cd_profile_store_search_path	(CdProfileStore	*profile_store,
						 const gchar	*path);

#define CD_PROFILE_STORE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_PROFILE_STORE, CdProfileStorePrivate))

/**
 * CdProfileStorePrivate:
 **/
struct _CdProfileStorePrivate
{
	GPtrArray			*profile_array;
	GPtrArray			*monitor_array;
	GPtrArray			*directory_array;
	GVolumeMonitor			*volume_monitor;
	guint				 mount_added_id;
};

enum {
	SIGNAL_ADDED,
	SIGNAL_REMOVED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (CdProfileStore, cd_profile_store, G_TYPE_OBJECT)

/**
 * cd_profile_store_in_array:
 **/
static gboolean
cd_profile_store_in_array (GPtrArray *array, const gchar *text)
{
	const gchar *tmp;
	guint i;

	for (i=0; i<array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (g_strcmp0 (text, tmp) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * cd_profile_store_remove_profile:
 **/
static gboolean
cd_profile_store_remove_profile (CdProfileStore *profile_store,
				 CdProfile *profile)
{
	gboolean ret;
	CdProfileStorePrivate *priv = profile_store->priv;

	/* grab a temporary reference on this profile */
	g_object_ref (profile);

	/* remove from list */
	ret = g_ptr_array_remove (priv->profile_array, profile);
	if (!ret) {
		g_warning ("CdProfileStore: failed to remove %s",
			   cd_profile_get_filename (profile));
		goto out;
	}

	/* emit a signal */
	g_debug ("CdProfileStore: emit removed (and changed): %s",
		 cd_profile_get_checksum (profile));
	g_signal_emit (profile_store, signals[SIGNAL_REMOVED], 0, profile);
out:
	g_object_unref (profile);
	return ret;
}

/**
 * cd_profile_store_notify_filename_cb:
 **/
static void
cd_profile_store_notify_filename_cb (CdProfile *profile,
				     GParamSpec *pspec,
				     CdProfileStore *profile_store)
{
	cd_profile_store_remove_profile (profile_store, profile);
}

/**
 * cd_profile_store_get_by_checksum:
 **/
static CdProfile *
cd_profile_store_get_by_checksum (CdProfileStore *profile_store,
				  const gchar *checksum)
{
	guint i;
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;
	const gchar *checksum_tmp;
	CdProfileStorePrivate *priv = profile_store->priv;

	g_return_val_if_fail (CD_IS_PROFILE_STORE (profile_store), NULL);
	g_return_val_if_fail (checksum != NULL, NULL);

	/* find profile */
	for (i=0; i<priv->profile_array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->profile_array, i);
		checksum_tmp = cd_profile_get_checksum (profile_tmp);
		if (g_strcmp0 (checksum, checksum_tmp) == 0) {
			profile = g_object_ref (profile_tmp);
			goto out;
		}
	}
out:
	return profile;
}

/**
 * cd_profile_store_get_by_filename:
 **/
static CdProfile *
cd_profile_store_get_by_filename (CdProfileStore *profile_store,
				  const gchar *filename)
{
	guint i;
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;
	const gchar *filename_tmp;
	CdProfileStorePrivate *priv = profile_store->priv;

	g_return_val_if_fail (CD_IS_PROFILE_STORE (profile_store), NULL);
	g_return_val_if_fail (filename != NULL, NULL);

	/* find profile */
	for (i=0; i<priv->profile_array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->profile_array, i);
		filename_tmp = cd_profile_get_filename (profile_tmp);
		if (g_strcmp0 (filename, filename_tmp) == 0) {
			profile = g_object_ref (profile_tmp);
			goto out;
		}
	}
out:
	return profile;
}

/**
 * cd_profile_store_add_profile:
 **/
static gboolean
cd_profile_store_add_profile (CdProfileStore *profile_store,
			      GFile *file)
{
	gboolean ret = FALSE;
	CdProfile *profile = NULL;
	CdProfile *profile_tmp = NULL;
	GError *error = NULL;
	gchar *filename = NULL;
	const gchar *checksum;
	CdProfileStorePrivate *priv = profile_store->priv;

	/* already added? */
	filename = g_file_get_path (file);
	profile = cd_profile_store_get_by_filename (profile_store, filename);
	if (profile != NULL)
		goto out;

	/* is system wide? */
	profile = cd_profile_new ();
	if (g_str_has_prefix (filename, "/usr/share/color") ||
	    g_str_has_prefix (filename, "/var/lib/color"))
		cd_profile_set_is_system_wide (profile, TRUE);

	/* parse the profile name */
	ret = cd_profile_set_filename (profile, filename, &error);
	if (!ret) {
		g_warning ("CdProfileStore: failed to add profile '%s': %s",
			   filename, error->message);
		g_error_free (error);
		goto out;
	}

	/* check the profile has not been added already */
	checksum = cd_profile_get_checksum (profile);
	profile_tmp = cd_profile_store_get_by_checksum (profile_store, checksum);
	if (profile_tmp != NULL) {
		/* remove the old profile in favour of the new one */
		cd_profile_store_remove_profile (profile_store, profile_tmp);
	}

	/* add to array */
	g_debug ("CdProfileStore: parsed new profile '%s'", filename);
	g_ptr_array_add (priv->profile_array, g_object_ref (profile));
	g_signal_connect (profile, "notify::file",
			  G_CALLBACK(cd_profile_store_notify_filename_cb),
			  profile_store);

	/* emit a signal */
	g_debug ("CdProfileStore: emit added (and changed): %s", filename);
	g_signal_emit (profile_store, signals[SIGNAL_ADDED], 0, profile);
out:
	g_free (filename);
	if (profile_tmp != NULL)
		g_object_unref (profile_tmp);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_profile_store_file_monitor_changed_cb:
 **/
static void
cd_profile_store_file_monitor_changed_cb (GFileMonitor *monitor,
					  GFile *file, GFile *other_file,
					  GFileMonitorEvent event_type,
					  CdProfileStore *profile_store)
{
	gchar *path = NULL;
	gchar *parent_path = NULL;
	GFile *parent = NULL;

	/* only care about created objects */
	if (event_type != G_FILE_MONITOR_EVENT_CREATED)
		goto out;

	/* ignore temp files */
	path = g_file_get_path (file);
	if (g_strrstr (path, ".goutputstream") != NULL) {
		g_debug ("CdProfileStore: ignoring gvfs temporary file");
		goto out;
	}

	/* just rescan the correct directory */
	parent = g_file_get_parent (file);
	parent_path = g_file_get_path (parent);
	g_debug ("CdProfileStore: %s was added, rescanning %s", path, parent_path);
	cd_profile_store_search_path (profile_store, parent_path);
out:
	if (parent != NULL)
		g_object_unref (parent);
	g_free (path);
	g_free (parent_path);
}

/**
 * cd_profile_store_search_path:
 **/
static gboolean
cd_profile_store_search_path (CdProfileStore *profile_store,
			      const gchar *path)
{
	GDir *dir = NULL;
	GError *error = NULL;
	gboolean ret;
	gboolean success = FALSE;
	const gchar *name;
	gchar *full_path;
	CdProfileStorePrivate *priv = profile_store->priv;
	GFileMonitor *monitor = NULL;
	GFile *file = NULL;

	/* add if correct type */
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {

		/* check the file actually is a profile when we try to parse it */
		file = g_file_new_for_path (path);
		success = cd_profile_store_add_profile (profile_store, file);
		goto out;
	}

	/* get contents */
	dir = g_dir_open (path, 0, &error);
	if (dir == NULL) {
		g_debug ("CdProfileStore: failed to open: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add an inotify watch if not already added? */
	ret = cd_profile_store_in_array (priv->directory_array, path);
	if (!ret) {
		file = g_file_new_for_path (path);
		monitor = g_file_monitor_directory (file,
						    G_FILE_MONITOR_NONE,
						    NULL,
						    &error);
		if (monitor == NULL) {
			g_debug ("CdProfileStore: failed to monitor path: %s",
				 error->message);
			g_error_free (error);
			goto out;
		}

		/* don't allow many files to cause re-scan after rescan */
		g_file_monitor_set_rate_limit (monitor, 1000);
		g_signal_connect (monitor, "changed",
				  G_CALLBACK(cd_profile_store_file_monitor_changed_cb),
				  profile_store);
		g_ptr_array_add (priv->monitor_array, g_object_ref (monitor));
		g_ptr_array_add (priv->directory_array, g_strdup (path));
	}

	/* process entire tree */
	do {
		name = g_dir_read_name (dir);
		if (name == NULL)
			break;

		/* make the compete path */
		full_path = g_build_filename (path, name, NULL);
		ret = cd_profile_store_search_path (profile_store, full_path);
		if (ret)
			success = TRUE;
		g_free (full_path);
	} while (TRUE);
out:
	if (monitor != NULL)
		g_object_unref (monitor);
	if (file != NULL)
		g_object_unref (file);
	if (dir != NULL)
		g_dir_close (dir);
	return success;
}

/**
 * cd_profile_store_add_profiles_from_mounted_volume:
 **/
static gboolean
cd_profile_store_add_profiles_from_mounted_volume (CdProfileStore *profile_store,
						   GMount *mount)
{
	GFile *root;
	gchar *path;
	gchar *path_root;
	const gchar *type;
	GFileInfo *info;
	GError *error = NULL;
	gboolean ret;
	gboolean success = FALSE;

	/* get the mount root */
	root = g_mount_get_root (mount);
	path_root = g_file_get_path (root);
	if (path_root == NULL)
		goto out;

	/* get the filesystem type */
	info = g_file_query_filesystem_info (root,
					     G_FILE_ATTRIBUTE_FILESYSTEM_TYPE,
					     NULL, &error);
	if (info == NULL) {
		g_warning ("CdProfileStore: failed to get filesystem type: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	type = g_file_info_get_attribute_string (info,
						 G_FILE_ATTRIBUTE_FILESYSTEM_TYPE);
	g_debug ("CdProfileStore: filesystem mounted on %s has type %s",
		 path_root, type);

	/* only scan hfs volumes for OSX */
	if (g_strcmp0 (type, "hfs") == 0) {
		path = g_build_filename (path_root,
					 "Library",
					 "ColorSync",
					 "Profiles",
					 "Displays",
					 NULL);
		ret = cd_profile_store_search_path (profile_store, path);
		if (ret)
			success = TRUE;
		g_free (path);

		/* no more matching */
		goto out;
	}

	/* and fat32 and ntfs for windows */
	if (g_strcmp0 (type, "ntfs") == 0 || g_strcmp0 (type, "msdos") == 0) {

		/* Windows XP */
		path = g_build_filename (path_root,
					 "Windows",
					 "system32",
					 "spool",
					 "drivers",
					 "color",
					 NULL);
		ret = cd_profile_store_search_path (profile_store, path);
		if (ret)
			success = TRUE;
		g_free (path);

		/* Windows 2000 */
		path = g_build_filename (path_root,
					 "Winnt",
					 "system32",
					 "spool",
					 "drivers",
					 "color", NULL);
		ret = cd_profile_store_search_path (profile_store, path);
		if (ret)
			success = TRUE;
		g_free (path);

		/* Windows 98 and ME */
		path = g_build_filename (path_root,
					 "Windows",
					 "System",
					 "Color",
					 NULL);
		ret = cd_profile_store_search_path (profile_store, path);
		if (ret)
			success = TRUE;
		g_free (path);

		/* no more matching */
		goto out;
	}
out:
	g_free (path_root);
	g_object_unref (root);
	return success;
}

/**
 * cd_profile_store_add_profiles_from_mounted_volumes:
 **/
static gboolean
cd_profile_store_add_profiles_from_mounted_volumes (CdProfileStore *profile_store)
{
	gboolean ret;
	gboolean success = FALSE;
	GList *mounts, *l;
	GMount *mount;
	CdProfileStorePrivate *priv = profile_store->priv;

	/* get all current mounts */
	mounts = g_volume_monitor_get_mounts (priv->volume_monitor);
	for (l = mounts; l != NULL; l = l->next) {
		mount = l->data;
		ret = cd_profile_store_add_profiles_from_mounted_volume (profile_store, mount);
		if (ret)
			success = TRUE;
		g_object_unref (mount);
	}
	g_list_free (mounts);
	return success;
}

/**
 * cd_profile_store_volume_monitor_mount_added_cb:
 **/
static void
cd_profile_store_volume_monitor_mount_added_cb (GVolumeMonitor *volume_monitor,
						GMount *mount,
						CdProfileStore *profile_store)
{
	cd_profile_store_add_profiles_from_mounted_volume (profile_store, mount);
}

/**
 * cd_profile_mount_tracking_enable:
 **/
static void
cd_profile_mount_tracking_enable (CdProfileStore *profile_store)
{
	if (profile_store->priv->mount_added_id != 0)
		return;
	profile_store->priv->mount_added_id =
		g_signal_connect (profile_store->priv->volume_monitor,
				  "mount-added",
				  G_CALLBACK(cd_profile_store_volume_monitor_mount_added_cb),
				  profile_store);
}

/**
 * cd_profile_mount_tracking_disable:
 **/
static void
cd_profile_mount_tracking_disable (CdProfileStore *profile_store)
{
	if (profile_store->priv->mount_added_id == 0)
		return;
	g_signal_handler_disconnect (profile_store->priv->volume_monitor,
				     profile_store->priv->mount_added_id);
	profile_store->priv->mount_added_id = 0;
}

/**
 * cd_profile_store_search:
 **/
gboolean
cd_profile_store_search (CdProfileStore *profile_store,
			 CdProfileSearchFlags flags)
{
	gboolean ret;
	gboolean success = FALSE;

	/* get OSX and Linux system-wide profiles */
	if (flags & CD_PROFILE_STORE_SEARCH_SYSTEM) {
		ret = cd_profile_store_search_path (profile_store,
						    "/usr/share/color/icc");
		if (ret)
			success = TRUE;
		ret = cd_profile_store_search_path (profile_store, 
						    "/usr/local/share/color/icc");
		if (ret)
			success = TRUE;
		ret = cd_profile_store_search_path (profile_store, 
						    "/Library/ColorSync/Profiles/Displays");
		if (ret)
			success = TRUE;
	}

	/* get OSX and Windows system-wide profiles when using Linux */
	if (flags & CD_PROFILE_STORE_SEARCH_VOLUMES) {
		ret = cd_profile_store_add_profiles_from_mounted_volumes (profile_store);
		if (ret)
			success = TRUE;
		cd_profile_mount_tracking_enable (profile_store);
	} else {
		cd_profile_mount_tracking_disable (profile_store);
	}

	/* get machine specific profiles */
	if (flags & CD_PROFILE_STORE_SEARCH_MACHINE) {
		ret = cd_profile_store_search_path (profile_store,
						    "/var/lib/color/icc");
		if (ret)
			success = TRUE;
	}

	return success;
}

/**
 * cd_profile_store_class_init:
 **/
static void
cd_profile_store_class_init (CdProfileStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_profile_store_finalize;
	signals[SIGNAL_ADDED] =
		g_signal_new ("added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdProfileStoreClass, added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);
	signals[SIGNAL_REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdProfileStoreClass, removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);

	g_type_class_add_private (klass, sizeof (CdProfileStorePrivate));
}

/**
 * cd_profile_store_init:
 **/
static void
cd_profile_store_init (CdProfileStore *profile_store)
{
	profile_store->priv = CD_PROFILE_STORE_GET_PRIVATE (profile_store);
	profile_store->priv->profile_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	profile_store->priv->monitor_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	profile_store->priv->directory_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

	/* watch for volumes to be connected */
	profile_store->priv->volume_monitor = g_volume_monitor_get ();
}

/**
 * cd_profile_store_finalize:
 **/
static void
cd_profile_store_finalize (GObject *object)
{
	CdProfileStore *profile_store = CD_PROFILE_STORE (object);
	CdProfileStorePrivate *priv = profile_store->priv;

	cd_profile_mount_tracking_disable (profile_store);
	g_ptr_array_unref (priv->profile_array);
	g_ptr_array_unref (priv->monitor_array);
	g_ptr_array_unref (priv->directory_array);
	g_object_unref (priv->volume_monitor);

	G_OBJECT_CLASS (cd_profile_store_parent_class)->finalize (object);
}

/**
 * cd_profile_store_new:
 **/
CdProfileStore *
cd_profile_store_new (void)
{
	CdProfileStore *profile_store;
	profile_store = g_object_new (CD_TYPE_PROFILE_STORE, NULL);
	return CD_PROFILE_STORE (profile_store);
}

