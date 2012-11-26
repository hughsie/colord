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

#ifndef __CD_COMMON_H
#define __CD_COMMON_H

#include <glib.h>
#include <lcms2.h>

G_BEGIN_DECLS

#define CD_PROFILE_DEFAULT_COPYRIGHT_STRING	"This profile is free of known copyright restrictions."

gboolean	 cd_profile_write_metadata_string	(cmsHPROFILE	 lcms_profile,
							 const gchar	*metadata,
							 gboolean	 clear_existing,
							 const gchar	*binary_name,
							 GError		**error);

G_END_DECLS

#endif /* __CD_COMMON_H */

