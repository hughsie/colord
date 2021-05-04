/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2013 Richard Hughes <richard@hughsie.com>
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
#ifdef __unix__
#include <gio/gunixfdlist.h>
#endif
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

#define GET_PRIVATE(o) (cd_client_get_instance_private (o))

#define CD_CLIENT_MESSAGE_TIMEOUT	15000 /* ms */
#define CD_CLIENT_IMPORT_DAEMON_TIMEOUT	5000 /* ms */
#define COLORD_DBUS_SERVICE		"org.freedesktop.ColorManager"
#define COLORD_DBUS_PATH		"/org/freedesktop/ColorManager"
#define COLORD_DBUS_INTERFACE		"org.freedesktop.ColorManager"

/**
 * CdClientPrivate:
 *
 * Private #CdClient data
 **/
typedef struct
{
	GDBusProxy		*proxy;
	gchar			*daemon_version;
	gchar			*system_vendor;
	gchar			*system_model;
} CdClientPrivate;

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
	PROP_SYSTEM_VENDOR,
	PROP_SYSTEM_MODEL,
	PROP_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };
static gpointer cd_client_object = NULL;

G_DEFINE_TYPE_WITH_PRIVATE (CdClient, cd_client, G_TYPE_OBJECT)

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
	if (!quark) {
		quark = g_quark_from_static_string ("cd_client_error");
		g_dbus_error_register_error (quark,
					     CD_CLIENT_ERROR_INTERNAL,
					     COLORD_DBUS_SERVICE ".Failed");
		g_dbus_error_register_error (quark,
					     CD_CLIENT_ERROR_ALREADY_EXISTS,
					     COLORD_DBUS_SERVICE ".AlreadyExists");
	}
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->daemon_version;
}

/**
 * cd_client_get_system_vendor:
 * @client: a #CdClient instance.
 *
 * Get system vendor.
 *
 * Return value: string containing the system vendor, e.g. "Lenovo"
 *
 * Since: 1.0.2
 **/
const gchar *
cd_client_get_system_vendor (CdClient *client)
{
	CdClientPrivate *priv = GET_PRIVATE (client);
	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->system_vendor;
}

/**
 * cd_client_get_system_model:
 * @client: a #CdClient instance.
 *
 * Get system model.
 *
 * Return value: string containing the system model, e.g. "T61"
 *
 * Since: 1.0.2
 **/
const gchar *
cd_client_get_system_model (CdClient *client)
{
	CdClientPrivate *priv = GET_PRIVATE (client);
	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->system_model;
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	return priv->proxy != NULL;
}

/**
 * cd_client_get_has_server:
 * @client: a #CdClient instance.
 *
 * Gets if the colord server is currently running.
 * WARNING: This function may block for up to 5 seconds waiting for the daemon
 * to start if it is not already running.
 *
 * Return value: %TRUE if the colord process is running
 *
 * Since: 0.1.12
 **/
gboolean
cd_client_get_has_server (CdClient *client)
{
	g_autofree gchar *name_owner = NULL;
	g_autoptr(GDBusProxy) proxy = NULL;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);

	/* get name owner */
	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					       G_DBUS_PROXY_FLAGS_NONE,
					       NULL,
					       COLORD_DBUS_SERVICE,
					       COLORD_DBUS_PATH,
					       COLORD_DBUS_INTERFACE,
					       NULL,
					       NULL);
	if (proxy == NULL)
		return FALSE;
	name_owner = g_dbus_proxy_get_name_owner (proxy);
	if (name_owner == NULL)
		return FALSE;

	/* just assume it's ready for use */
	return TRUE;
}

/**********************************************************************/

static void
cd_client_dbus_signal_cb (GDBusProxy *proxy,
			  gchar      *sender_name,
			  gchar      *signal_name,
			  GVariant   *parameters,
			  CdClient   *client)
{
	g_autofree gchar *object_path_tmp = NULL;
	g_autoptr(CdDevice) device = NULL;
	g_autoptr(CdProfile) profile = NULL;
	g_autoptr(CdSensor) sensor = NULL;

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
}

static void
cd_client_owner_notify_cb (GObject *object,
			   GParamSpec *pspec,
			   CdClient *client)
{
	/* daemon has quit, clearing caches */
}

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
	g_return_val_if_fail (g_task_is_valid (res, client), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_client_connect_cb (GObject *source_object,
		      GAsyncResult *res,
		      gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) daemon_version = NULL;
	g_autoptr(GVariant) system_model = NULL;
	g_autoptr(GVariant) system_vendor = NULL;
	CdClient *client = CD_CLIENT (g_task_get_source_object (task));
	CdClientPrivate *priv = GET_PRIVATE (client);

	/* get result */
	priv->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (priv->proxy == NULL) {
		g_task_return_new_error (task,
					 CD_CLIENT_ERROR,
					 CD_CLIENT_ERROR_INTERNAL,
					 "%s",
					 error->message);
		return;
	}

	/* get daemon version */
	daemon_version = g_dbus_proxy_get_cached_property (priv->proxy,
							   CD_CLIENT_PROPERTY_DAEMON_VERSION);
	if (daemon_version != NULL) {
		g_free (priv->daemon_version);
		priv->daemon_version = g_variant_dup_string (daemon_version, NULL);
	}

	/* get system info */
	system_vendor = g_dbus_proxy_get_cached_property (priv->proxy,
							  CD_CLIENT_PROPERTY_SYSTEM_VENDOR);
	if (system_vendor != NULL) {
		g_free (priv->system_vendor);
		priv->system_vendor = g_variant_dup_string (system_vendor, NULL);
	}

	/* get system model */
	system_model = g_dbus_proxy_get_cached_property (priv->proxy,
							 CD_CLIENT_PROPERTY_SYSTEM_MODEL);
	if (system_model != NULL) {
		g_free (priv->system_model);
		priv->system_model = g_variant_dup_string (system_model, NULL);
	}

	/* get signals from DBus */
	g_signal_connect_object (priv->proxy,
				 "g-signal",
				 G_CALLBACK (cd_client_dbus_signal_cb),
				 client, 0);

	/* watch to see if it's fallen off the bus */
	g_signal_connect_object (priv->proxy,
				 "notify::g-name-owner",
				 G_CALLBACK (cd_client_owner_notify_cb),
				 client, 0);

	/* success */
	g_task_return_boolean (task, TRUE);
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);

	/* already connected */
	if (priv->proxy != NULL) {
		g_task_return_boolean (task, TRUE);
		g_object_unref (task);
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
				  task);
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
	g_return_val_if_fail (g_task_is_valid (res, client), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_client_fixup_dbus_error (GError *error)
{
	g_autofree gchar *name = NULL;

	g_return_if_fail (error != NULL);

	/* is a remote error? */
	if (!g_dbus_error_is_remote_error (error))
		return;

	/* parse the remote error */
	name = g_dbus_error_get_remote_error (error);
	error->domain = CD_CLIENT_ERROR;
	error->code = cd_client_error_from_string (name);
	g_dbus_error_strip_remote_error (error);
}

static void
cd_client_create_device_cb (GObject *source_object,
			    GAsyncResult *res,
			    gpointer user_data)
{
	CdDevice *device;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *object_path = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create CdDevice object */
	g_variant_get (result, "(o)", &object_path);
	device = cd_device_new ();
	cd_device_set_object_path (device, object_path);

	/* success */
	g_task_return_pointer (task, device, (GDestroyNotify) g_object_unref);
}

/**
 * cd_client_create_device:
 * @client: a #CdClient instance.
 * @id: identifier for the device
 * @scope: the scope of the device
 * @properties: (element-type utf8 utf8) (allow-none): properties to
 *   set on the device, or %NULL
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	const gchar *value;
	GTask *task = NULL;
	GVariantBuilder builder;
	GList *list, *l;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);

	/* add properties */
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	if (properties != NULL) {
		list = g_hash_table_get_keys (properties);
		for (l = list; l != NULL; l = l->next) {
			value = g_hash_table_lookup (properties, l->data);
			g_variant_builder_add (&builder,
					       "{ss}",
					       l->data,
					       value != NULL ? value : "");
		}
		g_list_free (list);
	} else {
		/* just fake something here */
		g_variant_builder_add (&builder,
				       "{ss}",
				       CD_DEVICE_PROPERTY_KIND,
				       "unknown");
	}

	g_dbus_proxy_call (priv->proxy,
			   "CreateDevice",
			   g_variant_new ("(ssa{ss})",
					  id,
					  cd_object_scope_to_string (scope),
					  &builder),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_create_device_cb,
			   task);
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
	g_return_val_if_fail (g_task_is_valid (res, client), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_client_create_profile_cb (GObject *source_object,
			     GAsyncResult *res,
			     gpointer user_data)
{
	CdProfile *profile;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *object_path = NULL;
	g_autoptr(GDBusMessage) reply = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);

	reply = g_dbus_connection_send_message_with_reply_finish (G_DBUS_CONNECTION (source_object),
								  res,
								  &error);
	if (reply == NULL) {
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* this is an error message */
	if (g_dbus_message_get_message_type (reply) == G_DBUS_MESSAGE_TYPE_ERROR) {
		g_dbus_message_to_gerror (reply, &error);
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create thick CdDevice object */
	g_variant_get (g_dbus_message_get_body (reply), "(o)",
		       &object_path);
	profile = cd_profile_new ();
	cd_profile_set_object_path (profile, object_path);

	/* success */
	g_task_return_pointer (task, profile, (GDestroyNotify) g_object_unref);
}

/**
 * cd_client_create_profile:
 * @client: a #CdClient instance.
 * @id: identifier for the profile
 * @scope: the scope of the profile
 * @properties: (element-type utf8 utf8) (allow-none): properties to
 *   set on the profile, or %NULL
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	GDBusConnection *connection;
	gint fd = -1;
	GList *list, *l;
	GVariant *body;
	GVariantBuilder builder;
	GTask *task = NULL;
	g_autoptr(GDBusMessage) request = NULL;
	g_autoptr(GUnixFDList) fd_list = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);
	g_return_if_fail (id != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);

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
						  "CreateProfileWithFd");

	/* get fd if possible top avoid open() in daemon */
#ifdef __unix__
	if (properties != NULL) {
		const gchar *filename;
		filename = g_hash_table_lookup (properties,
						CD_PROFILE_PROPERTY_FILENAME);
		if (filename != NULL) {
			gint retval;
			fd = open (filename, O_RDONLY);
			if (fd < 0) {
				g_task_return_new_error (task,
							 CD_CLIENT_ERROR,
							 CD_CLIENT_ERROR_INTERNAL,
							 "Failed to open %s",
							 filename);
				return;
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
#endif

	/* set parameters */
	body = g_variant_new ("(ssha{ss})",
			      id,
			      cd_object_scope_to_string (scope),
			      fd > -1 ? 0 : -1,
			      &builder);
	g_dbus_message_set_body (request, body);

	/* send sync message to the bus */
	connection = g_dbus_proxy_get_connection (priv->proxy);
	g_dbus_connection_send_message_with_reply (connection,
						   request,
						   G_DBUS_SEND_MESSAGE_FLAGS_NONE,
						   CD_CLIENT_MESSAGE_TIMEOUT,
						   NULL,
						   cancellable,
						   cd_client_create_profile_cb,
						   task);
}

/**********************************************************************/
/**
 * cd_client_create_profile_for_icc:
 * @client: a #CdClient instance.
 * @icc: #CdIcc object
 * @scope: the scope of the profile
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Creates a color profile for an #CdIcc Object.
 *
 * Since: 1.1.1
 **/
void
cd_client_create_profile_for_icc (CdClient *client,
				  CdIcc *icc,
				  CdObjectScope scope,
				  GCancellable *cancellable,
				  GAsyncReadyCallback callback,
				  gpointer user_data)
{
	const gchar *checksum;
	const gchar *filename;
	g_autofree gchar *profile_id = NULL;
	g_autoptr(GHashTable) profile_props = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (CD_IS_ICC (icc));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	/* generate ID */
	checksum = cd_icc_get_checksum (icc);
	filename = cd_icc_get_filename (icc);
	profile_id = g_strdup_printf ("icc-%s", checksum);
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       NULL, NULL);
	g_hash_table_insert (profile_props,
			     (gpointer) CD_PROFILE_PROPERTY_FILENAME,
			     (gpointer) filename);
	g_hash_table_insert (profile_props,
			     (gpointer) CD_PROFILE_METADATA_FILE_CHECKSUM,
			     (gpointer) checksum);
	cd_client_create_profile (client,
				  profile_id,
				  scope,
				  profile_props,
				  NULL,
				  callback,
				  user_data);
}


/**
 * cd_client_create_profile_for_icc_finish:
 * @client: a #CdClient instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: (transfer full): a #CdProfile or %NULL
 *
 * Since: 1.1.1
 **/
CdProfile *
cd_client_create_profile_for_icc_finish (CdClient *client,
					 GAsyncResult *res,
					 GError **error)
{
	return cd_client_create_profile_finish (client, res, error);
}

/**********************************************************************/

static GFile *
cd_client_import_get_profile_destination (GFile *file)
{
	g_autofree gchar *basename = NULL;
	g_autofree gchar *destination = NULL;

	g_return_val_if_fail (file != NULL, NULL);

	/* get destination filename for this source file */
	basename = g_file_get_basename (file);
	destination = g_build_filename (g_get_user_data_dir (), "icc", basename, NULL);
	return g_file_new_for_path (destination);
}

static gboolean
cd_client_import_mkdir_and_copy (GFile *source,
				 GFile *destination,
				 GCancellable *cancellable,
				 GError **error)
{
	g_autoptr(GFile) parent = NULL;

	g_return_val_if_fail (source != NULL, FALSE);
	g_return_val_if_fail (destination != NULL, FALSE);

	/* get parent */
	parent = g_file_get_parent (destination);

	/* create directory */
	if (!g_file_query_exists (parent, cancellable)) {
		if (!g_file_make_directory_with_parents (parent, cancellable, error))
			return FALSE;
	}

	/* do the copy */
	return g_file_copy (source, destination,
			    G_FILE_COPY_OVERWRITE,
			    cancellable, NULL, NULL, error);
}

typedef struct {
	GFile			*dest;
	GFile			*file;
	guint			 hangcheck_id;
	guint			 profile_added_id;
} CdClientImportTaskData;

/**
 * cd_client_import_profile_finish:
 * @client: a #CdClient instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: (transfer full): a #CdProfile or %NULL
 *
 * Since: 0.1.12
 **/
CdProfile *
cd_client_import_profile_finish (CdClient *client,
				 GAsyncResult *res,
				 GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, client), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_client_import_task_data_free (CdClientImportTaskData *tdata)
{
	g_object_unref (tdata->file);
	g_object_unref (tdata->dest);
//	if (tdata->profile_added_id > 0)
//		g_signal_handler_disconnect (tdata->client, tdata->profile_added_id);
	if (tdata->hangcheck_id > 0)
		g_source_remove (tdata->hangcheck_id);
	g_free (tdata);
}

static void
cd_client_import_profile_added_cb (CdClient *client,
				   CdProfile *profile,
				   gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_task_return_pointer (task, g_object_ref (profile), (GDestroyNotify) g_object_unref);
	g_object_unref (task);
}

static gboolean
cd_client_import_hangcheck_cb (gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	CdClientImportTaskData *tdata = g_task_get_task_data (task);
	g_task_return_new_error (task,
				 CD_CLIENT_ERROR,
				 CD_CLIENT_ERROR_INTERNAL,
				 "The profile was not added in time");
	tdata->hangcheck_id = 0;
	g_object_unref (task);
	return G_SOURCE_REMOVE;
}

static void
cd_client_import_profile_find_filename_cb (GObject *source_object,
					   GAsyncResult *res,
					   gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	CdClient *client = CD_CLIENT (source_object);
	CdClientImportTaskData *tdata = g_task_get_task_data (task);
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(CdProfile) profile = NULL;

	/* does the profile already exist */
	profile = cd_client_find_profile_by_filename_finish (client, res, &error);
	if (profile != NULL) {
		g_autofree gchar *filename = NULL;
		filename = g_file_get_path (tdata->dest);
		g_task_return_new_error (task,
					 CD_CLIENT_ERROR,
					 CD_CLIENT_ERROR_ALREADY_EXISTS,
					 "The profile %s already exists",
					 filename);
		g_object_unref (task);
		return;
	}
	if (!g_error_matches (error,
			      CD_CLIENT_ERROR,
			      CD_CLIENT_ERROR_NOT_FOUND)) {
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	/* reset the error */
	g_clear_error (&error);

	/* watch for a new profile to be detected and added,
	 * but time out after a couple of seconds */
	tdata->hangcheck_id = g_timeout_add (CD_CLIENT_IMPORT_DAEMON_TIMEOUT,
					     cd_client_import_hangcheck_cb,
					     task);
	tdata->profile_added_id = g_signal_connect (client, "profile-added",
						    G_CALLBACK (cd_client_import_profile_added_cb),
						    task);

	/* copy profile to the correct place */
	ret = cd_client_import_mkdir_and_copy (tdata->file,
					       tdata->dest,
					       g_task_get_cancellable (task),
					       &error);
	if (!ret) {
		g_task_return_new_error (task,
					 CD_CLIENT_ERROR,
					 CD_CLIENT_ERROR_INTERNAL,
					 "Failed to copy: %s",
					 error->message);
		g_object_unref (task);
	}
}

static void
cd_client_import_profile_query_info_cb (GObject *source_object,
					GAsyncResult *res,
					gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	CdClient *client = CD_CLIENT (g_task_get_source_object (task));
	CdClientImportTaskData *tdata = g_task_get_task_data (task);
	const gchar *type;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *filename = NULL;
	g_autoptr(GFileInfo) info = NULL;

	/* get the file info */
	filename = g_file_get_path (tdata->dest);
	info = g_file_query_info_finish (G_FILE (source_object),
					 res,
					 &error);
	if (info == NULL) {
		g_task_return_new_error (task,
					 CD_CLIENT_ERROR,
					 CD_CLIENT_ERROR_INTERNAL,
					 "Cannot get content type for %s: %s",
					 filename,
					 error->message);
		g_object_unref (task);
		return;
	}

	/* check the content type */
	type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	if (g_strcmp0 (type, "application/vnd.iccprofile") != 0) {
		g_task_return_new_error (task,
					 CD_CLIENT_ERROR,
					 CD_CLIENT_ERROR_FILE_INVALID,
					 "Incorrect content type for %s, got %s",
					 filename, type);
		g_object_unref (task);
		return;
	}

	/* does this profile already exist? */
	cd_client_find_profile_by_filename (client,
					    filename,
					    g_task_get_cancellable (task),
					    cd_client_import_profile_find_filename_cb,
					    task);
}

/**
 * cd_client_import_profile:
 * @client: a #CdClient instance.
 * @file: a #GFile
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Imports a color profile into the users home directory.
 *
 * If the profile should be accessible for all users, then call
 * cd_profile_install_system_wide() on the result.
 *
 * Since: 0.1.12
 **/
void
cd_client_import_profile (CdClient *client,
			  GFile *file,
			  GCancellable *cancellable,
			  GAsyncReadyCallback callback,
			  gpointer user_data)
{
	CdClientImportTaskData *tdata;
	GTask *task;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (G_IS_FILE (file));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);
	tdata = g_new0 (CdClientImportTaskData, 1);
	tdata->file = g_object_ref (file);
	tdata->dest = cd_client_import_get_profile_destination (file);
	g_task_set_task_data (task, tdata, (GDestroyNotify) cd_client_import_task_data_free);

	/* check the file really is an ICC file */
	g_file_query_info_async (tdata->file,
				 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				 G_FILE_QUERY_INFO_NONE,
				 G_PRIORITY_DEFAULT,
				 cancellable,
				 cd_client_import_profile_query_info_cb,
				 task);
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
	g_return_val_if_fail (g_task_is_valid (res, client), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_client_delete_device_cb (GObject *source_object,
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
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (CD_IS_DEVICE (device));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "DeleteDevice",
			   g_variant_new ("(o)",
			   		  cd_device_get_object_path (device)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_delete_device_cb,
			   task);
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
	g_return_val_if_fail (g_task_is_valid (res, client), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_client_delete_profile_cb (GObject *source_object,
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
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (CD_IS_PROFILE (profile));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "DeleteProfile",
			   g_variant_new ("(o)",
			   		  cd_profile_get_object_path (profile)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_delete_profile_cb,
			   task);
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
	g_return_val_if_fail (g_task_is_valid (res, client), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_client_find_device_cb (GObject *source_object,
			    GAsyncResult *res,
			    gpointer user_data)
{
	CdDevice *device;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *object_path = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create a device object */
	g_variant_get (result, "(o)", &object_path);
	device = cd_device_new ();
	cd_device_set_object_path (device, object_path);

	/* success */
	g_task_return_pointer (task, device, (GDestroyNotify) g_object_unref);
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (id != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "FindDeviceById",
			   g_variant_new ("(s)", id),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_find_device_cb,
			   task);
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
	g_return_val_if_fail (g_task_is_valid (res, client), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_client_find_device_by_property_cb (GObject *source_object,
				      GAsyncResult *res,
				      gpointer user_data)
{
	CdDevice *device;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *object_path = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create a device object */
	g_variant_get (result, "(o)", &object_path);
	device = cd_device_new ();
	cd_device_set_object_path (device, object_path);

	/* success */
	g_task_return_pointer (task, device, (GDestroyNotify) g_object_unref);
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (key != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "FindDeviceByProperty",
			   g_variant_new ("(ss)", key, value),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_find_device_by_property_cb,
			   task);
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
	g_return_val_if_fail (g_task_is_valid (res, client), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_client_find_profile_cb (GObject *source_object,
			   GAsyncResult *res,
			   gpointer user_data)
{
	CdProfile *profile;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *object_path = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create a profile object */
	g_variant_get (result, "(o)", &object_path);
	profile = cd_profile_new ();
	cd_profile_set_object_path (profile, object_path);

	/* success */
	g_task_return_pointer (task, profile, (GDestroyNotify) g_object_unref);
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (id != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "FindProfileById",
			   g_variant_new ("(s)", id),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_find_profile_cb,
			   task);
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
	g_return_val_if_fail (g_task_is_valid (res, client), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_client_find_profile_by_filename_cb (GObject *source_object,
				       GAsyncResult *res,
				       gpointer user_data)
{
	CdProfile *profile;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *object_path = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create a profile object */
	g_variant_get (result, "(o)", &object_path);
	profile = cd_profile_new ();
	cd_profile_set_object_path (profile, object_path);

	/* success */
	g_task_return_pointer (task, profile, (GDestroyNotify) g_object_unref);
}

/**
 * cd_client_find_profile_by_filename:
 * @client: a #CdClient instance.
 * @filename: a profile filename
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (filename != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "FindProfileByFilename",
			   g_variant_new ("(s)", filename),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_find_profile_by_filename_cb,
			   task);
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
	g_return_val_if_fail (g_task_is_valid (res, client), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_client_get_standard_space_cb (GObject *source_object,
				 GAsyncResult *res,
				 gpointer user_data)
{
	CdProfile *profile;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *object_path = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create a profile object */
	g_variant_get (result, "(o)", &object_path);
	profile = cd_profile_new ();
	cd_profile_set_object_path (profile, object_path);

	/* success */
	g_task_return_pointer (task, profile, (GDestroyNotify) g_object_unref);
}

/**
 * cd_client_get_standard_space:
 * @client: a #CdStandardSpace instance.
 * @standard_space: a profile id
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "GetStandardSpace",
			   g_variant_new ("(s)",
			   		  cd_standard_space_to_string (standard_space)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_get_standard_space_cb,
			   task);
}

/**********************************************************************/

static GPtrArray *
cd_client_get_device_array_from_variant (CdClient *client,
					 GVariant *result)
{
	CdDevice *device;
	GPtrArray *array = NULL;
	GVariantIter iter;
	guint i;
	guint len;
	g_autoptr(GVariant) child = NULL;

	/* add each device */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	child = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, child);
	for (i = 0; i < len; i++) {
		g_autofree gchar *object_path_tmp = NULL;
		g_variant_get_child (child, i,
				     "o", &object_path_tmp);
		device = cd_device_new_with_object_path (object_path_tmp);
		g_ptr_array_add (array, device);
	}
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
	g_return_val_if_fail (g_task_is_valid (res, client), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_client_get_devices_cb (GObject *source_object,
			  GAsyncResult *res,
			  gpointer user_data)
{
	CdClient *client;
	GPtrArray *array;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *object_path = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	client = CD_CLIENT (g_task_get_source_object (task));
	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create a profile object */
	array = cd_client_get_device_array_from_variant (client, result);

	/* success */
	g_task_return_pointer (task, array, (GDestroyNotify) g_ptr_array_unref);
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "GetDevices",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_get_devices_cb,
			   task);
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
	g_return_val_if_fail (g_task_is_valid (res, client), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_client_get_devices_by_kind_cb (GObject *source_object,
				  GAsyncResult *res,
				  gpointer user_data)
{
	CdClient *client;
	GPtrArray *array;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *object_path = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	client = CD_CLIENT (g_task_get_source_object (task));
	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create a profile object */
	array = cd_client_get_device_array_from_variant (client, result);

	/* success */
	g_task_return_pointer (task, array, (GDestroyNotify) g_ptr_array_unref);
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "GetDevicesByKind",
			   g_variant_new ("(s)",
			   		  cd_device_kind_to_string (kind)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_get_devices_by_kind_cb,
			   task);
}

/**********************************************************************/

static GPtrArray *
cd_client_get_profile_array_from_variant (CdClient *client,
					 GVariant *result)
{
	CdProfile *profile;
	GPtrArray *array = NULL;
	GVariantIter iter;
	guint i;
	guint len;
	g_autoptr(GVariant) child = NULL;

	/* add each profile */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	child = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, child);
	for (i = 0; i < len; i++) {
		g_autofree gchar *object_path_tmp = NULL;
		g_variant_get_child (child, i,
				     "o", &object_path_tmp);
		profile = cd_profile_new_with_object_path (object_path_tmp);
		g_ptr_array_add (array, profile);
	}
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
 * Return value: (element-type CdProfile) (transfer container): the profiles
 *
 * Since: 0.1.8
 **/
GPtrArray *
cd_client_get_profiles_finish (CdClient *client,
			       GAsyncResult *res,
			       GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, client), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_client_get_profiles_cb (GObject *source_object,
			   GAsyncResult *res,
			   gpointer user_data)
{
	CdClient *client;
	GPtrArray *array;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *object_path = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	client = CD_CLIENT (g_task_get_source_object (task));
	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create a profile object */
	array = cd_client_get_profile_array_from_variant (client, result);

	/* success */
	g_task_return_pointer (task, array, (GDestroyNotify) g_ptr_array_unref);
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "GetProfiles",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_get_profiles_cb,
			   task);
}

/**********************************************************************/

static GPtrArray *
cd_client_get_sensor_array_from_variant (CdClient *client,
					 GVariant *result)
{
	CdSensor *sensor;
	GPtrArray *array = NULL;
	GVariantIter iter;
	guint i;
	guint len;
	g_autoptr(GVariant) child = NULL;

	/* add each sensor */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	child = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, child);
	for (i = 0; i < len; i++) {
		g_autofree gchar *object_path_tmp = NULL;
		g_variant_get_child (child, i,
				     "o", &object_path_tmp);
		sensor = cd_sensor_new_with_object_path (object_path_tmp);
		g_ptr_array_add (array, sensor);
	}
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
	g_return_val_if_fail (g_task_is_valid (res, client), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_client_get_sensors_cb (GObject *source_object,
			  GAsyncResult *res,
			  gpointer user_data)
{
	GPtrArray *array;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *object_path = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;
	CdClient *client = CD_CLIENT (g_task_get_source_object (task));

	client = CD_CLIENT (g_task_get_source_object (task));
	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create a sensor object */
	array = cd_client_get_sensor_array_from_variant (client, result);

	/* success */
	g_task_return_pointer (task, array, (GDestroyNotify) g_ptr_array_unref);
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
	CdClientPrivate *priv = GET_PRIVATE (client);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "GetSensors",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_get_sensors_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_client_find_profile_by_property_finish:
 * @client: a #CdClient instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: (transfer full): a #CdProfile or %NULL
 *
 * Since: 0.1.24
 **/
CdProfile *
cd_client_find_profile_by_property_finish (CdClient *client,
					   GAsyncResult *res,
					   GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, client), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_client_find_profile_by_property_cb (GObject *source_object,
				       GAsyncResult *res,
				       gpointer user_data)
{
	CdProfile *profile;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *object_path = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create a profile object */
	g_variant_get (result, "(o)", &object_path);
	profile = cd_profile_new ();
	cd_profile_set_object_path (profile, object_path);

	/* success */
	g_task_return_pointer (task, profile, (GDestroyNotify) g_object_unref);
}

/**
 * cd_client_find_profile_by_property:
 * @client: a #CdClient instance.
 * @key: the profile property key
 * @value: the profile property value
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Finds a color profile that has a property value.
 *
 * Since: 0.1.24
 **/
void
cd_client_find_profile_by_property (CdClient *client,
				    const gchar *key,
				    const gchar *value,
				    GCancellable *cancellable,
				    GAsyncReadyCallback callback,
				    gpointer user_data)
{
	CdClientPrivate *priv = GET_PRIVATE (client);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (key != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "FindProfileByProperty",
			   g_variant_new ("(ss)", key, value),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_find_profile_by_property_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_client_find_sensor_finish:
 * @client: a #CdClient instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: (transfer full): a #CdSensor or %NULL
 *
 * Since: 0.1.26
 **/
CdSensor *
cd_client_find_sensor_finish (CdClient *client,
			      GAsyncResult *res,
			      GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, client), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_client_find_sensor_cb (GObject *source_object,
			    GAsyncResult *res,
			    gpointer user_data)
{
	CdSensor *sensor;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *object_path = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_client_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* create a sensor object */
	g_variant_get (result, "(o)", &object_path);
	sensor = cd_sensor_new ();
	cd_sensor_set_object_path (sensor, object_path);

	/* success */
	g_task_return_pointer (task, sensor, (GDestroyNotify) g_object_unref);
}

/**
 * cd_client_find_sensor:
 * @client: a #CdClient instance.
 * @id: a sensor id
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Finds a sensor by an ID.
 *
 * Since: 0.1.26
 **/
void
cd_client_find_sensor (CdClient *client,
		       const gchar *id,
		       GCancellable *cancellable,
		       GAsyncReadyCallback callback,
		       gpointer user_data)
{
	CdClientPrivate *priv = GET_PRIVATE (client);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_CLIENT (client));
	g_return_if_fail (id != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (G_OBJECT (client), cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "FindSensorById",
			   g_variant_new ("(s)", id),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_client_find_sensor_cb,
			   task);
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
	CdClient *client = CD_CLIENT (object);
	CdClientPrivate *priv = GET_PRIVATE (client);

	switch (prop_id) {
	case PROP_DAEMON_VERSION:
		g_value_set_string (value, priv->daemon_version);
		break;
	case PROP_SYSTEM_VENDOR:
		g_value_set_string (value, priv->system_vendor);
		break;
	case PROP_SYSTEM_MODEL:
		g_value_set_string (value, priv->system_model);
		break;
	case PROP_CONNECTED:
		g_value_set_boolean (value, priv->proxy != NULL);
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
	 * CdClient:system-vendor:
	 *
	 * The system vendor.
	 *
	 * Since: 1.0.2
	 */
	g_object_class_install_property (object_class,
					 PROP_SYSTEM_VENDOR,
					 g_param_spec_string ("system-vendor",
							      "System Vendor",
							      NULL,
							      NULL,
							      G_PARAM_READABLE));
	/**
	 * CdClient:system-model:
	 *
	 * The system model.
	 *
	 * Since: 1.0.2
	 */
	g_object_class_install_property (object_class,
					 PROP_SYSTEM_MODEL,
					 g_param_spec_string ("system-model",
							      "System model",
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
	 * The ::device-removed signal is emitted when a device is removed.
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
}

/*
 * cd_client_init:
 */
static void
cd_client_init (CdClient *client)
{
	/* ensure the remote errors are registered */
	cd_client_error_quark ();
}

/*
 * cd_client_finalize:
 */
static void
cd_client_finalize (GObject *object)
{
	CdClient *client = CD_CLIENT (object);
	CdClientPrivate *priv = GET_PRIVATE (client);

	g_return_if_fail (CD_IS_CLIENT (object));

	g_free (priv->daemon_version);
	g_free (priv->system_vendor);
	g_free (priv->system_model);
	if (priv->proxy != NULL)
		g_object_unref (priv->proxy);

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

