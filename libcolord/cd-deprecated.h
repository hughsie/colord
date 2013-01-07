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

#if !defined (__COLORD_H_INSIDE__) && !defined (CD_COMPILATION)
#error "Only <colord.h> can be included directly."
#endif

#ifndef __CD_DEPRECATED_H__
#define __CD_DEPRECATED_H__

#include <glib-object.h>

#ifndef CD_DISABLE_DEPRECATED

/* Damn you x-rite for re-using the 'ColorMunki' brand for lots of
 * different types of hardware */
#define CD_SENSOR_KIND_COLOR_MUNKI	CD_SENSOR_KIND_COLOR_MUNKI_PHOTO

G_DEPRECATED_FOR(cd_color_xyz_set)
void		 cd_color_set_xyz			(CdColorXYZ		*dest,
							 gdouble		 X,
							 gdouble		 Y,
							 gdouble		 Z);
G_DEPRECATED_FOR(cd_color_rgb_set)
void		 cd_color_set_rgb			(CdColorRGB		*dest,
							 gdouble		 R,
							 gdouble		 G,
							 gdouble		 B);
G_DEPRECATED_FOR(cd_color_yxy_set)
void		 cd_color_set_yxy			(CdColorYxy		*dest,
							 gdouble		 Y,
							 gdouble		 x,
							 gdouble		 y);
G_DEPRECATED_FOR(cd_color_xyz_copy)
void		 cd_color_copy_xyz			(const CdColorXYZ	*src,
							 CdColorXYZ		*dest);
G_DEPRECATED_FOR(cd_color_yxy_set)
void		 cd_color_copy_yxy			(const CdColorYxy	*src,
							 CdColorYxy		*dest);
G_DEPRECATED_FOR(cd_color_xyz_clear)
void		 cd_color_clear_xyz			(CdColorXYZ		*dest);
G_DEPRECATED_FOR(cd_color_rgb_copy)
void		 cd_color_copy_rgb			(const CdColorRGB	*src,
							 CdColorRGB		*dest);
G_DEPRECATED_FOR(cd_color_rgb8_to_rgb)
void		 cd_color_convert_rgb8_to_rgb		(const CdColorRGB8	*src,
							 CdColorRGB		*dest);
G_DEPRECATED_FOR(cd_color_rgb_to_rgb8)
void		 cd_color_convert_rgb_to_rgb8		(const CdColorRGB	*src,
							 CdColorRGB8		*dest);
G_DEPRECATED_FOR(cd_color_yxy_to_xyz)
void		 cd_color_convert_yxy_to_xyz		(const CdColorYxy	*src,
							 CdColorXYZ		*dest);
G_DEPRECATED_FOR(cd_color_xyz_to_yxy)
void		 cd_color_convert_xyz_to_yxy		(const CdColorXYZ	*src,
							 CdColorYxy		*dest);

#endif /* CD_DISABLE_DEPRECATED */

#endif /* __CD_DEPRECATED_H__ */
