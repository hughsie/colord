/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2012 Richard Hughes <richard@hughsie.com>
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

#ifndef __CD_SENSOR_H
#define __CD_SENSOR_H

#include <glib-object.h>
#include <gio/gio.h>

#include "cd-enum.h"
#include "cd-color.h"

G_BEGIN_DECLS

#define CD_TYPE_SENSOR		(cd_sensor_get_type ())
#define CD_SENSOR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_SENSOR, CdSensor))
#define CD_SENSOR_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_SENSOR, CdSensorClass))
#define CD_IS_SENSOR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_SENSOR))
#define CD_IS_SENSOR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_SENSOR))
#define CD_SENSOR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_SENSOR, CdSensorClass))
#define CD_SENSOR_ERROR		(cd_sensor_error_quark ())
#define CD_SENSOR_TYPE_ERROR	(cd_sensor_error_get_type ())

typedef struct _CdSensorPrivate CdSensorPrivate;

typedef struct
{
	 GObject		 parent;
	 CdSensorPrivate	*priv;
} CdSensor;

typedef struct
{
	GObjectClass		 parent_class;
	void			(*button_pressed)	(CdSensor	*sensor);
	/*< private >*/
	/* Padding for future expansion */
	void (*_cd_sensor_reserved1) (void);
	void (*_cd_sensor_reserved2) (void);
	void (*_cd_sensor_reserved3) (void);
	void (*_cd_sensor_reserved4) (void);
	void (*_cd_sensor_reserved5) (void);
	void (*_cd_sensor_reserved6) (void);
	void (*_cd_sensor_reserved7) (void);
	void (*_cd_sensor_reserved8) (void);
} CdSensorClass;

GType		 cd_sensor_get_type			(void);
GQuark		 cd_sensor_error_quark			(void);
CdSensor	*cd_sensor_new				(void);
CdSensor	*cd_sensor_new_with_object_path		(const gchar	*object_path);

/* async */
void		 cd_sensor_connect			(CdSensor	*sensor,
							 GCancellable	*cancellable,
							 GAsyncReadyCallback callback,
							 gpointer	 user_data);
gboolean	 cd_sensor_connect_finish		(CdSensor	*sensor,
							 GAsyncResult	*res,
							 GError		**error)
							 G_GNUC_WARN_UNUSED_RESULT;
void	 	 cd_sensor_lock				(CdSensor	*sensor,
							 GCancellable	*cancellable,
							 GAsyncReadyCallback callback,
							 gpointer	 user_data);
gboolean 	 cd_sensor_lock_finish			(CdSensor	*sensor,
							 GAsyncResult	*res,
							 GError		**error)
							 G_GNUC_WARN_UNUSED_RESULT;
void		 cd_sensor_unlock			(CdSensor	*sensor,
							 GCancellable	*cancellable,
							 GAsyncReadyCallback callback,
							 gpointer	 user_data);
gboolean	 cd_sensor_unlock_finish		(CdSensor	*sensor,
							 GAsyncResult	*res,
							 GError		**error)
							 G_GNUC_WARN_UNUSED_RESULT;
void		 cd_sensor_set_options			(CdSensor	*sensor,
							 GHashTable	*values,
							 GCancellable	*cancellable,
							 GAsyncReadyCallback callback,
							 gpointer	 user_data);
gboolean	 cd_sensor_set_options_finish		(CdSensor	*sensor,
							 GAsyncResult	*res,
							 GError		**error)
							 G_GNUC_WARN_UNUSED_RESULT;
void		 cd_sensor_get_sample			(CdSensor	*sensor,
							 CdSensorCap	 cap,
							 GCancellable	*cancellable,
							 GAsyncReadyCallback callback,
							 gpointer	 user_data);
CdColorXYZ	*cd_sensor_get_sample_finish		(CdSensor	*sensor,
							 GAsyncResult	*res,
							 GError		**error)
							 G_GNUC_WARN_UNUSED_RESULT;

/* getters */
const gchar	*cd_sensor_get_object_path		(CdSensor	*sensor);
const gchar	*cd_sensor_get_id			(CdSensor	*sensor);
gboolean	 cd_sensor_get_connected		(CdSensor	*sensor);
CdSensorKind	 cd_sensor_get_kind			(CdSensor	*sensor);
CdSensorState	 cd_sensor_get_state			(CdSensor	*sensor);
CdSensorCap	 cd_sensor_get_mode			(CdSensor	*sensor);
const gchar	*cd_sensor_get_serial			(CdSensor	*sensor);
const gchar	*cd_sensor_get_model			(CdSensor	*sensor);
const gchar	*cd_sensor_get_vendor			(CdSensor	*sensor);
gboolean	 cd_sensor_get_native			(CdSensor	*sensor);
gboolean	 cd_sensor_get_embedded			(CdSensor	*sensor);
gboolean	 cd_sensor_get_locked			(CdSensor	*sensor);
guint64		 cd_sensor_get_caps			(CdSensor	*sensor);
gboolean	 cd_sensor_has_cap			(CdSensor	*sensor,
							 CdSensorCap	 cap);
GHashTable	*cd_sensor_get_options			(CdSensor	*sensor);
const gchar	*cd_sensor_get_option			(CdSensor	*sensor,
							 const gchar	*key);
GHashTable	*cd_sensor_get_metadata			(CdSensor	*sensor);
const gchar	*cd_sensor_get_metadata_item		(CdSensor	*sensor,
							 const gchar	*key);

/* utilities */
void		 cd_sensor_set_object_path		(CdSensor	*sensor,
							 const gchar	*object_path);
gboolean	 cd_sensor_equal			(CdSensor	*sensor1,
							 CdSensor	*sensor2);
gchar		*cd_sensor_to_string			(CdSensor	*sensor)
							 G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* __CD_SENSOR_H */

