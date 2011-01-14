/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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

#ifndef __CD_PROFILE_STORE_H
#define __CD_PROFILE_STORE_H

#include <glib-object.h>

#include "cd-profile.h"

G_BEGIN_DECLS

#define CD_TYPE_PROFILE_STORE		(cd_profile_store_get_type ())
#define CD_PROFILE_STORE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_PROFILE_STORE, CdProfileStore))
#define CD_PROFILE_STORE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_PROFILE_STORE, CdProfileStoreClass))
#define CD_IS_PROFILE_STORE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_PROFILE_STORE))
#define CD_IS_PROFILE_STORE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_PROFILE_STORE))
#define CD_PROFILE_STORE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_PROFILE_STORE, CdProfileStoreClass))

typedef struct _CdProfileStorePrivate	CdProfileStorePrivate;
typedef struct _CdProfileStore		CdProfileStore;
typedef struct _CdProfileStoreClass	CdProfileStoreClass;

struct _CdProfileStore
{
	 GObject			 parent;
	 CdProfileStorePrivate		*priv;
};

struct _CdProfileStoreClass
{
	GObjectClass	parent_class;
	void		(* added)		(CdProfile		*profile);
	void		(* removed)		(CdProfile		*profile);
};

typedef enum {
	CD_PROFILE_STORE_SEARCH_NONE		= 0,
	CD_PROFILE_STORE_SEARCH_SYSTEM		= 1,
	CD_PROFILE_STORE_SEARCH_VOLUMES		= 2,
	CD_PROFILE_STORE_SEARCH_MACHINE		= 4
} CdProfileSearchFlags;

GType		 cd_profile_store_get_type		(void);
CdProfileStore	*cd_profile_store_new			(void);
gboolean	 cd_profile_store_search		(CdProfileStore	*profile_store,
							 CdProfileSearchFlags	 flags);

G_END_DECLS

#endif /* __CD_PROFILE_STORE_H */

