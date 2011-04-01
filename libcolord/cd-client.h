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

#if !defined (__COLORD_H_INSIDE__) && !defined (CD_COMPILATION)
#error "Only <colord.h> can be included directly."
#endif

#ifndef __CD_CLIENT_H
#define __CD_CLIENT_H

#include <glib-object.h>
#include <gio/gio.h>

#include <libcolord/cd-device.h>
#include <libcolord/cd-profile.h>
#include <libcolord/cd-sensor.h>

G_BEGIN_DECLS

#define CD_TYPE_CLIENT		(cd_client_get_type ())
#define CD_CLIENT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_CLIENT, CdClient))
#define CD_CLIENT_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_CLIENT, CdClientClass))
#define CD_IS_CLIENT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_CLIENT))
#define CD_IS_CLIENT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_CLIENT))
#define CD_CLIENT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_CLIENT, CdClientClass))
#define CD_CLIENT_ERROR		(cd_client_error_quark ())
#define CD_CLIENT_TYPE_ERROR	(cd_client_error_get_type ())

typedef struct _CdClientPrivate CdClientPrivate;

typedef struct
{
	 GObject		 parent;
	 CdClientPrivate	*priv;
} CdClient;

typedef struct
{
	GObjectClass		 parent_class;
	void			(*device_added)		(CdClient		*client,
							 CdDevice		*device);
	void			(*device_removed)	(CdClient		*client,
							 CdDevice		*device);
	void			(*device_changed)	(CdClient		*client,
							 CdDevice		*device);
	void			(*profile_added)	(CdClient		*client,
							 CdProfile		*profile);
	void			(*profile_removed)	(CdClient		*client,
							 CdProfile		*profile);
	void			(*profile_changed)	(CdClient		*client,
							 CdProfile		*profile);
	void			(*sensor_added)		(CdClient		*client,
							 CdSensor		*sensor);
	void			(*sensor_removed)	(CdClient		*client,
							 CdSensor		*sensor);
	void			(*sensor_changed)	(CdClient		*client,
							 CdSensor		*sensor);
	void			(*changed)              (CdClient		*client);
	/*< private >*/
	/* Padding for future expansion */
	void (*_cd_client_reserved1) (void);
	void (*_cd_client_reserved2) (void);
	void (*_cd_client_reserved3) (void);
	void (*_cd_client_reserved4) (void);
	void (*_cd_client_reserved5) (void);
	void (*_cd_client_reserved6) (void);
	void (*_cd_client_reserved7) (void);
	void (*_cd_client_reserved8) (void);
} CdClientClass;

/**
 * CdClientError:
 * @CD_CLIENT_ERROR_FAILED: the transaction failed for an unknown reason
 *
 * Errors that can be thrown
 */
typedef enum
{
	CD_CLIENT_ERROR_FAILED,
	CD_CLIENT_ERROR_LAST
} CdClientError;

GType		 cd_client_get_type			(void);
GQuark		 cd_client_error_quark			(void);
CdClient	*cd_client_new				(void);

CdDevice	*cd_client_create_device_sync		(CdClient	*client,
							 const gchar	*id,
							 CdObjectScope	 scope,
							 GHashTable	*properties,
							 GCancellable	*cancellable,
							 GError		**error);
CdProfile	*cd_client_create_profile_sync		(CdClient	*client,
							 const gchar	*id,
							 CdObjectScope	 scope,
							 GHashTable	*properties,
							 GCancellable	*cancellable,
							 GError		**error);
gboolean	 cd_client_delete_device_sync		(CdClient	*client,
							 const gchar	*id,
							 GCancellable	*cancellable,
							 GError		**error);
gboolean	 cd_client_delete_profile_sync		(CdClient	*client,
							 const gchar	*id,
							 GCancellable	*cancellable,
							 GError		**error);
CdDevice	*cd_client_find_device_sync		(CdClient	*client,
							 const gchar	*id,
							 GCancellable	*cancellable,
							 GError		**error);
CdProfile	*cd_client_find_profile_sync		(CdClient	*client,
							 const gchar	*id,
							 GCancellable	*cancellable,
							 GError		**error);
CdProfile	*cd_client_find_profile_by_filename_sync (CdClient	*client,
							 const gchar	*filename,
							 GCancellable	*cancellable,
							 GError		**error);
GPtrArray	*cd_client_get_devices_sync		(CdClient	*client,
							 GCancellable	*cancellable,
							 GError		**error);
void		 cd_client_get_devices			(CdClient		*client,
							 GCancellable		*cancellable,
							 GAsyncReadyCallback	 callback,
							 gpointer		 user_data);
GPtrArray	*cd_client_get_devices_finish		(CdClient		*client,
							 GAsyncResult		*res,
							 GError			**error);
GPtrArray	*cd_client_get_profiles_sync		(CdClient	*client,
							 GCancellable	*cancellable,
							 GError		**error);
void		 cd_client_get_profiles			(CdClient		*client,
							 GCancellable		*cancellable,
							 GAsyncReadyCallback	 callback,
							 gpointer		 user_data);
GPtrArray	*cd_client_get_profiles_finish		(CdClient		*client,
							 GAsyncResult		*res,
							 GError			**error);
GPtrArray	*cd_client_get_sensors_sync		(CdClient	*client,
							 GCancellable	*cancellable,
							 GError		**error);
void		 cd_client_get_sensors			(CdClient		*client,
							 GCancellable		*cancellable,
							 GAsyncReadyCallback	 callback,
							 gpointer		 user_data);
GPtrArray	*cd_client_get_sensors_finish		(CdClient		*client,
							 GAsyncResult		*res,
							 GError			**error);

GPtrArray	*cd_client_get_devices_by_kind_sync	(CdClient	*client,
							 CdDeviceKind	 kind,
							 GCancellable	*cancellable,
							 GError		**error);
gboolean	 cd_client_connect_sync			(CdClient	*client,
							 GCancellable	*cancellable,
							 GError		**error);
const gchar	*cd_client_get_daemon_version		(CdClient	*client);

G_END_DECLS

#endif /* __CD_CLIENT_H */

