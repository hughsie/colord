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

#include <config.h>
#include <gio/gio.h>
#include <cd-plugin.h>

struct CdPluginPrivate {
	guint			 dummy;
};

/**
 * cd_plugin_get_description:
 */
const gchar *
cd_plugin_get_description (void)
{
	return "A dummy plugin that doesn't do anything";
}

/**
 * cd_plugin_initialize:
 */
void
cd_plugin_initialize (CdPlugin *plugin)
{
	/* create private area */
	plugin->priv = CD_PLUGIN_GET_PRIVATE (CdPluginPrivate);
	plugin->priv->dummy = 999;
}

/**
 * cd_plugin_destroy:
 */
void
cd_plugin_destroy (CdPlugin *plugin)
{
	plugin->priv->dummy = 0;
}
