/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright © Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
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

#include <config.h>
#include <cd-plugin.h>
#include <gudev/gudev.h>
#include <glib.h>

struct CdPluginPrivate {
	GUdevClient		*udev_client;
	gboolean		 scan_in_progress;
};

/**
 * cd_plugin_get_description:
 */
const gchar *
cd_plugin_get_description (void)
{
	return "Add and remove scanner devices using SANE";
}

/**
 * cd_plugin_config_enabled:
 */
gboolean
cd_plugin_config_enabled (void)
{
#ifdef HAVE_SANE
	return TRUE;
#else
	return FALSE;
#endif
}

/*
 * cd_plugin_colord_sane_finished_cb
 */
static void
cd_plugin_colord_sane_finished_cb(GPid pid,
				  gint status,
				  gpointer user_data)
{
	CdPluginPrivate *priv = (CdPluginPrivate *)user_data;

	priv->scan_in_progress = FALSE;
	g_spawn_close_pid (pid);
}

/*
 * cd_plugin_config_enabled_sane_devices
 */
static void
cd_plugin_config_enabled_sane_devices(CdPluginPrivate *priv)
{
	const gchar *argv[] = {COLORD_SANE_BINARY, NULL};
	GError *error = NULL;
	GPid colord_sane_pid;

	if (priv->scan_in_progress)
		return;
	g_spawn_async (NULL,
		       (gchar **) argv,
		       NULL,
		       G_SPAWN_DO_NOT_REAP_CHILD,
		       NULL,
		       NULL,
		       &colord_sane_pid,
		       &error);

	if (error != NULL) {
		g_warning ("CdPlugin: failed to spawn colord-sane helper: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	priv->scan_in_progress = TRUE;
	g_child_watch_add (colord_sane_pid,
			   cd_plugin_colord_sane_finished_cb,
			   priv);
}


/**
 * cd_plugin_uevent_cb:
 **/
static void
cd_plugin_uevent_cb (GUdevClient *udev_client,
		     const gchar *action,
		     GUdevDevice *udev_device,
		     CdPlugin *plugin)
{
	if (g_strcmp0 (action, "remove") == 0 ||
	    g_strcmp0 (action, "add") == 0) {
		cd_plugin_config_enabled_sane_devices (plugin->priv);
	}
}

/**
 * cd_plugin_coldplug:
 */
void
cd_plugin_coldplug (CdPlugin *plugin)
{
	cd_plugin_config_enabled_sane_devices (plugin->priv);

	/* watch udev for changes */
	g_signal_connect (plugin->priv->udev_client, "uevent",
			  G_CALLBACK (cd_plugin_uevent_cb), plugin);
}

/**
 * cd_plugin_initialize:
 */
void
cd_plugin_initialize (CdPlugin *plugin)
{
	const gchar *subsystems[] = { "usb", "scsi", NULL };

	/* create private */
	plugin->priv = CD_PLUGIN_GET_PRIVATE (CdPluginPrivate);
	plugin->priv->udev_client = g_udev_client_new (subsystems);
	plugin->priv->scan_in_progress = FALSE;
}

/**
 * cd_plugin_destroy:
 */
void
cd_plugin_destroy (CdPlugin *plugin)
{
	g_object_unref (plugin->priv->udev_client);
}
