/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2014 Richard Hughes <richard@hughsie.com>
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
 * CdEnumMatch:
 *
 * Matching an enumerated type to a string
 **/
typedef struct {
	guint		 value;
	const gchar	*string;
} CdEnumMatch;

static const CdEnumMatch enum_sensor_kind[] = {
	{CD_SENSOR_KIND_UNKNOWN,			"unknown"},	/* fall though value */
	{CD_SENSOR_KIND_COLORHUG,			"colorhug"},
	{CD_SENSOR_KIND_COLORHUG_SPECTRO,		"colorhug-spectro"},
	{CD_SENSOR_KIND_COLORIMTRE_HCFR,		"colorimtre-hcfr"},
	{CD_SENSOR_KIND_COLOR_MUNKI_PHOTO,		"color-munki-photo"},
	{CD_SENSOR_KIND_COLOR_MUNKI_SMILE,		"color-munki-smile"},
	{CD_SENSOR_KIND_DTP20,				"dtp20"},
	{CD_SENSOR_KIND_DTP22,				"dtp22"},
	{CD_SENSOR_KIND_DTP41,				"dtp41"},
	{CD_SENSOR_KIND_DTP51,				"dtp51"},
	{CD_SENSOR_KIND_DTP92,				"dtp92"},
	{CD_SENSOR_KIND_DTP94,				"dtp94"},
	{CD_SENSOR_KIND_DUMMY,				"dummy"},
	{CD_SENSOR_KIND_HUEY,				"huey"},
	{CD_SENSOR_KIND_I1_DISPLAY1,			"i1-display1"},
	{CD_SENSOR_KIND_I1_DISPLAY2,			"i1-display2"},
	{CD_SENSOR_KIND_I1_DISPLAY3,			"i1-display3"},
	{CD_SENSOR_KIND_I1_MONITOR,			"i1-monitor"},
	{CD_SENSOR_KIND_I1_PRO,				"i1-pro"},
	{CD_SENSOR_KIND_SPECTRO_SCAN,			"spectro-scan"},
	{CD_SENSOR_KIND_SPYDER2,			"spyder2"},
	{CD_SENSOR_KIND_SPYDER3,			"spyder3"},
	{CD_SENSOR_KIND_SPYDER4,			"spyder4"},
	{CD_SENSOR_KIND_SPYDER,				"spyder"},
	{0, NULL}
};

static const CdEnumMatch enum_device_kind[] = {
	{CD_DEVICE_KIND_UNKNOWN,			"unknown"},	/* fall though value */
	{CD_DEVICE_KIND_CAMERA,				"camera"},
	{CD_DEVICE_KIND_DISPLAY,			"display"},
	{CD_DEVICE_KIND_PRINTER,			"printer"},
	{CD_DEVICE_KIND_SCANNER,			"scanner"},
	{CD_DEVICE_KIND_WEBCAM,				"webcam"},
	{0, NULL}
};

static const CdEnumMatch enum_profile_kind[] = {
	{CD_PROFILE_KIND_UNKNOWN,			"unknown"},	/* fall though value */
	{CD_PROFILE_KIND_ABSTRACT,			"abstract"},
	{CD_PROFILE_KIND_COLORSPACE_CONVERSION,		"colorspace-conversion"},
	{CD_PROFILE_KIND_DEVICELINK,			"devicelink"},
	{CD_PROFILE_KIND_DISPLAY_DEVICE,		"display-device"},
	{CD_PROFILE_KIND_INPUT_DEVICE,			"input-device"},
	{CD_PROFILE_KIND_NAMED_COLOR,			"named-color"},
	{CD_PROFILE_KIND_OUTPUT_DEVICE,			"output-device"},
	{0, NULL}
};

static const CdEnumMatch enum_rendering_intent[] = {
	{CD_RENDERING_INTENT_UNKNOWN,			"unknown"},	/* fall though value */
	{CD_RENDERING_INTENT_ABSOLUTE_COLORIMETRIC,	"absolute-colorimetric"},
	{CD_RENDERING_INTENT_PERCEPTUAL,		"perceptual"},
	{CD_RENDERING_INTENT_RELATIVE_COLORIMETRIC,	"relative-colorimetric"},
	{CD_RENDERING_INTENT_SATURATION,		"saturation"},
	{0, NULL}
};

static const CdEnumMatch enum_pixel_format[] = {
	{CD_PIXEL_FORMAT_UNKNOWN,			"unknown"},	/* fall though value */
	{CD_PIXEL_FORMAT_ARGB32,			"argb32"},
	{CD_PIXEL_FORMAT_RGB24,				"rgb24"},
	{CD_PIXEL_FORMAT_CMYK32,			"cmyk32"},
	{0, NULL}
};

static const CdEnumMatch enum_colorspace[] = {
	{CD_COLORSPACE_UNKNOWN,				"unknown"},	/* fall though value */
	{CD_COLORSPACE_CMY,				"cmy"},
	{CD_COLORSPACE_CMYK,				"cmyk"},
	{CD_COLORSPACE_GRAY,				"gray"},
	{CD_COLORSPACE_HSV,				"hsv"},
	{CD_COLORSPACE_LAB,				"lab"},
	{CD_COLORSPACE_LUV,				"luv"},
	{CD_COLORSPACE_RGB,				"rgb"},
	{CD_COLORSPACE_XYZ,				"xyz"},
	{CD_COLORSPACE_YCBCR,				"ycbcr"},
	{CD_COLORSPACE_YXY,				"yxy"},
	{0, NULL}
};

static const CdEnumMatch enum_device_relation[] = {
	{CD_DEVICE_RELATION_UNKNOWN,			"unknown"},	/* fall though value */
	{CD_DEVICE_RELATION_HARD,			"hard"},
	{CD_DEVICE_RELATION_SOFT,			"soft"},
	{0, NULL}
};

static const CdEnumMatch enum_device_mode[] = {
	{CD_DEVICE_MODE_UNKNOWN,			"unknown"},	/* fall though value */
	{CD_DEVICE_MODE_PHYSICAL,			"physical"},
	{CD_DEVICE_MODE_VIRTUAL,			"virtual"},
	{0, NULL}
};

static const CdEnumMatch enum_object_scope[] = {
	{CD_OBJECT_SCOPE_UNKNOWN,			"unknown"},	/* fall though value */
	{CD_OBJECT_SCOPE_DISK,				"disk"},
	{CD_OBJECT_SCOPE_NORMAL,			"normal"},
	{CD_OBJECT_SCOPE_TEMP,				"temp"},
	{0, NULL}
};

static const CdEnumMatch enum_sensor_state[] = {
	{CD_SENSOR_STATE_UNKNOWN,			"unknown"},	/* fall though value */
	{CD_SENSOR_STATE_BUSY,				"busy"},
	{CD_SENSOR_STATE_IDLE,				"idle"},
	{CD_SENSOR_STATE_MEASURING,			"measuring"},
	{CD_SENSOR_STATE_STARTING,			"starting"},
	{0, NULL}
};

static const CdEnumMatch enum_sensor_cap[] = {
	{CD_SENSOR_CAP_UNKNOWN,				"unknown"},	/* fall though value */
	{CD_SENSOR_CAP_AMBIENT,				"ambient"},
	{CD_SENSOR_CAP_CALIBRATION,			"calibration"},
	{CD_SENSOR_CAP_CRT,				"crt"},
	{CD_SENSOR_CAP_LCD_CCFL,			"lcd-ccfl"},
	{CD_SENSOR_CAP_LCD,				"lcd"},
	{CD_SENSOR_CAP_LCD_RGB_LED,			"lcd-rgb-led"},
	{CD_SENSOR_CAP_LCD_WHITE_LED,			"lcd-white-led"},
	{CD_SENSOR_CAP_LED,				"led"},
	{CD_SENSOR_CAP_PLASMA,				"plasma"},
	{CD_SENSOR_CAP_PRINTER,				"printer"},
	{CD_SENSOR_CAP_PROJECTOR,			"projector"},
	{CD_SENSOR_CAP_SPOT,				"spot"},
	{CD_SENSOR_CAP_WIDE_GAMUT_LCD_CCFL,		"wide-gamut-lcd-ccfl"},
	{CD_SENSOR_CAP_WIDE_GAMUT_LCD_RGB_LED,		"wide-gamut-lcd-rgb-led"},
	{0, NULL}
};

static const CdEnumMatch enum_standard_space[] = {
	{CD_STANDARD_SPACE_UNKNOWN,			"unknown"},	/* fall though value */
	{CD_STANDARD_SPACE_ADOBE_RGB,			"adobe-rgb"},
	{CD_STANDARD_SPACE_PROPHOTO_RGB,		"prophoto-rgb"},
	{CD_STANDARD_SPACE_SRGB,			"srgb"},
	{0, NULL}
};

static const CdEnumMatch enum_profile_warning[] = {
//	{CD_PROFILE_WARNING_UNKNOWN,			"unknown"},	/* fall though value */
	{CD_PROFILE_WARNING_COPYRIGHT_MISSING,		"copyright-missing"},
	{CD_PROFILE_WARNING_DESCRIPTION_MISSING,	"description-missing"},
	{CD_PROFILE_WARNING_GRAY_AXIS_INVALID,		"gray-axis-invalid"},
	{CD_PROFILE_WARNING_GRAY_AXIS_NON_MONOTONIC,	"gray-axis-non-monotonic"},
	{CD_PROFILE_WARNING_NONE,			"none"},
	{CD_PROFILE_WARNING_PRIMARIES_INVALID,		"primaries-invalid"},
	{CD_PROFILE_WARNING_PRIMARIES_NON_ADDITIVE,	"primaries-non-additive"},
	{CD_PROFILE_WARNING_PRIMARIES_UNLIKELY,		"primaries-unlikely"},
	{CD_PROFILE_WARNING_SCUM_DOT,			"scum-dot"},
	{CD_PROFILE_WARNING_VCGT_NON_MONOTONIC,		"vcgt-non-monotonic"},
	{CD_PROFILE_WARNING_WHITEPOINT_INVALID,		"whitepoint-invalid"},
	{CD_PROFILE_WARNING_WHITEPOINT_UNLIKELY,	"whitepoint-unlikely"},
	{0, NULL}
};

static const CdEnumMatch enum_profile_quality[] = {
//	{CD_PROFILE_QUALITY_UNKNOWN,			"unknown"},	/* fall though value */
	{CD_PROFILE_QUALITY_HIGH,			"high"},
	{CD_PROFILE_QUALITY_LOW,			"low"},
	{CD_PROFILE_QUALITY_MEDIUM,			"medium"},
	{0, NULL}
};

/**
 * cd_enum_from_string:
 * @table: A #CdEnumMatch enum table of values
 * @string: the string constant to search for, e.g. "desktop-gnome"
 *
 * Search for a string value in a table of constants.
 *
 * Return value: the enumerated constant value, e.g. PK_SIGTYPE_ENUM_GPG
 */
static guint
cd_enum_from_string (const CdEnumMatch *table, const gchar *string)
{
	const gchar *string_tmp;
	guint i;

	/* return the first entry on non-found or error */
	if (string == NULL)
		return table[0].value;
	for (i = 0; ; i++) {
		string_tmp = table[i].string;
		if (string_tmp == NULL)
			break;
		if (g_strcmp0 (string, string_tmp) == 0)
			return table[i].value;
	}
	return table[0].value;
}

/**
 * cd_enum_to_string:
 * @table: A #CdEnumMatch enum table of values
 * @value: the enumerated constant value, e.g. PK_SIGTYPE_ENUM_GPG
 *
 * Search for a enum value in a table of constants.
 *
 * Return value: the string constant, e.g. "desktop-gnome"
 */
static const gchar *
cd_enum_to_string (const CdEnumMatch *table, guint value)
{
	guint i;
	guint tmp;
	const gchar *string_tmp;

	for (i=0;;i++) {
		string_tmp = table[i].string;
		if (string_tmp == NULL)
			break;
		tmp = table[i].value;
		if (tmp == value)
			return table[i].string;
	}
	return table[0].string;
}

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
	return cd_enum_to_string (enum_device_kind, kind_enum);
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
	return cd_enum_from_string (enum_device_kind, type);
}

/**
 * cd_profile_kind_to_string:
 *
 * Since: 2.91.1
 **/
const gchar *
cd_profile_kind_to_string (CdProfileKind kind)
{
	return cd_enum_to_string (enum_profile_kind, kind);
}

/**
 * cd_profile_kind_from_string:
 *
 * Since: 2.91.1
 **/
CdProfileKind
cd_profile_kind_from_string (const gchar *profile_kind)
{
	return cd_enum_from_string (enum_profile_kind, profile_kind);
}

/**
 * cd_rendering_intent_to_string:
 **/
const gchar *
cd_rendering_intent_to_string (CdRenderingIntent rendering_intent)
{
	return cd_enum_to_string (enum_rendering_intent, rendering_intent);
}

/**
 * cd_rendering_intent_from_string:
 **/
CdRenderingIntent
cd_rendering_intent_from_string (const gchar *rendering_intent)
{
	return cd_enum_from_string (enum_rendering_intent, rendering_intent);
}

/**
 * cd_pixel_format_to_string:
 **/
const gchar *
cd_pixel_format_to_string (CdPixelFormat pixel_format)
{
	return cd_enum_to_string (enum_pixel_format, pixel_format);
}

/**
 * cd_pixel_format_from_string:
 **/
CdPixelFormat
cd_pixel_format_from_string (const gchar *pixel_format)
{
	return cd_enum_from_string (enum_pixel_format, pixel_format);
}

/**
 * cd_colorspace_to_string:
 **/
const gchar *
cd_colorspace_to_string (CdColorspace colorspace)
{
	return cd_enum_to_string (enum_colorspace, colorspace);
}

/**
 * cd_colorspace_from_string:
 **/
CdColorspace
cd_colorspace_from_string (const gchar *colorspace)
{
	return cd_enum_from_string (enum_colorspace, colorspace);
}

/**
 * cd_device_mode_to_string:
 **/
const gchar *
cd_device_mode_to_string (CdDeviceMode device_mode)
{
	return cd_enum_to_string (enum_device_mode, device_mode);
}

/**
 * cd_device_mode_from_string:
 **/
CdDeviceMode
cd_device_mode_from_string (const gchar *device_mode)
{
	return cd_enum_from_string (enum_device_mode, device_mode);
}

/**
 * cd_device_relation_to_string:
 **/
const gchar *
cd_device_relation_to_string (CdDeviceRelation device_relation)
{
	return cd_enum_to_string (enum_device_relation, device_relation);
}

/**
 * cd_device_relation_from_string:
 **/
CdDeviceRelation
cd_device_relation_from_string (const gchar *device_relation)
{
	return cd_enum_from_string (enum_device_relation, device_relation);
}

/**
 * cd_object_scope_to_string:
 **/
const gchar *
cd_object_scope_to_string (CdObjectScope object_scope)
{
	return cd_enum_to_string (enum_object_scope, object_scope);
}

/**
 * cd_object_scope_from_string:
 **/
CdObjectScope
cd_object_scope_from_string (const gchar *object_scope)
{
	return cd_enum_from_string (enum_object_scope, object_scope);
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
	return cd_enum_to_string (enum_sensor_kind, sensor_kind);
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
	return cd_enum_from_string (enum_sensor_kind, sensor_kind);
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
	return cd_enum_to_string (enum_sensor_state, sensor_state);
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
	return cd_enum_from_string (enum_sensor_state, sensor_state);
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
	return cd_enum_to_string (enum_sensor_cap, sensor_cap);
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
	return cd_enum_from_string (enum_sensor_cap, sensor_cap);
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
	return cd_enum_to_string (enum_standard_space, standard_space);
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
	return cd_enum_from_string (enum_standard_space, standard_space);
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
	return cd_enum_to_string (enum_profile_warning, kind_enum);
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
	return cd_enum_from_string (enum_profile_warning, type);
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
	return cd_enum_to_string (enum_profile_quality, quality_enum);
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
	return cd_enum_from_string (enum_profile_quality, quality);
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
	if (error_enum == CD_PROFILE_ERROR_PROPERTY_INVALID)
		return CD_DBUS_INTERFACE_PROFILE ".PropertyInvalid";
	if (error_enum == CD_PROFILE_ERROR_FAILED_TO_GET_UID)
		return CD_DBUS_INTERFACE_PROFILE ".FailedToGetUid";
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
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_PROFILE ".PropertyInvalid") == 0)
		return CD_PROFILE_ERROR_PROPERTY_INVALID;
	if (g_strcmp0 (error_desc, CD_DBUS_INTERFACE_PROFILE ".FailedToGetUid") == 0)
		return CD_PROFILE_ERROR_FAILED_TO_GET_UID;
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

/**
 * cd_bitfield_from_enums: (skip)
 * @value: the values we want to add to the bitfield
 *
 * Return value: The return bitfield, or 0 if invalid
 *
 * Since: 0.1.26
 **/
guint64
cd_bitfield_from_enums (gint value, ...)
{
	va_list args;
	guint i;
	gint value_temp;
	guint64 values;

	/* we must query at least one thing */
	values = cd_bitfield_value (value);

	/* process the valist */
	va_start (args, value);
	for (i=0;; i++) {
		value_temp = va_arg (args, gint);
		if (value_temp == -1)
			break;
		values += cd_bitfield_value (value_temp);
	}
	va_end (args);

	return values;
}
