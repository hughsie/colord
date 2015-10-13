/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2015 Richard Hughes <richard@hughsie.com>
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

/**
 * This object contains all the low level logic for imaginary hardware.
 */

#include "config.h"

#include <glib-object.h>
#include <lcms2.h>

#include "cd-sensor.h"

typedef struct
{
	gboolean			 done_startup;
	CdColorRGB			 sample_fake;
	cmsHTRANSFORM			 transform_fake;
} CdSensorDummyPrivate;

static CdSensorDummyPrivate *
cd_sensor_dummy_get_private (CdSensor *sensor)
{
	return g_object_get_data (G_OBJECT (sensor), "priv");
}

static gboolean
cd_sensor_get_ambient_wait_cb (GTask *task)
{
	CdColorXYZ *sample = NULL;

	sample = cd_color_xyz_new ();
	sample->X = 7.7f;
	sample->Y = CD_SENSOR_NO_VALUE;
	sample->Z = CD_SENSOR_NO_VALUE;
	g_task_return_pointer (task, sample, (GDestroyNotify) cd_color_xyz_free);

	return G_SOURCE_REMOVE;
}

static gboolean
cd_sensor_get_sample_wait_cb (GTask *task)
{
	CdSensor *sensor = CD_SENSOR (g_task_get_source_object (task));
	CdSensorDummyPrivate *priv = cd_sensor_dummy_get_private (sensor);
	CdColorXYZ *sample = NULL;
	g_autoptr(GError) error = NULL;

	/* never setup */
	if (priv->transform_fake == NULL) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_NO_SUPPORT,
					 "no fake transfor set up");
		return G_SOURCE_REMOVE;
	}

	/* run the sample through the profile */
	sample = cd_color_xyz_new ();
	cmsDoTransform (priv->transform_fake, &priv->sample_fake, sample, 1);

	/* emulate */
	cd_sensor_button_pressed (sensor);

	/* just return without a problem */
	g_task_return_pointer (task, sample, (GDestroyNotify) cd_color_xyz_free);
	return G_SOURCE_REMOVE;
}

void
cd_sensor_get_sample_async (CdSensor *sensor,
			    CdSensorCap cap,
			    GCancellable *cancellable,
			    GAsyncReadyCallback callback,
			    gpointer user_data)
{
	g_autoptr(GTask) task = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	task = g_task_new (sensor, cancellable, callback, user_data);

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_MEASURING);

	/* just complete in idle */
	if (cap != CD_SENSOR_CAP_AMBIENT)
		g_timeout_add_seconds (2, (GSourceFunc) cd_sensor_get_sample_wait_cb, task);
	else
		g_timeout_add_seconds (2, (GSourceFunc) cd_sensor_get_ambient_wait_cb, task);
}

CdColorXYZ *
cd_sensor_get_sample_finish (CdSensor *sensor, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_pointer (G_TASK (res), error);
}

gboolean
cd_sensor_set_options_finish (CdSensor *sensor, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

void
cd_sensor_set_options_async (CdSensor *sensor,
			     GHashTable *options,
			     GCancellable *cancellable,
			     GAsyncReadyCallback callback,
			     gpointer user_data)
{
	CdSensorDummyPrivate *priv = cd_sensor_dummy_get_private (sensor);
	GList *l;
	const gchar *key_name;
	GVariant *value;
	g_autoptr(GTask) task = NULL;
	g_autoptr(GList) keys = NULL;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	task = g_task_new (sensor, cancellable, callback, user_data);

	/* look for any keys we recognise */
	keys = g_hash_table_get_keys (options);
	for (l = keys; l != NULL; l = l->next) {
		key_name = (const gchar *) l->data;
		value = g_hash_table_lookup (options, key_name);
		if (g_strcmp0 (g_variant_get_type_string (value), "d") != 0) {
			g_task_return_new_error (task,
						 CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_NO_SUPPORT,
						 "unexpected type '%s' not supported",
						 g_variant_get_type_string (value));
			return;
		}
		if (g_strcmp0 (key_name, "sample[red]") == 0) {
			priv->sample_fake.R = g_variant_get_double (value);
		} else if (g_strcmp0 (key_name, "sample[green]") == 0) {
			priv->sample_fake.G = g_variant_get_double (value);
		} else if (g_strcmp0 (key_name, "sample[blue]") == 0) {
			priv->sample_fake.B = g_variant_get_double (value);
		} else {
			g_task_return_new_error (task,
						 CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_NO_SUPPORT,
						 "option '%s' is not supported",
						 key_name);
			return;
		}
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

static void
cd_sensor_unref_private (CdSensorDummyPrivate *priv)
{
	if (priv->transform_fake != NULL)
		cmsDeleteTransform (priv->transform_fake);
	g_free (priv);
}

static cmsHTRANSFORM
cd_sensor_get_fake_transform (CdSensorDummyPrivate *priv)
{
	cmsHTRANSFORM transform;
	cmsHPROFILE profile_srgb;
	cmsHPROFILE profile_xyz;

	profile_srgb = cmsCreate_sRGBProfile ();
	profile_xyz = cmsCreateXYZProfile ();
	transform = cmsCreateTransform (profile_srgb, TYPE_RGB_DBL,
					profile_xyz, TYPE_XYZ_DBL,
					INTENT_RELATIVE_COLORIMETRIC,
					cmsFLAGS_NOOPTIMIZE);
	if (transform == NULL) {
		g_warning ("failed to setup RGB -> XYZ transform");
		goto out;
	}
out:
	if (profile_srgb != NULL)
		cmsCloseProfile (profile_srgb);
	if (profile_xyz != NULL)
		cmsCloseProfile (profile_xyz);
	return transform;
}

gboolean
cd_sensor_coldplug (CdSensor *sensor, GError **error)
{
	CdSensorDummyPrivate *priv;
	guint64 caps = cd_bitfield_from_enums (CD_SENSOR_CAP_LCD,
					       CD_SENSOR_CAP_CRT,
					       CD_SENSOR_CAP_PROJECTOR,
					       CD_SENSOR_CAP_SPOT,
					       CD_SENSOR_CAP_PRINTER,
					       CD_SENSOR_CAP_AMBIENT,
					       -1);
	g_object_set (sensor,
		      "id", "dummy",
		      "kind", CD_SENSOR_KIND_DUMMY,
		      "serial", "0123456789a",
		      "model", "Dummy Sensor #1",
		      "vendor", "Acme Corp",
		      "caps", caps,
		      "native", TRUE,
		      NULL);

	/* create private data */
	priv = g_new0 (CdSensorDummyPrivate, 1);
	priv->transform_fake = cd_sensor_get_fake_transform (priv);
	cd_color_rgb_set (&priv->sample_fake, 0.1, 0.2, 0.3);
	g_object_set_data_full (G_OBJECT (sensor), "priv", priv,
				(GDestroyNotify) cd_sensor_unref_private);
	return TRUE;
}

