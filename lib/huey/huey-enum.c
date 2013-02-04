/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Richard Hughes <richard@hughsie.com>
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
 * SECTION:huey-enum
 * @short_description: Types used by huey and libhuey
 *
 * These helper functions provide a way to marshal enumerated values to
 * text and back again.
 *
 * See also: #CdClient, #CdDevice
 */

#include "config.h"

#include <glib.h>

#include "huey-enum.h"

/**
 * huey_rc_to_string:
 *
 * Since: 0.1.29
 **/
const gchar *
huey_rc_to_string (guchar value)
{
	if (value == HUEY_RC_SUCCESS)
		return "success";
	if (value == HUEY_RC_LOCKED)
		return "locked";
	if (value == HUEY_RC_ERROR)
		return "error";
	if (value == HUEY_RC_RETRY)
		return "retry";
	if (value == HUEY_RC_UNKNOWN_5A)
		return "unknown5a";
	if (value == HUEY_RC_UNKNOWN_81)
		return "unknown81";
	return NULL;
}

/**
 * huey_cmd_code_to_string:
 *
 * Since: 0.1.29
 **/
const gchar *
huey_cmd_code_to_string (guchar value)
{
	if (value == HUEY_CMD_GET_STATUS)
		return "get-status";
	if (value == HUEY_CMD_READ_GREEN)
		return "read-green";
	if (value == HUEY_CMD_READ_BLUE)
		return "read-blue";
	if (value == HUEY_CMD_SET_VALUE)
		return "set-value";
	if (value == HUEY_CMD_GET_VALUE)
		return "get-value";
	if (value == HUEY_CMD_UNKNOWN_07)
		return "unknown07";
	if (value == HUEY_CMD_REGISTER_READ)
		return "reg-read";
	if (value == HUEY_CMD_UNLOCK)
		return "unlock";
	if (value == HUEY_CMD_UNKNOWN_0F)
		return "unknown0f";
	if (value == HUEY_CMD_UNKNOWN_10)
		return "unknown10";
	if (value == HUEY_CMD_UNKNOWN_11)
		return "unknown11";
	if (value == HUEY_CMD_UNKNOWN_12)
		return "unknown12";
	if (value == HUEY_CMD_SENSOR_MEASURE_RGB_CRT)
		return "measure-rgb-crt";
	if (value == HUEY_CMD_UNKNOWN_15)
		return "unknown15(status?)";
	if (value == HUEY_CMD_SENSOR_MEASURE_RGB)
		return "measure-rgb";
	if (value == HUEY_CMD_UNKNOWN_21)
		return "unknown21";
	if (value == HUEY_CMD_GET_AMBIENT)
		return "ambient";
	if (value == HUEY_CMD_SET_LEDS)
		return "set-leds";
	if (value == HUEY_CMD_UNKNOWN_19)
		return "unknown19";
	return NULL;
}
