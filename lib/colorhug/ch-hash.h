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

#if !defined (__COLORHUG_H_INSIDE__) && !defined (CH_COMPILATION)
#error "Only <colorhug.h> can be included directly."
#endif

#ifndef __CH_HASH_H
#define __CH_HASH_H

#include <glib.h>

G_BEGIN_DECLS

/* SHA1 hash */
typedef struct {
	guint8	bytes[20];
} ChSha1;

gchar		*ch_sha1_to_string		(const ChSha1		*sha1);
gboolean	 ch_sha1_parse			(const gchar		*value,
						 ChSha1			*sha1,
						 GError			**error);

G_END_DECLS

#endif /* __CH_HASH_H */
