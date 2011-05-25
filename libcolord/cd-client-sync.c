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

#include "cd-client.h"
#include "cd-client-sync.h"

/* tiny helper to help us do the async operation */
typedef struct {
	GError		**error;
	GMainLoop	*loop;
	gboolean	 ret;
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
