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

#include "cd-sensor-client.h"
#include "cd-sensor.h"

static void     cd_sensor_client_finalize	(GObject	*object);

#define CD_SENSOR_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_SENSOR_CLIENT, CdSensorClientPrivate))

/**
 * CdSensorClientPrivate:
 **/
struct _CdSensorClientPrivate
{
	GUdevClient			*gudev_client;
	GPtrArray			*array_sensors;
};

enum {
	SIGNAL_SENSOR_ADDED,
	SIGNAL_SENSOR_REMOVED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (CdSensorClient, cd_sensor_client, G_TYPE_OBJECT)

/**
 * cd_sensor_client_add:
 **/
static gboolean
cd_sensor_client_add (CdSensorClient *sensor_client,
		      GUdevDevice *device)
{
	gboolean ignore;
	gboolean ret;
	CdSensor *sensor = NULL;
	const gchar *device_file;
	GError *error = NULL;

	/* interesting device? */
	ret = g_udev_device_get_property_as_boolean (device, "COLORD_SENSOR");
	if (!ret)
		goto out;
	ignore = g_udev_device_get_property_as_boolean (device, "COLORD_IGNORE");
	if (ignore) {
		ret = FALSE;
		goto out;
	}

	/* actual device? */
	device_file = g_udev_device_get_device_file (device);
	if (device_file == NULL)
		goto out;

	/* get data */
	g_debug ("adding color management device: %s [%s]",
		 g_udev_device_get_sysfs_path (device),
		 device_file);
	sensor = cd_sensor_new ();
	ret = cd_sensor_set_from_device (sensor, device, &error);
	if (!ret) {
		g_warning ("CdSensorClient: failed to set CM sensor: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* load the sensor */
	ret = cd_sensor_load (sensor, &error);
	if (!ret) {
		/* not fatal, non-native devices are still useable */
		g_debug ("CdSensorClient: failed to load native sensor: %s",
			 error->message);
		g_clear_error (&error);
	}

	/* signal the addition */
	g_debug ("emit: added");
	g_signal_emit (sensor_client, signals[SIGNAL_SENSOR_ADDED], 0, sensor);

	/* keep track so we can remove with the same device */
	g_ptr_array_add (sensor_client->priv->array_sensors, g_object_ref (sensor));
out:
	if (sensor != NULL)
		g_object_unref (sensor);
	return ret;
}

/**
 * cd_sensor_client_remove:
 **/
static gboolean
cd_sensor_client_remove (CdSensorClient *sensor_client,
			 GUdevDevice *device)
{
	CdSensor *sensor;
	const gchar *device_file;
	const gchar *id;
	gboolean ret;
	guint i;

	/* interesting device? */
	ret = g_udev_device_get_property_as_boolean (device, "COLORD_SENSOR");
	if (!ret)
		goto out;

	/* actual device? */
	device_file = g_udev_device_get_device_file (device);
	if (device_file == NULL)
		goto out;

	/* get data */
	g_debug ("removing color management device: %s",
		 g_udev_device_get_sysfs_path (device));
	id = g_udev_device_get_property (device, "COLORD_SENSOR_KIND");
	for (i=0; i<sensor_client->priv->array_sensors->len; i++) {
		sensor = g_ptr_array_index (sensor_client->priv->array_sensors, i);
		if (g_strcmp0 (cd_sensor_get_id (sensor), id) == 0) {
			g_debug ("emit: removed");
			g_signal_emit (sensor_client, signals[SIGNAL_SENSOR_REMOVED], 0, sensor);
			g_ptr_array_remove_index_fast (sensor_client->priv->array_sensors, i);
			goto out;
		}
	}

	/* nothing found */
	g_warning ("removed CM sensor that was never added");
out:
	return ret;
}

/**
 * cd_sensor_client_uevent_cb:
 **/
static void
cd_sensor_client_uevent_cb (GUdevClient *gudev_client,
			    const gchar *action,
			    GUdevDevice *udev_device,
			    CdSensorClient *sensor_client)
{
	gboolean ret;

	/* remove */
	if (g_strcmp0 (action, "remove") == 0) {
		g_debug ("CdSensorClient: remove %s",
			 g_udev_device_get_sysfs_path (udev_device));
		ret = g_udev_device_has_property (udev_device, "COLORD_SENSOR");
		if (ret) {
			cd_sensor_client_remove (sensor_client,
						      udev_device);
			goto out;
		}
		goto out;
	}

	/* add */
	if (g_strcmp0 (action, "add") == 0) {
		g_debug ("CdSensorClient: add %s",
			 g_udev_device_get_sysfs_path (udev_device));
		ret = g_udev_device_has_property (udev_device, "COLORD_SENSOR");
		if (ret) {
			cd_sensor_client_add (sensor_client,
						   udev_device);
			goto out;
		}
		goto out;
	}
out:
	return;
}

/**
 * cd_sensor_client_coldplug:
 **/
void
cd_sensor_client_coldplug (CdSensorClient *sensor_client)
{
	GList *devices;
	GList *l;
	GUdevDevice *udev_device;

	/* get all video4linux devices */
	devices = g_udev_client_query_by_subsystem (sensor_client->priv->gudev_client,
						    "usb");
	for (l = devices; l != NULL; l = l->next) {
		udev_device = l->data;
		cd_sensor_client_add (sensor_client, udev_device);
	}
	g_list_foreach (devices, (GFunc) g_object_unref, NULL);
	g_list_free (devices);
}

/**
 * cd_sensor_client_class_init:
 **/
static void
cd_sensor_client_class_init (CdSensorClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_sensor_client_finalize;
	signals[SIGNAL_SENSOR_ADDED] =
		g_signal_new ("sensor-added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdSensorClientClass, sensor_added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_SENSOR);
	signals[SIGNAL_SENSOR_REMOVED] =
		g_signal_new ("sensor-removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdSensorClientClass, sensor_removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_SENSOR);

	g_type_class_add_private (klass, sizeof (CdSensorClientPrivate));
}

/**
 * cd_sensor_client_init:
 **/
static void
cd_sensor_client_init (CdSensorClient *sensor_client)
{
	const gchar *subsystems[] = {"usb", "video4linux", NULL};
	sensor_client->priv = CD_SENSOR_CLIENT_GET_PRIVATE (sensor_client);
	sensor_client->priv->array_sensors = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	sensor_client->priv->gudev_client = g_udev_client_new (subsystems);
	g_signal_connect (sensor_client->priv->gudev_client, "uevent",
			  G_CALLBACK (cd_sensor_client_uevent_cb), sensor_client);
}

/**
 * cd_sensor_client_finalize:
 **/
static void
cd_sensor_client_finalize (GObject *object)
{
	CdSensorClient *sensor_client = CD_SENSOR_CLIENT (object);
	CdSensorClientPrivate *priv = sensor_client->priv;

	g_object_unref (priv->gudev_client);
	g_ptr_array_unref (sensor_client->priv->array_sensors);

	G_OBJECT_CLASS (cd_sensor_client_parent_class)->finalize (object);
}

/**
 * cd_sensor_client_new:
 **/
CdSensorClient *
cd_sensor_client_new (void)
{
	CdSensorClient *sensor_client;
	sensor_client = g_object_new (CD_TYPE_SENSOR_CLIENT, NULL);
	return CD_SENSOR_CLIENT (sensor_client);
}

