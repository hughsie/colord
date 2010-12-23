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

#ifndef __CD_PROFILE_H
#define __CD_PROFILE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CD_TYPE_PROFILE		(cd_profile_get_type ())
#define CD_PROFILE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_PROFILE, CdProfile))
#define CD_PROFILE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_PROFILE, CdProfileClass))
#define CD_IS_PROFILE(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_PROFILE))
#define CD_IS_PROFILE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_PROFILE))
#define CD_PROFILE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_PROFILE, CdProfileClass))

typedef struct _CdProfilePrivate	CdProfilePrivate;
typedef struct _CdProfile		CdProfile;
typedef struct _CdProfileClass		CdProfileClass;

struct _CdProfile
{
	 GObject		 parent;
	 CdProfilePrivate	*priv;
};

struct _CdProfileClass
{
	GObjectClass		 parent_class;
	void			(* invalidate)		(CdProfile	*profile);
};

GType		 cd_profile_get_type			(void);
CdProfile	*cd_profile_new				(void);

/* accessors */
const gchar	*cd_profile_get_id			(CdProfile	*profile);
void		 cd_profile_set_id			(CdProfile	*profile,
							 const gchar	*id);
const gchar	*cd_profile_get_filename		(CdProfile	*profile);
gboolean	 cd_profile_set_filename		(CdProfile	*profile,
							 const gchar	*filename,
							 GError		**error);
gboolean	 cd_profile_register_object		(CdProfile	*profile,
							 GDBusConnection *connection,
							 GDBusInterfaceInfo *info,
							 GError		**error);
const gchar	*cd_profile_get_qualifier		(CdProfile	*profile);
void		 cd_profile_set_qualifier		(CdProfile	*profile,
							 const gchar	*qualifier);
const gchar	*cd_profile_get_title			(CdProfile	*profile);
const gchar	*cd_profile_get_object_path		(CdProfile	*profile);
void		 cd_profile_watch_sender		(CdProfile	*profile,
							 const gchar	*sender);

G_END_DECLS

#endif /* __CD_PROFILE_H */

