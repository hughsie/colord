/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <math.h>
#include <glib.h>

typedef struct {
	guint16	R;
	guint16	G;
	guint16	B;
} CdSensorHueyMultiplier;

#if 0
{
	guchar threshold_buffer[16];
	guchar result_buffer[16];
	CdSensorHueyMultiplier mult;
	threshold_buffer[0] = 0x02;
	threshold_buffer[1] = 0x8b;
	threshold_buffer[2] = 0x03;
	threshold_buffer[3] = 0x64;
	threshold_buffer[4] = 0x01;
	threshold_buffer[5] = 0x9b;

	mult.R = cd_buffer_read_uint16_be (threshold_buffer+0);
	mult.G = cd_buffer_read_uint16_be (threshold_buffer+2);
	mult.B = cd_buffer_read_uint16_be (threshold_buffer+4);

	g_debug ("multiplier = %i, %i, %i", mult.R, mult.G, mult.B);

	result_buffer[0] = 0x10;
	result_buffer[1] = 0x42;
	result_buffer[2] = 0x0f;
	result_buffer[3] = 0x5b;
	result_buffer[4] = 0x0f;
	result_buffer[5] = 0x49;

	device_rgb[0].R = (gdouble) mult.R / (gdouble)cd_buffer_read_uint16_be (result_buffer+0);
	device_rgb[0].G = (gdouble) mult.G / (gdouble)cd_buffer_read_uint16_be (result_buffer+2);
	device_rgb[0].B = (gdouble) mult.B / (gdouble)cd_buffer_read_uint16_be (result_buffer+4);

	/* get colors as vectors */
	color_native_vec3 = cd_color_get_RGB_Vec3 (&device_rgb[0]);
	color_result_vec3 = cd_color_get_XYZ_Vec3 (&cd_xyz);

	/* the matrix of data is designed to convert from 'device RGB' to XYZ */
	cd_mat33_vector_multiply (&calibration, color_native_vec3, color_result_vec3);

	/* scale correct */
	cd_vec3_scalar_multiply (color_result_vec3, HUEY_XYZ_POST_MULTIPLY_SCALE_FACTOR, color_result_vec3);
}
#endif

static gdouble
get_error (const CdColorXYZ *actual, const CdColorXYZ *measured)
{
	return fabs ((actual->X - measured->X) / measured->X) +
	       fabs ((actual->Y - measured->Y) / measured->Y) +
	       fabs ((actual->Z - measured->Z) / measured->Z);
}

/**
 * cd_sensor_huey_convert_device_RGB_to_XYZ:
 *
 * / X \   (( / R \             )   / d \    / c a l \ )
 * | Y | = (( | G | x pre-scale ) - | r |  * | m a t | ) x post_scale
 * \ Z /   (( \ B /             )   \ k /    \ l c d / )
 *
 * The device RGB values have to be scaled to something in the same
 * scale as the dark calibration. The results then have to be scaled
 * after convolving. I assume the first is a standard value, and the
 * second scale must be available in the eeprom somewhere.
 **/
static void
cd_sensor_huey_convert_device_RGB_to_XYZ (CdColorRGB *src, CdColorXYZ *dest,
					   CdMat3x3 *calibration, CdVec3 *dark_offset,
					   gdouble pre_scale, gdouble post_scale)
{
	CdVec3 *color_native_vec3;
	CdVec3 *color_result_vec3;
	CdVec3 temp;

	/* pre-multiply */
	color_native_vec3 = cd_color_get_RGB_Vec3 (src);
	cd_vec3_scalar_multiply (color_native_vec3, pre_scale, &temp);

	/* remove dark calibration */
	cd_vec3_subtract (&temp, dark_offset, &temp);

	/* convolve */
	color_result_vec3 = cd_color_get_XYZ_Vec3 (dest);
	cd_mat33_vector_multiply (calibration, &temp, color_result_vec3);

	/* post-multiply */
	cd_vec3_scalar_multiply (color_result_vec3, post_scale, color_result_vec3);
}

gint
main (gint argc, gchar *argv[])
{
	CdColorXYZ actual_xyz[5];
	CdColorRGB device_rgb[5];
	CdColorXYZ cd_xyz;
	CdMat3x3 calibration;
	CdVec3 dark_offset;
	gdouble	 *data;
	gdouble error;
	gdouble pre_scalar;
	gdouble post_scalar;
	gdouble min_error;
	gdouble best_post_scalar;
	gdouble best_pre_scalar;
	guint i;

	/* get the device RGB measured values */
	cd_color_set_RGB (&device_rgb[0], 0.082935, 0.053567, 0.001294);
	cd_color_set_RGB (&device_rgb[1], 0.066773, 0.150323, 0.009683);
	cd_color_set_RGB (&device_rgb[2], 0.013250, 0.021211, 0.095019);
	cd_color_set_RGB (&device_rgb[3], 0.156415, 0.220809, 0.105035);
	cd_color_set_RGB (&device_rgb[4], 0.000310, 0.000513, 0.000507);

	/* get some results from argyll */
	cd_color_set_XYZ (&actual_xyz[0], 82.537676, 42.634870, 2.142396);
	cd_color_set_XYZ (&actual_xyz[1], 61.758330, 122.072291, 17.345163);
	cd_color_set_XYZ (&actual_xyz[2], 36.544046, 19.224371, 161.438049);
	cd_color_set_XYZ (&actual_xyz[3], 174.129280, 180.500098, 179.302163);
	cd_color_set_XYZ (&actual_xyz[4], 0.407554, 0.419799, 0.849899);

	/* get the calibration vector */
	cd_vec3_init (&dark_offset, 0.014000, 0.014000, 0.016226);

	/* get the calibration matrix */
	data = cd_mat33_get_data (&calibration);
	data[0] = 0.154293;
	data[1] = -0.009611;
	data[2] = 0.038087;
	data[3] = -0.002070;
	data[4] = 0.122019;
	data[5] = 0.003279;
	data[6] = -0.000930;
	data[7] = 0.001326;
	data[8] = 0.253616;

	best_post_scalar = 0.0f;
	best_pre_scalar = 0.0f;
	min_error = 999999.0f;
	for (pre_scalar = 1900.0f; pre_scalar < 2100.0f; pre_scalar+=1.0f) {
		for (post_scalar = 0.25f; post_scalar < 5.0f; post_scalar += 0.000125f) {
			error = 0.0f;
			for (i=0; i<5; i++) {
				cd_sensor_huey_convert_device_RGB_to_XYZ (&device_rgb[i],
									   &cd_xyz,
									   &calibration,
									   &dark_offset,
									   pre_scalar,
									   post_scalar);
				g_debug ("gcolor-XYZ = %f,\t%f,\t%f",
					 cd_xyz.X, cd_xyz.Y, cd_xyz.Z);
				g_debug ("argyll-XYZ = %f,\t%f,\t%f",
					 actual_xyz[i].X, actual_xyz[i].Y, actual_xyz[i].Z);
				error += get_error (&actual_xyz[i], &cd_xyz);
			}
			if (error < min_error) {
				min_error = error;
				best_post_scalar = post_scalar;
				best_pre_scalar = pre_scalar;
			}
		}
	}
	g_debug ("best error=%lf%% @ pre %lf, post %lf", min_error * 100.0f, best_pre_scalar, best_post_scalar);

	return 0;
}
