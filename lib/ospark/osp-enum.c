/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
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

#include "osp-enum.h"

/**
 * osp_cmd_to_string:
 *
 * Since: 1.2.11
 **/
const gchar *
osp_cmd_to_string (OspCmd cmd)
{
	if (cmd == OSP_CMD_RESET)
		return "reset";
	if (cmd == OSP_CMD_RESET_TO_DEFAULTS)
		return "reset-to-defaults";
	if (cmd == OSP_CMD_GET_HARDWARE_VERSION)
		return "get-hardware-version";
	if (cmd == OSP_CMD_GET_FIRMWARE_VERSION)
		return "get-firmware-version";
	if (cmd == OSP_CMD_GET_SERIAL_NUMBER)
		return "get-serial-number";
	if (cmd == OSP_CMD_GET_SERIAL_NUMBER_LENGTH)
		return "get-serial-number-length";
	if (cmd == OSP_CMD_GET_DEVICE_ALIAS)
		return "get-device-alias";
	if (cmd == OSP_CMD_GET_DEVICE_ALIAS_LENGTH)
		return "get-device-alias-length";
	if (cmd == OSP_CMD_SET_DEVICE_ALIAS)
		return "set-device-alias";
	if (cmd == OSP_CMD_GET_NUMBER_OF_AVAILABLE_USER_STRINGS)
		return "get-number-of-available-user-strings";
	if (cmd == OSP_CMD_GET_USER_STRING_LENGTH)
		return "get-user-string-length";
	if (cmd == OSP_CMD_GET_USER_STRING)
		return "get-user-string";
	if (cmd == OSP_CMD_SET_USER_STRING)
		return "set-user-string";
	if (cmd == OSP_CMD_SET_LED)
		return "configures-status-led";
	if (cmd == OSP_CMD_PUT_DEVICE_IN_REPROGRAMMING_MODE)
		return "put-device-in-reprogramming-mode";
	if (cmd == OSP_CMD_GET_AND_SEND_CORRECTED_SPECTRUM)
		return "get-and-send-corrected-spectrum";
	if (cmd == OSP_CMD_GET_AND_SEND_RAW_SPECTRUM)
		return "get-and-send-raw-spectrum";
	if (cmd == OSP_CMD_GET_PARTIAL_SPECTRUM_MODE)
		return "get-partial-spectrum-mode";
	if (cmd == OSP_CMD_SET_PARTIAL_SPECTRUM_MODE)
		return "set-partial-spectrum-mode";
	if (cmd == OSP_CMD_GET_AND_SEND_PARTIAL_CORRECTED_SPECTRUM)
		return "get-and-send-partial-corrected-spectrum";
	if (cmd == OSP_CMD_SET_INTEGRATION_TIME)
		return "set-integration-time";
	if (cmd == OSP_CMD_GET_PIXEL_BINNING_FACTOR)
		return "get-pixel-binning-factor";
	if (cmd == OSP_CMD_GET_MAXIMUM_BINNING_FACTOR)
		return "get-maximum-binning-factor";
	if (cmd == OSP_CMD_GET_DEFAULT_BINNING_FACTOR)
		return "get-default-binning-factor";
	if (cmd == OSP_CMD_SET_PIXEL_BINNING_FACTOR)
		return "set-pixel-binning-factor";
	if (cmd == OSP_CMD_SET_DEFAULT_BINNING_FACTOR)
		return "set-default-binning-factor";
	if (cmd == OSP_CMD_SET_TRIGGER_DELAY_MS)
		return "set-trigger-delay-ms";
	if (cmd == OSP_CMD_GET_SCANS_TO_AVERAGE)
		return "get-scans-to-average";
	if (cmd == OSP_CMD_SET_SCANS_TO_AVERAGE)
		return "set-scans-to-average";
	if (cmd == OSP_CMD_GET_BOXCAR_WIDTH)
		return "get-boxcar-width";
	if (cmd == OSP_CMD_SET_BOXCAR_WIDTH)
		return "set-boxcar-width";
	if (cmd == OSP_CMD_GET_WAVELENGTH_COEFFICIENT_COUNT)
		return "get-wavelength-coefficient-count";
	if (cmd == OSP_CMD_GET_WAVELENGTH_COEFFICIENT)
		return "get-wavelength-coefficient";
	if (cmd == OSP_CMD_SET_WAVELENGTH_COEFFICIENT)
		return "set-wavelength-coefficient";
	if (cmd == OSP_CMD_GET_NONLINEARITY_COEFFICIENT_COUNT)
		return "get-nonlinearity-coefficient-count";
	if (cmd == OSP_CMD_GET_NONLINEARITY_COEFFICIENT)
		return "get-nonlinearity-coefficient";
	if (cmd == OSP_CMD_SET_NONLINEARITY_COEFFICIENT)
		return "set-nonlinearity-coefficient";
	if (cmd == OSP_CMD_GET_IRRADIANCE_CALIBRATION)
		return "get-irradiance-calibration";
	if (cmd == OSP_CMD_GET_IRRADIANCE_CALIBRATION_COUNT)
		return "get-irradiance-calibration-count";
	if (cmd == OSP_CMD_GET_IRRADIANCE_CALIBRATION_COLLECTION_AREA)
		return "get-irradiance-calibration-collection-area";
	if (cmd == OSP_CMD_SET_IRRADIANCE_CALIBRATION)
		return "set-irradiance-calibration";
	if (cmd == OSP_CMD_SET_IRRADIANCE_CALIBRATION_COLLECTION_AREA)
		return "set-irradiance-calibration-collection-area";
	if (cmd == OSP_CMD_GET_NUMBER_OF_STRAY_LIGHT_COEFFICIENTS)
		return "get-number-of-stray-light-coefficients";
	if (cmd == OSP_CMD_GET_STRAY_LIGHT_COEFFICIENT)
		return "get-stray-light-coefficient";
	if (cmd == OSP_CMD_SET_STRAY_LIGHT_COEFFICIENT)
		return "set-stray-light-coefficient";
	if (cmd == OSP_CMD_GET_HOT_PIXEL_INDICES)
		return "get-hot-pixel-indices";
	if (cmd == OSP_CMD_SET_HOT_PIXEL_INDICES)
		return "set-hot-pixel-indices";
	if (cmd == OSP_CMD_GET_BENCH_ID)
		return "get-bench-id";
	if (cmd == OSP_CMD_GET_BENCH_SERIAL_NUMBER)
		return "get-bench-serial-number";
	if (cmd == OSP_CMD_GET_SLIT_WIDTH_MICRONS)
		return "get-slit-width-microns";
	if (cmd == OSP_CMD_GET_FIBER_DIAMETER_MICRONS)
		return "get-fiber-diameter-microns";
	if (cmd == OSP_CMD_GET_GRATING)
		return "get-grating";
	if (cmd == OSP_CMD_GET_FILTER)
		return "get-filter";
	if (cmd == OSP_CMD_GET_COATING)
		return "get-coating";
	if (cmd == OSP_CMD_GET_GET_TEMPERATURE_SENSOR_COUNT)
		return "get-temperature-sensor-count";
	if (cmd == OSP_CMD_GET_READ_TEMPERATURE_SENSOR)
		return "read-temperature-sensor";
	if (cmd == OSP_CMD_GET_READ_ALL_TEMPERATURE_SENSORS)
		return "read-all-temperature-sensors";
	return NULL;
}

/**
 * osp_error_code_to_string:
 *
 * Since: 1.2.11
 **/
const gchar *
osp_error_code_to_string (OspErrorCode error_code)
{
	if (error_code == OSP_ERROR_CODE_SUCCESS)
		return "success";
	if (error_code == OSP_ERROR_CODE_UNSUPPORTED_PROTOCOL)
		return "unsupported-protocol";
	if (error_code == OSP_ERROR_CODE_UNKNOWN_MESSAGE_TYPE)
		return "unknown-message-type";
	if (error_code == OSP_ERROR_CODE_BAD_CHECKSUM)
		return "bad-checksum";
	if (error_code == OSP_ERROR_CODE_MESSAGE_TOO_LARGE)
		return "message-too-large";
	if (error_code == OSP_ERROR_CODE_PAYLOAD_LENGTH_INVALID)
		return "payload-length-invalid";
	if (error_code == OSP_ERROR_CODE_PAYLOAD_DATA_INVALID)
		return "payload-data-invalid";
	if (error_code == OSP_ERROR_CODE_DEVICE_NOT_READY)
		return "device-not-ready";
	if (error_code == OSP_ERROR_CODE_UNKNOWN_CHECKSUM_TYPE)
		return "unknown-checksum-type";
	if (error_code == OSP_ERROR_CODE_DEVICE_RESET)
		return "device-reset";
	if (error_code == OSP_ERROR_CODE_TOO_MANY_BUSES)
		return "too-many-busses";
	if (error_code == OSP_ERROR_CODE_OUT_OF_MEMORY)
		return "out-of-memory";
	if (error_code == OSP_ERROR_CODE_COMMAND_DATA_MISSING)
		return "command-data-missing";
	if (error_code == OSP_ERROR_CODE_INTERNAL_ERROR)
		return "internal-error";
	if (error_code == OSP_ERROR_CODE_COULD_NOT_DECRYPT)
		return "could-not-decrypt";
	if (error_code == OSP_ERROR_CODE_FIRMWARE_LAYOUT_INVALID)
		return "firmware-layout-invalid";
	if (error_code == OSP_ERROR_CODE_DATA_PACKET_INVALID_SIZE)
		return "packet-invalid-size";
	if (error_code == OSP_ERROR_CODE_HW_REVISION_INVALID)
		return "hardware-revision-invalid";
	if (error_code == OSP_ERROR_CODE_FLASH_MAP_INVALID)
		return "flash-map-invalid";
	if (error_code == OSP_ERROR_CODE_RESPONSE_DEFERRED)
		return "response-deferred";
	return NULL;
}
