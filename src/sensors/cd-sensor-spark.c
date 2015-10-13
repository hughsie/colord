/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
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
 * This object implements a driver for the OceanOptics SPark hardware.
 */

#include "config.h"

#include <glib-object.h>
#include <colord-private.h>
#include <ospark/ospark.h>

#include "../src/cd-sensor.h"

typedef struct
{
	GUsbDevice			*device;
	CdSpectrum			*dark_calibration;
} CdSensorSparkPrivate;

/* async state for the sensor readings */
typedef struct {
	gboolean			 ret;
	CdColorXYZ			*sample;
	gulong				 cancellable_id;
	GCancellable			*cancellable;
	GSimpleAsyncResult		*res;
	CdSensor			*sensor;
	CdSensorCap			 cap;
} CdSensorAsyncState;

static CdSensorSparkPrivate *
cd_sensor_spark_get_private (CdSensor *sensor)
{
	return g_object_get_data (G_OBJECT (sensor), "priv");
}

static void
cd_sensor_spark_get_sample_state_finish (CdSensorAsyncState *state,
					const GError *error)
{
	/* set result to temp memory location */
	if (state->ret) {
		g_simple_async_result_set_op_res_gpointer (state->res,
							   state->sample,
							   (GDestroyNotify) cd_color_xyz_free);
	} else {
		g_simple_async_result_set_from_error (state->res, error);
	}

	/* deallocate */
	if (state->cancellable != NULL) {
		g_cancellable_disconnect (state->cancellable, state->cancellable_id);
		g_object_unref (state->cancellable);
	}

	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_slice_free (CdSensorAsyncState, state);
}

static void
cd_sensor_spark_cancellable_cancel_cb (GCancellable *cancellable,
				      CdSensorAsyncState *state)
{
	g_warning ("cancelled spark");
}

static void
_print_spectra (CdSpectrum *sp)
{
	guint i, j;
	for (i = 0; i < 1024; i+=5) {
		g_print ("%.1fnm: ", cd_spectrum_get_wavelength (sp, i));
		for (j = 0; j < cd_spectrum_get_value_raw (sp, i) * 1000.f; j++)
			g_print ("*");
		g_print ("\n");
	}
}

static void
cd_sensor_spark_sample_thread_cb (GSimpleAsyncResult *res,
				 GObject *object,
				 GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) g_object_get_data (G_OBJECT (cancellable), "state");
	CdSensorSparkPrivate *priv = cd_sensor_spark_get_private (sensor);
	g_autoptr(CdIt8) it8_cmf = NULL;
	g_autoptr(CdIt8) it8_d65 = NULL;
	g_autoptr(CdSpectrum) sp_new = NULL;
	g_autoptr(CdSpectrum) sp = NULL;
	g_autoptr(CdSpectrum) illuminant = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file_cmf = NULL;
	g_autoptr(GFile) file_illuminant = NULL;

	/* measure */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_MEASURING);

	/* perform dark calibration */
	if (state->cap == CD_SENSOR_CAP_CALIBRATION) {
		sp = osp_device_take_spectrum (priv->device, &error);
		if (sp == NULL) {
			cd_sensor_spark_get_sample_state_finish (state, error);
			goto out;
		}
		_print_spectra (sp);
		if (priv->dark_calibration != NULL)
			cd_spectrum_free (priv->dark_calibration);
		priv->dark_calibration = cd_spectrum_dup (sp);
		cd_spectrum_set_id (priv->dark_calibration, "DarkCalibration");

		/* success */
		state->ret = TRUE;
		state->sample = cd_color_xyz_new ();
		cd_sensor_spark_get_sample_state_finish (state, NULL);
		goto out;
	}

	/* we have no dark calibration */
	if (priv->dark_calibration == NULL) {
		g_set_error_literal (&error,
				     CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_REQUIRED_DARK_CALIBRATION,
				     "no dark calibration provided");
		cd_sensor_spark_get_sample_state_finish (state, error);
		goto out;
	}

	/* get the color */
	sp = osp_device_take_spectrum (priv->device, &error);
	_print_spectra (sp);

	/* subtract the dark calibration */
	sp_new = cd_spectrum_subtract (sp, priv->dark_calibration);
	_print_spectra (sp_new);

	/* load CIE1931 */
	it8_cmf = cd_it8_new ();
	file_cmf = g_file_new_for_path ("/usr/share/colord/cmf/CIE1931-2deg-XYZ.cmf");
	if (!cd_it8_load_from_file (it8_cmf, file_cmf, &error)) {
		cd_sensor_spark_get_sample_state_finish (state, error);
		goto out;
	}

	/* load D65 */
	it8_d65 = cd_it8_new ();
	file_illuminant = g_file_new_for_path ("/usr/share/colord/illuminant/CIE-D65.sp");
	if (!cd_it8_load_from_file (it8_d65, file_illuminant, &error)) {
		cd_sensor_spark_get_sample_state_finish (state, error);
		goto out;
	}
	illuminant = cd_it8_get_spectrum_by_id (it8_d65, "1");
	g_assert (illuminant != NULL);

	/* convert to XYZ */
	state->sample = cd_color_xyz_new ();
	if (!cd_it8_utils_calculate_xyz_from_cmf (it8_cmf, illuminant,
						  sp_new, state->sample,
						  1.f, &error)) {
		cd_sensor_spark_get_sample_state_finish (state, error);
		goto out;
	}

	/* success */
	state->ret = TRUE;
	cd_sensor_spark_get_sample_state_finish (state, NULL);
out:

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
}

void
cd_sensor_get_sample_async (CdSensor *sensor,
			    CdSensorCap cap,
			    GCancellable *cancellable,
			    GAsyncReadyCallback callback,
			    gpointer user_data)
{
	CdSensorAsyncState *state;
	GCancellable *tmp;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* save state */
	state = g_slice_new0 (CdSensorAsyncState);
	state->res = g_simple_async_result_new (G_OBJECT (sensor),
						callback,
						user_data,
						cd_sensor_get_sample_async);
	state->sensor = g_object_ref (sensor);
	state->cap = cap;
	if (cancellable != NULL) {
		state->cancellable = g_object_ref (cancellable);
		state->cancellable_id = g_cancellable_connect (cancellable, G_CALLBACK (cd_sensor_spark_cancellable_cancel_cb), state, NULL);
	}

	/* run in a thread */
	tmp = g_cancellable_new ();
	g_object_set_data (G_OBJECT (tmp), "state", state);
	g_simple_async_result_run_in_thread (G_SIMPLE_ASYNC_RESULT (state->res),
					     cd_sensor_spark_sample_thread_cb,
					     0,
					     (GCancellable*) tmp);
	g_object_unref (tmp);
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

static void
cd_sensor_spark_lock_thread_cb (GSimpleAsyncResult *res,
			       GObject *object,
			       GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorSparkPrivate *priv = cd_sensor_spark_get_private (sensor);
	g_autoptr(GError) error = NULL;
	g_autofree gchar *serial_number_tmp = NULL;

	/* try to find the USB device */
	priv->device = cd_sensor_open_usb_device (sensor,
						  0x01, /* config */
						  0x00, /* interface */
						  &error);
	if (priv->device == NULL) {
		cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
		g_simple_async_result_set_from_error (res, error);
		goto out;
	}

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_STARTING);

	/* get serial number */
	serial_number_tmp = osp_device_get_serial (priv->device, &error);
	if (serial_number_tmp == NULL) {
		g_simple_async_result_set_from_error (res, error);
		goto out;
	}
	cd_sensor_set_serial (sensor, serial_number_tmp);
	g_debug ("Serial number: %s", serial_number_tmp);
out:
	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
}

void
cd_sensor_lock_async (CdSensor *sensor,
		      GCancellable *cancellable,
		      GAsyncReadyCallback callback,
		      gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* run in a thread */
	res = g_simple_async_result_new (G_OBJECT (sensor),
					 callback,
					 user_data,
					 cd_sensor_lock_async);
	g_simple_async_result_run_in_thread (res,
					     cd_sensor_spark_lock_thread_cb,
					     0,
					     cancellable);
	g_object_unref (res);
}

gboolean
cd_sensor_lock_finish (CdSensor *sensor,
		       GAsyncResult *res,
		       GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;
	return TRUE;
}

static void
cd_sensor_unlock_thread_cb (GSimpleAsyncResult *res,
			    GObject *object,
			    GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorSparkPrivate *priv = cd_sensor_spark_get_private (sensor);
	gboolean ret = FALSE;
	g_autoptr(GError) error = NULL;

	/* close */
	if (priv->device != NULL) {
		ret = g_usb_device_close (priv->device, &error);
		if (!ret) {
			g_simple_async_result_set_from_error (res, error);
			return;
		}

		/* clear */
		g_object_unref (priv->device);
		priv->device = NULL;
	}
}

void
cd_sensor_unlock_async (CdSensor *sensor,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* run in a thread */
	res = g_simple_async_result_new (G_OBJECT (sensor),
					 callback,
					 user_data,
					 cd_sensor_unlock_async);
	g_simple_async_result_run_in_thread (res,
					     cd_sensor_unlock_thread_cb,
					     0,
					     cancellable);
	g_object_unref (res);
}

gboolean
cd_sensor_unlock_finish (CdSensor *sensor,
			 GAsyncResult *res,
			 GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;
	return TRUE;
}

static void
cd_sensor_unref_private (CdSensorSparkPrivate *priv)
{
	if (priv->device != NULL)
		g_object_unref (priv->device);
	if (priv->dark_calibration != NULL)
		cd_spectrum_free (priv->dark_calibration);
	g_free (priv);
}

gboolean
cd_sensor_coldplug (CdSensor *sensor, GError **error)
{
	CdSensorSparkPrivate *priv;
	guint64 caps = cd_bitfield_from_enums (CD_SENSOR_CAP_LCD,
					       CD_SENSOR_CAP_CRT,
					       CD_SENSOR_CAP_CALIBRATION,
					       CD_SENSOR_CAP_PLASMA,
					       -1);
	g_object_set (sensor,
		      "native", TRUE,
		      "kind", CD_SENSOR_KIND_SPARK,
		      "caps", caps,
		      NULL);
	/* create private data */
	priv = g_new0 (CdSensorSparkPrivate, 1);
	g_object_set_data_full (G_OBJECT (sensor), "priv", priv,
				(GDestroyNotify) cd_sensor_unref_private);
	return TRUE;
}

