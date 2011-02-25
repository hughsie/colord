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
	if (rendering_intent == CD_RENDERING_INTENT_RELATIVE_COLORMETRIC)
		return "relative-colormetric";
	if (rendering_intent == CD_RENDERING_INTENT_SATURATION)
		return "saturation";
	if (rendering_intent == CD_RENDERING_INTENT_ABSOLUTE_COLORMETRIC)
		return "absolute-colormetric";
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
	if (g_strcmp0 (rendering_intent, "relative-colormetric") == 0)
		return CD_RENDERING_INTENT_RELATIVE_COLORMETRIC;
	if (g_strcmp0 (rendering_intent, "saturation") == 0)
		return CD_RENDERING_INTENT_SATURATION;
	if (g_strcmp0 (rendering_intent, "absolute-colormetric") == 0)
		return CD_RENDERING_INTENT_ABSOLUTE_COLORMETRIC;
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
