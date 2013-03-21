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

#ifndef __CD_ICC_H
#define __CD_ICC_H

#include <glib-object.h>
#include <gio/gio.h>

#include "cd-color.h"
#include "cd-enum.h"

G_BEGIN_DECLS

#define CD_TYPE_ICC		(cd_icc_get_type ())
#define CD_ICC(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_ICC, CdIcc))
#define CD_ICC_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_ICC, CdIccClass))
#define CD_IS_ICC(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_ICC))
#define CD_IS_ICC_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_ICC))
#define CD_ICC_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_ICC, CdIccClass))
#define CD_ICC_ERROR		(cd_icc_error_quark ())
#define CD_ICC_TYPE_ERROR	(cd_icc_error_get_type ())

typedef struct _CdIccPrivate CdIccPrivate;

typedef struct
{
	 GObject		 parent;
	 CdIccPrivate		*priv;
} CdIcc;

/**
 * CdIccError:
 * @CD_ICC_ERROR_FAILED_TO_OPEN:	Failed to open file
 * @CD_ICC_ERROR_FAILED_TO_PARSE:	Failed to parse data
 *
 * The ICC error code.
 *
 * Since: 0.1.32
 **/
typedef enum {
	CD_ICC_ERROR_FAILED_TO_OPEN,
	CD_ICC_ERROR_FAILED_TO_PARSE,
	CD_ICC_ERROR_LAST
} CdIccError;

typedef struct
{
	GObjectClass		 parent_class;
	/*< private >*/
	/* Padding for future expansion */
	void (*_cd_icc_reserved1) (void);
	void (*_cd_icc_reserved2) (void);
	void (*_cd_icc_reserved3) (void);
	void (*_cd_icc_reserved4) (void);
	void (*_cd_icc_reserved5) (void);
	void (*_cd_icc_reserved6) (void);
	void (*_cd_icc_reserved7) (void);
	void (*_cd_icc_reserved8) (void);
} CdIccClass;

GType		 cd_icc_get_type			(void);
GQuark		 cd_icc_error_quark			(void);
CdIcc		*cd_icc_new				(void);

gboolean	 cd_icc_load_data			(CdIcc		*icc,
							 const guint8	*data,
							 gsize		 data_len,
							 GError		**error);
gboolean	 cd_icc_load_file			(CdIcc		*icc,
							 GFile		*file,
							 GError		**error);
gboolean	 cd_icc_load_fd				(CdIcc		*icc,
							 gint		 fd,
							 GError		**error);
gchar		*cd_icc_to_string			(CdIcc		*icc);
gpointer	 cd_icc_get_handle			(CdIcc		*icc);
guint32		 cd_icc_get_size			(CdIcc		*icc);
const gchar	*cd_icc_get_filename			(CdIcc		*icc);
gdouble		 cd_icc_get_version			(CdIcc		*icc);
CdProfileKind	 cd_icc_get_kind			(CdIcc		*icc);

G_END_DECLS

#endif /* __CD_ICC_H */

