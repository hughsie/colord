/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2015 Richard Hughes <richard@hughsie.com>
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
 * SECTION:cd-sensor
 * @short_description: Client object for accessing information about colord sensors
 *
 * A helper GObject to use for accessing colord sensors, and to be notified
 * when it is changed.
 *
 * See also: #CdClient
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <string.h>

#include "cd-sensor.h"

static void	cd_sensor_class_init	(CdSensorClass	*klass);
static void	cd_sensor_init		(CdSensor	*sensor);
static void	cd_sensor_finalize	(GObject	*object);

#define GET_PRIVATE(o) (cd_sensor_get_instance_private (o))

#define COLORD_DBUS_SERVICE		"org.freedesktop.ColorManager"
#define COLORD_DBUS_INTERFACE_SENSOR	"org.freedesktop.ColorManager.Sensor"

/**
 * CdSensorPrivate:
 *
 * Private #CdSensor data
 **/
typedef struct
{
	gchar			*object_path;
	gchar			*id;
	CdSensorKind		 kind;
	CdSensorState		 state;
	CdSensorCap		 mode;
	gchar			*serial;
	gchar			*model;
	gchar			*vendor;
	gboolean		 native;
	gboolean		 embedded;
	gboolean		 locked;
	guint64			 caps;
	GHashTable		*options;
	GHashTable		*metadata;
	GDBusProxy		*proxy;
} CdSensorPrivate;

enum {
	PROP_0,
	PROP_OBJECT_PATH,
	PROP_ID,
	PROP_CONNECTED,
	PROP_KIND,
	PROP_STATE,
	PROP_MODE,
	PROP_SERIAL,
	PROP_MODEL,
	PROP_VENDOR,
	PROP_NATIVE,
	PROP_EMBEDDED,
	PROP_LOCKED,
	PROP_LAST
};

enum {
	SIGNAL_BUTTON_PRESSED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (CdSensor, cd_sensor, G_TYPE_OBJECT)

/**
 * cd_sensor_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.6
 **/
GQuark
cd_sensor_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("cd_sensor_error");
	return quark;
}

/**
 * cd_sensor_set_object_path:
 * @sensor: a #CdSensor instance.
 * @object_path: The colord object path.
 *
 * Sets the object path of the sensor.
 *
 * Since: 0.1.8
 **/
void
cd_sensor_set_object_path (CdSensor *sensor, const gchar *object_path)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_return_if_fail (priv->object_path == NULL);
	priv->object_path = g_strdup (object_path);
}

/**
 * cd_sensor_get_kind:
 * @sensor: a #CdSensor instance.
 *
 * Gets the sensor kind.
 *
 * Return value: A #CdSensorKind, e.g. %CD_SENSOR_KIND_HUEY
 *
 * Since: 0.1.6
 **/
CdSensorKind
cd_sensor_get_kind (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), CD_SENSOR_KIND_UNKNOWN);
	g_return_val_if_fail (priv->proxy != NULL, CD_SENSOR_KIND_UNKNOWN);
	return priv->kind;
}

/**
 * cd_sensor_get_state:
 * @sensor: a #CdSensor instance.
 *
 * Gets the sensor state.
 *
 * Return value: A #CdSensorState, e.g. %CD_SENSOR_STATE_IDLE
 *
 * Since: 0.1.6
 **/
CdSensorState
cd_sensor_get_state (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), CD_SENSOR_STATE_UNKNOWN);
	g_return_val_if_fail (priv->proxy != NULL, CD_SENSOR_STATE_UNKNOWN);
	return priv->state;
}

/**
 * cd_sensor_get_mode:
 * @sensor: a #CdSensor instance.
 *
 * Gets the sensor operating mode.
 *
 * Return value: A #CdSensorCap, e.g. %CD_SENSOR_CAP_AMBIENT
 *
 * Since: 0.1.6
 **/
CdSensorCap
cd_sensor_get_mode (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), CD_SENSOR_CAP_UNKNOWN);
	g_return_val_if_fail (priv->proxy != NULL, CD_SENSOR_CAP_UNKNOWN);
	return priv->mode;
}

/**
 * cd_sensor_get_serial:
 * @sensor: a #CdSensor instance.
 *
 * Gets the sensor serial number.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.6
 **/
const gchar *
cd_sensor_get_serial (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->serial;
}

/**
 * cd_sensor_get_model:
 * @sensor: a #CdSensor instance.
 *
 * Gets the sensor model.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.6
 **/
const gchar *
cd_sensor_get_model (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->model;
}

/**
 * cd_sensor_get_vendor:
 * @sensor: a #CdSensor instance.
 *
 * Gets the sensor vendor.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.6
 **/
const gchar *
cd_sensor_get_vendor (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->vendor;
}

/**
 * cd_sensor_get_native:
 * @sensor: a #CdSensor instance.
 *
 * Returns if the sensor has a native driver.
 *
 * Return value: %TRUE if VCGT is valid.
 *
 * Since: 0.1.6
 **/
gboolean
cd_sensor_get_native (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (priv->proxy != NULL, FALSE);
	return priv->native;
}

/**
 * cd_sensor_get_embedded:
 * @sensor: a #CdSensor instance.
 *
 * Returns if the sensor is embedded into the computer.
 *
 * Return value: %TRUE if embedded.
 *
 * Since: 0.1.26
 **/
gboolean
cd_sensor_get_embedded (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (priv->proxy != NULL, FALSE);
	return priv->embedded;
}

/**
 * cd_sensor_get_locked:
 * @sensor: a #CdSensor instance.
 *
 * Returns if the sensor is locked.
 *
 * Return value: %TRUE if VCGT is valid.
 *
 * Since: 0.1.6
 **/
gboolean
cd_sensor_get_locked (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (priv->proxy != NULL, FALSE);
	return priv->locked;
}

/**
 * cd_sensor_get_caps:
 * @sensor: a #CdSensor instance.
 *
 * Returns the sensor metadata.
 *
 * Return value: The sensor capability bitfield.
 *
 * Since: 0.1.6
 **/
guint64
cd_sensor_get_caps (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), 0);
	g_return_val_if_fail (priv->proxy != NULL, 0);
	return priv->caps;
}

/**
 * cd_sensor_has_cap:
 * @sensor: a #CdSensor instance.
 * @cap: a specified capability, e.g. %CD_SENSOR_CAP_LCD
 *
 * Returns the sensor metadata for a specific key.
 *
 * Return value: %TRUE if the sensor has the specified capability
 *
 * Since: 0.1.6
 **/
gboolean
cd_sensor_has_cap (CdSensor *sensor, CdSensorCap cap)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (priv->proxy != NULL, FALSE);
	return cd_bitfield_contain (priv->caps, cap);
}

/**
 * cd_sensor_set_caps_from_variant:
 **/
static void
cd_sensor_set_caps_from_variant (CdSensor *sensor, GVariant *variant)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	const gchar **caps_tmp;
	guint i;

	/* remove old entries */
	priv->caps = 0;

	/* insert the new metadata */
	caps_tmp = g_variant_get_strv (variant, NULL);
	for (i = 0; caps_tmp[i] != NULL; i++) {
		cd_bitfield_add (priv->caps,
				 cd_sensor_cap_from_string (caps_tmp[i]));
	}
	g_free (caps_tmp);
}

/**
 * cd_sensor_set_options_from_variant:
 **/
static void
cd_sensor_set_options_from_variant (CdSensor *sensor, GVariant *variant)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	const gchar *prop_key;
	GVariantIter iter;
	GVariant *prop_value;

	/* remove old entries */
	g_hash_table_remove_all (priv->options);

	/* insert the new options */
	g_variant_iter_init (&iter, variant);
	while (g_variant_iter_loop (&iter, "{sv}",
				    &prop_key, &prop_value)) {
		g_hash_table_insert (priv->options,
				     g_strdup (prop_key),
				     g_variant_ref (prop_value));

	}
}

/**
 * cd_sensor_set_metadata_from_variant:
 **/
static void
cd_sensor_set_metadata_from_variant (CdSensor *sensor, GVariant *variant)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
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
 * cd_sensor_dbus_properties_changed_cb:
 **/
static void
cd_sensor_dbus_properties_changed_cb (GDBusProxy *proxy,
				      GVariant *changed_properties,
				      const gchar * const *invalidated_properties,
				      CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	guint i;
	guint len;
	GVariantIter iter;
	gchar *property_name;
	GVariant *property_value;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	len = g_variant_iter_init (&iter, changed_properties);
	for (i = 0; i < len; i++) {
		g_variant_get_child (changed_properties, i,
				     "{&sv}",
				     &property_name,
				     &property_value);
		if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_KIND) == 0) {
			priv->kind = cd_sensor_kind_from_string (g_variant_get_string (property_value, NULL));
			g_object_notify (G_OBJECT (sensor), "kind");
		} else if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_STATE) == 0) {
			priv->state = cd_sensor_state_from_string (g_variant_get_string (property_value, NULL));
			g_object_notify (G_OBJECT (sensor), "state");
		} else if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_MODE) == 0) {
			priv->mode = cd_sensor_cap_from_string (g_variant_get_string (property_value, NULL));
			g_object_notify (G_OBJECT (sensor), "mode");
		} else if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_SERIAL) == 0) {
			g_free (priv->serial);
			priv->serial = g_variant_dup_string (property_value, NULL);
			g_object_notify (G_OBJECT (sensor), "serial");
		} else if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_MODEL) == 0) {
			g_free (priv->model);
			priv->model = g_variant_dup_string (property_value, NULL);
			g_object_notify (G_OBJECT (sensor), "model");
		} else if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_VENDOR) == 0) {
			g_free (priv->vendor);
			priv->vendor = g_variant_dup_string (property_value, NULL);
			g_object_notify (G_OBJECT (sensor), "vendor");
		} else if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_ID) == 0) {
			g_free (priv->id);
			priv->id = g_variant_dup_string (property_value, NULL);
			g_object_notify (G_OBJECT (sensor), "id");
		} else if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_NATIVE) == 0) {
			priv->native = g_variant_get_boolean (property_value);
			g_object_notify (G_OBJECT (sensor), "native");
		} else if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_EMBEDDED) == 0) {
			priv->embedded = g_variant_get_boolean (property_value);
			g_object_notify (G_OBJECT (sensor), "embedded");
		} else if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_LOCKED) == 0) {
			priv->locked = g_variant_get_boolean (property_value);
			g_object_notify (G_OBJECT (sensor), "locked");
		} else if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_CAPABILITIES) == 0) {
			cd_sensor_set_caps_from_variant (sensor, property_value);
			g_object_notify (G_OBJECT (sensor), "capabilities");
		} else if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_OPTIONS) == 0) {
			cd_sensor_set_options_from_variant (sensor, property_value);
		} else if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_METADATA) == 0) {
			cd_sensor_set_metadata_from_variant (sensor, property_value);
		} else {
			g_warning ("%s property unhandled", property_name);
		}
		g_variant_unref (property_value);
	}
}

/**
 * cd_sensor_dbus_signal_cb:
 **/
static void
cd_sensor_dbus_signal_cb (GDBusProxy *proxy,
			  gchar      *sender_name,
			  gchar      *signal_name,
			  GVariant   *parameters,
			  CdSensor   *sensor)
{
	g_return_if_fail (CD_IS_SENSOR (sensor));

	if (g_strcmp0 (signal_name, "ButtonPressed") == 0) {
		g_signal_emit (sensor, signals[SIGNAL_BUTTON_PRESSED], 0);
	} else {
		g_warning ("unhandled signal '%s'", signal_name);
	}
}

/**********************************************************************/

/**
 * cd_sensor_connect_cb:
 **/
static void
cd_sensor_connect_cb (GObject *source_object,
		      GAsyncResult *res,
		      gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) caps = NULL;
	g_autoptr(GVariant) embedded = NULL;
	g_autoptr(GVariant) id = NULL;
	g_autoptr(GVariant) kind = NULL;
	g_autoptr(GVariant) locked = NULL;
	g_autoptr(GVariant) metadata = NULL;
	g_autoptr(GVariant) model = NULL;
	g_autoptr(GVariant) mode = NULL;
	g_autoptr(GVariant) native = NULL;
	g_autoptr(GVariant) serial = NULL;
	g_autoptr(GVariant) state = NULL;
	g_autoptr(GVariant) vendor = NULL;
	CdSensor *sensor = CD_SENSOR (g_task_get_source_object (task));
	CdSensorPrivate *priv = GET_PRIVATE (sensor);

	/* get result */
	priv->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (priv->proxy == NULL) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "Failed to connect to sensor %s: %s",
					 cd_sensor_get_object_path (sensor),
					 error->message);
		return;
	}

	/* get kind */
	kind = g_dbus_proxy_get_cached_property (priv->proxy,
						 CD_SENSOR_PROPERTY_KIND);
	if (kind != NULL)
		priv->kind = cd_sensor_kind_from_string (g_variant_get_string (kind, NULL));

	/* get state */
	state = g_dbus_proxy_get_cached_property (priv->proxy,
						  CD_SENSOR_PROPERTY_STATE);
	if (state != NULL)
		priv->state = cd_colorspace_from_string (g_variant_get_string (state, NULL));

	/* get mode */
	mode = g_dbus_proxy_get_cached_property (priv->proxy,
						 CD_SENSOR_PROPERTY_MODE);
	if (mode != NULL)
		priv->mode = cd_sensor_cap_from_string (g_variant_get_string (state, NULL));

	/* get sensor serial */
	serial = g_dbus_proxy_get_cached_property (priv->proxy,
						   CD_SENSOR_PROPERTY_SERIAL);
	if (serial != NULL)
		priv->serial = g_variant_dup_string (serial, NULL);

	/* get vendor */
	vendor = g_dbus_proxy_get_cached_property (priv->proxy,
						   CD_SENSOR_PROPERTY_VENDOR);
	if (vendor != NULL)
		priv->vendor = g_variant_dup_string (vendor, NULL);

	/* get model */
	model = g_dbus_proxy_get_cached_property (priv->proxy,
						  CD_SENSOR_PROPERTY_MODEL);
	if (model != NULL)
		priv->model = g_variant_dup_string (model, NULL);

	/* get id */
	id = g_dbus_proxy_get_cached_property (priv->proxy,
					       CD_SENSOR_PROPERTY_ID);
	if (id != NULL)
		priv->id = g_variant_dup_string (id, NULL);

	/* get native */
	native = g_dbus_proxy_get_cached_property (priv->proxy,
						   CD_SENSOR_PROPERTY_NATIVE);
	if (native != NULL)
		priv->native = g_variant_get_boolean (native);

	/* get embedded */
	embedded = g_dbus_proxy_get_cached_property (priv->proxy,
						   CD_SENSOR_PROPERTY_EMBEDDED);
	if (embedded != NULL)
		priv->embedded = g_variant_get_boolean (embedded);

	/* get locked */
	locked = g_dbus_proxy_get_cached_property (priv->proxy,
						   CD_SENSOR_PROPERTY_LOCKED);
	if (locked != NULL)
		priv->locked = g_variant_get_boolean (locked);

	/* get if system wide */
	caps = g_dbus_proxy_get_cached_property (priv->proxy,
						 CD_SENSOR_PROPERTY_CAPABILITIES);
	if (caps != NULL)
		cd_sensor_set_caps_from_variant (sensor, caps);

	/* get metadata */
	metadata = g_dbus_proxy_get_cached_property (priv->proxy,
						     CD_SENSOR_PROPERTY_METADATA);
	if (metadata != NULL)
		cd_sensor_set_metadata_from_variant (sensor, metadata);

	/* get signals from DBus */
	g_signal_connect_object (priv->proxy,
				 "g-signal",
				 G_CALLBACK (cd_sensor_dbus_signal_cb),
				 sensor, 0);

	/* watch if any remote properties change */
	g_signal_connect_object (priv->proxy,
				 "g-properties-changed",
				 G_CALLBACK (cd_sensor_dbus_properties_changed_cb),
				 sensor, 0);

	/* we're done */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_sensor_connect:
 * @sensor: a #CdSensor instance
 * @cancellable: a #GCancellable or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Connects to the sensor.
 *
 * Since: 0.1.8
 **/
void
cd_sensor_connect (CdSensor *sensor,
		   GCancellable *cancellable,
		   GAsyncReadyCallback callback,
		   gpointer user_data)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_return_if_fail (callback != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (sensor, cancellable, callback, user_data);

	/* already connected */
	if (priv->proxy != NULL) {
		g_task_return_boolean (task, TRUE);
		return;
	}

	/* connect async */
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  COLORD_DBUS_SERVICE,
				  priv->object_path,
				  COLORD_DBUS_INTERFACE_SENSOR,
				  cancellable,
				  cd_sensor_connect_cb,
				  task);
}

/**
 * cd_sensor_connect_finish:
 * @sensor: a #CdSensor instance
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: %TRUE if we could connect to to the sensor
 *
 * Since: 0.1.8
 **/
gboolean
cd_sensor_connect_finish (CdSensor *sensor,
			  GAsyncResult *res,
			  GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

/**********************************************************************/

/**
 * cd_sensor_lock_finish:
 * @sensor: a #CdSensor instance.
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
cd_sensor_lock_finish (CdSensor *sensor,
				GAsyncResult *res,
				GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

/**
 * cd_sensor_fixup_dbus_error:
 **/
static void
cd_sensor_fixup_dbus_error (GError *error)
{
	g_autofree gchar *name = NULL;

	g_return_if_fail (error != NULL);

	/* is a remote error? */
	if (!g_dbus_error_is_remote_error (error))
		return;

	/* parse the remote error */
	name = g_dbus_error_get_remote_error (error);
	error->domain = CD_SENSOR_ERROR;
	error->code = cd_sensor_error_from_string (name);
	g_dbus_error_strip_remote_error (error);
}

static void
cd_sensor_lock_cb (GObject *source_object,
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
		cd_sensor_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_sensor_lock:
 * @sensor: a #CdSensor instance.
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Locks the device so we can use it.
 *
 * Since: 0.1.8
 **/
void
cd_sensor_lock (CdSensor *sensor,
		GCancellable *cancellable,
		GAsyncReadyCallback callback,
		gpointer user_data)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (sensor, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "Lock",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_sensor_lock_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_sensor_unlock_finish:
 * @sensor: a #CdSensor instance.
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
cd_sensor_unlock_finish (CdSensor *sensor,
			 GAsyncResult *res,
			 GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_sensor_unlock_cb (GObject *source_object,
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
		cd_sensor_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_sensor_unlock:
 * @sensor: a #CdSensor instance.
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Unlocks the sensor for use by other programs.
 *
 * Since: 0.1.8
 **/
void
cd_sensor_unlock (CdSensor *sensor,
		  GCancellable *cancellable,
		  GAsyncReadyCallback callback,
		  gpointer user_data)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (sensor, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "Unlock",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_sensor_unlock_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_sensor_set_options_finish:
 * @sensor: a #CdSensor instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: success
 *
 * Since: 0.1.20
 **/
gboolean
cd_sensor_set_options_finish (CdSensor *sensor,
			      GAsyncResult *res,
			      GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_sensor_set_options_cb (GObject *source_object,
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
		cd_sensor_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_sensor_set_options:
 * @sensor: a #CdSensor instance.
 * @values: (element-type utf8 GVariant): the options
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Sets options on the sensor device.
 *
 * Since: 0.1.20
 **/
void
cd_sensor_set_options (CdSensor *sensor,
		       GHashTable *values,
		       GCancellable *cancellable,
		       GAsyncReadyCallback callback,
		       gpointer user_data)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	GList *list, *l;
	GTask *task = NULL;	GVariantBuilder builder;

	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (sensor, cancellable, callback, user_data);

	/* convert the hash table to an array of {sv} */
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	list = g_hash_table_get_keys (values);
	for (l = list; l != NULL; l = l->next) {
		g_variant_builder_add (&builder,
				       "{sv}",
				       l->data,
				       g_hash_table_lookup (values,
							    l->data));
	}
	g_list_free (list);

	g_dbus_proxy_call (priv->proxy,
			   "SetOptions",
			   g_variant_new ("(a{sv})",
					  &builder),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_sensor_set_options_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_sensor_get_sample_finish:
 * @sensor: a #CdSensor instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: the XYZ reading, or %NULL
 *
 * Since: 0.1.8
 **/
CdColorXYZ *
cd_sensor_get_sample_finish (CdSensor *sensor,
			     GAsyncResult *res,
			     GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_sensor_get_sample_cb (GObject *source_object,
			 GAsyncResult *res,
			 gpointer user_data)
{
	CdColorXYZ *xyz;
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_sensor_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	xyz = cd_color_xyz_new ();
	g_variant_get (result,
		       "(ddd)",
		       &xyz->X,
		       &xyz->Y,
		       &xyz->Z);

	g_task_return_pointer (task, xyz, (GDestroyNotify) cd_color_xyz_free);
}

/**
 * cd_sensor_get_sample:
 * @sensor: a #CdSensor instance.
 * @cap: a #CdSensorCap
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Gets a color sample from a sensor
 *
 * Since: 0.1.8
 **/
void
cd_sensor_get_sample (CdSensor *sensor,
		      CdSensorCap cap,
		      GCancellable *cancellable,
		      GAsyncReadyCallback callback,
		      gpointer user_data)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (sensor, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "GetSample",
			   g_variant_new ("(s)",
			   		  cd_sensor_cap_to_string (cap)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_sensor_get_sample_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_sensor_get_spectrum_finish:
 * @sensor: a #CdSensor instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: the XYZ reading, or %NULL
 *
 * Since: 1.3.1
 **/
CdSpectrum *
cd_sensor_get_spectrum_finish (CdSensor *sensor,
			       GAsyncResult *res,
			       GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_sensor_get_spectrum_cb (GObject *source_object,
			   GAsyncResult *res,
			   gpointer user_data)
{
	CdSpectrum *sp;
	GVariantIter iter;
	gdouble sp_start = 0.f;
	gdouble sp_end = 0.f;
	gdouble tmp;
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;
	g_autoptr(GVariant) data = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_sensor_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create object from data */
	sp = cd_spectrum_new ();
	g_variant_get_child (result, 0, "d", &sp_start);
	g_variant_get_child (result, 1, "d", &sp_end);
	cd_spectrum_set_start (sp, sp_start);
	cd_spectrum_set_end (sp, sp_end);
	data = g_variant_get_child_value (result, 2);
	g_variant_iter_init (&iter, data);
	while (g_variant_iter_loop (&iter, "d", &tmp))
		cd_spectrum_add_value (sp, tmp);

	/* success */
	g_task_return_pointer (task, sp, (GDestroyNotify) cd_spectrum_free);
}

/**
 * cd_sensor_get_spectrum:
 * @sensor: a #CdSensor instance.
 * @cap: a #CdSensorCap
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Gets a color spectrum from a sensor
 *
 * Since: 1.3.1
 **/
void
cd_sensor_get_spectrum (CdSensor *sensor,
			CdSensorCap cap,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (sensor, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "GetSpectrum",
			   g_variant_new ("(s)",
					  cd_sensor_cap_to_string (cap)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_sensor_get_spectrum_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_sensor_get_object_path:
 * @sensor: a #CdSensor instance.
 *
 * Gets the object path for the sensor.
 *
 * Return value: the object path, or %NULL
 *
 * Since: 0.1.6
 **/
const gchar *
cd_sensor_get_object_path (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	return priv->object_path;
}

/**
 * cd_sensor_get_id:
 * @sensor: a #CdSensor instance.
 *
 * Gets the object ID for the sensor.
 *
 * Return value: the object ID, or %NULL
 *
 * Since: 0.1.26
 **/
const gchar *
cd_sensor_get_id (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	return priv->id;
}

/**
 * cd_sensor_get_connected:
 * @sensor: a #CdSensor instance.
 *
 * Gets if the sensor has been connected.
 *
 * Return value: %TRUE if properties are valid
 *
 * Since: 0.1.9
 **/
gboolean
cd_sensor_get_connected (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	return priv->proxy != NULL;
}

/**
 * cd_sensor_get_options:
 * @sensor: a #CdSensor instance.
 *
 * Gets any sensor options.
 *
 * Return value: (transfer container) (element-type utf8 GVariant): A
 *               refcounted #GHashTable of (string, GVariant).
 *
 * Since: 0.1.20
 **/
GHashTable *
cd_sensor_get_options (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	return g_hash_table_ref (priv->options);
}

/**
 * cd_sensor_get_option:
 * @sensor: a #CdSensor instance.
 * @key: a key to search for.
 *
 * Gets a specific sensor option.
 *
 * Return value: A const string, or %NULL of not found.
 *
 * Since: 0.1.20
 **/
const gchar *
cd_sensor_get_option (CdSensor *sensor, const gchar *key)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	return g_hash_table_lookup (priv->options, key);
}

/**
 * cd_sensor_get_metadata:
 * @sensor: a #CdSensor instance.
 *
 * Returns the sensor metadata.
 *
 * Return value: (transfer container) (element-type utf8 utf8): a
 *               #GHashTable.
 *
 * Since: 0.1.28
 **/
GHashTable *
cd_sensor_get_metadata (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return g_hash_table_ref (priv->metadata);
}

/**
 * cd_sensor_get_metadata_item:
 * @sensor: a #CdSensor instance.
 * @key: a key for the metadata dictionary
 *
 * Returns the sensor metadata for a specific key.
 *
 * Return value: the metadata value, or %NULL if not set.
 *
 * Since: 0.1.28
 **/
const gchar *
cd_sensor_get_metadata_item (CdSensor *sensor, const gchar *key)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return g_hash_table_lookup (priv->metadata, key);
}

/**
 * cd_sensor_equal:
 * @sensor1: one #CdSensor instance.
 * @sensor2: another #CdSensor instance.
 *
 * Tests two sensors for equality.
 *
 * Return value: %TRUE if the sensors are the same device
 *
 * Since: 0.1.8
 **/
gboolean
cd_sensor_equal (CdSensor *sensor1, CdSensor *sensor2)
{
	CdSensorPrivate *priv1 = GET_PRIVATE (sensor1);
	CdSensorPrivate *priv2 = GET_PRIVATE (sensor2);
	g_return_val_if_fail (CD_IS_SENSOR (sensor1), FALSE);
	g_return_val_if_fail (CD_IS_SENSOR (sensor2), FALSE);
	return g_strcmp0 (priv1->serial, priv2->serial) == 0;
}

/*
 * cd_sensor_set_property:
 */
static void
cd_sensor_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorPrivate *priv = GET_PRIVATE (sensor);

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
 * cd_sensor_get_property:
 */
static void
cd_sensor_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorPrivate *priv = GET_PRIVATE (sensor);

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_value_set_string (value, priv->object_path);
		break;
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_CONNECTED:
		g_value_set_boolean (value, priv->proxy != NULL);
		break;
	case PROP_KIND:
		g_value_set_uint (value, priv->kind);
		break;
	case PROP_STATE:
		g_value_set_uint (value, priv->state);
		break;
	case PROP_MODE:
		g_value_set_uint (value, priv->mode);
		break;
	case PROP_SERIAL:
		g_value_set_string (value, priv->serial);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	case PROP_VENDOR:
		g_value_set_string (value, priv->vendor);
		break;
	case PROP_NATIVE:
		g_value_set_boolean (value, priv->native);
		break;
	case PROP_EMBEDDED:
		g_value_set_boolean (value, priv->embedded);
		break;
	case PROP_LOCKED:
		g_value_set_boolean (value, priv->locked);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
    }
}

/*
 * cd_sensor_class_init:
 */
static void
cd_sensor_class_init (CdSensorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_sensor_finalize;
	object_class->set_property = cd_sensor_set_property;
	object_class->get_property = cd_sensor_get_property;

	/**
	 * CdSensor::button-pressed:
	 * @sensor: the #CdSensor instance that emitted the signal
	 *
	 * The ::button-pressed signal is emitted when the button has been pressed.
	 *
	 * Since: 0.1.6
	 **/
	signals [SIGNAL_BUTTON_PRESSED] =
		g_signal_new ("button-pressed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdSensorClass, button_pressed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/**
	 * CdSensor:object-path:
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
	 * CdSensor:id:
	 *
	 * The object ID of the remote object
	 *
	 * Since: 0.1.26
	 **/
	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_string ("id",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
	/**
	 * CdSensor:connected:
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
	 * CdSensor:kind:
	 *
	 * The sensor kind.
	 *
	 * Since: 0.1.6
	 **/
	g_object_class_install_property (object_class,
					 PROP_KIND,
					 g_param_spec_string ("kind",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdSensor:state:
	 *
	 * The sensor state.
	 *
	 * Since: 0.1.6
	 **/
	g_object_class_install_property (object_class,
					 PROP_STATE,
					 g_param_spec_string ("state",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdSensor:mode:
	 *
	 * The sensor mode.
	 *
	 * Since: 0.1.6
	 **/
	g_object_class_install_property (object_class,
					 PROP_MODE,
					 g_param_spec_string ("mode",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdSensor:serial:
	 *
	 * The sensor ID.
	 *
	 * Since: 0.1.6
	 **/
	g_object_class_install_property (object_class,
					 PROP_SERIAL,
					 g_param_spec_string ("serial",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
	/**
	 * CdSensor:model:
	 *
	 * The sensor model.
	 *
	 * Since: 0.1.6
	 **/
	g_object_class_install_property (object_class,
					 PROP_MODEL,
					 g_param_spec_string ("model",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
	/**
	 * CdSensor:vendor:
	 *
	 * The sensor vendor.
	 *
	 * Since: 0.1.6
	 **/
	g_object_class_install_property (object_class,
					 PROP_VENDOR,
					 g_param_spec_string ("vendor",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdSensor:native:
	 *
	 * If the sensor has a native driver.
	 *
	 * Since: 0.1.6
	 **/
	g_object_class_install_property (object_class,
					 PROP_NATIVE,
					 g_param_spec_string ("native",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdSensor:embedded:
	 *
	 * If the sensor has a native driver.
	 *
	 * Since: 0.1.26
	 **/
	g_object_class_install_property (object_class,
					 PROP_EMBEDDED,
					 g_param_spec_string ("embedded",
							      NULL, NULL,
							      FALSE,
							      G_PARAM_READABLE));

	/**
	 * CdSensor:locked:
	 *
	 * If the sensor is locked.
	 *
	 * Since: 0.1.6
	 **/
	g_object_class_install_property (object_class,
					 PROP_LOCKED,
					 g_param_spec_string ("locked",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
}

/*
 * cd_sensor_init:
 */
static void
cd_sensor_init (CdSensor *sensor)
{
	CdSensorPrivate *priv = GET_PRIVATE (sensor);
	priv->options = g_hash_table_new_full (g_str_hash,
						       g_str_equal,
						       (GDestroyNotify) g_free,
						       (GDestroyNotify) g_variant_unref);
	priv->metadata = g_hash_table_new_full (g_str_hash,
							g_str_equal,
							g_free,
							g_free);
}

/*
 * cd_sensor_finalize:
 */
static void
cd_sensor_finalize (GObject *object)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorPrivate *priv = GET_PRIVATE (sensor);

	g_return_if_fail (CD_IS_SENSOR (object));

	g_free (priv->object_path);
	g_free (priv->id);
	g_free (priv->serial);
	g_free (priv->model);
	g_free (priv->vendor);
	g_hash_table_unref (priv->options);
	g_hash_table_destroy (priv->metadata);
	if (priv->proxy != NULL)
		g_object_unref (priv->proxy);

	G_OBJECT_CLASS (cd_sensor_parent_class)->finalize (object);
}

/**
 * cd_sensor_new:
 *
 * Creates a new #CdSensor object.
 *
 * Return value: a new CdSensor object.
 *
 * Since: 0.1.6
 **/
CdSensor *
cd_sensor_new (void)
{
	CdSensor *sensor;
	sensor = g_object_new (CD_TYPE_SENSOR, NULL);
	return CD_SENSOR (sensor);
}

/**
 * cd_sensor_new_with_object_path:
 * @object_path: The colord object path.
 *
 * Creates a new #CdSensor object with a known object path.
 *
 * Return value: a new sensor object.
 *
 * Since: 0.1.8
 **/
CdSensor *
cd_sensor_new_with_object_path (const gchar *object_path)
{
	CdSensor *sensor;
	sensor = g_object_new (CD_TYPE_SENSOR,
				"object-path", object_path,
				NULL);
	return CD_SENSOR (sensor);
}
