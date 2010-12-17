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

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <locale.h>

static GMainLoop *loop = NULL;
static GDBusNodeInfo *introspection = NULL;
static GDBusConnection *connection = NULL;

#define COLORD_SERVICE		"org.freedesktop.ColorManager"

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	gboolean ret;
	guint retval = 1;
	guint owner_id = 0;
	GFile *file = NULL;
	gchar *introspection_data = NULL;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	if (! g_thread_supported ())
		g_thread_init (NULL);
	g_type_init ();

	/* TRANSLATORS: program name */
	g_set_application_name (_("Color Management"));
	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, _("Color Management D-Bus Service"));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* create new objects */
	loop = g_main_loop_new (NULL, FALSE);

	/* load introspection from file */
	file = g_file_new_for_path (DATADIR "/dbus-1/interfaces/org.freedesktop.ColorManager.xml");
	ret = g_file_load_contents (file, NULL, &introspection_data, NULL, NULL, &error);
	if (!ret) {
		g_warning ("failed to load introspection: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* build introspection from XML */
	introspection = g_dbus_node_info_new_for_xml (introspection_data, &error);
	if (introspection == NULL) {
		g_warning ("failed to load introspection: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* wait */
	g_main_loop_run (loop);

	/* success */
	retval = 0;
out:
	g_free (introspection_data);
	if (file != NULL)
		g_object_unref (file);
	if (owner_id > 0)
		g_bus_unown_name (owner_id);
	if (connection != NULL)
		g_object_unref (connection);
	g_dbus_node_info_unref (introspection);
	g_main_loop_unref (loop);
	return retval;
}

