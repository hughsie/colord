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

/**
 * SECTION:cd-types
 * @short_description: Types used by colord and libcolord
 *
 * These helper functions provide a way to marshal enumerated values to
 * text and back again.
 *
 * See also: #CdClient, #CdDevice
 */

#include "config.h"

#include <glib.h>

#include "cd-enum.h"

/**
 * cd_device_kind_to_string:
 *
 * Converts a #CdDeviceKind to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.1.0
 **/
const gchar *
cd_device_kind_to_string (CdDeviceKind kind_enum)
{
	const gchar *kind = NULL;
	switch (kind_enum) {
	case CD_DEVICE_KIND_DISPLAY:
		kind = "display";
		break;
	case CD_DEVICE_KIND_SCANNER:
		kind = "scanner";
		break;
	case CD_DEVICE_KIND_PRINTER:
		kind = "printer";
		break;
	case CD_DEVICE_KIND_CAMERA:
		kind = "camera";
		break;
	case CD_DEVICE_KIND_WEBCAM:
		kind = "webcam";
		break;
	default:
		kind = "unknown";
		break;
	}
	return kind;
}

/**
 * cd_device_kind_from_string:
 *
 * Converts a string to a #CdDeviceKind.
 *
 * Return value: enumerated value
 *
 * Since: 0.1.0
 **/
CdDeviceKind
cd_device_kind_from_string (const gchar *type)
{
	if (type == NULL)
		return CD_DEVICE_KIND_UNKNOWN;
	if (g_strcmp0 (type, "display") == 0)
		return CD_DEVICE_KIND_DISPLAY;
	if (g_strcmp0 (type, "scanner") == 0)
		return CD_DEVICE_KIND_SCANNER;
	if (g_strcmp0 (type, "printer") == 0)
		return CD_DEVICE_KIND_PRINTER;
	if (g_strcmp0 (type, "camera") == 0)
		return CD_DEVICE_KIND_CAMERA;
	if (g_strcmp0 (type, "webcam") == 0)
		return CD_DEVICE_KIND_WEBCAM;
	return CD_DEVICE_KIND_UNKNOWN;
}

/**
 * cd_profile_kind_to_string:
 *
 * Since: 2.91.1
 **/
const gchar *
cd_profile_kind_to_string (CdProfileKind kind)
{
	if (kind == CD_PROFILE_KIND_INPUT_DEVICE)
		return "input-device";
	if (kind == CD_PROFILE_KIND_DISPLAY_DEVICE)
		return "display-device";
	if (kind == CD_PROFILE_KIND_OUTPUT_DEVICE)
		return "output-device";
	if (kind == CD_PROFILE_KIND_DEVICELINK)
		return "devicelink";
	if (kind == CD_PROFILE_KIND_COLORSPACE_CONVERSION)
		return "colorspace-conversion";
	if (kind == CD_PROFILE_KIND_ABSTRACT)
		return "abstract";
	if (kind == CD_PROFILE_KIND_NAMED_COLOR)
		return "named-color";
	return "unknown";
}

/**
 * cd_profile_kind_from_string:
 *
 * Since: 2.91.1
 **/
CdProfileKind
cd_profile_kind_from_string (const gchar *profile_kind)
{
	if (g_strcmp0 (profile_kind, "input-device") == 0)
		return CD_PROFILE_KIND_INPUT_DEVICE;
	if (g_strcmp0 (profile_kind, "display-device") == 0)
		return CD_PROFILE_KIND_DISPLAY_DEVICE;
	if (g_strcmp0 (profile_kind, "output-device") == 0)
		return CD_PROFILE_KIND_OUTPUT_DEVICE;
	if (g_strcmp0 (profile_kind, "devicelink") == 0)
		return CD_PROFILE_KIND_DEVICELINK;
	if (g_strcmp0 (profile_kind, "colorspace-conversion") == 0)
		return CD_PROFILE_KIND_COLORSPACE_CONVERSION;
	if (g_strcmp0 (profile_kind, "abstract") == 0)
		return CD_PROFILE_KIND_ABSTRACT;
	if (g_strcmp0 (profile_kind, "named-color") == 0)
		return CD_PROFILE_KIND_NAMED_COLOR;
	return CD_PROFILE_KIND_UNKNOWN;
}

/**
 * cd_rendering_intent_to_string:
 **/
const gchar *
cd_rendering_intent_to_string (CdRenderingIntent rendering_intent)
{
	if (rendering_intent == CD_RENDERING_INTENT_PERCEPTUAL)
		return "perceptual";
	if (rendering_intent == CD_RENDERING_INTENT_RELATIVE_COLORIMETRIC)
		return "relative-colorimetric";
	if (rendering_intent == CD_RENDERING_INTENT_SATURATION)
		return "saturation";
	if (rendering_intent == CD_RENDERING_INTENT_ABSOLUTE_COLORIMETRIC)
		return "absolute-colorimetric";
	return "unknown";
}

/**
 * cd_rendering_intent_from_string:
 **/
CdRenderingIntent
cd_rendering_intent_from_string (const gchar *rendering_intent)
{
	if (g_strcmp0 (rendering_intent, "perceptual") == 0)
		return CD_RENDERING_INTENT_PERCEPTUAL;
	if (g_strcmp0 (rendering_intent, "relative-colorimetric") == 0)
		return CD_RENDERING_INTENT_RELATIVE_COLORIMETRIC;
	if (g_strcmp0 (rendering_intent, "saturation") == 0)
		return CD_RENDERING_INTENT_SATURATION;
	if (g_strcmp0 (rendering_intent, "absolute-colorimetric") == 0)
		return CD_RENDERING_INTENT_ABSOLUTE_COLORIMETRIC;
	return CD_RENDERING_INTENT_UNKNOWN;
}

/**
 * cd_colorspace_to_string:
 **/
const gchar *
cd_colorspace_to_string (CdColorspace colorspace)
{
	if (colorspace == CD_COLORSPACE_XYZ)
		return "xyz";
	if (colorspace == CD_COLORSPACE_LAB)
		return "lab";
	if (colorspace == CD_COLORSPACE_LUV)
		return "luv";
	if (colorspace == CD_COLORSPACE_YCBCR)
		return "ycbcr";
	if (colorspace == CD_COLORSPACE_YXY)
		return "yxy";
	if (colorspace == CD_COLORSPACE_RGB)
		return "rgb";
	if (colorspace == CD_COLORSPACE_GRAY)
		return "gray";
	if (colorspace == CD_COLORSPACE_HSV)
		return "hsv";
	if (colorspace == CD_COLORSPACE_CMYK)
		return "cmyk";
	if (colorspace == CD_COLORSPACE_CMY)
		return "cmy";
	return "unknown";
}

/**
 * cd_colorspace_from_string:
 **/
CdColorspace
cd_colorspace_from_string (const gchar *colorspace)
{
	if (g_strcmp0 (colorspace, "xyz") == 0)
		return CD_COLORSPACE_XYZ;
	if (g_strcmp0 (colorspace, "lab") == 0)
		return CD_COLORSPACE_LAB;
	if (g_strcmp0 (colorspace, "luv") == 0)
		return CD_COLORSPACE_LUV;
	if (g_strcmp0 (colorspace, "ycbcr") == 0)
		return CD_COLORSPACE_YCBCR;
	if (g_strcmp0 (colorspace, "yxy") == 0)
		return CD_COLORSPACE_YXY;
	if (g_strcmp0 (colorspace, "rgb") == 0)
		return CD_COLORSPACE_RGB;
	if (g_strcmp0 (colorspace, "gray") == 0)
		return CD_COLORSPACE_GRAY;
	if (g_strcmp0 (colorspace, "hsv") == 0)
		return CD_COLORSPACE_HSV;
	if (g_strcmp0 (colorspace, "cmyk") == 0)
		return CD_COLORSPACE_CMYK;
	if (g_strcmp0 (colorspace, "cmy") == 0)
		return CD_COLORSPACE_CMY;
	return CD_COLORSPACE_UNKNOWN;
}

/**
 * cd_device_mode_to_string:
 **/
const gchar *
cd_device_mode_to_string (CdDeviceMode device_mode)
{
	if (device_mode == CD_DEVICE_MODE_PHYSICAL)
		return "physical";
	if (device_mode == CD_DEVICE_MODE_VIRTUAL)
		return "virtual";
	return "unknown";
}

/**
 * cd_device_mode_from_string:
 **/
CdDeviceMode
cd_device_mode_from_string (const gchar *device_mode)
{
	if (g_strcmp0 (device_mode, "physical") == 0)
		return CD_DEVICE_MODE_PHYSICAL;
	if (g_strcmp0 (device_mode, "virtual") == 0)
		return CD_DEVICE_MODE_VIRTUAL;
	return CD_DEVICE_MODE_UNKNOWN;
}

/**
 * cd_device_relation_to_string:
 **/
const gchar *
cd_device_relation_to_string (CdDeviceRelation device_relation)
{
	if (device_relation == CD_DEVICE_RELATION_HARD)
		return "hard";
	if (device_relation == CD_DEVICE_RELATION_SOFT)
		return "soft";
	return "unknown";
}

/**
 * cd_device_relation_from_string:
 **/
CdDeviceRelation
cd_device_relation_from_string (const gchar *device_relation)
{
	if (g_strcmp0 (device_relation, "hard") == 0)
		return CD_DEVICE_RELATION_HARD;
	if (g_strcmp0 (device_relation, "soft") == 0)
		return CD_DEVICE_RELATION_SOFT;
	return CD_DEVICE_MODE_UNKNOWN;
}

/**
 * cd_object_scope_to_string:
 **/
const gchar *
cd_object_scope_to_string (CdObjectScope object_scope)
{
	if (object_scope == CD_OBJECT_SCOPE_NORMAL)
		return "normal";
	if (object_scope == CD_OBJECT_SCOPE_TEMP)
		return "temp";
	if (object_scope == CD_OBJECT_SCOPE_DISK)
		return "disk";
	return "unknown";
}

/**
 * cd_object_scope_from_string:
 **/
CdObjectScope
cd_object_scope_from_string (const gchar *object_scope)
{
	if (g_strcmp0 (object_scope, "normal") == 0)
		return CD_OBJECT_SCOPE_NORMAL;
	if (g_strcmp0 (object_scope, "temp") == 0)
		return CD_OBJECT_SCOPE_TEMP;
	if (g_strcmp0 (object_scope, "disk") == 0)
		return CD_OBJECT_SCOPE_DISK;
	return CD_OBJECT_SCOPE_UNKNOWN;
}

/**
 * cd_sensor_kind_to_string:
 * @sensor_kind: a #CdSensorKind
 *
 * Gets the sensor kind as a string.
 *
 * Return value: the sensor kind, e.g. 'huey'.
 **/
const gchar *
cd_sensor_kind_to_string (CdSensorKind sensor_kind)
{
	if (sensor_kind == CD_SENSOR_KIND_DUMMY)
		return "dummy";
	if (sensor_kind == CD_SENSOR_KIND_HUEY)
		return "huey";
	if (sensor_kind == CD_SENSOR_KIND_COLOR_MUNKI_PHOTO)
		return "color-munki-photo";
	if (sensor_kind == CD_SENSOR_KIND_SPYDER)
		return "spyder";
	if (sensor_kind == CD_SENSOR_KIND_SPYDER2)
		return "spyder2";
	if (sensor_kind == CD_SENSOR_KIND_SPYDER3)
		return "spyder3";
	if (sensor_kind == CD_SENSOR_KIND_SPYDER4)
		return "spyder4";
	if (sensor_kind == CD_SENSOR_KIND_DTP20)
		return "dtp20";
	if (sensor_kind == CD_SENSOR_KIND_DTP22)
		return "dtp22";
	if (sensor_kind == CD_SENSOR_KIND_DTP41)
		return "dtp41";
	if (sensor_kind == CD_SENSOR_KIND_DTP51)
		return "dtp51";
	if (sensor_kind == CD_SENSOR_KIND_DTP92)
		return "dtp92";
	if (sensor_kind == CD_SENSOR_KIND_DTP94)
		return "dtp94";
	if (sensor_kind == CD_SENSOR_KIND_SPECTRO_SCAN)
		return "spectro-scan";
	if (sensor_kind == CD_SENSOR_KIND_I1_PRO)
		return "i1-pro";
	if (sensor_kind == CD_SENSOR_KIND_I1_MONITOR)
		return "i1-monitor";
	if (sensor_kind == CD_SENSOR_KIND_COLORIMTRE_HCFR)
		return "colorimtre-hcfr";
	if (sensor_kind == CD_SENSOR_KIND_I1_DISPLAY1)
		return "i1-display1";
	if (sensor_kind == CD_SENSOR_KIND_I1_DISPLAY2)
		return "i1-display2";
	if (sensor_kind == CD_SENSOR_KIND_I1_DISPLAY3)
		return "i1-display3";
	if (sensor_kind == CD_SENSOR_KIND_COLORHUG)
		return "colorhug";
	if (sensor_kind == CD_SENSOR_KIND_COLORHUG_SPECTRO)
		return "colorhug-spectro";
	return "unknown";
}

/**
 * cd_sensor_kind_from_string:
 * @sensor_kind: the sensor kind, e.g. 'huey'.
 *
 * Gets the sensor kind as a enumerated value.
 *
 * Return value: a #CdSensorKind
 **/
CdSensorKind
cd_sensor_kind_from_string (const gchar *sensor_kind)
{
	if (g_strcmp0 (sensor_kind, "dummy") == 0)
		return CD_SENSOR_KIND_DUMMY;
	if (g_strcmp0 (sensor_kind, "huey") == 0)
		return CD_SENSOR_KIND_HUEY;
	if (g_strcmp0 (sensor_kind, "color-munki") == 0 ||
	    g_strcmp0 (sensor_kind, "color-munki-photo") == 0)
		return CD_SENSOR_KIND_COLOR_MUNKI_PHOTO;
	if (g_strcmp0 (sensor_kind, "spyder") == 0)
		return CD_SENSOR_KIND_SPYDER;
	if (g_strcmp0 (sensor_kind, "spyder2") == 0)
		return CD_SENSOR_KIND_SPYDER2;
	if (g_strcmp0 (sensor_kind, "spyder3") == 0)
		return CD_SENSOR_KIND_SPYDER3;
	if (g_strcmp0 (sensor_kind, "spyder4") == 0)
		return CD_SENSOR_KIND_SPYDER4;
	if (g_strcmp0 (sensor_kind, "dtp20") == 0)
		return CD_SENSOR_KIND_DTP20;
	if (g_strcmp0 (sensor_kind, "dtp22") == 0)
		return CD_SENSOR_KIND_DTP22;
	if (g_strcmp0 (sensor_kind, "dtp41") == 0)
		return CD_SENSOR_KIND_DTP41;
	if (g_strcmp0 (sensor_kind, "dtp51") == 0)
		return CD_SENSOR_KIND_DTP51;
	if (g_strcmp0 (sensor_kind, "dtp94") == 0)
		return CD_SENSOR_KIND_DTP94;
	if (g_strcmp0 (sensor_kind, "spectro-scan") == 0)
		return CD_SENSOR_KIND_SPECTRO_SCAN;
	if (g_strcmp0 (sensor_kind, "i1-pro") == 0)
		return CD_SENSOR_KIND_I1_PRO;
	if (g_strcmp0 (sensor_kind, "colorimtre-hcfr") == 0)
		return CD_SENSOR_KIND_COLORIMTRE_HCFR;
	if (g_strcmp0 (sensor_kind, "i1-display3") == 0)
		return CD_SENSOR_KIND_I1_DISPLAY3;
	if (g_strcmp0 (sensor_kind, "colorhug") == 0)
		return CD_SENSOR_KIND_COLORHUG;
	if (g_strcmp0 (sensor_kind, "colorhug-spectro") == 0)
		return CD_SENSOR_KIND_COLORHUG_SPECTRO;
	if (g_strcmp0 (sensor_kind, "color-munki-smile") == 0)
		return CD_SENSOR_KIND_COLOR_MUNKI_SMILE;
	return CD_SENSOR_KIND_UNKNOWN;
}

/**
 * cd_sensor_state_to_string:
 * @sensor_state: a #CdSensorState
 *
 * Gets the sensor stateability as a string.
 *
 * Return value: the sensor stateability, e.g. 'measuring'.
 **/
const gchar *
cd_sensor_state_to_string (CdSensorState sensor_state)
{
	if (sensor_state == CD_SENSOR_STATE_STARTING)
		return "starting";
	if (sensor_state == CD_SENSOR_STATE_IDLE)
		return "idle";
	if (sensor_state == CD_SENSOR_STATE_MEASURING)
		return "measuring";
	if (sensor_state == CD_SENSOR_STATE_BUSY)
		return "busy";
	return "unknown";
}

/**
 * cd_sensor_state_from_string:
 * @sensor_state: the sensor stateability, e.g. 'measuring'.
 *
 * Gets the sensor stateability as a enumerated value.
 *
 * Return value: a #CdSensorState
 **/
CdSensorState
cd_sensor_state_from_string (const gchar *sensor_state)
{
	if (g_strcmp0 (sensor_state, "starting") == 0)
		return CD_SENSOR_STATE_STARTING;
	if (g_strcmp0 (sensor_state, "idle") == 0)
		return CD_SENSOR_STATE_IDLE;
	if (g_strcmp0 (sensor_state, "measuring") == 0)
		return CD_SENSOR_STATE_MEASURING;
	if (g_strcmp0 (sensor_state, "busy") == 0)
		return CD_SENSOR_STATE_BUSY;
	return CD_SENSOR_STATE_UNKNOWN;
}

/**
 * cd_sensor_cap_to_string:
 * @sensor_cap: a #CdSensorCap
 *
 * Gets the sensor capability as a string.
 *
 * Return value: the sensor capability, e.g. 'projector'.
 **/
const gchar *
cd_sensor_cap_to_string (CdSensorCap sensor_cap)
{
	if (sensor_cap == CD_SENSOR_CAP_LCD)
		return "lcd";
	if (sensor_cap == CD_SENSOR_CAP_CRT)
		return "crt";
	if (sensor_cap == CD_SENSOR_CAP_PRINTER)
		return "printer";
	if (sensor_cap == CD_SENSOR_CAP_PROJECTOR)
		return "projector";
	if (sensor_cap == CD_SENSOR_CAP_SPOT)
		return "spot";
	if (sensor_cap == CD_SENSOR_CAP_AMBIENT)
		return "ambient";
	if (sensor_cap == CD_SENSOR_CAP_CALIBRATION)
		return "calibration";
	if (sensor_cap == CD_SENSOR_CAP_LED)
		return "led";
	return "unknown";
}

/**
 * cd_sensor_cap_from_string:
 * @sensor_cap: the sensor capability, e.g. 'projector'.
 *
 * Gets the sensor capability as a enumerated value.
 *
 * Return value: a #CdSensorCap
 **/
CdSensorCap
cd_sensor_cap_from_string (const gchar *sensor_cap)
{
	if (g_strcmp0 (sensor_cap, "lcd") == 0)
		return CD_SENSOR_CAP_LCD;
	if (g_strcmp0 (sensor_cap, "crt") == 0)
		return CD_SENSOR_CAP_CRT;
	if (g_strcmp0 (sensor_cap, "printer") == 0)
		return CD_SENSOR_CAP_PRINTER;
	if (g_strcmp0 (sensor_cap, "projector") == 0)
		return CD_SENSOR_CAP_PROJECTOR;
	if (g_strcmp0 (sensor_cap, "spot") == 0)
		return CD_SENSOR_CAP_SPOT;
	if (g_strcmp0 (sensor_cap, "ambient") == 0)
		return CD_SENSOR_CAP_AMBIENT;
	if (g_strcmp0 (sensor_cap, "calibration") == 0)
		return CD_SENSOR_CAP_CALIBRATION;
	if (g_strcmp0 (sensor_cap, "led") == 0)
		return CD_SENSOR_CAP_LED;
	return CD_SENSOR_CAP_UNKNOWN;
}

/**
 * cd_standard_space_to_string:
 * @standard_space: a #CdStandardSpace
 *
 * Gets the standard colorspace as a string.
 *
 * Return value: the standard colorspace, e.g. 'srgb'.
 **/
const gchar *
cd_standard_space_to_string (CdStandardSpace standard_space)
{
	if (standard_space == CD_STANDARD_SPACE_SRGB)
		return "srgb";
	if (standard_space == CD_STANDARD_SPACE_ADOBE_RGB)
		return "adobe-rgb";
	if (standard_space == CD_STANDARD_SPACE_PROPHOTO_RGB)
		return "prophoto-rgb";
	return "unknown";
}

/**
 * cd_standard_space_from_string:
 * @standard_space: the standard colorspace, e.g. 'srgb'.
 *
 * Gets the standard colorspace as a enumerated value.
 *
 * Return value: a #CdStandardSpace
 **/
CdStandardSpace
cd_standard_space_from_string (const gchar *standard_space)
{
	if (g_strcmp0 (standard_space, "srgb") == 0)
		return CD_STANDARD_SPACE_SRGB;
	if (g_strcmp0 (standard_space, "adobe-rgb") == 0)
		return CD_STANDARD_SPACE_ADOBE_RGB;
	return CD_STANDARD_SPACE_UNKNOWN;
}

/**
 * cd_profile_warning_to_string:
 *
 * Converts a #CdProfileWarning to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.1.25
 **/
const gchar *
cd_profile_warning_to_string (CdProfileWarning kind_enum)
{
	const gchar *kind = NULL;
	switch (kind_enum) {
	case CD_PROFILE_WARNING_NONE:
		kind = "none";
		break;
	case CD_PROFILE_WARNING_DESCRIPTION_MISSING:
		kind = "description-missing";
		break;
	case CD_PROFILE_WARNING_COPYRIGHT_MISSING:
		kind = "copyright-missing";
		break;
	case CD_PROFILE_WARNING_VCGT_NON_MONOTONIC:
		kind = "vcgt-non-monotonic";
		break;
	case CD_PROFILE_WARNING_SCUM_DOT:
		kind = "scum-dot";
		break;
	case CD_PROFILE_WARNING_GRAY_AXIS_INVALID:
		kind = "gray-axis-invalid";
		break;
	case CD_PROFILE_WARNING_GRAY_AXIS_NON_MONOTONIC:
		kind = "gray-axis-non-monotonic";
		break;
	case CD_PROFILE_WARNING_PRIMARIES_INVALID:
		kind = "primaries-invalid";
		break;
	case CD_PROFILE_WARNING_PRIMARIES_NON_ADDITIVE:
		kind = "primaries-non-additive";
		break;
	case CD_PROFILE_WARNING_PRIMARIES_UNLIKELY:
		kind = "primaries-unlikely";
		break;
	case CD_PROFILE_WARNING_WHITEPOINT_INVALID:
		kind = "whitepoint-invalid";
		break;
	default:
		kind = "unknown";
		break;
	}
	return kind;
}

/**
 * cd_profile_warning_from_string:
 *
 * Converts a string to a #CdProfileWarning.
 *
 * Return value: enumerated value
 *
 * Since: 0.1.25
 **/
CdProfileWarning
cd_profile_warning_from_string (const gchar *type)
{
	if (g_strcmp0 (type, "none") == 0)
		return CD_PROFILE_WARNING_NONE;
	if (g_strcmp0 (type, "description-missing") == 0)
		return CD_PROFILE_WARNING_DESCRIPTION_MISSING;
	if (g_strcmp0 (type, "copyright-missing") == 0)
		return CD_PROFILE_WARNING_COPYRIGHT_MISSING;
	if (g_strcmp0 (type, "vcgt-non-monotonic") == 0)
		return CD_PROFILE_WARNING_VCGT_NON_MONOTONIC;
	if (g_strcmp0 (type, "scum-dot") == 0)
		return CD_PROFILE_WARNING_SCUM_DOT;
	if (g_strcmp0 (type, "gray-axis-invalid") == 0)
		return CD_PROFILE_WARNING_GRAY_AXIS_INVALID;
	if (g_strcmp0 (type, "gray-axis-non-monotonic") == 0)
		return CD_PROFILE_WARNING_GRAY_AXIS_NON_MONOTONIC;
	if (g_strcmp0 (type, "primaries-invalid") == 0)
		return CD_PROFILE_WARNING_PRIMARIES_INVALID;
	if (g_strcmp0 (type, "primaries-non-additive") == 0)
		return CD_PROFILE_WARNING_PRIMARIES_NON_ADDITIVE;
	if (g_strcmp0 (type, "primaries-unlikely") == 0)
		return CD_PROFILE_WARNING_PRIMARIES_UNLIKELY;
	if (g_strcmp0 (type, "whitepoint-invalid") == 0)
		return CD_PROFILE_WARNING_WHITEPOINT_INVALID;
	return CD_PROFILE_WARNING_LAST;
}

/**
 * cd_profile_quality_to_string:
 *
 * Converts a #CdProfileQuality to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.1.27
 **/
const gchar *
cd_profile_quality_to_string (CdProfileQuality quality_enum)
{
	const gchar *kind = NULL;
	switch (quality_enum) {
	case CD_PROFILE_QUALITY_LOW:
		kind = "low";
		break;
	case CD_PROFILE_QUALITY_MEDIUM:
		kind = "medium";
		break;
	case CD_PROFILE_QUALITY_HIGH:
		kind = "high";
		break;
	default:
		kind = "unknown";
		break;
	}
	return kind;
}

/**
 * cd_profile_quality_from_string:
 *
 * Converts a string to a #CdProfileQuality.
 *
 * Return value: enumerated value
 *
 * Since: 0.1.27
 **/
CdProfileQuality
cd_profile_quality_from_string (const gchar *quality)
{
	if (g_strcmp0 (quality, "low") == 0)
		return CD_PROFILE_QUALITY_LOW;
	if (g_strcmp0 (quality, "medium") == 0)
		return CD_PROFILE_QUALITY_MEDIUM;
	if (g_strcmp0 (quality, "high") == 0)
		return CD_PROFILE_QUALITY_HIGH;
	return CD_PROFILE_QUALITY_LAST;
}

/**
 * cd_device_kind_to_profile_kind:
 * @device_kind: A #CdDeviceKind
 *
 * Gets the most suitable profile kind for a device kind.
 *
 * Return value: a #CdProfileKind
 *
 * Since: 0.1.6
 **/
CdProfileKind
cd_device_kind_to_profile_kind (CdDeviceKind device_kind)
{
	CdProfileKind profile_kind;
	switch (device_kind) {
	case CD_DEVICE_KIND_DISPLAY:
		profile_kind = CD_PROFILE_KIND_DISPLAY_DEVICE;
		break;
	case CD_DEVICE_KIND_CAMERA:
	case CD_DEVICE_KIND_SCANNER:
		profile_kind = CD_PROFILE_KIND_INPUT_DEVICE;
		break;
	case CD_DEVICE_KIND_PRINTER:
		profile_kind = CD_PROFILE_KIND_OUTPUT_DEVICE;
		break;
	default:
		profile_kind = CD_PROFILE_KIND_UNKNOWN;
	}
	return profile_kind;
}

#define	CD_DBUS_INTERFACE_SENSOR	"org.freedesktop.ColorManager.Sensor"

/**
 * cd_sensor_error_to_string:
 *
 * Converts a #CdSensorError to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.1.26
 **/
const gchar *
cd_sensor_error_to_string (CdSensorError error_enum)
{
	if (error_enum == CD_SENSOR_ERROR_NO_SUPPORT)
		return CD_DBUS_INTERFACE_SENSOR ".NoSupport";
	if (error_enum == CD_SENSOR_ERROR_NO_DATA)
		return CD_DBUS_INTERFACE_SENSOR ".NoData";
	if (error_enum == CD_SENSOR_ERROR_INTERNAL)
		return CD_DBUS_INTERFACE_SENSOR ".Internal";
	if (error_enum == CD_SENSOR_ERROR_ALREADY_LOCKED)
		return CD_DBUS_INTERFACE_SENSOR ".AlreadyLocked";
	if (error_enum == CD_SENSOR_ERROR_NOT_LOCKED)
		return CD_DBUS_INTERFACE_SENSOR ".NotLocked";
	if (error_enum == CD_SENSOR_ERROR_IN_USE)
		return CD_DBUS_INTERFACE_SENSOR ".InUse";
	if (error_enum == CD_SENSOR_ERROR_FAILED_TO_AUTHENTICATE)
		return CD_DBUS_INTERFACE_SENSOR ".FailedToAuthenticate";
	if (error_enum == CD_SENSOR_ERROR_REQUIRED_POSITION_CALIBRATE)
		return CD_DBUS_INTERFACE_SENSOR ".RequiredPositionCalibrate";
	if (error_enum == CD_SENSOR_ERROR_REQUIRED_POSITION_SURFACE)
		return CD_DBUS_INTERFACE_SENSOR ".RequiredPositionSurface";
	return NULL;
}

/**
 * cd_sensor_error_from_string:
 *
 * Converts a string to a #CdSensorError.
 *
 * Return value: enumerated value
 *
 * Since: 0.1.26
 **/
CdSensorError
cd_sensor_error_from_string (const gchar *error_desc)
{
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_SENSOR ".NoSupport") == 0)
		return CD_SENSOR_ERROR_NO_SUPPORT;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_SENSOR ".NoData") == 0)
		return CD_SENSOR_ERROR_NO_DATA;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_SENSOR ".Internal") == 0)
		return CD_SENSOR_ERROR_INTERNAL;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_SENSOR ".AlreadyLocked") == 0)
		return CD_SENSOR_ERROR_ALREADY_LOCKED;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_SENSOR ".NotLocked") == 0)
		return CD_SENSOR_ERROR_NOT_LOCKED;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_SENSOR ".InUse") == 0)
		return CD_SENSOR_ERROR_IN_USE;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_SENSOR ".FailedToAuthenticate") == 0)
		return CD_SENSOR_ERROR_FAILED_TO_AUTHENTICATE;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_SENSOR ".RequiredPositionCalibrate") == 0)
		return CD_SENSOR_ERROR_REQUIRED_POSITION_CALIBRATE;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_SENSOR ".RequiredPositionSurface") == 0)
		return CD_SENSOR_ERROR_REQUIRED_POSITION_SURFACE;
	return CD_SENSOR_ERROR_LAST;
}

#define	CD_DBUS_INTERFACE_PROFILE	"org.freedesktop.ColorManager.Profile"

/**
 * cd_profile_error_to_string:
 *
 * Converts a #CdProfileError to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.1.26
 **/
const gchar *
cd_profile_error_to_string (CdProfileError error_enum)
{
	if (error_enum == CD_PROFILE_ERROR_INTERNAL)
		return CD_DBUS_INTERFACE_PROFILE ".Internal";
	if (error_enum == CD_PROFILE_ERROR_ALREADY_INSTALLED)
		return CD_DBUS_INTERFACE_PROFILE ".AlreadyInstalled";
	if (error_enum == CD_PROFILE_ERROR_FAILED_TO_WRITE)
		return CD_DBUS_INTERFACE_PROFILE ".FailedToWrite";
	if (error_enum == CD_PROFILE_ERROR_FAILED_TO_PARSE)
		return CD_DBUS_INTERFACE_PROFILE ".FailedToParse";
	if (error_enum == CD_PROFILE_ERROR_FAILED_TO_READ)
		return CD_DBUS_INTERFACE_PROFILE ".FailedToRead";
	if (error_enum == CD_PROFILE_ERROR_FAILED_TO_AUTHENTICATE)
		return CD_DBUS_INTERFACE_PROFILE ".FailedToAuthenticate";
	return NULL;
}

/**
 * cd_profile_error_from_string:
 *
 * Converts a string to a #CdProfileError.
 *
 * Return value: enumerated value
 *
 * Since: 0.1.26
 **/
CdProfileError
cd_profile_error_from_string (const gchar *error_desc)
{
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_PROFILE ".Internal") == 0)
		return CD_PROFILE_ERROR_INTERNAL;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_PROFILE ".AlreadyInstalled") == 0)
		return CD_PROFILE_ERROR_ALREADY_INSTALLED;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_PROFILE ".FailedToWrite") == 0)
		return CD_PROFILE_ERROR_FAILED_TO_WRITE;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_PROFILE ".FailedToParse") == 0)
		return CD_PROFILE_ERROR_FAILED_TO_PARSE;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_PROFILE ".FailedToRead") == 0)
		return CD_PROFILE_ERROR_FAILED_TO_READ;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_PROFILE ".FailedToAuthenticate") == 0)
		return CD_PROFILE_ERROR_FAILED_TO_AUTHENTICATE;
	return CD_PROFILE_ERROR_LAST;
}

#define	CD_DBUS_INTERFACE_DEVICE	"org.freedesktop.ColorManager.Device"

/**
 * cd_device_error_to_string:
 *
 * Converts a #CdDeviceError to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.1.26
 **/
const gchar *
cd_device_error_to_string (CdDeviceError error_enum)
{
	if (error_enum == CD_DEVICE_ERROR_INTERNAL)
		return CD_DBUS_INTERFACE_DEVICE ".Internal";
	if (error_enum == CD_DEVICE_ERROR_PROFILE_DOES_NOT_EXIST)
		return CD_DBUS_INTERFACE_DEVICE ".ProfileDoesNotExist";
	if (error_enum == CD_DEVICE_ERROR_PROFILE_ALREADY_ADDED)
		return CD_DBUS_INTERFACE_DEVICE ".ProfileAlreadyAdded";
	if (error_enum == CD_DEVICE_ERROR_PROFILING)
		return CD_DBUS_INTERFACE_DEVICE ".Profiling";
	if (error_enum == CD_DEVICE_ERROR_NOTHING_MATCHED)
		return CD_DBUS_INTERFACE_DEVICE ".NothingMatched";
	if (error_enum == CD_DEVICE_ERROR_FAILED_TO_INHIBIT)
		return CD_DBUS_INTERFACE_DEVICE ".FailedToInhibit";
	if (error_enum == CD_DEVICE_ERROR_FAILED_TO_UNINHIBIT)
		return CD_DBUS_INTERFACE_DEVICE ".FailedToUninhibit";
	if (error_enum == CD_DEVICE_ERROR_FAILED_TO_AUTHENTICATE)
		return CD_DBUS_INTERFACE_DEVICE ".FailedToAuthenticate";
	if (error_enum == CD_DEVICE_ERROR_NOT_ENABLED)
		return CD_DBUS_INTERFACE_DEVICE ".NotEnabled";
	return NULL;
}

/**
 * cd_device_error_from_string:
 *
 * Converts a string to a #CdDeviceError.
 *
 * Return value: enumerated value
 *
 * Since: 0.1.26
 **/
CdDeviceError
cd_device_error_from_string (const gchar *error_desc)
{
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DEVICE ".Internal") == 0)
		return CD_DEVICE_ERROR_INTERNAL;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DEVICE ".ProfileDoesNotExist") == 0)
		return CD_DEVICE_ERROR_PROFILE_DOES_NOT_EXIST;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DEVICE ".ProfileAlreadyAdded") == 0)
		return CD_DEVICE_ERROR_PROFILE_ALREADY_ADDED;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DEVICE ".Profiling") == 0)
		return CD_DEVICE_ERROR_PROFILING;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DEVICE ".NothingMatched") == 0)
		return CD_DEVICE_ERROR_NOTHING_MATCHED;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DEVICE ".FailedToInhibit") == 0)
		return CD_DEVICE_ERROR_FAILED_TO_INHIBIT;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DEVICE ".FailedToUninhibit") == 0)
		return CD_DEVICE_ERROR_FAILED_TO_UNINHIBIT;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DEVICE ".FailedToAuthenticate") == 0)
		return CD_DEVICE_ERROR_FAILED_TO_AUTHENTICATE;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DEVICE ".NotEnabled") == 0)
		return CD_DEVICE_ERROR_NOT_ENABLED;
	return CD_DEVICE_ERROR_LAST;
}

#define	CD_DBUS_INTERFACE_DAEMON	"org.freedesktop.ColorManager"

/**
 * cd_client_error_to_string:
 *
 * Converts a #CdClientError to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.1.26
 **/
const gchar *
cd_client_error_to_string (CdClientError error_enum)
{
	if (error_enum == CD_CLIENT_ERROR_INTERNAL)
		return CD_DBUS_INTERFACE_DAEMON ".Internal";
	if (error_enum == CD_CLIENT_ERROR_ALREADY_EXISTS)
		return CD_DBUS_INTERFACE_DAEMON ".AlreadyExists";
	if (error_enum == CD_CLIENT_ERROR_FAILED_TO_AUTHENTICATE)
		return CD_DBUS_INTERFACE_DAEMON ".FailedToAuthenticate";
	if (error_enum == CD_CLIENT_ERROR_NOT_SUPPORTED)
		return CD_DBUS_INTERFACE_DAEMON ".NotSupported";
	if (error_enum == CD_CLIENT_ERROR_NOT_FOUND)
		return CD_DBUS_INTERFACE_DAEMON ".NotFound";
	if (error_enum == CD_CLIENT_ERROR_INPUT_INVALID)
		return CD_DBUS_INTERFACE_DAEMON ".InputInvalid";
	if (error_enum == CD_CLIENT_ERROR_FILE_INVALID)
		return CD_DBUS_INTERFACE_DAEMON ".FileInvalid";
	return NULL;
}

/**
 * cd_client_error_from_string:
 *
 * Converts a string to a #CdClientError.
 *
 * Return value: enumerated value
 *
 * Since: 0.1.26
 **/
CdClientError
cd_client_error_from_string (const gchar *error_desc)
{
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DAEMON ".Internal") == 0)
		return CD_CLIENT_ERROR_INTERNAL;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DAEMON ".AlreadyExists") == 0)
		return CD_CLIENT_ERROR_ALREADY_EXISTS;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DAEMON ".FailedToAuthenticate") == 0)
		return CD_CLIENT_ERROR_FAILED_TO_AUTHENTICATE;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DAEMON ".NotSupported") == 0)
		return CD_CLIENT_ERROR_NOT_SUPPORTED;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DAEMON ".NotFound") == 0)
		return CD_CLIENT_ERROR_NOT_FOUND;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DAEMON ".InputInvalid") == 0)
		return CD_CLIENT_ERROR_INPUT_INVALID;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_DAEMON ".FileInvalid") == 0)
		return CD_CLIENT_ERROR_FILE_INVALID;
	return CD_CLIENT_ERROR_LAST;
}
