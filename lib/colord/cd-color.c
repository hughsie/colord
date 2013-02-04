/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
 * SECTION:cd-color
 * @short_description: Color object data functionality
 *
 * Functions to manipulate color values.
 */

#include "config.h"

#include <math.h>
#include <glib-object.h>

#include "cd-color.h"

/**
 * cd_color_xyz_dup:
 *
 * Since: 0.1.27
 **/
CdColorXYZ *
cd_color_xyz_dup (const CdColorXYZ *src)
{
	CdColorXYZ *dest;
	g_return_val_if_fail (src != NULL, NULL);
	dest = cd_color_xyz_new ();
	dest->X = src->X;
	dest->Y = src->Y;
	dest->Z = src->Z;
	return dest;
}

/**
 * cd_color_rgb_dup:
 *
 * Since: 0.1.27
 **/
CdColorRGB *
cd_color_rgb_dup (const CdColorRGB *src)
{
	CdColorRGB *dest;
	g_return_val_if_fail (src != NULL, NULL);
	dest = cd_color_rgb_new ();
	dest->R = src->R;
	dest->G = src->G;
	dest->B = src->B;
	return dest;
}

/**
 * cd_color_yxy_dup:
 *
 * Since: 0.1.27
 **/
CdColorYxy *
cd_color_yxy_dup (const CdColorYxy *src)
{
	CdColorYxy *dest;
	g_return_val_if_fail (src != NULL, NULL);
	dest = cd_color_yxy_new ();
	dest->x = src->x;
	dest->y = src->y;
	return dest;
}

/**
 * cd_color_xyz_get_type:
 *
 * Gets a specific type.
 *
 * Return value: a #GType
 *
 * Since: 0.1.6
 **/
GType
cd_color_xyz_get_type (void)
{
	static GType type_id = 0;
	if (!type_id)
		type_id = g_boxed_type_register_static ("CdColorXYZ",
							(GBoxedCopyFunc) cd_color_xyz_dup,
							(GBoxedFreeFunc) cd_color_xyz_free);
	return type_id;
}

/**
 * cd_color_rgb_get_type:
 *
 * Gets a specific type.
 *
 * Return value: a #GType
 *
 * Since: 0.1.6
 **/
GType
cd_color_rgb_get_type (void)
{
	static GType type_id = 0;
	if (!type_id)
		type_id = g_boxed_type_register_static ("CdColorRGB",
							(GBoxedCopyFunc) cd_color_rgb_dup,
							(GBoxedFreeFunc) cd_color_rgb_free);
	return type_id;
}

/**
 * cd_color_yxy_get_type:
 *
 * Gets a specific type.
 *
 * Return value: a #GType
 *
 * Since: 0.1.6
 **/
GType
cd_color_yxy_get_type (void)
{
	static GType type_id = 0;
	if (!type_id)
		type_id = g_boxed_type_register_static ("CdColorYxy",
							(GBoxedCopyFunc) cd_color_yxy_dup,
							(GBoxedFreeFunc) cd_color_yxy_free);
	return type_id;
}

/**
 * cd_color_xyz_set:
 * @dest: the destination color
 * @X: component value
 * @Y: component value
 * @Z: component value
 *
 * Initialises a color value.
 *
 * Since: 0.1.27
 **/
void
cd_color_xyz_set (CdColorXYZ *dest, gdouble X, gdouble Y, gdouble Z)
{
	g_return_if_fail (dest != NULL);

	dest->X = X;
	dest->Y = Y;
	dest->Z = Z;
}

/**
 * cd_color_xyz_clear:
 * @dest: the destination color
 *
 * Initialises a color value.
 *
 * Since: 0.1.27
 **/
void
cd_color_xyz_clear (CdColorXYZ *dest)
{
	g_return_if_fail (dest != NULL);

	dest->X = 0.0f;
	dest->Y = 0.0f;
	dest->Z = 0.0f;
}

/**
 * cd_color_rgb_set:
 * @dest: the destination color
 * @R: component value
 * @G: component value
 * @B: component value
 *
 * Initialises a color value.
 *
 * Since: 0.1.27
 **/
void
cd_color_rgb_set (CdColorRGB *dest, gdouble R, gdouble G, gdouble B)
{
	g_return_if_fail (dest != NULL);

	dest->R = R;
	dest->G = G;
	dest->B = B;
}

/**
 * cd_color_yxy_set:
 * @dest: the destination color
 * @Y: component value
 * @x: component value
 * @y: component value
 *
 * Initialises a color value.
 *
 * Since: 0.1.27
 **/
void
cd_color_yxy_set (CdColorYxy *dest, gdouble Y, gdouble x, gdouble y)
{
	g_return_if_fail (dest != NULL);

	dest->Y = Y;
	dest->x = x;
	dest->y = y;
}

/**
 * cd_color_xyz_copy:
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 *
 * Since: 0.1.27
 **/
void
cd_color_xyz_copy (const CdColorXYZ *src, CdColorXYZ *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	dest->X = src->X;
	dest->Y = src->Y;
	dest->Z = src->Z;
}

/**
 * cd_color_yxy_copy:
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 *
 * Since: 0.1.27
 **/
void
cd_color_yxy_copy (const CdColorYxy *src, CdColorYxy *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	dest->Y = src->Y;
	dest->x = src->x;
	dest->y = src->y;
}

/**
 * cd_color_rgb_copy:
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 *
 * Since: 0.1.27
 **/
void
cd_color_rgb_copy (const CdColorRGB *src, CdColorRGB *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	dest->R = src->R;
	dest->G = src->G;
	dest->B = src->B;
}

/**
 * cd_color_rgb8_to_rgb:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 *
 * Since: 0.1.27
 **/
void
cd_color_rgb8_to_rgb (const CdColorRGB8 *src, CdColorRGB *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	dest->R = (gdouble) src->R / 255.0f;
	dest->G = (gdouble) src->G / 255.0f;
	dest->B = (gdouble) src->B / 255.0f;
}

/**
 * cd_color_value_double_to_uint8:
 **/
static guint8
cd_color_value_double_to_uint8 (gdouble value)
{
	if (value < 0)
		return 0;
	if (value > 1.0f)
		return 255;
	return value * 255.0f;
}

/**
 * cd_color_rgb_to_rgb8:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 *
 * Since: 0.1.27
 **/
void
cd_color_rgb_to_rgb8 (const CdColorRGB *src, CdColorRGB8 *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	/* also deal with overflow and underflow */
	dest->R = cd_color_value_double_to_uint8 (src->R);
	dest->G = cd_color_value_double_to_uint8 (src->G);
	dest->B = cd_color_value_double_to_uint8 (src->B);
}

/**
 * cd_color_yxy_to_xyz:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 *
 * Since: 0.1.27
 **/
void
cd_color_yxy_to_xyz (const CdColorYxy *src, CdColorXYZ *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	g_assert (src->Y >= 0.0f);
	g_assert (src->x >= 0.0f);
	g_assert (src->y >= 0.0f);
	g_assert (src->Y <= 100.0f);
	g_assert (src->x <= 1.0f);
	g_assert (src->y <= 1.0f);

	/* very small luminance */
	if (src->Y < 1e-6) {
		dest->X = 0.0f;
		dest->Y = 0.0f;
		dest->Z = 0.0f;
		return;
	}

	dest->X = (src->x * src->Y) / src->y;
	dest->Y = src->Y;
	dest->Z = (1.0f - src->x - src->y) * src->Y / src->y;
}

/**
 * cd_color_xyz_to_yxy:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 *
 * Since: 0.1.27
 **/
void
cd_color_xyz_to_yxy (const CdColorXYZ *src, CdColorYxy *dest)
{
	gdouble sum;

	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	/* prevent division by zero */
	sum = src->X + src->Y + src->Z;
	if (fabs (sum) < 1e-6) {
		dest->Y = 0.0f;
		dest->x = 0.0f;
		dest->y = 0.0f;
		return;
	}

	dest->Y = src->Y;
	dest->x = src->X / sum;
	dest->y = src->Y / sum;
}

/* source: http://www.vendian.org/mncharity/dir3/blackbody/
 * rescaled to make exactly 6500K equal to full intensity in all
 * channels */
static const CdColorRGB blackbody_data[] = {
	{ 1.0000, 0.0425, 0.0000 }, /* 1000K */
	{ 1.0000, 0.0668, 0.0000 }, /* 1100K */
	{ 1.0000, 0.0911, 0.0000 }, /* 1200K */
	{ 1.0000, 0.1149, 0.0000 }, /* ... */
	{ 1.0000, 0.1380, 0.0000 },
	{ 1.0000, 0.1604, 0.0000 },
	{ 1.0000, 0.1819, 0.0000 },
	{ 1.0000, 0.2024, 0.0000 },
	{ 1.0000, 0.2220, 0.0000 },
	{ 1.0000, 0.2406, 0.0000 },
	{ 1.0000, 0.2630, 0.0062 },
	{ 1.0000, 0.2868, 0.0155 },
	{ 1.0000, 0.3102, 0.0261 },
	{ 1.0000, 0.3334, 0.0379 },
	{ 1.0000, 0.3562, 0.0508 },
	{ 1.0000, 0.3787, 0.0650 },
	{ 1.0000, 0.4008, 0.0802 },
	{ 1.0000, 0.4227, 0.0964 },
	{ 1.0000, 0.4442, 0.1136 },
	{ 1.0000, 0.4652, 0.1316 },
	{ 1.0000, 0.4859, 0.1505 },
	{ 1.0000, 0.5062, 0.1702 },
	{ 1.0000, 0.5262, 0.1907 },
	{ 1.0000, 0.5458, 0.2118 },
	{ 1.0000, 0.5650, 0.2335 },
	{ 1.0000, 0.5839, 0.2558 },
	{ 1.0000, 0.6023, 0.2786 },
	{ 1.0000, 0.6204, 0.3018 },
	{ 1.0000, 0.6382, 0.3255 },
	{ 1.0000, 0.6557, 0.3495 },
	{ 1.0000, 0.6727, 0.3739 },
	{ 1.0000, 0.6894, 0.3986 },
	{ 1.0000, 0.7058, 0.4234 },
	{ 1.0000, 0.7218, 0.4485 },
	{ 1.0000, 0.7375, 0.4738 },
	{ 1.0000, 0.7529, 0.4992 },
	{ 1.0000, 0.7679, 0.5247 },
	{ 1.0000, 0.7826, 0.5503 },
	{ 1.0000, 0.7970, 0.5760 },
	{ 1.0000, 0.8111, 0.6016 },
	{ 1.0000, 0.8250, 0.6272 },
	{ 1.0000, 0.8384, 0.6529 },
	{ 1.0000, 0.8517, 0.6785 },
	{ 1.0000, 0.8647, 0.7040 },
	{ 1.0000, 0.8773, 0.7294 },
	{ 1.0000, 0.8897, 0.7548 },
	{ 1.0000, 0.9019, 0.7801 },
	{ 1.0000, 0.9137, 0.8051 },
	{ 1.0000, 0.9254, 0.8301 },
	{ 1.0000, 0.9367, 0.8550 },
	{ 1.0000, 0.9478, 0.8795 },
	{ 1.0000, 0.9587, 0.9040 },
	{ 1.0000, 0.9694, 0.9283 },
	{ 1.0000, 0.9798, 0.9524 },
	{ 1.0000, 0.9900, 0.9763 },
	{ 1.0000, 1.0000, 1.0000 }, /* 6500K */
	{ 0.9771, 0.9867, 1.0000 },
	{ 0.9554, 0.9740, 1.0000 },
	{ 0.9349, 0.9618, 1.0000 },
	{ 0.9154, 0.9500, 1.0000 },
	{ 0.8968, 0.9389, 1.0000 },
	{ 0.8792, 0.9282, 1.0000 },
	{ 0.8624, 0.9179, 1.0000 },
	{ 0.8465, 0.9080, 1.0000 },
	{ 0.8313, 0.8986, 1.0000 },
	{ 0.8167, 0.8895, 1.0000 },
	{ 0.8029, 0.8808, 1.0000 },
	{ 0.7896, 0.8724, 1.0000 },
	{ 0.7769, 0.8643, 1.0000 },
	{ 0.7648, 0.8565, 1.0000 },
	{ 0.7532, 0.8490, 1.0000 },
	{ 0.7420, 0.8418, 1.0000 },
	{ 0.7314, 0.8348, 1.0000 },
	{ 0.7212, 0.8281, 1.0000 },
	{ 0.7113, 0.8216, 1.0000 },
	{ 0.7018, 0.8153, 1.0000 },
	{ 0.6927, 0.8092, 1.0000 },
	{ 0.6839, 0.8032, 1.0000 },
	{ 0.6755, 0.7975, 1.0000 },
	{ 0.6674, 0.7921, 1.0000 },
	{ 0.6595, 0.7867, 1.0000 },
	{ 0.6520, 0.7816, 1.0000 },
	{ 0.6447, 0.7765, 1.0000 },
	{ 0.6376, 0.7717, 1.0000 },
	{ 0.6308, 0.7670, 1.0000 },
	{ 0.6242, 0.7623, 1.0000 },
	{ 0.6179, 0.7579, 1.0000 },
	{ 0.6117, 0.7536, 1.0000 },
	{ 0.6058, 0.7493, 1.0000 },
	{ 0.6000, 0.7453, 1.0000 },
	{ 0.5944, 0.7414, 1.0000 } /* 10000K */
};

/**
 * cd_color_rgb_interpolate:
 *
 * Since: 0.1.26
 **/
void
cd_color_rgb_interpolate (const CdColorRGB *p1,
			  const CdColorRGB *p2,
			  gdouble index,
			  CdColorRGB *result)
{
	g_return_if_fail (p1 != NULL);
	g_return_if_fail (p2 != NULL);
	g_return_if_fail (index >= 0.0f);
	g_return_if_fail (index <= 1.0f);
	g_return_if_fail (result != NULL);
	result->R = (1.0 - index) * p1->R + index * p2->R;
	result->G = (1.0 - index) * p1->G + index * p2->G;
	result->B = (1.0 - index) * p1->B + index * p2->B;
}

/**
 * cd_color_get_blackbody_rgb:
 * @temp: the temperature in Kelvin
 * @result: the destination color
 *
 * Get the blackbody color for a specific temperature.
 *
 * Since: 0.1.26
 **/
void
cd_color_get_blackbody_rgb (guint temp, CdColorRGB *result)
{
	gdouble alpha;
	gint temp_index;

	/* bilinear interpolate the blackbody data */
	alpha = (temp % 100) / 100.0;
	temp_index = (temp - 1000) / 100;
	cd_color_rgb_interpolate (&blackbody_data[temp_index],
				       &blackbody_data[temp_index + 1],
				       alpha,
				       result);
}
