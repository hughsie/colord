/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2013 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <glib.h>

#include "ch-common.h"

/**
 * ch_strerror:
 *
 * Since: 0.1.29
 **/
const gchar *
ch_strerror (ChError error_enum)
{
	const char *str = NULL;
	switch (error_enum) {
	case CH_ERROR_NONE:
		str = "Success";
		break;
	case CH_ERROR_UNKNOWN_CMD:
		str = "Unknown command";
		break;
	case CH_ERROR_WRONG_UNLOCK_CODE:
		str = "Wrong unlock code";
		break;
	case CH_ERROR_NOT_IMPLEMENTED:
		str = "Not implemented";
		break;
	case CH_ERROR_UNDERFLOW_SENSOR:
		str = "Underflow of sensor";
		break;
	case CH_ERROR_NO_SERIAL:
		str = "No serial";
		break;
	case CH_ERROR_WATCHDOG:
		str = "Watchdog";
		break;
	case CH_ERROR_INVALID_ADDRESS:
		str = "Invalid address";
		break;
	case CH_ERROR_INVALID_LENGTH:
		str = "Invalid length";
		break;
	case CH_ERROR_INVALID_CHECKSUM:
		str = "Invalid checksum";
		break;
	case CH_ERROR_INVALID_VALUE:
		str = "Invalid value";
		break;
	case CH_ERROR_UNKNOWN_CMD_FOR_BOOTLOADER:
		str = "Unknown command for bootloader";
		break;
	case CH_ERROR_OVERFLOW_MULTIPLY:
		str = "Overflow of multiply";
		break;
	case CH_ERROR_OVERFLOW_ADDITION:
		str = "Overflow of addition";
		break;
	case CH_ERROR_OVERFLOW_SENSOR:
		str = "Overflow of sensor";
		break;
	case CH_ERROR_OVERFLOW_STACK:
		str = "Overflow of stack";
		break;
	case CH_ERROR_NO_CALIBRATION:
		str = "No calibration";
		break;
	case CH_ERROR_DEVICE_DEACTIVATED:
		str = "Device deactivated";
		break;
	case CH_ERROR_INCOMPLETE_REQUEST:
		str = "Incomplete previous request";
		break;
	case CH_ERROR_SELF_TEST_SENSOR:
		str = "Self test failed: Sensor";
		break;
	case CH_ERROR_SELF_TEST_RED:
		str = "Self test failed: Red";
		break;
	case CH_ERROR_SELF_TEST_GREEN:
		str = "Self test failed: Green";
		break;
	case CH_ERROR_SELF_TEST_BLUE:
		str = "Self test failed: Blue";
		break;
	case CH_ERROR_SELF_TEST_MULTIPLIER:
		str = "Self test failed: Multiplier";
		break;
	case CH_ERROR_SELF_TEST_COLOR_SELECT:
		str = "Self test failed: Color Select";
		break;
	case CH_ERROR_SELF_TEST_TEMPERATURE:
		str = "Self test failed: Temperature";
		break;
	case CH_ERROR_INVALID_CALIBRATION:
		str = "Invalid calibration";
		break;
	case CH_ERROR_SRAM_FAILED:
		str = "SRAM failed";
		break;
	case CH_ERROR_OUT_OF_MEMORY:
		str = "Out of memory";
		break;
	case CH_ERROR_SELF_TEST_I2C:
		str = "Self test failed: I2C";
		break;
	case CH_ERROR_SELF_TEST_ADC_VDD:
		str = "Self test failed: ADC Vdd";
		break;
	case CH_ERROR_SELF_TEST_ADC_VSS:
		str = "Self test failed: ADC Vss";
		break;
	case CH_ERROR_SELF_TEST_ADC_VREF:
		str = "Self test failed: ADC Vref";
		break;
	default:
		str = "Unknown error, please report";
		break;
	}
	return str;
}

/**
 * ch_color_select_to_string:
 *
 * Since: 0.1.29
 **/
const gchar *
ch_color_select_to_string (ChColorSelect color_select)
{
	const char *str = NULL;
	switch (color_select) {
	case CH_COLOR_SELECT_BLUE:
		str = "Blue";
		break;
	case CH_COLOR_SELECT_RED:
		str = "Red";
		break;
	case CH_COLOR_SELECT_GREEN:
		str = "Green";
		break;
	case CH_COLOR_SELECT_WHITE:
		str = "White";
		break;
	default:
		str = "Unknown";
		break;
	}
	return str;
}

/**
 * ch_multiplier_to_string:
 *
 * Since: 0.1.29
 **/
const gchar *
ch_multiplier_to_string (ChFreqScale multiplier)
{
	const char *str = NULL;
	switch (multiplier) {
	case CH_FREQ_SCALE_0:
		str = "0%";
		break;
	case CH_FREQ_SCALE_2:
		str = "2%";
		break;
	case CH_FREQ_SCALE_20:
		str = "20%";
		break;
	case CH_FREQ_SCALE_100:
		str = "100%";
		break;
	default:
		str = "Unknown%";
		break;
	}
	return str;
}

/**
 * ch_command_to_string:
 *
 * Since: 0.1.29
 **/
const gchar *
ch_command_to_string (guint8 cmd)
{
	const char *str = NULL;
	switch (cmd) {
	case CH_CMD_GET_COLOR_SELECT:
		str = "get-color-select";
		break;
	case CH_CMD_SET_COLOR_SELECT:
		str = "set-color-select";
		break;
	case CH_CMD_GET_MULTIPLIER:
		str = "get-multiplier";
		break;
	case CH_CMD_SET_MULTIPLIER:
		str = "set-multiplier";
		break;
	case CH_CMD_GET_INTEGRAL_TIME:
		str = "get-integral-time";
		break;
	case CH_CMD_SET_INTEGRAL_TIME:
		str = "set-integral-time";
		break;
	case CH_CMD_GET_FIRMWARE_VERSION:
		str = "get-firmare-version";
		break;
	case CH_CMD_GET_CALIBRATION:
		str = "get-calibration";
		break;
	case CH_CMD_SET_CALIBRATION:
		str = "set-calibration";
		break;
	case CH_CMD_GET_SERIAL_NUMBER:
		str = "get-serial-number";
		break;
	case CH_CMD_SET_SERIAL_NUMBER:
		str = "set-serial-number";
		break;
	case CH_CMD_GET_OWNER_NAME:
		str = "get-owner-name";
		break;
	case CH_CMD_SET_OWNER_NAME:
		str = "set-owner-name";
		break;
	case CH_CMD_GET_OWNER_EMAIL:
		str = "get-owner-name";
		break;
	case CH_CMD_SET_OWNER_EMAIL:
		str = "set-owner-email";
		break;
	case CH_CMD_GET_LEDS:
		str = "get-leds";
		break;
	case CH_CMD_SET_LEDS:
		str = "set-leds";
		break;
	case CH_CMD_GET_PCB_ERRATA:
		str = "get-pcb-errata";
		break;
	case CH_CMD_SET_PCB_ERRATA:
		str = "set-pcb-errata";
		break;
	case CH_CMD_GET_DARK_OFFSETS:
		str = "get-dark-offsets";
		break;
	case CH_CMD_SET_DARK_OFFSETS:
		str = "set-dark-offsets";
		break;
	case CH_CMD_WRITE_EEPROM:
		str = "write-eeprom";
		break;
	case CH_CMD_TAKE_READING_RAW:
		str = "take-reading-raw";
		break;
	case CH_CMD_TAKE_READINGS:
		str = "take-readings";
		break;
	case CH_CMD_TAKE_READING_XYZ:
		str = "take-reading-xyz";
		break;
	case CH_CMD_RESET:
		str = "reset";
		break;
	case CH_CMD_READ_FLASH:
		str = "read-flash";
		break;
	case CH_CMD_ERASE_FLASH:
		str = "erase-flash";
		break;
	case CH_CMD_WRITE_FLASH:
		str = "write-flash";
		break;
	case CH_CMD_BOOT_FLASH:
		str = "boot-flash";
		break;
	case CH_CMD_SET_FLASH_SUCCESS:
		str = "set-flash-success";
		break;
	case CH_CMD_GET_CALIBRATION_MAP:
		str = "get-calibration-map";
		break;
	case CH_CMD_SET_CALIBRATION_MAP:
		str = "set-calibration-map";
		break;
	case CH_CMD_GET_HARDWARE_VERSION:
		str = "get-hardware-version";
		break;
	case CH_CMD_GET_REMOTE_HASH:
		str = "get-remote-hash";
		break;
	case CH_CMD_SET_REMOTE_HASH:
		str = "set-remote-hash";
		break;
	case CH_CMD_SELF_TEST:
		str = "self-test";
		break;
	case CH_CMD_WRITE_SRAM:
		str = "write-sram";
		break;
	case CH_CMD_READ_SRAM:
		str = "read-sram";
		break;
	case CH_CMD_GET_MEASURE_MODE:
		str = "get-measure-mode";
		break;
	case CH_CMD_SET_MEASURE_MODE:
		str = "set-measure-mode";
		break;
	case CH_CMD_GET_TEMPERATURE:
		str = "get-temperature";
		break;
	default:
		str = "unknown-command";
		break;
	}
	return str;
}

/**
 * ch_measure_mode_to_string:
 *
 * Since: 0.1.29
 **/
const gchar *
ch_measure_mode_to_string (ChMeasureMode measure_mode)
{
	const char *str = NULL;
	switch (measure_mode) {
	case CH_MEASURE_MODE_FREQUENCY:
		str = "frequency";
		break;
	case CH_MEASURE_MODE_DURATION:
		str = "duration";
		break;
	default:
		str = "unknown";
		break;
	}
	return str;
}

/**
 * ch_device_mode_to_string:
 *
 * Since: 0.1.29
 **/
const gchar *
ch_device_mode_to_string (ChDeviceMode device_mode)
{
	const char *str = NULL;
	switch (device_mode) {
	case CH_DEVICE_MODE_LEGACY:
		str = "legacy";
		break;
	case CH_DEVICE_MODE_BOOTLOADER:
		str = "bootloader";
		break;
	case CH_DEVICE_MODE_BOOTLOADER_PLUS:
		str = "bootloader-plus";
		break;
	case CH_DEVICE_MODE_FIRMWARE:
		str = "firmware";
		break;
	case CH_DEVICE_MODE_FIRMWARE_PLUS:
		str = "firmware-plus";
		break;
	default:
		str = "unknown";
		break;
	}
	return str;
}
