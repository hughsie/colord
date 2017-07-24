/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2012 Richard Hughes <richard@hughsie.com>
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

#ifndef __CD_SPAWN_H
#define __CD_SPAWN_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CD_TYPE_SPAWN		(cd_spawn_get_type ())
#define CD_SPAWN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_SPAWN, CdSpawn))
#define CD_SPAWN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_SPAWN, CdSpawnClass))
#define CD_IS_SPAWN(o)	 	(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_SPAWN))
#define CD_IS_SPAWN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_SPAWN))
#define CD_SPAWN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_SPAWN, CdSpawnClass))
#define CD_SPAWN_ERROR		(cd_spawn_error_quark ())
#define CD_SPAWN_TYPE_ERROR	(cd_spawn_error_get_type ())

typedef struct CdSpawnPrivate CdSpawnPrivate;

typedef struct
{
	 GObject		 parent;
	 CdSpawnPrivate		*priv;
} CdSpawn;

typedef struct
{
	GObjectClass	parent_class;
} CdSpawnClass;

/**
 * CdSpawnExitType:
 *
 * How the spawned file exited
 **/
typedef enum {
	CD_SPAWN_EXIT_TYPE_SUCCESS,		/* script run, without any problems */
	CD_SPAWN_EXIT_TYPE_FAILED,		/* script failed to run */
	CD_SPAWN_EXIT_TYPE_SIGQUIT,		/* we killed the instance (SIGQUIT) */
	CD_SPAWN_EXIT_TYPE_SIGKILL,		/* we killed the instance (SIGKILL) */
	CD_SPAWN_EXIT_TYPE_UNKNOWN
} CdSpawnExitType;

GType		 cd_spawn_get_type		  	(void);
CdSpawn		*cd_spawn_new				(void);

gboolean	 cd_spawn_argv				(CdSpawn	*spawn,
							 gchar		**argv,
							 gchar		**envp,
							 GError		**error)
							 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 cd_spawn_is_running			(CdSpawn	*spawn);
gboolean	 cd_spawn_kill				(CdSpawn	*spawn);
gboolean	 cd_spawn_send_stdin			(CdSpawn	*spawn,
							 const gchar	*command);

G_END_DECLS

#endif /* __CD_SPAWN_H */
