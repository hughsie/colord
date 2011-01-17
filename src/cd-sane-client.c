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
#ifdef HAVE_SANE
 #include <sane/sane.h>
#endif

#include "cd-sane-client.h"

static void     cd_sane_client_finalize	(GObject	*object);

#define CD_SANE_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_SANE_CLIENT, CdSaneClientPrivate))

/**
 * CdSaneClientPrivate:
 **/
struct _CdSaneClientPrivate
{
	gboolean			 init_sane;
	GPtrArray			*array;
};

enum {
	SIGNAL_ADDED,
	SIGNAL_REMOVED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (CdSaneClient, cd_sane_client, G_TYPE_OBJECT)

#ifdef HAVE_SANE
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
 * gcm_client_get_id_for_sane_device:
 **/
static gchar *
gcm_client_get_id_for_sane_device (const SANE_Device *sane_device)
{
	gchar *id;
	id = g_strdup_printf ("sane_%s", sane_device->model);
	gcm_utils_alphanum_lcase (id);
	return id;
}

/**
 * cd_sane_client_get_by_id:
 **/
static CdDevice *
cd_sane_client_get_by_id (CdSaneClient *sane_client,
			  const gchar *id)
{
	CdSaneClientPrivate *priv = sane_client->priv;
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
 * cd_sane_client_add:
 **/
static void
cd_sane_client_add (CdSaneClient *sane_client,
		    const SANE_Device *sane_device)
{
	gchar *id = NULL;
	CdDevice *device;

	/* ignore noname, no support devices */
	if (g_strcmp0 (sane_device->vendor, "Noname") == 0) {
		g_debug ("CdSaneClient: Ignoring sane device %s",
			 sane_device->name);
		goto out;
	}

	/* convert device_id 'plustek:libusb:004:002' to suitable id */
	id = gcm_client_get_id_for_sane_device (sane_device);
	device = cd_device_new ();
	cd_device_set_id (device, id);

	/* set known properties */
	cd_device_set_property_internal (device,
					 "Kind",
					 "scanner",
					 FALSE,
					 NULL);
	cd_device_set_property_internal (device,
					 "Model",
					 sane_device->model,
					 FALSE,
					 NULL);
	cd_device_set_property_internal (device,
					 "Vendor",
					 sane_device->vendor,
					 FALSE,
					 NULL);
	g_debug ("CdSaneClient: emit add: %s", id);
	g_signal_emit (sane_client, signals[SIGNAL_ADDED], 0, device);

	/* keep track so we can remove with the same device */
	g_ptr_array_add (sane_client->priv->array, device);
out:
	g_free (id);
}

/**
 * cd_sane_client_remove:
 **/
static void
cd_sane_client_remove (CdSaneClient *sane_client,
		       const SANE_Device *sane_device)
{
	gchar *id;
	CdDevice *device;

	/* find the id in the internal array */
	id = gcm_client_get_id_for_sane_device (sane_device);
	device = cd_sane_client_get_by_id (sane_client, id);
	g_assert (device != NULL);
	g_debug ("CdSaneClient: emit remove: %s", id);
	g_signal_emit (sane_client, signals[SIGNAL_REMOVED], 0, device);

	/* we don't care anymore */
	g_ptr_array_remove (sane_client->priv->array, device);
	g_free (id);
}
#endif

/**
 * cd_sane_client_refresh:
 **/
gboolean
cd_sane_client_refresh (CdSaneClient *sane_client, GError **error)
{
	gboolean ret = TRUE;
#ifdef HAVE_SANE
	gint i;
	SANE_Status status;
	const SANE_Device **device_list;

	/* force sane to drop it's cache of devices -- yes, it is that crap */
	if (sane_client->priv->init_sane) {
		sane_exit ();
		sane_client->priv->init_sane = FALSE;
	}
	status = sane_init (NULL, NULL);
	if (status != SANE_STATUS_GOOD) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to init SANE: %s",
			     sane_strstatus (status));
		goto out;
	}
	sane_client->priv->init_sane = TRUE;

	/* get scanners on the local server */
	status = sane_get_devices (&device_list, FALSE);
	if (status != SANE_STATUS_GOOD) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to get devices from SANE: %s",
			     sane_strstatus (status));
		goto out;
	}

	/* nothing */
	if (device_list == NULL || device_list[0] == NULL) {
		g_debug ("no devices to add");
		goto out;
	}

	/* add them */
	for (i=0; device_list[i] != NULL; i++)
		cd_sane_client_add (sane_client, device_list[i]);
		if (0) cd_sane_client_remove (sane_client, NULL);
out:
#endif
	return ret;
}

/**
 * cd_sane_client_class_init:
 **/
static void
cd_sane_client_class_init (CdSaneClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_sane_client_finalize;
	signals[SIGNAL_ADDED] =
		g_signal_new ("added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdSaneClientClass, added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_DEVICE);
	signals[SIGNAL_REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdSaneClientClass, removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_DEVICE);

	g_type_class_add_private (klass, sizeof (CdSaneClientPrivate));
}

/**
 * cd_sane_client_init:
 **/
static void
cd_sane_client_init (CdSaneClient *sane_client)
{
	sane_client->priv = CD_SANE_CLIENT_GET_PRIVATE (sane_client);
	sane_client->priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	sane_client->priv->init_sane = FALSE;
}

/**
 * cd_sane_client_finalize:
 **/
static void
cd_sane_client_finalize (GObject *object)
{
	CdSaneClient *sane_client = CD_SANE_CLIENT (object);
	CdSaneClientPrivate *priv = sane_client->priv;

#ifdef HAVE_SANE
	if (priv->init_sane)
		sane_exit ();
#endif
	g_ptr_array_unref (priv->array);

	G_OBJECT_CLASS (cd_sane_client_parent_class)->finalize (object);
}

/**
 * cd_sane_client_new:
 **/
CdSaneClient *
cd_sane_client_new (void)
{
	CdSaneClient *sane_client;
	sane_client = g_object_new (CD_TYPE_SANE_CLIENT, NULL);
	return CD_SANE_CLIENT (sane_client);
}

