/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

#ifndef __CD_SENSOR_H
#define __CD_SENSOR_H

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>

#ifdef HAVE_GUDEV
#include <gudev/gudev.h>
#endif

#include "cd-common.h"
#include "cd-enum.h"
#include "cd-color.h"

G_BEGIN_DECLS

#define CD_TYPE_SENSOR		(cd_sensor_get_type ())
#define CD_SENSOR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_SENSOR, CdSensor))
#define CD_SENSOR_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_SENSOR, CdSensorClass))
#define CD_IS_SENSOR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_SENSOR))
#define CD_IS_SENSOR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_SENSOR))
#define CD_SENSOR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_SENSOR, CdSensorClass))
#define CD_SENSOR_ERROR		cd_sensor_error_quark()

typedef struct _CdSensorPrivate	CdSensorPrivate;
typedef struct _CdSensor	CdSensor;
typedef struct _CdSensorClass	CdSensorClass;

struct _CdSensor
{
	 GObject		 parent;
	 CdSensorPrivate	*priv;
};

struct _CdSensorClass
{
	GObjectClass	 parent_class;
};

/* when the data is unavailable */
#define CD_SENSOR_NO_VALUE			-1.0f

GType		 cd_sensor_get_type		(void);
CdSensor	*cd_sensor_new			(void);
GQuark		 cd_sensor_error_quark		(void);

/* accessors */
const gchar	*cd_sensor_get_id		(CdSensor		*sensor);
const gchar	*cd_sensor_get_object_path	(CdSensor		*sensor);
const gchar	*cd_sensor_get_device_path	(CdSensor		*sensor);
gboolean	 cd_sensor_register_object	(CdSensor		*sensor,
						 GDBusConnection	*connection,
						 GDBusInterfaceInfo	*info,
						 GError			**error);
#ifdef HAVE_GUDEV
gboolean	 cd_sensor_set_from_device	(CdSensor		*sensor,
						 GUdevDevice		*device,
						 GError			**error);
void		 cd_sensor_set_index		(CdSensor		*sensor,
						 guint			 idx);
#endif
void		 cd_sensor_button_pressed	(CdSensor		*sensor);
gboolean	 cd_sensor_dump			(CdSensor		*sensor,
						 GString		*data,
						 GError			**error);
CdSensorKind	 cd_sensor_get_kind		(CdSensor		*sensor);
void		 cd_sensor_set_kind		(CdSensor		*sensor,
						 CdSensorKind		 kind);
gboolean	 cd_sensor_load			(CdSensor		*sensor,
						 GError			**error);
void		 cd_sensor_set_state		(CdSensor		*sensor,
						 CdSensorState		 state);
void		 cd_sensor_set_mode		(CdSensor		*sensor,
						 CdSensorCap		 mode);
CdSensorCap	 cd_sensor_get_mode		(CdSensor		*sensor);
void		 cd_sensor_set_serial		(CdSensor		*sensor,
						 const gchar		*serial);
void		 cd_sensor_add_option		(CdSensor		*sensor,
						 const gchar		*key,
						 GVariant		*value);

/* GModule */
void		 cd_sensor_get_sample_async	(CdSensor		*sensor,
						 CdSensorCap		 cap,
						 GCancellable		*cancellable,
						 GAsyncReadyCallback	 callback,
						 gpointer		 user_data);
CdColorXYZ	*cd_sensor_get_sample_finish	(CdSensor		*sensor,
						 GAsyncResult		*res,
						 GError			**error);
gboolean	 cd_sensor_coldplug		(CdSensor		*sensor,
						 GError			**error);
gboolean	 cd_sensor_dump_device		(CdSensor		*sensor,
						 GString		*data,
						 GError			**error);
void		 cd_sensor_lock_async		(CdSensor		*sensor,
						 GCancellable		*cancellable,
						 GAsyncReadyCallback	 callback,
						 gpointer		 user_data);
gboolean	 cd_sensor_lock_finish		(CdSensor		*sensor,
						 GAsyncResult		*res,
						 GError			**error);
void		 cd_sensor_unlock_async		(CdSensor		*sensor,
						 GCancellable		*cancellable,
						 GAsyncReadyCallback	 callback,
						 gpointer		 user_data);
gboolean	 cd_sensor_unlock_finish	(CdSensor		*sensor,
						 GAsyncResult		*res,
						 GError			**error);
void		 cd_sensor_set_options_async	(CdSensor		*sensor,
						 GHashTable		*options,
						 GCancellable		*cancellable,
						 GAsyncReadyCallback	 callback,
						 gpointer		 user_data);
gboolean	 cd_sensor_set_options_finish	(CdSensor		*sensor,
						 GAsyncResult		*res,
						 GError			**error);

G_END_DECLS

#endif /* __CD_SENSOR_H */

