/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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

#ifndef __CD_PROFILE_DB_H
#define __CD_PROFILE_DB_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CD_TYPE_PROFILE_DB		(cd_profile_db_get_type ())
#define CD_PROFILE_DB(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_PROFILE_DB, CdProfileDb))
#define CD_PROFILE_DB_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_PROFILE_DB, CdProfileDbClass))
#define CD_IS_PROFILE_DB(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_PROFILE_DB))
#define CD_IS_PROFILE_DB_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_PROFILE_DB))
#define CD_PROFILE_DB_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_PROFILE_DB, CdProfileDbClass))

typedef struct CdProfileDbPrivate CdProfileDbPrivate;

typedef struct
{
	 GObject		 parent;
	 CdProfileDbPrivate	*priv;
} CdProfileDb;

typedef struct
{
	GObjectClass	parent_class;
} CdProfileDbClass;

GType		 cd_profile_db_get_type		(void);
CdProfileDb	*cd_profile_db_new		(void);

gboolean	 cd_profile_db_load		(CdProfileDb	*pdb,
						 const gchar	*filename,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 cd_profile_db_empty		(CdProfileDb	*pdb,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 cd_profile_db_set_property	(CdProfileDb	*pdb,
						 const gchar	*profile_id,
						 const gchar	*property,
						 guint		 uid,
						 const gchar	*value,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 cd_profile_db_get_property	(CdProfileDb	*pdb,
						 const gchar	*profile_id,
						 const gchar	*property,
						 guint		 uid,
						 gchar		**value,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 cd_profile_db_remove		(CdProfileDb	*pdb,
						 const gchar	*profile_id,
						 const gchar	*property,
						 guint		 uid,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* __CD_PROFILE_DB_H */
