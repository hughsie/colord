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

#ifndef __CD_COMMON_H__
#define __CD_COMMON_H__

#include "config.h"

#include <gio/gio.h>

#include "../libcolord/cd-enum.h"

#define COLORD_DBUS_SERVICE		"org.freedesktop.ColorManager"
#define COLORD_DBUS_PATH		"/org/freedesktop/ColorManager"
#define COLORD_DBUS_INTERFACE		"org.freedesktop.ColorManager"
#define COLORD_DBUS_INTERFACE_DEVICE	"org.freedesktop.ColorManager.Device"
#define COLORD_DBUS_INTERFACE_PROFILE	"org.freedesktop.ColorManager.Profile"
#define COLORD_DBUS_INTERFACE_SENSOR	"org.freedesktop.ColorManager.Sensor"

#define CD_MAIN_ERROR			cd_main_error_quark()

typedef enum {
	CD_MAIN_ERROR_FAILED,
	CD_MAIN_ERROR_LAST
} CdMainError;

GQuark		 cd_main_error_quark		(void);
gboolean	 cd_main_sender_authenticated	(GDBusMethodInvocation *invocation,
						 const gchar	*sender,
						 const gchar	*action_id);
gchar		*cd_main_ensure_dbus_path	(const gchar	*object_path);
gboolean	 cd_main_mkdir_with_parents	(const gchar	*filename,
						 GError		**error);

#endif /* __CD_COMMON_H__ */

