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

#ifndef __CD_INHIBIT_H
#define __CD_INHIBIT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CD_TYPE_INHIBIT (cd_inhibit_get_type ())
G_DECLARE_DERIVABLE_TYPE (CdInhibit, cd_inhibit, CD, INHIBIT, GObject)

struct _CdInhibitClass
{
	GObjectClass		 parent_class;
	void			(*changed)	(CdInhibit	*inhibit);
};

GType		 cd_inhibit_get_type		(void);
CdInhibit	*cd_inhibit_new			(void);

gboolean	 cd_inhibit_add			(CdInhibit	*inhibit,
						 const gchar	*sender,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 cd_inhibit_remove		(CdInhibit	*inhibit,
						 const gchar	*sender,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 cd_inhibit_valid		(CdInhibit	*inhibit);
gchar		**cd_inhibit_get_bus_names	(CdInhibit	*inhibit);

G_END_DECLS

#endif /* __CD_INHIBIT_H */

