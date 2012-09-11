/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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

#include "cd-plugin.h"

/**
 * cd_plugin_device_added:
 **/
void
cd_plugin_device_added (CdPlugin *plugin, CdDevice *device)
{
	g_return_if_fail (plugin != NULL);
	g_return_if_fail (plugin->device_added != NULL);
	g_return_if_fail (CD_IS_DEVICE (device));
	plugin->device_added (plugin, device, plugin->user_data);
}

/**
 * cd_plugin_device_removed:
 **/
void
cd_plugin_device_removed (CdPlugin *plugin, CdDevice *device)
{
	g_return_if_fail (plugin != NULL);
	g_return_if_fail (plugin->device_removed != NULL);
	g_return_if_fail (CD_IS_DEVICE (device));
	plugin->device_removed (plugin, device, plugin->user_data);
}
