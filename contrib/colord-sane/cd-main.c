/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2013 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2013 Christopher James Halse Rogers <raof@ubuntu.com>
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
#include <glib/gi18n.h>
#include <locale.h>
#include <sane/sane.h>
#include <gudev/gudev.h>
#include <dbus/dbus.h>
#include <colord/colord.h>

typedef struct {
	gchar		*argv0;
	CdClient	*client;
	GMainLoop	*loop;
	GPtrArray	*array; /* of CdMainDev's */
} CdMainPrivate;

typedef struct {
	CdDevice	*device;
	gchar		*id; /* note: we can get this from CdDevice, but we don't wan't to connect() */
	gboolean	 valid;
} CdMainDev;

/**
 * cd_main_dev_free:
 **/
static void
cd_main_dev_free (CdMainDev *tmp)
{
	g_object_unref (tmp->device);
	g_free (tmp->id);
	g_free (tmp);
}

/**
 * cd_main_dev_find_by_id:
 **/
static CdMainDev *
cd_main_dev_find_by_id (CdMainPrivate *priv,
			const gchar *id)
{
	CdMainDev *tmp;
	guint i;

	/* nothing to find */
	if (priv->array->len == 0)
		goto out;
	for (i = 0; i < priv->array->len; i++) {
		tmp = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (tmp->id, id) == 0)
			return tmp;
	}
out:
	return NULL;
}

/**
 * cd_client_get_id_for_sane_device:
 **/
static gchar *
cd_client_get_id_for_sane_device (const SANE_Device *sane_device)
{
	gchar *id;
	id = g_strdup_printf ("sane-%s", sane_device->model);
	return id;
}

typedef struct {
	CdMainPrivate	*priv;
	gchar		*id;
} CdMainCreateDeviceHelper;

/**
 * cd_main_colord_create_device_cb:
 **/
static void
cd_main_colord_create_device_cb (GObject *source_object,
				 GAsyncResult *res,
				 gpointer user_data)
{
	CdClient *client = CD_CLIENT (source_object);
	CdDevice *device;
	GError *error = NULL;

	/* get result */
	device = cd_client_create_device_finish (client, res, &error);
	if (device == NULL) {
		g_warning ("failed to create device: %s",
			   error->message);
		g_error_free (error);
	}

	if (device != NULL)
		g_object_unref (device);
}

/**
 * cd_sane_client_add:
 **/
static void
cd_sane_client_add (CdMainPrivate *priv, const SANE_Device *sane_device)
{
	gchar *id = NULL;
	gchar *model = NULL;
	gchar *vendor = NULL;
	CdMainDev *dev;
	GHashTable *properties = NULL;

	/* ignore noname, no support devices */
	if (g_strcmp0 (sane_device->vendor, "Noname") == 0) {
		g_debug ("CdSaneClient: Ignoring sane device %s",
			 sane_device->name);
		goto out;
	}

	/* convert device_id 'plustek:libusb:004:002' to suitable id */
	id = cd_client_get_id_for_sane_device (sane_device);

	/* see if this device already exists */
	dev = cd_main_dev_find_by_id (priv, id);
	if (dev != NULL) {
		dev->valid = TRUE;
		goto out;
	}

	/* Make human readable */
	model = g_strdup (sane_device->model);
	g_strdelimit (model, "_", ' ');
	vendor = g_strdup (sane_device->vendor);
	g_strdelimit (vendor, "_", ' ');

	/* create initial device properties */
	properties = g_hash_table_new_full (g_str_hash, g_str_equal,
					      NULL, NULL);
	g_hash_table_insert (properties,
			     (gpointer) CD_DEVICE_PROPERTY_KIND,
			     (gpointer) cd_device_kind_to_string (CD_DEVICE_KIND_SCANNER));
	g_hash_table_insert (properties,
			     (gpointer) CD_DEVICE_PROPERTY_MODE,
			     (gpointer) cd_device_mode_to_string (CD_DEVICE_MODE_PHYSICAL));
	g_hash_table_insert (properties,
			     (gpointer) CD_DEVICE_PROPERTY_COLORSPACE,
			     (gpointer) cd_colorspace_to_string (CD_COLORSPACE_RGB));
	g_hash_table_insert (properties,
			     (gpointer) CD_DEVICE_PROPERTY_VENDOR,
			     (gpointer) vendor);
	g_hash_table_insert (properties,
			     (gpointer) CD_DEVICE_PROPERTY_MODEL,
			     (gpointer) model);
	g_hash_table_insert (properties,
			     (gpointer) CD_DEVICE_PROPERTY_SERIAL,
			     (gpointer) sane_device->name);
#if 0
	g_hash_table_insert (properties,
			     (gpointer) CD_DEVICE_METADATA_OWNER_CMDLINE,
			     (gpointer) priv->argv0);
#endif
	cd_client_create_device (priv->client,
				 id,
				 CD_OBJECT_SCOPE_NORMAL,
				 properties,
				 NULL,
				 cd_main_colord_create_device_cb,
				 NULL);
out:
	if (properties != NULL)
		g_hash_table_unref (properties);
	g_free (id);
	g_free (model);
	g_free (vendor);
}

/**
 * cd_main_colord_delete_device_cb:
 **/
static void
cd_main_colord_delete_device_cb (GObject *source_object,
				 GAsyncResult *res,
				 gpointer user_data)
{
	CdClient *client = CD_CLIENT (source_object);
	gboolean ret;
	GError *error = NULL;

	/* get result */
	ret = cd_client_delete_device_finish (client, res, &error);
	if (!ret) {
		g_warning ("failed to delete device: %s",
			   error->message);
		g_error_free (error);
	}
}

/**
 * cd_sane_client_remove:
 **/
static void
cd_sane_client_remove (CdMainPrivate *priv, CdDevice *device)
{
	g_debug ("Deleting device: %s", cd_device_get_object_path (device));
	cd_client_delete_device (priv->client,
				 device,
				 NULL,
				 cd_main_colord_delete_device_cb,
				 priv);
}

/**
 * cd_sane_client_refresh:
 **/
static void
cd_sane_client_refresh (CdMainPrivate *priv)
{
	CdMainDev *tmp;
	const SANE_Device **device_list = NULL;
	gint idx;
	guint i;
	SANE_Status status;

	status = sane_init (NULL, NULL);
	if (status != SANE_STATUS_GOOD) {
		g_warning ("failed to init SANE: %s",
			   sane_strstatus (status));
		goto out;
	}

	/* get scanners on the local server */
	status = sane_get_devices (&device_list, TRUE);
	if (status != SANE_STATUS_GOOD) {
		g_warning ("failed to get devices from SANE: %s",
			   sane_strstatus (status));
		goto out;
	}

	/* nothing */
	if (device_list == NULL || device_list[0] == NULL)
		goto out;

	/* add them */
	for (idx = 0; device_list[idx] != NULL; idx++)
		cd_sane_client_add (priv, device_list[idx]);

	/* remove any that are invalid */
	for (i = 0; i < priv->array->len; i++) {
		tmp = g_ptr_array_index (priv->array, i);
		if (tmp->valid)
			continue;
		cd_sane_client_remove (priv, tmp->device);
	}
out:
	g_main_loop_quit (priv->loop);
}

/**
 * cd_sane_add_device_if_from_colord_sane
 **/
static void
cd_sane_add_device_if_from_colord_sane (gpointer data,
				        gpointer user_data)
{
	CdDevice *device = (CdDevice *) data;
	CdMainDev *sane_device;
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	const gchar *cmdline;
	gboolean ret;
	GError *error = NULL;

	ret = cd_device_connect_sync (device, NULL, &error);

	if (!ret) {
		g_warning ("failed to receive list of devices: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	cmdline = cd_device_get_metadata_item (device,
					       CD_DEVICE_METADATA_OWNER_CMDLINE);
	if (g_strcmp0 (cmdline, priv->argv0) == 0) {
		sane_device = g_new (CdMainDev, 1);
		sane_device->device = g_object_ref (device);
		sane_device->id = (gchar *)cd_device_get_id (device);
		sane_device->valid = FALSE;
		g_ptr_array_add (priv->array, sane_device);
	}
}

/**
 * cd_sane_client_populate_existing_devices
 **/
static void
cd_sane_populate_existing_devices_cb (GObject *source_object,
				      GAsyncResult *res,
				      gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	GPtrArray *devices;
	GError *error = NULL;

	devices = cd_client_get_devices_by_kind_finish (priv->client, res, &error);
	if (error != NULL) {
		g_warning ("failed to receive list of devices: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	g_ptr_array_foreach (devices,
			     cd_sane_add_device_if_from_colord_sane,
			     priv);

	cd_sane_client_refresh (priv);
}

/**
 * cd_main_colord_connect_cb:
 **/
static void
cd_main_colord_connect_cb (GObject *source_object,
			   GAsyncResult *res,
			   gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	gboolean ret;
	GError *error = NULL;

	/* get result */
	ret = cd_client_connect_finish (priv->client, res, &error);
	if (!ret) {
		g_warning ("failed to connect to colord: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	cd_client_get_devices_by_kind (priv->client,
				       CD_DEVICE_KIND_SCANNER,
				       NULL,
				       cd_sane_populate_existing_devices_cb,
				       priv);
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	CdMainPrivate *priv = NULL;
	guint retval = 1;

	/* We need to init DBus' threading support as libSANE uses raw DBus */
	dbus_threads_init_default ();
#if !GLIB_CHECK_VERSION(2,36,0)
	g_type_init ();
#endif

	/* create new objects */
	priv = g_new0 (CdMainPrivate, 1);
	priv->loop = g_main_loop_new (NULL, FALSE);
	priv->client = cd_client_new ();
	priv->argv0 = g_strdup (argv[0]);
	priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_main_dev_free);

	/* connect to daemon */
	cd_client_connect (priv->client,
			   NULL,
			   cd_main_colord_connect_cb,
			   priv);

	/* process */
	g_main_loop_run (priv->loop);

	/* success */
	retval = 0;

	if (priv != NULL) {
		g_free (priv->argv0);
		if (priv->array != NULL)
			g_ptr_array_unref (priv->array);
		if (priv->client != NULL)
			g_object_unref (priv->client);
		g_main_loop_unref (priv->loop);
	}
	return retval;
}
