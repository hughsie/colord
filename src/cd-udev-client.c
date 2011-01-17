/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2011 Richard Hughes <richard@hughsie.com>
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
#include <gudev/gudev.h>

#include "cd-udev-client.h"

static void     cd_udev_client_finalize	(GObject	*object);

#define CD_UDEV_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_UDEV_CLIENT, CdUdevClientPrivate))

/**
 * CdUdevClientPrivate:
 **/
struct _CdUdevClientPrivate
{
	GUdevClient			*gudev_client;
	GPtrArray			*array;
};

enum {
	SIGNAL_ADDED,
	SIGNAL_REMOVED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (CdUdevClient, cd_udev_client, G_TYPE_OBJECT)

/**
 * gcm_utils_alphanum_lcase:
 **/
static void
gcm_utils_alphanum_lcase (gchar *data)
{
	guint i;

	g_return_if_fail (data != NULL);

	/* replace unsafe chars, and make lowercase */
	for (i=0; data[i] != '\0'; i++) {
		if (!g_ascii_isalnum (data[i]))
			data[i] = '_';
		data[i] = g_ascii_tolower (data[i]);
	}
}

/**
 * gcm_client_get_id_for_udev_device:
 **/
static gchar *
gcm_client_get_id_for_udev_device (GUdevDevice *udev_device)
{
	gchar *id;

	/* get id */
	id = g_strdup_printf ("sysfs_%s_%s",
			      g_udev_device_get_property (udev_device, "ID_VENDOR"),
			      g_udev_device_get_property (udev_device, "ID_MODEL"));

	/* replace unsafe chars */
	gcm_utils_alphanum_lcase (id);
	return id;
}

/**
 * cd_udev_client_get_by_id:
 **/
static CdDevice *
cd_udev_client_get_by_id (CdUdevClient *udev_client,
			  const gchar *id)
{
	CdUdevClientPrivate *priv = udev_client->priv;
	CdDevice *device = NULL;
	CdDevice *device_tmp;
	guint i;

	/* find device */
	for (i=0; i<priv->array->len; i++) {
		device_tmp = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (cd_device_get_id (device_tmp), id) == 0) {
			device = device_tmp;
			break;
		}
	}
	return device;
}

/**
 * cd_udev_client_add:
 **/
static void
cd_udev_client_add (CdUdevClient *udev_client,
		    GUdevDevice *udev_device)
{
	gchar *id;
	CdDevice *device;

	/* create new device */
	id = gcm_client_get_id_for_udev_device (udev_device);
	device = cd_device_new ();
	cd_device_set_id (device, id);
	cd_device_set_property_internal (device,
					 "Kind",
					 "camera",
					 FALSE,
					 NULL);
	cd_device_set_property_internal (device,
					 "Model",
					 g_udev_device_get_property (udev_device,
								     "ID_MODEL"),
					 FALSE,
					 NULL);
	cd_device_set_property_internal (device,
					 "Vendor",
					 g_udev_device_get_property (udev_device,
								     "ID_VENDOR"),
					 FALSE,
					 NULL);
	g_debug ("CdUdevClient: emit add: %s", id);
	g_signal_emit (udev_client, signals[SIGNAL_ADDED], 0, device);

	/* keep track so we can remove with the same device */
	g_ptr_array_add (udev_client->priv->array, device);
	g_free (id);
}

/**
 * cd_udev_client_remove:
 **/
static void
cd_udev_client_remove (CdUdevClient *udev_client,
		       GUdevDevice *udev_device)
{
	gchar *id;
	CdDevice *device;

	/* find the id in the internal array */
	id = gcm_client_get_id_for_udev_device (udev_device);
	device = cd_udev_client_get_by_id (udev_client, id);
	g_assert (device != NULL);
	g_debug ("CdUdevClient: emit remove: %s", id);
	g_signal_emit (udev_client, signals[SIGNAL_REMOVED], 0, device);

	/* we don't care anymore */
	g_ptr_array_remove (udev_client->priv->array, device);
	g_free (id);
}

/**
 * cd_udev_client_uevent_cb:
 **/
static void
cd_udev_client_uevent_cb (GUdevClient *gudev_client,
			  const gchar *action,
			  GUdevDevice *udev_device,
			  CdUdevClient *udev_client)
{
	const gchar *value;

	/* matches our udev rules, which we can change without recompiling */
	value = g_udev_device_get_property (udev_device, "COLORD_DEVICE");
	if (value == NULL)
		return;

	/* emit signal if it's interesting */
	if (g_strcmp0 (action, "remove") == 0) {
		cd_udev_client_remove (udev_client,
				       udev_device);
	} else if (g_strcmp0 (action, "add") == 0) {
		cd_udev_client_add (udev_client,
				    udev_device);
	}
}

/**
 * cd_udev_client_coldplug:
 **/
void
cd_udev_client_coldplug (CdUdevClient *udev_client)
{
	GList *devices;
	GList *l;
	GUdevDevice *udev_device;

	/* get all video4linux devices */
	devices = g_udev_client_query_by_subsystem (udev_client->priv->gudev_client,
						    "video4linux");
	for (l = devices; l != NULL; l = l->next) {
		udev_device = l->data;
		cd_udev_client_add (udev_client, udev_device);
	}
	g_list_foreach (devices, (GFunc) g_object_unref, NULL);
	g_list_free (devices);
}

/**
 * cd_udev_client_class_init:
 **/
static void
cd_udev_client_class_init (CdUdevClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_udev_client_finalize;
	signals[SIGNAL_ADDED] =
		g_signal_new ("added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdUdevClientClass, added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_DEVICE);
	signals[SIGNAL_REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdUdevClientClass, removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_DEVICE);

	g_type_class_add_private (klass, sizeof (CdUdevClientPrivate));
}

/**
 * cd_udev_client_init:
 **/
static void
cd_udev_client_init (CdUdevClient *udev_client)
{
	const gchar *subsystems[] = {"video4linux", NULL};
	udev_client->priv = CD_UDEV_CLIENT_GET_PRIVATE (udev_client);
	udev_client->priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	udev_client->priv->gudev_client = g_udev_client_new (subsystems);
	g_signal_connect (udev_client->priv->gudev_client, "uevent",
			  G_CALLBACK (cd_udev_client_uevent_cb), udev_client);
}

/**
 * cd_udev_client_finalize:
 **/
static void
cd_udev_client_finalize (GObject *object)
{
	CdUdevClient *udev_client = CD_UDEV_CLIENT (object);
	CdUdevClientPrivate *priv = udev_client->priv;

	g_object_unref (priv->gudev_client);
	g_ptr_array_unref (udev_client->priv->array);

	G_OBJECT_CLASS (cd_udev_client_parent_class)->finalize (object);
}

/**
 * cd_udev_client_new:
 **/
CdUdevClient *
cd_udev_client_new (void)
{
	CdUdevClient *udev_client;
	udev_client = g_object_new (CD_TYPE_UDEV_CLIENT, NULL);
	return CD_UDEV_CLIENT (udev_client);
}

