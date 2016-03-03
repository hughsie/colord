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
	CdSpectrum			*dark_cal;
	CdSpectrum			*irradiance_cal;
	GFile				*dark_cal_file;
	GFile				*irradiance_cal_file;
	CdSpectrum			*sensitivity_cal;
} CdSensorSparkPrivate;

static CdSensorSparkPrivate *
cd_sensor_spark_get_private (CdSensor *sensor)
{
	return g_object_get_data (G_OBJECT (sensor), "priv");
}

static CdSpectrum *
cd_sensor_spark_get_dark_calibration (CdSensor *sensor, GError **error)
{
	CdSensorSparkPrivate *priv = cd_sensor_spark_get_private (sensor);
	CdSpectrum *sp;
	const gchar *kind;
	g_autoptr(CdIt8) it8 = NULL;
	g_autoptr(GError) error_local = NULL;

	/* take a baseline (hopefully black) */
	sp = osp_device_take_spectrum (priv->device, &error_local);
	if (sp == NULL) {
		g_set_error (error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_NO_DATA,
			     "failed to get spectrum: %s",
			     error_local->message);
		return NULL;
	}

	/* print something for debugging */
	if (g_getenv ("SPARK_DEBUG") != NULL) {
		g_autofree gchar *txt = NULL;
		txt = cd_spectrum_to_string (sp, 180, 20);
		g_print ("DARKCAL\n%s", txt);
	}

	/* save locally */
	if (priv->dark_cal != NULL)
		cd_spectrum_free (priv->dark_cal);
	priv->dark_cal = cd_spectrum_dup (sp);
	cd_spectrum_set_id (priv->dark_cal, "1");

	/* save to file */
	it8 = cd_it8_new ();
	kind = cd_sensor_kind_to_string (cd_sensor_get_kind (sensor));
	cd_it8_set_instrument (it8, kind);
	cd_it8_set_kind (it8, CD_IT8_KIND_SPECT);
	cd_it8_set_originator (it8, "colord");
	cd_it8_set_normalized (it8, FALSE);
	cd_it8_set_spectral (it8, TRUE);
	cd_it8_set_enable_created (it8, TRUE);
	cd_it8_set_title (it8, "Dark Calibration");
	cd_it8_add_spectrum (it8, sp);
	if (!cd_it8_save_to_file (it8,
				  priv->dark_cal_file,
				  &error_local)) {
		g_set_error (error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_INTERNAL,
			     "failed to save dark calibration: %s",
			     error_local->message);
		return NULL;
	}

	return sp;
}

static CdSpectrum *
cd_sensor_spark_get_irradiance_calibration (CdSensor *sensor, CdSpectrum *sp_in, GError **error)
{
	CdSensorSparkPrivate *priv = cd_sensor_spark_get_private (sensor);
	CdSpectrum *sp;
	const gchar *kind;
	guint i;
	g_autoptr(CdIt8) it8 = NULL;
	g_autoptr(CdSpectrum) sp_black_body = NULL;
	g_autoptr(GError) error_local = NULL;

	/* create reference spectra for a halogen bulb */
	sp_black_body = cd_spectrum_planckian_new_full (3200,
							cd_spectrum_get_start (sp_in),
							cd_spectrum_get_end (sp_in),
							1);
	cd_spectrum_normalize_max (sp_black_body, 1.f);
	if (g_getenv ("SPARK_DEBUG") != NULL) {
		g_autofree gchar *txt = NULL;
		txt = cd_spectrum_to_string (sp_black_body, 180, 20);
		g_print ("BLACKBODY@3200K\n%s", txt);
	}

	/* normalize the sensor result too */
	cd_spectrum_normalize_max (sp_in, 1.f);
	if (g_getenv ("SPARK_DEBUG") != NULL) {
		g_autofree gchar *txt = NULL;
		txt = cd_spectrum_to_string (sp_in, 180, 20);
		g_print ("NORMALIZED-SENSOR-RESPONSE\n%s", txt);
	}

	/* resample, calculating the correction curve */
	sp = cd_spectrum_new ();
	cd_spectrum_set_start (sp, cd_spectrum_get_start (sp_in));
	cd_spectrum_set_end (sp, cd_spectrum_get_end (sp_in));
	for (i = cd_spectrum_get_start (sp); i < cd_spectrum_get_end (sp); i += 5) {
		gdouble ref;
		gdouble val;
		ref = cd_spectrum_get_value_for_nm (sp_black_body, i);
		val = cd_spectrum_get_value_for_nm (sp_in, i);
		cd_spectrum_add_value (sp, ref / val);
	}
	cd_spectrum_normalize_max (sp, 1.f);

	/* try to use this to recreate the black body model */
	if (g_getenv ("SPARK_DEBUG") != NULL) {
		g_autofree gchar *txt = NULL;
		g_autoptr(CdSpectrum) sp_test = NULL;
		sp_test = cd_spectrum_multiply (sp, sp_in, 5);
		cd_spectrum_normalize_max (sp_test, 1.f);
		txt = cd_spectrum_to_string (sp_test, 180, 20);
		g_print ("CALIBRATED-RESPONSE\n%s", txt);
	}

	/* save locally */
	if (priv->irradiance_cal != NULL)
		cd_spectrum_free (priv->irradiance_cal);
	priv->irradiance_cal = cd_spectrum_dup (sp);
	cd_spectrum_set_id (priv->irradiance_cal, "1");

	/* save to file */
	it8 = cd_it8_new ();
	kind = cd_sensor_kind_to_string (cd_sensor_get_kind (sensor));
	cd_it8_set_instrument (it8, kind);
	cd_it8_set_kind (it8, CD_IT8_KIND_SPECT);
	cd_it8_set_originator (it8, "colord");
	cd_it8_set_normalized (it8, FALSE);
	cd_it8_set_spectral (it8, TRUE);
	cd_it8_set_enable_created (it8, TRUE);
	cd_it8_set_title (it8, "Dark Calibration");
	cd_it8_add_spectrum (it8, sp);
	if (!cd_it8_save_to_file (it8,
				  priv->irradiance_cal_file,
				  &error_local)) {
		g_set_error (error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_INTERNAL,
			     "failed to save irradiance calibration: %s",
			     error_local->message);
		return NULL;
	}


	return sp;
}

static CdSpectrum *
cd_sensor_spark_get_spectrum (CdSensor *sensor,
			      CdSensorCap cap,
			      GError **error)
{
	CdSensorSparkPrivate *priv = cd_sensor_spark_get_private (sensor);
	CdSpectrum *sp;
	g_autoptr(CdSpectrum) sp_tmp = NULL;
	g_autoptr(CdSpectrum) sp_biased = NULL;
	g_autoptr(GError) error_local = NULL;

	/* measure */
	cd_sensor_set_state_in_idle (sensor, CD_SENSOR_STATE_MEASURING);

	/* perform dark calibration */
	if (cap == CD_SENSOR_CAP_CALIBRATION_DARK)
		return cd_sensor_spark_get_dark_calibration (sensor, error);

	/* we have no dark calibration */
	if (priv->dark_cal == NULL ||
	    cd_spectrum_get_size (priv->dark_cal) == 0) {
		g_set_error_literal (error,
				     CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_REQUIRED_DARK_CALIBRATION,
				     "no dark calibration provided");
		return NULL;
	}

	/* get the spectrum */
	sp_tmp = osp_device_take_spectrum (priv->device, &error_local);
	if (sp_tmp == NULL) {
		g_set_error (error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_NO_DATA,
			     "failed to get spectrum: %s",
			     error_local->message);
		return NULL;
	}

	/* print something for debugging */
	if (g_getenv ("SPARK_DEBUG") != NULL) {
		g_autofree gchar *txt = NULL;
		txt = cd_spectrum_to_string (sp_tmp, 180, 20);
		g_print ("RAW\n%s", txt);
	}

	/* we don't have a method for getting this accurately yet */
	if (FALSE) {
		/* we have an invalid dark calibration */
		if (cd_spectrum_get_size (sp_tmp) !=
		    cd_spectrum_get_size (priv->dark_cal)) {
			g_set_error_literal (error,
					     CD_SENSOR_ERROR,
					     CD_SENSOR_ERROR_REQUIRED_DARK_CALIBRATION,
					     "dark calibration was invalid");
			return NULL;
		}

		/* print something for debugging */
		if (g_getenv ("SPARK_DEBUG") != NULL) {
			g_autofree gchar *txt = NULL;
			txt = cd_spectrum_to_string (priv->dark_cal, 180, 20);
			g_print ("DARKCAL\n%s", txt);
		}

		/* subtract the dark calibration */
		sp_biased = cd_spectrum_subtract (sp_tmp, priv->dark_cal, 5);
		if (sp_biased == NULL) {
			g_set_error_literal (error,
					     CD_SENSOR_ERROR,
					     CD_SENSOR_ERROR_NO_DATA,
					     "failed to get subtract spectra");
			return NULL;
		}

		/* print something for debugging */
		if (g_getenv ("SPARK_DEBUG") != NULL) {
			g_autofree gchar *txt = NULL;
			txt = cd_spectrum_to_string (sp_biased, 180, 20);
			g_print ("RAW-DARKCAL\n%s", txt);
		}
	} else {
		sp_biased = cd_spectrum_dup (sp_tmp);
	}

	/* ensure we never have negative readings */
	cd_spectrum_limit_min (sp_biased, 0.f);

	/* perform irradiance calibration */
	if (cap == CD_SENSOR_CAP_CALIBRATION_IRRADIANCE) {
		sp = cd_sensor_spark_get_irradiance_calibration (sensor, sp_biased, error);
		if (sp == NULL)
			return NULL;
	} else {
		g_autoptr(CdSpectrum) sp_resampled = NULL;
		g_autoptr(CdSpectrum) sp_irradiance = NULL;

		/* we have no irradiance calibration */
		if (priv->irradiance_cal == NULL ||
		    cd_spectrum_get_size (priv->irradiance_cal) == 0) {
			g_set_error_literal (error,
					     CD_SENSOR_ERROR,
					     CD_SENSOR_ERROR_REQUIRED_IRRADIANCE_CALIBRATION,
					     "no irradiance calibration provided");
			return NULL;
		}

		/* resample to a linear data space */
		sp_resampled = cd_spectrum_resample (sp_biased,
						     cd_spectrum_get_start (sp_biased),
						     cd_spectrum_get_end (sp_biased),
						     5);

		/* print something for debugging */
		if (g_getenv ("SPARK_DEBUG") != NULL) {
			g_autofree gchar *txt = NULL;
			txt = cd_spectrum_to_string (sp_resampled, 180, 20);
			g_print ("RESAMPLED\n%s", txt);
		}

		/* multiply with the irradiance calibration */
		sp_irradiance = cd_spectrum_multiply (sp_resampled, priv->irradiance_cal, 1);

		/* print something for debugging */
		if (g_getenv ("SPARK_DEBUG") != NULL) {
			g_autofree gchar *txt = NULL;
			txt = cd_spectrum_to_string (priv->irradiance_cal, 180, 20);
			g_print ("IRRADIANCECAL\n%s", txt);
		}

		/* multiply the spectrum with the sensitivity factor */
		sp = cd_spectrum_multiply (sp_irradiance, priv->sensitivity_cal, 1);
	}

	/* print something for debugging */
	if (g_getenv ("SPARK_DEBUG") != NULL) {
		g_autofree gchar *txt = NULL;
		txt = cd_spectrum_to_string (sp, 180, 20);
		g_print ("FINAL\n%s", txt);
	}
	return sp;
}

static void
cd_sensor_spark_sample_thread_cb (GTask *task,
				  gpointer source_object,
				  gpointer task_data,
				  GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (source_object);
	CdSensorCap cap = GPOINTER_TO_UINT (task_data);
	CdSpectrum *illuminant = NULL;
	g_autoptr(CdColorXYZ) sample = NULL;
	g_autoptr(CdIt8) it8_cmf = NULL;
	g_autoptr(CdIt8) it8_d65 = NULL;
	g_autoptr(CdSpectrum) sp = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file_cmf = NULL;
	g_autoptr(GFile) file_illuminant = NULL;

	/* get the correct spectra */
	sp = cd_sensor_spark_get_spectrum (sensor, cap, &error);
	if (sp == NULL) {
		g_task_return_new_error (task,
					 error->domain,
					 error->code,
					 "%s", error->message);
		return;
	}

	/* return dummy data */
	if (cap == CD_SENSOR_CAP_CALIBRATION) {
		CdColorXYZ *sample_tmp = NULL;
		sample_tmp = cd_color_xyz_new ();
		g_task_return_pointer (task, sample_tmp, NULL);
		return;
	}

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
						  sp, sample,
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
	g_return_val_if_fail (g_task_is_valid (res, sensor), NULL);
	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cd_sensor_spark_spectrum_thread_cb (GTask *task,
				    gpointer source_object,
				    gpointer task_data,
				    GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (source_object);
	CdSensorCap cap = GPOINTER_TO_UINT (task_data);
	CdSpectrum *spectrum;
	GError *error = NULL;

	/* success */
	spectrum = cd_sensor_spark_get_spectrum (sensor, cap, &error);
	if (spectrum == NULL) {
		g_task_return_error (task, error);
		return;
	}
	g_task_return_pointer (task, spectrum, NULL);
}

void
cd_sensor_get_spectrum_async (CdSensor *sensor,
			    CdSensorCap cap,
			    GCancellable *cancellable,
			    GAsyncReadyCallback callback,
			    gpointer user_data)
{
	g_autoptr(GTask) task = NULL;
	g_return_if_fail (CD_IS_SENSOR (sensor));
	task = g_task_new (sensor, cancellable, callback, user_data);
	g_task_set_task_data (task, GUINT_TO_POINTER (cap), NULL);
	g_task_run_in_thread (task, cd_sensor_spark_spectrum_thread_cb);
}

CdSpectrum *
cd_sensor_get_spectrum_finish (CdSensor *sensor, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, sensor), NULL);
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
	g_autofree gchar *fn = NULL;

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

	/* can we load a dark calibration? */
	fn = g_strdup_printf ("/var/lib/colord/sensor-%s-dark-cal-%s.sp",
			      cd_sensor_kind_to_string (cd_sensor_get_kind (sensor)),
			      serial_number_tmp);
	priv->dark_cal_file = g_file_new_for_path (fn);
	if (g_file_query_exists (priv->dark_cal_file, NULL)) {
		g_autoptr(CdIt8) it8 = cd_it8_new ();
		if (!cd_it8_load_from_file (it8,
					    priv->dark_cal_file,
					    &error)) {
			g_task_return_new_error (task,
						 CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_NO_DATA,
						 "%s", error->message);
			return;
		}
		priv->dark_cal = cd_spectrum_dup (cd_it8_get_spectrum_by_id (it8, "1"));
		g_debug ("loaded dark calibration with %i elements",
			 cd_spectrum_get_size (priv->dark_cal));

		/* print something for debugging */
		if (g_getenv ("SPARK_DEBUG") != NULL) {
			g_autofree gchar *txt = NULL;
			txt = cd_spectrum_to_string (priv->dark_cal, 180, 20);
			g_print ("DARKCAL:\n%s", txt);
		}
	}

	/* can we load a irradiance calibration? */
	fn = g_strdup_printf ("/var/lib/colord/sensor-%s-irradiance-cal-%s.sp",
			      cd_sensor_kind_to_string (cd_sensor_get_kind (sensor)),
			      serial_number_tmp);
	priv->irradiance_cal_file = g_file_new_for_path (fn);
	if (g_file_query_exists (priv->irradiance_cal_file, NULL)) {
		g_autoptr(CdIt8) it8 = cd_it8_new ();
		if (!cd_it8_load_from_file (it8,
					    priv->irradiance_cal_file,
					    &error)) {
			g_task_return_new_error (task,
						 CD_SENSOR_ERROR,
						 CD_SENSOR_ERROR_NO_DATA,
						 "%s", error->message);
			return;
		}
		priv->irradiance_cal = cd_spectrum_dup (cd_it8_get_spectrum_by_id (it8, "1"));
		g_debug ("loaded irradiance calibration with %i elements",
			 cd_spectrum_get_size (priv->irradiance_cal));

		/* print something for debugging */
		if (g_getenv ("SPARK_DEBUG") != NULL) {
			g_autofree gchar *txt = NULL;
			txt = cd_spectrum_to_string (priv->irradiance_cal, 180, 20);
			g_print ("IRRADIANCECAL:\n%s", txt);
		}
	}

	/* load the sensor sensitivity from a file */
	priv->sensitivity_cal = cd_spectrum_new ();
	cd_spectrum_set_start (priv->sensitivity_cal, 0);
	cd_spectrum_set_end (priv->sensitivity_cal, 1000);
	cd_spectrum_add_value (priv->sensitivity_cal, 34210); // <- FIXME: this needs to come from the device itself

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

	/* clear up the stuff we allocated in Lock() */
	g_clear_object (&priv->dark_cal_file);
	if (priv->sensitivity_cal != NULL) {
		cd_spectrum_free (priv->sensitivity_cal);
		priv->sensitivity_cal = NULL;
	}
	if (priv->dark_cal != NULL) {
		cd_spectrum_free (priv->dark_cal);
		priv->dark_cal = NULL;
	}
	g_clear_object (&priv->irradiance_cal_file);
	if (priv->sensitivity_cal != NULL) {
		cd_spectrum_free (priv->sensitivity_cal);
		priv->sensitivity_cal = NULL;
	}
	if (priv->irradiance_cal != NULL) {
		cd_spectrum_free (priv->irradiance_cal);
		priv->irradiance_cal = NULL;
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
	if (priv->dark_cal != NULL)
		cd_spectrum_free (priv->dark_cal);
	if (priv->irradiance_cal != NULL)
		cd_spectrum_free (priv->irradiance_cal);
	g_free (priv);
}

gboolean
cd_sensor_coldplug (CdSensor *sensor, GError **error)
{
	CdSensorSparkPrivate *priv;
	guint64 caps = cd_bitfield_from_enums (CD_SENSOR_CAP_LCD,
					       CD_SENSOR_CAP_CRT,
					       CD_SENSOR_CAP_CALIBRATION_DARK,
					       CD_SENSOR_CAP_CALIBRATION_IRRADIANCE,
					       CD_SENSOR_CAP_PLASMA,
					       CD_SENSOR_CAP_SPECTRAL,
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
