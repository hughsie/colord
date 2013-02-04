/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#ifndef __CD_MAPPING_DB_H
#define __CD_MAPPING_DB_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CD_TYPE_MAPPING_DB		(cd_mapping_db_get_type ())
#define CD_MAPPING_DB(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_MAPPING_DB, CdMappingDb))
#define CD_MAPPING_DB_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_MAPPING_DB, CdMappingDbClass))
#define CD_IS_MAPPING_DB(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_MAPPING_DB))
#define CD_IS_MAPPING_DB_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_MAPPING_DB))
#define CD_MAPPING_DB_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_MAPPING_DB, CdMappingDbClass))

typedef struct CdMappingDbPrivate CdMappingDbPrivate;

typedef struct
{
	 GObject		 parent;
	 CdMappingDbPrivate	*priv;
} CdMappingDb;

typedef struct
{
	GObjectClass	parent_class;
} CdMappingDbClass;

GType		 cd_mapping_db_get_type		(void);
CdMappingDb	*cd_mapping_db_new		(void);

gboolean	 cd_mapping_db_load		(CdMappingDb	*mdb,
						 const gchar	*filename,
						 GError		**error);
gboolean	 cd_mapping_db_empty		(CdMappingDb	*mdb,
						 GError		**error);
gboolean	 cd_mapping_db_clear_timestamp	(CdMappingDb	*mdb,
						 const gchar	*device_id,
						 const gchar	*profile_id,
						 GError		**error);
gboolean	 cd_mapping_db_add		(CdMappingDb	*mdb,
						 const gchar	*device_id,
						 const gchar	*profile_id,
						 GError		**error);
gboolean	 cd_mapping_db_remove		(CdMappingDb	*mdb,
						 const gchar	*device_id,
						 const gchar	*profile_id,
						 GError		**error);
GPtrArray	*cd_mapping_db_get_profiles	(CdMappingDb	*mdb,
						 const gchar	*device_id,
						 GError		**error);
GPtrArray	*cd_mapping_db_get_devices	(CdMappingDb	*mdb,
						 const gchar	*profile_id,
						 GError		**error);
guint64		 cd_mapping_db_get_timestamp	(CdMappingDb	*mdb,
						 const gchar	*device_id,
						 const gchar	*profile_id,
						 GError		**error);

G_END_DECLS

#endif /* __CD_MAPPING_DB_H */
