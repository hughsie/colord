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

#ifndef __CD_UDEV_CLIENT_H
#define __CD_UDEV_CLIENT_H

#include <glib-object.h>

#include "cd-device.h"
#include "cd-udev-client.h"

G_BEGIN_DECLS

#define CD_TYPE_UDEV_CLIENT		(cd_udev_client_get_type ())
#define CD_UDEV_CLIENT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_UDEV_CLIENT, CdUdevClient))
#define CD_UDEV_CLIENT_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_UDEV_CLIENT, CdUdevClientClass))
#define CD_IS_UDEV_CLIENT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_UDEV_CLIENT))
#define CD_IS_UDEV_CLIENT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_UDEV_CLIENT))
#define CD_UDEV_CLIENT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_UDEV_CLIENT, CdUdevClientClass))

typedef struct _CdUdevClientPrivate	CdUdevClientPrivate;
typedef struct _CdUdevClient		CdUdevClient;
typedef struct _CdUdevClientClass	CdUdevClientClass;

struct _CdUdevClient
{
	 GObject			 parent;
	 CdUdevClientPrivate		*priv;
};

struct _CdUdevClientClass
{
	GObjectClass	parent_class;
	void		(* device_added)	(CdUdevClient	*udev_client,
						 CdDevice	*device);
	void		(* device_removed)	(CdUdevClient	*udev_client,
						 CdDevice	*device);
};

GType		 cd_udev_client_get_type	(void);
CdUdevClient	*cd_udev_client_new		(void);
void		 cd_udev_client_coldplug	(CdUdevClient	*udev_client);

G_END_DECLS

#endif /* __CD_UDEV_CLIENT_H */

