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
 * SECTION:cd-deprecated
 * @short_description: Deprecated functionality
 *
 * Functions which have been deprecated.
 */

#include "config.h"

#include <glib.h>

#include "cd-color.h"
#include "cd-deprecated.h"

#ifndef CD_DISABLE_DEPRECATED

/**
 * cd_color_set_xyz:
 * @dest: the destination color
 * @X: component value
 * @Y: component value
 * @Z: component value
 *
 * Initialises a color value.
 *
 * Deprecated: 0.1.27: Use cd_color_xyz_set.
 **/
void
cd_color_set_xyz (CdColorXYZ *dest, gdouble X, gdouble Y, gdouble Z)
{
	cd_color_xyz_set (dest, X, Y, Z);
}

/**
 * cd_color_clear_xyz:
 * @dest: the destination color
 *
 * Initialises a color value.
 *
 * Deprecated: 0.1.27: Use cd_color_xyz_clear.
 **/
void
cd_color_clear_xyz (CdColorXYZ *dest)
{
	cd_color_xyz_clear (dest);
}

/**
 * cd_color_set_rgb:
 * @dest: the destination color
 * @R: component value
 * @G: component value
 * @B: component value
 *
 * Initialises a color value.
 *
 * Deprecated: 0.1.27: Use cd_color_rgb_set.
 **/
void
cd_color_set_rgb (CdColorRGB *dest, gdouble R, gdouble G, gdouble B)
{
	cd_color_rgb_set (dest, R, G, B);
}

/**
 * cd_color_set_yxy:
 * @dest: the destination color
 * @Y: component value
 * @x: component value
 * @y: component value
 *
 * Initialises a color value.
 *
 * Deprecated: 0.1.27: Use cd_color_yxy_set.
 **/
void
cd_color_set_yxy (CdColorYxy *dest, gdouble Y, gdouble x, gdouble y)
{
	cd_color_yxy_set (dest, Y, x, y);
}

/**
 * cd_color_copy_xyz:
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 *
 * Deprecated: 0.1.27: Use cd_color_xyz_copy.
 **/
void
cd_color_copy_xyz (const CdColorXYZ *src, CdColorXYZ *dest)
{
	cd_color_xyz_copy (src, dest);
}

/**
 * cd_color_copy_yxy:
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 *
 * Deprecated: 0.1.27: Use cd_color_yxy_copy.
 **/
void
cd_color_copy_yxy (const CdColorYxy *src, CdColorYxy *dest)
{
	cd_color_yxy_copy (src, dest);
}

/**
 * cd_color_copy_rgb:
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 *
 * Deprecated: 0.1.27: Use cd_color_rgb_copy.
 **/
void
cd_color_copy_rgb (const CdColorRGB *src, CdColorRGB *dest)
{
	cd_color_rgb_copy (src, dest);
}

/**
 * cd_color_convert_rgb8_to_rgb:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 *
 * Deprecated: 0.1.27: Use cd_color_convert_rgb8_to_rgb.
 **/
void
cd_color_convert_rgb8_to_rgb (const CdColorRGB8 *src, CdColorRGB *dest)
{
	cd_color_rgb8_to_rgb (src, dest);
}

/**
 * cd_color_convert_rgb_to_rgb8:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 *
 * Deprecated: 0.1.27: Use cd_color_rgb_to_rgb8.
 **/
void
cd_color_convert_rgb_to_rgb8 (const CdColorRGB *src, CdColorRGB8 *dest)
{
	cd_color_rgb_to_rgb8 (src, dest);
}

/**
 * cd_color_convert_yxy_to_xyz:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 *
 * Deprecated: 0.1.27: Use cd_color_yxy_to_xyz.
 **/
void
cd_color_convert_yxy_to_xyz (const CdColorYxy *src, CdColorXYZ *dest)
{
	cd_color_yxy_to_xyz (src, dest);
}

/**
 * cd_color_convert_xyz_to_yxy:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 *
 * Deprecated: 0.1.27: Use cd_color_xyz_to_yxy.
 **/
void
cd_color_convert_xyz_to_yxy (const CdColorXYZ *src, CdColorYxy *dest)
{
	cd_color_xyz_to_yxy (src, dest);
}

#endif /* CD_DISABLE_DEPRECATED */
