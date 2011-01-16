/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
