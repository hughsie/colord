/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2014 Richard Hughes <richard@hughsie.com>
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
#include <lcms2.h>

#include "cd-color.h"
#include "cd-interp.h"
#include "cd-interp-akima.h"
#include "cd-interp-linear.h"

/* this is private */
struct _CdColorSwatch {
	gchar		*name;
	CdColorLab	 value;
};

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
 * cd_color_lab_dup:
 *
 * Since: 0.1.32
 **/
CdColorLab *
cd_color_lab_dup (const CdColorLab *src)
{
	CdColorLab *dest;
	g_return_val_if_fail (src != NULL, NULL);
	dest = cd_color_lab_new ();
	dest->L = src->L;
	dest->a = src->a;
	dest->b = src->b;
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
	dest->Y = src->Y;
	dest->x = src->x;
	dest->y = src->y;
	return dest;
}

/**
 * cd_color_uvw_dup:
 *
 * Since: 1.1.6
 **/
CdColorUVW *
cd_color_uvw_dup (const CdColorUVW *src)
{
	CdColorUVW *dest;
	g_return_val_if_fail (src != NULL, NULL);
	dest = cd_color_uvw_new ();
	dest->U = src->U;
	dest->V = src->V;
	dest->W = src->W;
	return dest;
}

/**
 * cd_color_swatch_dup:
 *
 * Since: 0.1.32
 **/
CdColorSwatch *
cd_color_swatch_dup (const CdColorSwatch *src)
{
	CdColorSwatch *dest;
	g_return_val_if_fail (src != NULL, NULL);
	dest = cd_color_swatch_new ();
	dest->name = g_strdup (src->name);
	cd_color_lab_copy (&src->value, &dest->value);
	return dest;
}

/**
 * cd_color_swatch_get_name:
 *
 * Since: 0.1.32
 **/
const gchar *
cd_color_swatch_get_name (const CdColorSwatch *swatch)
{
	g_return_val_if_fail (swatch != NULL, NULL);
	return swatch->name;
}

/**
 * cd_color_swatch_get_value:
 *
 * Since: 0.1.32
 **/
const CdColorLab *
cd_color_swatch_get_value (const CdColorSwatch *swatch)
{
	g_return_val_if_fail (swatch != NULL, NULL);
	return &swatch->value;
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
 * cd_color_lab_get_type:
 *
 * Gets a specific type.
 *
 * Return value: a #GType
 *
 * Since: 0.1.32
 **/
GType
cd_color_lab_get_type (void)
{
	static GType type_id = 0;
	if (!type_id)
		type_id = g_boxed_type_register_static ("CdColorLab",
							(GBoxedCopyFunc) cd_color_lab_dup,
							(GBoxedFreeFunc) cd_color_lab_free);
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
 * cd_color_uvw_get_type:
 *
 * Gets a specific type.
 *
 * Return value: a #GType
 *
 * Since: 1.1.6
 **/
GType
cd_color_uvw_get_type (void)
{
	static GType type_id = 0;
	if (!type_id)
		type_id = g_boxed_type_register_static ("CdColorUVW",
							(GBoxedCopyFunc) cd_color_uvw_dup,
							(GBoxedFreeFunc) cd_color_uvw_free);
	return type_id;
}

/**
 * cd_color_swatch_get_type:
 *
 * Gets a specific type.
 *
 * Return value: a #GType
 *
 * Since: 0.1.32
 **/
GType
cd_color_swatch_get_type (void)
{
	static GType type_id = 0;
	if (!type_id)
		type_id = g_boxed_type_register_static ("CdColorSwatch",
							(GBoxedCopyFunc) cd_color_swatch_dup,
							(GBoxedFreeFunc) cd_color_swatch_free);
	return type_id;
}

/**
 * cd_color_xyz_new:
 *
 * Allocates a color value.
 *
 * Return value: A newly allocated #CdColorXYZ object
 *
 * Since: 0.1.0
 **/
CdColorXYZ *
cd_color_xyz_new (void)
{
	return g_slice_new0 (CdColorXYZ);
}

/**
 * cd_color_rgb_new:
 *
 * Allocates a color value.
 *
 * Return value: A newly allocated #CdColorRGB object
 *
 * Since: 0.1.0
 **/
CdColorRGB *
cd_color_rgb_new (void)
{
	return g_slice_new0 (CdColorRGB);
}

/**
 * cd_color_lab_new:
 *
 * Allocates a color value.
 *
 * Return value: A newly allocated #CdColorLab object
 *
 * Since: 0.1.32
 **/
CdColorLab *
cd_color_lab_new (void)
{
	return g_slice_new0 (CdColorLab);
}

/**
 * cd_color_yxy_new:
 *
 * Allocates a color value.
 *
 * Return value: A newly allocated #CdColorYxy object
 *
 * Since: 0.1.0
 **/
CdColorYxy *
cd_color_yxy_new (void)
{
	return g_slice_new0 (CdColorYxy);
}

/**
 * cd_color_uvw_new:
 *
 * Allocates a color value.
 *
 * Return value: A newly allocated #CdColorUVW object
 *
 * Since: 1.1.6
 **/
CdColorUVW *
cd_color_uvw_new (void)
{
	return g_slice_new0 (CdColorUVW);
}

/**
 * cd_color_swatch_new:
 *
 * Allocates a color value.
 *
 * Return value: A newly allocated #CdColorSwatch object
 *
 * Since: 0.1.32
 **/
CdColorSwatch *
cd_color_swatch_new (void)
{
	return g_slice_new0 (CdColorSwatch);
}

/**
 * cd_color_xyz_free:
 * @src: the color object
 *
 * Deallocates a color value.
 *
 * Since: 0.1.0
 **/
void
cd_color_xyz_free (CdColorXYZ *src)
{
	g_slice_free (CdColorXYZ, src);
}

/**
 * cd_color_rgb_free:
 * @src: the color object
 *
 * Deallocates a color value.
 *
 * Since: 0.1.0
 **/
void
cd_color_rgb_free (CdColorRGB *src)
{
	g_slice_free (CdColorRGB, src);
}

/**
 * cd_color_lab_free:
 * @src: the color object
 *
 * Deallocates a color value.
 *
 * Since: 0.1.32
 **/
void
cd_color_lab_free (CdColorLab *src)
{
	g_slice_free (CdColorLab, src);
}

/**
 * cd_color_yxy_free:
 * @src: the color object
 *
 * Deallocates a color value.
 *
 * Since: 0.1.0
 **/
void
cd_color_yxy_free (CdColorYxy *src)
{
	g_slice_free (CdColorYxy, src);
}

/**
 * cd_color_uvw_free:
 * @src: the color object
 *
 * Deallocates a color value.
 *
 * Since: 1.1.6
 **/
void
cd_color_uvw_free (CdColorUVW *src)
{
	g_slice_free (CdColorUVW, src);
}

/**
 * cd_color_swatch_free:
 * @src: the color object
 *
 * Deallocates a color swatch.
 *
 * Since: 0.1.32
 **/
void
cd_color_swatch_free (CdColorSwatch *src)
{
	g_free (src->name);
	g_slice_free (CdColorSwatch, src);
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
 * cd_color_lab_set:
 * @dest: the destination color
 * @L: component value
 * @a: component value
 * @b: component value
 *
 * Initialises a color value.
 *
 * Since: 0.1.32
 **/
void
cd_color_lab_set (CdColorLab *dest, gdouble L, gdouble a, gdouble b)
{
	g_return_if_fail (dest != NULL);

	dest->L = L;
	dest->a = a;
	dest->b = b;
}

/**
 * cd_color_lab_delta_e76:
 * @p1: Lab value 1
 * @p2: Lab value 2
 *
 * Calculates the ΔE of two colors using the 1976 formula.
 *
 * Return value: distance metric, where JND ΔE ≈ 2.3
 *
 * Since: 0.1.32
 **/
gdouble
cd_color_lab_delta_e76 (const CdColorLab *p1, const CdColorLab *p2)
{
	return sqrt (pow (p2->L - p1->L, 2) +
		     pow (p2->a - p1->a, 2) +
		     pow (p2->b - p1->b, 2));
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
 * cd_color_uvw_set:
 * @dest: the destination color
 * @U: component value
 * @V: component value
 * @W: component value
 *
 * Initialises a color value.
 *
 * Since: 1.1.6
 **/
void
cd_color_uvw_set (CdColorUVW *dest, gdouble U, gdouble V, gdouble W)
{
	g_return_if_fail (dest != NULL);

	dest->U = U;
	dest->V = V;
	dest->W = W;
}

/**
 * cd_color_swatch_set_name:
 * @dest: the destination swatch
 * @name: component name
 *
 * Initialises a swatch name.
 *
 * Since: 0.1.32
 **/
void
cd_color_swatch_set_name (CdColorSwatch *dest, const gchar *name)
{
	g_return_if_fail (dest != NULL);
	g_return_if_fail (name != NULL);
	g_free (dest->name);
	dest->name = g_strdup (name);
}

/**
 * cd_color_swatch_set_value:
 * @dest: the destination swatch
 * @value: component value
 *
 * Initialises a swatch value.
 *
 * Since: 0.1.32
 **/
void
cd_color_swatch_set_value (CdColorSwatch *dest, const CdColorLab *value)
{
	g_return_if_fail (dest != NULL);
	g_return_if_fail (value != NULL);
	cd_color_lab_copy (value, &dest->value);
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
 * cd_color_uvw_copy:
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 *
 * Since: 1.1.6
 **/
void
cd_color_uvw_copy (const CdColorUVW *src, CdColorUVW *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	dest->U = src->U;
	dest->V = src->V;
	dest->W = src->W;
}

/**
 * cd_color_lab_copy:
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 *
 * Since: 0.1.32
 **/
void
cd_color_lab_copy (const CdColorLab *src, CdColorLab *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	dest->L = src->L;
	dest->a = src->a;
	dest->b = src->b;
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
 * cd_color_xyz_normalize:
 * @src: the source color
 * @dest: the destination color
 *
 * Normalizes @src to y=1.0
 *
 * Since: 1.1.6
 **/
void
cd_color_xyz_normalize (const CdColorXYZ *src, gdouble max, CdColorXYZ *dest)
{
	dest->X = max * src->X / src->Y;
	dest->Z = max * src->Z / src->Y;
	dest->Y = max;
}

/**
 * cd_color_xyz_to_cct:
 * @src: the source color
 *
 * Gets the correlated color temperature for the XYZ value.
 *
 * Since: 1.1.6
 **/
gdouble
cd_color_xyz_to_cct (const CdColorXYZ *src)
{
	cmsCIExyY tmp;
	cmsCIEXYZ src_lcms;
	gboolean ret;
	gdouble value;

	/* in case cmsFloat64Number != gdouble */
	src_lcms.X = src->X;
	src_lcms.Y = src->Y;
	src_lcms.Z = src->Z;
	cmsXYZ2xyY (&tmp, &src_lcms);
	ret = cmsTempFromWhitePoint (&value, &tmp);
	if (!ret)
		return -1.f;
	return value;
}

/**
 * cd_color_uvw_get_chroma_difference:
 * @p1: color
 * @p2: color
 *
 * Gets the chromaticity distance in the CIE 1960 UCS.
 *
 * Return value: The Euclidean distance
 *
 * Since: 1.1.6
 **/
gdouble
cd_color_uvw_get_chroma_difference (const CdColorUVW *p1, const CdColorUVW *p2)
{
	return sqrt (pow ((p1->U - p2->U), 2) + pow ((p1->V - p2->V), 2));
}

/**
 * cd_color_uvw_set_planckian_locus:
 * @dest: destination color
 * @temp: temperature in Kelvin
 *
 * Sets the CIEUVW color from a Planckian locus of specific temperature.
 *
 * Since: 1.1.6
 **/
void
cd_color_uvw_set_planckian_locus (CdColorUVW *dest, gdouble temp)
{
	dest->W = 1.0;
	dest->U = (0.860117757 +
		   (1.54118254 * temp * 1e-4) +
		   (1.28641212 * pow (temp, 2) * 1e-7)) /
		  (1.0 +
		   (8.42420235 * temp * 1e-4) +
		   (7.08145163 * pow (temp, 2) * 1e-7));
	dest->V = (0.317398726 +
		   (4.22806245 * temp * 1e-5) +
		   (4.20481691 * pow (temp, 2) * 1e-8)) /
		  (1.0 -
		   (2.89741816 * temp * 1e-5) +
		   (1.61456053 * pow (temp, 2) * 1e-7));
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
		cd_color_yxy_set (dest, 0.f, 0.f, 0.f);
		return;
	}

	dest->Y = src->Y;
	dest->x = src->X / sum;
	dest->y = src->Y / sum;
}

typedef struct {
	gdouble	 Y;
	gdouble	 u;
	gdouble	 v;
} CdColorYuv;

static void
cd_color_xyz_to_yuv (const CdColorXYZ *src, CdColorYuv *dest)
{
	gdouble sum = src->X + 15 * src->Y + 3 * src->Z;
	dest->Y = src->Y;
	dest->u = 4 * src->X / sum;
	dest->v = 6 * src->Y / sum;
}

/**
 * cd_color_xyz_to_uvw:
 * @src: the source color
 * @whitepoint: the whitepoint
 * @dest: the destination color
 *
 * Convert from one color format to another.
 *
 * Since: 1.1.6
 **/
void
cd_color_xyz_to_uvw (const CdColorXYZ *src,
		     const CdColorXYZ *whitepoint,
		     CdColorUVW *dest)
{
	CdColorYuv wp;
	CdColorYuv tmp;

	cd_color_xyz_to_yuv (whitepoint, &wp);
	cd_color_xyz_to_yuv (src, &tmp);

	dest->W = 25 * pow (src->Y * 100.f / wp.Y, 1.f/3.f) - 17.f;
	dest->U = 13 * dest->W * (tmp.u - wp.u);
	dest->V = 13 * dest->W * (tmp.v - wp.v);
}

/**
 * cd_color_yxy_to_uvw:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 *
 * Since: 1.1.6
 **/
void
cd_color_yxy_to_uvw (const CdColorYxy *src, CdColorUVW *dest)
{
	gdouble sum = (-2 * src->x) + (12 * src->y) + (3 * src->Y);
	dest->U = (4 * src->x) / sum;
	dest->V = (6 * src->y) / sum;
	dest->W = src->Y;
}

/* source: https://github.com/jonls/redshift/blob/master/README-colorramp
 * use a Planckian curve below 5000K */
static const CdColorRGB blackbody_data_d65plankian[] = {
	{ 1.0000, 0.1817, 0.0000 }, /* 1000K */
	{ 1.0000, 0.2550, 0.0000 }, /* 1100K */
	{ 1.0000, 0.3094, 0.0000 }, /* 1200K */
	{ 1.0000, 0.3536, 0.0000 }, /* ... */
	{ 1.0000, 0.3909, 0.0000 },
	{ 1.0000, 0.4232, 0.0000 },
	{ 1.0000, 0.4516, 0.0000 },
	{ 1.0000, 0.4768, 0.0000 },
	{ 1.0000, 0.4992, 0.0000 },
	{ 1.0000, 0.5194, 0.0000 },
	{ 1.0000, 0.5436, 0.0868 },
	{ 1.0000, 0.5662, 0.1407 },
	{ 1.0000, 0.5873, 0.1836 },
	{ 1.0000, 0.6072, 0.2214 },
	{ 1.0000, 0.6260, 0.2559 },
	{ 1.0000, 0.6437, 0.2882 },
	{ 1.0000, 0.6605, 0.3187 },
	{ 1.0000, 0.6765, 0.3479 },
	{ 1.0000, 0.6916, 0.3758 },
	{ 1.0000, 0.7060, 0.4027 },
	{ 1.0000, 0.7198, 0.4286 },
	{ 1.0000, 0.7329, 0.4537 },
	{ 1.0000, 0.7454, 0.4779 },
	{ 1.0000, 0.7574, 0.5015 },
	{ 1.0000, 0.7689, 0.5243 },
	{ 1.0000, 0.7799, 0.5464 },
	{ 1.0000, 0.7904, 0.5679 },
	{ 1.0000, 0.8005, 0.5888 },
	{ 1.0000, 0.8102, 0.6092 },
	{ 1.0000, 0.8196, 0.6289 },
	{ 1.0000, 0.8285, 0.6482 },
	{ 1.0000, 0.8372, 0.6669 },
	{ 1.0000, 0.8455, 0.6851 },
	{ 1.0000, 0.8535, 0.7028 },
	{ 1.0000, 0.8612, 0.7201 },
	{ 1.0000, 0.8686, 0.7369 },
	{ 1.0000, 0.8758, 0.7533 },
	{ 1.0000, 0.8827, 0.7692 },
	{ 1.0000, 0.8893, 0.7848 },
	{ 1.0000, 0.8958, 0.7999 },
	{ 1.0000, 0.9020, 0.8147 },
	{ 1.0000, 0.9096, 0.8284 },
	{ 1.0000, 0.9171, 0.8419 },
	{ 1.0000, 0.9244, 0.8552 },
	{ 1.0000, 0.9316, 0.8684 },
	{ 1.0000, 0.9385, 0.8813 },
	{ 1.0000, 0.9454, 0.8940 },
	{ 1.0000, 0.9520, 0.9066 },
	{ 1.0000, 0.9585, 0.9189 },
	{ 1.0000, 0.9649, 0.9311 },
	{ 1.0000, 0.9711, 0.9431 },
	{ 1.0000, 0.9771, 0.9548 },
	{ 1.0000, 0.9831, 0.9664 },
	{ 1.0000, 0.9888, 0.9778 },
	{ 1.0000, 0.9945, 0.9890 },
	{ 1.0000, 1.0000, 1.0000 }, /* 6500K */
	{ 0.9895, 0.9935, 1.0000 },
	{ 0.9794, 0.9872, 1.0000 },
	{ 0.9698, 0.9812, 1.0000 },
	{ 0.9605, 0.9754, 1.0000 },
	{ 0.9516, 0.9698, 1.0000 },
	{ 0.9430, 0.9644, 1.0000 },
	{ 0.9348, 0.9592, 1.0000 },
	{ 0.9269, 0.9542, 1.0000 },
	{ 0.9193, 0.9494, 1.0000 },
	{ 0.9119, 0.9447, 1.0000 },
	{ 0.9049, 0.9402, 1.0000 },
	{ 0.8981, 0.9358, 1.0000 },
	{ 0.8915, 0.9316, 1.0000 },
	{ 0.8852, 0.9275, 1.0000 },
	{ 0.8791, 0.9236, 1.0000 },
	{ 0.8732, 0.9197, 1.0000 },
	{ 0.8674, 0.9160, 1.0000 },
	{ 0.8619, 0.9125, 1.0000 },
	{ 0.8566, 0.9090, 1.0000 },
	{ 0.8514, 0.9056, 1.0000 },
	{ 0.8464, 0.9023, 1.0000 },
	{ 0.8415, 0.8991, 1.0000 },
	{ 0.8368, 0.8960, 1.0000 },
	{ 0.8323, 0.8930, 1.0000 },
	{ 0.8278, 0.8901, 1.0000 },
	{ 0.8235, 0.8873, 1.0000 },
	{ 0.8194, 0.8845, 1.0000 },
	{ 0.8153, 0.8818, 1.0000 },
	{ 0.8114, 0.8792, 1.0000 },
	{ 0.8075, 0.8767, 1.0000 },
	{ 0.8038, 0.8742, 1.0000 },
	{ 0.8002, 0.8718, 1.0000 },
	{ 0.7967, 0.8694, 1.0000 },
	{ 0.7932, 0.8671, 1.0000 },
	{ 0.7898, 0.8649, 1.0000 } /* 10000K */
};

/* source: http://www.vendian.org/mncharity/dir3/blackbody/
 * rescaled to make exactly 6500K equal to full intensity in all
 * channels */
static const CdColorRGB blackbody_data_d65modified[] = {
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
 * cd_color_rgb_from_wavelength:
 * @dest: a #CdColorRGB for the RGB result
 * @wavelength: the wavelength roughly between 380nm and 780nm
 *
 * Set an RGB color which is roughly representative to the wavelength.
 *
 * Since: 1.3.4
 **/
void
cd_color_rgb_from_wavelength (CdColorRGB *dest, gdouble wavelength)
{
	const gdouble gamma = 0.80;
	gdouble factor;

	/* use the colors specified in
	 * http://www.efg2.com/Lab/ScienceAndEngineering/Spectra.htm */
	if (wavelength < 380) {
		dest->R = 0.0;
		dest->G = 0.0;
		dest->B = 0.0;
	} else if (wavelength < 440) {
		dest->R = -(wavelength - 440.f) / (440.f - 380.f);
		dest->G = 0.0;
		dest->B = 1.0;
	} else if (wavelength < 490) {
		dest->R = 0.0;
		dest->G = (wavelength - 440) / (490 - 440);
		dest->B = 1.0;
	} else if (wavelength < 510) {
		dest->R = 0.0;
		dest->G = 1.0;
		dest->B = -(wavelength - 510) / (510 - 490);
	} else if (wavelength < 580) {
		dest->R = (wavelength - 510) / (580 - 510);
		dest->G = 1.0;
		dest->B = 0.0;
	} else if (wavelength < 645) {
		dest->R = 1.0;
		dest->G = -(wavelength - 645) / (645 - 580);
		dest->B = 0.0;
	} else if (wavelength < 781) {
		dest->R = 1.0;
		dest->G = 0.0;
		dest->B = 0.0;
	} else {
		dest->R = 0.0;
		dest->G = 0.0;
		dest->B = 0.0;
	}

	/* intensity should fall off near the vision limits */
	if (wavelength >= 380 && wavelength < 420) {
		factor = 0.3 + 0.7 * (wavelength - 380) / (420 - 380);
	} else if (wavelength >= 420 && wavelength < 701) {
		factor = 1.0;
	} else if (wavelength >= 701 && wavelength < 781) {
		factor = 0.3 + 0.7 * (780 - wavelength) / (780 - 700);
	} else {
		factor = 0.0;
	};

	/* scale by factor and then apply gamma */
	if (dest->R > 0.f)
		dest->R = pow (dest->R * factor, gamma);
	if (dest->G > 0.f)
		dest->G = pow (dest->G * factor, gamma);
	if (dest->B > 0.f)
		dest->B = pow (dest->B * factor, gamma);
}

/**
 * cd_color_rgb_array_is_monotonic:
 * @array: (element-type CdColorRGB): Input array
 *
 * Checks the array for monotonicity.
 *
 * Return value: %TRUE if the array is monotonic
 *
 * Since: 0.1.31
 **/
gboolean
cd_color_rgb_array_is_monotonic (const GPtrArray *array)
{
	CdColorRGB last_rgb;
	CdColorRGB *rgb;
	guint i;

	g_return_val_if_fail (array != NULL, FALSE);

	/* check if monotonic */
	cd_color_rgb_set (&last_rgb, 0.0, 0.0, 0.0);
	for (i = 0; i < array->len; i++) {
		rgb = g_ptr_array_index (array, i);
		if (rgb->R < last_rgb.R)
			return FALSE;
		if (rgb->G < last_rgb.G)
			return FALSE;
		if (rgb->B < last_rgb.B)
			return FALSE;
		cd_color_rgb_copy (rgb, &last_rgb);
	}
	return TRUE;
}

/**
 * cd_color_rgb_array_new:
 *
 * Creates a new RGB array.
 *
 * Return value: (element-type CdColorRGB) (transfer full): New array
 *
 * Since: 0.1.31
 **/
GPtrArray *
cd_color_rgb_array_new (void)
{
	return g_ptr_array_new_with_free_func ((GDestroyNotify) cd_color_rgb_free);
}

/**
 * cd_color_rgb_array_interpolate:
 * @array: (element-type CdColorRGB): Input array
 * @new_length: the target length of the return array
 *
 * Interpolate the RGB array to a different size.
 * This uses the Akima interpolation algorithm unless the array would become
 * non-monotonic, in which case it falls back to linear interpolation.
 *
 * Return value: (element-type CdColorRGB) (transfer full): An array of size @new_length or %NULL
 *
 * Since: 0.1.31
 **/
GPtrArray *
cd_color_rgb_array_interpolate (const GPtrArray *array, guint new_length)
{
	CdColorRGB *rgb;
	CdInterp *interp[3];
	gboolean ret;
	gdouble tmp;
	GPtrArray *result = NULL;
	guint i;
	guint j;
	guint m;

	g_return_val_if_fail (array != NULL, NULL);
	g_return_val_if_fail (new_length > 0, NULL);

	/* check if monotonic */
	ret = cd_color_rgb_array_is_monotonic (array);
	if (!ret)
		goto out;

	/* create new array */
	result = cd_color_rgb_array_new ();
	for (i = 0; i < new_length; i++) {
		rgb = cd_color_rgb_new ();
		g_ptr_array_add (result, rgb);
	}

	/* try each interpolation method in turn */
	for (m = 0; m < 2; m++) {

		/* setup interpolation */
		for (j = 0; j < 3; j++) {
			if (m == 0)
				interp[j] = cd_interp_akima_new ();
			else if (m == 1)
				interp[j] = cd_interp_linear_new ();
		}

		/* add data */
		for (i = 0; i < array->len; i++) {
			rgb = g_ptr_array_index (array, i);
			tmp = (gdouble) i / (gdouble) (array->len - 1);
			cd_interp_insert (interp[0], tmp, rgb->R);
			cd_interp_insert (interp[1], tmp, rgb->G);
			cd_interp_insert (interp[2], tmp, rgb->B);
		}

		/* do interpolation of array */
		for (j = 0; j < 3; j++) {
			ret = cd_interp_prepare (interp[j], NULL);
			if (!ret)
				break;
		}
		for (i = 0; i < new_length; i++) {
			tmp = (gdouble) i / (gdouble) (new_length - 1);
			rgb = g_ptr_array_index (result, i);
			rgb->R = cd_interp_eval (interp[0], tmp, NULL);
			rgb->G = cd_interp_eval (interp[1], tmp, NULL);
			rgb->B = cd_interp_eval (interp[2], tmp, NULL);
		}

		/* tear down the interpolation */
		for (j = 0; j < 3; j++)
			g_object_unref (interp[j]);

		/* check if monotonic */
		ret = cd_color_rgb_array_is_monotonic (result);
		if (ret)
			break;

		/* try harder */
	}
out:
	return result;
}

/**
 * cd_color_get_blackbody_rgb_full:
 * @temp: the temperature in Kelvin
 * @result: the destination color
 * @flags: some #CdColorBlackbodyFlags, e.g. %CD_COLOR_BLACKBODY_FLAG_USE_PLANCKIAN
 *
 * Get the blackbody color for a specific temperature. If the temperature
 * range is outside 1000K to 10000K then the result is clipped.
 *
 * Return value: TRUE if @temp was in range and the result accurate
 *
 * Since: 1.3.5
 **/
gboolean
cd_color_get_blackbody_rgb_full (gdouble temp,
				 CdColorRGB *result,
				 CdColorBlackbodyFlags flags)
{
	gboolean ret = TRUE;
	guint temp_quot, temp_rem;
	const CdColorRGB *blackbody_func = blackbody_data_d65modified;

	g_return_val_if_fail (!isnan (temp), FALSE);
	g_return_val_if_fail (result != NULL, FALSE);

	/* use modified curve */
	if (flags & CD_COLOR_BLACKBODY_FLAG_USE_PLANCKIAN)
		blackbody_func = blackbody_data_d65plankian;

	/* check lower bound */
	if (temp < 1000) {
		ret = FALSE;
		temp = 1000;
	}

	/* check upper bound */
	if (temp > 10000) {
		ret = FALSE;
		temp = 10000;
	}

	/* blackbody data has a resolution of 100 K */
	temp_quot = (guint) temp / 100;
	temp_rem = (guint) temp % 100;

	/* blackbody data starts at 1000 K */
	temp_quot -= 10;

	if (temp_rem == 0) {
		/* exact match for data point */
		*result = blackbody_func[temp_quot];
		return ret;
	}

	/* bilinear interpolate the blackbody data */
	cd_color_rgb_interpolate (&blackbody_func[temp_quot],
				  &blackbody_func[temp_quot + 1],
				  temp_rem / 100.0,
				  result);
	return ret;
}

/**
 * cd_color_get_blackbody_rgb:
 * @temp: the temperature in Kelvin
 * @result: the destination color
 *
 * Get the blackbody color for a specific temperature. If the temperature
 * range is outside 1000K to 10000K then the result is clipped.
 *
 * Return value: TRUE if @temp was in range and the result accurate
 *
 * Since: 0.1.26
 **/
gboolean
cd_color_get_blackbody_rgb (guint temp, CdColorRGB *result)
{
	return cd_color_get_blackbody_rgb_full (temp, result,
						CD_COLOR_BLACKBODY_FLAG_NONE);
}
