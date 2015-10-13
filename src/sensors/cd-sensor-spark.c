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

static CdSensorSparkPrivate *
cd_sensor_spark_get_private (CdSensor *sensor)
{
	return g_object_get_data (G_OBJECT (sensor), "priv");
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
cd_sensor_spark_sample_thread_cb (GTask *task,
				  gpointer source_object,
				  gpointer task_data,
				  GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (source_object);
	CdSensorSparkPrivate *priv = cd_sensor_spark_get_private (sensor);
	CdSensorCap cap = GPOINTER_TO_UINT (task_data);
	CdSpectrum *illuminant = NULL;
	g_autoptr(CdColorXYZ) sample = NULL;
	g_autoptr(CdIt8) it8_cmf = NULL;
	g_autoptr(CdIt8) it8_d65 = NULL;
	g_autoptr(CdSpectrum) sp_new = NULL;
	g_autoptr(CdSpectrum) sp = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file_cmf = NULL;
	g_autoptr(GFile) file_illuminant = NULL;

	/* measure */
	cd_sensor_set_state_in_idle (sensor, CD_SENSOR_STATE_MEASURING);

	/* perform dark calibration */
	if (cap == CD_SENSOR_CAP_CALIBRATION) {
		sp = osp_device_take_spectrum (priv->device, &error);
		if (sp == NULL) {
			g_task_return_new_error (task,
						 CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_NO_DATA,
						 "%s", error->message);
			return;
		}
		_print_spectra (sp);
		if (priv->dark_calibration != NULL)
			cd_spectrum_free (priv->dark_calibration);
		priv->dark_calibration = cd_spectrum_dup (sp);
		cd_spectrum_set_id (priv->dark_calibration, "DarkCalibration");

		/* success */
		sample = cd_color_xyz_new ();
		g_task_return_pointer (task, sample, NULL);
		sample = NULL;
		return;
	}

	/* we have no dark calibration */
	if (priv->dark_calibration == NULL) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_REQUIRED_DARK_CALIBRATION,
					 "no dark calibration provided");
		return;
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
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_NO_SUPPORT,
					 "%s", error->message);
		return;
	}

	/* load D65 */
	it8_d65 = cd_it8_new ();
	file_illuminant = g_file_new_for_path ("/usr/share/colord/illuminant/CIE-D65.sp");
	if (!cd_it8_load_from_file (it8_d65, file_illuminant, &error)) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_NO_SUPPORT,
					 "%s", error->message);
		return;
	}
	illuminant = cd_it8_get_spectrum_by_id (it8_d65, "1");
	g_assert (illuminant != NULL);

	/* convert to XYZ */
	sample = cd_color_xyz_new ();
	if (!cd_it8_utils_calculate_xyz_from_cmf (it8_cmf, illuminant,
						  sp_new, sample,
						  1.f, &error)) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "%s", error->message);
		return;
	}

	/* success */
	g_task_return_pointer (task, sample, NULL);
	sample = NULL;
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
	g_task_set_task_data (task, GUINT_TO_POINTER (cap), NULL);
	g_task_run_in_thread (task, cd_sensor_spark_sample_thread_cb);
}

CdColorXYZ *
cd_sensor_get_sample_finish (CdSensor *sensor, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_sensor_spark_lock_thread_cb (GTask *task,
				gpointer source_object,
				gpointer task_data,
				GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (source_object);
	CdSensorSparkPrivate *priv = cd_sensor_spark_get_private (sensor);
	g_autoptr(GError) error = NULL;
	g_autofree gchar *serial_number_tmp = NULL;

	/* try to find the USB device */
	priv->device = cd_sensor_open_usb_device (sensor,
						  0x01, /* config */
						  0x00, /* interface */
						  &error);
	if (priv->device == NULL) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_INTERNAL,
					 "%s", error->message);
		return;
	}

	/* set state */
	cd_sensor_set_state_in_idle (sensor, CD_SENSOR_STATE_STARTING);

	/* get serial number */
	serial_number_tmp = osp_device_get_serial (priv->device, &error);
	if (serial_number_tmp == NULL) {
		g_task_return_new_error (task,
					 CD_SENSOR_ERROR,
					 CD_SENSOR_ERROR_NO_DATA,
					 "%s", error->message);
		return;
	}
	cd_sensor_set_serial (sensor, serial_number_tmp);
	g_debug ("Serial number: %s", serial_number_tmp);

	/* success */
	g_task_return_boolean (task, TRUE);
}

void
cd_sensor_lock_async (CdSensor *sensor,
		      GCancellable *cancellable,
		      GAsyncReadyCallback callback,
		      gpointer user_data)
{
	g_autoptr(GTask) task = NULL;
	g_return_if_fail (CD_IS_SENSOR (sensor));
	task = g_task_new (sensor, cancellable, callback, user_data);
	g_task_run_in_thread (task, cd_sensor_spark_lock_thread_cb);
}

gboolean
cd_sensor_lock_finish (CdSensor *sensor,
		       GAsyncResult *res,
		       GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_sensor_unlock_thread_cb (GTask *task,
			    gpointer source_object,
			    gpointer task_data,
			    GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (source_object);
	CdSensorSparkPrivate *priv = cd_sensor_spark_get_private (sensor);
	g_autoptr(GError) error = NULL;

	/* close */
	if (priv->device != NULL) {
		if (!g_usb_device_close (priv->device, &error)) {
			g_task_return_new_error (task,
						 CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_INTERNAL,
						 "%s", error->message);
			return;
		}
		g_clear_object (&priv->device);
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

void
cd_sensor_unlock_async (CdSensor *sensor,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data)
{
	g_autoptr(GTask) task = NULL;
	g_return_if_fail (CD_IS_SENSOR (sensor));
	task = g_task_new (sensor, cancellable, callback, user_data);
	g_task_run_in_thread (task, cd_sensor_unlock_thread_cb);
}

gboolean
cd_sensor_unlock_finish (CdSensor *sensor, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
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
