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

#define CD_PROFILE_ARRAY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_PROFILE_ARRAY, CdProfileArrayPrivate))

/**
 * CdProfileArrayPrivate:
 *
 * Private #CdProfileArray data
 **/
struct _CdProfileArrayPrivate
{
	GPtrArray			*array;
};

G_DEFINE_TYPE (CdProfileArray, cd_profile_array, G_TYPE_OBJECT)

static gpointer cd_profile_array_object = NULL;

/**
 * cd_profile_array_add:
 **/
void
cd_profile_array_add (CdProfileArray *profile_array, CdProfile *profile)
{
	g_return_if_fail (CD_IS_PROFILE_ARRAY (profile_array));
	g_return_if_fail (CD_IS_PROFILE (profile));

	g_ptr_array_add (profile_array->priv->array,
			 g_object_ref (profile));
}

/**
 * cd_profile_array_remove:
 **/
void
cd_profile_array_remove (CdProfileArray *profile_array, CdProfile *profile)
{
	g_return_if_fail (CD_IS_PROFILE_ARRAY (profile_array));
	g_return_if_fail (CD_IS_PROFILE (profile));

	g_ptr_array_remove (profile_array->priv->array,
			    profile);
}

/**
 * cd_profile_array_get_by_id_owner:
 **/
CdProfile *
cd_profile_array_get_by_id_owner (CdProfileArray *profile_array,
				  const gchar *id,
				  guint owner)
{
	CdProfileArrayPrivate *priv = profile_array->priv;
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;
	guint i;

	/* find profile */
	for (i=0; i<priv->array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->array, i);
		if (cd_profile_get_owner (profile_tmp) != owner)
			continue;
		if (g_strcmp0 (cd_profile_get_id (profile_tmp), id) == 0) {
			profile = g_object_ref (profile_tmp);
			goto out;
		}
	}
	for (i=0; i<priv->array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (cd_profile_get_id (profile_tmp), id) == 0) {
			profile = g_object_ref (profile_tmp);
			goto out;
		}
	}
out:
	return profile;
}

/**
 * cd_profile_array_get_by_filename:
 **/
CdProfile *
cd_profile_array_get_by_filename (CdProfileArray *profile_array,
				  const gchar *filename)
{
	CdProfileArrayPrivate *priv = profile_array->priv;
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;
	guint i;

	/* find profile */
	for (i=0; i<priv->array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (cd_profile_get_filename (profile_tmp),
			       filename) == 0) {
			profile = g_object_ref (profile_tmp);
			break;
		}
	}
	return profile;
}

/**
 * cd_profile_array_get_by_kind:
 **/
GPtrArray *
cd_profile_array_get_by_kind (CdProfileArray *profile_array,
			      CdProfileKind kind)
{
	CdProfileArrayPrivate *priv = profile_array->priv;
	CdProfile *profile_tmp;
	GPtrArray *array;
	guint i;

	/* find profile */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (i=0; i<priv->array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->array, i);
		if (cd_profile_get_kind (profile_tmp) == kind) {
			g_ptr_array_add (array,
					 g_object_ref (profile_tmp));
		}
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
	CdProfileArrayPrivate *priv = profile_array->priv;
	CdProfile *profile_tmp;
	GPtrArray *array;
	GHashTable *hash_tmp;
	guint i;
	const gchar *value_tmp;

	/* find profile */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (i=0; i<priv->array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->array, i);
		hash_tmp = cd_profile_get_metadata (profile_tmp);
		value_tmp = g_hash_table_lookup (hash_tmp, "key");
		if (g_strcmp0 (value_tmp, value) == 0) {
			g_ptr_array_add (array,
					 g_object_ref (profile_tmp));
		}
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
	CdProfileArrayPrivate *priv = profile_array->priv;
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;
	guint i;

	/* find profile */
	for (i=0; i<priv->array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (cd_profile_get_object_path (profile_tmp),
			       object_path) == 0) {
			profile = g_object_ref (profile_tmp);
			break;
		}
	}
	return profile;
}

/**
 * cd_profile_array_class_init:
 **/
GVariant *
cd_profile_array_get_variant (CdProfileArray *profile_array)
{
	CdProfileArrayPrivate *priv = profile_array->priv;
	CdProfile *profile;
	guint i;
	GVariant *variant;
	GVariant **variant_array = NULL;

	/* copy the object paths */
	variant_array = g_new0 (GVariant *, priv->array->len + 1);
	for (i=0; i<priv->array->len; i++) {
		profile = g_ptr_array_index (priv->array, i);
		variant_array[i] = g_variant_new_object_path (cd_profile_get_object_path (profile));
	}

	/* format the value */
	variant = g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
				       variant_array,
				       priv->array->len);

	return variant;
}

/**
 * cd_profile_array_class_init:
 **/
static void
cd_profile_array_class_init (CdProfileArrayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_profile_array_finalize;

	g_type_class_add_private (klass, sizeof (CdProfileArrayPrivate));
}

/**
 * cd_profile_array_init:
 **/
static void
cd_profile_array_init (CdProfileArray *profile_array)
{
	profile_array->priv = CD_PROFILE_ARRAY_GET_PRIVATE (profile_array);
	profile_array->priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

/**
 * cd_profile_array_finalize:
 **/
static void
cd_profile_array_finalize (GObject *object)
{
	CdProfileArray *profile_array = CD_PROFILE_ARRAY (object);
	CdProfileArrayPrivate *priv = profile_array->priv;

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

