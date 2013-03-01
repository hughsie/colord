/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Richard Hughes <richard@hughsie.com>
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

#if !defined (__COLORD_H_INSIDE__) && !defined (CD_COMPILATION)
#error "Only <colord.h> can be included directly."
#endif

#ifndef __CD_INTERP_H
#define __CD_INTERP_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CD_TYPE_INTERP		(cd_interp_get_type ())
#define CD_INTERP(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_INTERP, CdInterp))
#define CD_INTERP_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_INTERP, CdInterpClass))
#define CD_IS_INTERP(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_INTERP))
#define CD_IS_INTERP_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_INTERP))
#define CD_INTERP_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_INTERP, CdInterpClass))
#define CD_INTERP_ERROR		(cd_interp_error_quark ())
#define CD_INTERP_TYPE_ERROR	(cd_interp_error_get_type ())

typedef struct _CdInterpPrivate CdInterpPrivate;

typedef struct
{
	 GObject		 parent;
	 CdInterpPrivate	*priv;
} CdInterp;

typedef struct
{
	GObjectClass		 parent_class;
	/*< private >*/
	gboolean		 (*prepare)		(CdInterp	*interp,
							 GError		**error);
	gdouble			 (*eval)		(CdInterp	*interp,
							 gdouble	 value,
							 GError		**error);
	/* Padding for future expansion */
	void (*_cd_interp_reserved1) (void);
	void (*_cd_interp_reserved2) (void);
	void (*_cd_interp_reserved3) (void);
	void (*_cd_interp_reserved4) (void);
	void (*_cd_interp_reserved5) (void);
	void (*_cd_interp_reserved6) (void);
	void (*_cd_interp_reserved7) (void);
	void (*_cd_interp_reserved8) (void);
} CdInterpClass;

/**
 * CdInterpError:
 * @CD_INTERP_ERROR_FAILED: the method failed for an unknown reason
 *
 * Errors that can be thrown
 */
typedef enum
{
	CD_INTERP_ERROR_FAILED,
	CD_INTERP_ERROR_LAST
} CdInterpError;

/**
 * CdInterpKind:
 *
 * The kind of interpolation.
 **/
typedef enum {
	CD_INTERP_KIND_LINEAR,
	CD_INTERP_KIND_AKIMA,
	CD_INTERP_KIND_LAST
} CdInterpKind;

GType		 cd_interp_get_type		(void);
GQuark		 cd_interp_error_quark		(void);

CdInterpKind	 cd_interp_get_kind		(CdInterp	*interp);
GArray		*cd_interp_get_x		(CdInterp	*interp);
GArray		*cd_interp_get_y		(CdInterp	*interp);
guint		 cd_interp_get_size		(CdInterp	*interp);
void		 cd_interp_insert		(CdInterp	*interp,
						 gdouble	 x,
						 gdouble	 y);
gboolean	 cd_interp_prepare		(CdInterp	*interp,
						 GError		**error);
gdouble		 cd_interp_eval			(CdInterp	*interp,
						 gdouble	 value,
						 GError		**error);

const gchar	*cd_interp_kind_to_string	(CdInterpKind	 kind);

G_END_DECLS

#endif /* __CD_INTERP_H */

