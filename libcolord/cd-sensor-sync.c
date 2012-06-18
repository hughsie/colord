/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
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
 * SECTION:cd-sensor-sync
 * @short_description: Sync helpers for #CdSensor
 *
 * These helper functions provide a simple way to use the async functions
 * in command line tools.
 *
 * See also: #CdSensor
 */

#include "config.h"

#include <glib.h>
#include <gio/gio.h>

#include "cd-sensor.h"
#include "cd-sensor-sync.h"

/* tiny helper to help us do the async operation */
typedef struct {
	GError		**error;
	GMainLoop	*loop;
	gboolean	 ret;
	CdColorXYZ	*sample;
} CdSensorHelper;

static void
cd_sensor_connect_finish_sync (CdSensor *sensor,
			       GAsyncResult *res,
			       CdSensorHelper *helper)
{
	helper->ret = cd_sensor_connect_finish (sensor,
						res,
						helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_sensor_connect_sync:
 * @sensor: a #CdSensor instance.
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
cd_sensor_connect_sync (CdSensor *sensor,
			GCancellable *cancellable,
			GError **error)
{
	CdSensorHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_sensor_connect (sensor, cancellable,
			   (GAsyncReadyCallback) cd_sensor_connect_finish_sync,
			   &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_sensor_lock_finish_sync (CdSensor *sensor,
			    GAsyncResult *res,
			    CdSensorHelper *helper)
{
	helper->ret = cd_sensor_lock_finish (sensor,
					     res,
					     helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_sensor_lock_sync:
 * @sensor: a #CdSensor instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Locks the device so we can use it.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.6
 **/
gboolean
cd_sensor_lock_sync (CdSensor *sensor,
		     GCancellable *cancellable,
		     GError **error)
{
	CdSensorHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_sensor_lock (sensor, cancellable,
			(GAsyncReadyCallback) cd_sensor_lock_finish_sync,
			&helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_sensor_unlock_finish_sync (CdSensor *sensor,
			      GAsyncResult *res,
			      CdSensorHelper *helper)
{
	helper->ret = cd_sensor_unlock_finish (sensor,
					       res,
					       helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_sensor_unlock_sync:
 * @sensor: a #CdSensor instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Unlocks the device for use by other programs.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.6
 **/
gboolean
cd_sensor_unlock_sync (CdSensor *sensor,
		       GCancellable *cancellable,
		       GError **error)
{
	CdSensorHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_sensor_unlock (sensor, cancellable,
			  (GAsyncReadyCallback) cd_sensor_unlock_finish_sync,
			  &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_sensor_set_options_finish_sync (CdSensor *sensor,
				   GAsyncResult *res,
				   CdSensorHelper *helper)
{
	helper->ret = cd_sensor_set_options_finish (sensor,
						    res,
						    helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_sensor_set_options_sync:
 * @sensor: a #CdSensor instance.
 * @values: (element-type utf8 GVariant): the options
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets options on the sensor device.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.20
 **/
gboolean
cd_sensor_set_options_sync (CdSensor *sensor,
			    GHashTable *values,
			    GCancellable *cancellable,
			    GError **error)
{
	CdSensorHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_sensor_set_options (sensor, values, cancellable,
			       (GAsyncReadyCallback) cd_sensor_set_options_finish_sync,
			       &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

static void
cd_sensor_get_sample_finish_sync (CdSensor *sensor,
				  GAsyncResult *res,
				  CdSensorHelper *helper)
{
	helper->sample = cd_sensor_get_sample_finish (sensor,
						      res,
						      helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * cd_sensor_get_sample_sync:
 * @sensor: a #CdSensor instance.
 * @cap: The device capability, e.g. %CD_SENSOR_CAP_AMBIENT.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Gets a sample from the sensor.
 *
 * WARNING: This function is synchronous, and may block.
 * Do not use it in GUI applications.
 *
 * Return value: the XYZ reading, with ambient levels in Lux encoded in X, or %NULL for error.
 *
 * Since: 0.1.8
 **/
CdColorXYZ *
cd_sensor_get_sample_sync (CdSensor *sensor,
			   CdSensorCap cap,
			   GCancellable *cancellable,
			   GError **error)
{
	CdSensorHelper helper;

	/* create temp object */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	cd_sensor_get_sample (sensor, cap, cancellable,
			      (GAsyncReadyCallback) cd_sensor_get_sample_finish_sync,
			      &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.sample;
}

/**********************************************************************/
