/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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
#include "cd-profile-sync.h"

static void	cd_device_class_init	(CdDeviceClass	*klass);
static void	cd_device_init		(CdDevice	*device);
static void	cd_device_finalize	(GObject		*object);

#define GET_PRIVATE(o) (cd_device_get_instance_private (o))

#define COLORD_DBUS_SERVICE		"org.freedesktop.ColorManager"
#define COLORD_DBUS_INTERFACE_DEVICE	"org.freedesktop.ColorManager.Device"

/**
 * CdDevicePrivate:
 *
 * Private #CdDevice data
 **/
typedef struct
{
	GDBusProxy		*proxy;
	gchar			*object_path;
	gchar			*id;
	gchar			*model;
	gchar			*serial;
	gchar			*seat;
	gchar			*format;
	gchar			*vendor;
	gchar			**profiling_inhibitors;
	guint64			 created;
	guint64			 modified;
	GPtrArray		*profiles;
	CdDeviceKind		 kind;
	CdColorspace		 colorspace;
	CdDeviceMode		 mode;
	CdObjectScope		 scope;
	gboolean		 enabled;
	gboolean		 embedded;
	guint			 owner;
	GHashTable		*metadata;
} CdDevicePrivate;

enum {
	PROP_0,
	PROP_OBJECT_PATH,
	PROP_CONNECTED,
	PROP_CREATED,
	PROP_MODIFIED,
	PROP_ID,
	PROP_MODEL,
	PROP_VENDOR,
	PROP_SERIAL,
	PROP_SEAT,
	PROP_FORMAT,
	PROP_KIND,
	PROP_COLORSPACE,
	PROP_MODE,
	PROP_SCOPE,
	PROP_OWNER,
	PROP_PROFILING_INHIBITORS,
	PROP_ENABLED,
	PROP_EMBEDDED,
	PROP_LAST
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (CdDevice, cd_device, G_TYPE_OBJECT)

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
 * cd_device_set_object_path:
 * @device: a #CdDevice instance.
 * @object_path: The colord object path.
 *
 * Sets the object path of the device.
 *
 * Since: 0.1.8
 **/
void
cd_device_set_object_path (CdDevice *device, const gchar *object_path)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_if_fail (CD_IS_DEVICE (device));
	g_return_if_fail (priv->object_path == NULL);
	priv->object_path = g_strdup (object_path);
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
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->id;
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
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->model;
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
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->vendor;
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
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->serial;
}

/**
 * cd_device_get_seat:
 * @device: a #CdDevice instance.
 *
 * Gets the device seat identifier.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.24
 **/
const gchar *
cd_device_get_seat (CdDevice *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->seat;
}

/**
 * cd_device_get_format:
 * @device: a #CdDevice instance.
 *
 * Gets the device format.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.9
 **/
const gchar *
cd_device_get_format (CdDevice *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->format;
}

/**
 * cd_device_get_profiling_inhibitors:
 * @device: a #CdDevice instance.
 *
 * Gets any profiling inhibitors for the device.
 *
 * Return value: (transfer none): A strv, or %NULL for invalid
 *
 * Since: 0.1.17
 **/
const gchar **
cd_device_get_profiling_inhibitors (CdDevice *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return (const gchar **) priv->profiling_inhibitors;
}

/**
 * cd_device_get_created:
 * @device: a #CdDevice instance.
 *
 * Gets the device creation date.
 *
 * Return value: A value in microseconds, or 0 for invalid
 *
 * Since: 0.1.0
 **/
guint64
cd_device_get_created (CdDevice *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), 0);
	g_return_val_if_fail (priv->proxy != NULL, 0);
	return priv->created;
}

/**
 * cd_device_get_modified:
 * @device: a #CdDevice instance.
 *
 * Gets the device modified date.
 *
 * Return value: A value in microseconds, or 0 for invalid
 *
 * Since: 0.1.1
 **/
guint64
cd_device_get_modified (CdDevice *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), 0);
	g_return_val_if_fail (priv->proxy != NULL, 0);
	return priv->modified;
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
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), CD_DEVICE_KIND_UNKNOWN);
	g_return_val_if_fail (priv->proxy != NULL, CD_DEVICE_KIND_UNKNOWN);
	return priv->kind;
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
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), CD_COLORSPACE_UNKNOWN);
	g_return_val_if_fail (priv->proxy != NULL, CD_COLORSPACE_UNKNOWN);
	return priv->colorspace;
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
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), CD_DEVICE_MODE_UNKNOWN);
	g_return_val_if_fail (priv->proxy != NULL, CD_DEVICE_MODE_UNKNOWN);
	return priv->mode;
}

/**
 * cd_device_get_enabled:
 * @device: a #CdDevice instance.
 *
 * Gets the device enabled state.
 *
 * Return value: %TRUE if the device is enabled
 *
 * Since: 0.1.26
 **/
gboolean
cd_device_get_enabled (CdDevice *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (priv->proxy != NULL, FALSE);
	return priv->enabled;
}

/**
 * cd_device_get_embedded:
 * @device: a #CdDevice instance.
 *
 * Returns if the device is embedded in the computer and cannot be
 * removed.
 *
 * Return value: %TRUE if embedded.
 *
 * Since: 0.1.27
 **/
gboolean
cd_device_get_embedded (CdDevice *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (priv->proxy != NULL, FALSE);
	return priv->embedded;
}

/**
 * cd_device_get_scope:
 * @device: a #CdDevice instance.
 *
 * Gets the device scope.
 *
 * Return value: An object scope, e.g. %CD_OBJECT_SCOPE_TEMP
 *
 * Since: 0.1.10
 **/
CdObjectScope
cd_device_get_scope (CdDevice *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), CD_OBJECT_SCOPE_UNKNOWN);
	g_return_val_if_fail (priv->proxy != NULL, CD_OBJECT_SCOPE_UNKNOWN);
	return priv->scope;
}

/**
 * cd_device_get_owner:
 * @device: a #CdDevice instance.
 *
 * Gets the device owner.
 *
 * Return value: The UID of the user that created the device
 *
 * Since: 0.1.13
 **/
guint
cd_device_get_owner (CdDevice *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), G_MAXUINT);
	g_return_val_if_fail (priv->proxy != NULL, G_MAXUINT);
	return priv->owner;
}

/**
 * cd_device_get_profiles:
 * @device: a #CdDevice instance.
 *
 * Gets the device profiles.
 *
 * Return value: (element-type CdProfile) (transfer container): An array of #CdProfile's
 *
 * Since: 0.1.0
 **/
GPtrArray *
cd_device_get_profiles (CdDevice *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	if (priv->profiles == NULL)
		return NULL;
	return g_ptr_array_ref (priv->profiles);
}

/**
 * cd_device_get_default_profile:
 * @device: a #CdDevice instance.
 *
 * Gets the default device profile. A profile will not be returned
 * if the device is being profiled or is disabled.
 *
 * Return value: (transfer full): A #CdProfile's or NULL
 *
 * Since: 0.1.1
 **/
CdProfile *
cd_device_get_default_profile (CdDevice *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	if (priv->profiles == NULL)
		return NULL;
	if (priv->profiles->len == 0)
		return NULL;
	if (!priv->enabled)
		return NULL;
	if (g_strv_length (priv->profiling_inhibitors) > 0)
		return NULL;
	return g_object_ref (g_ptr_array_index (priv->profiles, 0));
}

/**
 * cd_device_set_profiles_array_from_variant:
 **/
static void
cd_device_set_profiles_array_from_variant (CdDevice *device,
					   GVariant *profiles)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	CdProfile *profile_tmp;
	gsize len;
	guint i;

	g_ptr_array_set_size (priv->profiles, 0);
	if (profiles == NULL)
		return;
	len = g_variant_n_children (profiles);
	for (i = 0; i < len; i++) {
		g_autofree gchar *object_path_tmp = NULL;
		g_variant_get_child (profiles, i,
				     "o", &object_path_tmp);
		profile_tmp = cd_profile_new_with_object_path (object_path_tmp);
		g_ptr_array_add (priv->profiles, profile_tmp);
	}
}

/**
 * cd_device_get_metadata:
 * @device: a #CdDevice instance.
 *
 * Returns the device metadata.
 *
 * Return value: (transfer container) (element-type utf8 utf8): a
 *               #GHashTable.
 *
 * Since: 0.1.5
 **/
GHashTable *
cd_device_get_metadata (CdDevice *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return g_hash_table_ref (priv->metadata);
}

/**
 * cd_device_get_metadata_item:
 * @device: a #CdDevice instance.
 * @key: a key for the metadata dictionary
 *
 * Returns the device metadata for a specific key.
 *
 * Return value: the metadata value, or %NULL if not set.
 *
 * Since: 0.1.5
 **/
const gchar *
cd_device_get_metadata_item (CdDevice *device, const gchar *key)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return g_hash_table_lookup (priv->metadata, key);
}

/**
 * cd_device_set_metadata_from_variant:
 **/
static void
cd_device_set_metadata_from_variant (CdDevice *device, GVariant *variant)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	GVariantIter iter;
	const gchar *prop_key;
	const gchar *prop_value;

	/* remove old entries */
	g_hash_table_remove_all (priv->metadata);

	/* insert the new metadata */
	g_variant_iter_init (&iter, variant);
	while (g_variant_iter_loop (&iter, "{ss}",
				    &prop_key, &prop_value)) {
		g_hash_table_insert (priv->metadata,
				     g_strdup (prop_key),
				     g_strdup (prop_value));

	}
}

/**
 * cd_device_get_nullable_str:
 *
 * We can't get nullable types from a GVariant yet. Work around...
 **/
static gchar *
cd_device_get_nullable_str (GVariant *value)
{
	const gchar *tmp;
	tmp = g_variant_get_string (value, NULL);
	if (tmp == NULL)
		return NULL;
	if (tmp[0] == '\0')
		return NULL;
	return g_strdup (tmp);
}

/**
 * cd_device_dbus_properties_changed_cb:
 **/
static void
cd_device_dbus_properties_changed_cb (GDBusProxy  *proxy,
				      GVariant    *changed_properties,
				      const gchar * const *invalidated_properties,
				      CdDevice    *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	guint i;
	guint len;
	GVariantIter iter;
	gchar *property_name;
	GVariant *property_value;

	g_return_if_fail (CD_IS_DEVICE (device));

	len = g_variant_iter_init (&iter, changed_properties);
	for (i = 0; i < len; i++) {
		g_variant_get_child (changed_properties, i,
				     "{sv}",
				     &property_name,
				     &property_value);
		if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_MODEL) == 0) {
			g_free (priv->model);
			priv->model = cd_device_get_nullable_str (property_value);
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_SERIAL) == 0) {
			g_free (priv->serial);
			priv->serial = cd_device_get_nullable_str (property_value);
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_SEAT) == 0) {
			g_free (priv->seat);
			priv->seat = cd_device_get_nullable_str (property_value);
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_FORMAT) == 0) {
			g_free (priv->format);
			priv->format = cd_device_get_nullable_str (property_value);
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_VENDOR) == 0) {
			g_free (priv->vendor);
			priv->vendor = cd_device_get_nullable_str (property_value);
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_PROFILING_INHIBITORS) == 0) {
			g_free (priv->profiling_inhibitors);
			priv->profiling_inhibitors = g_variant_dup_strv (property_value, NULL);
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_KIND) == 0) {
			priv->kind =
				cd_device_kind_from_string (g_variant_get_string (property_value, NULL));
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_COLORSPACE) == 0) {
			priv->colorspace =
				cd_colorspace_from_string (g_variant_get_string (property_value, NULL));
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_MODE) == 0) {
			priv->mode =
				cd_device_mode_from_string (g_variant_get_string (property_value, NULL));
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_PROFILES) == 0) {
			cd_device_set_profiles_array_from_variant (device,
								   property_value);
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_CREATED) == 0) {
			priv->created = g_variant_get_uint64 (property_value);
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_ENABLED) == 0) {
			priv->enabled = g_variant_get_boolean (property_value);
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_EMBEDDED) == 0) {
			priv->embedded = g_variant_get_boolean (property_value);
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_MODIFIED) == 0) {
			priv->modified = g_variant_get_uint64 (property_value);
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_METADATA) == 0) {
			cd_device_set_metadata_from_variant (device, property_value);
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_OWNER) == 0) {
			priv->owner = g_variant_get_uint32 (property_value);
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_SCOPE) == 0) {
			priv->scope = cd_object_scope_from_string (g_variant_get_string (property_value, NULL));
		} else if (g_strcmp0 (property_name, CD_DEVICE_PROPERTY_ID) == 0) {
			/* ignore this, we don't support it changing */;
		} else {
			g_warning ("%s property unhandled", property_name);
		}
		g_free (property_name);
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

	g_return_if_fail (CD_IS_DEVICE (device));

	if (g_strcmp0 (signal_name, "Changed") == 0) {
		g_signal_emit (device, signals[SIGNAL_CHANGED], 0);
	} else {
		g_warning ("unhandled signal '%s'", signal_name);
	}
	g_free (object_path_tmp);
}

/**********************************************************************/

/**
 * cd_device_connect_finish:
 * @device: a #CdDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: success
 *
 * Since: 0.1.8
 **/
gboolean
cd_device_connect_finish (CdDevice *device,
			  GAsyncResult *res,
			  GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, device), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_device_connect_cb (GObject *source_object,
		      GAsyncResult *res,
		      gpointer user_data)
{
	CdDevice *device;
	CdDevicePrivate *priv;
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) colorspace = NULL;
	g_autoptr(GVariant) created = NULL;
	g_autoptr(GVariant) embedded = NULL;
	g_autoptr(GVariant) enabled = NULL;
	g_autoptr(GVariant) format = NULL;
	g_autoptr(GVariant) id = NULL;
	g_autoptr(GVariant) kind = NULL;
	g_autoptr(GVariant) metadata = NULL;
	g_autoptr(GVariant) model = NULL;
	g_autoptr(GVariant) mode = NULL;
	g_autoptr(GVariant) modified = NULL;
	g_autoptr(GVariant) owner = NULL;
	g_autoptr(GVariant) profiles = NULL;
	g_autoptr(GVariant) profiling_inhibitors = NULL;
	g_autoptr(GVariant) scope = NULL;
	g_autoptr(GVariant) seat = NULL;
	g_autoptr(GVariant) serial = NULL;
	g_autoptr(GVariant) vendor = NULL;

	device = CD_DEVICE (g_task_get_source_object (task));
	priv = GET_PRIVATE (device);
	priv->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (priv->proxy == NULL) {
		g_task_return_new_error (task,
					 CD_DEVICE_ERROR,
					 CD_DEVICE_ERROR_INTERNAL,
					 "Failed to connect to device %s: %s",
					 cd_device_get_object_path (device),
					 error->message);
		return;
	}

	/* get device id */
	id = g_dbus_proxy_get_cached_property (priv->proxy,
					       CD_DEVICE_PROPERTY_ID);
	if (id != NULL)
		priv->id = cd_device_get_nullable_str (id);

	/* if the device is missing, then fail */
	if (id == NULL) {
		g_task_return_new_error (task,
					 CD_DEVICE_ERROR,
					 CD_DEVICE_ERROR_INTERNAL,
					 "Failed to connect to missing device %s",
					 cd_device_get_object_path (device));
		return;
	}

	/* get kind */
	kind = g_dbus_proxy_get_cached_property (priv->proxy,
						 CD_DEVICE_PROPERTY_KIND);
	if (kind != NULL)
		priv->kind =
			cd_device_kind_from_string (g_variant_get_string (kind, NULL));

	/* get colorspace */
	colorspace = g_dbus_proxy_get_cached_property (priv->proxy,
						       CD_DEVICE_PROPERTY_COLORSPACE);
	if (colorspace != NULL)
		priv->colorspace =
			cd_colorspace_from_string (g_variant_get_string (colorspace, NULL));

	/* get scope */
	scope = g_dbus_proxy_get_cached_property (priv->proxy,
						  CD_DEVICE_PROPERTY_SCOPE);
	if (scope != NULL)
		priv->scope =
			cd_object_scope_from_string (g_variant_get_string (scope, NULL));

	/* get enabled */
	enabled = g_dbus_proxy_get_cached_property (priv->proxy,
						    CD_DEVICE_PROPERTY_ENABLED);
	if (enabled != NULL)
		priv->enabled = g_variant_get_boolean (enabled);

	/* get owner */
	owner = g_dbus_proxy_get_cached_property (priv->proxy,
						  CD_DEVICE_PROPERTY_OWNER);
	if (owner != NULL)
		priv->owner = g_variant_get_uint32 (owner);

	/* get mode */
	mode = g_dbus_proxy_get_cached_property (priv->proxy,
						 CD_DEVICE_PROPERTY_MODE);
	if (mode != NULL)
		priv->mode =
			cd_device_mode_from_string (g_variant_get_string (mode, NULL));

	/* get model */
	model = g_dbus_proxy_get_cached_property (priv->proxy,
						  CD_DEVICE_PROPERTY_MODEL);
	if (model != NULL)
		priv->model = cd_device_get_nullable_str (model);

	/* get serial */
	serial = g_dbus_proxy_get_cached_property (priv->proxy,
						   CD_DEVICE_PROPERTY_SERIAL);
	if (serial != NULL)
		priv->serial = cd_device_get_nullable_str (serial);

	/* get seat */
	seat = g_dbus_proxy_get_cached_property (priv->proxy,
						 CD_DEVICE_PROPERTY_SEAT);
	if (seat != NULL)
		priv->seat = cd_device_get_nullable_str (seat);

	/* get format */
	format = g_dbus_proxy_get_cached_property (priv->proxy,
						   CD_DEVICE_PROPERTY_FORMAT);
	if (format != NULL)
		priv->format = cd_device_get_nullable_str (format);

	/* get vendor */
	vendor = g_dbus_proxy_get_cached_property (priv->proxy,
						   CD_DEVICE_PROPERTY_VENDOR);
	if (vendor != NULL)
		priv->vendor = cd_device_get_nullable_str (vendor);

	/* get profiling inhibitors */
	profiling_inhibitors = g_dbus_proxy_get_cached_property (priv->proxy,
								 CD_DEVICE_PROPERTY_PROFILING_INHIBITORS);
	if (profiling_inhibitors != NULL)
		priv->profiling_inhibitors = g_variant_dup_strv (profiling_inhibitors, NULL);

	/* get created */
	created = g_dbus_proxy_get_cached_property (priv->proxy,
						    CD_DEVICE_PROPERTY_CREATED);
	if (created != NULL)
		priv->created = g_variant_get_uint64 (created);

	/* get modified */
	modified = g_dbus_proxy_get_cached_property (priv->proxy,
						     CD_DEVICE_PROPERTY_MODIFIED);
	if (modified != NULL)
		priv->modified = g_variant_get_uint64 (modified);

	/* get profiles */
	profiles = g_dbus_proxy_get_cached_property (priv->proxy,
						     CD_DEVICE_PROPERTY_PROFILES);
	cd_device_set_profiles_array_from_variant (device, profiles);

	/* get embedded */
	embedded = g_dbus_proxy_get_cached_property (priv->proxy,
						     CD_DEVICE_PROPERTY_EMBEDDED);
	if (embedded != NULL)
		priv->embedded = g_variant_get_boolean (embedded);

	/* get metadata */
	metadata = g_dbus_proxy_get_cached_property (priv->proxy,
						     CD_DEVICE_PROPERTY_METADATA);
	if (metadata != NULL)
		cd_device_set_metadata_from_variant (device, metadata);

	/* get signals from DBus */
	g_signal_connect_object (priv->proxy,
				 "g-signal",
				 G_CALLBACK (cd_device_dbus_signal_cb),
				 device, 0);

	/* watch if any remote properties change */
	g_signal_connect_object (priv->proxy,
				 "g-properties-changed",
				 G_CALLBACK (cd_device_dbus_properties_changed_cb),
				 device, 0);

	/* success */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_device_connect:
 * @device: a #CdDevice instance.
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Connects to the object and fills up initial properties.
 *
 * Since: 0.1.8
 **/
void
cd_device_connect (CdDevice *device,
		   GCancellable *cancellable,
		   GAsyncReadyCallback callback,
		   gpointer user_data)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_DEVICE (device));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (device, cancellable, callback, user_data);

	/* already connected */
	if (priv->proxy != NULL) {
		g_task_return_boolean (task, TRUE);
		return;
	}

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  COLORD_DBUS_SERVICE,
				  priv->object_path,
				  COLORD_DBUS_INTERFACE_DEVICE,
				  cancellable,
				  cd_device_connect_cb,
				  task);
}

/**********************************************************************/

/**
 * cd_device_set_property_finish:
 * @device: a #CdDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: success
 *
 * Since: 0.1.8
 **/
gboolean
cd_device_set_property_finish (CdDevice *device,
				GAsyncResult *res,
				GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, device), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

/**
 * cd_device_fixup_dbus_error:
 **/
static void
cd_device_fixup_dbus_error (GError *error)
{
	g_autofree gchar *name = NULL;

	g_return_if_fail (error != NULL);

	/* is a remote error? */
	if (!g_dbus_error_is_remote_error (error))
		return;

	/* parse the remote error */
	name = g_dbus_error_get_remote_error (error);
	error->domain = CD_DEVICE_ERROR;
	error->code = cd_device_error_from_string (name);
	g_dbus_error_strip_remote_error (error);
}

static void
cd_device_set_property_cb (GObject *source_object,
			    GAsyncResult *res,
			    gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_device_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_device_set_property:
 * @device: a #CdDevice instance.
 * @key: a property key
 * @value: a property key
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Sets a property on the device.
 *
 * Since: 0.1.8
 **/
void
cd_device_set_property (CdDevice *device,
			const gchar *key,
			const gchar *value,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_DEVICE (device));
	g_return_if_fail (key != NULL);
	g_return_if_fail (value != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (device, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "SetProperty",
			   g_variant_new ("(ss)",
			   		  key, value),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_device_set_property_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_device_add_profile_finish:
 * @device: a #CdDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: success
 *
 * Since: 0.1.8
 **/
gboolean
cd_device_add_profile_finish (CdDevice *device,
				GAsyncResult *res,
				GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, device), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_device_add_profile_cb (GObject *source_object,
			    GAsyncResult *res,
			    gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_device_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_device_add_profile:
 * @device: a #CdDevice instance.
 * @relation: a #CdDeviceRelation, e.g. #CD_DEVICE_RELATION_HARD
 * @profile: a #CdProfile instance
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Adds a profile to a device.
 *
 * Since: 0.1.8
 **/
void
cd_device_add_profile (CdDevice *device,
		       CdDeviceRelation relation,
		       CdProfile *profile,
		       GCancellable *cancellable,
		       GAsyncReadyCallback callback,
		       gpointer user_data)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_DEVICE (device));
	g_return_if_fail (CD_IS_PROFILE (profile));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (device, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "AddProfile",
			   g_variant_new ("(so)",
					  cd_device_relation_to_string (relation),
					  cd_profile_get_object_path (profile)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_device_add_profile_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_device_remove_profile_finish:
 * @device: a #CdDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: success
 *
 * Since: 0.1.8
 **/
gboolean
cd_device_remove_profile_finish (CdDevice *device,
				 GAsyncResult *res,
				 GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, device), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_device_remove_profile_cb (GObject *source_object,
			     GAsyncResult *res,
			     gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_device_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_device_remove_profile:
 * @device: a #CdDevice instance.
 * @profile: a #CdProfile instance
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Removes a profile from a device.
 *
 * Since: 0.1.8
 **/
void
cd_device_remove_profile (CdDevice *device,
			  CdProfile *profile,
			  GCancellable *cancellable,
			  GAsyncReadyCallback callback,
			  gpointer user_data)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_DEVICE (device));
	g_return_if_fail (CD_IS_PROFILE (profile));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (device, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "RemoveProfile",
			   g_variant_new ("(o)",
					  cd_profile_get_object_path (profile)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_device_remove_profile_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_device_make_profile_default_finish:
 * @device: a #CdDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: success
 *
 * Since: 0.1.8
 **/
gboolean
cd_device_make_profile_default_finish (CdDevice *device,
				       GAsyncResult *res,
				       GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, device), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_device_make_profile_default_cb (GObject *source_object,
				   GAsyncResult *res,
				   gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_device_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_device_make_profile_default:
 * @device: a #CdDevice instance.
 * @profile: a #CdProfile instance
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Makes an already added profile default for a device.
 *
 * Since: 0.1.8
 **/
void
cd_device_make_profile_default (CdDevice *device,
				CdProfile *profile,
				GCancellable *cancellable,
				GAsyncReadyCallback callback,
				gpointer user_data)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_DEVICE (device));
	g_return_if_fail (CD_IS_PROFILE (profile));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (device, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "MakeProfileDefault",
			   g_variant_new ("(o)",
					  cd_profile_get_object_path (profile)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_device_make_profile_default_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_device_profiling_inhibit_finish:
 * @device: a #CdDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: success
 *
 * Since: 0.1.8
 **/
gboolean
cd_device_profiling_inhibit_finish (CdDevice *device,
				    GAsyncResult *res,
				    GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, device), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_device_profiling_inhibit_cb (GObject *source_object,
				GAsyncResult *res,
				gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_device_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_device_profiling_inhibit:
 * @device: a #CdDevice instance.
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Sets up the device for profiling and causes no profiles to be
 * returned if cd_device_get_profile_for_qualifiers_sync() is used.
 *
 * Since: 0.1.8
 **/
void
cd_device_profiling_inhibit (CdDevice *device,
			     GCancellable *cancellable,
			     GAsyncReadyCallback callback,
			     gpointer user_data)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_DEVICE (device));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (device, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "ProfilingInhibit",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_device_profiling_inhibit_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_device_profiling_uninhibit_finish:
 * @device: a #CdDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: success
 *
 * Since: 0.1.8
 **/
gboolean
cd_device_profiling_uninhibit_finish (CdDevice *device,
				      GAsyncResult *res,
				      GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, device), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_device_profiling_uninhibit_cb (GObject *source_object,
				  GAsyncResult *res,
				  gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GVariant) result = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_device_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_device_profiling_uninhibit:
 * @device: a #CdDevice instance.
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Restores the device after profiling and causes normal profiles to be
 * returned if cd_device_get_profile_for_qualifiers_sync() is used.
 *
 * Since: 0.1.8
 **/
void
cd_device_profiling_uninhibit (CdDevice *device,
			       GCancellable *cancellable,
			       GAsyncReadyCallback callback,
			       gpointer user_data)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_DEVICE (device));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (device, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "ProfilingUninhibit",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_device_profiling_uninhibit_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_device_get_profile_for_qualifiers_finish:
 * @device: a #CdDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: (transfer full): a #CdProfile or %NULL
 *
 * Since: 0.1.8
 **/
CdProfile *
cd_device_get_profile_for_qualifiers_finish (CdDevice *device,
					     GAsyncResult *res,
					     GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, device), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_device_get_profile_for_qualifiers_cb (GObject *source_object,
					 GAsyncResult *res,
					 gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GVariant) result = NULL;
	CdProfile *profile;
	gchar *object_path = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_device_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create CdProfile object */
	g_variant_get (result, "(o)", &object_path);
	profile = cd_profile_new ();
	cd_profile_set_object_path (profile, object_path);

	/* success */
	g_task_return_pointer (task, profile, (GDestroyNotify) g_object_unref);
}

/**
 * cd_device_get_profile_for_qualifiers:
 * @device: a #CdDevice instance.
 * @qualifiers: a set of qualifiers that can included wildcards
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Gets the prefered profile for some qualifiers.
 *
 * Since: 0.1.8
 **/
void
cd_device_get_profile_for_qualifiers (CdDevice *device,
				      const gchar **qualifiers,
				      GCancellable *cancellable,
				      GAsyncReadyCallback callback,
				      gpointer user_data)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	GTask *task = NULL;	guint i;
	GVariantBuilder builder;

	g_return_if_fail (CD_IS_DEVICE (device));
	g_return_if_fail (qualifiers != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	/* squash char** into an array of strings */
	g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
	for (i = 0; qualifiers[i] != NULL; i++)
		g_variant_builder_add (&builder, "s", qualifiers[i]);

	task = g_task_new (device, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "GetProfileForQualifiers",
			   g_variant_new ("(as)",
					  &builder),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_device_get_profile_for_qualifiers_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_device_get_profile_relation_finish:
 * @device: a #CdDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: success
 *
 * Since: 0.1.8
 **/
CdDeviceRelation
cd_device_get_profile_relation_finish (CdDevice *device,
				       GAsyncResult *res,
				       GError **error)
{
	gssize tmp;
	g_return_val_if_fail (g_task_is_valid (res, device), 0);
	tmp = g_task_propagate_int (G_TASK (res), error);
	if (tmp == -1)
		return CD_DEVICE_RELATION_UNKNOWN;
	return tmp;
}

static void
cd_device_get_profile_relation_cb (GObject *source_object,
				   GAsyncResult *res,
				   gpointer user_data)
{
	CdDeviceRelation relation;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *relation_string = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_device_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	g_variant_get (result, "(s)", &relation_string);
	relation = cd_device_relation_from_string (relation_string);
	g_task_return_int (task, relation);
}

/**
 * cd_device_get_profile_relation:
 * @device: a #CdDevice instance.
 * @profile: a #CdProfile instance
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Gets the property relationship to the device.
 *
 * Since: 0.1.8
 **/
void
cd_device_get_profile_relation (CdDevice *device,
				CdProfile *profile,
				GCancellable *cancellable,
				GAsyncReadyCallback callback,
				gpointer user_data)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_DEVICE (device));
	g_return_if_fail (CD_IS_PROFILE (profile));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (device, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "GetProfileRelation",
			   g_variant_new ("(o)",
					  cd_profile_get_object_path (profile)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_device_get_profile_relation_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_device_set_enabled_finish:
 * @device: a #CdDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: success
 *
 * Since: 0.1.26
 **/
gboolean
cd_device_set_enabled_finish (CdDevice *device,
			      GAsyncResult *res,
			      GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, device), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_device_set_enabled_cb (GObject *source_object,
			  GAsyncResult *res,
			  gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_device_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_device_set_enabled:
 * @device: a #CdDevice instance.
 * @enabled: the enabled state
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Enables or disables a device.
 *
 * Since: 0.1.26
 **/
void
cd_device_set_enabled (CdDevice *device,
		       gboolean enabled,
		       GCancellable *cancellable,
		       GAsyncReadyCallback callback,
		       gpointer user_data)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_DEVICE (device));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (device, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "SetEnabled",
			   g_variant_new ("(b)", enabled),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_device_set_enabled_cb,
			   task);
}

/**********************************************************************/

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
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	return priv->object_path;
}

/**
 * cd_device_get_connected:
 * @device: a #CdDevice instance.
 *
 * Gets if the device has been connected.
 *
 * Return value: %TRUE if properties are valid
 *
 * Since: 0.1.9
 **/
gboolean
cd_device_get_connected (CdDevice *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	g_return_val_if_fail (CD_IS_DEVICE (device), FALSE);
	return priv->proxy != NULL;
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
	CdDevicePrivate *priv = GET_PRIVATE (device);
	struct tm *time_tm;
	time_t t;
	gchar time_buf[256];
	GString *string;

	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);

	/* get a human readable time */
	t = (time_t) priv->created;
	time_tm = localtime (&t);
	strftime (time_buf, sizeof time_buf, "%c", time_tm);

	string = g_string_new ("");
	g_string_append_printf (string, "  object-path:          %s\n",
				priv->object_path);
	g_string_append_printf (string, "  created:              %s\n",
				time_buf);

	return g_string_free (string, FALSE);
}

/**
 * cd_device_equal:
 * @device1: one #CdDevice instance.
 * @device2: another #CdDevice instance.
 *
 * Tests two devices for equality.
 *
 * Return value: %TRUE if the devices are the same device
 *
 * Since: 0.1.8
 **/
gboolean
cd_device_equal (CdDevice *device1, CdDevice *device2)
{
	CdDevicePrivate *priv1 = GET_PRIVATE (device1);
	CdDevicePrivate *priv2 = GET_PRIVATE (device2);
	g_return_val_if_fail (CD_IS_DEVICE (device1), FALSE);
	g_return_val_if_fail (CD_IS_DEVICE (device2), FALSE);
	return g_strcmp0 (priv1->id, priv2->id) == 0;
}

/*
 * _cd_device_set_property:
 */
static void
_cd_device_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CdDevice *device = CD_DEVICE (object);
	CdDevicePrivate *priv = GET_PRIVATE (device);

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_free (priv->object_path);
		priv->object_path = g_value_dup_string (value);
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
	CdDevicePrivate *priv = GET_PRIVATE (device);

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_value_set_string (value, priv->object_path);
		break;
	case PROP_CONNECTED:
		g_value_set_boolean (value, priv->proxy != NULL);
		break;
	case PROP_CREATED:
		g_value_set_uint64 (value, priv->created);
		break;
	case PROP_MODIFIED:
		g_value_set_uint64 (value, priv->modified);
		break;
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	case PROP_SERIAL:
		g_value_set_string (value, priv->serial);
		break;
	case PROP_SEAT:
		g_value_set_string (value, priv->seat);
		break;
	case PROP_FORMAT:
		g_value_set_string (value, priv->format);
		break;
	case PROP_VENDOR:
		g_value_set_string (value, priv->vendor);
		break;
	case PROP_PROFILING_INHIBITORS:
		g_value_set_boxed (value, priv->profiling_inhibitors);
		break;
	case PROP_KIND:
		g_value_set_uint (value, priv->kind);
		break;
	case PROP_COLORSPACE:
		g_value_set_uint (value, priv->colorspace);
		break;
	case PROP_MODE:
		g_value_set_uint (value, priv->mode);
		break;
	case PROP_SCOPE:
		g_value_set_uint (value, priv->scope);
		break;
	case PROP_ENABLED:
		g_value_set_boolean (value, priv->enabled);
		break;
	case PROP_OWNER:
		g_value_set_uint (value, priv->owner);
		break;
	case PROP_EMBEDDED:
		g_value_set_boolean (value, priv->embedded);
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
	object_class->set_property = _cd_device_set_property;
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
	 * CdDevice:object-path:
	 *
	 * The object path of the remote object
	 *
	 * Since: 0.1.8
	 **/
	g_object_class_install_property (object_class,
					 PROP_OBJECT_PATH,
					 g_param_spec_string ("object-path",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * CdDevice:connected:
	 *
	 * The if the object path has been connected as is valid for use.
	 *
	 * Since: 0.1.9
	 **/
	g_object_class_install_property (object_class,
					 PROP_CONNECTED,
					 g_param_spec_string ("connected",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
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
							      G_PARAM_READABLE));
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
							      G_PARAM_READABLE));
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
							      G_PARAM_READABLE));
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
							      G_PARAM_READABLE));
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
							      G_PARAM_READABLE));
	/**
	 * CdDevice:seat:
	 *
	 * The device seat identifier.
	 *
	 * Since: 0.1.24
	 **/
	g_object_class_install_property (object_class,
					 PROP_SEAT,
					 g_param_spec_string ("seat",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
	/**
	 * CdDevice:format:
	 *
	 * The device format.
	 *
	 * Since: 0.1.9
	 **/
	g_object_class_install_property (object_class,
					 PROP_FORMAT,
					 g_param_spec_string ("format",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
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
							      G_PARAM_READABLE));
	/**
	 * CdDevice:profiling_inhibitors:
	 *
	 * Any profiling inhibitors.
	 *
	 * Since: 0.1.17
	 **/
	g_object_class_install_property (object_class,
					 PROP_PROFILING_INHIBITORS,
					 g_param_spec_boxed ("profiling-inhibitors",
							      NULL, NULL,
							      G_TYPE_STRV,
							      G_PARAM_READABLE));
	/**
	 * CdDevice:kind:
	 *
	 * The device kind, e.g. %CD_DEVICE_KIND_DISPLAY.
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
							    G_PARAM_READABLE));
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
							    G_PARAM_READABLE));

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
							    G_PARAM_READABLE));

	/**
	 * CdDevice:scope:
	 *
	 * The device scope, e.g. %CD_OBJECT_SCOPE_TEMP.
	 *
	 * Since: 0.1.10
	 **/
	g_object_class_install_property (object_class,
					 PROP_SCOPE,
					 g_param_spec_uint ("scope",
							    NULL, NULL,
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READABLE));

	/**
	 * CdDevice:enabled:
	 *
	 * The device enabled state.
	 *
	 * Since: 0.1.26
	 **/
	g_object_class_install_property (object_class,
					 PROP_ENABLED,
					 g_param_spec_boolean ("enabled",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READABLE));

	/**
	 * CdDevice:owner:
	 *
	 * The device owner, e.g. 500.
	 *
	 * Since: 0.1.13
	 **/
	g_object_class_install_property (object_class,
					 PROP_OWNER,
					 g_param_spec_uint ("owner",
							    NULL, NULL,
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READABLE));
	/**
	 * CdDevice:embedded:
	 *
	 * If the device is embedded in the device and cannot be removed.
	 *
	 * Since: 0.1.27
	 **/
	g_object_class_install_property (object_class,
					 PROP_EMBEDDED,
					 g_param_spec_string ("embedded",
							      NULL, NULL,
							      FALSE,
							      G_PARAM_READABLE));
}

/*
 * cd_device_init:
 */
static void
cd_device_init (CdDevice *device)
{
	CdDevicePrivate *priv = GET_PRIVATE (device);
	priv->profiles = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->metadata = g_hash_table_new_full (g_str_hash,
							g_str_equal,
							g_free,
							g_free);
}

/*
 * cd_device_finalize:
 */
static void
cd_device_finalize (GObject *object)
{
	CdDevice *device = CD_DEVICE (object);
	CdDevicePrivate *priv = GET_PRIVATE (device);

	g_return_if_fail (CD_IS_DEVICE (object));

	g_hash_table_destroy (priv->metadata);
	g_free (priv->object_path);
	g_free (priv->id);
	g_free (priv->model);
	g_free (priv->serial);
	g_free (priv->seat);
	g_free (priv->format);
	g_free (priv->vendor);
	g_strfreev (priv->profiling_inhibitors);
	g_ptr_array_unref (priv->profiles);
	if (priv->proxy != NULL)
		g_object_unref (priv->proxy);

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

/**
 * cd_device_new_with_object_path:
 * @object_path: The colord object path.
 *
 * Creates a new #CdDevice object with a known object path.
 *
 * Return value: a new device object.
 *
 * Since: 0.1.8
 **/
CdDevice *
cd_device_new_with_object_path (const gchar *object_path)
{
	CdDevice *device;
	device = g_object_new (CD_TYPE_DEVICE,
			       "object-path", object_path,
			       NULL);
	return CD_DEVICE (device);
}
