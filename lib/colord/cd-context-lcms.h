/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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


#if !defined (CD_COMPILATION)
#error "You cannot include this file externaly"
#endif

#ifndef __CD_CONTEXT_PRIVATE_H
#define __CD_CONTEXT_PRIVATE_H

#include <glib.h>

gpointer	 cd_context_lcms_new		(void);
void		 cd_context_lcms_free		(gpointer	 ctx);
void		 cd_context_lcms_error_clear	(gpointer	 ctx);
gboolean	 cd_context_lcms_error_check	(gpointer	 ctx,
						 GError		**error);

/* not part of the API */
void		_cd_context_lcms_pre26_start	(void);
void		_cd_context_lcms_pre26_stop	(void);

G_END_DECLS

#endif /* __CD_CONTEXT_PRIVATE_H */

