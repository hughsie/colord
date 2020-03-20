/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2013 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "config.h"

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

#include <limits.h>
#include <stdlib.h>

#include <glib.h>

#include "cd-test-shared.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

gchar *
cd_test_get_filename (const gchar *filename)
{
	gchar *tmp;
	char full_tmp[PATH_MAX];
	const gchar *testdatadir;

	/* running in the installed system */
	if (g_getenv ("INSTALLED_TESTS") != NULL) {
		return g_build_filename (LIBEXECDIR,
					 "installed-tests",
					 PACKAGE_NAME,
					 filename,
					 NULL);
	}
	/* self tests are running */
	testdatadir = g_getenv ("TESTDATADIR");
	if (testdatadir != NULL) {
		g_autofree gchar *path = NULL;
		path = g_build_filename (testdatadir, filename, NULL);
		tmp = realpath (path, full_tmp);
		if (tmp != NULL)
			return g_strdup (full_tmp);
	}
	return NULL;
}

static GMainLoop *_test_loop = NULL;
static guint _test_loop_timeout_id = 0;

static gboolean
cd_test_hang_check_cb (gpointer user_data)
{
	g_main_loop_quit (_test_loop);
	_test_loop_timeout_id = 0;
	return G_SOURCE_REMOVE;
}

void
cd_test_loop_run_with_timeout (guint timeout_ms)
{
	g_assert (_test_loop_timeout_id == 0);
	g_assert (_test_loop == NULL);
	_test_loop = g_main_loop_new (NULL, FALSE);
	_test_loop_timeout_id = g_timeout_add (timeout_ms, cd_test_hang_check_cb, NULL);
	g_main_loop_run (_test_loop);
}

void
cd_test_loop_quit (void)
{
	if (_test_loop_timeout_id > 0) {
		g_source_remove (_test_loop_timeout_id);
		_test_loop_timeout_id = 0;
	}
	if (_test_loop != NULL) {
		g_main_loop_quit (_test_loop);
		g_main_loop_unref (_test_loop);
		_test_loop = NULL;
	}
}

gboolean
cd_test_compare_lines (const gchar *txt1, const gchar *txt2, GError **error)
{
	g_autofree gchar *output = NULL;

	/* exactly the same */
	if (g_strcmp0 (txt1, txt2) == 0)
		return TRUE;

	/* save temp files and diff them */
	if (!g_file_set_contents ("/tmp/a", txt1, -1, error))
		return FALSE;
	if (!g_file_set_contents ("/tmp/b", txt2, -1, error))
		return FALSE;
	if (!g_spawn_command_line_sync ("diff -urNp /tmp/b /tmp/a",
					&output, NULL, NULL, error))
		return FALSE;

	/* just output the diff */
	g_set_error_literal (error, 1, 0, output);
	return FALSE;
}
