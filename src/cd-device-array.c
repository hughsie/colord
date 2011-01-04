/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <glib-object.h>

#include "cd-common.h"
#include "cd-device-array.h"

static void     cd_device_array_finalize	(GObject     *object);

#define CD_DEVICE_ARRAY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_DEVICE_ARRAY, CdDeviceArrayPrivate))

/**
 * CdDeviceArrayPrivate:
 *
 * Private #CdDeviceArray data
 **/
struct _CdDeviceArrayPrivate
{
	GPtrArray			*array;
};

G_DEFINE_TYPE (CdDeviceArray, cd_device_array, G_TYPE_OBJECT)

static gpointer cd_device_array_object = NULL;

/**
 * cd_device_array_add:
 **/
void
cd_device_array_add (CdDeviceArray *device_array, CdDevice *device)
{
	g_return_if_fail (CD_IS_DEVICE_ARRAY (device_array));
	g_return_if_fail (CD_IS_DEVICE (device));

	g_ptr_array_add (device_array->priv->array,
			 g_object_ref (device));
}

/**
 * cd_device_array_remove:
 **/
void
cd_device_array_remove (CdDeviceArray *device_array, CdDevice *device)
{
	g_return_if_fail (CD_IS_DEVICE_ARRAY (device_array));
	g_return_if_fail (CD_IS_DEVICE (device));

	g_ptr_array_remove (device_array->priv->array,
			    device);
}

/**
 * cd_device_array_get_by_id:
 **/
CdDevice *
cd_device_array_get_by_id (CdDeviceArray *device_array,
			    const gchar *id)
{
	CdDeviceArrayPrivate *priv = device_array->priv;
	CdDevice *device = NULL;
	CdDevice *device_tmp;
	guint i;

	/* find device */
	for (i=0; i<priv->array->len; i++) {
		device_tmp = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (cd_device_get_id (device_tmp), id) == 0) {
			device = g_object_ref (device_tmp);
			break;
		}
	}
	return device;
}

/**
 * cd_device_array_get_by_object_path:
 **/
CdDevice *
cd_device_array_get_by_object_path (CdDeviceArray *device_array,
				     const gchar *object_path)
{
	CdDeviceArrayPrivate *priv = device_array->priv;
	CdDevice *device = NULL;
	CdDevice *device_tmp;
	guint i;

	/* find device */
	for (i=0; i<priv->array->len; i++) {
		device_tmp = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (cd_device_get_object_path (device_tmp),
			       object_path) == 0) {
			device = g_object_ref (device_tmp);
			break;
		}
	}
	return device;
}

/**
 * cd_device_array_class_init:
 **/
GVariant *
cd_device_array_get_variant (CdDeviceArray *device_array)
{
	CdDeviceArrayPrivate *priv = device_array->priv;
	CdDevice *device;
	guint i;
	GVariant *variant;
	GVariant **variant_array = NULL;

	/* copy the object paths */
	variant_array = g_new0 (GVariant *, priv->array->len + 1);
	for (i=0; i<priv->array->len; i++) {
		device = g_ptr_array_index (priv->array, i);
		variant_array[i] = g_variant_new_object_path (cd_device_get_object_path (device));
	}

	/* format the value */
	variant = g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
				       variant_array,
				       priv->array->len);
	return variant;
}

/**
 * cd_device_array_class_init:
 **/
static void
cd_device_array_class_init (CdDeviceArrayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_device_array_finalize;

	g_type_class_add_private (klass, sizeof (CdDeviceArrayPrivate));
}

/**
 * cd_device_array_init:
 **/
static void
cd_device_array_init (CdDeviceArray *device_array)
{
	device_array->priv = CD_DEVICE_ARRAY_GET_PRIVATE (device_array);
	device_array->priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

/**
 * cd_device_array_finalize:
 **/
static void
cd_device_array_finalize (GObject *object)
{
	CdDeviceArray *device_array = CD_DEVICE_ARRAY (object);
	CdDeviceArrayPrivate *priv = device_array->priv;

	g_ptr_array_unref (priv->array);

	G_OBJECT_CLASS (cd_device_array_parent_class)->finalize (object);
}

/**
 * cd_device_array_new:
 **/
CdDeviceArray *
cd_device_array_new (void)
{
	if (cd_device_array_object != NULL) {
		g_object_ref (cd_device_array_object);
	} else {
		cd_device_array_object = g_object_new (CD_TYPE_DEVICE_ARRAY, NULL);
		g_object_add_weak_pointer (cd_device_array_object,
					   &cd_device_array_object);
	}
	return CD_DEVICE_ARRAY (cd_device_array_object);
}

