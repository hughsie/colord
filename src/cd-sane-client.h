/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2011 Richard Hughes <richard@hughsie.com>
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

#ifndef __CD_SANE_CLIENT_H
#define __CD_SANE_CLIENT_H

#include <glib-object.h>

#include "cd-device.h"
#include "cd-sane-client.h"

G_BEGIN_DECLS

#define CD_TYPE_SANE_CLIENT		(cd_sane_client_get_type ())
#define CD_SANE_CLIENT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_SANE_CLIENT, CdSaneClient))
#define CD_SANE_CLIENT_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_SANE_CLIENT, CdSaneClientClass))
#define CD_IS_SANE_CLIENT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_SANE_CLIENT))
#define CD_IS_SANE_CLIENT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_SANE_CLIENT))
#define CD_SANE_CLIENT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_SANE_CLIENT, CdSaneClientClass))

typedef struct _CdSaneClientPrivate	CdSaneClientPrivate;
typedef struct _CdSaneClient		CdSaneClient;
typedef struct _CdSaneClientClass	CdSaneClientClass;

struct _CdSaneClient
{
	 GObject			 parent;
	 CdSaneClientPrivate		*priv;
};

struct _CdSaneClientClass
{
	GObjectClass	parent_class;
	void		(* added)		(CdSaneClient	*sane_client,
						 CdDevice	*device);
	void		(* removed)		(CdSaneClient	*sane_client,
						 CdDevice	*device);
};

GType		 cd_sane_client_get_type	(void);
CdSaneClient	*cd_sane_client_new		(void);
gboolean	 cd_sane_client_refresh		(CdSaneClient	*sane_client,
						 GError		**error);

G_END_DECLS

#endif /* __CD_SANE_CLIENT_H */

