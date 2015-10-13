/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

#ifndef __CD_DEVICE_ARRAY_H
#define __CD_DEVICE_ARRAY_H

#include <glib-object.h>

#include "cd-device.h"

G_BEGIN_DECLS

#define CD_TYPE_DEVICE_ARRAY (cd_device_array_get_type ())
G_DECLARE_DERIVABLE_TYPE (CdDeviceArray, cd_device_array, CD, DEVICE_ARRAY, GObject)

struct _CdDeviceArrayClass
{
	GObjectClass		 parent_class;
};

typedef enum {
	CD_DEVICE_ARRAY_FLAG_NONE		= 0,
	CD_DEVICE_ARRAY_FLAG_OWNER_OPTIONAL	= 1,
	CD_DEVICE_ARRAY_FLAG_LAST,
} CdDeviceArrayFlags;

CdDeviceArray	*cd_device_array_new			(void);

void		 cd_device_array_add			(CdDeviceArray	*device_array,
							 CdDevice	*device);
void		 cd_device_array_remove			(CdDeviceArray	*device_array,
							 CdDevice	*device);
CdDevice	*cd_device_array_get_by_id_owner	(CdDeviceArray	*device_array,
							 const gchar	*id,
							 guint		 owner,
							 CdDeviceArrayFlags flags);
CdDevice	*cd_device_array_get_by_object_path	(CdDeviceArray	*device_array,
							 const gchar	*object_path);
CdDevice	*cd_device_array_get_by_property	(CdDeviceArray	*device_array,
							 const gchar	*key,
							 const gchar	*value);
GPtrArray	*cd_device_array_get_array		(CdDeviceArray	*device_array);
GPtrArray	*cd_device_array_get_by_kind		(CdDeviceArray	*device_array,
							 CdDeviceKind	 kind);

G_END_DECLS

#endif /* __CD_DEVICE_ARRAY_H */

