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
 * SECTION:cd-device
 * @short_description: Client object for accessing information about colord devices
 *
 * A helper GObject to use for accessing colord devices, and to be notified
 * when it is changed.
 *
 * See also: #CdClient
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <string.h>

#include "cd-device.h"
#include "cd-profile.h"

static void	cd_device_class_init	(CdDeviceClass	*klass);
static void	cd_device_init		(CdDevice	*device);
static void	cd_device_finalize	(GObject		*object);

#define CD_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_DEVICE, CdDevicePrivate))

/**
 * CdDevicePrivate:
 *
 * Private #PkDevice data
 **/
struct _CdDevicePrivate
{
	GDBusProxy		*proxy;
	gchar			*object_path;
	gchar			*id;
	gchar			*model;
	gchar			*serial;
	gchar			*vendor;
	guint64			 created;
	guint64			 modified;
	GPtrArray		*profiles;
	CdDeviceKind		 kind;
	CdColorspace		 colorspace;
	CdDeviceMode		 mode;
};

enum {
	PROP_0,
	PROP_CREATED,
	PROP_MODIFIED,
	PROP_ID,
	PROP_MODEL,
	PROP_VENDOR,
	PROP_SERIAL,
	PROP_KIND,
	PROP_COLORSPACE,
	PROP_MODE,
	PROP_LAST
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (CdDevice, cd_device, G_TYPE_OBJECT)

/**
 * cd_device_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.0
 **/
GQuark
cd_device_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("cd_device_error");
	return quark;
}

/**
 * cd_device_get_id:
 * @device: a #CdDevice instance.
 *
 * Gets the device ID.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.0
 **/
const gchar *
cd_device_get_id (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	return device->priv->id;
}

/**
 * cd_device_get_model:
 * @device: a #CdDevice instance.
 *
 * Gets the device model.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.0
 **/
const gchar *
cd_device_get_model (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	return device->priv->model;
}

/**
 * cd_device_get_vendor:
 * @device: a #CdDevice instance.
 *
 * Gets the device vendor.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.1
 **/
const gchar *
cd_device_get_vendor (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	return device->priv->vendor;
}

/**
 * cd_device_get_serial:
 * @device: a #CdDevice instance.
 *
 * Gets the device serial number.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.0
 **/
const gchar *
cd_device_get_serial (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	return device->priv->serial;
}

/**
 * cd_device_get_created:
 * @device: a #CdDevice instance.
 *
 * Gets the device creation date.
 *
 * Return value: A value in seconds, or 0 for invalid
 *
 * Since: 0.1.0
 **/
guint64
cd_device_get_created (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), 0);
	return device->priv->created;
}

/**
 * cd_device_get_modified:
 * @device: a #CdDevice instance.
 *
 * Gets the device modified date.
 *
 * Return value: A value in seconds, or 0 for invalid
 *
 * Since: 0.1.1
 **/
guint64
cd_device_get_modified (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), 0);
	return device->priv->modified;
}

/**
 * cd_device_get_kind:
 * @device: a #CdDevice instance.
 *
 * Gets the device kind.
 *
 * Return value: A device kind, e.g. %CD_DEVICE_KIND_DISPLAY
 *
 * Since: 0.1.0
 **/
CdDeviceKind
cd_device_get_kind (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), CD_DEVICE_KIND_UNKNOWN);
	return device->priv->kind;
}

/**
 * cd_device_get_colorspace:
 * @device: a #CdDevice instance.
 *
 * Gets the device colorspace.
 *
 * Return value: A colorspace, e.g. %CD_COLORSPACE_RGB
 *
 * Since: 0.1.1
 **/
CdColorspace
cd_device_get_colorspace (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), CD_COLORSPACE_UNKNOWN);
	return device->priv->colorspace;
}

/**
 * cd_device_get_mode:
 * @device: a #CdDevice instance.
 *
 * Gets the device mode.
 *
 * Return value: A colorspace, e.g. %CD_DEVICE_MODE_VIRTUAL
 *
 * Since: 0.1.2
 **/
CdDeviceMode
cd_device_get_mode (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), CD_DEVICE_MODE_UNKNOWN);
	return device->priv->mode;
}

/**
 * cd_device_get_profiles:
 * @device: a #CdDevice instance.
 *
 * Gets the device profiles.
 *
 * Return value: An array of #CdProfile's, free with g_ptr_array_unref()
 *
 * Since: 0.1.0
 **/
GPtrArray *
cd_device_get_profiles (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	if (device->priv->profiles == NULL)
		return NULL;
	return g_ptr_array_ref (device->priv->profiles);
}

/**
 * cd_device_get_default_profile:
 * @device: a #CdDevice instance.
 *
 * Gets the default device profile.
 *
 * Return value: A #CdProfile's or NULL, free with g_object_unref()
 *
 * Since: 0.1.1
 **/
CdProfile *
cd_device_get_default_profile (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	if (device->priv->profiles == NULL)
		return NULL;
	if (device->priv->profiles->len == 0)
		return NULL;
	return g_object_ref (g_ptr_array_index (device->priv->profiles, 0));
}

/**
 * cd_device_set_profiles_array_from_variant:
 **/
static gboolean
cd_device_set_profiles_array_from_variant (CdDevice *device,
					   GVariant *profiles,
					   GCancellable *cancellable,
					   GError **error)
{
	CdProfile *profile_tmp;
	gboolean ret = TRUE;
	gchar *object_path_tmp;
	GError *error_local = NULL;
	gsize len;
	guint i;
	GVariantIter iter;

	g_ptr_array_set_size (device->priv->profiles, 0);
	if (profiles == NULL)
		goto out;
	len = g_variant_iter_init (&iter, profiles);
	for (i=0; i<len; i++) {
		g_variant_get_child (profiles, i,
				     "o", &object_path_tmp);
		profile_tmp = cd_profile_new ();
		ret = cd_profile_set_object_path_sync (profile_tmp,
						       object_path_tmp,
						       cancellable,
						       &error_local);
		if (!ret) {
			g_set_error (error,
				     CD_DEVICE_ERROR,
				     CD_DEVICE_ERROR_FAILED,
				     "Failed to set profile object path: %s",
				     error_local->message);
			g_error_free (error_local);
			g_object_unref (profile_tmp);
			goto out;
		}
		g_ptr_array_add (device->priv->profiles, profile_tmp);
		g_free (object_path_tmp);
	}
out:
	return ret;
}

/**
 * cd_device_dbus_properties_changed:
 **/
static void
cd_device_dbus_properties_changed (GDBusProxy  *proxy,
				   GVariant    *changed_properties,
				   const gchar * const *invalidated_properties,
				   CdDevice    *device)
{
	guint i;
	guint len;
	GVariantIter iter;
	gchar *property_name;
	GVariant *property_value;

	len = g_variant_iter_init (&iter, changed_properties);
	for (i=0; i < len; i++) {
		g_variant_get_child (changed_properties, i,
				     "{sv}",
				     &property_name,
				     &property_value);
		if (g_strcmp0 (property_name, "Model") == 0) {
			g_free (device->priv->model);
			device->priv->model = g_variant_dup_string (property_value, NULL);
		} else if (g_strcmp0 (property_name, "Serial") == 0) {
			g_free (device->priv->serial);
			device->priv->serial = g_variant_dup_string (property_value, NULL);
		} else if (g_strcmp0 (property_name, "Vendor") == 0) {
			g_free (device->priv->vendor);
			device->priv->vendor = g_variant_dup_string (property_value, NULL);
		} else if (g_strcmp0 (property_name, "Kind") == 0) {
			device->priv->kind =
				cd_device_kind_from_string (g_variant_get_string (property_value, NULL));
		} else if (g_strcmp0 (property_name, "Colorspace") == 0) {
			device->priv->colorspace =
				cd_colorspace_from_string (g_variant_get_string (property_value, NULL));
		} else if (g_strcmp0 (property_name, "Mode") == 0) {
			device->priv->mode =
				cd_device_mode_from_string (g_variant_get_string (property_value, NULL));
		} else if (g_strcmp0 (property_name, "Profiles") == 0) {
			cd_device_set_profiles_array_from_variant (device,
								   property_value,
								   NULL,
								   NULL);
		} else if (g_strcmp0 (property_name, "Created") == 0) {
			device->priv->created = g_variant_get_uint64 (property_value);
		} else if (g_strcmp0 (property_name, "Modified") == 0) {
			device->priv->modified = g_variant_get_uint64 (property_value);
		} else if (g_strcmp0 (property_name, "DeviceId") == 0) {
			/* ignore this, we don't support it changing */;
		} else {
			g_warning ("%s property unhandled", property_name);
		}
		g_variant_unref (property_value);
	}
}

/**
 * cd_device_dbus_signal_cb:
 **/
static void
cd_device_dbus_signal_cb (GDBusProxy *proxy,
			  gchar      *sender_name,
			  gchar      *signal_name,
			  GVariant   *parameters,
			  CdDevice   *device)
{
	gchar *object_path_tmp = NULL;

	if (g_strcmp0 (signal_name, "Changed") == 0) {
		g_debug ("emit Changed on %s", device->priv->object_path);
		g_signal_emit (device, signals[SIGNAL_CHANGED], 0);
	} else {
		g_warning ("unhandled signal '%s'", signal_name);
	}
	g_free (object_path_tmp);
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
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.0
 **/
gboolean
cd_device_set_object_path_sync (CdDevice *device,
				const gchar *object_path,
				GCancellable *cancellable,
				GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *created = NULL;
	GVariant *modified = NULL;
	GVariant *id = NULL;
	GVariant *kind = NULL;
	GVariant *model = NULL;
	GVariant *serial = NULL;
	GVariant *vendor = NULL;
	GVariant *colorspace = NULL;
	GVariant *mode = NULL;
	GVariant *profiles = NULL;

	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->proxy == NULL, FALSE);

	/* connect to the daemon */
	device->priv->proxy =
		g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					       G_DBUS_PROXY_FLAGS_NONE,
					       NULL,
					       COLORD_DBUS_SERVICE,
					       object_path,
					       COLORD_DBUS_INTERFACE_DEVICE,
					       cancellable,
					       &error_local);
	if (device->priv->proxy == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_DEVICE_ERROR,
			     CD_DEVICE_ERROR_FAILED,
			     "Failed to connect to device %s: %s",
			     object_path,
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* save object path */
	device->priv->object_path = g_strdup (object_path);

	/* get device id */
	id = g_dbus_proxy_get_cached_property (device->priv->proxy,
					       "DeviceId");
	if (id != NULL)
		device->priv->id = g_variant_dup_string (id, NULL);

	/* get kind */
	kind = g_dbus_proxy_get_cached_property (device->priv->proxy,
						 "Kind");
	if (kind != NULL)
		device->priv->kind =
			cd_device_kind_from_string (g_variant_get_string (kind, NULL));

	/* get colorspace */
	colorspace = g_dbus_proxy_get_cached_property (device->priv->proxy,
						       "Colorspace");
	if (colorspace != NULL)
		device->priv->colorspace =
			cd_colorspace_from_string (g_variant_get_string (colorspace, NULL));

	/* get mode */
	mode = g_dbus_proxy_get_cached_property (device->priv->proxy,
						 "Mode");
	if (mode != NULL)
		device->priv->mode =
			cd_device_mode_from_string (g_variant_get_string (mode, NULL));

	/* get model */
	model = g_dbus_proxy_get_cached_property (device->priv->proxy,
						  "Model");
	if (model != NULL)
		device->priv->model = g_variant_dup_string (model, NULL);

	/* get serial */
	serial = g_dbus_proxy_get_cached_property (device->priv->proxy,
						   "Serial");
	if (serial != NULL)
		device->priv->serial = g_variant_dup_string (serial, NULL);

	/* get vendor */
	vendor = g_dbus_proxy_get_cached_property (device->priv->proxy,
						   "Vendor");
	if (vendor != NULL)
		device->priv->vendor = g_variant_dup_string (vendor, NULL);

	/* get created */
	created = g_dbus_proxy_get_cached_property (device->priv->proxy,
						    "Created");
	if (created != NULL)
		device->priv->created = g_variant_get_uint64 (created);

	/* get modified */
	modified = g_dbus_proxy_get_cached_property (device->priv->proxy,
						     "Modified");
	if (modified != NULL)
		device->priv->modified = g_variant_get_uint64 (modified);

	/* get profiles */
	profiles = g_dbus_proxy_get_cached_property (device->priv->proxy,
						     "Profiles");
	ret = cd_device_set_profiles_array_from_variant (device,
							 profiles,
							 cancellable,
							 error);
	if (!ret)
		goto out;

	/* get signals from DBus */
	g_signal_connect (device->priv->proxy,
			  "g-signal",
			  G_CALLBACK (cd_device_dbus_signal_cb),
			  device);

	/* watch if any remote properties change */
	g_signal_connect (device->priv->proxy,
			  "g-properties-changed",
			  G_CALLBACK (cd_device_dbus_properties_changed),
			  device);

	/* success */
	g_debug ("Connected to device %s",
		 device->priv->id);
out:
	if (id != NULL)
		g_variant_unref (id);
	if (model != NULL)
		g_variant_unref (model);
	if (vendor != NULL)
		g_variant_unref (vendor);
	if (serial != NULL)
		g_variant_unref (serial);
	if (colorspace != NULL)
		g_variant_unref (colorspace);
	if (mode != NULL)
		g_variant_unref (mode);
	if (created != NULL)
		g_variant_unref (created);
	if (modified != NULL)
		g_variant_unref (modified);
	if (kind != NULL)
		g_variant_unref (kind);
	if (profiles != NULL)
		g_variant_unref (profiles);
	return ret;
}

/**
 * cd_device_set_property_sync:
 **/
static gboolean
cd_device_set_property_sync (CdDevice *device,
			     const gchar *name,
			     const gchar *value,
			     GCancellable *cancellable,
			     GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *response = NULL;

	/* execute sync method */
	response = g_dbus_proxy_call_sync (device->priv->proxy,
					   "SetProperty",
					   g_variant_new ("(ss)",
						       name,
						       value),
					   G_DBUS_CALL_FLAGS_NONE,
					   -1, NULL, &error_local);
	if (response == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_DEVICE_ERROR,
			     CD_DEVICE_ERROR_FAILED,
			     "Failed to set profile object path: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	if (response != NULL)
		g_variant_unref (response);
	return ret;
}

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
	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->proxy != NULL, FALSE);

	/* execute sync helper */
	return cd_device_set_property_sync (device, "Model", value,
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
	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->proxy != NULL, FALSE);

	/* execute sync helper */
	return cd_device_set_property_sync (device, "Serial", value,
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
	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->proxy != NULL, FALSE);

	/* execute sync helper */
	return cd_device_set_property_sync (device, "Vendor", value,
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
	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->proxy != NULL, FALSE);

	/* execute sync helper */
	return cd_device_set_property_sync (device, "Kind",
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
	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->proxy != NULL, FALSE);

	/* execute sync helper */
	return cd_device_set_property_sync (device, "Colorspace",
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
	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->proxy != NULL, FALSE);

	/* execute sync helper */
	return cd_device_set_property_sync (device, "Mode",
					    cd_device_mode_to_string (mode),
					    cancellable, error);
}

/**
 * cd_device_add_profile_sync:
 * @device: a #CdDevice instance.
 * @relation: a #CdDeviceRelation, e.g. #CD_DEVICE_RELATION_HARD
 * @profile: a #CdProfile instance
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Adds a profile to a device.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.3
 **/
gboolean
cd_device_add_profile_sync (CdDevice *device,
			    CdDeviceRelation relation,
			    CdProfile *profile,
			    GCancellable *cancellable,
			    GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *response = NULL;

	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (relation != CD_DEVICE_RELATION_UNKNOWN, FALSE);
	g_return_val_if_fail (device->priv->proxy != NULL, FALSE);

	/* execute sync method */
	response = g_dbus_proxy_call_sync (device->priv->proxy,
					   "AddProfile",
					   g_variant_new ("(so)",
							  cd_device_relation_to_string (relation),
							  cd_profile_get_object_path (profile)),
					   G_DBUS_CALL_FLAGS_NONE,
					   -1, NULL, &error_local);
	if (response == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_DEVICE_ERROR,
			     CD_DEVICE_ERROR_FAILED,
			     "Failed to add profile to device: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	if (response != NULL)
		g_variant_unref (response);
	return ret;
}

/**
 * cd_device_remove_profile_sync:
 * @device: a #CdDevice instance.
 * @profile: a #CdProfile instance
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Removes a profile from a device.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.2
 **/
gboolean
cd_device_remove_profile_sync (CdDevice *device,
			       CdProfile *profile,
			       GCancellable *cancellable,
			       GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *response = NULL;

	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (device->priv->proxy != NULL, FALSE);

	/* execute sync method */
	response = g_dbus_proxy_call_sync (device->priv->proxy,
					   "RemoveProfile",
					   g_variant_new ("(o)",
							  cd_profile_get_object_path (profile)),
					   G_DBUS_CALL_FLAGS_NONE,
					   -1, NULL, &error_local);
	if (response == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_DEVICE_ERROR,
			     CD_DEVICE_ERROR_FAILED,
			     "Failed to remove profile from device: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	if (response != NULL)
		g_variant_unref (response);
	return ret;
}

/**
 * cd_device_make_profile_default_sync:
 * @device: a #CdDevice instance.
 * @profile: a #CdProfile instance
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Makes an already added profile default for a device.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.0
 **/
gboolean
cd_device_make_profile_default_sync (CdDevice *device,
				     CdProfile *profile,
				     GCancellable *cancellable,
				     GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *response = NULL;

	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (device->priv->proxy != NULL, FALSE);

	/* execute sync method */
	response = g_dbus_proxy_call_sync (device->priv->proxy,
					   "MakeProfileDefault",
					   g_variant_new ("(s)",
							  cd_profile_get_id (profile)),
					   G_DBUS_CALL_FLAGS_NONE,
					   -1, NULL, &error_local);
	if (response == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_DEVICE_ERROR,
			     CD_DEVICE_ERROR_FAILED,
			     "Failed to make profile default on device: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	if (response != NULL)
		g_variant_unref (response);
	return ret;
}

/**
 * cd_device_profiling_inhibit_sync:
 * @device: a #CdDevice instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets up the device for profiling and causes no profiles to be
 * returned if cd_device_get_profile_for_qualifiers_sync() is used.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.1
 **/
gboolean
cd_device_profiling_inhibit_sync (CdDevice *device,
				  GCancellable *cancellable,
				  GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *response = NULL;

	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->proxy != NULL, FALSE);

	/* execute sync method */
	response = g_dbus_proxy_call_sync (device->priv->proxy,
					   "ProfilingInhibit",
					   NULL,
					   G_DBUS_CALL_FLAGS_NONE,
					   -1, NULL, &error_local);
	if (response == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_DEVICE_ERROR,
			     CD_DEVICE_ERROR_FAILED,
			     "Failed to inhibit device: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	if (response != NULL)
		g_variant_unref (response);
	return ret;
}

/**
 * cd_device_profiling_uninhibit_sync:
 * @device: a #CdDevice instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Restores the device after profiling and causes normal profiles to be
 * returned if cd_device_get_profile_for_qualifiers_sync() is used.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.1
 **/
gboolean
cd_device_profiling_uninhibit_sync (CdDevice *device,
				    GCancellable *cancellable,
				    GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *response = NULL;

	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->proxy != NULL, FALSE);

	/* execute sync method */
	response = g_dbus_proxy_call_sync (device->priv->proxy,
					   "ProfilingUninhibit",
					   NULL,
					   G_DBUS_CALL_FLAGS_NONE,
					   -1, NULL, &error_local);
	if (response == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_DEVICE_ERROR,
			     CD_DEVICE_ERROR_FAILED,
			     "Failed to uninhibit device: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	if (response != NULL)
		g_variant_unref (response);
	return ret;
}

/**
 * cd_device_get_profile_for_qualifiers_sync:
 * @device: a #CdDevice instance.
 * @qualifier: a qualifier that can included wildcards
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Gets the prefered profile for some qualifiers.
 *
 * Return value: a #CdProfile, free with g_object_unref()
 *
 * Since: 0.1.3
 **/
CdProfile *
cd_device_get_profile_for_qualifiers_sync (CdDevice *device,
					   const gchar **qualifiers,
					   GCancellable *cancellable,
					   GError **error)
{
	CdProfile *profile = NULL;
	CdProfile *profile_tmp = NULL;
	gboolean ret;
	gchar *object_path = NULL;
	GError *error_local = NULL;
	guint i;
	GVariantBuilder builder;
	GVariant *response = NULL;

	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	g_return_val_if_fail (qualifiers != NULL, NULL);
	g_return_val_if_fail (device->priv->proxy != NULL, NULL);

	/* squash char** into an array of strings */
	g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
	for (i=0; qualifiers[i] != NULL; i++)
		g_variant_builder_add (&builder, "s", qualifiers[i]);

	/* execute sync method */
	response = g_dbus_proxy_call_sync (device->priv->proxy,
					   "GetProfileForQualifiers",
					   g_variant_new ("(as)",
							  &builder),
					   G_DBUS_CALL_FLAGS_NONE,
					   -1, NULL, &error_local);
	if (response == NULL) {
		g_set_error (error,
			     CD_DEVICE_ERROR,
			     CD_DEVICE_ERROR_FAILED,
			     "Failed to get a suitable profile: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* create thick CdDevice object */
	g_variant_get (response, "(o)",
		       &object_path);
	profile_tmp = cd_profile_new ();
	ret = cd_profile_set_object_path_sync (profile_tmp,
					       object_path,
					       cancellable,
					       error);
	if (!ret)
		goto out;

	/* success */
	profile = g_object_ref (profile_tmp);
out:
	g_free (object_path);
	if (profile_tmp != NULL)
		g_object_unref (profile_tmp);
	if (response != NULL)
		g_variant_unref (response);
	return profile;
}

/**
 * cd_device_get_object_path:
 * @device: a #CdDevice instance.
 *
 * Gets the object path for the device.
 *
 * Return value: the object path, or %NULL
 *
 * Since: 0.1.0
 **/
const gchar *
cd_device_get_object_path (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	return device->priv->object_path;
}

/**
 * cd_device_to_string:
 * @device: a #CdDevice instance.
 *
 * Converts the device to a string description.
 *
 * Return value: text representation of #CdDevice
 *
 * Since: 0.1.0
 **/
gchar *
cd_device_to_string (CdDevice *device)
{
	struct tm *time_tm;
	time_t t;
	gchar time_buf[256];
	GString *string;

	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);

	/* get a human readable time */
	t = (time_t) device->priv->created;
	time_tm = localtime (&t);
	strftime (time_buf, sizeof time_buf, "%c", time_tm);

	string = g_string_new ("");
	g_string_append_printf (string, "  object-path:          %s\n",
				device->priv->object_path);
	g_string_append_printf (string, "  created:              %s\n",
				time_buf);

	return g_string_free (string, FALSE);
}

/*
 * cd_device_set_property:
 */
static void
cd_device_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CdDevice *device = CD_DEVICE (object);

	switch (prop_id) {
	case PROP_ID:
		g_free (device->priv->id);
		device->priv->id = g_strdup (g_value_get_string (value));
		break;
	case PROP_MODEL:
		g_free (device->priv->model);
		device->priv->model = g_strdup (g_value_get_string (value));
		break;
	case PROP_CREATED:
		device->priv->created = g_value_get_uint64 (value);
		break;
	case PROP_MODIFIED:
		device->priv->modified = g_value_get_uint64 (value);
		break;
	case PROP_KIND:
		device->priv->kind = g_value_get_uint (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/*
 * cd_device_get_property:
 */
static void
cd_device_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	CdDevice *device = CD_DEVICE (object);

	switch (prop_id) {
	case PROP_CREATED:
		g_value_set_uint64 (value, device->priv->created);
		break;
	case PROP_MODIFIED:
		g_value_set_uint64 (value, device->priv->modified);
		break;
	case PROP_ID:
		g_value_set_string (value, device->priv->id);
		break;
	case PROP_MODEL:
		g_value_set_string (value, device->priv->model);
		break;
	case PROP_SERIAL:
		g_value_set_string (value, device->priv->serial);
		break;
	case PROP_VENDOR:
		g_value_set_string (value, device->priv->vendor);
		break;
	case PROP_KIND:
		g_value_set_uint (value, device->priv->kind);
		break;
	case PROP_COLORSPACE:
		g_value_set_uint (value, device->priv->colorspace);
		break;
	case PROP_MODE:
		g_value_set_uint (value, device->priv->mode);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
    }
}

/*
 * cd_device_class_init:
 */
static void
cd_device_class_init (CdDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_device_finalize;
	object_class->set_property = cd_device_set_property;
	object_class->get_property = cd_device_get_property;

	/**
	 * CdDevice::changed:
	 * @device: the #CdDevice instance that emitted the signal
	 *
	 * The ::changed signal is emitted when the device data has changed.
	 *
	 * Since: 0.1.0
	 **/
	signals [SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdDeviceClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/**
	 * CdDevice:created:
	 *
	 * The time the device was created.
	 *
	 * Since: 0.1.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_CREATED,
					 g_param_spec_uint64 ("created",
							      NULL, NULL,
							      0, G_MAXUINT64, 0,
							      G_PARAM_READWRITE));
	/**
	 * CdDevice:modified:
	 *
	 * The last time the device was modified.
	 *
	 * Since: 0.1.1
	 **/
	g_object_class_install_property (object_class,
					 PROP_MODIFIED,
					 g_param_spec_uint64 ("modified",
							      NULL, NULL,
							      0, G_MAXUINT64, 0,
							      G_PARAM_READWRITE));
	/**
	 * CdDevice:id:
	 *
	 * The device ID.
	 *
	 * Since: 0.1.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_string ("id",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * CdDevice:model:
	 *
	 * The device model.
	 *
	 * Since: 0.1.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_MODEL,
					 g_param_spec_string ("model",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * CdDevice:serial:
	 *
	 * The device serial number.
	 *
	 * Since: 0.1.1
	 **/
	g_object_class_install_property (object_class,
					 PROP_SERIAL,
					 g_param_spec_string ("serial",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * CdDevice:vendor:
	 *
	 * The device vendor.
	 *
	 * Since: 0.1.1
	 **/
	g_object_class_install_property (object_class,
					 PROP_VENDOR,
					 g_param_spec_string ("vendor",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * CdDevice:kind:
	 *
	 * The device kind, e.g. %CD_DEVICE_KIND_KEYBOARD.
	 *
	 * Since: 0.1.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_KIND,
					 g_param_spec_uint ("kind",
							    NULL, NULL,
							    CD_DEVICE_KIND_UNKNOWN,
							    CD_DEVICE_KIND_LAST,
							    CD_DEVICE_KIND_UNKNOWN,
							    G_PARAM_READWRITE));
	/**
	 * CdDevice:colorspace:
	 *
	 * The device colorspace, e.g. %CD_COLORSPACE_RGB.
	 *
	 * Since: 0.1.1
	 **/
	g_object_class_install_property (object_class,
					 PROP_COLORSPACE,
					 g_param_spec_uint ("colorspace",
							    NULL, NULL,
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READWRITE));

	/**
	 * CdDevice:mode:
	 *
	 * The device colorspace, e.g. %CD_DEVICE_MODE_VIRTUAL.
	 *
	 * Since: 0.1.2
	 **/
	g_object_class_install_property (object_class,
					 PROP_MODE,
					 g_param_spec_uint ("mode",
							    NULL, NULL,
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (CdDevicePrivate));
}

/*
 * cd_device_init:
 */
static void
cd_device_init (CdDevice *device)
{
	device->priv = CD_DEVICE_GET_PRIVATE (device);
	device->priv->profiles = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

/*
 * cd_device_finalize:
 */
static void
cd_device_finalize (GObject *object)
{
	CdDevice *device;

	g_return_if_fail (CD_IS_DEVICE (object));

	device = CD_DEVICE (object);

	g_free (device->priv->object_path);
	g_free (device->priv->id);
	g_free (device->priv->model);
	g_ptr_array_unref (device->priv->profiles);
	if (device->priv->proxy != NULL)
		g_object_unref (device->priv->proxy);

	G_OBJECT_CLASS (cd_device_parent_class)->finalize (object);
}

/**
 * cd_device_new:
 *
 * Creates a new #CdDevice object.
 *
 * Return value: a new CdDevice object.
 *
 * Since: 0.1.0
 **/
CdDevice *
cd_device_new (void)
{
	CdDevice *device;
	device = g_object_new (CD_TYPE_DEVICE, NULL);
	return CD_DEVICE (device);
}

