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

#ifndef __CD_PROFILE_ARRAY_H
#define __CD_PROFILE_ARRAY_H

#include <glib-object.h>

#include "cd-profile.h"

G_BEGIN_DECLS

#define CD_TYPE_PROFILE_ARRAY		(cd_profile_array_get_type ())
#define CD_PROFILE_ARRAY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_PROFILE_ARRAY, CdProfileArray))
#define CD_PROFILE_ARRAY_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_PROFILE_ARRAY, CdProfileArrayClass))
#define CD_IS_PROFILE_ARRAY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_PROFILE_ARRAY))
#define CD_IS_PROFILE_ARRAY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_PROFILE_ARRAY))
#define CD_PROFILE_ARRAY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_PROFILE_ARRAY, CdProfileArrayClass))

typedef struct _CdProfileArrayPrivate	CdProfileArrayPrivate;
typedef struct _CdProfileArray		CdProfileArray;
typedef struct _CdProfileArrayClass	CdProfileArrayClass;

struct _CdProfileArray
{
	 GObject		 parent;
	 CdProfileArrayPrivate	*priv;
};

struct _CdProfileArrayClass
{
	GObjectClass		 parent_class;
};

GType		 cd_profile_array_get_type		(void);
CdProfileArray	*cd_profile_array_new			(void);

void		 cd_profile_array_add			(CdProfileArray	*profile_array,
							 CdProfile	*profile);
void		 cd_profile_array_remove		(CdProfileArray	*profile_array,
							 CdProfile	*profile);
CdProfile	*cd_profile_array_get_by_id_owner	(CdProfileArray	*profile_array,
							 const gchar	*id,
							 guint		 owner);
CdProfile	*cd_profile_array_get_by_filename	(CdProfileArray	*profile_array,
							 const gchar	*filename);
CdProfile	*cd_profile_array_get_by_property	(CdProfileArray	*profile_array,
							 const gchar	*key,
							 const gchar	*value);
CdProfile	*cd_profile_array_get_by_object_path	(CdProfileArray	*profile_array,
							 const gchar	*object_path);
GPtrArray	*cd_profile_array_get_by_kind		(CdProfileArray	*profile_array,
							 CdProfileKind	 kind);
GPtrArray	*cd_profile_array_get_by_metadata	(CdProfileArray	*profile_array,
							 const gchar	*key,
							 const gchar	*value);
GVariant	*cd_profile_array_get_variant		(CdProfileArray	*profile_array);

G_END_DECLS

#endif /* __CD_PROFILE_ARRAY_H */

