/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2013 Richard Hughes <richard@hughsie.com>
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

#ifndef __CD_ICC_STORE_H
#define __CD_ICC_STORE_H

#include <glib-object.h>

#include "cd-icc.h"

G_BEGIN_DECLS

#define CD_TYPE_ICC_STORE		(cd_icc_store_get_type ())
#define CD_ICC_STORE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_ICC_STORE, CdIccStore))
#define CD_ICC_STORE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_ICC_STORE, CdIccStoreClass))
#define CD_IS_ICC_STORE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_ICC_STORE))
#define CD_IS_ICC_STORE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_ICC_STORE))
#define CD_ICC_STORE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_ICC_STORE, CdIccStoreClass))

typedef struct _CdIccStorePrivate	CdIccStorePrivate;
typedef struct _CdIccStore		CdIccStore;
typedef struct _CdIccStoreClass		CdIccStoreClass;

struct _CdIccStore
{
	 GObject		 parent;
	 CdIccStorePrivate	*priv;
};

struct _CdIccStoreClass
{
	GObjectClass	parent_class;
	void		(* added)		(CdIcc		*icc);
	void		(* removed)		(CdIcc		*icc);
};

/**
 * CdIccStoreSearchFlags:
 * @CD_ICC_STORE_SEARCH_FLAGS_NONE:			No flags set.
 * @CD_ICC_STORE_SEARCH_FLAGS_CREATE_LOCATION:		Create the location if it does not exist
 *
 * Flags used when adding scan locations.
 *
 * Since: 1.1.1
 **/
typedef enum {
	CD_ICC_STORE_SEARCH_FLAGS_NONE			= 0,
	CD_ICC_STORE_SEARCH_FLAGS_CREATE_LOCATION	= 1,
	/*< private >*/
	CD_ICC_STORE_SEARCH_FLAGS_LAST
} CdIccStoreSearchFlags;

/**
 * CdIccStoreSearchKind:
 * @CD_ICC_STORE_SEARCH_KIND_SYSTEM:		Per-system locations
 * @CD_ICC_STORE_SEARCH_KIND_MACHINE:		Per-machine locations
 * @CD_ICC_STORE_SEARCH_KIND_USER:		Per-user locations
 *
 * The kind of profiles locations to search.
 *
 * Since: 1.1.1
 **/
typedef enum {
	CD_ICC_STORE_SEARCH_KIND_SYSTEM,
	CD_ICC_STORE_SEARCH_KIND_MACHINE,
	CD_ICC_STORE_SEARCH_KIND_USER,
	/*< private >*/
	CD_ICC_STORE_SEARCH_KIND_LAST
} CdIccStoreSearchKind;

GType		 cd_icc_store_get_type		(void);
CdIccStore	*cd_icc_store_new		(void);
gboolean	 cd_icc_store_search_location	(CdIccStore	*store,
						 const gchar	*location,
						 CdIccStoreSearchFlags search_flags,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 cd_icc_store_search_kind	(CdIccStore	*store,
						 CdIccStoreSearchKind search_kind,
						 CdIccStoreSearchFlags search_flags,
						 GCancellable	*cancellable,
						 GError		**error);
void		 cd_icc_store_set_load_flags	(CdIccStore	*store,
						 CdIccLoadFlags	 load_flags);
CdIccLoadFlags	 cd_icc_store_get_load_flags	(CdIccStore	*store);
void		 cd_icc_store_set_cache		(CdIccStore	*store,
						 GResource	*cache);
GPtrArray	*cd_icc_store_get_all		(CdIccStore	*store);
CdIcc		*cd_icc_store_find_by_filename	(CdIccStore	*store,
						 const gchar	*filename);
CdIcc		*cd_icc_store_find_by_checksum	(CdIccStore	*store,
						 const gchar	*checksum);

G_END_DECLS

#endif /* __CD_ICC_STORE_H */
