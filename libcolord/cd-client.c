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
 * SECTION:cd-client
 * @short_description: Main client object for accessing the colord daemon
 *
 * A helper GObject to use for accessing colord information, and to be notified
 * when it is changed.
 *
 * See also: #CdDevice
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>

#include "cd-client.h"
#include "cd-device.h"
#include "cd-sensor.h"

static void	cd_client_class_init	(CdClientClass	*klass);
static void	cd_client_init		(CdClient	*client);
static void	cd_client_finalize	(GObject	*object);

#define CD_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_CLIENT, CdClientPrivate))

/**
 * CdClientPrivate:
 *
 * Private #CdClient data
 **/
struct _CdClientPrivate
{
	GDBusProxy		*proxy;
	gchar			*daemon_version;
	GPtrArray		*device_cache;
	GPtrArray		*profile_cache;
	GPtrArray		*sensor_cache;
};

typedef struct {
	CdClient		*client;
	GCancellable		*cancellable;
	GPtrArray		*array;
	GSimpleAsyncResult	*res;
} CdClientState;

enum {
	SIGNAL_CHANGED,
	SIGNAL_DEVICE_ADDED,
	SIGNAL_DEVICE_REMOVED,
	SIGNAL_DEVICE_CHANGED,
	SIGNAL_PROFILE_ADDED,
	SIGNAL_PROFILE_REMOVED,
	SIGNAL_PROFILE_CHANGED,
	SIGNAL_SENSOR_ADDED,
	SIGNAL_SENSOR_REMOVED,
	SIGNAL_SENSOR_CHANGED,
	SIGNAL_LAST
};

enum {
	PROP_0,
	PROP_DAEMON_VERSION,
	PROP_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };
static gpointer cd_client_object = NULL;

G_DEFINE_TYPE (CdClient, cd_client, G_TYPE_OBJECT)

/**
 * cd_client_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.0
 **/
GQuark
cd_client_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("cd_client_error");
	return quark;
}

/**
 * cd_client_get_cache_device:
 **/
static CdDevice *
cd_client_get_cache_device (CdClient *client,
			    const gchar *object_path)
{
	CdDevice *device = NULL;
	CdDevice *device_tmp;
	guint i;

	/* try and find existing */
	for (i=0; i<client->priv->device_cache->len; i++) {
		device_tmp = g_ptr_array_index (client->priv->device_cache, i);
		if (g_strcmp0 (cd_device_get_object_path (device_tmp),
			       object_path) == 0) {
			device = g_object_ref (device_tmp);
			break;
		}
	}
	return device;
}

/**
 * cd_client_get_cache_device_create:
 **/
static CdDevice *
cd_client_get_cache_device_create (CdClient *client,
				   const gchar *object_path,
				   GCancellable *cancellable,
				   GError **error)
{
	CdDevice *device = NULL;
	gboolean ret;
	GError *error_local = NULL;

	/* try and find existing, else create */
	device = cd_client_get_cache_device (client, object_path);
	if (device == NULL) {
		device = cd_device_new ();
		ret = cd_device_set_object_path_sync (device,
						      object_path,
						      cancellable,
						      &error_local);
		if (!ret) {
			g_set_error (error,
				     CD_CLIENT_ERROR,
				     CD_CLIENT_ERROR_FAILED,
				     "Failed to set device object path: %s",
				     error_local->message);
			g_error_free (error_local);
			g_object_unref (device);
			device = NULL;
			goto out;
		}
		g_ptr_array_add (client->priv->device_cache,
				 g_object_ref (device));
	}
out:
	return device;
}

/**
 * cd_client_get_cache_profile:
 **/
static CdProfile *
cd_client_get_cache_profile (CdClient *client,
			    const gchar *object_path)
{
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;
	guint i;

	/* try and find existing */
	for (i=0; i<client->priv->profile_cache->len; i++) {
		profile_tmp = g_ptr_array_index (client->priv->profile_cache, i);
		if (g_strcmp0 (cd_profile_get_object_path (profile_tmp),
			       object_path) == 0) {
			profile = g_object_ref (profile_tmp);
			break;
		}
	}
	return profile;
}

/**
 * cd_client_get_cache_profile_create:
 **/
static CdProfile *
cd_client_get_cache_profile_create (CdClient *client,
				   const gchar *object_path,
				   GCancellable *cancellable,
				   GError **error)
{
	CdProfile *profile = NULL;
	gboolean ret;
	GError *error_local = NULL;

	/* try and find existing, else create */
	profile = cd_client_get_cache_profile (client, object_path);
	if (profile == NULL) {
		profile = cd_profile_new ();
		ret = cd_profile_set_object_path_sync (profile,
						      object_path,
						      cancellable,
						      &error_local);
		if (!ret) {
			g_set_error (error,
				     CD_CLIENT_ERROR,
				     CD_CLIENT_ERROR_FAILED,
				     "Failed to set profile object path: %s",
				     error_local->message);
			g_error_free (error_local);
			g_object_unref (profile);
			profile = NULL;
			goto out;
		}
		g_ptr_array_add (client->priv->profile_cache,
				 g_object_ref (profile));
	}
out:
	return profile;
}

/**
 * cd_client_get_cache_sensor:
 **/
static CdSensor *
cd_client_get_cache_sensor (CdClient *client,
			    const gchar *object_path)
{
	CdSensor *sensor = NULL;
	CdSensor *sensor_tmp;
	guint i;

	/* try and find existing */
	for (i=0; i<client->priv->sensor_cache->len; i++) {
		sensor_tmp = g_ptr_array_index (client->priv->sensor_cache, i);
		if (g_strcmp0 (cd_sensor_get_object_path (sensor_tmp),
			       object_path) == 0) {
			sensor = g_object_ref (sensor_tmp);
			break;
		}
	}
	return sensor;
}

/**
 * cd_client_get_cache_sensor_create:
 **/
static CdSensor *
cd_client_get_cache_sensor_create (CdClient *client,
				   const gchar *object_path,
				   GCancellable *cancellable,
				   GError **error)
{
	CdSensor *sensor = NULL;
	gboolean ret;
	GError *error_local = NULL;

	/* try and find existing, else create */
	sensor = cd_client_get_cache_sensor (client, object_path);
	if (sensor == NULL) {
		sensor = cd_sensor_new ();
		ret = cd_sensor_set_object_path_sync (sensor,
						      object_path,
						      cancellable,
						      &error_local);
		if (!ret) {
			g_set_error (error,
				     CD_CLIENT_ERROR,
				     CD_CLIENT_ERROR_FAILED,
				     "Failed to set sensor object path: %s",
				     error_local->message);
			g_error_free (error_local);
			g_object_unref (sensor);
			sensor = NULL;
			goto out;
		}
		g_ptr_array_add (client->priv->sensor_cache,
				 g_object_ref (sensor));
	}
out:
	return sensor;
}

/**
 * cd_client_get_device_array_from_variant:
 **/
static GPtrArray *
cd_client_get_device_array_from_variant (CdClient *client,
					 GVariant *result,
					 GCancellable *cancellable,
					 GError **error)
{
	CdDevice *device;
	gchar *object_path_tmp;
	GPtrArray *array = NULL;
	GPtrArray *array_tmp = NULL;
	guint i;
	guint len;
	GVariant *child = NULL;
	GVariantIter iter;

	/* add each device */
	array_tmp = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	child = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, child);
	for (i=0; i < len; i++) {
		g_variant_get_child (child, i,
				     "o", &object_path_tmp);
		g_debug ("%s", object_path_tmp);

		/* try to get from device cache, else create */
		device = cd_client_get_cache_device_create (client,
							   object_path_tmp,
							   cancellable,
							   error);
		if (device == NULL)
			goto out;
		g_ptr_array_add (array_tmp, device);
		g_free (object_path_tmp);
	}

	/* success */
	array = g_ptr_array_ref (array_tmp);
out:
	if (child != NULL)
		g_variant_unref (child);
	if (array_tmp != NULL)
		g_ptr_array_unref (array_tmp);
	return array;
}

/**
 * cd_client_get_devices_sync:
 * @client: a #CdClient instance.
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Get an array of the device objects.
 *
 * Return value: (transfer full): an array of #CdDevice objects,
 *		 free with g_ptr_array_unref()
 *
 * Since: 0.1.0
 **/
GPtrArray *
cd_client_get_devices_sync (CdClient *client,
			    GCancellable *cancellable,
			    GError **error)
{
	GError *error_local = NULL;
	GPtrArray *array = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "GetDevices",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to GetDevices: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* convert to array of CdDevice's */
	array = cd_client_get_device_array_from_variant (client,
							 result,
							 cancellable,
							 error);
	if (array == NULL)
		goto out;
out:
	if (result != NULL)
		g_variant_unref (result);
	return array;
}

/**
 * cd_client_get_devices_state_finish:
 **/
static void
cd_client_get_devices_state_finish (CdClientState *state,
				    const GError *error)
{
	/* get result */
	if (state->array != NULL) {
		g_simple_async_result_set_op_res_gpointer (state->res,
							   g_ptr_array_ref (state->array),
							   (GDestroyNotify) g_ptr_array_unref);
	} else {
		g_simple_async_result_set_from_error (state->res,
						      error);
	}

	/* complete */
	g_simple_async_result_complete_in_idle (state->res);

	/* deallocate */
	if (state->cancellable != NULL)
		g_object_unref (state->cancellable);
	if (state->array != NULL)
		g_ptr_array_unref (state->array);
	g_object_unref (state->res);
	g_object_unref (state->client);
	g_slice_free (CdClientState, state);
}

/**
 * cd_client_get_devices_cb:
 **/
static void
cd_client_get_devices_cb (GObject *source_object,
			  GAsyncResult *res,
			  gpointer user_data)
{
	CdClientState *state = (CdClientState *) user_data;
	GDBusProxy *proxy = G_DBUS_PROXY (source_object);
	GError *error = NULL;
	GVariant *result;

	/* get result */
	result = g_dbus_proxy_call_finish (proxy, res, &error);
	if (result == NULL) {
		cd_client_get_devices_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* convert to array of CdDevice's */
	state->array = cd_client_get_device_array_from_variant (state->client,
								result,
								state->cancellable,
								&error);
	if (state->array == NULL) {
		cd_client_get_devices_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* we're done */
	cd_client_get_devices_state_finish (state, NULL);
out:
	if (result != NULL)
		g_variant_unref (result);
}

/**
 * cd_client_get_devices:
 * @client: a #CdClient instance
 * @cancellable: a #GCancellable or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Gets a transacton ID from the daemon.
 *
 * Since: 0.1.2
 **/
void
cd_client_get_devices (CdClient *client,
		       GCancellable *cancellable,
		       GAsyncReadyCallback callback,
		       gpointer user_data)
{
	CdClientState *state;
	GError *error = NULL;
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (callback != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_get_devices);

	/* save state */
	state = g_slice_new0 (CdClientState);
	state->res = g_object_ref (res);
	state->client = g_object_ref (client);
	if (cancellable != NULL)
		state->cancellable = g_object_ref (cancellable);

	/* check not already cancelled */
	if (cancellable != NULL &&
	     g_cancellable_set_error_if_cancelled (cancellable, &error)) {
		cd_client_get_devices_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* call D-Bus method async */
	g_dbus_proxy_call (client->priv->proxy,
			   "GetDevices",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_get_devices_cb,
			   state);
out:
	g_object_unref (res);
}

/**
 * cd_client_get_devices_finish:
 * @client: a #CdClient instance
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: the devices, or %NULL if unset, free with g_ptr_array_unref()
 *
 * Since: 0.1.2
 **/
GPtrArray *
cd_client_get_devices_finish (CdClient *client,
			      GAsyncResult *res,
			      GError **error)
{
	gpointer source_tag;
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	source_tag = g_simple_async_result_get_source_tag (simple);

	g_return_val_if_fail (source_tag == cd_client_get_devices, NULL);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	return g_ptr_array_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

/***************************************************************************************************/

/**
 * cd_client_create_device_sync:
 * @client: a #CdClient instance.
 * @id: identifier for the device
 * @scope: the scope of the device
 * @properties: properties to set on the device, or %NULL
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Creates a color device.
 *
 * Return value: A #CdDevice object, or %NULL for error
 *
 * Since: 0.1.2
 **/
CdDevice *
cd_client_create_device_sync (CdClient *client,
			      const gchar *id,
			      CdObjectScope scope,
			      GHashTable *properties,
			      GCancellable *cancellable,
			      GError **error)
{
	CdDevice *device = NULL;
	CdDevice *device_tmp = NULL;
	gboolean ret;
	gchar *object_path = NULL;
	GError *error_local = NULL;
	GList *list, *l;
	GVariantBuilder builder;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	/* add properties */
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	if (properties != NULL) {
		list = g_hash_table_get_keys (properties);
		for (l = list; l != NULL; l = l->next) {
			g_debug ("adding property %s",
				 (const gchar*) l->data);
			g_variant_builder_add (&builder,
					       "{ss}",
					       l->data,
					       g_hash_table_lookup (properties,
								    l->data));
		}
		g_list_free (list);
	} else {
		/* just fake something here */
		g_variant_builder_add (&builder,
				       "{ss}",
				       "Kind",
				       "unknown");
	}

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "CreateDevice",
					 g_variant_new ("(ssa{ss})",
						        id,
						        cd_object_scope_to_string (scope),
						        &builder),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to CreateDevice: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* create thick CdDevice object */
	g_variant_get (result, "(o)",
		       &object_path);
	device_tmp = cd_device_new ();
	ret = cd_device_set_object_path_sync (device_tmp,
					      object_path,
					      cancellable,
					      error);
	if (!ret)
		goto out;

	/* add to cache */
	g_ptr_array_add (client->priv->device_cache,
			 g_object_ref (device_tmp));

	/* success */
	device = g_object_ref (device_tmp);
out:
	g_free (object_path);
	if (device_tmp != NULL)
		g_object_unref (device_tmp);
	if (result != NULL)
		g_variant_unref (result);
	return device;
}

/**
 * cd_client_create_profile_sync:
 * @client: a #CdClient instance.
 * @id: identifier for the device
 * @scope: the scope of the profile
 * @properties: properties to set on the profile, or %NULL
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Creates a color profile.
 *
 * Return value: A #CdProfile object, or %NULL for error
 *
 * Since: 0.1.2
 **/
CdProfile *
cd_client_create_profile_sync (CdClient *client,
			       const gchar *id,
			       CdObjectScope scope,
			       GHashTable *properties,
			       GCancellable *cancellable,
			       GError **error)
{
	CdProfile *profile = NULL;
	CdProfile *profile_tmp = NULL;
	gboolean ret;
	gchar *object_path = NULL;
	GError *error_local = NULL;
	GList *list, *l;
	GVariantBuilder builder;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	/* add properties */
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	if (properties != NULL) {
		list = g_hash_table_get_keys (properties);
		for (l = list; l != NULL; l = l->next) {
			g_debug ("adding property %s",
				 (const gchar*) l->data);
			g_variant_builder_add (&builder,
					       "{ss}",
					       l->data,
					       g_hash_table_lookup (properties,
								    l->data));
		}
		g_list_free (list);
	} else {
		/* just fake something here */
		g_variant_builder_add (&builder,
				       "{ss}",
				       "Qualifier",
				       "");
	}

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "CreateProfile",
					 g_variant_new ("(ssa{ss})",
						        id,
						        cd_object_scope_to_string (scope),
						        &builder),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to CreateProfile: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* create thick CdDevice object */
	g_variant_get (result, "(o)",
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
	if (result != NULL)
		g_variant_unref (result);
	return profile;
}

/**
 * cd_client_delete_device_sync:
 * @client: a #CdClient instance.
 * @device: a #CdDevice instance.
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Deletes a color device.
 *
 * Return value: %TRUE is the device was deleted
 *
 * Since: 0.1.0
 **/
gboolean
cd_client_delete_device_sync (CdClient *client,
			      const gchar *id,
			      GCancellable *cancellable,
			      GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (client->priv->proxy != NULL, FALSE);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "DeleteDevice",
					 g_variant_new ("(s)", id),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to DeleteDevice: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	if (result != NULL)
		g_variant_unref (result);
	return ret;
}

/**
 * cd_client_delete_profile_sync:
 * @client: a #CdClient instance.
 * @profile: a #CdProfile instance.
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Deletes a color profile.
 *
 * Return value: %TRUE is the profile was deleted
 *
 * Since: 0.1.0
 **/
gboolean
cd_client_delete_profile_sync (CdClient *client,
			       const gchar *id,
			       GCancellable *cancellable,
			       GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (client->priv->proxy != NULL, FALSE);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "DeleteProfile",
					 g_variant_new ("(s)", id),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to DeleteProfile: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	if (result != NULL)
		g_variant_unref (result);
	return ret;
}

/**
 * cd_client_find_device_sync:
 * @client: a #CdClient instance.
 * @id: identifier for the device
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Finds a color device.
 *
 * Return value: A #CdDevice object, or %NULL for error
 *
 * Since: 0.1.0
 **/
CdDevice *
cd_client_find_device_sync (CdClient *client,
			    const gchar *id,
			    GCancellable *cancellable,
			    GError **error)
{
	CdDevice *device = NULL;
	CdDevice *device_tmp = NULL;
	gboolean ret;
	gchar *object_path = NULL;
	GError *error_local = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "FindDeviceById",
					 g_variant_new ("(s)", id),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to FindDeviceById: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* create GObject CdDevice object */
	g_variant_get (result, "(o)",
		       &object_path);
	device_tmp = cd_device_new ();
	ret = cd_device_set_object_path_sync (device_tmp,
					      object_path,
					      cancellable,
					      error);
	if (!ret)
		goto out;

	/* success */
	device = g_object_ref (device_tmp);
out:
	g_free (object_path);
	if (device_tmp != NULL)
		g_object_unref (device_tmp);
	if (result != NULL)
		g_variant_unref (result);
	return device;
}

/**
 * cd_client_find_profile_sync:
 * @client: a #CdClient instance.
 * @id: identifier for the filename
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Finds a color profile.
 *
 * Return value: A #CdProfile object, or %NULL for error
 *
 * Since: 0.1.0
 **/
CdProfile *
cd_client_find_profile_sync (CdClient *client,
			     const gchar *id,
			     GCancellable *cancellable,
			     GError **error)
{
	CdProfile *profile = NULL;
	CdProfile *profile_tmp = NULL;
	gboolean ret;
	gchar *object_path = NULL;
	GError *error_local = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "FindProfileById",
					 g_variant_new ("(s)", id),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to FindProfileById: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* create GObject CdDevice object */
	g_variant_get (result, "(o)",
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
	if (result != NULL)
		g_variant_unref (result);
	return profile;
}

/**
 * cd_client_find_profile_by_filename_sync:
 * @client: a #CdClient instance.
 * @filename: filename for the profile
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Finds a color profile from its filename.
 *
 * Return value: A #CdProfile object, or %NULL for error
 *
 * Since: 0.1.3
 **/
CdProfile *
cd_client_find_profile_by_filename_sync (CdClient *client,
					 const gchar *filename,
					 GCancellable *cancellable,
					 GError **error)
{
	CdProfile *profile = NULL;
	CdProfile *profile_tmp = NULL;
	gboolean ret;
	gchar *object_path = NULL;
	GError *error_local = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "FindProfileByFilename",
					 g_variant_new ("(s)", filename),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to FindProfileByFilename: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* create GObject CdDevice object */
	g_variant_get (result, "(o)",
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
	if (result != NULL)
		g_variant_unref (result);
	return profile;
}

/**
 * cd_client_get_devices_by_kind_sync:
 * @client: a #CdClient instance.
 * @kind: a #CdDeviceKind, e.g. %CD_DEVICE_KIND_DISPLAY
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Get an array of the device objects of a specified kind.
 *
 * Return value: (transfer full): an array of #CdDevice objects,
 *		 free with g_ptr_array_unref()
 *
 * Since: 0.1.0
 **/
GPtrArray *
cd_client_get_devices_by_kind_sync (CdClient *client,
				    CdDeviceKind kind,
				    GCancellable *cancellable,
				    GError **error)
{
	GError *error_local = NULL;
	GPtrArray *array = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "GetDevicesByKind",
					 g_variant_new ("(s)",
					 	        cd_device_kind_to_string (kind)),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to GetDevicesByKind: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* convert to array of CdDevice's */
	array = cd_client_get_device_array_from_variant (client,
							 result,
							 cancellable,
							 error);
	if (array == NULL)
		goto out;
out:
	if (result != NULL)
		g_variant_unref (result);
	return array;
}

/**
 * cd_client_get_profile_array_from_variant:
 **/
static GPtrArray *
cd_client_get_profile_array_from_variant (CdClient *client,
					  GVariant *result,
					  GCancellable *cancellable,
					  GError **error)
{
	CdProfile *profile;
	gboolean ret;
	gchar *object_path_tmp;
	GError *error_local = NULL;
	GPtrArray *array = NULL;
	GPtrArray *array_tmp = NULL;
	guint i;
	guint len;
	GVariant *child = NULL;
	GVariantIter iter;

	/* add each profile */
	array_tmp = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	child = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, child);
	for (i=0; i < len; i++) {
		g_variant_get_child (child, i,
				     "o", &object_path_tmp);
		g_debug ("%s", object_path_tmp);

		/* create profile and add to the array */
		profile = cd_profile_new ();
		ret = cd_profile_set_object_path_sync (profile,
						      object_path_tmp,
						      cancellable,
						      &error_local);
		if (!ret) {
			g_set_error (error,
				     CD_CLIENT_ERROR,
				     CD_CLIENT_ERROR_FAILED,
				     "Failed to set profile object path: %s",
				     error_local->message);
			g_error_free (error_local);
			g_object_unref (profile);
			goto out;
		}
		g_ptr_array_add (array_tmp, profile);
		g_free (object_path_tmp);
	}

	/* success */
	array = g_ptr_array_ref (array_tmp);
out:
	if (child != NULL)
		g_variant_unref (child);
	if (array_tmp != NULL)
		g_ptr_array_unref (array_tmp);
	return array;
}

/**
 * cd_client_get_profiles_sync:
 * @client: a #CdClient instance.
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Get an array of the profile objects.
 *
 * Return value: (transfer full): an array of #CdProfile objects,
 *		 free with g_ptr_array_unref()
 *
 * Since: 0.1.0
 **/
GPtrArray *
cd_client_get_profiles_sync (CdClient *client,
			    GCancellable *cancellable,
			    GError **error)
{
	GError *error_local = NULL;
	GPtrArray *array = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "GetProfiles",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to GetProfiles: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* convert to array of CdProfile's */
	array = cd_client_get_profile_array_from_variant (client,
							  result,
							  cancellable,
							  error);
	if (array == NULL)
		goto out;
out:
	if (result != NULL)
		g_variant_unref (result);
	return array;
}

/**
 * cd_client_get_profiles_state_finish:
 **/
static void
cd_client_get_profiles_state_finish (CdClientState *state,
				    const GError *error)
{
	/* get result */
	if (state->array != NULL) {
		g_simple_async_result_set_op_res_gpointer (state->res,
							   g_ptr_array_ref (state->array),
							   (GDestroyNotify) g_ptr_array_unref);
	} else {
		g_simple_async_result_set_from_error (state->res,
						      error);
	}

	/* complete */
	g_simple_async_result_complete_in_idle (state->res);

	/* deallocate */
	if (state->cancellable != NULL)
		g_object_unref (state->cancellable);
	if (state->array != NULL)
		g_ptr_array_unref (state->array);
	g_object_unref (state->res);
	g_object_unref (state->client);
	g_slice_free (CdClientState, state);
}

/**
 * cd_client_get_profiles_cb:
 **/
static void
cd_client_get_profiles_cb (GObject *source_object,
			   GAsyncResult *res,
			   gpointer user_data)
{
	CdClientState *state = (CdClientState *) user_data;
	GDBusProxy *proxy = G_DBUS_PROXY (source_object);
	GError *error = NULL;
	GVariant *result;

	/* get result */
	result = g_dbus_proxy_call_finish (proxy, res, &error);
	if (result == NULL) {
		cd_client_get_profiles_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* convert to array of CdProfile's */
	state->array = cd_client_get_profile_array_from_variant (state->client,
								 result,
								 state->cancellable,
								 &error);
	if (state->array == NULL) {
		cd_client_get_profiles_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* we're done */
	cd_client_get_profiles_state_finish (state, NULL);
out:
	if (result != NULL)
		g_variant_unref (result);
}

/**
 * cd_client_get_profiles:
 * @client: a #CdClient instance
 * @cancellable: a #GCancellable or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Gets a transacton ID from the daemon.
 *
 * Since: 0.1.2
 **/
void
cd_client_get_profiles (CdClient *client,
		        GCancellable *cancellable,
		        GAsyncReadyCallback callback,
		        gpointer user_data)
{
	CdClientState *state;
	GError *error = NULL;
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (callback != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_get_profiles);

	/* save state */
	state = g_slice_new0 (CdClientState);
	state->res = g_object_ref (res);
	state->client = g_object_ref (client);
	if (cancellable != NULL)
		state->cancellable = g_object_ref (cancellable);

	/* check not already cancelled */
	if (cancellable != NULL &&
	     g_cancellable_set_error_if_cancelled (cancellable, &error)) {
		cd_client_get_profiles_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* call D-Bus method async */
	g_dbus_proxy_call (client->priv->proxy,
			   "GetProfiles",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_get_profiles_cb,
			   state);
out:
	g_object_unref (res);
}

/**
 * cd_client_get_profiles_finish:
 * @client: a #CdClient instance
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: the profiles, or %NULL if unset, free with g_ptr_array_unref()
 *
 * Since: 0.1.2
 **/
GPtrArray *
cd_client_get_profiles_finish (CdClient *client,
			      GAsyncResult *res,
			      GError **error)
{
	gpointer source_tag;
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	source_tag = g_simple_async_result_get_source_tag (simple);

	g_return_val_if_fail (source_tag == cd_client_get_profiles, NULL);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	return g_ptr_array_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

/***************************************************************************************************/

/**
 * cd_client_get_sensor_array_from_variant:
 **/
static GPtrArray *
cd_client_get_sensor_array_from_variant (CdClient *client,
					 GVariant *result,
					 GCancellable *cancellable,
					 GError **error)
{
	CdSensor *sensor;
	gboolean ret;
	gchar *object_path_tmp;
	GError *error_local = NULL;
	GPtrArray *array = NULL;
	GPtrArray *array_tmp = NULL;
	guint i;
	guint len;
	GVariant *child = NULL;
	GVariantIter iter;

	/* add each sensor */
	array_tmp = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	child = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, child);
	for (i=0; i < len; i++) {
		g_variant_get_child (child, i,
				     "o", &object_path_tmp);
		g_debug ("%s", object_path_tmp);

		/* create sensor and add to the array */
		sensor = cd_sensor_new ();
		ret = cd_sensor_set_object_path_sync (sensor,
						      object_path_tmp,
						      cancellable,
						      &error_local);
		if (!ret) {
			g_set_error (error,
				     CD_CLIENT_ERROR,
				     CD_CLIENT_ERROR_FAILED,
				     "Failed to set sensor object path: %s",
				     error_local->message);
			g_error_free (error_local);
			g_object_unref (sensor);
			goto out;
		}
		g_ptr_array_add (array_tmp, sensor);
		g_free (object_path_tmp);
	}

	/* success */
	array = g_ptr_array_ref (array_tmp);
out:
	if (child != NULL)
		g_variant_unref (child);
	if (array_tmp != NULL)
		g_ptr_array_unref (array_tmp);
	return array;
}

/**
 * cd_client_get_sensors_sync:
 * @client: a #CdClient instance.
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Get an array of the sensor objects.
 *
 * Return value: (transfer full): an array of #CdSensor objects,
 *		 free with g_ptr_array_unref()
 *
 * Since: 0.1.0
 **/
GPtrArray *
cd_client_get_sensors_sync (CdClient *client,
			    GCancellable *cancellable,
			    GError **error)
{
	GError *error_local = NULL;
	GPtrArray *array = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "GetSensors",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to GetSensors: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* convert to array of CdSensor's */
	array = cd_client_get_sensor_array_from_variant (client,
							 result,
							 cancellable,
							 error);
	if (array == NULL)
		goto out;
out:
	if (result != NULL)
		g_variant_unref (result);
	return array;
}

/**
 * cd_client_get_sensors_state_finish:
 **/
static void
cd_client_get_sensors_state_finish (CdClientState *state,
				    const GError *error)
{
	/* get result */
	if (state->array != NULL) {
		g_simple_async_result_set_op_res_gpointer (state->res,
							   g_ptr_array_ref (state->array),
							   (GDestroyNotify) g_ptr_array_unref);
	} else {
		g_simple_async_result_set_from_error (state->res,
						      error);
	}

	/* complete */
	g_simple_async_result_complete_in_idle (state->res);

	/* deallocate */
	if (state->cancellable != NULL)
		g_object_unref (state->cancellable);
	if (state->array != NULL)
		g_ptr_array_unref (state->array);
	g_object_unref (state->res);
	g_object_unref (state->client);
	g_slice_free (CdClientState, state);
}

/**
 * cd_client_get_sensors_cb:
 **/
static void
cd_client_get_sensors_cb (GObject *source_object,
			   GAsyncResult *res,
			   gpointer user_data)
{
	CdClientState *state = (CdClientState *) user_data;
	GDBusProxy *proxy = G_DBUS_PROXY (source_object);
	GError *error = NULL;
	GVariant *result;

	/* get result */
	result = g_dbus_proxy_call_finish (proxy, res, &error);
	if (result == NULL) {
		cd_client_get_sensors_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* convert to array of CdSensor's */
	state->array = cd_client_get_sensor_array_from_variant (state->client,
								 result,
								 state->cancellable,
								 &error);
	if (state->array == NULL) {
		cd_client_get_sensors_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* we're done */
	cd_client_get_sensors_state_finish (state, NULL);
out:
	if (result != NULL)
		g_variant_unref (result);
}

/**
 * cd_client_get_sensors:
 * @client: a #CdClient instance
 * @cancellable: a #GCancellable or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Gets a transacton ID from the daemon.
 *
 * Since: 0.1.2
 **/
void
cd_client_get_sensors (CdClient *client,
		        GCancellable *cancellable,
		        GAsyncReadyCallback callback,
		        gpointer user_data)
{
	CdClientState *state;
	GError *error = NULL;
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (callback != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_get_sensors);

	/* save state */
	state = g_slice_new0 (CdClientState);
	state->res = g_object_ref (res);
	state->client = g_object_ref (client);
	if (cancellable != NULL)
		state->cancellable = g_object_ref (cancellable);

	/* check not already cancelled */
	if (cancellable != NULL &&
	     g_cancellable_set_error_if_cancelled (cancellable, &error)) {
		cd_client_get_sensors_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* call D-Bus method async */
	g_dbus_proxy_call (client->priv->proxy,
			   "GetSensors",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_get_sensors_cb,
			   state);
out:
	g_object_unref (res);
}

/**
 * cd_client_get_sensors_finish:
 * @client: a #CdClient instance
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: the sensors, or %NULL if unset, free with g_ptr_array_unref()
 *
 * Since: 0.1.2
 **/
GPtrArray *
cd_client_get_sensors_finish (CdClient *client,
			      GAsyncResult *res,
			      GError **error)
{
	gpointer source_tag;
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	source_tag = g_simple_async_result_get_source_tag (simple);

	g_return_val_if_fail (source_tag == cd_client_get_sensors, NULL);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	return g_ptr_array_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

/***************************************************************************************************/

/**
 * cd_client_dbus_signal_cb:
 **/
static void
cd_client_dbus_signal_cb (GDBusProxy *proxy,
			  gchar      *sender_name,
			  gchar      *signal_name,
			  GVariant   *parameters,
			  CdClient   *client)
{
	CdDevice *device = NULL;
	CdProfile *profile = NULL;
	CdSensor *sensor = NULL;
	gchar *object_path_tmp = NULL;
	GError *error = NULL;

	if (g_strcmp0 (signal_name, "Changed") == 0) {
		g_warning ("changed");
	} else if (g_strcmp0 (signal_name, "DeviceAdded") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);

		/* try to get from device cache, else create */
		device = cd_client_get_cache_device_create (client,
							    object_path_tmp,
							    NULL,
							    &error);
		if (device == NULL) {
			g_warning ("failed to get cached device %s: %s",
				   object_path_tmp, error->message);
			g_error_free (error);
			goto out;
		}
		g_debug ("CdClient: emit '%s' on %s",
			 signal_name, object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_DEVICE_ADDED], 0,
			       device);
	} else if (g_strcmp0 (signal_name, "DeviceRemoved") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);

		/* if the device isn't in the cache, we don't care
		 * as the process isn't aware of it's existance */
		device = cd_client_get_cache_device (client,
						     object_path_tmp);
		if (device == NULL) {
			g_debug ("failed to get cached device %s",
				 object_path_tmp);
			goto out;
		}
		g_debug ("CdClient: emit '%s' on %s",
			 signal_name, object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_DEVICE_REMOVED], 0,
			       device);
	} else if (g_strcmp0 (signal_name, "DeviceChanged") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);

		/* is device in the cache? */
		device = cd_client_get_cache_device (client,
						     object_path_tmp);
		if (device == NULL) {
			g_debug ("failed to get cached device %s",
				 object_path_tmp);
			goto out;
		}
		g_debug ("CdClient: emit '%s' on %s",
			 signal_name, object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_DEVICE_CHANGED], 0,
			       device);
	} else if (g_strcmp0 (signal_name, "ProfileAdded") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);

		/* try to get from device cache, else create */
		profile = cd_client_get_cache_profile_create (client,
							      object_path_tmp,
							      NULL,
							      &error);
		if (profile == NULL) {
			g_warning ("failed to get cached device %s: %s",
				   object_path_tmp, error->message);
			g_error_free (error);
			goto out;
		}
		g_debug ("CdClient: emit '%s' on %s",
			 signal_name, object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_PROFILE_ADDED], 0,
			       profile);
	} else if (g_strcmp0 (signal_name, "ProfileRemoved") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);

		/* if the profile isn't in the cache, we don't care
		 * as the process isn't aware of it's existance */
		profile = cd_client_get_cache_profile (client,
						       object_path_tmp);
		if (profile == NULL) {
			g_debug ("failed to get cached profile %s",
				 object_path_tmp);
			goto out;
		}
		g_debug ("CdClient: emit '%s' on %s",
			 signal_name, object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_PROFILE_REMOVED], 0,
			       profile);
	} else if (g_strcmp0 (signal_name, "ProfileChanged") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);

		/* is device in the cache? */
		profile = cd_client_get_cache_profile (client,
						       object_path_tmp);
		if (profile == NULL) {
			g_debug ("failed to get cached profile %s",
				 object_path_tmp);
			goto out;
		}
		g_debug ("CdClient: emit '%s' on %s",
			 signal_name, object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_PROFILE_CHANGED], 0,
			       profile);
	} else if (g_strcmp0 (signal_name, "SensorAdded") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);

		/* try to get from device cache, else create */
		sensor = cd_client_get_cache_sensor_create (client,
							    object_path_tmp,
							    NULL,
							    &error);
		if (sensor == NULL) {
			g_warning ("failed to get cached device %s: %s",
				   object_path_tmp, error->message);
			g_error_free (error);
			goto out;
		}
		g_debug ("CdClient: emit '%s' on %s",
			 signal_name, object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_SENSOR_ADDED], 0,
			       sensor);
	} else if (g_strcmp0 (signal_name, "SensorRemoved") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);

		/* if the sensor isn't in the cache, we don't care
		 * as the process isn't aware of it's existance */
		sensor = cd_client_get_cache_sensor (client,
						     object_path_tmp);
		if (sensor == NULL) {
			g_debug ("failed to get cached sensor %s",
				 object_path_tmp);
			goto out;
		}
		g_debug ("CdClient: emit '%s' on %s",
			 signal_name, object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_SENSOR_REMOVED], 0,
			       sensor);
	} else if (g_strcmp0 (signal_name, "SensorChanged") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);

		/* is device in the cache? */
		sensor = cd_client_get_cache_sensor (client,
						     object_path_tmp);
		if (sensor == NULL) {
			g_debug ("failed to get cached sensor %s",
				 object_path_tmp);
			goto out;
		}
		g_debug ("CdClient: emit '%s' on %s",
			 signal_name, object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_SENSOR_CHANGED], 0,
			       sensor);
	} else {
		g_warning ("unhandled signal '%s'", signal_name);
	}
out:
	g_free (object_path_tmp);
	if (device != NULL)
		g_object_unref (device);
	if (sensor != NULL)
		g_object_unref (sensor);
	if (profile != NULL)
		g_object_unref (profile);
}

/**
 * cd_client_owner_notify_cb:
 **/
static void
cd_client_owner_notify_cb (GObject *object,
			   GParamSpec *pspec,
			   CdClient *client)
{
	g_debug ("daemon has quit, clearing caches");
	g_ptr_array_set_size (client->priv->device_cache, 0);
	g_ptr_array_set_size (client->priv->profile_cache, 0);
	g_ptr_array_set_size (client->priv->sensor_cache, 0);
}

/**
 * cd_client_connect_sync:
 * @client: a #CdClient instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Connects to the colord daemon.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.0
 **/
gboolean
cd_client_connect_sync (CdClient *client, GCancellable *cancellable, GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *daemon_version = NULL;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (client->priv->proxy == NULL, FALSE);

	/* connect to the daemon */
	client->priv->proxy =
		g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					       G_DBUS_PROXY_FLAGS_NONE,
					       NULL,
					       COLORD_DBUS_SERVICE,
					       COLORD_DBUS_PATH,
					       COLORD_DBUS_INTERFACE,
					       cancellable,
					       &error_local);
	if (client->priv->proxy == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to connect to colord: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* get daemon version */
	daemon_version = g_dbus_proxy_get_cached_property (client->priv->proxy,
							   "Title");
	if (daemon_version != NULL)
		client->priv->daemon_version = g_variant_dup_string (daemon_version, NULL);

	/* get signals from DBus */
	g_signal_connect (client->priv->proxy,
			  "g-signal",
			  G_CALLBACK (cd_client_dbus_signal_cb),
			  client);

	/* watch to see if it's fallen off the bus */
	g_signal_connect (client->priv->proxy,
			 "notify::g-name-owner",
			 G_CALLBACK (cd_client_owner_notify_cb),
			 client);

	/* success */
	g_debug ("Connected to colord daemon version %s",
		 client->priv->daemon_version);
out:
	if (daemon_version != NULL)
		g_variant_unref (daemon_version);
	return ret;
}

/**
 * cd_client_get_daemon_version:
 * @client: a #CdClient instance.
 *
 * Get colord daemon version.
 *
 * Return value: string containing the daemon version, e.g. 0.1.0
 *
 * Since: 0.1.0
 **/
const gchar *
cd_client_get_daemon_version (CdClient *client)
{
	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	return client->priv->daemon_version;
}

/*
 * cd_client_get_property:
 */
static void
cd_client_get_property (GObject *object,
			 guint prop_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	CdClient *client;
	client = CD_CLIENT (object);

	switch (prop_id) {
	case PROP_DAEMON_VERSION:
		g_value_set_string (value, client->priv->daemon_version);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/*
 * cd_client_class_init:
 */
static void
cd_client_class_init (CdClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = cd_client_get_property;
	object_class->finalize = cd_client_finalize;

	/**
	 * CdClient:daemon-version:
	 *
	 * The daemon version.
	 *
	 * Since: 0.1.0
	 */
	g_object_class_install_property (object_class,
					 PROP_DAEMON_VERSION,
					 g_param_spec_string ("daemon-version",
							      "Daemon version",
							      NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdClient::device-added:
	 * @client: the #CdClient instance that emitted the signal
	 * @device: the #CdDevice that was added.
	 *
	 * The ::device-added signal is emitted when a device is added.
	 *
	 * Since: 0.1.0
	 **/
	signals [SIGNAL_DEVICE_ADDED] =
		g_signal_new ("device-added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdClientClass, device_added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_DEVICE);

	/**
	 * CdClient::device-removed:
	 * @client: the #CdClient instance that emitted the signal
	 * @device: the #CdDevice that was removed.
	 *
	 * The ::device-added signal is emitted when a device is removed.
	 *
	 * Since: 0.1.0
	 **/
	signals [SIGNAL_DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdClientClass, device_removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_DEVICE);
	/**
	 * CdClient::device-changed:
	 * @client: the #CdClient instance that emitted the signal
	 * @device: the #CdDevice that was changed.
	 *
	 * The ::device-changed signal is emitted when a device is changed.
	 *
	 * Since: 0.1.2
	 **/
	signals [SIGNAL_DEVICE_CHANGED] =
		g_signal_new ("device-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdClientClass, device_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_DEVICE);
	/**
	 * CdClient::profile-added:
	 * @client: the #CdClient instance that emitted the signal
	 * @profile: the #CdProfile that was added.
	 *
	 * The ::profile-added signal is emitted when a profile is added.
	 *
	 * Since: 0.1.2
	 **/
	signals [SIGNAL_PROFILE_ADDED] =
		g_signal_new ("profile-added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdClientClass, profile_added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_PROFILE);

	/**
	 * CdClient::profile-removed:
	 * @client: the #CdClient instance that emitted the signal
	 * @profile: the #CdProfile that was removed.
	 *
	 * The ::profile-added signal is emitted when a profile is removed.
	 *
	 * Since: 0.1.2
	 **/
	signals [SIGNAL_PROFILE_REMOVED] =
		g_signal_new ("profile-removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdClientClass, profile_removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_PROFILE);
	/**
	 * CdClient::profile-changed:
	 * @client: the #CdClient instance that emitted the signal
	 * @profile: the #CdProfile that was removed.
	 *
	 * The ::profile-changed signal is emitted when a profile is changed.
	 *
	 * Since: 0.1.2
	 **/
	signals [SIGNAL_PROFILE_CHANGED] =
		g_signal_new ("profile-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdClientClass, profile_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_PROFILE);

	/**
	 * CdClient::sensor-added:
	 * @client: the #CdClient instance that emitted the signal
	 * @sensor: the #CdSensor that was added.
	 *
	 * The ::sensor-added signal is emitted when a sensor is added.
	 *
	 * Since: 0.1.6
	 **/
	signals [SIGNAL_SENSOR_ADDED] =
		g_signal_new ("sensor-added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdClientClass, sensor_added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_SENSOR);

	/**
	 * CdClient::sensor-removed:
	 * @client: the #CdClient instance that emitted the signal
	 * @sensor: the #CdSensor that was removed.
	 *
	 * The ::sensor-added signal is emitted when a sensor is removed.
	 *
	 * Since: 0.1.6
	 **/
	signals [SIGNAL_SENSOR_REMOVED] =
		g_signal_new ("sensor-removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdClientClass, sensor_removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_SENSOR);
	/**
	 * CdClient::sensor-changed:
	 * @client: the #CdClient instance that emitted the signal
	 * @sensor: the #CdSensor that was removed.
	 *
	 * The ::sensor-changed signal is emitted when a sensor is changed.
	 *
	 * Since: 0.1.6
	 **/
	signals [SIGNAL_SENSOR_CHANGED] =
		g_signal_new ("sensor-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdClientClass, sensor_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_SENSOR);

	/**
	 * CdClient::changed:
	 * @client: the #CdDevice instance that emitted the signal
	 *
	 * The ::changed signal is emitted when properties may have changed.
	 *
	 * Since: 0.1.0
	 **/
	signals [SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdClientClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (CdClientPrivate));
}

/*
 * cd_client_init:
 */
static void
cd_client_init (CdClient *client)
{
	client->priv = CD_CLIENT_GET_PRIVATE (client);
	client->priv->device_cache = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	client->priv->profile_cache = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	client->priv->sensor_cache = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

/*
 * cd_client_finalize:
 */
static void
cd_client_finalize (GObject *object)
{
	CdClient *client = CD_CLIENT (object);

	g_return_if_fail (CD_IS_CLIENT (object));

	g_free (client->priv->daemon_version);
	g_ptr_array_unref (client->priv->device_cache);
	g_ptr_array_unref (client->priv->profile_cache);
	g_ptr_array_unref (client->priv->sensor_cache);
	if (client->priv->proxy != NULL)
		g_object_unref (client->priv->proxy);

	G_OBJECT_CLASS (cd_client_parent_class)->finalize (object);
}

/**
 * cd_client_new:
 *
 * Creates a new #CdClient object.
 *
 * Return value: a new CdClient object.
 *
 * Since: 0.1.0
 **/
CdClient *
cd_client_new (void)
{
	if (cd_client_object != NULL) {
		g_object_ref (cd_client_object);
	} else {
		cd_client_object = g_object_new (CD_TYPE_CLIENT, NULL);
		g_object_add_weak_pointer (cd_client_object, &cd_client_object);
	}
	return CD_CLIENT (cd_client_object);
}

