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

#include <glib-object.h>

#include "cd-common.h"
#include "cd-profile-array.h"

static void     cd_profile_array_finalize	(GObject     *object);

#define GET_PRIVATE(o) (cd_profile_array_get_instance_private (o))

/**
 * CdProfileArrayPrivate:
 *
 * Private #CdProfileArray data
 **/
typedef struct
{
	GPtrArray			*array;
} CdProfileArrayPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (CdProfileArray, cd_profile_array, G_TYPE_OBJECT)

static gpointer cd_profile_array_object = NULL;

/**
 * cd_profile_array_add:
 **/
void
cd_profile_array_add (CdProfileArray *profile_array, CdProfile *profile)
{
	CdProfileArrayPrivate *priv = GET_PRIVATE (profile_array);
	g_return_if_fail (CD_IS_PROFILE_ARRAY (profile_array));
	g_return_if_fail (CD_IS_PROFILE (profile));
	g_ptr_array_add (priv->array, g_object_ref (profile));
}

/**
 * cd_profile_array_remove:
 **/
void
cd_profile_array_remove (CdProfileArray *profile_array, CdProfile *profile)
{
	CdProfileArrayPrivate *priv = GET_PRIVATE (profile_array);
	g_return_if_fail (CD_IS_PROFILE_ARRAY (profile_array));
	g_return_if_fail (CD_IS_PROFILE (profile));
	g_ptr_array_remove (priv->array, profile);
}

/**
 * cd_profile_array_get_by_id_owner:
 **/
CdProfile *
cd_profile_array_get_by_id_owner (CdProfileArray *profile_array,
				  const gchar *id,
				  guint owner)
{
	CdProfileArrayPrivate *priv = GET_PRIVATE (profile_array);
	CdProfile *profile_tmp;
	guint i;

	/* find profile */
	for (i = 0; i < priv->array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->array, i);
		if (cd_profile_get_owner (profile_tmp) != owner)
			continue;
		if (g_strcmp0 (cd_profile_get_id (profile_tmp), id) == 0)
			return g_object_ref (profile_tmp);
	}
	for (i = 0; i < priv->array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (cd_profile_get_id (profile_tmp), id) == 0)
			return g_object_ref (profile_tmp);
	}
	return NULL;
}

/**
 * cd_profile_array_get_by_filename:
 **/
static CdProfile *
cd_profile_array_get_by_basename (CdProfileArray *profile_array,
				  const gchar *filename)
{
	CdProfileArrayPrivate *priv = GET_PRIVATE (profile_array);
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;
	const gchar *tmp;
	gchar *basename;
	guint i;

	/* find profile */
	for (i = 0; i < priv->array->len && profile == NULL; i++) {
		profile_tmp = g_ptr_array_index (priv->array, i);
		tmp = cd_profile_get_filename (profile_tmp);
		if (tmp == NULL)
			continue;
		basename = g_path_get_basename (tmp);
		if (g_strcmp0 (basename, filename) == 0)
			profile = g_object_ref (profile_tmp);
		g_free (basename);
	}
	return profile;
}

/**
 * cd_profile_array_get_by_filename:
 **/
CdProfile *
cd_profile_array_get_by_filename (CdProfileArray *profile_array,
				  const gchar *filename)
{
	CdProfileArrayPrivate *priv = GET_PRIVATE (profile_array);
	CdProfile *profile_tmp;
	guint i;

	g_return_val_if_fail (filename != NULL, NULL);

	/* support getting the file without the path */
	if (filename[0] != '/')
		return cd_profile_array_get_by_basename (profile_array, filename);

	/* find profile */
	for (i = 0; i < priv->array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (cd_profile_get_filename (profile_tmp), filename) == 0)
			return g_object_ref (profile_tmp);
	}
	return NULL;
}

/**
 * cd_profile_array_get_by_property:
 **/
CdProfile *
cd_profile_array_get_by_property (CdProfileArray *profile_array,
				  const gchar *key,
				  const gchar *value)
{
	CdProfileArrayPrivate *priv = GET_PRIVATE (profile_array);
	CdProfile *profile_tmp;
	guint i;

	/* special case */
	if (g_strcmp0 (key, CD_PROFILE_PROPERTY_FILENAME) == 0)
		return cd_profile_array_get_by_filename (profile_array, value);

	/* find profile */
	for (i = 0; i < priv->array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (cd_profile_get_metadata_item (profile_tmp, key), value) == 0)
			return g_object_ref (profile_tmp);
	}
	return NULL;
}

/**
 * cd_profile_array_get_by_kind:
 **/
GPtrArray *
cd_profile_array_get_by_kind (CdProfileArray *profile_array,
			      CdProfileKind kind)
{
	CdProfileArrayPrivate *priv = GET_PRIVATE (profile_array);
	CdProfile *profile_tmp;
	GPtrArray *array;
	guint i;

	/* find profile */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (i = 0; i < priv->array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->array, i);
		if (cd_profile_get_kind (profile_tmp) == kind)
			g_ptr_array_add (array, g_object_ref (profile_tmp));
	}
	return array;
}

/**
 * cd_profile_array_get_by_metadata:
 **/
GPtrArray *
cd_profile_array_get_by_metadata (CdProfileArray *profile_array,
				  const gchar *key,
				  const gchar *value)
{
	CdProfileArrayPrivate *priv = GET_PRIVATE (profile_array);
	CdProfile *profile_tmp;
	GPtrArray *array;
	GHashTable *hash_tmp;
	guint i;
	const gchar *value_tmp;

	/* find profile */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (i = 0; i < priv->array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->array, i);
		hash_tmp = cd_profile_get_metadata (profile_tmp);
		value_tmp = g_hash_table_lookup (hash_tmp, key);
		if (g_strcmp0 (value_tmp, value) == 0)
			g_ptr_array_add (array, g_object_ref (profile_tmp));
	}
	return array;
}

/**
 * cd_profile_array_get_by_object_path:
 **/
CdProfile *
cd_profile_array_get_by_object_path (CdProfileArray *profile_array,
				     const gchar *object_path)
{
	CdProfileArrayPrivate *priv = GET_PRIVATE (profile_array);
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;
	guint i;

	/* find profile */
	for (i = 0; i < priv->array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (cd_profile_get_object_path (profile_tmp), object_path) == 0)
			return g_object_ref (profile_tmp);
	}
	return profile;
}

/**
 * cd_profile_array_class_init:
 **/
GVariant *
cd_profile_array_get_variant (CdProfileArray *profile_array)
{
	CdProfileArrayPrivate *priv = GET_PRIVATE (profile_array);
	CdProfile *profile;
	GVariant **variant_array = NULL;
	guint i;

	/* copy the object paths */
	variant_array = g_new0 (GVariant *, priv->array->len + 1);
	for (i = 0; i < priv->array->len; i++) {
		profile = g_ptr_array_index (priv->array, i);
		variant_array[i] = g_variant_new_object_path (cd_profile_get_object_path (profile));
	}

	/* format the value */
	return g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
				    variant_array,
				    priv->array->len);
}

/**
 * cd_profile_array_class_init:
 **/
static void
cd_profile_array_class_init (CdProfileArrayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_profile_array_finalize;
}

/**
 * cd_profile_array_init:
 **/
static void
cd_profile_array_init (CdProfileArray *profile_array)
{
	CdProfileArrayPrivate *priv = GET_PRIVATE (profile_array);
	priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

/**
 * cd_profile_array_finalize:
 **/
static void
cd_profile_array_finalize (GObject *object)
{
	CdProfileArray *profile_array = CD_PROFILE_ARRAY (object);
	CdProfileArrayPrivate *priv = GET_PRIVATE (profile_array);

	g_ptr_array_unref (priv->array);

	G_OBJECT_CLASS (cd_profile_array_parent_class)->finalize (object);
}

/**
 * cd_profile_array_new:
 **/
CdProfileArray *
cd_profile_array_new (void)
{
	if (cd_profile_array_object != NULL) {
		g_object_ref (cd_profile_array_object);
	} else {
		cd_profile_array_object = g_object_new (CD_TYPE_PROFILE_ARRAY, NULL);
		g_object_add_weak_pointer (cd_profile_array_object,
					   &cd_profile_array_object);
	}
	return CD_PROFILE_ARRAY (cd_profile_array_object);
}

