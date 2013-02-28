/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

/* async state for the sensor readings */
typedef struct {
	gboolean		 ret;
	CdColorXYZ		*sample;
	GSimpleAsyncResult	*res;
	CdSensor		*sensor;
} CdSensorAsyncState;

static CdSensorDummyPrivate *
cd_sensor_dummy_get_private (CdSensor *sensor)
{
	return g_object_get_data (G_OBJECT (sensor), "priv");
}

static void
cd_sensor_get_sample_state_finish (CdSensorAsyncState *state,
				   const GError *error)
{
	if (state->ret) {
		g_simple_async_result_set_op_res_gpointer (state->res,
							   state->sample,
							   (GDestroyNotify) cd_color_xyz_free);
	} else {
		g_simple_async_result_set_from_error (state->res, error);
	}

	/* set state */
	cd_sensor_set_state (state->sensor, CD_SENSOR_STATE_IDLE);

	/* complete */
	g_simple_async_result_complete_in_idle (state->res);

	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_slice_free (CdSensorAsyncState, state);
}

static gboolean
cd_sensor_get_ambient_wait_cb (CdSensorAsyncState *state)
{
	state->ret = TRUE;
	state->sample = cd_color_xyz_new ();
	state->sample->X = 7.7f;
	state->sample->Y = CD_SENSOR_NO_VALUE;
	state->sample->Z = CD_SENSOR_NO_VALUE;

	/* just return without a problem */
	cd_sensor_get_sample_state_finish (state, NULL);
	return G_SOURCE_REMOVE;
}

static gboolean
cd_sensor_get_sample_wait_cb (CdSensorAsyncState *state)
{
	CdSensorDummyPrivate *priv = cd_sensor_dummy_get_private (state->sensor);
	GError *error = NULL;

	/* never setup */
	if (priv->transform_fake == NULL) {
		g_set_error_literal (&error,
				     CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_NO_SUPPORT,
				     "no fake transfor set up");
		cd_sensor_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* run the sample through the profile */
	state->ret = TRUE;
	state->sample = cd_color_xyz_new ();
	cmsDoTransform (priv->transform_fake,
			&priv->sample_fake,
			state->sample, 1);

	/* emulate */
	cd_sensor_button_pressed (state->sensor);

	/* just return without a problem */
	cd_sensor_get_sample_state_finish (state, NULL);
out:
	return G_SOURCE_REMOVE;
}

void
cd_sensor_get_sample_async (CdSensor *sensor,
			    CdSensorCap cap,
			    GCancellable *cancellable,
			    GAsyncReadyCallback callback,
			    gpointer user_data)
{
	CdSensorAsyncState *state;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* save state */
	state = g_slice_new0 (CdSensorAsyncState);
	state->res = g_simple_async_result_new (G_OBJECT (sensor),
						callback,
						user_data,
						cd_sensor_get_sample_async);
	state->sensor = g_object_ref (sensor);

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_MEASURING);

	/* just complete in idle */
	if (cap != CD_SENSOR_CAP_AMBIENT)
		g_timeout_add_seconds (2, (GSourceFunc) cd_sensor_get_sample_wait_cb, state);
	else
		g_timeout_add_seconds (2, (GSourceFunc) cd_sensor_get_ambient_wait_cb, state);
}

CdColorXYZ *
cd_sensor_get_sample_finish (CdSensor *sensor,
			     GAsyncResult *res,
			     GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* failed */
	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	/* grab detail */
	return cd_color_xyz_dup (g_simple_async_result_get_op_res_gpointer (simple));
}

gboolean
cd_sensor_set_options_finish (CdSensor *sensor,
			      GAsyncResult *res,
			      GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* failed */
	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	/* grab detail */
	return g_simple_async_result_get_op_res_gboolean (simple);
}

void
cd_sensor_set_options_async (CdSensor *sensor,
			     GHashTable *options,
			     GCancellable *cancellable,
			     GAsyncReadyCallback callback,
			     gpointer user_data)
{
	CdSensorDummyPrivate *priv = cd_sensor_dummy_get_private (sensor);
	GSimpleAsyncResult *res;
	GList *keys;
	GList *l;
	gboolean ret = TRUE;
	const gchar *key_name;
	GVariant *value;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	res = g_simple_async_result_new (G_OBJECT (sensor),
					 callback,
					 user_data,
					 cd_sensor_set_options_async);

	/* look for any keys we recognise */
	keys = g_hash_table_get_keys (options);
	for (l = keys; l != NULL; l = l->next) {
		key_name = (const gchar *) l->data;
		value = g_hash_table_lookup (options, key_name);
		if (g_strcmp0 (g_variant_get_type_string (value), "d") != 0) {
			ret = FALSE;
			g_simple_async_result_set_error (res,
							 CD_SENSOR_ERROR,
							 CD_SENSOR_ERROR_NO_SUPPORT,
							 "unexpected type '%s' not supported",
							 g_variant_get_type_string (value));
			break;
		}
		if (g_strcmp0 (key_name, "sample[red]") == 0) {
			priv->sample_fake.R = g_variant_get_double (value);
		} else if (g_strcmp0 (key_name, "sample[green]") == 0) {
			priv->sample_fake.G = g_variant_get_double (value);
		} else if (g_strcmp0 (key_name, "sample[blue]") == 0) {
			priv->sample_fake.B = g_variant_get_double (value);
		} else {
			ret = FALSE;
			g_simple_async_result_set_error (res,
							 CD_SENSOR_ERROR,
							 CD_SENSOR_ERROR_NO_SUPPORT,
							 "option '%s' is not supported",
							 key_name);
			break;
		}
	}
	g_list_free (keys);

	/* success */
	if (ret)
		g_simple_async_result_set_op_res_gboolean (res, TRUE);

	/* complete */
	g_simple_async_result_complete_in_idle (res);
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

