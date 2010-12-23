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

#ifndef __CD_DEVICE_H
#define __CD_DEVICE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CD_TYPE_DEVICE		(cd_device_get_type ())
#define CD_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_DEVICE, CdDevice))
#define CD_DEVICE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_DEVICE, CdDeviceClass))
#define CD_IS_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_DEVICE))
#define CD_IS_DEVICE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_DEVICE))
#define CD_DEVICE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_DEVICE, CdDeviceClass))

typedef struct _CdDevicePrivate	CdDevicePrivate;
typedef struct _CdDevice	CdDevice;
typedef struct _CdDeviceClass	CdDeviceClass;

struct _CdDevice
{
	 GObject		 parent;
	 CdDevicePrivate	*priv;
};

struct _CdDeviceClass
{
	GObjectClass		 parent_class;
	void			(* invalidate)		(CdDevice	*device);
};

GType		 cd_device_get_type			(void);
CdDevice	*cd_device_new				(void);

/* accessors */
const gchar	*cd_device_get_id			(CdDevice	*device);
void		 cd_device_set_id			(CdDevice	*device,
							 const gchar	*id);
GPtrArray	*cd_device_get_profiles			(CdDevice	*device);
void		 cd_device_set_profiles			(CdDevice	*device,
							 GPtrArray	*profiles);
const gchar	*cd_device_get_object_path		(CdDevice	*device);
gboolean	 cd_device_register_object		(CdDevice	*device,
							 GDBusConnection *connection,
							 GDBusInterfaceInfo *info,
							 GError		**error);
void		 cd_device_watch_sender			(CdDevice	*device,
							 const gchar	*sender);

G_END_DECLS

#endif /* __CD_DEVICE_H */

