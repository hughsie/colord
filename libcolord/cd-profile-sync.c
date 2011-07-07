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
 * SECTION:cd-profile-sync
 * @short_description: Sync helpers for #CdProfile
 *
 * These helper functions provide a simple way to use the async functions
 * in command line tools.
 *
 * See also: #CdProfile
 */

#include "config.h"

#include <glib.h>
#include <gio/gio.h>

#include "cd-profile.h"
#include "cd-profile-sync.h"

/* tiny helper to help us do the async operation */
typedef struct {
	GError		**error;
	GMainLoop	*loop;
	gboolean	 ret;
	CdProfile	*profile;
} CdProfileHelper;

static void
cd_profile_connect_finish_sync (CdProfile *profile,
				GAsyncResult *res,
				CdProfileHelper *helper)
{
	helper->ret = cd_profile_connect_finish (profile,
						 res,
						 helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_profile_connect_sync:
 * @profile: a #CdProfile instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Connects to the object and fills up initial properties.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.8
 **/
gboolean
cd_profile_connect_sync (CdProfile *profile,
			 GCancellable *cancellable,
			 GError **error)
{
	CdProfileHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_profile_connect (profile, cancellable,
			    (GAsyncReadyCallback) cd_profile_connect_finish_sync,
			    &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_profile_set_property_finish_sync (CdProfile *profile,
				     GAsyncResult *res,
				     CdProfileHelper *helper)
{
	helper->ret = cd_profile_set_property_finish (profile,
						 res,
						 helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_profile_set_property_sync:
 * @profile: a #CdProfile instance.
 * @key: The key
 * @value: The value
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets properties on an object
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.8
 **/
gboolean
cd_profile_set_property_sync (CdProfile *profile,
			      const gchar *key,
			      const gchar *value,
			      GCancellable *cancellable,
			      GError **error)
{
	CdProfileHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_profile_set_property (profile, key, value, cancellable,
				 (GAsyncReadyCallback) cd_profile_set_property_finish_sync,
				 &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_profile_install_system_wide_finish_sync (CdProfile *profile,
					    GAsyncResult *res,
					    CdProfileHelper *helper)
{
	helper->ret = cd_profile_install_system_wide_finish (profile,
						 res,
						 helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_profile_install_system_wide_sync:
 * @profile: a #CdProfile instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the profile system wide.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.8
 **/
gboolean
cd_profile_install_system_wide_sync (CdProfile *profile,
				     GCancellable *cancellable,
				     GError **error)
{
	CdProfileHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_profile_install_system_wide (profile, cancellable,
					(GAsyncReadyCallback) cd_profile_install_system_wide_finish_sync,
					&helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

/**
 * cd_profile_set_filename_sync:
 * @profile: a #CdProfile instance.
 * @value: The filename.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the profile model.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.0
 **/
gboolean
cd_profile_set_filename_sync (CdProfile *profile,
			      const gchar *value,
			      GCancellable *cancellable,
			      GError **error)
{
	return cd_profile_set_property_sync (profile,
					     CD_PROFILE_PROPERTY_FILENAME,
					     value,
					     cancellable, error);
}

/**
 * cd_profile_set_qualifier_sync:
 * @profile: a #CdProfile instance.
 * @value: The qualifier.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the profile model.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.0
 **/
gboolean
cd_profile_set_qualifier_sync (CdProfile *profile,
			       const gchar *value,
			       GCancellable *cancellable,
			       GError **error)
{
	return cd_profile_set_property_sync (profile, CD_PROFILE_PROPERTY_QUALIFIER, value,
					     cancellable, error);
}
