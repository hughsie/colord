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
 * SECTION:cd-sensor-dtp94-private
 */

#include "cd-sensor-dtp94-private.h"

/**
 * cd_sensor_dtp94_rc_parse:
 **/
guint8
cd_sensor_dtp94_rc_parse (const guint8 *data, gsize length)
{
	gchar *endptr = NULL;
	guint64 tmp = CD_SENSOR_DTP92_RC_UNKNOWN;

	/* invalid data */
	if (length < 4 ||
	    data[0] != '<' ||
	    data[1] == '\0' ||
	    data[2] == '\0' ||
	    data[3] != '>') {
		goto out;
	}

	/* parse number */
	tmp = g_ascii_strtoull ((const gchar *) data + 1, &endptr, 16);
	if (endptr == (const gchar *) data + 1)
		goto out;
out:
	return tmp;
}

/**
 * cd_sensor_dtp94_rc_to_string:
 **/
const gchar *
cd_sensor_dtp94_rc_to_string (guint8 value)
{
	if (value == CD_SENSOR_DTP92_RC_OK)
		return "ok";
	if (value == CD_SENSOR_DTP92_RC_BAD_COMMAND)
		return "bad-command";
	if (value == CD_SENSOR_DTP92_RC_PRM_RANGE)
		return "prm-range";
	if (value == CD_SENSOR_DTP92_RC_MEMORY_OVERFLOW)
		return "memory-overflow";
	if (value == CD_SENSOR_DTP92_RC_INVALID_BAUD_RATE)
		return "invalid-baud-rate";
	if (value == CD_SENSOR_DTP92_RC_TIMEOUT)
		return "timeout";
	if (value == CD_SENSOR_DTP92_RC_SYNTAX_ERROR)
		return "syntax-error";
	if (value == CD_SENSOR_DTP92_RC_NO_DATA_AVAILABLE)
		return "no-data-available";
	if (value == CD_SENSOR_DTP92_RC_MISSING_PARAMETER)
		return "missing-parameter";
	if (value == CD_SENSOR_DTP92_RC_CALIBRATION_DENIED)
		return "calibration-denied";
	if (value == CD_SENSOR_DTP92_RC_NEEDS_OFFSET_CAL)
		return "needs-offset-cal";
	if (value == CD_SENSOR_DTP92_RC_NEEDS_RATIO_CAL)
		return "needs-ratio-cal";
	if (value == CD_SENSOR_DTP92_RC_NEEDS_LUMINANCE_CAL)
		return "needs-luminance-cal";
	if (value == CD_SENSOR_DTP92_RC_NEEDS_WHITE_POINT_CAL)
		return "needs-white-point-cal";
	if (value == CD_SENSOR_DTP92_RC_NEEDS_BLACK_POINT_CAL)
		return "needs-black-point-cal";
	if (value == CD_SENSOR_DTP92_RC_INVALID_READING)
		return "invalid-reading";
	if (value == CD_SENSOR_DTP92_RC_BAD_COMP_TABLE)
		return "bad-comp-table";
	if (value == CD_SENSOR_DTP92_RC_TOO_MUCH_LIGHT)
		return "too-much-light";
	if (value == CD_SENSOR_DTP92_RC_NOT_ENOUGH_LIGHT)
		return "not-enough-light";
	if (value == CD_SENSOR_DTP92_RC_BAD_SERIAL_NUMBER)
		return "bad-serial-number";
	if (value == CD_SENSOR_DTP92_RC_NO_MODULATION)
		return "no-modulation";
	if (value == CD_SENSOR_DTP92_RC_EEPROM_FAILURE)
		return "eeprom-failure";
	if (value == CD_SENSOR_DTP92_RC_FLASH_WRITE_FAILURE)
		return "flash-write-failure";
	if (value == CD_SENSOR_DTP92_RC_INST_INTERNAL_ERROR)
		return "inst-internal-error";
	return NULL;
}
