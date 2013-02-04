/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#ifndef __CD_PLUGIN_H
#define __CD_PLUGIN_H

#include <glib-object.h>
#include <colord-private.h>

#include "cd-device.h"
#include "cd-config.h"

G_BEGIN_DECLS

typedef struct CdPlugin CdPlugin;
typedef struct CdPluginPrivate CdPluginPrivate;

typedef const gchar	*(*CdPluginGetDescFunc)		(void);
typedef void		 (*CdPluginFunc)		(CdPlugin	*plugin);
typedef void		 (*CdPluginDeviceFunc)		(CdPlugin	*plugin,
							 CdDevice	*device,
							 gpointer	 user_data);
typedef gboolean	 (*CdPluginConfigEnabledFunc)	(CdConfig	*config);

struct CdPlugin {
	GModule			*module;
	CdPluginPrivate		*priv;
	gpointer		 user_data;
	CdPluginDeviceFunc	 device_added;
	CdPluginDeviceFunc	 device_removed;
};

typedef enum {
	CD_PLUGIN_PHASE_INIT,				/* plugin started */
	CD_PLUGIN_PHASE_DESTROY,			/* plugin finalized */
	CD_PLUGIN_PHASE_COLDPLUG,			/* system ready for devices */
	CD_PLUGIN_PHASE_STATE_CHANGED,			/* system state has changed */
	CD_PLUGIN_PHASE_UNKNOWN
} CdPluginPhase;

#define	CD_PLUGIN_GET_PRIVATE(x)			g_new0 (x,1)

const gchar	*cd_plugin_get_description		(void);
void		 cd_plugin_initialize			(CdPlugin	*plugin);
void		 cd_plugin_coldplug			(CdPlugin	*plugin);
void		 cd_plugin_destroy			(CdPlugin	*plugin);
void		 cd_plugin_state_changed		(CdPlugin	*plugin);

void		 cd_plugin_device_added			(CdPlugin	*plugin,
							 CdDevice	*device);
void		 cd_plugin_device_removed		(CdPlugin	*plugin,
							 CdDevice	*device);

/* optional function which returns false if plugin should not be enabled */
gboolean	 cd_plugin_config_enabled		(CdConfig	*config);

G_END_DECLS

#endif /* __CD_PLUGIN_H */
