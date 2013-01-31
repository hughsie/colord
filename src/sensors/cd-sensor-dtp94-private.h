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

#ifndef __CD_SENSOR_DTP94_PRIVATE_H
#define __CD_SENSOR_DTP94_PRIVATE_H

#include <glib.h>

G_BEGIN_DECLS

#define CD_SENSOR_DTP94_VENDOR_ID			0x0765
#define CD_SENSOR_DTP94_PRODUCT_ID			0xd094

/* return values from the commands */
#define CD_SENSOR_DTP92_RC_OK				0x00
#define CD_SENSOR_DTP92_RC_BAD_COMMAND			0x01
#define CD_SENSOR_DTP92_RC_PRM_RANGE			0x02
#define CD_SENSOR_DTP92_RC_MEMORY_OVERFLOW		0x04
#define CD_SENSOR_DTP92_RC_INVALID_BAUD_RATE		0x05
#define CD_SENSOR_DTP92_RC_TIMEOUT			0x07
#define CD_SENSOR_DTP92_RC_SYNTAX_ERROR			0x08
#define CD_SENSOR_DTP92_RC_NO_DATA_AVAILABLE		0x0b
#define CD_SENSOR_DTP92_RC_MISSING_PARAMETER		0x0c
#define CD_SENSOR_DTP92_RC_CALIBRATION_DENIED		0x0d
#define CD_SENSOR_DTP92_RC_NEEDS_OFFSET_CAL		0x16
#define CD_SENSOR_DTP92_RC_NEEDS_RATIO_CAL		0x17
#define CD_SENSOR_DTP92_RC_NEEDS_LUMINANCE_CAL		0x18
#define CD_SENSOR_DTP92_RC_NEEDS_WHITE_POINT_CAL	0x19
#define CD_SENSOR_DTP92_RC_NEEDS_BLACK_POINT_CAL	0x2a
#define CD_SENSOR_DTP92_RC_INVALID_READING		0x20
#define CD_SENSOR_DTP92_RC_BAD_COMP_TABLE		0x25
#define CD_SENSOR_DTP92_RC_TOO_MUCH_LIGHT		0x28
#define CD_SENSOR_DTP92_RC_NOT_ENOUGH_LIGHT		0x29
#define CD_SENSOR_DTP92_RC_BAD_SERIAL_NUMBER		0x40
#define CD_SENSOR_DTP92_RC_NO_MODULATION		0x50
#define CD_SENSOR_DTP92_RC_EEPROM_FAILURE		0x70
#define CD_SENSOR_DTP92_RC_FLASH_WRITE_FAILURE		0x71
#define CD_SENSOR_DTP92_RC_INST_INTERNAL_ERROR		0x7f
#define CD_SENSOR_DTP92_RC_UNKNOWN			0xff

guint8		 cd_sensor_dtp94_rc_parse	(const guint8	*data,
						 gsize		 length);
const gchar	*cd_sensor_dtp94_rc_to_string	(guint8		 value);

G_END_DECLS

#endif /* __CD_SENSOR_DTP94_PRIVATE_H */

