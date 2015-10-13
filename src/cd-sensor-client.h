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

#ifndef __CD_SENSOR_CLIENT_H
#define __CD_SENSOR_CLIENT_H

#include <glib-object.h>

#include "cd-sensor.h"
#include "cd-sensor-client.h"

G_BEGIN_DECLS

#define CD_TYPE_SENSOR_CLIENT		(cd_sensor_client_get_type ())
G_DECLARE_DERIVABLE_TYPE (CdSensorClient, cd_sensor_client, CD, SENSOR_CLIENT, GObject)

struct _CdSensorClientClass
{
	GObjectClass	parent_class;
	void		(* sensor_added)	(CdSensorClient	*sensor_client,
						 CdSensor	*sensor);
	void		(* sensor_removed)	(CdSensorClient	*sensor_client,
						 CdSensor	*sensor);
};

GType		 cd_sensor_client_get_type	(void);
CdSensorClient	*cd_sensor_client_new		(void);
void		 cd_sensor_client_coldplug	(CdSensorClient	*sensor_client);
CdSensor	*cd_sensor_client_get_by_id	(CdSensorClient	*sensor_client,
						 const gchar	*sensor_id);

G_END_DECLS

#endif /* __CD_SENSOR_CLIENT_H */

