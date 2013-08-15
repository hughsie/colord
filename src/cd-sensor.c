/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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
#include <sys/time.h>
#include <gmodule.h>
#include <colord-private.h>

#include "cd-common.h"
#include "cd-sensor.h"

static void cd_sensor_finalize			 (GObject *object);

#define CD_SENSOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_SENSOR, CdSensorPrivate))

/**
 * CdSensorIface:
 */
typedef struct {
	void		 (*get_sample_async)	(CdSensor		*sensor,
						 CdSensorCap		 cap,
						 GCancellable		*cancellable,
						 GAsyncReadyCallback	 callback,
						 gpointer		 user_data);
	CdColorXYZ	*(*get_sample_finish)	(CdSensor		*sensor,
						 GAsyncResult		*res,
						 GError			**error);
	gboolean	 (*coldplug)		(CdSensor		*sensor,
						 GError			**error);
	gboolean	 (*dump_device)		(CdSensor		*sensor,
						 GString		*data,
						 GError			**error);
	void		 (*lock_async)		(CdSensor		*sensor,
						 GCancellable		*cancellable,
						 GAsyncReadyCallback	 callback,
						 gpointer		 user_data);
	gboolean	 (*lock_finish)		(CdSensor		*sensor,
						 GAsyncResult		*res,
						 GError			**error);
	void		 (*unlock_async)	(CdSensor		*sensor,
						 GCancellable		*cancellable,
						 GAsyncReadyCallback	 callback,
						 gpointer		 user_data);
	gboolean	 (*unlock_finish)	(CdSensor		*sensor,
						 GAsyncResult		*res,
						 GError			**error);
	void		 (*set_options_async)	(CdSensor		*sensor,
						 GHashTable		*options,
						 GCancellable		*cancellable,
						 GAsyncReadyCallback	 callback,
						 gpointer		 user_data);
	gboolean	 (*set_options_finish)	(CdSensor		*sensor,
						 GAsyncResult		*res,
						 GError			**error);
} CdSensorIface;

/**
 * CdSensorPrivate:
 *
 * Private #CdSensor data
 **/
struct _CdSensorPrivate
{
	gchar				*id;
	CdSensorKind			 kind;
	CdSensorState			 state;
	CdSensorCap			 mode;
	gchar				*serial;
	gchar				*model;
	gchar				*vendor;
#ifdef HAVE_GUDEV
	GUdevDevice			*device;
#endif
	gboolean			 native;
	gboolean			 embedded;
	gboolean			 locked;
	guint64				 caps;
	gchar				*object_path;
	guint				 watcher_id;
	GDBusConnection			*connection;
	guint				 registration_id;
	CdSensorIface			*desc;
	GHashTable			*options;
	GHashTable			*metadata;
#ifdef HAVE_GUSB
	GUsbContext			*usb_ctx;
	GUsbDeviceList			*device_list;
#endif
};

enum {
	PROP_0,
	PROP_OBJECT_PATH,
	PROP_ID,
	PROP_NATIVE,
	PROP_LOCKED,
	PROP_STATE,
	PROP_MODE,
	PROP_VENDOR,
	PROP_MODEL,
	PROP_SERIAL,
	PROP_KIND,
	PROP_CAPS,
	PROP_LAST
};

G_DEFINE_TYPE (CdSensor, cd_sensor, G_TYPE_OBJECT)

/**
 * cd_sensor_error_quark:
 **/
GQuark
cd_sensor_error_quark (void)
{
	guint i;
	static GQuark quark = 0;
	if (!quark) {
		quark = g_quark_from_static_string ("CdSensor");
		for (i = 0; i < CD_SENSOR_ERROR_LAST; i++) {
			g_dbus_error_register_error (quark,
						     i,
						     cd_sensor_error_to_string (i));
		}
	}
	return quark;
}

/**
 * cd_sensor_get_object_path:
 **/
const gchar *
cd_sensor_get_object_path (CdSensor *sensor)
{
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	return sensor->priv->object_path;
}

/**
 * cd_sensor_get_id:
 **/
const gchar *
cd_sensor_get_id (CdSensor *sensor)
{
	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	return sensor->priv->id;
}

/**
 * cd_sensor_set_id:
 **/
static void
cd_sensor_set_id (CdSensor *sensor, const gchar *id)
{
	gchar *id_tmp;

	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_free (sensor->priv->id);

	/* make sure object path is sane */
	id_tmp = cd_main_ensure_dbus_path (id);
	sensor->priv->object_path = g_build_filename (COLORD_DBUS_PATH,
						      "sensors",
						      id_tmp,
						      NULL);
	sensor->priv->id = g_strdup (id);
	g_free (id_tmp);
}

/**
 * cd_sensor_dbus_emit_property_changed:
 **/
static void
cd_sensor_dbus_emit_property_changed (CdSensor *sensor,
				      const gchar *property_name,
				      GVariant *property_value)
{
	GVariantBuilder builder;
	GVariantBuilder invalidated_builder;

	/* not yet connected */
	if (sensor->priv->connection == NULL)
		return;

	/* build the dict */
	g_variant_builder_init (&invalidated_builder, G_VARIANT_TYPE ("as"));
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add (&builder,
			       "{sv}",
			       property_name,
			       property_value);
	g_debug ("CdSensor: emit PropertiesChanged(%s)", property_name);
	g_dbus_connection_emit_signal (sensor->priv->connection,
				       NULL,
				       sensor->priv->object_path,
				       "org.freedesktop.DBus.Properties",
				       "PropertiesChanged",
				       g_variant_new ("(sa{sv}as)",
				       COLORD_DBUS_INTERFACE_SENSOR,
				       &builder,
				       &invalidated_builder),
				       NULL);
}

/**
 * cd_sensor_button_pressed:
 *
 * Causes the ::button-pressed signal to be fired.
 **/
void
cd_sensor_button_pressed (CdSensor *sensor)
{
	/* not yet connected */
	if (sensor->priv->connection == NULL)
		return;

	/* emit signal */
	g_debug ("CdSensor: emit ButtonPressed on %s",
		 sensor->priv->object_path);
	g_dbus_connection_emit_signal (sensor->priv->connection,
				       NULL,
				       sensor->priv->object_path,
				       COLORD_DBUS_INTERFACE_SENSOR,
				       "ButtonPressed",
				       NULL,
				       NULL);
}

/**
 * cd_sensor_set_serial:
 * @sensor: a valid #CdSensor instance
 * @serial_number: the serial number
 *
 * Sets the sensor serial number which can be used to uniquely identify
 * the device.
 **/
void
cd_sensor_set_serial (CdSensor *sensor, const gchar *serial)
{
	g_free (sensor->priv->serial);
	sensor->priv->serial = g_strdup (serial);
	cd_sensor_dbus_emit_property_changed (sensor,
					      "Serial",
					      g_variant_new_string (serial));
}

/**
 * cd_sensor_set_kind:
 * @sensor: a valid #CdSensor instance
 * @kind: the sensor kind, e.g %CD_SENSOR_KIND_HUEY
 *
 * Sets the device kind.
 **/
void
cd_sensor_set_kind (CdSensor *sensor, CdSensorKind kind)
{
	sensor->priv->kind = kind;
	cd_sensor_dbus_emit_property_changed (sensor,
					      "Kind",
					      g_variant_new_uint32 (kind));
}

/**
 * cd_sensor_get_kind:
 **/
CdSensorKind
cd_sensor_get_kind (CdSensor *sensor)
{
	return sensor->priv->kind;
}

/**
 * cd_sensor_load:
 * @sensor: a valid #CdSensor instance
 * @kind: the sensor kind, e.g %CD_SENSOR_KIND_HUEY
 *
 * Sets the device kind.
 **/
gboolean
cd_sensor_load (CdSensor *sensor, GError **error)
{
	CdSensorIface *desc;
	gboolean ret = FALSE;
	gchar *backend_name = NULL;
	gchar *path = NULL;
	gchar *path_fallback = NULL;
	GModule *handle;

	/* no module */
	if (sensor->priv->kind == CD_SENSOR_KIND_UNKNOWN) {
		ret = TRUE;
		goto out;
	}

	/* can we load a module? */
	backend_name = g_strdup_printf ("libcolord_sensor_%s." G_MODULE_SUFFIX,
					cd_sensor_kind_to_string (sensor->priv->kind));
	path = g_build_filename (LIBDIR, "colord-sensors", backend_name, NULL);
	g_debug ("Trying to load sensor driver: %s", path);
	handle = g_module_open (path, G_MODULE_BIND_LOCAL);
	if (handle == NULL) {
		g_debug ("opening module %s failed : %s",
			 backend_name, g_module_error ());
		g_debug ("Trying to fall back to : libcolord_sensor_argyll");
		path_fallback = g_build_filename (LIBDIR,
						  "colord-sensors",
						  "libcolord_sensor_argyll.so",
						  NULL);
		handle = g_module_open (path_fallback, G_MODULE_BIND_LOCAL);
	}
	if (handle == NULL) {
		g_set_error (error, 1, 0,
			     "opening module %s (and fallback) failed : %s",
			     backend_name, g_module_error ());
		goto out;
	}

	/* dlload module if it exists */
	desc = sensor->priv->desc = g_new0 (CdSensorIface, 1);

	/* connect up exported methods */
	g_module_symbol (handle, "cd_sensor_get_sample_async", (gpointer *)&desc->get_sample_async);
	g_module_symbol (handle, "cd_sensor_get_sample_finish", (gpointer *)&desc->get_sample_finish);
	g_module_symbol (handle, "cd_sensor_set_options_async", (gpointer *)&desc->set_options_async);
	g_module_symbol (handle, "cd_sensor_set_options_finish", (gpointer *)&desc->set_options_finish);
	g_module_symbol (handle, "cd_sensor_coldplug", (gpointer *)&desc->coldplug);
	g_module_symbol (handle, "cd_sensor_dump_device", (gpointer *)&desc->dump_device);
	g_module_symbol (handle, "cd_sensor_lock_async", (gpointer *)&desc->lock_async);
	g_module_symbol (handle, "cd_sensor_lock_finish", (gpointer *)&desc->lock_finish);
	g_module_symbol (handle, "cd_sensor_unlock_async", (gpointer *)&desc->unlock_async);
	g_module_symbol (handle, "cd_sensor_unlock_finish", (gpointer *)&desc->unlock_finish);

	/* coldplug with data */
	if (desc->coldplug != NULL) {
		ret = desc->coldplug (sensor, error);
		if (!ret)
			goto out;
	}
out:
//		g_module_close (handle);
	g_free (backend_name);
	g_free (path);
	g_free (path_fallback);
	return ret;
}

/**
 * cd_sensor_set_locked:
 **/
static void
cd_sensor_set_locked (CdSensor *sensor, gboolean locked)
{
	sensor->priv->locked = locked;
	cd_sensor_dbus_emit_property_changed (sensor,
					      "Locked",
					      g_variant_new_boolean (locked));
}

/**
 * _cd_sensor_lock_async:
 *
 * Lock the sensor. You don't ever need to call this, and this method
 * should only be used from the internal self check program.
 **/
void
_cd_sensor_lock_async (CdSensor *sensor,
		       GCancellable *cancellable,
		       GAsyncReadyCallback callback,
		       gpointer user_data)
{
	g_return_if_fail (CD_IS_SENSOR (sensor));
	g_return_if_fail (sensor->priv->desc != NULL);

	/* proxy up */
	if (sensor->priv->desc->lock_async) {
		sensor->priv->desc->lock_async (sensor,
						cancellable,
						callback,
						user_data);
	}
}

/**
 * _cd_sensor_lock_finish:
 *
 * Finish locking the sensor. You don't ever need to call this, and this
 * method should only be used from the internal self check program.
 **/
gboolean
_cd_sensor_lock_finish (CdSensor *sensor,
			GAsyncResult *res,
			GError **error)
{
	gboolean ret = TRUE;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (sensor->priv->desc != NULL, FALSE);

	/* proxy up */
	if (sensor->priv->desc->lock_finish) {
		ret = sensor->priv->desc->lock_finish (sensor,
						       res,
						       error);
	}
	cd_sensor_set_locked (sensor, TRUE);
	return ret;
}

/**
 * cd_sensor_set_state:
 * @sensor: a valid #CdSensor instance
 * @state: the sensor state, e.g %CD_SENSOR_STATE_IDLE
 *
 * Sets the device state.
 **/
void
cd_sensor_set_state (CdSensor *sensor, CdSensorState state)
{
	sensor->priv->state = state;
	cd_sensor_dbus_emit_property_changed (sensor,
					      "State",
					      g_variant_new_string (cd_sensor_state_to_string (state)));
}

/**
 * cd_sensor_set_mode:
 **/
void
cd_sensor_set_mode (CdSensor *sensor, CdSensorCap mode)
{
	sensor->priv->mode = mode;
	cd_sensor_dbus_emit_property_changed (sensor,
					      "Mode",
					      g_variant_new_string (cd_sensor_cap_to_string (mode)));
}

/**
 * cd_sensor_get_mode:
 **/
CdSensorCap
cd_sensor_get_mode (CdSensor *sensor)
{
	return sensor->priv->mode;
}

/**
 * cd_sensor_dump:
 * @sensor: a valid #CdSensor instance
 * @data: A valid #GString for the returned data
 * @error: a #GError or %NULL
 *
 * Dumps the unstructured device data to a string.
 *
 * Return value: %TRUE for success.
 **/
gboolean
cd_sensor_dump (CdSensor *sensor, GString *data, GError **error)
{
	CdSensorPrivate *priv = sensor->priv;
	gboolean ret = TRUE;

	/* write common sensor details */
	g_string_append (data, "// AUTOMATICALLY GENERATED -- DO NOT EDIT\n");
	g_string_append_printf (data, "generic-dump-version:%i\n", 1);
	g_string_append_printf (data, "kind:%s\n", cd_sensor_kind_to_string (priv->kind));
	g_string_append_printf (data, "vendor:%s\n", priv->vendor);
	g_string_append_printf (data, "model:%s\n", priv->model);
	g_string_append_printf (data, "serial-number:%s\n", priv->serial);

	/* no type */
	if (sensor->priv->desc == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_INTERNAL,
				     "need to load sensor! [cd_sensor_load]");
		goto out;
	}

	/* dump sensor */
	if (sensor->priv->desc->dump_device == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = sensor->priv->desc->dump_device (sensor, data, error);
out:
	return ret;
}

/**
 * cd_sensor_get_sample_cb:
 **/
static void
cd_sensor_get_sample_cb (GObject *source_object,
			 GAsyncResult *res,
			 gpointer user_data)
{
	GVariant *result = NULL;
	CdColorXYZ *sample;
	CdSensor *sensor = CD_SENSOR (source_object);
	GDBusMethodInvocation *invocation = (GDBusMethodInvocation *) user_data;
	GError *error = NULL;

	/* get the result */
	sample = sensor->priv->desc->get_sample_finish (sensor, res, &error);
	if (sample == NULL) {
		g_dbus_method_invocation_return_gerror (invocation,
							error);
		g_error_free (error);
		goto out;
	}

	/* return value */
	g_debug ("returning value %f, %f, %f",
		 sample->X,
		 sample->Y,
		 sample->Z);
	result = g_variant_new ("(ddd)",
				sample->X,
				sample->Y,
				sample->Z);
	g_dbus_method_invocation_return_value (invocation, result);
out:
	if (sample != NULL)
		cd_color_xyz_free (sample);
}

/**
 * cd_sensor_set_options_cb:
 **/
static void
cd_sensor_set_options_cb (GObject *source_object,
			  GAsyncResult *res,
			  gpointer user_data)
{
	gboolean ret;
	CdSensor *sensor = CD_SENSOR (source_object);
	GDBusMethodInvocation *invocation = (GDBusMethodInvocation *) user_data;
	GError *error = NULL;

	/* get the result */
	ret = sensor->priv->desc->set_options_finish (sensor, res, &error);
	if (!ret) {
		g_dbus_method_invocation_return_gerror (invocation,
							error);
		g_error_free (error);
		goto out;
	}
	g_dbus_method_invocation_return_value (invocation, NULL);
out:
	return;
}

/**
 * cd_sensor_lock_cb:
 **/
static void
cd_sensor_lock_cb (GObject *source_object,
		   GAsyncResult *res,
		   gpointer user_data)
{
	CdSensor *sensor = CD_SENSOR (source_object);
	gboolean ret;
	GDBusMethodInvocation *invocation = (GDBusMethodInvocation *) user_data;
	GError *error = NULL;

	/* get the result */
	ret = sensor->priv->desc->lock_finish (sensor, res, &error);
	if (!ret) {
		g_dbus_method_invocation_return_error (invocation,
						       CD_SENSOR_ERROR,
						       CD_SENSOR_ERROR_NO_SUPPORT,
						       "failed to lock: %s",
						       error->message);
		g_error_free (error);
		goto out;
	}
	cd_sensor_set_locked (sensor, TRUE);
	g_dbus_method_invocation_return_value (invocation, NULL);
out:
	return;
}

/**
 * cd_sensor_unlock_cb:
 **/
static void
cd_sensor_unlock_cb (GObject *source_object,
		     GAsyncResult *res,
		     gpointer user_data)
{
	CdSensor *sensor = CD_SENSOR (source_object);
	gboolean ret;
	GDBusMethodInvocation *invocation = (GDBusMethodInvocation *) user_data;
	GError *error = NULL;

	/* get the result */
	if (sensor->priv->desc != NULL &&
	    sensor->priv->desc->unlock_finish != NULL) {
		ret = sensor->priv->desc->unlock_finish (sensor, res, &error);
		if (!ret) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SENSOR_ERROR,
							       CD_SENSOR_ERROR_NO_SUPPORT,
							       "failed to unlock: %s",
							       error->message);
			g_error_free (error);
			goto out;
		}
	}
	cd_sensor_set_locked (sensor, FALSE);
	g_dbus_method_invocation_return_value (invocation, NULL);
out:
	return;
}

/**
 * cd_sensor_unlock_quietly_cb:
 **/
static void
cd_sensor_unlock_quietly_cb (GObject *source_object,
			     GAsyncResult *res,
			     gpointer user_data)
{
	CdSensor *sensor = CD_SENSOR (source_object);
	gboolean ret;
	GError *error = NULL;

	/* get the result */
	if (sensor->priv->desc != NULL &&
	    sensor->priv->desc->unlock_finish != NULL) {
		ret = sensor->priv->desc->unlock_finish (sensor, res, &error);
		if (!ret) {
			g_warning ("failed to unlock: %s",
				   error->message);
			g_error_free (error);
			goto out;
		}
	}
	cd_sensor_set_locked (sensor, FALSE);
out:
	return;
}

/**
 * cd_sensor_name_vanished_cb:
 **/
static void
cd_sensor_name_vanished_cb (GDBusConnection *connection,
			     const gchar *name,
			     gpointer user_data)
{
	CdSensor *sensor = CD_SENSOR (user_data);

	/* dummy */
	g_debug ("locked sender has vanished without doing Unlock()!");
	if (sensor->priv->desc == NULL ||
	    sensor->priv->desc->unlock_async == NULL) {
		cd_sensor_set_locked (sensor, FALSE);
		goto out;
	}

	/* no longer valid */
	sensor->priv->desc->unlock_async (sensor,
					  NULL,
					  cd_sensor_unlock_quietly_cb,
					  NULL);
out:
	sensor->priv->watcher_id = 0;
}

/**
 * cd_sensor_dbus_method_call:
 **/
static void
cd_sensor_dbus_method_call (GDBusConnection *connection, const gchar *sender,
			    const gchar *object_path, const gchar *interface_name,
			    const gchar *method_name, GVariant *parameters,
			    GDBusMethodInvocation *invocation, gpointer user_data)
{
	CdSensorCap cap;
	CdSensor *sensor = CD_SENSOR (user_data);
	CdSensorPrivate *priv = sensor->priv;
	const gchar *cap_tmp = NULL;
	gboolean ret;
	gchar *key;
	GError *error = NULL;
	GHashTable *options = NULL;
	GVariantIter iter;
	GVariant *result = NULL;
	GVariant *value;

	/* return '' */
	if (g_strcmp0 (method_name, "Lock") == 0) {

		g_debug ("CdSensor %s:Lock()", sender);

		/* check locked */
		if (priv->locked) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SENSOR_ERROR,
							       CD_SENSOR_ERROR_ALREADY_LOCKED,
							       "sensor is already locked");
			goto out;
		}

		/* require auth */
		ret = cd_main_sender_authenticated (connection,
						    sender,
						    "org.freedesktop.color-manager.sensor-lock",
						    &error);
		if (!ret) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SENSOR_ERROR,
							       CD_SENSOR_ERROR_FAILED_TO_AUTHENTICATE,
							       "%s", error->message);
			g_error_free (error);
			goto out;
		}

		/* watch this bus name */
		priv->watcher_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
						     sender,
						     G_BUS_NAME_WATCHER_FLAGS_NONE,
						     NULL,
						     cd_sensor_name_vanished_cb,
						     sensor,
						     NULL);

		/* no support */
		if (sensor->priv->desc == NULL ||
		    sensor->priv->desc->lock_async == NULL) {
			cd_sensor_set_locked (sensor, TRUE);
			g_dbus_method_invocation_return_value (invocation, NULL);
			goto out;
		}

		/* proxy */
		sensor->priv->desc->lock_async (sensor,
						NULL,
						cd_sensor_lock_cb,
						invocation);
		goto out;
	}

	if (g_strcmp0 (method_name, "Unlock") == 0) {

		g_debug ("CdSensor %s:Unlock()", sender);

		/* check locked */
		if (!priv->locked) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SENSOR_ERROR,
							       CD_SENSOR_ERROR_NOT_LOCKED,
							       "sensor is not yet locked");
			goto out;
		}

		/* require auth */
		ret = cd_main_sender_authenticated (connection,
						    sender,
						    "org.freedesktop.color-manager.sensor-lock",
						    &error);
		if (!ret) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SENSOR_ERROR,
							       CD_SENSOR_ERROR_FAILED_TO_AUTHENTICATE,
							       "%s", error->message);
			g_error_free (error);
			goto out;
		}

		/* un-watch this bus name */
		if (priv->watcher_id != 0) {
			g_bus_unwatch_name (priv->watcher_id);
			priv->watcher_id = 0;
		}

		/* no support */
		if (sensor->priv->desc == NULL ||
		    sensor->priv->desc->unlock_async == NULL) {
			cd_sensor_set_locked (sensor, FALSE);
			g_dbus_method_invocation_return_value (invocation, NULL);
			goto out;
		}

		/* proxy */
		sensor->priv->desc->unlock_async (sensor,
				     NULL,
				     cd_sensor_unlock_cb,
				     invocation);
		goto out;
	}

	/* return 'ddd,d' */
	if (g_strcmp0 (method_name, "GetSample") == 0) {

		g_debug ("CdSensor %s:GetSample()", sender);

		/* check locked */
		if (!priv->locked) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SENSOR_ERROR,
							       CD_SENSOR_ERROR_NOT_LOCKED,
							       "sensor is not yet locked");
			goto out;
		}

		/*  check idle */
		if (priv->state != CD_SENSOR_STATE_IDLE) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SENSOR_ERROR,
							       CD_SENSOR_ERROR_IN_USE,
							       "sensor not idle: %s",
							       cd_sensor_state_to_string (priv->state));
			goto out;
		}

		/* no support */
		if (sensor->priv->desc == NULL ||
		    sensor->priv->desc->get_sample_async == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SENSOR_ERROR,
							       CD_SENSOR_ERROR_NO_SUPPORT,
							       "no sensor->get_sample");
			goto out;
		}

		/* get the type */
		g_variant_get (parameters, "(&s)", &cap_tmp);
		cap = cd_sensor_cap_from_string (cap_tmp);
		if (cap == CD_SENSOR_CAP_UNKNOWN) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SENSOR_ERROR,
							       CD_SENSOR_ERROR_INTERNAL,
							       "cap '%s' unknown",
							       cap_tmp);
			goto out;
		}

		/* proxy */
		sensor->priv->desc->get_sample_async (sensor,
						      cap,
						      NULL,
						      cd_sensor_get_sample_cb,
						      invocation);
		goto out;
	}

	/* return '' */
	if (g_strcmp0 (method_name, "SetOptions") == 0) {

		g_debug ("CdSensor %s:SetOptions()", sender);

		/* check locked */
		if (!priv->locked) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SENSOR_ERROR,
							       CD_SENSOR_ERROR_NOT_LOCKED,
							       "sensor is not yet locked");
			goto out;
		}

		/*  check idle */
		if (priv->state != CD_SENSOR_STATE_IDLE) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SENSOR_ERROR,
							       CD_SENSOR_ERROR_IN_USE,
							       "sensor not idle: %s",
							       cd_sensor_state_to_string (priv->state));
			goto out;
		}

		/* no support */
		if (sensor->priv->desc == NULL ||
		    sensor->priv->desc->set_options_async == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SENSOR_ERROR,
							       CD_SENSOR_ERROR_NO_SUPPORT,
							       "no sensor options support");
			goto out;
		}

		/* unwrap the parameters into a hash table */
		options = g_hash_table_new_full (g_str_hash, g_str_equal,
						 g_free, (GDestroyNotify) g_variant_unref);
		result = g_variant_get_child_value (parameters, 0);
		g_variant_iter_init (&iter, result);
		while (g_variant_iter_next (&iter, "{sv}", &key, &value))
			g_hash_table_insert (options, key, value);

		/* proxy */
		sensor->priv->desc->set_options_async (sensor,
						       options,
						       NULL,
						       cd_sensor_set_options_cb,
						       invocation);
		goto out;
	}

	/* we suck */
	g_critical ("failed to process sensor method %s", method_name);
out:
	if (options != NULL)
		g_hash_table_unref (options);
	if (result != NULL)
		g_variant_unref (result);
}

/**
 * cd_sensor_get_options_as_variant:
 **/
static GVariant *
cd_sensor_get_options_as_variant (CdSensor *sensor)
{
	GList *list, *l;
	GVariantBuilder builder;
	GVariant *value;

	/* do not try to build an empty array */
	if (g_hash_table_size (sensor->priv->options) == 0) {
		value = g_variant_new_array (G_VARIANT_TYPE ("{sv}"),
					     NULL, 0);
		goto out;
	}

	/* add all the keys in the dictionary to the variant builder */
	list = g_hash_table_get_keys (sensor->priv->options);
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	for (l = list; l != NULL; l = l->next) {
		g_variant_builder_add (&builder,
				       "{sv}",
				       l->data,
				       g_hash_table_lookup (sensor->priv->options,
							    l->data));
	}
	g_list_free (list);
	value = g_variant_builder_end (&builder);
out:
	return value;
}

/**
 * cd_sensor_get_metadata_as_variant:
 **/
static GVariant *
cd_sensor_get_metadata_as_variant (CdSensor *sensor)
{
	GList *list, *l;
	GVariantBuilder builder;
	GVariant *value;

	/* we always must have at least one bit of metadata */
	if (g_hash_table_size (sensor->priv->metadata) == 0) {
		value = g_variant_new_array (G_VARIANT_TYPE ("{sv}"),
					     NULL, 0);
		goto out;
	}
	/* add all the keys in the dictionary to the variant builder */
	list = g_hash_table_get_keys (sensor->priv->metadata);
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	for (l = list; l != NULL; l = l->next) {
		g_variant_builder_add (&builder,
				       "{ss}",
				       l->data,
				       g_hash_table_lookup (sensor->priv->metadata,
							    l->data));
	}
	g_list_free (list);
	value = g_variant_builder_end (&builder);
out:
	return value;
}

/**
 * cd_sensor_get_nullable_for_string:
 **/
static GVariant *
cd_sensor_get_nullable_for_string (const gchar *value)
{
	if (value == NULL)
		return g_variant_new_string ("");
	return g_variant_new_string (value);
}

/**
 * cd_sensor_get_variant_for_caps:
 **/
static GVariant *
cd_sensor_get_variant_for_caps (guint64 caps)
{
	guint i;
	GVariantBuilder builder;

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
	for (i = 0; i < CD_SENSOR_CAP_LAST; i++) {
		if (!cd_bitfield_contain (caps, i))
			continue;
		g_variant_builder_add (&builder, "s",
				       cd_sensor_cap_to_string (i));
	}
	return g_variant_new ("as", &builder);
}

/**
 * cd_sensor_dbus_get_property:
 **/
static GVariant *
cd_sensor_dbus_get_property (GDBusConnection *connection_, const gchar *sender,
			     const gchar *object_path, const gchar *interface_name,
			     const gchar *property_name, GError **error,
			     gpointer user_data)
{
	CdSensor *sensor = CD_SENSOR (user_data);
	CdSensorPrivate *priv = sensor->priv;
	GVariant *retval = NULL;

	if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_ID) == 0) {
		retval = g_variant_new_string (priv->id);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_KIND) == 0) {
		retval = g_variant_new_string (cd_sensor_kind_to_string (priv->kind));
		goto out;
	}
	if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_STATE) == 0) {
		retval = g_variant_new_string (cd_sensor_state_to_string (priv->state));
		goto out;
	}
	if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_MODE) == 0) {
		retval = g_variant_new_string (cd_sensor_cap_to_string (priv->mode));
		goto out;
	}
	if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_SERIAL) == 0) {
		retval = cd_sensor_get_nullable_for_string (priv->serial);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_MODEL) == 0) {
		retval = cd_sensor_get_nullable_for_string (priv->model);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_VENDOR) == 0) {
		retval = cd_sensor_get_nullable_for_string (priv->vendor);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_NATIVE) == 0) {
		retval = g_variant_new_boolean (priv->native);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_LOCKED) == 0) {
		retval = g_variant_new_boolean (priv->locked);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_EMBEDDED) == 0) {
		retval = g_variant_new_boolean (priv->embedded);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_CAPABILITIES) == 0) {
		retval = cd_sensor_get_variant_for_caps (priv->caps);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_OPTIONS) == 0) {
		retval = cd_sensor_get_options_as_variant (sensor);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_SENSOR_PROPERTY_METADATA) == 0) {
		retval = cd_sensor_get_metadata_as_variant (sensor);
		goto out;
	}

	/* return an error */
	g_set_error (error,
		     CD_SENSOR_ERROR,
		     CD_SENSOR_ERROR_INTERNAL,
		     "failed to get sensor property %s",
		     property_name);
out:
	return retval;
}

/**
 * cd_sensor_register_object:
 **/
gboolean
cd_sensor_register_object (CdSensor *sensor,
			   GDBusConnection *connection,
			   GDBusInterfaceInfo *info,
			   GError **error)
{
	GError *error_local = NULL;
	gboolean ret = FALSE;

	static const GDBusInterfaceVTable interface_vtable = {
		cd_sensor_dbus_method_call,
		cd_sensor_dbus_get_property,
		NULL
	};

	sensor->priv->connection = connection;
	sensor->priv->registration_id = g_dbus_connection_register_object (
		connection,
		sensor->priv->object_path,
		info,
		&interface_vtable,
		sensor,  /* user_data */
		NULL,  /* user_data_free_func */
		&error_local); /* GError** */
	if (sensor->priv->registration_id == 0) {
		g_set_error (error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_INTERNAL,
			     "failed to register object: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
	g_debug ("CdSensor: Register interface %i on %s",
		 sensor->priv->registration_id,
		 sensor->priv->object_path);

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cd_sensor_get_device_path:
 **/
const gchar *
cd_sensor_get_device_path (CdSensor *sensor)
{
#ifdef HAVE_GUDEV
	return g_udev_device_get_sysfs_path (sensor->priv->device);
#else
	return NULL;
#endif
}

#ifdef HAVE_GUSB
/**
 * cd_sensor_open_usb_device:
 **/
GUsbDevice *
cd_sensor_open_usb_device (CdSensor *sensor,
			   gint config,
			   gint interface,
			   GError **error)
{
	CdSensorPrivate *priv = sensor->priv;
	gboolean ret;
	guint8 busnum;
	guint8 devnum;
	GUsbDevice *device;
	GUsbDevice *device_success = NULL;

	/* convert from GUdevDevice to GUsbDevice */
	busnum = g_udev_device_get_sysfs_attr_as_int (priv->device, "busnum");
	devnum = g_udev_device_get_sysfs_attr_as_int (priv->device, "devnum");
	device = g_usb_device_list_find_by_bus_address (priv->device_list,
							busnum,
							devnum,
							error);
	if (device == NULL)
		goto out;

	/* open device, set config and claim interface */
	ret = g_usb_device_open (device, error);
	if (!ret)
		goto out;
	ret = g_usb_device_set_configuration (device,
					      config,
					      error);
	if (!ret)
		goto out;
	ret = g_usb_device_claim_interface (device,
					    interface,
					    G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
					    error);
	if (!ret)
		goto out;

	/* success */
	device_success = g_object_ref (device);
out:
	if (device != NULL)
		g_object_unref (device);
	return device_success;
}
#endif

/**
 * cd_sensor_add_cap:
 **/
void
cd_sensor_add_cap (CdSensor *sensor, CdSensorCap cap)
{
	cd_bitfield_add (sensor->priv->caps, cap);
	cd_sensor_dbus_emit_property_changed (sensor,
					      "Capabilities",
					      cd_sensor_get_variant_for_caps (sensor->priv->caps));

}

#ifdef HAVE_GUDEV
/**
 * cd_sensor_get_device:
 **/
GUdevDevice *
cd_sensor_get_device (CdSensor *sensor)
{
	return sensor->priv->device;
}

/**
 * cd_sensor_set_model:
 **/
static void
cd_sensor_set_model (CdSensor *sensor,
		     const gchar *model)
{
	if (g_strcmp0 (model, "colormunki") == 0)
		model = "ColorMunki";
	sensor->priv->model = g_strdup (model);
}

/**
 * cd_sensor_set_from_device:
 **/
gboolean
cd_sensor_set_from_device (CdSensor *sensor,
			   GUdevDevice *device,
			   GError **error)
{
	CdSensorCap cap;
	CdSensorPrivate *priv = sensor->priv;
	const gchar *images[] = { "attach", "calibrate", "screen", NULL };
	const gchar *images_md[] = { CD_SENSOR_METADATA_IMAGE_ATTACH,
				     CD_SENSOR_METADATA_IMAGE_CALIBRATE,
				     CD_SENSOR_METADATA_IMAGE_SCREEN,
				     NULL };
	const gchar *kind_str;
	const gchar *model_tmp = NULL;
	const gchar *vendor_tmp = NULL;
	const gchar * const *caps_str;
	gboolean ret;
	gboolean use_database;
	gchar *tmp;
	guint i;

	/* only use the database if we found both the VID and the PID */
	use_database = g_udev_device_has_property (device, "ID_VENDOR_FROM_DATABASE") &&
			g_udev_device_has_property (device, "ID_MODEL_FROM_DATABASE");

	/* vendor */
	if (use_database)
		vendor_tmp = g_udev_device_get_property (device, "ID_VENDOR_FROM_DATABASE");
	if (vendor_tmp == NULL)
		vendor_tmp = g_udev_device_get_property (device, "ID_VENDOR");
	if (vendor_tmp == NULL)
		vendor_tmp = g_udev_device_get_sysfs_attr (device, "manufacturer");
	if (vendor_tmp == NULL)
		vendor_tmp = "unknown";
	priv->vendor = g_strdup (vendor_tmp);

	/* make name sane */
	g_strdelimit (priv->vendor, "_", ' ');

	/* model */
	if (use_database)
		model_tmp = g_udev_device_get_property (device, "ID_MODEL_FROM_DATABASE");
	if (model_tmp == NULL)
		model_tmp = g_udev_device_get_property (device, "ID_MODEL");
	if (model_tmp == NULL)
		model_tmp = g_udev_device_get_sysfs_attr (device, "product");
	if (model_tmp == NULL)
		model_tmp = "Unknown";
	cd_sensor_set_model (sensor, model_tmp);

	/* make name sane */
	g_strdelimit (priv->model, "_", ' ');

	/* try to get type */
	kind_str = g_udev_device_get_property (device, "COLORD_SENSOR_KIND");
	priv->kind = cd_sensor_kind_from_string (kind_str);
	if (priv->kind == CD_SENSOR_KIND_UNKNOWN) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to recognize color device: %s - %s",
			     vendor_tmp, model_tmp);
		goto out;
	}

	/* get caps */
	caps_str = g_udev_device_get_property_as_strv (device,
						       "COLORD_SENSOR_CAPS");
	if (caps_str != NULL) {
		for (i = 0; caps_str[i] != NULL; i++) {
			cap = cd_sensor_cap_from_string (caps_str[i]);
			if (cap != CD_SENSOR_CAP_UNKNOWN) {
				cd_bitfield_add (priv->caps, cap);
			} else {
				g_warning ("Unknown sensor cap %s on %s",
					   caps_str[i], kind_str);
			}
		}
	}

	/* is the sensor embeded, e.g. on the W700? */
	ret = g_udev_device_get_property_as_boolean (device,
						     "COLORD_SENSOR_EMBEDDED");
	if (ret)
		priv->embedded = TRUE;

	/* add image metadata if the files exist */
	for (i = 0; images[i] != NULL; i++) {
		tmp = g_strdup_printf ("%s/colord/icons/%s-%s.svg",
				       DATADIR, kind_str, images[i]);
		if (g_file_test (tmp, G_FILE_TEST_EXISTS)) {
			g_debug ("helper image %s found", tmp);
			g_hash_table_insert (priv->metadata,
					     g_strdup (images_md[i]),
					     tmp);
		} else {
			g_debug ("helper image %s not found", tmp);
			g_free (tmp);
		}
	}

	/* some properties might not be valid in the GUdevDevice if the
	 * device changes as this is only a snapshot */
	priv->device = g_object_ref (device);

	/* success */
	ret = TRUE;
out:
	return ret;
}
#endif

/**
 * cd_sensor_set_index:
 **/
void
cd_sensor_set_index (CdSensor *sensor,
		     guint idx)
{
	gchar *id;
	id = g_strdup_printf ("%s-%02i",
			      cd_sensor_kind_to_string (sensor->priv->kind),
			      idx);
	cd_sensor_set_id (sensor, id);
	g_free (id);
}

/**
 * cd_sensor_add_option:
 **/
void
cd_sensor_add_option (CdSensor *sensor,
		      const gchar *key,
		      GVariant *value)
{
	GVariant *options;
	g_hash_table_insert (sensor->priv->options,
			     g_strdup (key),
			     g_variant_ref_sink (value));

	/* update clients */
	options = cd_sensor_get_options_as_variant (sensor);
	cd_sensor_dbus_emit_property_changed (sensor,
					      "Options",
					      options);
}

/**
 * cd_sensor_debug_data:
 * @debug_mode: the debug mode, e.g %CD_SENSOR_DEBUG_MODE_REQUEST
 * @data: the data of size @length
 * @length: the size of data
 *
 * Prints some debugging of the request to the console if debugging mode
 * is enabled.
 **/
void
cd_sensor_debug_data (CdSensorDebugMode debug_mode,
		      const guint8 *data,
		      gsize length)
{
	guint i;
	if (debug_mode == CD_SENSOR_DEBUG_MODE_REQUEST)
		g_print ("%c[%dm request\t", 0x1B, 31);
	else if (debug_mode == CD_SENSOR_DEBUG_MODE_RESPONSE)
		g_print ("%c[%dm response\t", 0x1B, 34);
	for (i = 0; i <  length; i++)
		g_print ("%02x [%c]\t", data[i], g_ascii_isprint (data[i]) ? data[i] : '?');
	g_print ("%c[%dm\n", 0x1B, 0);
}

/**
 * cd_sensor_get_property:
 **/
static void
cd_sensor_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorPrivate *priv = sensor->priv;

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_value_set_string (value, priv->object_path);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * cd_sensor_set_property:
 **/
static void
cd_sensor_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorPrivate *priv = sensor->priv;

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_free (priv->object_path);
		priv->object_path = g_strdup (g_value_get_string (value));
		break;
	case PROP_MODEL:
		g_free (priv->model);
		priv->model = g_strdup (g_value_get_string (value));
		break;
	case PROP_VENDOR:
		g_free (priv->vendor);
		priv->vendor = g_strdup (g_value_get_string (value));
		break;
	case PROP_ID:
		cd_sensor_set_id (sensor, g_value_get_string (value));
		break;
	case PROP_NATIVE:
		priv->native = g_value_get_boolean (value);
		break;
	case PROP_LOCKED:
		priv->locked = g_value_get_boolean (value);
		break;
	case PROP_STATE:
		priv->state = g_value_get_uint (value);
		break;
	case PROP_MODE:
		priv->mode = g_value_get_uint (value);
		break;
	case PROP_KIND:
		cd_sensor_set_kind (sensor, g_value_get_uint (value));
		break;
	case PROP_CAPS:
		priv->caps = g_value_get_uint64 (value);
		break;
	case PROP_SERIAL:
		cd_sensor_set_serial (sensor, g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * cd_sensor_class_init:
 **/
static void
cd_sensor_class_init (CdSensorClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_sensor_finalize;
	object_class->get_property = cd_sensor_get_property;
	object_class->set_property = cd_sensor_set_property;

	/**
	 * CdSensor:object-path:
	 */
	pspec = g_param_spec_string ("object-path", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_OBJECT_PATH, pspec);

	/**
	 * CdSensor:id:
	 */
	pspec = g_param_spec_string ("id", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ID, pspec);

	/**
	 * CdSensor:serial:
	 */
	pspec = g_param_spec_string ("serial", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SERIAL, pspec);

	/**
	 * CdSensor:model:
	 */
	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	/**
	 * CdSensor:vendor:
	 */
	pspec = g_param_spec_string ("vendor", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_VENDOR, pspec);

	/**
	 * CdSensor:caps:
	 */
	pspec = g_param_spec_uint64 ("caps", NULL, NULL,
				     0, G_MAXUINT64, 0,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CAPS, pspec);

	/**
	 * CdSensor:kind:
	 */
	pspec = g_param_spec_uint ("kind", NULL, NULL,
				   0, G_MAXUINT, CD_SENSOR_KIND_UNKNOWN,
				   G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_KIND, pspec);

	/**
	 * CdSensor:native:
	 */
	pspec = g_param_spec_boolean ("native", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_NATIVE, pspec);

	/**
	 * CdSensor:locked:
	 */
	pspec = g_param_spec_boolean ("locked", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LOCKED, pspec);

	g_type_class_add_private (klass, sizeof (CdSensorPrivate));
}

/**
 * cd_sensor_init:
 **/
static void
cd_sensor_init (CdSensor *sensor)
{
	sensor->priv = CD_SENSOR_GET_PRIVATE (sensor);
	sensor->priv->state = CD_SENSOR_STATE_IDLE;
	sensor->priv->mode = CD_SENSOR_CAP_UNKNOWN;
#ifdef HAVE_GUSB
	sensor->priv->usb_ctx = g_usb_context_new (NULL);
	sensor->priv->device_list = g_usb_device_list_new (sensor->priv->usb_ctx);
#endif
	sensor->priv->options = g_hash_table_new_full (g_str_hash,
						       g_str_equal,
						       (GDestroyNotify) g_free,
						       (GDestroyNotify) g_variant_unref);
	sensor->priv->metadata = g_hash_table_new_full (g_str_hash,
							g_str_equal,
							g_free,
							g_free);
}

/**
 * cd_sensor_finalize:
 **/
static void
cd_sensor_finalize (GObject *object)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorPrivate *priv = sensor->priv;

	if (priv->registration_id > 0) {
		g_debug ("CdSensor: Unregister interface %i on %s",
			  priv->registration_id,
			  priv->object_path);
		g_dbus_connection_unregister_object (priv->connection,
						     priv->registration_id);
	}
	if (priv->watcher_id != 0)
		g_bus_unwatch_name (priv->watcher_id);
	g_free (priv->model);
	g_free (priv->vendor);
	g_free (priv->serial);
	g_free (priv->id);
	g_free (priv->object_path);
	g_hash_table_unref (priv->options);
	g_hash_table_unref (priv->metadata);
#ifdef HAVE_GUSB
	g_object_unref (priv->usb_ctx);
	g_object_unref (priv->device_list);
#endif
#ifdef HAVE_GUDEV
	if (priv->device != NULL)
		g_object_unref (priv->device);
#endif

	G_OBJECT_CLASS (cd_sensor_parent_class)->finalize (object);
}

/**
 * cd_sensor_new:
 **/
CdSensor *
cd_sensor_new (void)
{
	CdSensor *sensor;
	sensor = g_object_new (CD_TYPE_SENSOR, NULL);
	return CD_SENSOR (sensor);
}

