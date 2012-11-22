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

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>
#include <syslog.h>

#include "cd-config.h"

static void     cd_config_finalize	(GObject     *object);

#define CD_CONFIG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_CONFIG, CdConfigPrivate))

/**
 * CdConfigPrivate:
 *
 * Private #CdConfig data
 **/
struct _CdConfigPrivate
{
	GKeyFile			*keyfile;
};

G_DEFINE_TYPE (CdConfig, cd_config, G_TYPE_OBJECT)

/**
 * cd_config_get_boolean:
 **/
gboolean
cd_config_get_boolean (CdConfig *config, const gchar *key)
{
	return g_key_file_get_boolean (config->priv->keyfile,
				       "colord", key, NULL);
}

/**
 * cd_config_get_string:
 **/
gchar *
cd_config_get_string (CdConfig *config, const gchar *key)
{
	return g_key_file_get_string (config->priv->keyfile,
				      "colord", key, NULL);
}

/**
 * cd_config_get_strv:
 **/
gchar **
cd_config_get_strv (CdConfig *config, const gchar *key)
{
	return g_key_file_get_string_list (config->priv->keyfile,
					   "colord", key, NULL, NULL);
}

/**
 * cd_config_class_init:
 **/
static void
cd_config_class_init (CdConfigClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_config_finalize;
	g_type_class_add_private (klass, sizeof (CdConfigPrivate));
}

/**
 * cd_config_init:
 **/
static void
cd_config_init (CdConfig *config)
{
	gboolean ret;
	GError *error = NULL;

	config->priv = CD_CONFIG_GET_PRIVATE (config);
	config->priv->keyfile = g_key_file_new ();

	/* load */
	syslog (LOG_INFO, "Using config file %s", SYSCONFDIR "/colord.conf");
	ret = g_key_file_load_from_file (config->priv->keyfile,
					 SYSCONFDIR "/colord.conf",
					 G_KEY_FILE_NONE,
					 &error);
	if (!ret) {
		g_warning ("failed to load config file: %s",
			   error->message);
		g_error_free (error);
	}
}

/**
 * cd_config_finalize:
 **/
static void
cd_config_finalize (GObject *object)
{
	CdConfig *config = CD_CONFIG (object);
	CdConfigPrivate *priv = config->priv;

	g_key_file_free (priv->keyfile);

	G_OBJECT_CLASS (cd_config_parent_class)->finalize (object);
}

/**
 * cd_config_new:
 **/
CdConfig *
cd_config_new (void)
{
	CdConfig *config;
	config = g_object_new (CD_TYPE_CONFIG, NULL);
	return CD_CONFIG (config);
}

