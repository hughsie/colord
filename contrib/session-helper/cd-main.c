/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2012 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <lcms2.h>
#include <math.h>

#include "cd-debug.h"
#include "cd-state.h"
#include "cd-lcms-helpers.h"
#include "cd-client-sync.h"
#include "cd-device-sync.h"
#include "cd-profile-sync.h"
#include "cd-it8.h"
#include "cd-sensor-sync.h"
#include "cd-session.h"

typedef struct {
	/* global */
	CdClient		*client;
	CdSessionStatus		 status;
	GDBusConnection		*connection;
	GDBusNodeInfo		*introspection;
	GMainLoop		*loop;
	guint32			 progress;
	guint			 watcher_id;
	CdState			*state;

	/* for the task */
	CdSessionInteraction	 interaction_code_last;
	CdSensor		*sensor;
	CdDevice		*device;
	CdSensorCap		 device_kind;
	GPtrArray		*array;
	cmsCIEXYZ		 whitepoint;
	gdouble			 native_whitepoint;
	guint			 target_whitepoint;
	guint			 screen_brightness;
	CdIt8			*it8_cal;
	CdIt8			*it8_ti1;
	CdIt8			*it8_ti3;
	CdProfileQuality	 quality;
	GCancellable		*cancellable;
	gchar			*title;
	gchar			*basename;
	gchar			*working_path;
} CdMainPrivate;

typedef struct {
	CdColorRGB		 color;
	CdColorRGB		 best_so_far;
	gdouble			 error;
} CdMainCalibrateItem;

#define CD_SESSION_ERROR			cd_main_error_quark()

/**
 * cd_main_error_to_string:
 **/
static const gchar *
cd_main_error_to_string (CdSessionError error_enum)
{
	if (error_enum == CD_SESSION_ERROR_INTERNAL)
		return CD_SESSION_DBUS_SERVICE ".Internal";
	if (error_enum == CD_SESSION_ERROR_FAILED_TO_FIND_DEVICE)
		return CD_SESSION_DBUS_SERVICE ".FailedToFindDevice";
	if (error_enum == CD_SESSION_ERROR_FAILED_TO_FIND_SENSOR)
		return CD_SESSION_DBUS_SERVICE ".FailedToFindSensor";
	if (error_enum == CD_SESSION_ERROR_FAILED_TO_FIND_TOOL)
		return CD_SESSION_DBUS_SERVICE ".FailedToFindTool";
	if (error_enum == CD_SESSION_ERROR_FAILED_TO_GENERATE_PROFILE)
		return CD_SESSION_DBUS_SERVICE ".FailedToGenerateProfile";
	if (error_enum == CD_SESSION_ERROR_FAILED_TO_GET_WHITEPOINT)
		return CD_SESSION_DBUS_SERVICE ".FailedToGetWhitepoint";
	if (error_enum == CD_SESSION_ERROR_FAILED_TO_OPEN_PROFILE)
		return CD_SESSION_DBUS_SERVICE ".FailedToOpenProfile";
	if (error_enum == CD_SESSION_ERROR_FAILED_TO_SAVE_PROFILE)
		return CD_SESSION_DBUS_SERVICE ".FailedToSaveProfile";
	if (error_enum == CD_SESSION_ERROR_INVALID_VALUE)
		return CD_SESSION_DBUS_SERVICE ".InvalidValue";
	return NULL;
}

/**
 * cd_main_error_quark:
 **/
static GQuark
cd_main_error_quark (void)
{
	guint i;
	static GQuark quark = 0;
	if (!quark) {
		quark = g_quark_from_static_string ("colord");
		for (i = 1; i < CD_SESSION_ERROR_LAST; i++) {
			g_dbus_error_register_error (quark,
						     i,
						     cd_main_error_to_string (i));
		}
	}
	return quark;
}

/**
 * cd_main_calib_idle_delay_cb:
 **/
static gboolean
cd_main_calib_idle_delay_cb (gpointer user_data)
{
	GMainLoop *loop = (GMainLoop *) user_data;
	g_main_loop_quit (loop);
	return FALSE;
}

/**
 * cd_main_calib_idle_delay:
 **/
static void
cd_main_calib_idle_delay (guint ms)
{
	GMainLoop *loop;
	loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add (ms, cd_main_calib_idle_delay_cb, loop);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
}

/**
 * cd_main_emit_update_sample:
 **/
static void
cd_main_emit_update_sample (CdMainPrivate *priv, CdColorRGB *color)
{
	/* emit signal */
	g_debug ("CdMain: Emitting UpdateSample(%f,%f,%f)",
		 color->R, color->G, color->B);
	g_dbus_connection_emit_signal (priv->connection,
				       NULL,
				       CD_SESSION_DBUS_PATH,
				       CD_SESSION_DBUS_INTERFACE_DISPLAY,
				       "UpdateSample",
				       g_variant_new ("(ddd)",
						      color->R,
						      color->G,
						      color->B),
				       NULL);
	cd_main_calib_idle_delay (200);
}

/**
 * cd_main_get_display_ti1:
 **/
static const gchar *
cd_main_get_display_ti1 (CdProfileQuality quality)
{
	switch (quality) {
	case CD_PROFILE_QUALITY_LOW:
		return "display-short.ti1";
	case CD_PROFILE_QUALITY_MEDIUM:
		return "display-normal.ti1";
	case CD_PROFILE_QUALITY_HIGH:
		return "display-long.ti1";
	default:
		break;
	}
	return NULL;
}

/**
 * cd_main_emit_interaction_required:
 **/
static void
cd_main_emit_interaction_required (CdMainPrivate *priv,
				   CdSessionInteraction code)
{
	const gchar *image = NULL;
	const gchar *message = NULL;
	gchar *path;

	/* save so we know what was asked for */
	priv->interaction_code_last = code;

	/* emit signal */
	switch (code) {
	case CD_SESSION_INTERACTION_ATTACH_TO_SCREEN:
		image = cd_sensor_get_metadata_item (priv->sensor,
						     CD_SENSOR_METADATA_IMAGE_ATTACH);
		message = "attach the sensor to the screen";
		break;
	case CD_SESSION_INTERACTION_MOVE_TO_SURFACE:
		image = cd_sensor_get_metadata_item (priv->sensor,
						     CD_SENSOR_METADATA_IMAGE_SCREEN);
		message = "move the sensor to the surface position";
		break;
	case CD_SESSION_INTERACTION_MOVE_TO_CALIBRATION:
		image = cd_sensor_get_metadata_item (priv->sensor,
						     CD_SENSOR_METADATA_IMAGE_CALIBRATE);
		message = "move the sensor to the calibrate position";
		break;
	case CD_SESSION_INTERACTION_SHUT_LAPTOP_LID:
		message = "shut the laptop lid";
		break;
	default:
		message = "";
		break;
	}
	if (image != NULL) {
		path = g_build_filename (DATADIR,
					 "colord",
					 "icons",
					 image,
					 NULL);
	} else {
		path = g_strdup ("");
	}
	g_debug ("CdMain: Emitting InteractionRequired(%i,%s,%s)",
		 code, message, path);
	g_dbus_connection_emit_signal (priv->connection,
				       NULL,
				       CD_SESSION_DBUS_PATH,
				       CD_SESSION_DBUS_INTERFACE_DISPLAY,
				       "InteractionRequired",
				       g_variant_new ("(uss)",
						      code,
						      message,
						      path),
				       NULL);
	g_free (path);
}

/**
 * cd_main_emit_update_gamma:
 **/
static void
cd_main_emit_update_gamma (CdMainPrivate *priv,
			   GPtrArray *array)
{
	GVariantBuilder builder;
	guint i;
	CdColorRGB *color;

	/* emit signal */
	g_debug ("CdMain: Emitting UpdateGamma(%i elements)",
		 array->len);

	/* build the dict */
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	for (i = 0; i < array->len; i++) {
		color = g_ptr_array_index (array, i);
		g_variant_builder_add (&builder,
				       "(ddd)",
				       color->R,
				       color->G,
				       color->B);
	}
	g_dbus_connection_emit_signal (priv->connection,
				       NULL,
				       CD_SESSION_DBUS_PATH,
				       CD_SESSION_DBUS_INTERFACE_DISPLAY,
				       "UpdateGamma",
				       g_variant_new ("(a(ddd))",
						      &builder),
				       NULL);
	cd_main_calib_idle_delay (200);
}

/**
 * cd_main_emit_finished:
 **/
static void
cd_main_emit_finished (CdMainPrivate *priv,
		       CdSessionError exit_code,
		       const gchar *message)
{
	/* emit signal */
	g_debug ("CdMain: Emitting Finished(%u,%s)",
		 exit_code, message);
	g_dbus_connection_emit_signal (priv->connection,
				       NULL,
				       CD_SESSION_DBUS_PATH,
				       CD_SESSION_DBUS_INTERFACE_DISPLAY,
				       "Finished",
				       g_variant_new ("(us)",
						      exit_code,
						      message != NULL ? message : ""),
				       NULL);
}

/**
 * cd_main_calib_get_sample:
 **/
static gboolean
cd_main_calib_get_sample (CdMainPrivate *priv,
			  CdColorXYZ *xyz,
			  GError **error)
{
	CdColorXYZ *xyz_tmp;
	gboolean ret = TRUE;

	xyz_tmp = cd_sensor_get_sample_sync (priv->sensor,
					     priv->device_kind,
					     priv->cancellable,
					     error);
	if (xyz_tmp == NULL) {
		ret = FALSE;
		goto out;
	}
	cd_color_xyz_copy (xyz_tmp, xyz);
out:
	if (xyz_tmp != NULL)
		cd_color_xyz_free (xyz_tmp);
	return ret;
}

/**
 * cd_main_calib_get_native_whitepoint:
 **/
static gboolean
cd_main_calib_get_native_whitepoint (CdMainPrivate *priv,
				     gdouble *temp,
				     GError **error)
{
	CdColorRGB rgb;
	CdColorXYZ xyz;
	cmsCIExyY chroma;
	gboolean ret = TRUE;

	rgb.R = 1.0;
	rgb.G = 1.0;
	rgb.B = 1.0;
	cd_main_emit_update_sample (priv, &rgb);

	ret = cd_main_calib_get_sample (priv, &xyz, error);
	if (!ret)
		goto out;

	cmsXYZ2xyY (&chroma, (cmsCIEXYZ *) &xyz);
	g_debug ("x:%f,y:%f,Y:%f", chroma.x, chroma.y, chroma.Y);
	cmsTempFromWhitePoint (temp, &chroma);
out:
	return ret;
}

/**
 * cd_main_calib_try_item:
 **/
static gboolean
cd_main_calib_try_item (CdMainPrivate *priv,
		        CdMainCalibrateItem *item,
		        gboolean *new_best,
		        GError **error)
{
	gboolean ret = TRUE;
	gdouble error_tmp;
	CdColorXYZ xyz;
	cmsCIELab lab;

	g_debug ("try %f,%f,%f", item->color.R, item->color.G, item->color.B);
	cd_main_emit_update_gamma (priv, priv->array);

	/* get the sample using the default matrix */
	ret = cd_main_calib_get_sample (priv, &xyz, error);
	if (!ret)
		goto out;

	/* get error */
	cmsXYZ2Lab (&priv->whitepoint, &lab, (const cmsCIEXYZ *) &xyz);
	error_tmp = sqrt (lab.a * lab.a + lab.b * lab.b);
	g_debug ("%f\t%f\t%f = %f", lab.L, lab.a, lab.b, error_tmp);
	if (error_tmp < item->error) {
		cd_color_rgb_copy (&item->color, &item->best_so_far);
		item->error = error_tmp;
		if (new_best != NULL)
			*new_best = TRUE;
	}
out:
	return ret;
}

/**
 * cd_main_calib_process_item:
 **/
static gboolean
cd_main_calib_process_item (CdMainPrivate *priv,
			    CdMainCalibrateItem *item,
			    CdState *state,
			    GError **error)
{
	CdState *state_local;
	gboolean new_best = FALSE;
	gboolean ret = TRUE;
	gdouble good_enough_interval = 0.0f;
	gdouble interval = 0.05;
	gdouble tmp;
	guint i;
	guint number_steps = 0;

	/* reset the state */
	ret = cd_state_set_steps (state,
				  error,
				  3,	/* get baseline sample */
				  97,	/* get other samples */
				  -1);
	if (!ret)
		goto out;

	/* copy the current color balance as the best */
	cd_color_rgb_copy (&item->color, &item->best_so_far);

	/* get a baseline error */
	ret = cd_main_calib_try_item (priv, item, NULL, error);
	if (!ret)
		goto out;

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;

	/* use a different smallest interval for each quality */
	if (priv->quality == CD_PROFILE_QUALITY_LOW) {
		good_enough_interval = 0.009;
	} else if (priv->quality == CD_PROFILE_QUALITY_MEDIUM) {
		good_enough_interval = 0.006;
	} else if (priv->quality == CD_PROFILE_QUALITY_HIGH) {
		good_enough_interval = 0.003;
	}

	/* do the progress the best we can */
	state_local = cd_state_get_child (state);
	for (tmp = interval; tmp > good_enough_interval; tmp /= 2)
		number_steps++;
	cd_state_set_number_steps (state_local, number_steps);
	for (i = 0; i < 500; i++) {

		/* check if cancelled */
		ret = g_cancellable_set_error_if_cancelled (priv->cancellable,
							    error);
		if (ret) {
			ret = FALSE;
			goto out;
		}

		/* blue */
		cd_color_rgb_copy (&item->best_so_far, &item->color);
		if (item->best_so_far.B > interval) {
			item->color.B = item->best_so_far.B - interval;
			ret = cd_main_calib_try_item (priv, item, &new_best, error);
			if (!ret)
				goto out;
			if (new_best) {
				g_debug ("New best: blue down by %f", interval);
				new_best = FALSE;
				continue;
			}
		}
		if (item->best_so_far.B < 1.0 - interval) {
			item->color.B = item->best_so_far.B + interval;
			ret = cd_main_calib_try_item (priv, item, &new_best, error);
			if (!ret)
				goto out;
			if (new_best) {
				g_debug ("New best: blue up by %f", interval);
				new_best = FALSE;
				continue;
			}
		}

		/* red */
		cd_color_rgb_copy (&item->best_so_far, &item->color);
		if (item->best_so_far.R > interval) {
			item->color.R = item->best_so_far.R - interval;
			ret = cd_main_calib_try_item (priv, item, &new_best, error);
			if (!ret)
				goto out;
			if (new_best) {
				g_debug ("New best: red down by %f", interval);
				new_best = FALSE;
				continue;
			}
		}
		if (item->best_so_far.R < 1.0 - interval) {
			item->color.R = item->best_so_far.R + interval;
			ret = cd_main_calib_try_item (priv, item, &new_best, error);
			if (!ret)
				goto out;
			if (new_best) {
				g_debug ("New best: red up by %f", interval);
				new_best = FALSE;
				continue;
			}
		}

		/* green */
		cd_color_rgb_copy (&item->best_so_far, &item->color);
		if (item->best_so_far.G > interval) {
			item->color.G = item->best_so_far.G - interval;
			ret = cd_main_calib_try_item (priv, item, &new_best, error);
			if (!ret)
				goto out;
			if (new_best) {
				g_debug ("New best: green down by %f", interval);
				new_best = FALSE;
				continue;
			}
		}
		if (item->best_so_far.G < 1.0 - interval) {
			item->color.G = item->best_so_far.G + interval;
			ret = cd_main_calib_try_item (priv, item, &new_best, error);
			if (!ret)
				goto out;
			if (new_best) {
				g_debug ("New best: green up by %f", interval);
				new_best = FALSE;
				continue;
			}
		}

		/* done */
		ret = cd_state_done (state_local, error);
		if (!ret)
			goto out;

		/* done */
		interval /= 2;
		if (interval < good_enough_interval) {
			g_debug ("no improvement, best RGB was: %f,%f,%f",
				 item->best_so_far.R,
				 item->best_so_far.G,
				 item->best_so_far.B);
			break;
		}
	}

	/* save this */
	cd_color_rgb_copy (&item->best_so_far,
			   &item->color);

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * cd_main_calib_interpolate_up:
 *
 * Interpolate from the current number of points to a new size
 **/
static gboolean
cd_main_calib_interpolate_up (CdMainPrivate *priv,
			      guint new_size,
			      GError **error)
{
	CdMainCalibrateItem *p1;
	CdMainCalibrateItem *p2;
	CdMainCalibrateItem *result;
	gboolean ret = TRUE;
	gdouble mix;
	guint i;
	GPtrArray *old_array;

	/* make a deep copy */
	old_array = g_ptr_array_new_with_free_func (g_free);
	for (i = 0; i < priv->array->len; i++) {
		p1 = g_ptr_array_index (priv->array, i);
		result = g_new (CdMainCalibrateItem, 1);
		result->error = p1->error;
		cd_color_rgb_copy (&p1->color, &result->color);
		g_ptr_array_add (old_array, result);
	}

	/* interpolate the new array */
	g_ptr_array_set_size (priv->array, 0);
	for (i = 0; i < new_size; i++) {
		mix = (gdouble) (old_array->len - 1) /
			(gdouble) (new_size - 1) *
			(gdouble) i;
		p1 = g_ptr_array_index (old_array, (guint) floor (mix));
		p2 = g_ptr_array_index (old_array, (guint) ceil (mix));
		result = g_new (CdMainCalibrateItem, 1);
		result->error = G_MAXDOUBLE;
		cd_color_rgb_set (&result->color, 1.0, 1.0, 1.0);
		cd_color_rgb_interpolate (&p1->color,
					  &p2->color,
					  mix - (gint) mix,
					  &result->color);
		g_ptr_array_add (priv->array, result);
	}
//out:
	g_ptr_array_unref (old_array);
	return ret;
}

/**
 * cd_main_calib_process:
 **/
static gboolean
cd_main_calib_process (CdMainPrivate *priv,
		       CdState *state,
		       GError **error)
{
	CdColorRGB rgb;
	CdMainCalibrateItem *item;
	CdState *state_local;
	CdState *state_loop;
	cmsCIExyY whitepoint_tmp;
	gboolean ret;
	gdouble temp;
	guint i;
	guint precision_steps = 0;

	/* reset the state */
	ret = cd_state_set_steps (state,
				  error,
				  1,	/* get native whitepoint */
				  3,	/* normalize white */
				  94,	/* refine other points */
				  1,	/* get new whitepoint */
				  1,	/* write calibrate point */
				  -1);
	if (!ret)
		goto out;

	/* clear gamma ramp to linear */
	priv->array = g_ptr_array_new_with_free_func (g_free);
	item = g_new0 (CdMainCalibrateItem, 1);
	item->error = G_MAXDOUBLE;
	cd_color_rgb_set (&item->color, 0.0, 0.0, 0.0);
	g_ptr_array_add (priv->array, item);
	item = g_new0 (CdMainCalibrateItem, 1);
	item->error = G_MAXDOUBLE;
	cd_color_rgb_set (&item->color, 1.0, 1.0, 1.0);
	g_ptr_array_add (priv->array, item);
	cd_main_emit_update_gamma (priv, priv->array);

	/* get whitepoint */
	ret = cd_main_calib_get_native_whitepoint (priv, &priv->native_whitepoint, error);
	if (!ret)
		goto out;
	if (priv->native_whitepoint < 1000 ||
	    priv->native_whitepoint > 100000) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_SESSION_ERROR,
				     CD_SESSION_ERROR_FAILED_TO_GET_WHITEPOINT,
				     "failed to get native temperature");
		goto out;
	}
	g_debug ("native temperature %f", priv->native_whitepoint);

	/* get the target whitepoint XYZ for the Lab check */
	if (priv->target_whitepoint > 0) {
		cmsWhitePointFromTemp (&whitepoint_tmp,
				       (gdouble) priv->target_whitepoint);
	} else {
		cmsWhitePointFromTemp (&whitepoint_tmp,
				       priv->native_whitepoint);
	}
	cmsxyY2XYZ (&priv->whitepoint, &whitepoint_tmp);

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;

	/* should we seed the first value with a good approximation */
	if (priv->target_whitepoint > 0) {
		CdColorRGB tmp;
		cd_color_get_blackbody_rgb (6500 - (priv->native_whitepoint - priv->target_whitepoint), &tmp);
		g_debug ("Seeding with %f,%f,%f",
			 tmp.R, tmp.G, tmp.B);
		cd_color_rgb_copy (&tmp, &item->color);
	}

	/* process the last item in the array (255,255,255) */
	item = g_ptr_array_index (priv->array, 1);
	state_local = cd_state_get_child (state);
	ret = cd_main_calib_process_item (priv,
					  item,
					  state_local,
					  error);
	if (!ret)
		goto out;

	/* ensure white is normalised to 1 */
	temp = 1.0f / (gdouble) MAX (MAX (item->color.R, item->color.G), item->color.B);
	item->color.R *= temp;
	item->color.G *= temp;
	item->color.B *= temp;

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;

	/* expand out the array into more points (interpolating) */
	if (priv->quality == CD_PROFILE_QUALITY_LOW) {
		precision_steps = 5;
	} else if (priv->quality == CD_PROFILE_QUALITY_MEDIUM) {
		precision_steps = 11;
	} else if (priv->quality == CD_PROFILE_QUALITY_HIGH) {
		precision_steps = 21;
	}
	ret = cd_main_calib_interpolate_up (priv, precision_steps, error);
	if (!ret)
		goto out;

	/* refine the other points */
	state_local = cd_state_get_child (state);
	cd_state_set_number_steps (state_local, priv->array->len - 1);
	for (i = priv->array->len - 2; i > 0 ; i--) {

		/* set new sample patch */
		rgb.R = 1.0 / (gdouble) (priv->array->len - 1) * (gdouble) i;
		rgb.G = 1.0 / (gdouble) (priv->array->len - 1) * (gdouble) i;
		rgb.B = 1.0 / (gdouble) (priv->array->len - 1) * (gdouble) i;
		cd_main_emit_update_sample (priv, &rgb);

		/* process this section */
		item = g_ptr_array_index (priv->array, i);
		state_loop = cd_state_get_child (state_local);
		ret = cd_main_calib_process_item (priv, item, state_loop, error);
		if (!ret)
			goto out;

		/* done */
		ret = cd_state_done (state_local, error);
		if (!ret)
			goto out;
	}

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;

	/* set this */
	cd_main_emit_update_gamma (priv, priv->array);

	/* get new whitepoint */
	ret = cd_main_calib_get_native_whitepoint (priv, &temp, error);
	if (!ret)
		goto out;
	g_debug ("new native temperature %f", temp);

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;

	/* save the results */
	priv->it8_cal = cd_it8_new_with_kind (CD_IT8_KIND_CAL);
	cd_it8_set_originator (priv->it8_cal, "colord-session");
	cd_it8_set_instrument (priv->it8_cal, cd_sensor_kind_to_string (cd_sensor_get_kind (priv->sensor)));
	ret = cd_main_calib_interpolate_up (priv, 256, error);
	if (!ret)
		goto out;
	for (i = 0; i < priv->array->len; i++) {
		item = g_ptr_array_index (priv->array, i);
		cd_it8_add_data (priv->it8_cal, &item->color, NULL);
	}

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * cd_main_finished_quit_cb:
 **/
static gboolean
cd_main_finished_quit_cb (gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	g_main_loop_quit (priv->loop);
	return FALSE;
}

/**
 * cd_main_load_samples:
 **/
static gboolean
cd_main_load_samples (CdMainPrivate *priv, GError **error)
{
	const gchar *filename;
	gboolean ret;
	gchar *path;
	GFile *file;

	filename = cd_main_get_display_ti1 (priv->quality);
	path = g_build_filename (DATADIR,
				 "colord",
				 "ti1",
				 filename,
				 NULL);
	g_debug ("opening source file %s", path);
	file = g_file_new_for_path (path);
	priv->it8_ti1 = cd_it8_new ();
	ret = cd_it8_load_from_file (priv->it8_ti1, file, error);
	if (!ret)
		goto out;
out:
	g_free (path);
	g_object_unref (file);
	return ret;
}

/**
 * cd_main_write_colprof_files:
 **/
static gboolean
cd_main_write_colprof_files (CdMainPrivate *priv, GError **error)
{
	gboolean ret = TRUE;
	gchar *filename_ti3 = NULL;
	gchar *data_cal = NULL;
	gchar *data_ti3 = NULL;
	gchar *data = NULL;
	gchar *path_ti3 = NULL;

	/* build temp path */
	priv->working_path = g_dir_make_tmp ("colord-session-XXXXXX", error);
	if (priv->working_path == NULL) {
		ret = FALSE;
		goto out;
	}

	/* save .ti3 with ti1 and cal data appended together */
	ret = cd_it8_save_to_data (priv->it8_ti3,
				   &data_ti3,
				   NULL,
				   error);
	if (!ret)
		goto out;
	ret = cd_it8_save_to_data (priv->it8_cal,
				   &data_cal,
				   NULL,
				   error);
	if (!ret)
		goto out;
	data = g_strdup_printf ("%s\n%s", data_ti3, data_cal);
	filename_ti3 = g_strdup_printf ("%s.ti3", priv->basename);
	path_ti3 = g_build_filename (priv->working_path,
				     filename_ti3,
				     NULL);
	g_debug ("saving %s", path_ti3);
	ret = g_file_set_contents (path_ti3, data, -1, error);
	if (!ret)
		goto out;
out:
	g_free (data);
	g_free (data_cal);
	g_free (data_ti3);
	g_free (filename_ti3);
	g_free (path_ti3);
	return ret;
}

/**
 * cd_main_get_colprof_quality_arg:
 **/
static const gchar *
cd_main_get_colprof_quality_arg (CdProfileQuality quality)
{
	if (quality == CD_PROFILE_QUALITY_LOW)
		return "-ql";
	if (quality == CD_PROFILE_QUALITY_MEDIUM)
		return "-qm";
	if (quality == CD_PROFILE_QUALITY_HIGH)
		return "-qh";
	return NULL;
}

#define CD_PROFILE_DEFAULT_COPYRIGHT_STRING	"This profile is free of known copyright restrictions."

/**
 * cd_main_find_argyll_tool:
 **/
static gchar *
cd_main_find_argyll_tool (const gchar *command,
			  GError **error)
{
	gboolean ret;
	gchar *filename;

	/* try the original argyllcms filename installed in /usr/local/bin */
	filename = g_strdup_printf ("/usr/local/bin/%s", command);
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (ret)
		goto out;

	/* try the debian filename installed in /usr/bin */
	g_free (filename);
	filename = g_strdup_printf ("/usr/bin/argyll-%s", command);
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (ret)
		goto out;

	/* try the original argyllcms filename installed in /usr/bin */
	g_free (filename);
	filename = g_strdup_printf ("/usr/bin/%s", command);
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (ret)
		goto out;

	/* eek */
	g_free (filename);
	filename = NULL;
	g_set_error (error,
		     CD_SESSION_ERROR,
		     CD_SESSION_ERROR_FAILED_TO_FIND_TOOL,
		     "failed to get filename for %s", command);
out:
	return filename;
}

/**
 * cd_main_import_profile:
 **/
static gboolean
cd_main_import_profile (CdMainPrivate *priv, GError **error)
{
	CdProfile *profile;
	gboolean ret = TRUE;
	gchar *filename;
	gchar *path;
	GFile *file;

	filename = g_strdup_printf ("%s.icc", priv->basename);
	path = g_build_filename (priv->working_path,
				 filename,
				 NULL);
	g_debug ("trying to import %s", path);
	file = g_file_new_for_path (path);
	profile = cd_client_import_profile_sync (priv->client,
						 file,
						 priv->cancellable,
						 error);
	if (profile == NULL) {
		ret = FALSE;
		goto out;
	}
	g_debug ("imported %s", cd_profile_get_object_path (profile));

	/* add profile to device and set default */
	ret = cd_profile_connect_sync (profile,
				       priv->cancellable,
				       error);
	if (!ret)
		goto out;
	ret = cd_device_add_profile_sync (priv->device,
					  CD_DEVICE_RELATION_HARD,
					  profile,
					  priv->cancellable,
					  error);
	if (!ret)
		goto out;
	ret = cd_device_make_profile_default_sync (priv->device,
						   profile,
						   priv->cancellable,
						   error);
	if (!ret)
		goto out;
	g_debug ("set %s default on %s",
		 cd_profile_get_id (profile),
		 cd_device_get_id (priv->device));
out:
	g_free (filename);
	g_free (path);
	g_object_unref (file);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_main_set_profile_metadata:
 **/
static gboolean
cd_main_set_profile_metadata (CdMainPrivate *priv, GError **error)
{
	cmsHANDLE dict_md = NULL;
	cmsHPROFILE lcms_profile = NULL;
	gboolean ret;
	gchar *brightness_str = NULL;
	gchar *data = NULL;
	gchar *profile_fn = NULL;
	gchar *profile_path = NULL;
	gsize len;

	/* get profile */
	profile_fn = g_strdup_printf ("%s.icc", priv->basename);
	profile_path = g_build_filename (priv->working_path,
					 profile_fn,
					 NULL);

	/* open profile */
	ret = g_file_get_contents (profile_path, &data, &len, error);
	if (!ret)
		goto out;
	lcms_profile = cmsOpenProfileFromMem (data, len);
	if (lcms_profile == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_SESSION_ERROR,
			     CD_SESSION_ERROR_FAILED_TO_OPEN_PROFILE,
			     "failed to open profile %s",
			     profile_path);
		goto out;
	}

	/* add DICT data */
	dict_md = cmsDictAlloc (NULL);
	_cmsDictAddEntryAscii (dict_md,
			       CD_PROFILE_METADATA_CMF_PRODUCT,
			       "colord");
	_cmsDictAddEntryAscii (dict_md,
			       CD_PROFILE_METADATA_CMF_BINARY,
			       "colord-session");
	_cmsDictAddEntryAscii (dict_md,
			       CD_PROFILE_METADATA_CMF_VERSION,
			       PACKAGE_VERSION);
	_cmsDictAddEntryAscii (dict_md,
			       CD_PROFILE_METADATA_DATA_SOURCE,
			       CD_PROFILE_METADATA_DATA_SOURCE_CALIB);
	_cmsDictAddEntryAscii (dict_md,
			       CD_PROFILE_METADATA_LICENSE,
			       "CC0");
	_cmsDictAddEntryAscii (dict_md,
			       CD_PROFILE_METADATA_QUALITY,
			       cd_profile_quality_to_string (priv->quality));
	_cmsDictAddEntryAscii (dict_md,
			       CD_PROFILE_METADATA_MAPPING_DEVICE_ID,
			       cd_device_get_id (priv->device));
	_cmsDictAddEntryAscii (dict_md,
			       CD_PROFILE_METADATA_MEASUREMENT_DEVICE,
			       cd_sensor_kind_to_string (cd_sensor_get_kind (priv->sensor)));
	if (priv->screen_brightness > 0) {
		brightness_str = g_strdup_printf ("%u", priv->screen_brightness);
		_cmsDictAddEntryAscii (dict_md,
				       CD_PROFILE_METADATA_SCREEN_BRIGHTNESS,
				       brightness_str);
		g_free (brightness_str);
	}
	ret = cmsWriteTag (lcms_profile, cmsSigMetaTag, dict_md);
	if (!ret) {
		g_set_error_literal (error,
				     CD_SESSION_ERROR,
				     CD_SESSION_ERROR_FAILED_TO_SAVE_PROFILE,
				     "cannot initialize dict tag");
		goto out;
	}

	/* write profile id */
	ret = cmsMD5computeID (lcms_profile);
	if (!ret) {
		g_set_error (error,
			     CD_SESSION_ERROR,
			     CD_SESSION_ERROR_INTERNAL,
			     "failed to compute profile id for %s",
			     profile_path);
		goto out;
	}

	/* save file */
	ret = cmsSaveProfileToFile (lcms_profile, profile_path);
	if (!ret) {
		g_set_error (error,
			     CD_SESSION_ERROR,
			     CD_SESSION_ERROR_FAILED_TO_SAVE_PROFILE,
			     "failed to save profile to %s",
			     profile_path);
		goto out;
	}
out:
	g_free (profile_fn);
	g_free (profile_path);
	g_free (data);
	if (dict_md != NULL)
		cmsDictFree (dict_md);
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	return ret;
}

/**
 * cd_main_generate_profile:
 **/
static gboolean
cd_main_generate_profile (CdMainPrivate *priv, GError **error)
{
	gboolean ret;
	gchar *command;
	gchar *stderr = NULL;
	gint exit_status = 0;
	gchar *cmd_debug = NULL;
	GPtrArray *array = NULL;

	/* get correct name of the command */
	command = cd_main_find_argyll_tool ("colprof", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup (command));
	g_ptr_array_add (array, g_strdup ("-v"));
//	g_ptr_array_add (array, g_strdup_printf ("-A%s", cd_device_get_vendor (priv->device)));
	g_ptr_array_add (array, g_strdup_printf ("-M%s", cd_device_get_model (priv->device)));
	g_ptr_array_add (array, g_strdup_printf ("-D%s", priv->title));
	g_ptr_array_add (array, g_strdup_printf ("-C%s", CD_PROFILE_DEFAULT_COPYRIGHT_STRING));
	g_ptr_array_add (array, g_strdup (cd_main_get_colprof_quality_arg (priv->quality)));
	g_ptr_array_add (array, g_strdup ("-aG"));
	g_ptr_array_add (array, g_strdup (priv->basename));
	g_ptr_array_add (array, NULL);

	/* run the command */
	cmd_debug = g_strjoinv (" ", (gchar **) array->pdata);
	g_debug ("running '%s'", cmd_debug);
	ret = g_spawn_sync (priv->working_path,
			    (gchar **) array->pdata,
			    NULL,
			    0,
			    NULL, NULL,
			    NULL,
			    &stderr,
			    &exit_status,
			    error);
	if (!ret)
		goto out;
	if (exit_status != 0) {
		ret = FALSE;
		g_set_error (error,
			     CD_SESSION_ERROR,
			     CD_SESSION_ERROR_FAILED_TO_GENERATE_PROFILE,
			     "colprof failed: %s", stderr);
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_free (command);
	g_free (stderr);
	g_free (cmd_debug);
	return ret;
}

/**
 * cd_main_display_get_samples:
 **/
static gboolean
cd_main_display_get_samples (CdMainPrivate *priv,
			     CdState *state,
			     GError **error)
{
	CdColorRGB rgb;
	CdColorXYZ xyz;
	gboolean ret = TRUE;
	guint i;
	guint size;

	size = cd_it8_get_data_size (priv->it8_ti1);
	cd_state_set_number_steps (state, size);
	for (i = 0; i < size; i++) {
		cd_it8_get_data_item (priv->it8_ti1,
				      i,
				      &rgb,
				      NULL);
		cd_main_emit_update_sample (priv, &rgb);
		ret = cd_main_calib_get_sample (priv, &xyz, error);
		if (!ret)
			goto out;
		cd_it8_add_data (priv->it8_ti3,
				 &rgb,
				 &xyz);

		/* done */
		ret = cd_state_done (state, error);
		if (!ret)
			goto out;
	}
out:
	return ret;
}

/**
 * cd_main_display_characterize:
 **/
static gboolean
cd_main_display_characterize (CdMainPrivate *priv,
			      CdState *state,
			      GError **error)
{
	CdState *state_local;
	gboolean ret;

	/* reset the state */
	ret = cd_state_set_steps (state,
				  error,
				  1,	/* load samples */
				  96,	/* measure samples */
				  1,	/* run colprof */
				  1,	/* set metadata */
				  1,	/* import profile */
				  -1);
	if (!ret)
		goto out;

	/* load the ti1 file */
	ret = cd_main_load_samples (priv, error);
	if (!ret)
		goto out;

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;

	/* create the ti3 file */
	priv->it8_ti3 = cd_it8_new_with_kind (CD_IT8_KIND_TI3);
	cd_it8_set_normalized (priv->it8_ti3, TRUE);
	cd_it8_set_originator (priv->it8_ti3, "colord-session");
	cd_it8_set_title (priv->it8_ti3, priv->title);
	cd_it8_set_spectral (priv->it8_ti3, FALSE);
	cd_it8_set_instrument (priv->it8_ti3, cd_sensor_get_model (priv->sensor));

	/* measure each sample */
	state_local = cd_state_get_child (state);
	ret = cd_main_display_get_samples (priv, state_local, error);
	if (!ret)
		goto out;

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;

	/* write out files */
	ret = cd_main_write_colprof_files (priv, error);
	if (!ret)
		goto out;

	/* run colprof */
	ret = cd_main_generate_profile (priv, error);
	if (!ret)
		goto out;

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;

	/* set metadata on the profile */
	ret = cd_main_set_profile_metadata (priv, error);
	if (!ret)
		goto out;

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;

	/* import profile */
	ret = cd_main_import_profile (priv, error);
	if (!ret)
		goto out;

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * cd_main_remove_temp_file:
 **/
static gboolean
cd_main_remove_temp_file (const gchar *filename,
			  GCancellable *cancellable,
			  GError **error)
{
	gboolean ret;
	GFile *file;

	g_debug ("removing %s", filename);
	file = g_file_new_for_path (filename);
	ret = g_file_delete (file, cancellable, error);
	if (!ret)
		goto out;
out:
	g_object_unref (file);
	return ret;
}

/**
 * cd_main_remove_temp_files:
 **/
static gboolean
cd_main_remove_temp_files (CdMainPrivate *priv, GError **error)
{
	const gchar *filename;
	gboolean ret;
	gchar *src;
	GDir *dir;

	/* try to open */
	dir = g_dir_open (priv->working_path, 0, error);
	if (dir == NULL) {
		ret = FALSE;
		goto out;
	}

	/* find each */
	while ((filename = g_dir_read_name (dir))) {
		src = g_build_filename (priv->working_path,
					filename,
					NULL);
		ret = cd_main_remove_temp_file (src,
						priv->cancellable,
						error);
		g_free (src);
		if (!ret)
			goto out;
	}

	/* remove directory */
	ret = cd_main_remove_temp_file (priv->working_path,
					priv->cancellable,
					error);
	if (!ret)
		goto out;
out:
	if (dir != NULL)
		g_dir_close (dir);
	return ret;
}

/**
 * cd_main_start_calibration:
 **/
static gboolean
cd_main_start_calibration (CdMainPrivate *priv,
			   CdState *state,
			   GError **error)
{
	CdState *state_local;
	gboolean ret;
	GError *error_local = NULL;

	/* reset the state */
	ret = cd_state_set_steps (state,
				  error,
				  74,	/* calibration */
				  25,	/* characterization */
				  1,	/* remove temp files */
				  -1);
	if (!ret)
		goto out;

	/* do the calibration */
	state_local = cd_state_get_child (state);
	ret = cd_main_calib_process (priv, state_local, &error_local);
	if (!ret) {
		if (g_error_matches (error_local,
				     CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_REQUIRED_POSITION_CALIBRATE)) {
			priv->status = CD_SESSION_STATUS_WAITING_FOR_INTERACTION;
			cd_main_emit_interaction_required (priv,
							   CD_SESSION_INTERACTION_MOVE_TO_CALIBRATION);
			g_error_free (error_local);
		} else if (g_error_matches (error_local,
					    CD_SENSOR_ERROR,
					    CD_SENSOR_ERROR_REQUIRED_POSITION_SURFACE)) {
			priv->status = CD_SESSION_STATUS_WAITING_FOR_INTERACTION;
			cd_main_emit_interaction_required (priv,
							   CD_SESSION_INTERACTION_MOVE_TO_SURFACE);
			g_error_free (error_local);
		} else {
			g_propagate_error (error, error_local);
			goto out;
		}
		goto out;
	}

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;

	/* do the characterization */
	state_local = cd_state_get_child (state);
	ret = cd_main_display_characterize (priv, state_local, error);
	if (!ret)
		goto out;

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;

	/* remove temp files */
	ret = cd_main_remove_temp_files (priv, error);
	if (!ret)
		goto out;

	/* done */
	ret = cd_state_done (state, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * cd_main_start_calibration_cb:
 **/
static gboolean
cd_main_start_calibration_cb (gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	gboolean ret;
	GError *error = NULL;

	/* reset the state */
	cd_state_reset (priv->state);
	ret = cd_main_start_calibration (priv,
					 priv->state,
					 &error);
	if (!ret) {
		/* use the error code if it's our error domain */
		if (error->domain == CD_SESSION_ERROR) {
			cd_main_emit_finished (priv,
					       error->code,
					       error->message);
		} else {
			cd_main_emit_finished (priv,
					       CD_SESSION_ERROR_INTERNAL,
					       error->message);
		}
		g_timeout_add (200, cd_main_finished_quit_cb, priv);
		g_error_free (error);
		goto out;
	}

	/* success */
	cd_main_emit_finished (priv, CD_SESSION_ERROR_NONE, NULL);
	g_timeout_add (200, cd_main_finished_quit_cb, priv);
out:
	return FALSE;
}

/**
 * cd_main_status_to_text:
 **/
static const gchar *
cd_main_status_to_text (CdSessionStatus status)
{
	if (status == CD_SESSION_STATUS_IDLE)
		return "idle";
	if (status == CD_SESSION_STATUS_WAITING_FOR_INTERACTION)
		return "waiting-for-interaction";
	if (status == CD_SESSION_STATUS_RUNNING)
		return "running";
	return NULL;
}

/**
 * cd_main_sender_vanished_cb:
 **/
static void
cd_main_sender_vanished_cb (GDBusConnection *connection,
			    const gchar *name,
			    gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;

	/* FIXME: make configurable? */
	g_debug ("Quitting daemon as sender has quit");
	g_cancellable_cancel (priv->cancellable);
	g_main_loop_quit (priv->loop);
}

/**
 * cd_main_find_device:
 **/
static CdDevice *
cd_main_find_device (CdMainPrivate *priv,
		     const gchar *device_id,
		     GError **error)
{
	CdDevice *device = NULL;
	CdDevice *device_tmp;
	gboolean ret;
	GError *error_local = NULL;

	device_tmp = cd_client_find_device_sync (priv->client,
						 device_id,
						 NULL,
						 &error_local);
	if (device_tmp == NULL) {
		g_set_error (error,
			     CD_SESSION_ERROR,
			     CD_SESSION_ERROR_FAILED_TO_FIND_DEVICE,
			     "%s", error_local->message);
		g_error_free (error_local);
		goto out;
	}
	ret = cd_device_connect_sync (device_tmp,
				      NULL,
				      &error_local);
	if (!ret) {
		g_set_error (error,
			     CD_SESSION_ERROR,
			     CD_SESSION_ERROR_FAILED_TO_FIND_DEVICE,
			     "%s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* mark device to be profiled in colord */
	ret = cd_device_profiling_inhibit_sync (device_tmp,
						NULL,
						&error_local);
	if (!ret) {
		g_set_error (error,
			     CD_SESSION_ERROR,
			     CD_SESSION_ERROR_INTERNAL,
			     "%s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* success */
	device = g_object_ref (device_tmp);
out:
	if (device_tmp != NULL)
		g_object_unref (device_tmp);
	return device;
}

/**
 * cd_main_find_sensor:
 **/
static CdSensor *
cd_main_find_sensor (CdMainPrivate *priv,
		     const gchar *sensor_id,
		     GError **error)
{
	CdSensor *sensor_tmp;
	CdSensor *sensor = NULL;
	gboolean ret;
	GError *error_local = NULL;

	sensor_tmp = cd_client_find_sensor_sync (priv->client,
						 sensor_id,
						 NULL,
						 &error_local);
	if (sensor_tmp == NULL) {
		g_set_error (error,
			     CD_SESSION_ERROR,
			     CD_SESSION_ERROR_FAILED_TO_FIND_SENSOR,
			     "%s", error_local->message);
		g_error_free (error_local);
		goto out;
	}
	ret = cd_sensor_connect_sync (sensor_tmp,
				      NULL,
				      &error_local);
	if (!ret) {
		g_set_error (error,
			     CD_SESSION_ERROR,
			     CD_SESSION_ERROR_FAILED_TO_FIND_SENSOR,
			     "%s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* lock the sensor */
	ret = cd_sensor_lock_sync (sensor_tmp,
				   NULL,
				   &error_local);
	if (!ret) {
		g_set_error (error,
			     CD_SESSION_ERROR,
			     CD_SESSION_ERROR_FAILED_TO_FIND_SENSOR,
			     "%s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* success */
	sensor = g_object_ref (sensor_tmp);
out:
	if (sensor_tmp != NULL)
		g_object_unref (sensor_tmp);
	return sensor;
}

/**
 * cd_main_set_basename:
 **/
static void
cd_main_set_basename (CdMainPrivate *priv)
{
	const gchar *tmp;
	gchar *date_str = NULL;
	GDateTime *datetime;
	GString *str;

	str = g_string_new ("");

	/* add vendor */
	tmp = cd_device_get_vendor (priv->device);
	if (tmp != NULL)
		g_string_append_printf (str, "%s ", tmp);

	/* add model */
	tmp = cd_device_get_model (priv->device);
	if (tmp != NULL)
		g_string_append_printf (str, "%s ", tmp);

	/* fall back to _something_ */
	if (str->len == 0)
		g_string_append (str, "Profile ");

	/* add the quality */
	g_string_append_printf (str, "(%s) ",
				cd_profile_quality_to_string (priv->quality));

	/* add date and time */
	datetime = g_date_time_new_now_utc ();
	date_str = g_date_time_format (datetime, "%F %H-%M-%S");
	g_string_append_printf (str, "%s ", date_str);
	g_free (date_str);
	g_date_time_unref (datetime);

	/* add the sensor */
	tmp = cd_sensor_kind_to_string (cd_sensor_get_kind (priv->sensor));
	if (tmp != NULL)
		g_string_append_printf (str, "%s ", tmp);

	/* remove trailing space */
	g_string_set_size (str, str->len - 1);

	/* make suitable filename */
	g_strdelimit (str->str, "\"*?", '_');
	priv->basename = g_string_free (str, FALSE);
}

/**
 * cd_main_quit_loop_cb:
 **/
static gboolean
cd_main_quit_loop_cb (gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	g_main_loop_quit (priv->loop);
	return FALSE;
}

/**
 * cd_main_daemon_method_call:
 **/
static void
cd_main_daemon_method_call (GDBusConnection *connection,
			    const gchar *sender,
			    const gchar *object_path,
			    const gchar *interface_name,
			    const gchar *method_name,
			    GVariant *parameters,
			    GDBusMethodInvocation *invocation,
			    gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	const gchar *device_id;
	const gchar *prop_key;
	const gchar *sensor_id;
	GError *error = NULL;
	GVariantIter *iter = NULL;
	GVariant *prop_value;

	/* should be impossible */
	if (g_strcmp0 (interface_name, "org.freedesktop.ColorHelper.Display") != 0) {
		g_dbus_method_invocation_return_error (invocation,
						       CD_SESSION_ERROR,
						       CD_SESSION_ERROR_INTERNAL,
						       "cannot execute method %s on %s",
						       method_name, interface_name);
		goto out;
	}

	if (g_strcmp0 (method_name, "Start") == 0) {
		g_variant_get (parameters, "(&s&sa{sv})",
			       &device_id,
			       &sensor_id,
			       &iter);
		g_debug ("CdMain: %s:Start(%s,%s)",
			 sender,
			 device_id,
			 sensor_id);

		/* set the default parameters */
		priv->quality = CD_PROFILE_QUALITY_MEDIUM;
		priv->device_kind = CD_SENSOR_CAP_LCD;
		while (g_variant_iter_next (iter, "{&sv}",
					    &prop_key, &prop_value)) {
			if (g_strcmp0 (prop_key, "Quality") == 0) {
				priv->quality = g_variant_get_uint32 (prop_value);
				g_debug ("Quality: %s",
					 cd_profile_quality_to_string (priv->quality));
			} else if (g_strcmp0 (prop_key, "Whitepoint") == 0) {
				priv->target_whitepoint = g_variant_get_uint32 (prop_value);
				g_debug ("Whitepoint: %iK",
					 priv->target_whitepoint);
			} else if (g_strcmp0 (prop_key, "Title") == 0) {
				priv->title = g_variant_dup_string (prop_value, NULL);
				g_debug ("Title: %s", priv->title);
			} else if (g_strcmp0 (prop_key, "DeviceKind") == 0) {
				priv->device_kind = g_variant_get_uint32 (prop_value);
				g_debug ("Device kind: %s",
					 cd_sensor_cap_to_string (priv->device_kind));
			} else if (g_strcmp0 (prop_key, "Brightness") == 0) {
				priv->screen_brightness = g_variant_get_uint32 (prop_value);
				g_debug ("Device brightness: %i", priv->screen_brightness);
			} else {
				/* not a fatal warning */
				g_warning ("option %s unsupported", prop_key);
			}
		}

		/* set a decent default */
		if (priv->title == NULL)
			priv->title = g_strdup ("Profile");

		if (priv->status != CD_SESSION_STATUS_IDLE) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SESSION_ERROR,
							       CD_SESSION_ERROR_INTERNAL,
							       "cannot start as status is %s",
							       cd_main_status_to_text (priv->status));
			goto out;
		}

		/* check the quality argument */
		if (priv->quality > 2) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SESSION_ERROR,
							       CD_SESSION_ERROR_INVALID_VALUE,
							       "invalid quality value %i",
							       priv->quality);
			goto out;
		}

		if (priv->target_whitepoint != 0 &&
		    (priv->target_whitepoint < 1000 ||
		     priv->target_whitepoint > 100000)) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SESSION_ERROR,
							       CD_SESSION_ERROR_INVALID_VALUE,
							       "invalid target whitepoint value %i",
							       priv->target_whitepoint);
			goto out;
		}

		/* watch to see when the sender quits */
		priv->watcher_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
						     sender,
						     G_BUS_NAME_WATCHER_FLAGS_NONE,
						     NULL,
						     cd_main_sender_vanished_cb,
						     priv, NULL);
		priv->status = CD_SESSION_STATUS_IDLE;

		/* start calibration */
		priv->device = cd_main_find_device (priv,
						    device_id,
						    &error);
		if (priv->device == NULL) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			g_error_free (error);
			g_timeout_add (200, cd_main_finished_quit_cb, priv);
			goto out;
		}
		priv->sensor = cd_main_find_sensor (priv,
						    sensor_id,
						    &error);
		if (priv->sensor == NULL) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			g_error_free (error);
			g_timeout_add (200, cd_main_finished_quit_cb, priv);
			goto out;
		}

		/* set the filename of all the calibrated files */
		cd_main_set_basename (priv);

		/* ask the user to attach the device to the screen if
		 * the sensor is external, otherwise to shut the lid */
		if (cd_sensor_get_embedded (priv->sensor)) {
			cd_main_emit_interaction_required (priv,
							   CD_SESSION_INTERACTION_SHUT_LAPTOP_LID);
		} else {
			cd_main_emit_interaction_required (priv,
							   CD_SESSION_INTERACTION_ATTACH_TO_SCREEN);
		}
		priv->status = CD_SESSION_STATUS_WAITING_FOR_INTERACTION;
		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	if (g_strcmp0 (method_name, "Cancel") == 0) {
		g_debug ("CdMain: %s:Cancel()", sender);
		if (priv->status != CD_SESSION_STATUS_RUNNING &&
		    priv->status != CD_SESSION_STATUS_WAITING_FOR_INTERACTION) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SESSION_ERROR,
							       CD_SESSION_ERROR_INTERNAL,
							       "cannot cancel as status is %s",
							       cd_main_status_to_text (priv->status));
			goto out;
		}
		g_cancellable_cancel (priv->cancellable);
		priv->status = CD_SESSION_STATUS_IDLE;
		g_timeout_add (1000, cd_main_quit_loop_cb, priv);
		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	if (g_strcmp0 (method_name, "Resume") == 0) {
		g_debug ("CdMain: %s:Resume()", sender);
		if (priv->status != CD_SESSION_STATUS_WAITING_FOR_INTERACTION) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_SESSION_ERROR,
							       CD_SESSION_ERROR_INTERNAL,
							       "cannot resume as status is %s",
							       cd_main_status_to_text (priv->status));
			goto out;
		}

		/* actually start the process now */
		g_idle_add (cd_main_start_calibration_cb, priv);
		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	/* we suck */
	g_critical ("failed to process method %s", method_name);
out:
	return;
}

/**
 * cd_main_daemon_get_property:
 **/
static GVariant *
cd_main_daemon_get_property (GDBusConnection *connection_, const gchar *sender,
			     const gchar *object_path, const gchar *interface_name,
			     const gchar *property_name, GError **error,
			     gpointer user_data)
{
	GVariant *retval = NULL;
	CdMainPrivate *priv = (CdMainPrivate *) user_data;

	/* main interface */
	if (g_strcmp0 (interface_name,
		       CD_SESSION_DBUS_INTERFACE) == 0) {
		if (g_strcmp0 (property_name, "DaemonVersion") == 0) {
			retval = g_variant_new_string (VERSION);
		} else {
			g_critical ("failed to get %s property %s",
				    interface_name, property_name);
		}
		goto out;
	}

	/* display interface */
	if (g_strcmp0 (interface_name, CD_SESSION_DBUS_INTERFACE_DISPLAY) == 0) {
		if (g_strcmp0 (property_name, "Progress") == 0) {
			retval = g_variant_new_uint32 (priv->progress);
		} else {
			g_critical ("failed to get %s property %s",
				    interface_name, property_name);
		}
		goto out;
	}
out:
	return retval;
}

/**
 * cd_main_on_bus_acquired_cb:
 **/
static void
cd_main_on_bus_acquired_cb (GDBusConnection *connection,
			    const gchar *name,
			    gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	guint registration_id;
	guint i;
	static const GDBusInterfaceVTable interface_vtable = {
		cd_main_daemon_method_call,
		cd_main_daemon_get_property,
		NULL
	};

	priv->connection = g_object_ref (connection);
	for (i = 0; i < 2; i++) {
		registration_id = g_dbus_connection_register_object (connection,
								     CD_SESSION_DBUS_PATH,
								     priv->introspection->interfaces[i],
								     &interface_vtable,
								     priv,  /* user_data */
								     NULL,  /* user_data_free_func */
								     NULL); /* GError** */
		g_assert (registration_id > 0);
	}
}

/**
 * cd_main_on_name_acquired_cb:
 **/
static void
cd_main_on_name_acquired_cb (GDBusConnection *connection,
			     const gchar *name,
			     gpointer user_data)
{
	g_debug ("CdMain: acquired name: %s", name);
}

/**
 * cd_main_on_name_lost_cb:
 **/
static void
cd_main_on_name_lost_cb (GDBusConnection *connection,
			 const gchar *name,
			 gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	g_debug ("CdMain: lost name: %s", name);
	g_main_loop_quit (priv->loop);
}

/**
 * cd_main_timed_exit_cb:
 **/
static gboolean
cd_main_timed_exit_cb (gpointer user_data)
{
	CdMainPrivate *priv = (CdMainPrivate *) user_data;
	g_main_loop_quit (priv->loop);
	return FALSE;
}

/**
 * cd_main_load_introspection:
 **/
static GDBusNodeInfo *
cd_main_load_introspection (const gchar *filename, GError **error)
{
	gboolean ret;
	gchar *data = NULL;
	GDBusNodeInfo *info = NULL;
	GFile *file;

	/* load file */
	file = g_file_new_for_path (filename);
	ret = g_file_load_contents (file, NULL, &data,
				    NULL, NULL, error);
	if (!ret)
		goto out;

	/* build introspection from XML */
	info = g_dbus_node_info_new_for_xml (data, error);
	if (info == NULL)
		goto out;
out:
	g_object_unref (file);
	g_free (data);
	return info;
}

/**
 * cd_main_emit_property_changed:
 **/
static void
cd_main_emit_property_changed (CdMainPrivate *priv,
			       const gchar *property_name,
			       GVariant *property_value)
{
	GVariantBuilder builder;
	GVariantBuilder invalidated_builder;

	/* build the dict */
	g_variant_builder_init (&invalidated_builder, G_VARIANT_TYPE ("as"));
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add (&builder,
			       "{sv}",
			       property_name,
			       property_value);
	g_dbus_connection_emit_signal (priv->connection,
				       NULL,
				       CD_SESSION_DBUS_PATH,
				       "org.freedesktop.DBus.Properties",
				       "PropertiesChanged",
				       g_variant_new ("(sa{sv}as)",
				       CD_SESSION_DBUS_INTERFACE_DISPLAY,
				       &builder,
				       &invalidated_builder),
				       NULL);
}

/**
 * cd_main_percentage_changed_cb:
 **/
static void
cd_main_percentage_changed_cb (CdState *state,
			       guint value,
			       CdMainPrivate *priv)
{
	g_debug ("CdMain: Emitting PropertiesChanged(Progress) %i", value);
	cd_main_emit_property_changed (priv,
				       "Progress",
				       g_variant_new_uint32 (value));
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	CdMainPrivate *priv;
	gboolean ret;
	gboolean timed_exit = FALSE;
	GError *error = NULL;
	GOptionContext *context;
	guint owner_id = 0;
	guint retval = 1;
	const GOptionEntry options[] = {
		{ "timed-exit", '\0', 0, G_OPTION_ARG_NONE, &timed_exit,
		  "Exit after a small delay", NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	g_type_init ();
	priv = g_new0 (CdMainPrivate, 1);
	priv->status = CD_SESSION_STATUS_IDLE;
	priv->interaction_code_last = CD_SESSION_INTERACTION_NONE;
	priv->cancellable = g_cancellable_new ();
	priv->loop = g_main_loop_new (NULL, FALSE);

	/* track progress of the calibration */
	priv->state = cd_state_new ();
	cd_state_set_enable_profile (priv->state, TRUE);
	g_signal_connect (priv->state,
			  "percentage-changed",
			  G_CALLBACK (cd_main_percentage_changed_cb),
			  priv);

	/* TRANSLATORS: program name */
	g_set_application_name ("Color Management");
	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, cd_debug_get_option_group ());
	g_option_context_set_summary (context, "Color Management D-Bus Service");
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* load introspection from file */
	priv->introspection = cd_main_load_introspection (DATADIR "/dbus-1/interfaces/"
							  CD_SESSION_DBUS_INTERFACE ".xml",
							  &error);
	if (priv->introspection == NULL) {
		g_warning ("CdMain: failed to load introspection: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* get client */
	priv->client = cd_client_new ();
	ret = cd_client_connect_sync (priv->client, NULL, &error);
	if (!ret) {
		g_warning ("failed to contact colord: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* own the object */
	owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
				   CD_SESSION_DBUS_SERVICE,
				   G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
				    G_BUS_NAME_OWNER_FLAGS_REPLACE,
				   cd_main_on_bus_acquired_cb,
				   cd_main_on_name_acquired_cb,
				   cd_main_on_name_lost_cb,
				   priv, NULL);

	/* Only timeout and close the mainloop if we have specified it
	 * on the command line */
	if (timed_exit)
		g_timeout_add_seconds (5, cd_main_timed_exit_cb, priv);

	/* wait */
	g_main_loop_run (priv->loop);

	/* success */
	retval = 0;
out:
	if (owner_id > 0)
		g_bus_unown_name (owner_id);
	g_main_loop_unref (priv->loop);
	if (priv->client != NULL)
		g_object_unref (priv->client);
	if (priv->connection != NULL)
		g_object_unref (priv->connection);
	if (priv->introspection != NULL)
		g_dbus_node_info_unref (priv->introspection);
	if (priv->array != NULL)
		g_ptr_array_unref (priv->array);
	if (priv->sensor != NULL)
		g_object_unref (priv->sensor);
	if (priv->device != NULL)
		g_object_unref (priv->device);
	if (priv->cancellable != NULL)
		g_object_unref (priv->cancellable);
	if (priv->it8_cal != NULL)
		g_object_unref (priv->it8_cal);
	if (priv->it8_ti1 != NULL)
		g_object_unref (priv->it8_ti1);
	if (priv->it8_ti3 != NULL)
		g_object_unref (priv->it8_ti3);
	if (priv->state != NULL)
		g_object_unref (priv->state);
	g_free (priv->working_path);
	g_free (priv->basename);
	g_free (priv->title);
	g_free (priv);
	return retval;
}
