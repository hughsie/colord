/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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
 * SECTION:cd-device-sync
 * @short_description: sync helpers for #CdDevice
 *
 * These helper functions provide a simple way to use the async functions
 * in command line tools.
 *
 * See also: #CdDevice
 */

#include "config.h"

#include <glib.h>
#include <gio/gio.h>

#include "cd-device.h"
#include "cd-device-sync.h"

/* tiny helper to help us do the async operation */
typedef struct {
	GError		**error;
	GMainLoop	*loop;
	gboolean	 ret;
	CdDevice	*device;
} CdDeviceHelper;

static void
cd_device_set_object_path_finish_sync (CdDevice *device,
					GAsyncResult *res,
					CdDeviceHelper *helper)
{
	helper->ret = cd_device_set_object_path_finish (device,
							 res,
							 helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_device_set_object_path_sync:
 * @device: a #CdDevice instance.
 * @object_path: The colord object path.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the object path of the object and fills up initial properties.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.0
 **/
gboolean
cd_device_set_object_path_sync (CdDevice *device,
				const gchar *object_path,
				GCancellable *cancellable,
				GError **error)
{
	CdDeviceHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_device_set_object_path (device, object_path, cancellable,
				    (GAsyncReadyCallback) cd_device_set_object_path_finish_sync,
				    &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

/**
 * cd_device_set_model_sync:
 * @device: a #CdDevice instance.
 * @value: The model.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the device model.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.0
 **/
gboolean
cd_device_set_model_sync (CdDevice *device,
			  const gchar *value,
			  GCancellable *cancellable,
			  GError **error)
{
	return cd_device_set_property_sync (device,
					    CD_DEVICE_PROPERTY_MODEL,
					    value,
					    cancellable, error);
}

/**
 * cd_device_set_serial_sync:
 * @device: a #CdDevice instance.
 * @value: The string value.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the device serial number.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.1
 **/
gboolean
cd_device_set_serial_sync (CdDevice *device,
			   const gchar *value,
			   GCancellable *cancellable,
			   GError **error)
{
	return cd_device_set_property_sync (device,
					    CD_DEVICE_PROPERTY_SERIAL,
					    value,
					    cancellable, error);
}

/**
 * cd_device_set_vendor_sync:
 * @device: a #CdDevice instance.
 * @value: The string value.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the device vendor.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.1
 **/
gboolean
cd_device_set_vendor_sync (CdDevice *device,
			   const gchar *value,
			   GCancellable *cancellable,
			   GError **error)
{
	return cd_device_set_property_sync (device,
					    CD_DEVICE_PROPERTY_VENDOR,
					    value,
					    cancellable, error);
}

/**
 * cd_device_set_kind_sync:
 * @device: a #CdDevice instance.
 * @kind: The device kind, e.g. #CD_DEVICE_KIND_DISPLAY
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the device kind.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.0
 **/
gboolean
cd_device_set_kind_sync (CdDevice *device,
			 CdDeviceKind kind,
			 GCancellable *cancellable,
			 GError **error)
{
	return cd_device_set_property_sync (device,
					    CD_DEVICE_PROPERTY_KIND,
					    cd_device_kind_to_string (kind),
					    cancellable, error);
}

/**
 * cd_device_set_colorspace_sync:
 * @device: a #CdDevice instance.
 * @kind: The device kind, e.g. #CD_DEVICE_KIND_DISPLAY
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the device kind.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.1
 **/
gboolean
cd_device_set_colorspace_sync (CdDevice *device,
			       CdColorspace colorspace,
			       GCancellable *cancellable,
			       GError **error)
{
	return cd_device_set_property_sync (device,
					    CD_DEVICE_PROPERTY_COLORSPACE,
					    cd_colorspace_to_string (colorspace),
					    cancellable, error);
}

/**
 * cd_device_set_mode_sync:
 * @device: a #CdDevice instance.
 * @mode: The device kind, e.g. #CD_DEVICE_MODE_VIRTUAL
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the device mode.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.2
 **/
gboolean
cd_device_set_mode_sync (CdDevice *device,
			 CdDeviceMode mode,
			 GCancellable *cancellable,
			 GError **error)
{
	return cd_device_set_property_sync (device,
					    CD_DEVICE_PROPERTY_MODE,
					    cd_device_mode_to_string (mode),
					    cancellable, error);
}
