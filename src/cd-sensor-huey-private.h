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

#ifndef __CD_SENSOR_HUEY_PRIVATE_H
#define __CD_SENSOR_HUEY_PRIVATE_H

#include <glib.h>

G_BEGIN_DECLS


/* All hueys have this VID/PID */
#define CD_SENSOR_HUEY_VENDOR_ID				0x0971
#define CD_SENSOR_HUEY_PRODUCT_ID				0x2005

/* Return values from the commands */
#define CD_SENSOR_HUEY_RETURN_SUCCESS				0x00
#define CD_SENSOR_HUEY_RETURN_LOCKED				0xc0
#define CD_SENSOR_HUEY_RETURN_UNKNOWN_5A			0x5a /* seen in profiling */
#define CD_SENSOR_HUEY_RETURN_ERROR				0x80
#define CD_SENSOR_HUEY_RETURN_UNKNOWN_81			0x81 /* seen once in init */
#define CD_SENSOR_HUEY_RETURN_RETRY				0x90

/*
 * Get the currect status of the device
 *
 *  input:   00 00 00 00 3f 00 00 00
 * returns: 00 00 43 69 72 30 30 31  (or)
 *     "Cir001" --^^^^^^^^^^^^^^^^^ -- Circuit1?...
 *          c0 00 4c 6f 63 6b 65 64
 *     "locked" --^^^^^^^^^^^^^^^^^
 */
#define CD_SENSOR_HUEY_COMMAND_GET_STATUS			0x00

/*
 * Read the green sample data
 *
 * input:   02 xx xx xx xx xx xx xx
 * returns: 00 02 00 00 0a 00 00 00 (or)
 *          00 02 00 0e c6 80 00 00
 *            data --^^^^^ ^-- only ever 00 or 80
 *                    |
 *                    \-- for RGB(00,00,00) is 09 f2
 *                            RGB(ff,ff,ff) is 00 00
 *                            RGB(ff,00,00) is 02 a5
 *                            RGB(00,ff,00) is 00 f1
 *                            RGB(00,00,ff) is 08 56
 *
 * This doesn't do a sensor read, it seems to be a simple accessor.
 * CD_SENSOR_HUEY_COMMAND_SENSOR_MEASURE_RGB has to be used before this one.
 */
#define CD_SENSOR_HUEY_COMMAND_READ_GREEN			0x02

/*
 * Read the blue sample data
 *
 * input:   03 xx xx xx xx xx xx xx
 * returns: 00 03 00 0f 18 00 00 00
 *            data --^^^^^ ^-- only ever 00 or 80
 *                    |
 *                    \-- for RGB(00,00,00) is 09 64
 *                            RGB(ff,ff,ff) is 08 80
 *                            RGB(ff,00,00) is 03 22
 *                            RGB(00,ff,00) is 00 58
 *                            RGB(00,00,ff) is 00 59
 *
 * This doesn't do a sensor read, it seems to be a simple accessor.
 * CD_SENSOR_HUEY_COMMAND_SENSOR_MEASURE_RGB has to be used before this one.
 */
#define CD_SENSOR_HUEY_COMMAND_READ_BLUE			0x03

/*
 * Set value of some 32 bit register.
 *
 * input:   05 ?? 11 12 13 14 xx xx
 * returns: 00 05 00 00 00 00 00 00
 *              ^--- always the same no matter the input
 *
 * This is never used in profiling
 */
#define CD_SENSOR_HUEY_COMMAND_SET_VALUE			0x05

/*
 * Get the value of some 32 bit register.
 *
 * input:   06 xx xx xx xx xx xx xx
 * returns: 00 06 11 12 13 14 00 00
 *    4 bytes ----^^^^^^^^^^^ (from CD_SENSOR_HUEY_COMMAND_SET_VALUE)
 *
 * This is some sort of 32bit register on the device.
 * The default value at plug-in is 00 0f 42 40, although during
 * profiling it is set to 00 00 6f 00 and then 00 00 61 00.
 */
#define CD_SENSOR_HUEY_COMMAND_GET_VALUE			0x06

/* NEVER USED */
#define CD_SENSOR_HUEY_COMMAND_UNKNOWN_07			0x07

/*
 * Reads a register value.
 *
 * (sent at startup  after the unlock)
 * input:   08 0b xx xx xx xx xx xx
 *             ^^-- register address
 * returns: 00 08 0b b8 00 00 00 00
 *      address --^^ ^^-- value
 *
 * It appears you can only ask for one byte at a time.
 */
#define CD_SENSOR_HUEY_COMMAND_REGISTER_READ			0x08

/*
 * Unlock a locked sensor.
 *
 * input:   0e 47 72 4d 62 6b 65 64
 *  "GrMbked"--^^^^^^^^^^^^^^^^^^^^
 * returns: 00 0e 00 00 00 00 00 00
 *
 * It might be only GrMbk that is needed to unlock.
 * We still don't know how to 'lock' a device, it just kinda happens.
 */
#define CD_SENSOR_HUEY_COMMAND_UNLOCK				0x0e

/*
 * Unknown command
 *
 * returns: all NULL all of the time */
#define CD_SENSOR_HUEY_COMMAND_UNKNOWN_0F			0x0f

/*
 * Unknown command
 *
 * Something to do with sampling */
#define CD_SENSOR_HUEY_COMMAND_UNKNOWN_10			0x10

/*
 * Unknown command
 *
 * Something to do with sampling (that needs a retry with code 5a)
 */
#define CD_SENSOR_HUEY_COMMAND_UNKNOWN_11			0x11

/*
 * Unknown command
 *
 * something to do with sampling
 */
#define CD_SENSOR_HUEY_COMMAND_UNKNOWN_12			0x12

/*
 * Measures RGB value, and return the red value (only used in CRT mode).
 *
 * Seems to have to retry, every single time.
 *
 *                   Gain?
 *              _______|_______
 *             /---\ /---\ /---\
 * input:   13 02 41 00 54 00 49 00
 * returns: 00 13 00 00 01 99 02 00
 *                   ^^^^^ - would match CD_SENSOR_HUEY_COMMAND_SENSOR_MEASURE_RGB
 *
 * The gain seems not to change for different measurements with different
 * colors. This seems to be a less precise profile too.
 */
#define CD_SENSOR_HUEY_COMMAND_SENSOR_MEASURE_RGB_CRT		0x13

/*
 * Unknown command
 *
 * returns: seems to be sent, but not requested
 */
#define CD_SENSOR_HUEY_COMMAND_UNKNOWN_15			0x15

/*
 * Sample a color and return the red component
 *
 * input:   16 00 01 00 01 00 01 00
 * returns: 00 16 00 00 00 00 00 00
 *
 * or:
 *             ,,-,,-,,-,,-,,-,,-- 'gain control'
 *             || || || || || ||
 * input:   16 00 35 00 48 00 1d 03
 * returns: 00 16 00 0b d0 00 00 00
 *            data --^^^^^ ^^-- only ever 00 or 80
 *
 * This is used when profiling, and all commands are followed by
 * CD_SENSOR_HUEY_COMMAND_READ_GREEN and HUEY_COMMAND_READ_BLUE.
 * 
 * The returned values are some kind of 16 bit register count that
 * indicate how much light fell on a sensor. If the sensors are
 * converting light to pulses, then the 'gain' control tells the sensor
 * how long to read. It's therefore quicker to read white than black.
 *
 * Given there exists only GREEN and BLUE accessors, and that RED comes
 * first in a RGB sequence, I think it's safe to assume that this command
 * does the measurement, and the others just return cached data.
 *
 * argyll does (for #ff0000)
 *
 * -> 16 00 01 00 01 00 01 00
 * <-       00 00 0b 00 00 00
 * -> 02 xx xx xx xx xx xx xx
 * <-       00 00 12 00 00 00
 * -> 03 xx xx xx xx xx xx xx
 * <-       00 03 41 00 00 00
 *
 * then does:
 *
 * -> 16 01 63 00 d9 00 04 00
 * <-       00 0f ce 80 00 00
 * -> 02 xx xx xx xx xx xx xx
 * <-       00 0e d0 80 00 00
 * -> 03 xx xx xx xx xx xx xx
 * <-       00 0d 3c 00 00 00
 *
 * then returns XYZ=87.239169 45.548708 1.952249
 */
#define CD_SENSOR_HUEY_COMMAND_SENSOR_MEASURE_RGB		0x16

/*
 * Unknown command (some sort of poll?)
 *
 * input:   21 09 00 02 00 00 08 00 (or)
 * returns: [never seems to return a value]
 *
 * Only when profiling, and over and over.
 */
#define CD_SENSOR_HUEY_COMMAND_UNKNOWN_21			0x21

/*
 * Get the level of ambient light from the sensor
 *
 *                 ,,--- The output-type, where 00 is LCD and 02 is CRT
 *  input:   17 03 00 xx xx xx xx xx
 * returns: 90 17 03 00 00 00 00 00  then on second read:
 * 	    00 17 03 00 00 62 57 00 in light (or)
 * 	    00 17 03 00 00 00 08 00 in dark
 * 	no idea	--^^       ^---^ = 16bits data
 */
#define CD_SENSOR_HUEY_COMMAND_GET_AMBIENT			0x17

/*
 * Set the LEDs on the sensor
 *
 * input:   18 00 f0 xx xx xx xx xx
 * returns: 00 18 f0 00 00 00 00 00
 *   led mask ----^^
 */
#define CD_SENSOR_HUEY_COMMAND_SET_LEDS				0x18

/*
 * Unknown command
 *
 * returns: all NULL for NULL input: times out for f1 f2 f3 f4 f5 f6 f7 f8 */
#define CD_SENSOR_HUEY_COMMAND_UNKNOWN_19			0x19

/*
 * Register map:
 *     x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xA  xB  xC  xD  xE  xF
 * 0x [serial-number.][matrix-lcd....................................|
 * 1x ...............................................................|
 * 2x .......]                                                       |
 * 3x         [calib-lcd-time][matrix-crt............................|
 * 4x ...............................................................|
 * 5x .......................................][calib-crt-time]       |
 * 6x                             [calib_vector......................|
 * 7x ...........]                            [unlock-string.....]   |
 * 8x                                                                |
 * 9x                 [calib_value...]                               |
 */
#define CD_SENSOR_HUEY_EEPROM_ADDR_SERIAL			0x00 /* 4 bytes */
#define CD_SENSOR_HUEY_EEPROM_ADDR_CALIBRATION_DATA_LCD		0x04 /* 36 bytes */
#define CD_SENSOR_HUEY_EEPROM_ADDR_CALIBRATION_TIME_LCD		0x32 /* 4 bytes */
#define CD_SENSOR_HUEY_EEPROM_ADDR_CALIBRATION_DATA_CRT		0x36 /* 36 bytes */
#define CD_SENSOR_HUEY_EEPROM_ADDR_CALIBRATION_TIME_CRT		0x5a /* 4 bytes */
#define CD_SENSOR_HUEY_EEPROM_ADDR_DARK_OFFSET			0x67 /* 12 bytes */
#define CD_SENSOR_HUEY_EEPROM_ADDR_UNLOCK			0x7a /* 5 bytes */
#define CD_SENSOR_HUEY_EEPROM_ADDR_AMBIENT_CALIB_VALUE		0x94 /* 4 bytes */

const gchar	*cd_sensor_huey_return_code_to_string	(guchar	 value);
const gchar	*cd_sensor_huey_command_code_to_string	(guchar	 value);

G_END_DECLS

#endif /* __CD_SENSOR_HUEY_PRIVATE_H */

