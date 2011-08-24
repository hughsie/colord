/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

#define CD_SENSOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_SENSOR, CdSensorPrivate))

#define COLORD_DBUS_SERVICE		"org.freedesktop.ColorManager"
#define COLORD_DBUS_INTERFACE_SENSOR	"org.freedesktop.ColorManager.Sensor"

/**
 * CdSensorPrivate:
 *
 * Private #CdSensor data
 **/
struct _CdSensorPrivate
{
	gchar			*object_path;
	CdSensorKind		 kind;
	CdSensorState		 state;
	CdSensorCap		 mode;
	gchar			*serial;
	gchar			*model;
	gchar			*vendor;
	gboolean		 native;
	gboolean		 locked;
	guint			 caps;
	GDBusProxy		*proxy;
};

enum {
	PROP_0,
	PROP_OBJECT_PATH,
	PROP_CONNECTED,
	PROP_KIND,
	PROP_STATE,
	PROP_MODE,
	PROP_SERIAL,
	PROP_MODEL,
	PROP_VENDOR,
	PROP_NATIVE,
	PROP_LOCKED,
	PROP_LAST
};

enum {
	SIGNAL_BUTTON_PRESSED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (CdSensor, cd_sensor, G_TYPE_OBJECT)

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
	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_return_if_fail (sensor->priv->object_path == NULL);
	sensor->priv->object_path = g_strdup (object_path);
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
	g_return_val_if_fail (CD_IS_SENSOR (sensor), CD_SENSOR_KIND_UNKNOWN);
	g_return_val_if_fail (sensor->priv->proxy != NULL, CD_SENSOR_KIND_UNKNOWN);
	return sensor->priv->kind;
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
	g_return_val_if_fail (CD_IS_SENSOR (sensor), CD_SENSOR_STATE_UNKNOWN);
	g_return_val_if_fail (sensor->priv->proxy != NULL, CD_SENSOR_STATE_UNKNOWN);
	return sensor->priv->state;
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
	g_return_val_if_fail (CD_IS_SENSOR (sensor), CD_SENSOR_CAP_UNKNOWN);
	g_return_val_if_fail (sensor->priv->proxy != NULL, CD_SENSOR_CAP_UNKNOWN);
	return sensor->priv->mode;
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
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	g_return_val_if_fail (sensor->priv->proxy != NULL, NULL);
	return sensor->priv->serial;
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
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	g_return_val_if_fail (sensor->priv->proxy != NULL, NULL);
	return sensor->priv->model;
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
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	g_return_val_if_fail (sensor->priv->proxy != NULL, NULL);
	return sensor->priv->vendor;
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
	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (sensor->priv->proxy != NULL, FALSE);
	return sensor->priv->native;
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
	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (sensor->priv->proxy != NULL, FALSE);
	return sensor->priv->locked;
}

/**
 * cd_sensor_get_caps:
 * @sensor: a #CdSensor instance.
 *
 * Returns the sensor metadata.
 *
 * Return value: a #GHashTable, free with g_hash_table_unref().
 *
 * Since: 0.1.6
 **/
guint
cd_sensor_get_caps (CdSensor *sensor)
{
	g_return_val_if_fail (CD_IS_SENSOR (sensor), 0);
	g_return_val_if_fail (sensor->priv->proxy != NULL, 0);
	return sensor->priv->caps;
}

/**
 * cd_sensor_get_caps_item:
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
	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (sensor->priv->proxy != NULL, FALSE);
	return (sensor->priv->caps & (1 << cap)) > 0;
}

/**
 * cd_sensor_set_caps_from_variant:
 **/
static void
cd_sensor_set_caps_from_variant (CdSensor *sensor, GVariant *variant)
{
	const gchar **caps_tmp;
	guint i;
	guint value;

	/* remove old entries */
	sensor->priv->caps = 0;

	/* insert the new metadata */
	caps_tmp = g_variant_get_strv (variant, NULL);
	for (i=0; caps_tmp[i] != NULL; i++) {
		value = 1 << cd_sensor_cap_from_string (caps_tmp[i]);
		sensor->priv->caps += value;
	}
}

/**
 * cd_sensor_dbus_properties_changed_cb:
 **/
static void
cd_sensor_dbus_properties_changed_cb (GDBusProxy  *proxy,
				   GVariant    *changed_properties,
				   const gchar * const *invalidated_properties,
				   CdSensor   *sensor)
{
	guint i;
	guint len;
	GVariantIter iter;
	gchar *property_name;
	GVariant *property_value;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	len = g_variant_iter_init (&iter, changed_properties);
	for (i=0; i < len; i++) {
		g_variant_get_child (changed_properties, i,
				     "{sv}",
				     &property_name,
				     &property_value);
		if (g_strcmp0 (property_name, "Kind") == 0) {
			sensor->priv->kind = cd_sensor_kind_from_string (g_variant_get_string (property_value, NULL));
			g_object_notify (G_OBJECT (sensor), "kind");
		} else if (g_strcmp0 (property_name, "State") == 0) {
			sensor->priv->state = cd_sensor_state_from_string (g_variant_get_string (property_value, NULL));
			g_object_notify (G_OBJECT (sensor), "state");
		} else if (g_strcmp0 (property_name, "Mode") == 0) {
			sensor->priv->mode = cd_sensor_cap_from_string (g_variant_get_string (property_value, NULL));
			g_object_notify (G_OBJECT (sensor), "mode");
		} else if (g_strcmp0 (property_name, "Serial") == 0) {
			g_free (sensor->priv->serial);
			sensor->priv->serial = g_variant_dup_string (property_value, NULL);
			g_object_notify (G_OBJECT (sensor), "serial");
		} else if (g_strcmp0 (property_name, "Model") == 0) {
			g_free (sensor->priv->model);
			sensor->priv->model = g_variant_dup_string (property_value, NULL);
			g_object_notify (G_OBJECT (sensor), "model");
		} else if (g_strcmp0 (property_name, "Vendor") == 0) {
			g_free (sensor->priv->vendor);
			sensor->priv->vendor = g_variant_dup_string (property_value, NULL);
			g_object_notify (G_OBJECT (sensor), "vendor");
		} else if (g_strcmp0 (property_name, "Native") == 0) {
			sensor->priv->native = g_variant_get_boolean (property_value);
			g_object_notify (G_OBJECT (sensor), "native");
		} else if (g_strcmp0 (property_name, "Locked") == 0) {
			sensor->priv->locked = g_variant_get_boolean (property_value);
			g_object_notify (G_OBJECT (sensor), "locked");
		} else if (g_strcmp0 (property_name, "Capabilities") == 0) {
			cd_sensor_set_caps_from_variant (sensor, property_value);
			g_object_notify (G_OBJECT (sensor), "capabilities");
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
	GVariant *model = NULL;
	GVariant *serial = NULL;
	GVariant *vendor = NULL;
	GVariant *kind = NULL;
	GVariant *state = NULL;
	GVariant *mode = NULL;
	GVariant *native = NULL;
	GVariant *locked = NULL;
	GVariant *caps = NULL;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);
	CdSensor *sensor = CD_SENSOR (g_async_result_get_source_object (G_ASYNC_RESULT (user_data)));
	GError *error = NULL;

	/* get result */
	sensor->priv->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (sensor->priv->proxy == NULL) {
		g_simple_async_result_set_from_error (res_source,
						      error);
		g_simple_async_result_complete (res_source);
		g_error_free (error);
		goto out;
	}

	/* get kind */
	kind = g_dbus_proxy_get_cached_property (sensor->priv->proxy,
						 "Kind");
	if (kind != NULL)
		sensor->priv->kind = cd_sensor_kind_from_string (g_variant_get_string (kind, NULL));

	/* get state */
	state = g_dbus_proxy_get_cached_property (sensor->priv->proxy,
						  "State");
	if (state != NULL)
		sensor->priv->state = cd_colorspace_from_string (g_variant_get_string (state, NULL));

	/* get mode */
	mode = g_dbus_proxy_get_cached_property (sensor->priv->proxy,
						 "Mode");
	if (mode != NULL)
		sensor->priv->mode = cd_sensor_cap_from_string (g_variant_get_string (state, NULL));

	/* get sensor serial */
	serial = g_dbus_proxy_get_cached_property (sensor->priv->proxy,
						   "Serial");
	if (serial != NULL)
		sensor->priv->serial = g_variant_dup_string (serial, NULL);

	/* get vendor */
	vendor = g_dbus_proxy_get_cached_property (sensor->priv->proxy,
						   "Vendor");
	if (vendor != NULL)
		sensor->priv->vendor = g_variant_dup_string (vendor, NULL);

	/* get vendor */
	model = g_dbus_proxy_get_cached_property (sensor->priv->proxy,
						  "Model");
	if (model != NULL)
		sensor->priv->model = g_variant_dup_string (model, NULL);

	/* get native */
	native = g_dbus_proxy_get_cached_property (sensor->priv->proxy,
						   "Native");
	if (native != NULL)
		sensor->priv->native = g_variant_get_boolean (native);

	/* get locked */
	locked = g_dbus_proxy_get_cached_property (sensor->priv->proxy,
						   "Locked");
	if (locked != NULL)
		sensor->priv->locked = g_variant_get_boolean (locked);

	/* get if system wide */
	caps = g_dbus_proxy_get_cached_property (sensor->priv->proxy,
						 "Capabilities");
	if (caps != NULL)
		cd_sensor_set_caps_from_variant (sensor, caps);

	/* get signals from DBus */
	g_signal_connect (sensor->priv->proxy,
			  "g-signal",
			  G_CALLBACK (cd_sensor_dbus_signal_cb),
			  sensor);

	/* watch if any remote properties change */
	g_signal_connect (sensor->priv->proxy,
			  "g-properties-changed",
			  G_CALLBACK (cd_sensor_dbus_properties_changed_cb),
			  sensor);

	/* we're done */
	g_simple_async_result_complete (res_source);
out:
	if (kind != NULL)
		g_variant_unref (kind);
	if (state != NULL)
		g_variant_unref (state);
	if (mode != NULL)
		g_variant_unref (mode);
	if (serial != NULL)
		g_variant_unref (serial);
	if (model != NULL)
		g_variant_unref (model);
	if (vendor != NULL)
		g_variant_unref (vendor);
	if (native != NULL)
		g_variant_unref (native);
	if (locked != NULL)
		g_variant_unref (locked);
	if (caps != NULL)
		g_variant_unref (caps);
	g_object_unref (res_source);
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
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_return_if_fail (callback != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	res = g_simple_async_result_new (G_OBJECT (sensor),
					 callback,
					 user_data,
					 cd_sensor_connect);

	/* already connected */
	if (sensor->priv->proxy != NULL) {
		g_simple_async_result_set_op_res_gboolean (res, TRUE);
		g_simple_async_result_complete_in_idle (res);
		return;
	}

	/* connect async */
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  COLORD_DBUS_SERVICE,
				  sensor->priv->object_path,
				  COLORD_DBUS_INTERFACE_SENSOR,
				  cancellable,
				  cd_sensor_connect_cb,
				  res);
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
	gpointer source_tag;
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	source_tag = g_simple_async_result_get_source_tag (simple);

	g_return_val_if_fail (source_tag == cd_sensor_connect, FALSE);

	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return TRUE;
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
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_simple_async_result_get_op_res_gboolean (simple);
}

static void
cd_sensor_lock_cb (GObject *source_object,
		   GAsyncResult *res,
		   gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_FAILED,
						 "Failed to Lock: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	g_simple_async_result_set_op_res_gboolean (res_source, TRUE);
	g_variant_unref (result);
out:
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
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
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (sensor->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (sensor),
					 callback,
					 user_data,
					 cd_sensor_lock);
	g_dbus_proxy_call (sensor->priv->proxy,
			   "Lock",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_sensor_lock_cb,
			   res);
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
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_simple_async_result_get_op_res_gboolean (simple);
}

static void
cd_sensor_unlock_cb (GObject *source_object,
		     GAsyncResult *res,
		     gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_FAILED,
						 "Failed to Unlock: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	g_simple_async_result_set_op_res_gboolean (res_source, TRUE);
	g_variant_unref (result);
out:
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
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
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (sensor->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (sensor),
					 callback,
					 user_data,
					 cd_sensor_unlock);
	g_dbus_proxy_call (sensor->priv->proxy,
			   "Unlock",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_sensor_unlock_cb,
			   res);
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
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	return cd_color_xyz_dup (g_simple_async_result_get_op_res_gpointer (simple));
}

static void
cd_sensor_get_sample_cb (GObject *source_object,
			 GAsyncResult *res,
			 gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	CdColorXYZ *xyz;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_FAILED,
						 "Failed to GetSample: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	xyz = cd_color_xyz_new ();
	g_variant_get (result,
		       "(ddd)",
		       &xyz->X,
		       &xyz->Y,
		       &xyz->Z);

	g_simple_async_result_set_op_res_gpointer (res_source,
						   xyz,
						   (GDestroyNotify) cd_color_xyz_free);
	g_variant_unref (result);
out:
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
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
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (sensor->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (sensor),
					 callback,
					 user_data,
					 cd_sensor_get_sample);
	g_dbus_proxy_call (sensor->priv->proxy,
			   "GetSample",
			   g_variant_new ("(s)",
			   		  cd_sensor_cap_to_string (cap)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_sensor_get_sample_cb,
			   res);
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
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	return sensor->priv->object_path;
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
	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	return sensor->priv->proxy != NULL;
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
	g_return_val_if_fail (CD_IS_SENSOR (sensor1), FALSE);
	g_return_val_if_fail (CD_IS_SENSOR (sensor2), FALSE);
	return g_strcmp0 (sensor1->priv->serial, sensor2->priv->serial) == 0;
}

/*
 * cd_sensor_set_property:
 */
static void
cd_sensor_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CdSensor *sensor = CD_SENSOR (object);

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_free (sensor->priv->object_path);
		sensor->priv->object_path = g_value_dup_string (value);
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

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_value_set_string (value, sensor->priv->object_path);
		break;
	case PROP_CONNECTED:
		g_value_set_boolean (value, sensor->priv->proxy != NULL);
		break;
	case PROP_KIND:
		g_value_set_uint (value, sensor->priv->kind);
		break;
	case PROP_STATE:
		g_value_set_uint (value, sensor->priv->state);
		break;
	case PROP_MODE:
		g_value_set_uint (value, sensor->priv->mode);
		break;
	case PROP_SERIAL:
		g_value_set_string (value, sensor->priv->serial);
		break;
	case PROP_MODEL:
		g_value_set_string (value, sensor->priv->model);
		break;
	case PROP_VENDOR:
		g_value_set_string (value, sensor->priv->vendor);
		break;
	case PROP_NATIVE:
		g_value_set_boolean (value, sensor->priv->native);
		break;
	case PROP_LOCKED:
		g_value_set_boolean (value, sensor->priv->locked);
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

	g_type_class_add_private (klass, sizeof (CdSensorPrivate));
}

/*
 * cd_sensor_init:
 */
static void
cd_sensor_init (CdSensor *sensor)
{
	sensor->priv = CD_SENSOR_GET_PRIVATE (sensor);
}

/*
 * cd_sensor_finalize:
 */
static void
cd_sensor_finalize (GObject *object)
{
	CdSensor *sensor;
	guint ret;

	g_return_if_fail (CD_IS_SENSOR (object));

	sensor = CD_SENSOR (object);

	g_free (sensor->priv->object_path);
	g_free (sensor->priv->serial);
	g_free (sensor->priv->model);
	g_free (sensor->priv->vendor);
	if (sensor->priv->proxy != NULL) {
		ret = g_signal_handlers_disconnect_by_func (sensor->priv->proxy,
							    G_CALLBACK (cd_sensor_dbus_signal_cb),
							    sensor);
		g_assert (ret > 0);
		ret = g_signal_handlers_disconnect_by_func (sensor->priv->proxy,
							    G_CALLBACK (cd_sensor_dbus_properties_changed_cb),
							    sensor);
		g_assert (ret > 0);
		g_object_unref (sensor->priv->proxy);
		g_assert (!G_IS_DBUS_PROXY (sensor->priv->proxy));
	}

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
