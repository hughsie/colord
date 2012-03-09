/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2012 Richard Hughes <richard@hughsie.com>
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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <sane/sane.h>
#include <gudev/gudev.h>

#include "cd-client.h"
#include "cd-device.h"

typedef struct {
	CdClient	*client;
	gboolean	 doing_refresh;
	gboolean	 init_sane;
	GDBusNodeInfo	*introspection;
	GMainLoop	*loop;
	GPtrArray	*array; /* of CdMainDev's */
	GUdevClient	*gudev_client;
	guint		 owner_id;
	guint		 timer_id;
} CdMainPrivate;

typedef struct {
	CdDevice	*device;
	gchar		*id; /* note: we can get this from CdDevice, but we don't wan't to connect() */
	gboolean	 valid;
} CdMainDev;

#define COLORD_SANE_DBUS_SERVICE	"org.freedesktop.colord-sane"
#define COLORD_SANE_DBUS_PATH		"/org/freedesktop/colord_sane"
#define COLORD_SANE_DBUS_INTERFACE	"org.freedesktop.colord.sane"

#define COLORD_SANE_UEVENT_DELAY	2 /* seconds */

/**
 * cd_main_dev_free:
 **/
static void
cd_main_dev_free (CdMainDev *tmp)
{
	g_object_unref (tmp->device);
	g_free (tmp->id);
	g_free (tmp);
}

/**
 * cd_main_dev_set_invalid:
 **/
static void
cd_main_dev_set_invalid (CdMainPrivate *priv)
{
	CdMainDev *tmp;
	guint i;

	/* nothing to set */
	if (priv->array->len == 0)
		return;
	for (i = 0; i < priv->array->len; i++) {
		tmp = g_ptr_array_index (priv->array, i);
		tmp->valid = FALSE;
	}
}

/**
 * cd_main_dev_find_by_id:
 **/
static CdMainDev *
cd_main_dev_find_by_id (CdMainPrivate *priv,
			const gchar *id)
{
	CdMainDev *tmp;
	guint i;

	/* nothing to find */
	if (priv->array->len == 0)
		goto out;
	for (i = 0; i < priv->array->len; i++) {
		tmp = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (tmp->id, id) == 0)
			return tmp;
	}
out:
	return NULL;
}

/**
 * cd_client_get_id_for_sane_device:
 **/
static gchar *
cd_client_get_id_for_sane_device (const SANE_Device *sane_device)
{
	gchar *id;
	id = g_strdup_printf ("sane-%s", sane_device->model);
	return id;
}

typedef struct {
	CdMainPrivate	*priv;
	gchar		*id;
} CdMainCreateDeviceHelper;

/**
 * cd_main_colord_create_device_cb:
 **/
static void
cd_main_colord_create_device_cb (GObject *source_object,
				 GAsyncResult *res,
				 gpointer user_data)
{
	CdClient *client = CD_CLIENT (source_object);
	CdDevice *device;
	CdMainCreateDeviceHelper *helper = (CdMainCreateDeviceHelper *) user_data;
	CdMainDev *dev;
	CdMainPrivate *priv = helper->priv;
	GError *error = NULL;

	/* get result */
	device = cd_client_create_device_finish (client, res, &error);
	if (device == NULL) {
		g_warning ("failed to create device: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	g_debug ("Added device: %s", cd_device_get_object_path (device));
	dev = g_new (CdMainDev, 1);
	dev->id = g_strdup (helper->id);
	dev->device = g_object_ref (device);
	g_ptr_array_add (priv->array, dev);
out:
	g_free (helper->id);
	g_free (helper);
	if (device != NULL)
		g_object_unref (device);
}

/**
 * cd_sane_client_add:
 **/
static void
cd_sane_client_add (CdMainPrivate *priv, const SANE_Device *sane_device)
{
	CdMainCreateDeviceHelper *helper = NULL;
	CdMainDev *dev;
	gchar *id = NULL;
	gchar *model = NULL;
	gchar *vendor = NULL;
	GHashTable *properties = NULL;

	/* ignore noname, no support devices */
	if (g_strcmp0 (sane_device->vendor, "Noname") == 0) {
		g_debug ("CdSaneClient: Ignoring sane device %s",
			 sane_device->name);
		goto out;
	}

	/* convert device_id 'plustek:libusb:004:002' to suitable id */
	id = cd_client_get_id_for_sane_device (sane_device);

	/* see if this device already exists */
	dev = cd_main_dev_find_by_id (priv, id);
	if (dev != NULL) {
		dev->valid = TRUE;
		goto out;
	}

	/* Make human readable */
	model = g_strdup (sane_device->model);
	g_strdelimit (model, "_", ' ');
	vendor = g_strdup (sane_device->vendor);
	g_strdelimit (vendor, "_", ' ');

	/* create initial device properties */
	properties = g_hash_table_new_full (g_str_hash, g_str_equal,
					      NULL, NULL);
	g_hash_table_insert (properties,
			     (gpointer) CD_DEVICE_PROPERTY_KIND,
			     (gpointer) cd_device_kind_to_string (CD_DEVICE_KIND_SCANNER));
	g_hash_table_insert (properties,
			     (gpointer) CD_DEVICE_PROPERTY_MODE,
			     (gpointer) cd_device_mode_to_string (CD_DEVICE_MODE_PHYSICAL));
	g_hash_table_insert (properties,
			     (gpointer) CD_DEVICE_PROPERTY_COLORSPACE,
			     (gpointer) cd_colorspace_to_string (CD_COLORSPACE_RGB));
	g_hash_table_insert (properties,
			     (gpointer) CD_DEVICE_PROPERTY_VENDOR,
			     (gpointer) vendor);
	g_hash_table_insert (properties,
			     (gpointer) CD_DEVICE_PROPERTY_MODEL,
			     (gpointer) model);
	g_hash_table_insert (properties,
			     (gpointer) CD_DEVICE_PROPERTY_SERIAL,
			     (gpointer) sane_device->name);
	helper = g_new0 (CdMainCreateDeviceHelper, 1);
	helper->priv = priv;
	helper->id = g_strdup (id);
	cd_client_create_device (priv->client,
				 id,
				 CD_OBJECT_SCOPE_TEMP,
				 properties,
				 NULL,
				 cd_main_colord_create_device_cb,
				 helper);
out:
	if (properties != NULL)
		g_hash_table_unref (properties);
	g_free (id);
	g_free (model);
	g_free (vendor);
}

/**
 * cd_main_colord_delete_device_cb:
 **/
static void
cd_main_colord_delete_device_cb (GObject *source_object,
				 GAsyncResult *res,
				 gpointer user_data)
{
	CdClient *client = CD_CLIENT (source_object);
	gboolean ret;
	GError *error = NULL;

	/* get result */
	ret = cd_client_delete_device_finish (client, res, &error);
	if (!ret) {
		g_warning ("failed to delete device: %s",
			   error->message);
		g_error_free (error);
	}
}

/**
 * cd_sane_client_remove:
 **/
static void
cd_sane_client_remove (CdMainPrivate *priv, CdDevice *device)
{
	g_debug ("Deleting device: %s", cd_device_get_object_path (device));
	cd_client_delete_device (priv->client,
				 device,
				 NULL,
				 cd_main_colord_delete_device_cb,
				 priv);
}

/**
 * cd_sane_client_refresh:
 **/
static void
cd_sane_client_refresh (CdMainPrivate *priv)
{
	CdMainDev *tmp;
	const SANE_Device **device_list = NULL;
	gint idx;
	guint i;
	SANE_Status status;

	/* don't be re-entrant */
	if (priv->doing_refresh)
		return;
	priv->doing_refresh = TRUE;

	/* force sane to drop it's cache of devices -- yes, it is that crap */
	if (priv->init_sane) {
		sane_exit ();
		priv->init_sane = FALSE;
	}
	status = sane_init (NULL, NULL);
	if (status != SANE_STATUS_GOOD) {
		g_warning ("failed to init SANE: %s",
			   sane_strstatus (status));
		goto out;
	}
	priv->init_sane = TRUE;

	/* invalidate all devices */
	cd_main_dev_set_invalid (priv);

	/* get scanners on the local server */
	status = sane_get_devices (&device_list, TRUE);
	if (status != SANE_STATUS_GOOD) {
		g_warning ("failed to get devices from SANE: %s",
			   sane_strstatus (status));
		goto out;
	}

	/* nothing */
	if (device_list == NULL || device_list[0] == NULL)
		goto out;

	/* add them */
	for (idx = 0; device_list[idx] != NULL; idx++)
		cd_sane_client_add (priv, device_list[idx]);

	/* remove any that are invalid */
	for (i = 0; i < priv->array->len; i++) {
		tmp = g_ptr_array_index (priv->array, i);
		if (tmp->valid)
			continue;
		cd_sane_client_remove (priv, tmp->device);
	}
out:
	priv->doing_refresh = FALSE;
}

/**
 * cd_sane_client_refresh_cb:
 **/
static gboolean
cd_sane_client_refresh_cb (gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	g_debug ("Refreshing scanner devices...");
	cd_sane_client_refresh (priv);
	priv->timer_id = 0;
	return FALSE;
}

/**
 * cd_sane_client_refresh_schedule:
 **/
static void
cd_sane_client_refresh_schedule (CdMainPrivate *priv)
{
	if (priv->timer_id != 0)
		g_source_remove (priv->timer_id);
	priv->timer_id = g_timeout_add_seconds (COLORD_SANE_UEVENT_DELAY,
						cd_sane_client_refresh_cb,
						priv);
}

/**
 * cd_main_daemon_method_call:
 **/
static void
cd_main_daemon_method_call (GDBusConnection *connection_, const gchar *sender,
			    const gchar *object_path, const gchar *interface_name,
			    const gchar *method_name, GVariant *parameters,
			    GDBusMethodInvocation *invocation, gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;

	/* return '' */
	if (g_strcmp0 (method_name, "Refresh") == 0) {
		g_debug ("CdMain: %s:Refresh()", sender);
		cd_sane_client_refresh_schedule (priv);
		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}
out:
	return;
}

/**
 * cd_main_daemon_get_property:
 **/
static GVariant *
cd_main_daemon_get_property (GDBusConnection *connection_, const gchar *sender,
			     const gchar *object_path, const gchar *interface_name,
			     const gchar *property_name, GError **error,
			     gpointer user_data)
{
	GVariant *retval = NULL;

	if (g_strcmp0 (property_name, "DaemonVersion") == 0) {
		retval = g_variant_new_string (VERSION);
	} else {
		g_critical ("failed to get property %s",
			    property_name);
	}

	return retval;
}

/**
 * cd_main_on_bus_acquired_cb:
 **/
static void
cd_main_on_bus_acquired_cb (GDBusConnection *connection_,
			    const gchar *name,
			    gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	guint registration_id;
	static const GDBusInterfaceVTable interface_vtable = {
		cd_main_daemon_method_call,
		cd_main_daemon_get_property,
		NULL
	};

	registration_id = g_dbus_connection_register_object (connection_,
							     COLORD_SANE_DBUS_PATH,
							     priv->introspection->interfaces[0],
							     &interface_vtable,
							     priv,  /* user_data */
							     NULL,  /* user_data_free_func */
							     NULL); /* GError** */
	g_assert (registration_id > 0);
}

/**
 * cd_main_colord_connect_cb:
 **/
static void
cd_main_colord_connect_cb (GObject *source_object,
			   GAsyncResult *res,
			   gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	gboolean ret;
	GError *error = NULL;

	/* get result */
	ret = cd_client_connect_finish (priv->client, res, &error);
	if (!ret) {
		g_warning ("failed to connect to colord: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* refresh */
	cd_sane_client_refresh (priv);
out:
	return;
}

/**
 * cd_main_udev_uevent_cb:
 **/
static void
cd_main_udev_uevent_cb (GUdevClient *gudev_client,
			const gchar *action,
			GUdevDevice *udev_device,
			CdMainPrivate *priv)
{
	/* add or remove */
	if (g_strcmp0 (action, "add") == 0 ||
	    g_strcmp0 (action, "remove") == 0) {
		cd_sane_client_refresh_schedule (priv);
	}
}

/**
 * cd_main_on_name_acquired_cb:
 **/
static void
cd_main_on_name_acquired_cb (GDBusConnection *connection_,
			     const gchar *name,
			     gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	const gchar *subsystems[] = {"usb", NULL};

	g_debug ("CdMain: acquired name: %s", name);

	/* setup */
	priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_main_dev_free);
	priv->client = cd_client_new ();
	priv->gudev_client = g_udev_client_new (subsystems);
	g_signal_connect (priv->gudev_client, "uevent",
			  G_CALLBACK (cd_main_udev_uevent_cb), priv);

	/* connect to daemon */
	cd_client_connect (priv->client,
			   NULL,
			   cd_main_colord_connect_cb,
			   priv);
}

/**
 * cd_main_on_name_lost_cb:
 **/
static void
cd_main_on_name_lost_cb (GDBusConnection *connection_,
			 const gchar *name,
			 gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	g_debug ("CdMain: lost name: %s", name);
	g_main_loop_quit (priv->loop);
}

/**
 * cd_main_timed_exit_cb:
 **/
static gboolean
cd_main_timed_exit_cb (gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	g_main_loop_quit (priv->loop);
	return FALSE;
}

/**
 * cd_main_load_introspection:
 **/
static GDBusNodeInfo *
cd_main_load_introspection (const gchar *filename, GError **error)
{
	gboolean ret;
	gchar *data = NULL;
	GDBusNodeInfo *info = NULL;
	GFile *file;

	/* load file */
	file = g_file_new_for_path (filename);
	ret = g_file_load_contents (file, NULL, &data,
				    NULL, NULL, error);
	if (!ret)
		goto out;

	/* build introspection from XML */
	info = g_dbus_node_info_new_for_xml (data, error);
	if (info == NULL)
		goto out;
out:
	g_object_unref (file);
	g_free (data);
	return info;
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	CdMainPrivate *priv = NULL;
	gboolean immediate_exit = FALSE;
	gboolean timed_exit = FALSE;
	GError *error = NULL;
	GOptionContext *context;
	guint retval = 1;
	const GOptionEntry options[] = {
		{ "timed-exit", '\0', 0, G_OPTION_ARG_NONE, &timed_exit,
		  /* TRANSLATORS: exit after we've started up, used for user profiling */
		  _("Exit after a small delay"), NULL },
		{ "immediate-exit", '\0', 0, G_OPTION_ARG_NONE, &immediate_exit,
		  /* TRANSLATORS: exit straight away, used for automatic profiling */
		  _("Exit after the engine has loaded"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();

	/* TRANSLATORS: program name */
	g_set_application_name (_("Color Management (SANE helper)"));
	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_set_summary (context, _("Color Management D-Bus Service (SANE)"));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* create new objects */
	priv = g_new0 (CdMainPrivate, 1);
	priv->loop = g_main_loop_new (NULL, FALSE);

	/* load introspection from file */
	priv->introspection = cd_main_load_introspection (DATADIR "/dbus-1/interfaces/"
							  COLORD_SANE_DBUS_INTERFACE ".xml",
							  &error);
	if (priv->introspection == NULL) {
		g_warning ("CdMain: failed to load introspection: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* own the object */
	priv->owner_id = g_bus_own_name (G_BUS_TYPE_SYSTEM,
					 COLORD_SANE_DBUS_SERVICE,
					 G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
					 G_BUS_NAME_OWNER_FLAGS_REPLACE,
					 cd_main_on_bus_acquired_cb,
					 cd_main_on_name_acquired_cb,
					 cd_main_on_name_lost_cb,
					 priv, NULL);

	/* Only timeout and close the mainloop if we have specified it
	 * on the command line */
	if (immediate_exit)
		g_idle_add (cd_main_timed_exit_cb, priv);
	else if (timed_exit)
		g_timeout_add_seconds (5, cd_main_timed_exit_cb, priv);

	/* wait */
	g_main_loop_run (priv->loop);

	/* success */
	retval = 0;
out:
	if (priv != NULL) {
		if (priv->init_sane)
			sane_exit ();
		if (priv->array != NULL)
			g_ptr_array_unref (priv->array);
		if (priv->owner_id > 0)
			g_bus_unown_name (priv->owner_id);
		if (priv->client != NULL)
			g_object_unref (priv->client);
		if (priv->gudev_client != NULL)
			g_object_unref (priv->gudev_client);
		if (priv->introspection != NULL)
			g_dbus_node_info_unref (priv->introspection);
		g_main_loop_unref (priv->loop);
	}
	return retval;
}
