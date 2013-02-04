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
 * SECTION:munki-enum
 * @short_description: Types used by munki and libmunki
 *
 * These helper functions provide a way to marshal enumerated values to
 * text and back again.
 *
 * See also: #CdClient, #CdDevice
 */

#include "config.h"

#include <glib.h>

#include "munki-enum.h"

/**
 * munki_command_value_to_string:
 **/
const gchar *
munki_command_value_to_string (guint8 value)
{
	if (value == MUNKI_COMMAND_DIAL_ROTATE)
		return "dial-rotate";
	if (value == MUNKI_COMMAND_BUTTON_PRESSED)
		return "button-released";
	if (value == MUNKI_COMMAND_BUTTON_RELEASED)
		return "button-released";
	return NULL;
}

/**
 * munki_button_state_to_string:
 **/
const gchar *
munki_button_state_to_string (guint8 value)
{
	if (value == MUNKI_BUTTON_STATE_RELEASED)
		return "released";
	if (value == MUNKI_BUTTON_STATE_PRESSED)
		return "pressed";
	return NULL;
}

/**
 * munki_dial_position_to_string:
 **/
const gchar *
munki_dial_position_to_string (guint8 value)
{
	if (value == MUNKI_DIAL_POSITION_PROJECTOR)
		return "projector";
	if (value == MUNKI_DIAL_POSITION_SURFACE)
		return "surface";
	if (value == MUNKI_DIAL_POSITION_CALIBRATION)
		return "calibration";
	if (value == MUNKI_DIAL_POSITION_AMBIENT)
		return "ambient";
	return NULL;
}

/**
 * munki_endpoint_to_string:
 **/
const gchar *
munki_endpoint_to_string (guint value)
{
	if (value == MUNKI_EP_CONTROL)
		return "control";
	if (value == MUNKI_EP_DATA)
		return "data";
	if (value == MUNKI_EP_EVENT)
		return "event";
	return NULL;
}
