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

#ifndef __CD_SENSOR_DUMMY_H
#define __CD_SENSOR_DUMMY_H

#include <glib-object.h>

#include "cd-sensor.h"

G_BEGIN_DECLS

#define CD_TYPE_SENSOR_DUMMY		(cd_sensor_dummy_get_type ())
#define CD_SENSOR_DUMMY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_SENSOR_DUMMY, CdSensorDummy))
#define CD_SENSOR_DUMMY_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_SENSOR_DUMMY, CdSensorDummyClass))
#define CD_IS_SENSOR_DUMMY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_SENSOR_DUMMY))

typedef struct _CdSensorHueyPrivate	CdSensorHueyPrivate;
typedef struct _CdSensorDummy		CdSensorDummy;
typedef struct _CdSensorDummyClass	CdSensorDummyClass;

struct _CdSensorDummy
{
	 CdSensor		 parent;
	 CdSensorHueyPrivate	*priv;
};

struct _CdSensorDummyClass
{
	CdSensorClass		 parent_class;
};

GType		 cd_sensor_dummy_get_type		 (void);
CdSensor	*cd_sensor_dummy_new			 (void);

G_END_DECLS

#endif /* __CD_SENSOR_DUMMY_H */

