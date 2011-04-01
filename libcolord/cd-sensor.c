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

/**
 * CdSensorPrivate:
 *
 * Private #PkSensor data
 **/
struct _CdSensorPrivate
{
	gchar			*object_path;
	CdSensorKind		 kind;
	CdSensorState		 state;
	gchar			*serial;
	gchar			*model;
	gchar			*vendor;
	gboolean		 native;
	guint			 caps;
	GDBusProxy		*proxy;
};

enum {
	PROP_0,
	PROP_KIND,
	PROP_STATE,
	PROP_SERIAL,
	PROP_MODEL,
	PROP_VENDOR,
	PROP_NATIVE,
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
	g_return_val_if_fail (CD_IS_SENSOR (sensor), 0);
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
	g_return_val_if_fail (CD_IS_SENSOR (sensor), 0);
	return sensor->priv->state;
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
	return sensor->priv->vendor;
}

/**
 * cd_sensor_get_native:
 * @sensor: a #CdSensor instance.
 *
 * Returns if the sensor has a VCGT table.
 *
 * Return value: %TRUE if VCGT is valid.
 *
 * Since: 0.1.6
 **/
gboolean
cd_sensor_get_native (CdSensor *sensor)
{
	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	return sensor->priv->native;
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
	return sensor->priv->caps;
}

/**
 * cd_sensor_get_caps_item:
 * @sensor: a #CdSensor instance.
 * @cap: a specified capability, e.g. %CD_SENSOR_CAP_DISPLAY
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
 * cd_sensor_dbus_properties_changed:
 **/
static void
cd_sensor_dbus_properties_changed (GDBusProxy  *proxy,
				   GVariant    *changed_properties,
				   const gchar * const *invalidated_properties,
				   CdSensor   *sensor)
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
		if (g_strcmp0 (property_name, "Kind") == 0) {
			sensor->priv->kind = cd_sensor_kind_from_string (g_variant_get_string (property_value, NULL));
			g_object_notify (G_OBJECT (sensor), "kind");
		} else if (g_strcmp0 (property_name, "State") == 0) {
			sensor->priv->state = cd_sensor_state_from_string (g_variant_get_string (property_value, NULL));
			g_object_notify (G_OBJECT (sensor), "state");
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
	if (g_strcmp0 (signal_name, "ButtonPressed") == 0) {
		g_debug ("emit ButtonPressed on %s", sensor->priv->object_path);
		g_signal_emit (sensor, signals[SIGNAL_BUTTON_PRESSED], 0);
	} else {
		g_warning ("unhandled signal '%s'", signal_name);
	}
}

/**
 * cd_sensor_set_object_path_sync:
 * @sensor: a #CdSensor instance.
 * @object_path: The colord object path.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the object path of the object and fills up initial properties.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.6
 **/
gboolean
cd_sensor_set_object_path_sync (CdSensor *sensor,
				 const gchar *object_path,
				 GCancellable *cancellable,
				 GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *model = NULL;
	GVariant *serial = NULL;
	GVariant *vendor = NULL;
	GVariant *kind = NULL;
	GVariant *state = NULL;
	GVariant *native = NULL;
	GVariant *caps = NULL;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (sensor->priv->proxy == NULL, FALSE);

	/* connect to the daemon */
	sensor->priv->proxy =
		g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					       G_DBUS_PROXY_FLAGS_NONE,
					       NULL,
					       COLORD_DBUS_SERVICE,
					       object_path,
					       COLORD_DBUS_INTERFACE_SENSOR,
					       cancellable,
					       &error_local);
	if (sensor->priv->proxy == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_FAILED,
			     "Failed to connect to sensor %s: %s",
			     object_path,
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* save object path */
	sensor->priv->object_path = g_strdup (object_path);

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
			  G_CALLBACK (cd_sensor_dbus_properties_changed),
			  sensor);

	/* success */
	g_debug ("Connected to sensor %s",
		 sensor->priv->serial);
out:
	if (kind != NULL)
		g_variant_unref (kind);
	if (state != NULL)
		g_variant_unref (state);
	if (serial != NULL)
		g_variant_unref (serial);
	if (model != NULL)
		g_variant_unref (model);
	if (vendor != NULL)
		g_variant_unref (vendor);
	if (native != NULL)
		g_variant_unref (native);
	if (caps != NULL)
		g_variant_unref (caps);
	return ret;
}

/**
 * cd_sensor_lock_sync:
 * @sensor: a #CdSensor instance.
 * @value: The model.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the sensor model.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.6
 **/
gboolean
cd_sensor_lock_sync (CdSensor *sensor,
		     GCancellable *cancellable,
		     GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *response = NULL;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (sensor->priv->proxy != NULL, FALSE);

	/* execute sync method */
	response = g_dbus_proxy_call_sync (sensor->priv->proxy,
					   "Lock",
					   NULL,
					   G_DBUS_CALL_FLAGS_NONE,
					   -1, NULL, &error_local);
	if (response == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_FAILED,
			     "Failed to set property: %s",
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
 * cd_sensor_get_sample_sync:
 * @sensor: a #CdSensor instance.
 * @value: The vendor.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Gets a sample from the sensor.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.6
 **/
gboolean
cd_sensor_get_sample_sync (CdSensor *sensor,
			   CdSensorCap cap,
			   gdouble **values,
			   gdouble *ambient,
			   GCancellable *cancellable,
			   GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *response = NULL;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (sensor->priv->proxy != NULL, FALSE);

	/* execute sync method */
	response = g_dbus_proxy_call_sync (sensor->priv->proxy,
					   "GetSample",
					   g_variant_new ("(s)",
							  cd_sensor_cap_to_string (cap)),
					   G_DBUS_CALL_FLAGS_NONE,
					   -1, NULL, &error_local);
	if (response == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_FAILED,
			     "Failed to set property: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* get the values */
	g_variant_get (response,
		       "((ddd)d)",
		       &values[0],
		       &values[1],
		       &values[2],
		       ambient);
out:
	if (response != NULL)
		g_variant_unref (response);
	return ret;
}

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

/*
 * cd_sensor_set_property:
 */
static void
cd_sensor_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
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
	case PROP_KIND:
		g_value_set_uint (value, sensor->priv->kind);
		break;
	case PROP_STATE:
		g_value_set_uint (value, sensor->priv->state);
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
							      G_PARAM_READWRITE));

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
							      G_PARAM_READWRITE));

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
							      G_PARAM_READWRITE));
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
							      G_PARAM_READWRITE));
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
							      G_PARAM_READWRITE));

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
							      G_PARAM_READWRITE));

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

	g_return_if_fail (CD_IS_SENSOR (object));

	sensor = CD_SENSOR (object);

	g_free (sensor->priv->object_path);
	g_free (sensor->priv->serial);
	g_free (sensor->priv->model);
	g_free (sensor->priv->vendor);
	if (sensor->priv->proxy != NULL)
		g_object_unref (sensor->priv->proxy);

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

