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

#ifndef __CD_ENUM_H
#define __CD_ENUM_H

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * CdDeviceKind:
 *
 * The device type.
 **/
typedef enum {
	CD_DEVICE_KIND_UNKNOWN,
	CD_DEVICE_KIND_DISPLAY,
	CD_DEVICE_KIND_SCANNER,
	CD_DEVICE_KIND_PRINTER,
	CD_DEVICE_KIND_CAMERA,
	CD_DEVICE_KIND_LAST
} CdDeviceKind;

/**
 * CdProfileKind:
 *
 * The profile type.
 **/
typedef enum {
	CD_PROFILE_KIND_UNKNOWN,
	CD_PROFILE_KIND_INPUT_DEVICE,
	CD_PROFILE_KIND_DISPLAY_DEVICE,
	CD_PROFILE_KIND_OUTPUT_DEVICE,
	CD_PROFILE_KIND_DEVICELINK,
	CD_PROFILE_KIND_COLORSPACE_CONVERSION,
	CD_PROFILE_KIND_ABSTRACT,
	CD_PROFILE_KIND_NAMED_COLOR,
	CD_PROFILE_KIND_LAST
} CdProfileKind;

/**
 * CdObjectScope:
 *
 * The options type.
 **/
typedef enum {
	CD_OBJECT_SCOPE_UNKNOWN,
	CD_OBJECT_SCOPE_NORMAL,
	CD_OBJECT_SCOPE_TEMP,
	CD_OBJECT_SCOPE_DISK,
	CD_OBJECT_SCOPE_LAST
} CdObjectScope;

/**
 * CdRenderingIntent:
 *
 * The rendering intent.
 **/
typedef enum {
	CD_RENDERING_INTENT_UNKNOWN,
	CD_RENDERING_INTENT_PERCEPTUAL,
	CD_RENDERING_INTENT_RELATIVE_COLORIMETRIC,
	CD_RENDERING_INTENT_SATURATION,
	CD_RENDERING_INTENT_ABSOLUTE_COLORIMETRIC,
	CD_RENDERING_INTENT_LAST
} CdRenderingIntent;

/* I can't spell:
 * These compatibility defines will be removed after 3 releases or
 * six months, whichever comes faster. [2011-03-28] */
#define CD_RENDERING_INTENT_RELATIVE_COLORMETRIC	\
	CD_RENDERING_INTENT_RELATIVE_COLORIMETRIC
#define CD_RENDERING_INTENT_ABSOLUTE_COLORMETRIC	\
	CD_RENDERING_INTENT_ABSOLUTE_COLORIMETRIC

/**
 * CdColorspace:
 *
 * The known colorspace.
 **/
typedef enum {
	CD_COLORSPACE_UNKNOWN,
	CD_COLORSPACE_XYZ,
	CD_COLORSPACE_LAB,
	CD_COLORSPACE_LUV,
	CD_COLORSPACE_YCBCR,
	CD_COLORSPACE_YXY,
	CD_COLORSPACE_RGB,
	CD_COLORSPACE_GRAY,
	CD_COLORSPACE_HSV,
	CD_COLORSPACE_CMYK,
	CD_COLORSPACE_CMY,
	CD_COLORSPACE_LAST
} CdColorspace;

/**
 * CdDeviceMode:
 *
 * The device mode.
 **/
typedef enum {
	CD_DEVICE_MODE_UNKNOWN,
	CD_DEVICE_MODE_PHYSICAL,
	CD_DEVICE_MODE_VIRTUAL,
	CD_DEVICE_MODE_LAST
} CdDeviceMode;

/**
 * CdDeviceRelation:
 *
 * The device to profile relationship.
 **/
typedef enum {
	CD_DEVICE_RELATION_UNKNOWN,
	CD_DEVICE_RELATION_SOFT,
	CD_DEVICE_RELATION_HARD,
	CD_DEVICE_RELATION_LAST,
} CdDeviceRelation;

/**
 * CdSensorKind:
 *
 * The sensor type.
 **/
typedef enum {
	CD_SENSOR_KIND_UNKNOWN,
	CD_SENSOR_KIND_DUMMY,
	CD_SENSOR_KIND_HUEY,
	CD_SENSOR_KIND_COLOR_MUNKI,
	CD_SENSOR_KIND_SPYDER,
	CD_SENSOR_KIND_DTP20,
	CD_SENSOR_KIND_DTP22,
	CD_SENSOR_KIND_DTP41,
	CD_SENSOR_KIND_DTP51,
	CD_SENSOR_KIND_DTP94,
	CD_SENSOR_KIND_SPECTRO_SCAN,
	CD_SENSOR_KIND_I1_PRO,
	CD_SENSOR_KIND_COLORIMTRE_HCFR,
	CD_SENSOR_KIND_LAST
} CdSensorKind;

/**
 * CdSensorCap:
 *
 * The sensor capabilities.
 **/
typedef enum {
	CD_SENSOR_CAP_UNKNOWN,
	CD_SENSOR_CAP_LCD,
	CD_SENSOR_CAP_CRT,
	CD_SENSOR_CAP_PRINTER,
	CD_SENSOR_CAP_SPOT,
	CD_SENSOR_CAP_PROJECTOR,
	CD_SENSOR_CAP_AMBIENT,
	CD_SENSOR_CAP_CALIBRATION,
	CD_SENSOR_CAP_LAST
} CdSensorCap;

/**
 * CdSensorState:
 *
 * The state of the sensor.
 **/
typedef enum {
	CD_SENSOR_STATE_UNKNOWN,
	CD_SENSOR_STATE_STARTING,
	CD_SENSOR_STATE_IDLE,
	CD_SENSOR_STATE_MEASURING,
	CD_SENSOR_STATE_LAST
} CdSensorState;

/**
 * CdStandardSpace:
 *
 * A standard colorspace
 **/
typedef enum {
	CD_STANDARD_SPACE_UNKNOWN,
	CD_STANDARD_SPACE_SRGB,
	CD_STANDARD_SPACE_ADOBE_RGB,
	CD_STANDARD_SPACE_PROPHOTO_RGB,
	CD_STANDARD_SPACE_LAST
} CdStandardSpace;

#define COLORD_DBUS_SERVICE		"org.freedesktop.ColorManager"
#define COLORD_DBUS_PATH		"/org/freedesktop/ColorManager"
#define COLORD_DBUS_INTERFACE		"org.freedesktop.ColorManager"
#define COLORD_DBUS_INTERFACE_DEVICE	"org.freedesktop.ColorManager.Device"
#define COLORD_DBUS_INTERFACE_PROFILE	"org.freedesktop.ColorManager.Profile"
#define COLORD_DBUS_INTERFACE_SENSOR	"org.freedesktop.ColorManager.Sensor"

const gchar	*cd_device_kind_to_string		(CdDeviceKind		 kind_enum);
CdDeviceKind	 cd_device_kind_from_string		(const gchar		*kind);
const gchar	*cd_profile_kind_to_string		(CdProfileKind		 profile_kind);
CdProfileKind	 cd_profile_kind_from_string		(const gchar		*profile_kind);
CdRenderingIntent cd_rendering_intent_from_string	(const gchar		*rendering_intent);
const gchar	*cd_rendering_intent_to_string		(CdRenderingIntent	 rendering_intent);
const gchar	*cd_colorspace_to_string		(CdColorspace		 colorspace);
CdColorspace	 cd_colorspace_from_string		(const gchar		*colorspace);
const gchar	*cd_device_mode_to_string		(CdDeviceMode		 device_mode);
CdDeviceMode	 cd_device_mode_from_string		(const gchar		*device_mode);
const gchar	*cd_device_relation_to_string		(CdDeviceRelation	 device_relation);
CdDeviceRelation cd_device_relation_from_string		(const gchar		*device_relation);
const gchar	*cd_object_scope_to_string		(CdObjectScope		 object_scope);
CdObjectScope	 cd_object_scope_from_string		(const gchar		*object_scope);
const gchar	*cd_sensor_kind_to_string		(CdSensorKind		 sensor_kind);
CdSensorKind	 cd_sensor_kind_from_string		(const gchar		*sensor_kind);
const gchar	*cd_sensor_state_to_string		(CdSensorState		 sensor_state);
CdSensorState	 cd_sensor_state_from_string		(const gchar		*sensor_state);
const gchar	*cd_sensor_cap_to_string		(CdSensorCap		 sensor_cap);
CdSensorCap	 cd_sensor_cap_from_string		(const gchar		*sensor_cap);
const gchar	*cd_standard_space_to_string		(CdStandardSpace	 standard_space);
CdStandardSpace	 cd_standard_space_from_string		(const gchar		*standard_space);

G_END_DECLS

#endif /* __CD_ENUM_H */

