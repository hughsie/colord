/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2013 Richard Hughes <richard@hughsie.com>
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

/**
 * SECTION:cd-icc-store
 * @short_description: An object to monitor a directory full of ICC profiles
 */

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>

#include "cd-icc-store.h"

static void	cd_icc_store_finalize	(GObject	*object);

#define CD_ICC_STORE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_ICC_STORE, CdIccStorePrivate))

struct _CdIccStorePrivate
{
	CdIccLoadFlags		 load_flags;
	GPtrArray		*directory_array;
	GPtrArray		*icc_array;
	GResource		*cache;
};

enum {
	SIGNAL_ADDED,
	SIGNAL_REMOVED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (CdIccStore, cd_icc_store, G_TYPE_OBJECT)

#define CD_ICC_STORE_MAX_RECURSION_LEVELS	  2

static gboolean
cd_icc_store_search_path (CdIccStore *store,
			  const gchar *path,
			  guint depth,
			  GCancellable *cancellable,
			  GError **error);
static gboolean
cd_icc_store_search_path_child (CdIccStore *store,
				const gchar *path,
				GFileInfo *info,
				guint depth,
				GCancellable *cancellable,
				GError **error);

typedef struct {
	gchar		*path;
	GFileMonitor	*monitor;
} CdIccStoreDirHelper;

/**
 * cd_icc_store_helper_free:
 **/
static void
cd_icc_store_helper_free (CdIccStoreDirHelper *helper)
{
	g_free (helper->path);
	if (helper->monitor != NULL)
		g_object_unref (helper->monitor);
	g_free (helper);
}

/**
 * cd_icc_store_find_by_filename:
 * @store: a #CdIccStore instance.
 * @filename: a fully qualified filename
 *
 * Finds a ICC object in the store by filename.
 *
 * Return value: (transfer full): an ICC profile object or %NULL
 *
 * Since: 1.0.2
 **/
CdIcc *
cd_icc_store_find_by_filename (CdIccStore *store, const gchar *filename)
{
	CdIcc *tmp;
	guint i;
	GPtrArray *array = store->priv->icc_array;

	g_return_val_if_fail (CD_IS_ICC_STORE (store), NULL);
	g_return_val_if_fail (filename != NULL, NULL);

	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (g_strcmp0 (filename, cd_icc_get_filename (tmp)) == 0)
			return g_object_ref (tmp);
	}
	return NULL;
}

/**
 * cd_icc_store_find_by_checksum:
 * @store: a #CdIccStore instance.
 * @checksum: a checksum value
 *
 * Finds a ICC object in the store by checksum.
 *
 * Return value: (transfer full): an ICC profile object or %NULL
 *
 * Since: 1.0.2
 **/
CdIcc *
cd_icc_store_find_by_checksum (CdIccStore *store, const gchar *checksum)
{
	CdIcc *tmp;
	guint i;
	GPtrArray *array = store->priv->icc_array;

	g_return_val_if_fail (CD_IS_ICC_STORE (store), NULL);
	g_return_val_if_fail (checksum != NULL, NULL);

	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (g_strcmp0 (checksum, cd_icc_get_checksum (tmp)) == 0)
			return g_object_ref (tmp);
	}
	return NULL;
}

/**
 * cd_icc_store_find_by_directory:
 **/
static CdIccStoreDirHelper *
cd_icc_store_find_by_directory (CdIccStore *store, const gchar *path)
{
	CdIccStoreDirHelper *tmp;
	guint i;
	GPtrArray *array = store->priv->directory_array;

	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (g_strcmp0 (path, tmp->path) == 0)
			return tmp;
	}
	return NULL;
}

/**
 * cd_icc_store_remove_icc:
 **/
static gboolean
cd_icc_store_remove_icc (CdIccStore *store, const gchar *filename)
{
	CdIcc *icc = NULL;
	gboolean ret = FALSE;

	/* find exact pointer */
	icc = cd_icc_store_find_by_filename (store, filename);
	if (icc == NULL)
		goto out;

	/* we have a ref so we can emit the signal */
	ret = g_ptr_array_remove (store->priv->icc_array, icc);
	if (!ret) {
		g_warning ("failed to remove %s", filename);
		goto out;
	}

	/* emit a signal */
	g_signal_emit (store, signals[SIGNAL_REMOVED], 0, icc);
out:
	if (icc != NULL)
		g_object_unref (icc);
	return ret;
}

/**
 * cd_icc_store_add_icc:
 **/
static gboolean
cd_icc_store_add_icc (CdIccStore *store, GFile *file, GError **error)
{
	CdIcc *icc;
	CdIcc *icc_tmp = NULL;
	CdIccStorePrivate *priv = store->priv;
	gboolean ret;
	gchar *cache_key = NULL;
	gchar *filename;
	gchar *basename = NULL;
	GBytes *data = NULL;

	/* use the GResource cache if available */
	icc = cd_icc_new ();
	filename = g_file_get_path (file);
	if (store->priv->cache != NULL) {
		if (g_str_has_prefix (filename, "/usr/share/color/icc/colord/")) {
			cache_key = g_build_filename ("/org/freedesktop/colord",
						      "profiles",
						      filename + 28,
						      NULL);
			data = g_resource_lookup_data (store->priv->cache,
						       cache_key,
						       G_RESOURCE_LOOKUP_FLAGS_NONE,
						       NULL);
		}
	}

	/* parse new icc object */
	if (data != NULL) {
		g_debug ("Using built-in %s", basename);
		cd_icc_set_filename (icc, filename);
		ret = cd_icc_load_data (icc,
					g_bytes_get_data (data, NULL),
					g_bytes_get_size (data),
					CD_ICC_LOAD_FLAGS_METADATA,
					error);
		if (!ret)
			goto out;
	} else {
		ret = cd_icc_load_file (icc,
					file,
					store->priv->load_flags,
					NULL,
					error);
		if (!ret)
			goto out;
	}

	/* check it's not a duplicate */
	icc_tmp = cd_icc_store_find_by_checksum (store, cd_icc_get_checksum (icc));
	if (icc_tmp != NULL) {
		ret = TRUE;
		g_debug ("CdIccStore: Failed to add %s as profile %s "
			 "already exists with the same checksum of %s",
			 filename,
			 cd_icc_get_filename (icc_tmp),
			 cd_icc_get_checksum (icc_tmp));
		goto out;
	}

	/* add to list */
	g_ptr_array_add (priv->icc_array, g_object_ref (icc));

	/* emit a signal */
	g_signal_emit (store, signals[SIGNAL_ADDED], 0, icc);
out:
	if (data != NULL)
		g_bytes_unref (data);
	if (icc_tmp != NULL)
		g_object_unref (icc_tmp);
	g_object_unref (icc);
	g_free (filename);
	g_free (cache_key);
	return ret;
}

/**
 * cd_icc_store_created_query_info_cb:
 **/
static void
cd_icc_store_created_query_info_cb (GObject *source_object,
				    GAsyncResult *res,
				    gpointer user_data)
{
	GFileInfo *info;
	GError *error = NULL;
	gchar *path;
	GFile *file = G_FILE (source_object);
	GFile *parent;
	gboolean ret;
	CdIccStore *store = CD_ICC_STORE (user_data);

	info = g_file_query_info_finish (file, res, NULL);
	if (info == NULL)
		return;
	parent = g_file_get_parent (file);
	path = g_file_get_path (parent);
	ret = cd_icc_store_search_path_child (store,
					      path,
					      info,
					      0,
					      NULL,
					      &error);
	if (!ret) {
		g_warning ("failed to search file: %s",
			   error->message);
		g_error_free (error);
	}
	g_free (path);
	g_object_unref (info);
	g_object_unref (parent);
}

/**
 * cd_icc_store_remove_from_prefix:
 **/
static void
cd_icc_store_remove_from_prefix (CdIccStore *store, const gchar *prefix)
{
	CdIccStorePrivate *priv = store->priv;
	CdIcc *tmp;
	const gchar *filename;
	guint i;

	for (i = 0; i < priv->icc_array->len; i++) {
		tmp = g_ptr_array_index (priv->icc_array, i);
		filename = cd_icc_get_filename (tmp);
		if (g_str_has_prefix (filename, prefix)) {
			g_debug ("auto-removed %s as path removed", prefix);
			cd_icc_store_remove_icc (store, filename);
		}
	}
}

/**
 * cd_icc_store_file_monitor_changed_cb:
 **/
static void
cd_icc_store_file_monitor_changed_cb (GFileMonitor *monitor,
				      GFile *file,
				      GFile *other_file,
				      GFileMonitorEvent event_type,
				      CdIccStore *store)
{
	gchar *path = NULL;
	gchar *parent_path = NULL;
	CdIcc *tmp;
	CdIccStoreDirHelper *helper;

	/* icc was deleted */
	if (event_type == G_FILE_MONITOR_EVENT_DELETED) {

		/* we can either have two things here, a directory or a
		 * file. We can't call g_file_query_info_async() as the
		 * inode doesn't exist anymore */
		path = g_file_get_path (file);
		tmp = cd_icc_store_find_by_filename (store, path);
		if (tmp != NULL) {
			/* is a file */
			cd_icc_store_remove_icc (store, path);
			goto out;
		}

		/* is a directory, urgh. Remove all ICCs there. */
		cd_icc_store_remove_from_prefix (store, path);
		helper = cd_icc_store_find_by_directory (store, path);
		if (helper != NULL) {
			g_ptr_array_remove (store->priv->directory_array,
					    helper);
		}
		goto out;
	}

	/* ignore temp files */
	path = g_file_get_path (file);
	if (g_strrstr (path, ".goutputstream") != NULL) {
		g_debug ("ignoring gvfs temporary file");
		goto out;
	}

	/* only care about created objects */
	if (event_type == G_FILE_MONITOR_EVENT_CREATED) {
		g_file_query_info_async (file,
					 G_FILE_ATTRIBUTE_STANDARD_NAME ","
					 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
					 G_FILE_ATTRIBUTE_STANDARD_TYPE,
					 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					 G_PRIORITY_LOW,
					 NULL,
					 cd_icc_store_created_query_info_cb,
					 store);
		goto out;
	}
out:
	g_free (path);
	g_free (parent_path);
}

/**
 * cd_icc_store_search_path_child:
 **/
static gboolean
cd_icc_store_search_path_child (CdIccStore *store,
				const gchar *path,
				GFileInfo *info,
				guint depth,
				GCancellable *cancellable,
				GError **error)
{
	const gchar *name;
	const gchar *type;
	gboolean ret = TRUE;
	gchar *full_path;
	GFile *file = NULL;

	/* further down the worm-hole */
	name = g_file_info_get_name (info);
	full_path = g_build_filename (path, name, NULL);
	if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
		ret = cd_icc_store_search_path (store,
						full_path,
						depth + 1,
						cancellable,
						error);
		if (!ret)
			goto out;
		goto out;
	}

	/* ignore temp files */
	if (g_strrstr (full_path, ".goutputstream") != NULL) {
		g_debug ("ignoring gvfs temporary file");
		goto out;
	}

	/* check type */
	type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	if (g_strcmp0 (type, "application/vnd.iccprofile") != 0) {
		g_debug ("Incorrect content type for %s, got %s", full_path, type);
		goto out;
	}

	/* is a file */
	file = g_file_new_for_path (full_path);
	ret = cd_icc_store_add_icc (store, file, error);
	if (!ret)
		goto out;
out:
	if (file != NULL)
		g_object_unref (file);
	g_free (full_path);
	return ret;
}

/**
 * cd_icc_store_search_path:
 **/
static gboolean
cd_icc_store_search_path (CdIccStore *store,
			  const gchar *path,
			  guint depth,
			  GCancellable *cancellable,
			  GError **error)
{
	CdIccStoreDirHelper *helper;
	GFileEnumerator *enumerator = NULL;
	GFile *file = NULL;
	gboolean ret = TRUE;
	GFileInfo *info;
	GError *error_local = NULL;

	/* check sanity */
	if (depth > CD_ICC_STORE_MAX_RECURSION_LEVELS) {
		ret = FALSE;
		g_set_error (error,
			     CD_ICC_ERROR,
			     CD_ICC_ERROR_FAILED_TO_OPEN,
			     "cannot recurse more than %i levels deep",
			     CD_ICC_STORE_MAX_RECURSION_LEVELS);
		goto out;
	}

	/* add an inotify watch if not already added */
	file = g_file_new_for_path (path);
	helper = cd_icc_store_find_by_directory (store, path);
	if (helper == NULL) {
		helper = g_new0 (CdIccStoreDirHelper, 1);
		helper->path = g_strdup (path);
		helper->monitor = g_file_monitor_directory (file,
							    G_FILE_MONITOR_NONE,
							    NULL,
							    error);
		if (helper->monitor == NULL) {
			ret = FALSE;
			cd_icc_store_helper_free (helper);
			goto out;
		}
		g_signal_connect (helper->monitor, "changed",
				  G_CALLBACK(cd_icc_store_file_monitor_changed_cb),
				  store);
		g_ptr_array_add (store->priv->directory_array, helper);
	}

	/* get contents of directory */
	enumerator = g_file_enumerate_children (file,
						G_FILE_ATTRIBUTE_STANDARD_NAME ","
						G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
						G_FILE_ATTRIBUTE_STANDARD_TYPE,
						G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
						cancellable,
						error);
	if (enumerator == NULL) {
		ret = FALSE;
		helper = cd_icc_store_find_by_directory (store, path);
		if (helper != NULL)
			g_ptr_array_remove (store->priv->directory_array, helper);
		goto out;
	}

	/* get all the files */
	while (TRUE) {
		info = g_file_enumerator_next_file (enumerator,
						    cancellable,
						    &error_local);
		if (info == NULL && error_local != NULL) {
			ret = FALSE;
			g_propagate_error (error, error_local);
			goto out;
		}

		/* special value, meaning "no more files to process" */
		if (info == NULL)
			break;

		/* process this child */
		ret = cd_icc_store_search_path_child (store,
						      path,
						      info,
						      depth,
						      cancellable,
						      error);
		g_object_unref (info);
		if (!ret)
			goto out;
	}
out:
	if (enumerator != NULL)
		g_object_unref (enumerator);
	g_object_unref (file);
	return ret;
}

/**
 * cd_icc_store_set_load_flags:
 * @store: a #CdIccStore instance.
 * @load_flags: #CdIccLoadFlags, e.g. %CD_ICC_LOAD_FLAGS_TRANSLATIONS
 *
 * Sets the load flags to use when loading newly added profiles
 *
 * Since: 1.0.2
 **/
void
cd_icc_store_set_load_flags (CdIccStore *store, CdIccLoadFlags load_flags)
{
	g_return_if_fail (CD_IS_ICC_STORE (store));
	store->priv->load_flags = load_flags | CD_ICC_LOAD_FLAGS_FALLBACK_MD5;
}

/**
 * cd_icc_store_get_load_flags:
 * @store: a #CdIccStore instance.
 *
 * Gets the load flags to use when loading newly added profiles
 *
 * Return value: the load flags to use
 *
 * Since: 1.0.2
 **/
CdIccLoadFlags
cd_icc_store_get_load_flags (CdIccStore *store)
{
	g_return_val_if_fail (CD_IS_ICC_STORE (store), 0);
	return store->priv->load_flags;
}

/**
 * cd_icc_store_set_cache:
 * @store: a #CdIccStore instance.
 * @cache: a #GResource
 *
 * Sets an optional cache to use when reading profiles. This is probably
 * only useful to the colord daemon. This function can only be called once.
 *
 * Since: 1.0.2
 **/
void
cd_icc_store_set_cache (CdIccStore *store, GResource *cache)
{
	g_return_if_fail (CD_IS_ICC_STORE (store));
	g_return_if_fail (store->priv->cache == NULL);
	store->priv->cache = g_resource_ref (cache);
}

/**
 * cd_icc_store_get_all:
 * @store: a #CdIccStore instance.
 *
 * Gets the list of #CdIcc objects in the store
 *
 * Return value: (transfer container) (element-type CdIcc): ICC profile objects
 *
 * Since: 1.0.2
 **/
GPtrArray *
cd_icc_store_get_all (CdIccStore *store)
{
	g_return_val_if_fail (CD_IS_ICC_STORE (store), NULL);
	return g_ptr_array_ref (store->priv->icc_array);
}

/**
 * cd_icc_store_search_kind:
 * @store: a #CdIccStore instance.
 * @search_kind: a #CdIccStoreSearchKind, e.g. %CD_ICC_STORE_SEARCH_KIND_USER
 * @search_flags: a #CdIccStoreSearchFlags, e.g. %CD_ICC_STORE_SEARCH_FLAGS_CREATE_LOCATION
 * @cancellable: A #GCancellable or %NULL
 * @error: A #GError or %NULL
 *
 * Adds a location to be watched for ICC profiles
 *
 * Return value: %TRUE for success
 *
 * Since: 1.0.2
 **/
gboolean
cd_icc_store_search_kind (CdIccStore *store,
			  CdIccStoreSearchKind search_kind,
			  CdIccStoreSearchFlags search_flags,
			  GCancellable *cancellable,
			  GError **error)
{
	gboolean ret = TRUE;
	gchar *tmp;
	GPtrArray *locations;
	guint i;

	g_return_val_if_fail (CD_IS_ICC_STORE (store), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* get the locations for each kind */
	locations = g_ptr_array_new_with_free_func (g_free);
	switch (search_kind) {
	case CD_ICC_STORE_SEARCH_KIND_USER:
		tmp = g_build_filename (g_get_user_data_dir (), "icc", NULL);
		g_ptr_array_add (locations, tmp);
		tmp = g_build_filename (g_get_home_dir (), ".color", "icc", NULL);
		g_ptr_array_add (locations, tmp);
		break;
	case CD_ICC_STORE_SEARCH_KIND_MACHINE:
		g_ptr_array_add (locations, g_strdup (CD_SYSTEM_PROFILES_DIR));
		g_ptr_array_add (locations, g_strdup ("/var/lib/color/icc"));
		break;
	case CD_ICC_STORE_SEARCH_KIND_SYSTEM:
		g_ptr_array_add (locations, g_strdup ("/usr/share/color/icc"));
		g_ptr_array_add (locations, g_strdup ("/usr/local/share/color/icc"));
		g_ptr_array_add (locations, g_strdup ("/Library/ColorSync/Profiles/Displays"));
		break;
	default:
		break;
	}

	/* add any found locations */
	for (i = 0; i < locations->len; i++) {
		tmp = g_ptr_array_index (locations, i);
		ret = cd_icc_store_search_location (store,
						    tmp,
						    search_flags,
						    cancellable,
						    error);
		if (!ret)
			goto out;

		/* only create the first location */
		search_flags &= ~CD_ICC_STORE_SEARCH_FLAGS_CREATE_LOCATION;
	}
out:
	g_ptr_array_unref (locations);
	return ret;
}

/**
 * cd_icc_store_search_location:
 * @store: a #CdIccStore instance.
 * @location: a fully qualified path
 * @search_flags: #CdIccStoreSearchFlags, e.g. %CD_ICC_STORE_SEARCH_FLAGS_CREATE_LOCATION
 * @cancellable: A #GCancellable or %NULL
 * @error: A #GError or %NULL
 *
 * Adds a location to be watched for ICC profiles
 *
 * Return value: %TRUE for success
 *
 * Since: 1.0.2
 **/
gboolean
cd_icc_store_search_location (CdIccStore *store,
			      const gchar *location,
			      CdIccStoreSearchFlags search_flags,
			      GCancellable *cancellable,
			      GError **error)
{
	gboolean ret = TRUE;
	gboolean exists;
	GFile *file;

	g_return_val_if_fail (CD_IS_ICC_STORE (store), FALSE);
	g_return_val_if_fail (location != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* does folder exist? */
	file = g_file_new_for_path (location);
	exists = g_file_query_exists (file, cancellable);
	if (!exists) {
		if ((search_flags & CD_ICC_STORE_SEARCH_FLAGS_CREATE_LOCATION) > 0) {
			ret = g_file_make_directory_with_parents (file, cancellable, error);
			if (!ret)
				goto out;
		} else {
			/* the directory does not exist */
			goto out;
		}
	}

	/* search all */
	ret = cd_icc_store_search_path (store, location, 0, cancellable, error);
	if (!ret)
		goto out;
out:
	g_object_unref (file);
	return ret;
}

/**
 * cd_icc_store_class_init:
 **/
static void
cd_icc_store_class_init (CdIccStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_icc_store_finalize;

	/**
	 * CdIccStore::added:
	 * @profile: the #CdIccStore instance that emitted the signal
	 * @icc: the #CdIcc that was added
	 *
	 * The ::added signal is emitted when an ICC profile has been added.
	 *
	 * Since: 1.0.2
	 **/
	signals[SIGNAL_ADDED] =
		g_signal_new ("added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdIccStoreClass, added),
			      NULL, NULL, g_cclosure_marshal_generic,
			      G_TYPE_NONE, 1, CD_TYPE_ICC);
	/**
	 * CdIccStore::removed:
	 * @profile: the #CdIccStore instance that emitted the signal
	 * @icc: the #CdIcc that was removed
	 *
	 * The ::removed signal is emitted when an ICC profile has been removed.
	 *
	 * Since: 1.0.2
	 **/
	signals[SIGNAL_REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdIccStoreClass, removed),
			      NULL, NULL, g_cclosure_marshal_generic,
			      G_TYPE_NONE, 1, CD_TYPE_ICC);

	g_type_class_add_private (klass, sizeof (CdIccStorePrivate));
}

/**
 * cd_icc_store_init:
 **/
static void
cd_icc_store_init (CdIccStore *store)
{
	store->priv = CD_ICC_STORE_GET_PRIVATE (store);
	store->priv->load_flags = CD_ICC_LOAD_FLAGS_FALLBACK_MD5;
	store->priv->icc_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	store->priv->directory_array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_icc_store_helper_free);
}

/**
 * cd_icc_store_finalize:
 **/
static void
cd_icc_store_finalize (GObject *object)
{
	CdIccStore *store = CD_ICC_STORE (object);
	CdIccStorePrivate *priv = store->priv;

	g_ptr_array_unref (priv->icc_array);
	g_ptr_array_unref (priv->directory_array);
	if (priv->cache != NULL)
		g_resource_unref (priv->cache);

	G_OBJECT_CLASS (cd_icc_store_parent_class)->finalize (object);
}

/**
 * cd_icc_store_new:
 *
 * Creates a new #CdIccStore object.
 *
 * Return value: a new CdIccStore object.
 *
 * Since: 1.0.2
 **/
CdIccStore *
cd_icc_store_new (void)
{
	CdIccStore *store;
	store = g_object_new (CD_TYPE_ICC_STORE, NULL);
	return CD_ICC_STORE (store);
}
