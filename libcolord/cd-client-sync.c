/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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
 * SECTION:cd-client-sync
 * @short_description: sync helpers for #CdClient
 *
 * These helper functions provide a simple way to use the async functions
 * in command line tools.
 *
 * See also: #CdClient
 */

#include "config.h"

#include <glib.h>
#include <gio/gio.h>

#include "cd-profile.h"
#include "cd-client.h"
#include "cd-client-sync.h"

/* tiny helper to help us do the async operation */
typedef struct {
	GError		**error;
	GMainLoop	*loop;
	gboolean	 ret;
	CdProfile	*profile;
} CdClientHelper;

static void
cd_client_connect_finish_sync (CdClient *client,
			       GAsyncResult *res,
			       CdClientHelper *helper)
{
	helper->ret = cd_client_connect_finish (client, res, helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_client_connect_sync:
 * @client: a #CdClient instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Connects to the colord daemon.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.0
 **/
gboolean
cd_client_connect_sync (CdClient *client,
			GCancellable *cancellable,
			GError **error)
{
	CdClientHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_client_connect (client, cancellable,
			   (GAsyncReadyCallback) cd_client_connect_finish_sync, &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_client_delete_profile_finish_sync (CdClient *client,
				      GAsyncResult *res,
				      CdClientHelper *helper)
{
	helper->ret = cd_client_delete_profile_finish (client, res, helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_client_delete_profile_sync:
 * @client: a #CdClient instance.
 * @id: a #CdProfile id.
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Deletes a color profile.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
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
	CdClientHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_client_delete_profile (client, id, cancellable,
				  (GAsyncReadyCallback) cd_client_delete_profile_finish_sync, &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_client_delete_device_finish_sync (CdClient *client,
				      GAsyncResult *res,
				      CdClientHelper *helper)
{
	helper->ret = cd_client_delete_device_finish (client, res, helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_client_delete_device_sync:
 * @client: a #CdClient instance.
 * @id: a #CdProfile id.
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Deletes a color device.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
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
	CdClientHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_client_delete_device (client, id, cancellable,
				  (GAsyncReadyCallback) cd_client_delete_device_finish_sync, &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_client_find_profile_by_filename_finish_sync (CdClient *client,
						GAsyncResult *res,
						CdClientHelper *helper)
{
	helper->profile = cd_client_find_profile_by_filename_finish (client,
								     res,
								     helper->error);
	g_main_loop_quit (helper->loop);
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
	CdClientHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;
	helper.profile = NULL;

	/* run async method */
	cd_client_find_profile_by_filename (client, filename, cancellable,
					    (GAsyncReadyCallback) cd_client_find_profile_by_filename_finish_sync,
					    &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.profile;
}

/**********************************************************************/

static void
cd_client_create_profile_finish_sync (CdClient *client,
				      GAsyncResult *res,
				      CdClientHelper *helper)
{
	helper->profile = cd_client_create_profile_finish (client,
							   res,
							   helper->error);
	g_main_loop_quit (helper->loop);
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
	CdClientHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;
	helper.profile = NULL;

	/* run async method */
	cd_client_create_profile (client, id, scope,
				  properties, cancellable,
				  (GAsyncReadyCallback) cd_client_create_profile_finish_sync,
				  &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.profile;
}

/**********************************************************************/
