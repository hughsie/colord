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
 * SECTION:cd-device-sync
 * @short_description: Sync helpers for #CdDevice
 *
 * These helper functions provide a simple way to use the async functions
 * in command line tools.
 *
 * See also: #CdDevice
 */

#include "config.h"

#include <glib.h>
#include <gio/gio.h>

#include "cd-profile.h"
#include "cd-device.h"
#include "cd-device-sync.h"

/* tiny helper to help us do the async operation */
typedef struct {
	GError		**error;
	GMainLoop	*loop;
	gboolean	 ret;
	CdDevice	*device;
	CdProfile	*profile;
} CdDeviceHelper;

static void
cd_device_connect_finish_sync (CdDevice *device,
			       GAsyncResult *res,
			       CdDeviceHelper *helper)
{
	helper->ret = cd_device_connect_finish (device,
						res,
						helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_device_connect_sync:
 * @device: a #CdDevice instance.
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
cd_device_connect_sync (CdDevice *device,
			GCancellable *cancellable,
			GError **error)
{
	CdDeviceHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_device_connect (device, cancellable,
			   (GAsyncReadyCallback) cd_device_connect_finish_sync,
			   &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_device_set_property_finish_sync (CdDevice *device,
				    GAsyncResult *res,
				    CdDeviceHelper *helper)
{
	helper->ret = cd_device_set_property_finish (device,
						     res,
						     helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_device_set_property_sync:
 * @device: a #CdDevice instance.
 * @key: The property key
 * @value: The property value
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets an object property.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.8
 **/
gboolean
cd_device_set_property_sync (CdDevice *device,
			     const gchar *key,
			     const gchar *value,
			     GCancellable *cancellable,
			     GError **error)
{
	CdDeviceHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_device_set_property (device, key, value, cancellable,
				(GAsyncReadyCallback) cd_device_set_property_finish_sync,
				&helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_device_add_profile_finish_sync (CdDevice *device,
				   GAsyncResult *res,
				   CdDeviceHelper *helper)
{
	helper->ret = cd_device_add_profile_finish (device,
						    res,
						    helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_device_add_profile_sync:
 * @device: a #CdDevice instance.
 * @relation: a #CdDeviceRelation, e.g. #CD_DEVICE_RELATION_HARD
 * @profile: a #CdProfile instance
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Adds a profile to a device.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.3
 **/
gboolean
cd_device_add_profile_sync (CdDevice *device,
			    CdDeviceRelation relation,
			    CdProfile *profile,
			    GCancellable *cancellable,
			    GError **error)
{
	CdDeviceHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_device_add_profile (device, relation, profile, cancellable,
			    (GAsyncReadyCallback) cd_device_add_profile_finish_sync,
			    &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_device_remove_profile_finish_sync (CdDevice *device,
				      GAsyncResult *res,
				      CdDeviceHelper *helper)
{
	helper->ret = cd_device_remove_profile_finish (device,
						       res,
						       helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_device_remove_profile_sync:
 * @device: a #CdDevice instance.
 * @profile: a #CdProfile instance
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Removes a profile from a device.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.2
 **/
gboolean
cd_device_remove_profile_sync (CdDevice *device,
			       CdProfile *profile,
			       GCancellable *cancellable,
			       GError **error)
{
	CdDeviceHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_device_remove_profile (device, profile, cancellable,
				  (GAsyncReadyCallback) cd_device_remove_profile_finish_sync,
				  &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_device_get_profile_for_qualifiers_finish_sync (CdDevice *device,
						  GAsyncResult *res,
						  CdDeviceHelper *helper)
{
	helper->profile = cd_device_get_profile_for_qualifiers_finish (device,
								       res,
								       helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_device_get_profile_for_qualifiers_sync:
 * @device: a #CdDevice instance.
 * @qualifiers: a set of qualifiers that can included wildcards
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Gets the prefered profile for some qualifiers.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: (transfer full): a #CdProfile or %NULL
 *
 * Since: 0.1.8
 **/
CdProfile *
cd_device_get_profile_for_qualifiers_sync (CdDevice *device,
					   const gchar **qualifiers,
					   GCancellable *cancellable,
					   GError **error)
{
	CdDeviceHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_device_get_profile_for_qualifiers (device, qualifiers, cancellable,
					      (GAsyncReadyCallback) cd_device_get_profile_for_qualifiers_finish_sync,
					     &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.profile;
}

/**********************************************************************/

static void
cd_device_make_profile_default_finish_sync (CdDevice *device,
					    GAsyncResult *res,
					    CdDeviceHelper *helper)
{
	helper->ret = cd_device_make_profile_default_finish (device,
						 res,
						 helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_device_make_profile_default_sync:
 * @device: a #CdDevice instance.
 * @profile: a #CdProfile instance
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Makes an already added profile default for a device.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.8
 **/
gboolean
cd_device_make_profile_default_sync (CdDevice *device,
				     CdProfile *profile,
				     GCancellable *cancellable,
				     GError **error)
{
	CdDeviceHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_device_make_profile_default (device, profile, cancellable,
					(GAsyncReadyCallback) cd_device_make_profile_default_finish_sync,
					&helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_device_profiling_inhibit_finish_sync (CdDevice *device,
					 GAsyncResult *res,
					 CdDeviceHelper *helper)
{
	helper->ret = cd_device_profiling_inhibit_finish (device,
							  res,
							  helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_device_profiling_inhibit_sync:
 * @device: a #CdDevice instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets up the device for profiling and causes no profiles to be
 * returned if cd_device_get_profile_for_qualifiers_sync() is used.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.1
 **/
gboolean
cd_device_profiling_inhibit_sync (CdDevice *device,
				  GCancellable *cancellable,
				  GError **error)
{
	CdDeviceHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_device_profiling_inhibit (device, cancellable,
				     (GAsyncReadyCallback) cd_device_profiling_inhibit_finish_sync,
				     &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_device_profiling_uninhibit_finish_sync (CdDevice *device,
					   GAsyncResult *res,
					   CdDeviceHelper *helper)
{
	helper->ret = cd_device_profiling_uninhibit_finish (device,
							    res,
							    helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_device_profiling_uninhibit_sync:
 * @device: a #CdDevice instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets up the device for profiling and causes no profiles to be
 * returned if cd_device_get_profile_for_qualifiers_sync() is used.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.1
 **/
gboolean
cd_device_profiling_uninhibit_sync (CdDevice *device,
				    GCancellable *cancellable,
				    GError **error)
{
	CdDeviceHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_device_profiling_uninhibit (device, cancellable,
				       (GAsyncReadyCallback) cd_device_profiling_uninhibit_finish_sync,
				       &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_device_get_profile_relation_finish_sync (CdDevice *device,
					    GAsyncResult *res,
					    CdDeviceHelper *helper)
{
	helper->ret = cd_device_get_profile_relation_finish (device,
						 res,
						 helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_device_get_profile_relation_sync:
 * @device: a #CdDevice instance.
 * @profile: a #CdProfile instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Gets the property relationship to the device.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.8
 **/
CdDeviceRelation
cd_device_get_profile_relation_sync (CdDevice *device,
				     CdProfile *profile,
				     GCancellable *cancellable,
				     GError **error)
{
	CdDeviceHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_device_get_profile_relation (device, profile, cancellable,
					(GAsyncReadyCallback) cd_device_get_profile_relation_finish_sync,
					&helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_device_set_enabled_finish_sync (CdDevice *device,
				   GAsyncResult *res,
				   CdDeviceHelper *helper)
{
	helper->ret = cd_device_set_enabled_finish (device,
						    res,
						    helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_device_set_enabled_sync:
 * @device: a #CdDevice instance.
 * @enabled: the enabled state
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Enables or disables a device.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.26
 **/
gboolean
cd_device_set_enabled_sync (CdDevice *device,
			    gboolean enabled,
			    GCancellable *cancellable,
			    GError **error)
{
	CdDeviceHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_device_set_enabled (device, enabled, cancellable,
				(GAsyncReadyCallback) cd_device_set_enabled_finish_sync,
				&helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

/**
 * cd_device_set_model_sync:
 * @device: a #CdDevice instance.
 * @value: The model.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the device model.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.0
 **/
gboolean
cd_device_set_model_sync (CdDevice *device,
			  const gchar *value,
			  GCancellable *cancellable,
			  GError **error)
{
	return cd_device_set_property_sync (device,
					    CD_DEVICE_PROPERTY_MODEL,
					    value,
					    cancellable, error);
}

/**
 * cd_device_set_serial_sync:
 * @device: a #CdDevice instance.
 * @value: The string value.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the device serial number.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.1
 **/
gboolean
cd_device_set_serial_sync (CdDevice *device,
			   const gchar *value,
			   GCancellable *cancellable,
			   GError **error)
{
	return cd_device_set_property_sync (device,
					    CD_DEVICE_PROPERTY_SERIAL,
					    value,
					    cancellable, error);
}

/**
 * cd_device_set_vendor_sync:
 * @device: a #CdDevice instance.
 * @value: The string value.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the device vendor.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.1
 **/
gboolean
cd_device_set_vendor_sync (CdDevice *device,
			   const gchar *value,
			   GCancellable *cancellable,
			   GError **error)
{
	return cd_device_set_property_sync (device,
					    CD_DEVICE_PROPERTY_VENDOR,
					    value,
					    cancellable, error);
}

/**
 * cd_device_set_kind_sync:
 * @device: a #CdDevice instance.
 * @kind: The device kind, e.g. #CD_DEVICE_KIND_DISPLAY
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the device kind.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.0
 **/
gboolean
cd_device_set_kind_sync (CdDevice *device,
			 CdDeviceKind kind,
			 GCancellable *cancellable,
			 GError **error)
{
	return cd_device_set_property_sync (device,
					    CD_DEVICE_PROPERTY_KIND,
					    cd_device_kind_to_string (kind),
					    cancellable, error);
}

/**
 * cd_device_set_colorspace_sync:
 * @device: a #CdDevice instance.
 * @colorspace: The device colorspace, e.g. #CD_COLORSPACE_RGB
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the device kind.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.1
 **/
gboolean
cd_device_set_colorspace_sync (CdDevice *device,
			       CdColorspace colorspace,
			       GCancellable *cancellable,
			       GError **error)
{
	return cd_device_set_property_sync (device,
					    CD_DEVICE_PROPERTY_COLORSPACE,
					    cd_colorspace_to_string (colorspace),
					    cancellable, error);
}

/**
 * cd_device_set_mode_sync:
 * @device: a #CdDevice instance.
 * @mode: The device kind, e.g. #CD_DEVICE_MODE_VIRTUAL
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the device mode.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.2
 **/
gboolean
cd_device_set_mode_sync (CdDevice *device,
			 CdDeviceMode mode,
			 GCancellable *cancellable,
			 GError **error)
{
	return cd_device_set_property_sync (device,
					    CD_DEVICE_PROPERTY_MODE,
					    cd_device_mode_to_string (mode),
					    cancellable, error);
}
