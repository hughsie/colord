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

#include <glib-object.h>
#include <gudev/gudev.h>
#include <gio/gio.h>

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

typedef struct _CdSensorPrivate	CdSensorPrivate;
typedef struct _CdSensor	CdSensor;
typedef struct _CdSensorClass	CdSensorClass;

typedef struct {
	CdColorXYZ	 value;
	gdouble		 luminance;
} CdSensorSample;

struct _CdSensor
{
	 GObject		 parent;
	 CdSensorPrivate	*priv;
};

struct _CdSensorClass
{
	GObjectClass	 parent_class;
	/* vtable */
	void		 (*get_sample_async)	(CdSensor		*sensor,
						 CdSensorCap		 cap,
						 GCancellable		*cancellable,
						 GAsyncReadyCallback	 callback,
						 gpointer		 user_data);
	gboolean	 (*get_sample_finish)	(CdSensor		*sensor,
						 GAsyncResult		*res,
						 CdSensorSample		*value,
						 GError			**error);
	gboolean	 (*dump)		(CdSensor		*sensor,
						 GString		*data,
						 GError			**error);
};

/* dummy */
#define CD_SENSOR_ERROR	1

/* when the data is unavailable */
#define CD_SENSOR_NO_VALUE			-1.0f

/**
 * CdSensorError:
 *
 * The error code.
 **/
typedef enum {
	CD_SENSOR_ERROR_USER_ABORT,
	CD_SENSOR_ERROR_NO_SUPPORT,
	CD_SENSOR_ERROR_NO_DATA,
	CD_SENSOR_ERROR_INTERNAL
} CdSensorError;

GType		 cd_sensor_get_type		(void);
CdSensor	*cd_sensor_new			(void);

/* accessors */
const gchar	*cd_sensor_get_id		(CdSensor		*sensor);
const gchar	*cd_sensor_get_object_path	(CdSensor		*sensor);
gboolean	 cd_sensor_register_object	(CdSensor		*sensor,
						 GDBusConnection	*connection,
						 GDBusInterfaceInfo	*info,
						 GError			**error);
gboolean	 cd_sensor_set_from_device	(CdSensor		*sensor,
						 GUdevDevice		*device,
						 GError			**error);
void		 cd_sensor_button_pressed	(CdSensor		*sensor);
gboolean	 cd_sensor_dump			(CdSensor		*sensor,
						 GString		*data,
						 GError			**error);

/* designed to be used by derived class */
void		 cd_sensor_set_state		(CdSensor		*sensor,
						 CdSensorState		 state);
void		 cd_sensor_set_serial		(CdSensor		*sensor,
						 const gchar		*serial);

/* not used externally */
void		 cd_sensor_get_sample_async	(CdSensor		*sensor,
						 CdSensorCap		 cap,
						 GCancellable		*cancellable,
						 GAsyncReadyCallback	 callback,
						 gpointer		 user_data);
gboolean	 cd_sensor_get_sample_finish	(CdSensor		*sensor,
						 GAsyncResult		*res,
						 CdSensorSample		*sample,
						 GError			**error);
void		 cd_sensor_copy_sample		(const CdSensorSample	*source,
						 CdSensorSample		*result);

G_END_DECLS

#endif /* __CD_SENSOR_H */

