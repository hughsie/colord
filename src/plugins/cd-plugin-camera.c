/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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

#include <config.h>
#include <gio/gio.h>
#include <cd-plugin.h>
#include <gudev/gudev.h>
#include <cd-device.h>

struct CdPluginPrivate {
	GUdevClient		*udev_client;
	GHashTable		*devices;
};

/**
 * cd_plugin_get_description:
 */
const gchar *
cd_plugin_get_description (void)
{
	return "Add and remove camera devices using info from video4linux";
}

/**
 * cd_plugin_get_camera_id_for_udev_device:
 **/
static gchar *
cd_plugin_get_camera_id_for_udev_device (GUdevDevice *udev_device)
{
	GString *string;
	const gchar *tmp;

	/* get id */
	string = g_string_new ("sysfs");
	tmp = g_udev_device_get_property (udev_device, "ID_VENDOR");
	if (tmp != NULL)
		g_string_append_printf (string, "-%s", tmp);
	tmp = g_udev_device_get_property (udev_device, "ID_MODEL");
	if (tmp != NULL)
		g_string_append_printf (string, "-%s", tmp);

	/* fallback */
	if (string->len == 5) {
		tmp = g_udev_device_get_device_file (udev_device);
		g_string_append_printf (string, "-%s", tmp);
	}

	return g_string_free (string, FALSE);
}

/**
 * cd_plugin_is_device_embedded:
 **/
static gboolean
cd_plugin_is_device_embedded (GUdevDevice *device)
{
	const gchar *removable;
	gboolean embedded = FALSE;
	GUdevDevice *p = device;
	GPtrArray *array;
	guint i;

	/* get a chain of all the parent devices */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	do {
		p = g_udev_device_get_parent (p);
		if (p == NULL)
			break;
		g_ptr_array_add (array, p);
	} while (TRUE);

	/* find a parent with a removable sysfs file */
	for (i = 0; i < array->len; i++) {
		p = g_ptr_array_index (array, i);
		removable = g_udev_device_get_sysfs_attr (p, "removable");
		if (removable != NULL) {
			if (g_strcmp0 (removable, "fixed") == 0)
				embedded = TRUE;
			break;
		}
	}
	g_ptr_array_unref (array);
	return embedded;
}

/**
 * cd_plugin_add:
 **/
static void
cd_plugin_add (CdPlugin *plugin, GUdevDevice *udev_device)
{
	CdDevice *device = NULL;
	const gchar *kind = "webcam";
	const gchar *seat;
	gboolean embedded;
	gboolean ret;
	gchar *id = NULL;
	gchar *model = NULL;
	gchar *vendor = NULL;

	/* is a scanner? */
	ret = g_udev_device_has_property (udev_device, "COLORD_DEVICE");
	if (!ret)
		goto out;

	/* replace underscores with spaces */
	model = g_strdup (g_udev_device_get_property (udev_device,
						      "ID_MODEL"));
	if (model != NULL) {
		g_strdelimit (model, "_\r\n", ' ');
		g_strchomp (model);
	}
	vendor = g_strdup (g_udev_device_get_property (udev_device,
						       "ID_VENDOR"));
	if (vendor != NULL) {
		g_strdelimit (vendor, "_\r\n", ' ');
		g_strchomp (vendor);
	}

	/* is a proper camera and not a webcam */
	ret = g_udev_device_has_property (udev_device, "ID_GPHOTO2");
	if (ret)
		kind = "camera";

	/* generate ID */
	id = cd_plugin_get_camera_id_for_udev_device (udev_device);

	/* assume device belongs to "seat0" if not tagged */
	seat = g_udev_device_get_property (udev_device, "ID_SEAT");
	if (seat == NULL)
		seat = "seat0";

	/* find if the device is embedded */
	embedded = cd_plugin_is_device_embedded (udev_device);

	/* create new device */
	device = cd_device_new ();
	cd_device_set_id (device, id);
	cd_device_set_property_internal (device,
					 CD_DEVICE_PROPERTY_KIND,
					 kind,
					 FALSE,
					 NULL);
	if (model != NULL) {
		cd_device_set_property_internal (device,
						 CD_DEVICE_PROPERTY_MODEL,
						 model,
						 FALSE,
						 NULL);
	}
	if (vendor != NULL) {
		cd_device_set_property_internal (device,
						 CD_DEVICE_PROPERTY_VENDOR,
						 vendor,
						 FALSE,
						 NULL);
	}
	cd_device_set_property_internal (device,
					 CD_DEVICE_PROPERTY_COLORSPACE,
					 "rgb",
					 FALSE,
					 NULL);
	cd_device_set_property_internal (device,
					 CD_DEVICE_PROPERTY_SERIAL,
					 g_udev_device_get_sysfs_path (udev_device),
					 FALSE,
					 NULL);
	cd_device_set_property_internal (device,
					 CD_DEVICE_PROPERTY_SEAT,
					 seat,
					 FALSE,
					 NULL);
	if (embedded) {
		cd_device_set_property_internal (device,
						 CD_DEVICE_PROPERTY_EMBEDDED,
						 NULL,
						 FALSE,
						 NULL);
	}

	/* keep track so we can remove with the same device */
	g_hash_table_insert (plugin->priv->devices,
			     g_strdup (g_udev_device_get_sysfs_path (udev_device)),
			     g_object_ref (device));

	g_debug ("CdPlugin: emit add: %s", id);
	cd_plugin_device_added (plugin, device);
out:
	if (device != NULL)
		g_object_unref (device);
	g_free (id);
	g_free (model);
	g_free (vendor);
}

/**
 * cd_plugin_uevent_cb:
 **/
static void
cd_plugin_uevent_cb (GUdevClient *udev_client,
		     const gchar *action,
		     GUdevDevice *udev_device,
		     CdPlugin *plugin)
{
	const gchar *sysfs_path;
	CdDevice *device;

	/* remove */
	if (g_strcmp0 (action, "remove") == 0) {

		/* is this a camera device we added */
		sysfs_path = g_udev_device_get_sysfs_path (udev_device);
		device = g_hash_table_lookup (plugin->priv->devices, sysfs_path);
		if (device == NULL)
			goto out;

		g_debug ("CdPlugin: remove %s", sysfs_path);
		cd_plugin_device_removed (plugin, device);
		g_hash_table_remove (plugin->priv->devices, sysfs_path);
		goto out;
	}

	/* add */
	if (g_strcmp0 (action, "add") == 0) {
		cd_plugin_add (plugin, udev_device);
		goto out;
	}
out:
	return;
}

/**
 * cd_plugin_coldplug:
 */
void
cd_plugin_coldplug (CdPlugin *plugin)
{
	GList *devices;
	GList *l;
	GUdevDevice *udev_device;

	/* add all USB scanner devices */
	devices = g_udev_client_query_by_subsystem (plugin->priv->udev_client,
						    "usb");
	for (l = devices; l != NULL; l = l->next) {
		udev_device = l->data;
		cd_plugin_add (plugin, udev_device);
	}
	g_list_foreach (devices, (GFunc) g_object_unref, NULL);
	g_list_free (devices);

	/* add all video4linux devices */
	devices = g_udev_client_query_by_subsystem (plugin->priv->udev_client,
						    "video4linux");
	for (l = devices; l != NULL; l = l->next) {
		udev_device = l->data;
		cd_plugin_add (plugin, udev_device);
	}
	g_list_foreach (devices, (GFunc) g_object_unref, NULL);
	g_list_free (devices);

	/* watch udev for changes */
	g_signal_connect (plugin->priv->udev_client, "uevent",
			  G_CALLBACK (cd_plugin_uevent_cb), plugin);
}

/**
 * cd_plugin_initialize:
 */
void
cd_plugin_initialize (CdPlugin *plugin)
{
	const gchar *subsystems[] = { "usb", "video4linux", NULL };

	/* create private */
	plugin->priv = CD_PLUGIN_GET_PRIVATE (CdPluginPrivate);
	plugin->priv->devices = g_hash_table_new_full (g_str_hash,
						       g_str_equal,
						       g_free,
						       (GDestroyNotify) g_object_unref);
	plugin->priv->udev_client = g_udev_client_new (subsystems);
}

/**
 * cd_plugin_destroy:
 */
void
cd_plugin_destroy (CdPlugin *plugin)
{
	g_object_unref (plugin->priv->udev_client);
	g_hash_table_unref (plugin->priv->devices);
}
