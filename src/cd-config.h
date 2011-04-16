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

#ifndef __CD_CONFIG_H
#define __CD_CONFIG_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CD_TYPE_CONFIG		(cd_config_get_type ())
#define CD_CONFIG(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_CONFIG, CdConfig))
#define CD_CONFIG_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_CONFIG, CdConfigClass))
#define CD_IS_CONFIG(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_CONFIG))

typedef struct _CdConfigPrivate	CdConfigPrivate;
typedef struct _CdConfig		CdConfig;
typedef struct _CdConfigClass		CdConfigClass;

struct _CdConfig
{
	 GObject		 parent;
	 CdConfigPrivate	*priv;
};

struct _CdConfigClass
{
	GObjectClass		 parent_class;
};

GType		 cd_config_get_type		(void);
CdConfig	*cd_config_new			(void);
gboolean	 cd_config_get_boolean		(CdConfig	*config,
						 const gchar	*key);
gchar		*cd_config_get_string		(CdConfig	*config,
						 const gchar	*key);
gchar		**cd_config_get_strv		(CdConfig	*config,
						 const gchar	*key);

G_END_DECLS

#endif /* __CD_CONFIG_H */

