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
 * cd_color_set_xyz:
 * @dest: the destination color
 * @X: component value
 * @Y: component value
 * @Z: component value
 *
 * Initialises a color value.
 **/
void
cd_color_set_xyz (CdColorXYZ *dest, gdouble X, gdouble Y, gdouble Z)
{
	g_return_if_fail (dest != NULL);

	dest->X = X;
	dest->Y = Y;
	dest->Z = Z;
}

/**
 * cd_color_clear_xyz:
 * @dest: the destination color
 *
 * Initialises a color value.
 **/
void
cd_color_clear_xyz (CdColorXYZ *dest)
{
	g_return_if_fail (dest != NULL);

	dest->X = 0.0f;
	dest->Y = 0.0f;
	dest->Z = 0.0f;
}

/**
 * cd_color_set_rgb:
 * @dest: the destination color
 * @R: component value
 * @G: component value
 * @B: component value
 *
 * Initialises a color value.
 **/
void
cd_color_set_rgb (CdColorRGB *dest, gdouble R, gdouble G, gdouble B)
{
	g_return_if_fail (dest != NULL);

	dest->R = R;
	dest->G = G;
	dest->B = B;
}

/**
 * cd_color_set_yxy:
 * @dest: the destination color
 * @Y: component value
 * @x: component value
 * @y: component value
 *
 * Initialises a color value.
 **/
void
cd_color_set_yxy (CdColorYxy *dest, gdouble Y, gdouble x, gdouble y)
{
	g_return_if_fail (dest != NULL);

	dest->Y = Y;
	dest->x = x;
	dest->y = y;
}

/**
 * cd_color_copy_xyz:
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 **/
void
cd_color_copy_xyz (const CdColorXYZ *src, CdColorXYZ *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	dest->X = src->X;
	dest->Y = src->Y;
	dest->Z = src->Z;
}

/**
 * cd_color_copy_yxy:
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 **/
void
cd_color_copy_yxy (const CdColorYxy *src, CdColorYxy *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	dest->Y = src->Y;
	dest->x = src->x;
	dest->y = src->y;
}

/**
 * cd_color_copy_rgb:
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 **/
void
cd_color_copy_rgb (const CdColorRGB *src, CdColorRGB *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	dest->R = src->R;
	dest->G = src->G;
	dest->B = src->B;
}

/**
 * cd_color_convert_rgb8_to_rgb:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 **/
void
cd_color_convert_rgb8_to_rgb (const CdColorRGB8 *src, CdColorRGB *dest)
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
 * cd_color_convert_rgb_to_rgb8:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 **/
void
cd_color_convert_rgb_to_rgb8 (const CdColorRGB *src, CdColorRGB8 *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	/* also deal with overflow and underflow */
	dest->R = cd_color_value_double_to_uint8 (src->R);
	dest->G = cd_color_value_double_to_uint8 (src->G);
	dest->B = cd_color_value_double_to_uint8 (src->B);
}

/**
 * cd_color_convert_yxy_to_xyz:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 **/
void
cd_color_convert_yxy_to_xyz (const CdColorYxy *src, CdColorXYZ *dest)
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
 * cd_color_convert_xyz_to_yxy:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 **/
void
cd_color_convert_xyz_to_yxy (const CdColorXYZ *src, CdColorYxy *dest)
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
