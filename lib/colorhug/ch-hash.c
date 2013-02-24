/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2013 Richard Hughes <richard@hughsie.com>
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

#include <glib.h>
#include <string.h>

#include "ch-hash.h"

/**
 * ch_sha1_to_string:
 * @sha1: A %ChSha1
 *
 * Gets a string representation of the SHA1 hash.
 *
 * Return value: A string, free with g_free().
 *
 * Since: 0.1.29
 **/
gchar *
ch_sha1_to_string (const ChSha1 *sha1)
{
	GString *string = NULL;
	guint i;

	g_return_val_if_fail (sha1 != NULL, NULL);

	/* read each byte and convert to hex */
	string = g_string_new ("");
	for (i = 0; i < 20; i++) {
		g_string_append_printf (string, "%02x",
					sha1->bytes[i]);
	}
	return g_string_free (string, FALSE);
}

/**
 * ch_sha1_parse:
 * @value: A string representation of the SHA1 hash
 * @sha1: A %ChSha1
 * @error: A %GError, or %NULL
 *
 * Parses a SHA1 hash from a string value.
 *
 * Return value: %TRUE for success
 *
 * Since: 0.1.29
 **/
gboolean
ch_sha1_parse (const gchar *value, ChSha1 *sha1, GError **error)
{
	gboolean ret = TRUE;
	gchar tmp[3] = { '\0', '\0', '\0'};
	guint i;
	guint len;

	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (sha1 != NULL, FALSE);

	/* not a SHA1 hash */
	len = strlen (value);
	if (len != 40) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Invalid SHA1 hash '%s'",
			     value);
		goto out;
	}

	/* parse */
	for (i = 0; i < len; i += 2) {
		tmp[0] = value[i+0];
		tmp[1] = value[i+1];
		sha1->bytes[i/2] = g_ascii_strtoull (tmp, NULL, 16);
	}
out:
	return ret;
}
