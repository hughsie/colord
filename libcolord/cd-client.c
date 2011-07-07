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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib/gstdio.h>
#include <glib.h>

#include "cd-enum.h"
#include "cd-client.h"
#include "cd-client-sync.h"
#include "cd-device.h"
#include "cd-device-sync.h"
#include "cd-sensor.h"
#include "cd-profile-sync.h"

static void	cd_client_class_init	(CdClientClass	*klass);
static void	cd_client_init		(CdClient	*client);
static void	cd_client_finalize	(GObject	*object);

#define CD_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_CLIENT, CdClientPrivate))

#define CD_CLIENT_MESSAGE_TIMEOUT	15000 /* ms */

#define COLORD_DBUS_SERVICE		"org.freedesktop.ColorManager"
#define COLORD_DBUS_PATH		"/org/freedesktop/ColorManager"
#define COLORD_DBUS_INTERFACE		"org.freedesktop.ColorManager"

/**
 * CdClientPrivate:
 *
 * Private #CdClient data
 **/
struct _CdClientPrivate
{
	GDBusProxy		*proxy;
	gchar			*daemon_version;
};

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
	PROP_CONNECTED,
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
 * cd_client_get_daemon_version:
 * @client: a #CdClient instance.
 *
 * Get colord daemon version.
 *
 * Return value: string containing the daemon version, e.g. "0.1.0"
 *
 * Since: 0.1.0
 **/
const gchar *
cd_client_get_daemon_version (CdClient *client)
{
	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);
	return client->priv->daemon_version;
}

/**
 * cd_client_get_connected:
 * @client: a #CdClient instance.
 *
 * Gets if the client has been connected.
 *
 * Return value: %TRUE if properties are valid
 *
 * Since: 0.1.9
 **/
gboolean
cd_client_get_connected (CdClient *client)
{
	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	return client->priv->proxy != NULL;
}

/**********************************************************************/

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

	if (g_strcmp0 (signal_name, "Changed") == 0) {
		g_warning ("changed");
	} else if (g_strcmp0 (signal_name, "DeviceAdded") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		device = cd_device_new_with_object_path (object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_DEVICE_ADDED], 0,
			       device);
	} else if (g_strcmp0 (signal_name, "DeviceRemoved") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		device = cd_device_new_with_object_path (object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_DEVICE_REMOVED], 0,
			       device);
	} else if (g_strcmp0 (signal_name, "DeviceChanged") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		device = cd_device_new_with_object_path (object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_DEVICE_CHANGED], 0,
			       device);
	} else if (g_strcmp0 (signal_name, "ProfileAdded") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		profile = cd_profile_new_with_object_path (object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_PROFILE_ADDED], 0,
			       profile);
	} else if (g_strcmp0 (signal_name, "ProfileRemoved") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		profile = cd_profile_new_with_object_path (object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_PROFILE_REMOVED], 0,
			       profile);
	} else if (g_strcmp0 (signal_name, "ProfileChanged") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		profile = cd_profile_new_with_object_path (object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_PROFILE_CHANGED], 0,
			       profile);
	} else if (g_strcmp0 (signal_name, "SensorAdded") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		sensor = cd_sensor_new_with_object_path (object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_SENSOR_ADDED], 0,
			       sensor);
	} else if (g_strcmp0 (signal_name, "SensorRemoved") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		sensor = cd_sensor_new_with_object_path (object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_SENSOR_REMOVED], 0,
			       sensor);
	} else if (g_strcmp0 (signal_name, "SensorChanged") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		sensor = cd_sensor_new_with_object_path (object_path_tmp);
		g_signal_emit (client, signals[SIGNAL_SENSOR_CHANGED], 0,
			       sensor);
	} else {
		g_warning ("unhandled signal '%s'", signal_name);
	}
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
	/* daemon has quit, clearing caches */
}

/**********************************************************************/

/**********************************************************************/

/**
 * cd_client_connect_finish:
 * @client: a #CdClient instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: success
 *
 * Since: 0.1.6
 **/
gboolean
cd_client_connect_finish (CdClient *client,
			  GAsyncResult *res,
			  GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_simple_async_result_get_op_res_gboolean (simple);
}

static void
cd_client_connect_cb (GObject *source_object,
		      GAsyncResult *res,
		      gpointer user_data)
{
	GError *error = NULL;
	GVariant *daemon_version = NULL;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);
	CdClient *client = CD_CLIENT (g_async_result_get_source_object (G_ASYNC_RESULT (user_data)));

	/* get result */
	client->priv->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (client->priv->proxy == NULL) {
		g_simple_async_result_set_from_error (G_SIMPLE_ASYNC_RESULT (res_source),
						      error);
		g_simple_async_result_complete (G_SIMPLE_ASYNC_RESULT (res_source));
		g_error_free (error);
		goto out;
	}

	/* get daemon version */
	daemon_version = g_dbus_proxy_get_cached_property (client->priv->proxy,
							   CD_CLIENT_PROPERTY_DAEMON_VERSION);
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
	g_simple_async_result_set_op_res_gboolean (res_source, TRUE);
out:
	if (daemon_version != NULL)
		g_variant_unref (daemon_version);
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
}

/**
 * cd_client_connect:
 * @client: a #CdClient instance
 * @cancellable: a #GCancellable or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Connects to the colord daemon.
 *
 * Since: 0.1.6
 **/
void
cd_client_connect (CdClient *client,
		   GCancellable *cancellable,
		   GAsyncReadyCallback callback,
		   gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_connect);

	/* already connected */
	if (client->priv->proxy != NULL) {
		g_simple_async_result_set_op_res_gboolean (res, TRUE);
		g_simple_async_result_complete_in_idle (res);
		return;
	}

	/* connect async */
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  COLORD_DBUS_SERVICE,
				  COLORD_DBUS_PATH,
				  COLORD_DBUS_INTERFACE,
				  cancellable,
				  cd_client_connect_cb,
				  res);
}

/**********************************************************************/

/**
 * cd_client_create_device_finish:
 * @client: a #CdClient instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: (transfer full): a #CdDevice or %NULL
 *
 * Since: 0.1.8
 **/
CdDevice *
cd_client_create_device_finish (CdClient *client,
				GAsyncResult *res,
				GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_object_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

static void
cd_client_create_device_cb (GObject *source_object,
			    GAsyncResult *res,
			    gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	gchar *object_path = NULL;
	CdDevice *device;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_CLIENT_ERROR,
						 CD_CLIENT_ERROR_FAILED,
						 "Failed to CreateDevice: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* create CdDevice object */
	g_variant_get (result, "(o)",
		       &object_path);
	device = cd_device_new ();
	cd_device_set_object_path (device, object_path);

	/* success */
	g_simple_async_result_set_op_res_gpointer (res_source,
						   device,
						   (GDestroyNotify) g_object_ref);
	g_variant_unref (result);
out:
	g_free (object_path);
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
}

/**
 * cd_client_create_device:
 * @client: a #CdClient instance.
 * @id: identifier for the device
 * @scope: the scope of the device
 * @properties: properties to set on the device, or %NULL
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Creates a color device.
 *
 * Since: 0.1.8
 **/
void
cd_client_create_device (CdClient *client,
			 const gchar *id,
			 CdObjectScope scope,
			 GHashTable *properties,
			 GCancellable *cancellable,
			 GAsyncReadyCallback callback,
			 gpointer user_data)
{
	GSimpleAsyncResult *res;
	GVariantBuilder builder;
	GList *list, *l;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (client->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_create_device);

	/* add properties */
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	if (properties != NULL) {
		list = g_hash_table_get_keys (properties);
		for (l = list; l != NULL; l = l->next) {
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
				       CD_DEVICE_PROPERTY_KIND,
				       "unknown");
	}

	g_dbus_proxy_call (client->priv->proxy,
			   "CreateDevice",
			   g_variant_new ("(ssa{ss})",
					  id,
					  cd_object_scope_to_string (scope),
					  &builder),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_create_device_cb,
			   res);
}

/**********************************************************************/

/**
 * cd_client_create_profile_finish:
 * @client: a #CdClient instance.
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
cd_client_create_profile_finish (CdClient *client,
				 GAsyncResult *res,
				 GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	return g_object_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

static void
cd_client_create_profile_cb (GObject *source_object,
			     GAsyncResult *res,
			     gpointer user_data)
{
	GError *error = NULL;
	GDBusMessage *reply;
	gchar *object_path = NULL;
	CdProfile *profile;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);

	reply = g_dbus_connection_send_message_with_reply_finish (G_DBUS_CONNECTION (source_object),
								  res,
								  &error);
	if (reply == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_CLIENT_ERROR,
						 CD_CLIENT_ERROR_FAILED,
						 "Failed to CreateProfile: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* this is an error message */
	if (g_dbus_message_get_message_type (reply) == G_DBUS_MESSAGE_TYPE_ERROR) {
		g_dbus_message_to_gerror (reply, &error);
		g_simple_async_result_set_error (res_source,
						 CD_CLIENT_ERROR,
						 CD_CLIENT_ERROR_FAILED,
						 "Failed to CreateProfile: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* create thick CdDevice object */
	g_variant_get (g_dbus_message_get_body (reply), "(o)",
		       &object_path);
	profile = cd_profile_new ();
	cd_profile_set_object_path (profile, object_path);

	/* success */
	g_simple_async_result_set_op_res_gpointer (res_source,
						   profile,
						   (GDestroyNotify) g_object_ref);
out:
	if (reply != NULL)
		g_object_unref (reply);
	g_free (object_path);
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
}

/**
 * cd_client_create_profile:
 * @client: a #CdClient instance.
 * @id: identifier for the profile
 * @scope: the scope of the profile
 * @properties: properties to set on the profile, or %NULL
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Creates a color profile.
 *
 * Since: 0.1.8
 **/
void
cd_client_create_profile (CdClient *client,
			  const gchar *id,
			  CdObjectScope scope,
			  GHashTable *properties,
			  GCancellable *cancellable,
			  GAsyncReadyCallback callback,
			  gpointer user_data)
{
	const gchar *filename;
	GDBusConnection *connection;
	GDBusMessage *request = NULL;
	gint fd = -1;
	gint retval;
	GList *list, *l;
	GUnixFDList *fd_list = NULL;
	GVariant *body;
	GVariantBuilder builder;
	GSimpleAsyncResult *res;

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_create_profile);

	/* add properties */
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	if (properties != NULL &&
	    g_hash_table_size (properties) > 0) {
		list = g_hash_table_get_keys (properties);
		for (l = list; l != NULL; l = l->next) {
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
				       CD_PROFILE_PROPERTY_QUALIFIER,
				       "");
	}

	/* do low level call */
	request = g_dbus_message_new_method_call (COLORD_DBUS_SERVICE,
						  COLORD_DBUS_PATH,
						  COLORD_DBUS_INTERFACE,
						  "CreateProfile");

	/* get fd if possible top avoid open() in daemon */
	if (properties != NULL) {
		filename = g_hash_table_lookup (properties,
						CD_PROFILE_PROPERTY_FILENAME);
		if (filename != NULL) {
			fd = open (filename, O_RDONLY);
			if (fd < 0) {
				g_simple_async_result_set_error (res,
								 CD_CLIENT_ERROR,
								 CD_CLIENT_ERROR_FAILED,
								 "Failed to open %s",
								 filename);
				g_simple_async_result_complete_in_idle (res);
				goto out;
			}

			/* set out of band file descriptor */
			fd_list = g_unix_fd_list_new ();
			retval = g_unix_fd_list_append (fd_list, fd, NULL);
			g_assert (retval != -1);
			g_dbus_message_set_unix_fd_list (request, fd_list);

			/* g_unix_fd_list_append did a dup() already */
			close (fd);
		}
	}

	/* set parameters */
	body = g_variant_new ("(ssa{ss})",
			      id,
			      cd_object_scope_to_string (scope),
			      &builder);
	g_dbus_message_set_body (request, body);

	/* send sync message to the bus */
	connection = g_dbus_proxy_get_connection (client->priv->proxy);
	g_dbus_connection_send_message_with_reply (connection,
						   request,
						   G_DBUS_SEND_MESSAGE_FLAGS_NONE,
						   CD_CLIENT_MESSAGE_TIMEOUT,
						   NULL,
						   cancellable,
						   cd_client_create_profile_cb,
						   res);
out:
	if (fd_list != NULL)
		g_object_unref (fd_list);
	if (request != NULL)
		g_object_unref (request);
}

/**********************************************************************/

/**
 * cd_client_delete_device_finish:
 * @client: a #CdClient instance.
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
cd_client_delete_device_finish (CdClient *client,
				GAsyncResult *res,
				GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_simple_async_result_get_op_res_gboolean (simple);
}

static void
cd_client_delete_device_cb (GObject *source_object,
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
						 CD_CLIENT_ERROR,
						 CD_CLIENT_ERROR_FAILED,
						 "Failed to DeleteDevice: %s",
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
 * cd_client_delete_device:
 * @client: a #CdClient instance.
 * @device: a #CdDevice
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Deletes a device.
 *
 * Since: 0.1.8
 **/
void
cd_client_delete_device (CdClient *client,
			 CdDevice *device,
			 GCancellable *cancellable,
			 GAsyncReadyCallback callback,
			 gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (client->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_delete_device);
	g_dbus_proxy_call (client->priv->proxy,
			   "DeleteDevice",
			   g_variant_new ("(o)",
			   		  cd_device_get_object_path (device)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_delete_device_cb,
			   res);
}

/**********************************************************************/

/**
 * cd_client_delete_profile_finish:
 * @client: a #CdClient instance.
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
cd_client_delete_profile_finish (CdClient *client,
				 GAsyncResult *res,
				 GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_simple_async_result_get_op_res_gboolean (simple);
}

static void
cd_client_delete_profile_cb (GObject *source_object,
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
						 CD_CLIENT_ERROR,
						 CD_CLIENT_ERROR_FAILED,
						 "Failed to DeleteProfile: %s",
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
 * cd_client_delete_profile:
 * @client: a #CdClient instance.
 * @profile: a #CdProfile
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Deletes a profile.
 *
 * Since: 0.1.8
 **/
void
cd_client_delete_profile (CdClient *client,
			  CdProfile *profile,
			  GCancellable *cancellable,
			  GAsyncReadyCallback callback,
			  gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (client->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_delete_profile);
	g_dbus_proxy_call (client->priv->proxy,
			   "DeleteProfile",
			   g_variant_new ("(o)",
			   		  cd_profile_get_object_path (profile)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_delete_profile_cb,
			   res);
}

/**********************************************************************/

/**
 * cd_client_find_device_finish:
 * @client: a #CdClient instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: (transfer full): a #CdDevice or %NULL
 *
 * Since: 0.1.8
 **/
CdDevice *
cd_client_find_device_finish (CdClient *client,
			      GAsyncResult *res,
			      GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_object_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

static void
cd_client_find_device_cb (GObject *source_object,
			    GAsyncResult *res,
			    gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	CdDevice *device;
	gchar *object_path = NULL;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_CLIENT_ERROR,
						 CD_CLIENT_ERROR_FAILED,
						 "Failed to FindDeviceById: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* create a device object */
	g_variant_get (result, "(o)",
		       &object_path);
	device = cd_device_new ();
	cd_device_set_object_path (device, object_path);

	/* success */
	g_simple_async_result_set_op_res_gpointer (res_source,
						   device,
						   (GDestroyNotify) g_object_unref);
	g_variant_unref (result);
out:
	g_free (object_path);
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
}

/**
 * cd_client_find_device:
 * @client: a #CdClient instance.
 * @id: a device id
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Finds a device by an ID.
 *
 * Since: 0.1.8
 **/
void
cd_client_find_device (CdClient *client,
		       const gchar *id,
		       GCancellable *cancellable,
		       GAsyncReadyCallback callback,
		       gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (client->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_find_device);
	g_dbus_proxy_call (client->priv->proxy,
			   "FindDeviceById",
			   g_variant_new ("(s)", id),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_find_device_cb,
			   res);
}

/**********************************************************************/

/**
 * cd_client_find_device_by_property_finish:
 * @client: a #CdClient instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: (transfer full): a #CdDevice or %NULL
 *
 * Since: 0.1.8
 **/
CdDevice *
cd_client_find_device_by_property_finish (CdClient *client,
					  GAsyncResult *res,
					  GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_object_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

static void
cd_client_find_device_by_property_cb (GObject *source_object,
				      GAsyncResult *res,
				      gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	CdDevice *device;
	gchar *object_path = NULL;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_CLIENT_ERROR,
						 CD_CLIENT_ERROR_FAILED,
						 "Failed to FindDeviceByProperty: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* create a device object */
	g_variant_get (result, "(o)",
		       &object_path);
	device = cd_device_new ();
	cd_device_set_object_path (device, object_path);

	/* success */
	g_simple_async_result_set_op_res_gpointer (res_source,
						   device,
						   (GDestroyNotify) g_object_unref);
	g_variant_unref (result);
out:
	g_free (object_path);
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
}

/**
 * cd_client_find_device_by_property:
 * @client: a #CdClient instance.
 * @key: the device property key
 * @value: the device property value
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Finds a color device that has a property value.
 *
 * Since: 0.1.8
 **/
void
cd_client_find_device_by_property (CdClient *client,
				   const gchar *key,
				   const gchar *value,
				   GCancellable *cancellable,
				   GAsyncReadyCallback callback,
				   gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (client->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_find_device_by_property);
	g_dbus_proxy_call (client->priv->proxy,
			   "FindDeviceByProperty",
			   g_variant_new ("(ss)", key, value),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_find_device_by_property_cb,
			   res);
}

/**********************************************************************/

/**
 * cd_client_find_profile_finish:
 * @client: a #CdClient instance.
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
cd_client_find_profile_finish (CdClient *client,
			       GAsyncResult *res,
			       GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_object_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

static void
cd_client_find_profile_cb (GObject *source_object,
			   GAsyncResult *res,
			   gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	CdProfile *profile;
	gchar *object_path = NULL;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_CLIENT_ERROR,
						 CD_CLIENT_ERROR_FAILED,
						 "Failed to FindProfileById: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* create a profile object */
	g_variant_get (result, "(o)",
		       &object_path);
	profile = cd_profile_new ();
	cd_profile_set_object_path (profile, object_path);

	/* success */
	g_simple_async_result_set_op_res_gpointer (res_source,
						   profile,
						   (GDestroyNotify) g_object_unref);
	g_variant_unref (result);
out:
	g_free (object_path);
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
}

/**
 * cd_client_find_profile:
 * @client: a #CdClient instance.
 * @id: a profile id
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Finds a profile by an ID.
 *
 * Since: 0.1.8
 **/
void
cd_client_find_profile (CdClient *client,
			const gchar *id,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (client->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_find_profile);
	g_dbus_proxy_call (client->priv->proxy,
			   "FindProfileById",
			   g_variant_new ("(s)", id),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_find_profile_cb,
			   res);
}

/**********************************************************************/

/**
 * cd_client_find_profile_by_filename_finish:
 * @client: a #CdClient instance.
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
cd_client_find_profile_by_filename_finish (CdClient *client,
					   GAsyncResult *res,
					   GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_object_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

static void
cd_client_find_profile_by_filename_cb (GObject *source_object,
				       GAsyncResult *res,
				       gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	CdProfile *profile;
	gchar *object_path = NULL;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_CLIENT_ERROR,
						 CD_CLIENT_ERROR_FAILED,
						 "Failed to FindProfileByFilename: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* create a profile object */
	g_variant_get (result, "(o)",
		       &object_path);
	profile = cd_profile_new ();
	cd_profile_set_object_path (profile, object_path);

	/* success */
	g_simple_async_result_set_op_res_gpointer (res_source,
						   profile,
						   (GDestroyNotify) g_object_unref);
	g_variant_unref (result);
out:
	g_free (object_path);
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
}

/**
 * cd_client_find_profile_by_filename:
 * @client: a #CdClient instance.
 * @filename: a #profile filename
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Finds a profile by a filename.
 *
 * Since: 0.1.8
 **/
void
cd_client_find_profile_by_filename (CdClient *client,
				    const gchar *filename,
				    GCancellable *cancellable,
				    GAsyncReadyCallback callback,
				    gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (client->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_find_profile_by_filename);
	g_dbus_proxy_call (client->priv->proxy,
			   "FindProfileByFilename",
			   g_variant_new ("(s)", filename),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_find_profile_by_filename_cb,
			   res);
}

/**********************************************************************/

/**
 * cd_client_get_standard_space_finish:
 * @client: a #CdClient instance.
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
cd_client_get_standard_space_finish (CdClient *client,
				     GAsyncResult *res,
				     GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_object_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

static void
cd_client_get_standard_space_cb (GObject *source_object,
				 GAsyncResult *res,
				 gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	CdProfile *profile;
	gchar *object_path = NULL;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_CLIENT_ERROR,
						 CD_CLIENT_ERROR_FAILED,
						 "Failed to FindProfileById: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* create a profile object */
	g_variant_get (result, "(o)",
		       &object_path);
	profile = cd_profile_new ();
	cd_profile_set_object_path (profile, object_path);

	/* success */
	g_simple_async_result_set_op_res_gpointer (res_source,
						   profile,
						   (GDestroyNotify) g_object_unref);
	g_variant_unref (result);
out:
	g_free (object_path);
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
}

/**
 * cd_client_get_standard_space:
 * @client: a #CdStandardSpace instance.
 * @standard_space: a #profile id
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Finds a standard profile space.
 *
 * Since: 0.1.8
 **/
void
cd_client_get_standard_space (CdClient *client,
			      CdStandardSpace standard_space,
			      GCancellable *cancellable,
			      GAsyncReadyCallback callback,
			      gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (client->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_get_standard_space);
	g_dbus_proxy_call (client->priv->proxy,
			   "GetStandardSpace",
			   g_variant_new ("(s)",
			   		  cd_standard_space_to_string (standard_space)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_get_standard_space_cb,
			   res);
}

/**********************************************************************/

/**
 * cd_client_get_device_array_from_variant:
 **/
static GPtrArray *
cd_client_get_device_array_from_variant (CdClient *client,
					 GVariant *result)
{
	CdDevice *device;
	gchar *object_path_tmp;
	GPtrArray *array = NULL;
	guint i;
	guint len;
	GVariant *child = NULL;
	GVariantIter iter;

	/* add each device */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	child = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, child);
	for (i=0; i < len; i++) {
		g_variant_get_child (child, i,
				     "o", &object_path_tmp);
		device = cd_device_new_with_object_path (object_path_tmp);
		g_ptr_array_add (array, device);
		g_free (object_path_tmp);
	}
	g_variant_unref (child);
	return array;
}

/**
 * cd_client_get_devices_finish:
 * @client: a #CdClient instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: (element-type CdDevice) (transfer full): the devices
 *
 * Since: 0.1.8
 **/
GPtrArray *
cd_client_get_devices_finish (CdClient *client,
			      GAsyncResult *res,
			      GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_ptr_array_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

static void
cd_client_get_devices_cb (GObject *source_object,
			  GAsyncResult *res,
			  gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	GPtrArray *array;
	gchar *object_path = NULL;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);
	CdClient *client = CD_CLIENT (g_async_result_get_source_object (G_ASYNC_RESULT (res_source)));

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_CLIENT_ERROR,
						 CD_CLIENT_ERROR_FAILED,
						 "Failed to GetDevices: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* create a profile object */
	array = cd_client_get_device_array_from_variant (client, result);

	/* success */
	g_simple_async_result_set_op_res_gpointer (res_source,
						   array,
						   (GDestroyNotify) g_ptr_array_unref);
	g_variant_unref (result);
out:
	g_free (object_path);
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
}

/**
 * cd_client_get_devices:
 * @client: a #CdClient instance.
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Gets an array of color devices.
 *
 * Since: 0.1.8
 **/
void
cd_client_get_devices (CdClient *client,
		       GCancellable *cancellable,
		       GAsyncReadyCallback callback,
		       gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (client->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_get_devices);
	g_dbus_proxy_call (client->priv->proxy,
			   "GetDevices",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_get_devices_cb,
			   res);
}

/**********************************************************************/

/**
 * cd_client_get_devices_by_kind_finish:
 * @client: a #CdClient instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: (element-type CdDevice) (transfer full): the devices
 *
 * Since: 0.1.8
 **/
GPtrArray *
cd_client_get_devices_by_kind_finish (CdClient *client,
				      GAsyncResult *res,
				      GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_ptr_array_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

static void
cd_client_get_devices_by_kind_cb (GObject *source_object,
				  GAsyncResult *res,
				  gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	GPtrArray *array;
	gchar *object_path = NULL;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);
	CdClient *client = CD_CLIENT (g_async_result_get_source_object (G_ASYNC_RESULT (res_source)));

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_CLIENT_ERROR,
						 CD_CLIENT_ERROR_FAILED,
						 "Failed to GetDevicesByKind: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* create a profile object */
	array = cd_client_get_device_array_from_variant (client, result);

	/* success */
	g_simple_async_result_set_op_res_gpointer (res_source,
						   array,
						   (GDestroyNotify) g_ptr_array_unref);
	g_variant_unref (result);
out:
	g_free (object_path);
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
}

/**
 * cd_client_get_devices_by_kind:
 * @client: a #CdClient instance.
 * @kind: the type of device.
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Gets an array of color devices.
 *
 * Since: 0.1.8
 **/
void
cd_client_get_devices_by_kind (CdClient *client,
			       CdDeviceKind kind,
			       GCancellable *cancellable,
			       GAsyncReadyCallback callback,
			       gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (client->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_get_devices_by_kind);
	g_dbus_proxy_call (client->priv->proxy,
			   "GetDevicesByKind",
			   g_variant_new ("(s)",
			   		  cd_device_kind_to_string (kind)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_get_devices_by_kind_cb,
			   res);
}

/**********************************************************************/

/**
 * cd_client_get_profile_array_from_variant:
 **/
static GPtrArray *
cd_client_get_profile_array_from_variant (CdClient *client,
					 GVariant *result)
{
	CdProfile *profile;
	gchar *object_path_tmp;
	GPtrArray *array = NULL;
	guint i;
	guint len;
	GVariant *child = NULL;
	GVariantIter iter;

	/* add each profile */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	child = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, child);
	for (i=0; i < len; i++) {
		g_variant_get_child (child, i,
				     "o", &object_path_tmp);
		profile = cd_profile_new_with_object_path (object_path_tmp);
		g_ptr_array_add (array, profile);
		g_free (object_path_tmp);
	}
	g_variant_unref (child);
	return array;
}

/**
 * cd_client_get_profiles_finish:
 * @client: a #CdClient instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: (element-type CdProfile) (transfer full): the profiles
 *
 * Since: 0.1.8
 **/
GPtrArray *
cd_client_get_profiles_finish (CdClient *client,
			       GAsyncResult *res,
			       GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_ptr_array_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

static void
cd_client_get_profiles_cb (GObject *source_object,
			   GAsyncResult *res,
			   gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	GPtrArray *array;
	gchar *object_path = NULL;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);
	CdClient *client = CD_CLIENT (g_async_result_get_source_object (G_ASYNC_RESULT (res_source)));

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_CLIENT_ERROR,
						 CD_CLIENT_ERROR_FAILED,
						 "Failed to GetProfiles: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* create a profile object */
	array = cd_client_get_profile_array_from_variant (client, result);

	/* success */
	g_simple_async_result_set_op_res_gpointer (res_source,
						   array,
						   (GDestroyNotify) g_ptr_array_unref);
	g_variant_unref (result);
out:
	g_free (object_path);
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
}

/**
 * cd_client_get_profiles:
 * @client: a #CdClient instance.
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Gets an array of color profiles.
 *
 * Since: 0.1.8
 **/
void
cd_client_get_profiles (CdClient *client,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (client->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_get_profiles);
	g_dbus_proxy_call (client->priv->proxy,
			   "GetProfiles",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_get_profiles_cb,
			   res);
}

/**********************************************************************/

/**
 * cd_client_get_sensor_array_from_variant:
 **/
static GPtrArray *
cd_client_get_sensor_array_from_variant (CdClient *client,
					 GVariant *result)
{
	CdSensor *sensor;
	gchar *object_path_tmp;
	GPtrArray *array = NULL;
	guint i;
	guint len;
	GVariant *child = NULL;
	GVariantIter iter;

	/* add each sensor */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	child = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, child);
	for (i=0; i < len; i++) {
		g_variant_get_child (child, i,
				     "o", &object_path_tmp);
		sensor = cd_sensor_new_with_object_path (object_path_tmp);
		g_ptr_array_add (array, sensor);
		g_free (object_path_tmp);
	}
	g_variant_unref (child);
	return array;
}

/**
 * cd_client_get_sensors_finish:
 * @client: a #CdClient instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: (element-type CdSensor) (transfer full): the sensors
 *
 * Since: 0.1.8
 **/
GPtrArray *
cd_client_get_sensors_finish (CdClient *client,
			      GAsyncResult *res,
			      GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_ptr_array_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

static void
cd_client_get_sensors_cb (GObject *source_object,
			  GAsyncResult *res,
			  gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	GPtrArray *array;
	gchar *object_path = NULL;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);
	CdClient *client = CD_CLIENT (g_async_result_get_source_object (G_ASYNC_RESULT (res_source)));

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_CLIENT_ERROR,
						 CD_CLIENT_ERROR_FAILED,
						 "Failed to GetSensors: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* create a sensor object */
	array = cd_client_get_sensor_array_from_variant (client, result);

	/* success */
	g_simple_async_result_set_op_res_gpointer (res_source,
						   array,
						   (GDestroyNotify) g_ptr_array_unref);
	g_variant_unref (result);
out:
	g_free (object_path);
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
}

/**
 * cd_client_get_sensors:
 * @client: a #CdClient instance.
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Gets an array of color sensors.
 *
 * Since: 0.1.8
 **/
void
cd_client_get_sensors (CdClient *client,
		       GCancellable *cancellable,
		       GAsyncReadyCallback callback,
		       gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (client->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (client),
					 callback,
					 user_data,
					 cd_client_get_sensors);
	g_dbus_proxy_call (client->priv->proxy,
			   "GetSensors",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_get_sensors_cb,
			   res);
}

/**********************************************************************/

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
	case PROP_CONNECTED:
		g_value_set_boolean (value, client->priv->proxy != NULL);
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
	 * CdClient:connected:
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

