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
 * SECTION:cd-sensor-huey-private
 */

#include "cd-sensor-huey-private.h"

/**
 * cd_sensor_huey_return_code_to_string:
 **/
const gchar *
cd_sensor_huey_return_code_to_string (guchar value)
{
	if (value == CD_SENSOR_HUEY_RETURN_SUCCESS)
		return "success";
	if (value == CD_SENSOR_HUEY_RETURN_LOCKED)
		return "locked";
	if (value == CD_SENSOR_HUEY_RETURN_ERROR)
		return "error";
	if (value == CD_SENSOR_HUEY_RETURN_RETRY)
		return "retry";
	if (value == CD_SENSOR_HUEY_RETURN_UNKNOWN_5A)
		return "unknown5a";
	if (value == CD_SENSOR_HUEY_RETURN_UNKNOWN_81)
		return "unknown81";
	return NULL;
}

/**
 * cd_sensor_huey_command_code_to_string:
 **/
const gchar *
cd_sensor_huey_command_code_to_string (guchar value)
{
	if (value == CD_SENSOR_HUEY_COMMAND_GET_STATUS)
		return "get-status";
	if (value == CD_SENSOR_HUEY_COMMAND_READ_GREEN)
		return "read-green";
	if (value == CD_SENSOR_HUEY_COMMAND_READ_BLUE)
		return "read-blue";
	if (value == CD_SENSOR_HUEY_COMMAND_SET_VALUE)
		return "set-value";
	if (value == CD_SENSOR_HUEY_COMMAND_GET_VALUE)
		return "get-value";
	if (value == CD_SENSOR_HUEY_COMMAND_UNKNOWN_07)
		return "unknown07";
	if (value == CD_SENSOR_HUEY_COMMAND_REGISTER_READ)
		return "reg-read";
	if (value == CD_SENSOR_HUEY_COMMAND_UNLOCK)
		return "unlock";
	if (value == CD_SENSOR_HUEY_COMMAND_UNKNOWN_0F)
		return "unknown0f";
	if (value == CD_SENSOR_HUEY_COMMAND_UNKNOWN_10)
		return "unknown10";
	if (value == CD_SENSOR_HUEY_COMMAND_UNKNOWN_11)
		return "unknown11";
	if (value == CD_SENSOR_HUEY_COMMAND_UNKNOWN_12)
		return "unknown12";
	if (value == CD_SENSOR_HUEY_COMMAND_SENSOR_MEASURE_RGB_CRT)
		return "measure-rgb-crt";
	if (value == CD_SENSOR_HUEY_COMMAND_UNKNOWN_15)
		return "unknown15(status?)";
	if (value == CD_SENSOR_HUEY_COMMAND_SENSOR_MEASURE_RGB)
		return "measure-rgb";
	if (value == CD_SENSOR_HUEY_COMMAND_UNKNOWN_21)
		return "unknown21";
	if (value == CD_SENSOR_HUEY_COMMAND_GET_AMBIENT)
		return "ambient";
	if (value == CD_SENSOR_HUEY_COMMAND_SET_LEDS)
		return "set-leds";
	if (value == CD_SENSOR_HUEY_COMMAND_UNKNOWN_19)
		return "unknown19";
	return NULL;
}
