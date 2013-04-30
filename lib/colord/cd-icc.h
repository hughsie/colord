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
 * @CD_ICC_ERROR_INVALID_LOCALE:	Locale was invalid
 * @CD_ICC_ERROR_NO_DATA:		No data to read
 * @CD_ICC_ERROR_FAILED_TO_SAVE:	Failed to save file
 * @CD_ICC_ERROR_FAILED_TO_CREATE:	Failed to create file
 * @CD_ICC_ERROR_INVALID_COLORSPACE:	Invalid colorspace
 *
 * The ICC error code.
 *
 * Since: 0.1.32
 **/
typedef enum {
	CD_ICC_ERROR_FAILED_TO_OPEN,
	CD_ICC_ERROR_FAILED_TO_PARSE,
	CD_ICC_ERROR_INVALID_LOCALE,
	CD_ICC_ERROR_NO_DATA,
	CD_ICC_ERROR_FAILED_TO_SAVE,
	CD_ICC_ERROR_FAILED_TO_CREATE,
	CD_ICC_ERROR_INVALID_COLORSPACE,
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

/**
 * CdIccLoadFlags:
 * @CD_ICC_LOAD_FLAGS_NONE:		No flags set.
 * @CD_ICC_LOAD_FLAGS_NAMED_COLORS:	Parse any named colors in the profile.
 * @CD_ICC_LOAD_FLAGS_TRANSLATIONS:	Parse all translations in the profile.
 * @CD_ICC_LOAD_FLAGS_METADATA:		Parse the metadata in the profile.
 * @CD_ICC_LOAD_FLAGS_FALLBACK_MD5:	Calculate the profile MD5 if a profile
 * 					ID was not supplied in the profile.
 * @CD_ICC_LOAD_FLAGS_PRIMARIES:	Parse the primaries in the profile.
 *
 * Flags used when loading an ICC profile.
 *
 * Since: 0.1.32
 **/
typedef enum {
	CD_ICC_LOAD_FLAGS_NONE		= 0,
	CD_ICC_LOAD_FLAGS_NAMED_COLORS	= (1 << 0),
	CD_ICC_LOAD_FLAGS_TRANSLATIONS	= (1 << 1),
	CD_ICC_LOAD_FLAGS_METADATA	= (1 << 2),
	CD_ICC_LOAD_FLAGS_FALLBACK_MD5	= (1 << 3),
	CD_ICC_LOAD_FLAGS_PRIMARIES	= (1 << 4),
	/* new entries go here: */
	CD_ICC_LOAD_FLAGS_ALL		= 0xff,
	CD_ICC_LOAD_FLAGS_LAST
} CdIccLoadFlags;

/**
 * CdIccSaveFlags:
 * @CD_ICC_SAVE_FLAGS_NONE:		No flags set.
 *
 * Flags used when saving an ICC profile.
 *
 * Since: 0.1.32
 **/
typedef enum {
	CD_ICC_SAVE_FLAGS_NONE		= 0,
	CD_ICC_SAVE_FLAGS_LAST
} CdIccSaveFlags;

GType		 cd_icc_get_type			(void);
GQuark		 cd_icc_error_quark			(void);
CdIcc		*cd_icc_new				(void);

gboolean	 cd_icc_load_data			(CdIcc		*icc,
							 const guint8	*data,
							 gsize		 data_len,
							 CdIccLoadFlags	 flags,
							 GError		**error);
gboolean	 cd_icc_load_file			(CdIcc		*icc,
							 GFile		*file,
							 CdIccLoadFlags	 flags,
							 GCancellable	*cancellable,
							 GError		**error);
gboolean	 cd_icc_load_fd				(CdIcc		*icc,
							 gint		 fd,
							 CdIccLoadFlags	 flags,
							 GError		**error);
gboolean	 cd_icc_load_handle			(CdIcc		*icc,
							 gpointer	 handle,
							 CdIccLoadFlags	 flags,
							 GError		**error);
gboolean	 cd_icc_save_file			(CdIcc		*icc,
							 GFile		*file,
							 CdIccSaveFlags	 flags,
							 GCancellable	*cancellable,
							 GError		**error);
gchar		*cd_icc_to_string			(CdIcc		*icc);
gpointer	 cd_icc_get_handle			(CdIcc		*icc);
guint32		 cd_icc_get_size			(CdIcc		*icc);
const gchar	*cd_icc_get_filename			(CdIcc		*icc);
gdouble		 cd_icc_get_version			(CdIcc		*icc);
void		 cd_icc_set_version			(CdIcc		*icc,
							 gdouble	 version);
CdProfileKind	 cd_icc_get_kind			(CdIcc		*icc);
void		 cd_icc_set_kind			(CdIcc		*icc,
							 CdProfileKind	 kind);
CdColorspace	 cd_icc_get_colorspace			(CdIcc		*icc);
void		 cd_icc_set_colorspace			(CdIcc		*icc,
							 CdColorspace	 colorspace);
GHashTable	*cd_icc_get_metadata			(CdIcc		*icc);
const gchar	*cd_icc_get_metadata_item		(CdIcc		*icc,
							 const gchar	*key);
void		 cd_icc_add_metadata			(CdIcc		*icc,
							 const gchar	*key,
							 const gchar	*value);
void		 cd_icc_remove_metadata			(CdIcc		*icc,
							 const gchar	*key);
GPtrArray	*cd_icc_get_named_colors		(CdIcc		*icc);
gboolean	 cd_icc_get_can_delete			(CdIcc		*icc);
GDateTime	*cd_icc_get_created			(CdIcc		*icc);
const gchar	*cd_icc_get_checksum			(CdIcc		*icc);
const gchar	*cd_icc_get_description			(CdIcc		*icc,
							 const gchar	*locale,
							 GError		**error);
const gchar	*cd_icc_get_copyright			(CdIcc		*icc,
							 const gchar	*locale,
							 GError		**error);
const gchar	*cd_icc_get_manufacturer		(CdIcc		*icc,
							 const gchar	*locale,
							 GError		**error);
const gchar	*cd_icc_get_model			(CdIcc		*icc,
							 const gchar	*locale,
							 GError		**error);
void		 cd_icc_set_description			(CdIcc		*icc,
							 const gchar	*locale,
							 const gchar	*value);
void		 cd_icc_set_description_items		(CdIcc		*icc,
							 GHashTable	*values);
void		 cd_icc_set_copyright			(CdIcc		*icc,
							 const gchar	*locale,
							 const gchar	*value);
void		 cd_icc_set_copyright_items		(CdIcc		*icc,
							 GHashTable	*values);
void		 cd_icc_set_manufacturer		(CdIcc		*icc,
							 const gchar	*locale,
							 const gchar	*value);
void		 cd_icc_set_manufacturer_items		(CdIcc		*icc,
							 GHashTable	*values);
void		 cd_icc_set_model			(CdIcc		*icc,
							 const gchar	*locale,
							 const gchar	*value);
void		 cd_icc_set_model_items			(CdIcc		*icc,
							 GHashTable	*values);
const CdColorXYZ *cd_icc_get_red			(CdIcc		*icc);
const CdColorXYZ *cd_icc_get_green			(CdIcc		*icc);
const CdColorXYZ *cd_icc_get_blue			(CdIcc		*icc);
const CdColorXYZ *cd_icc_get_white			(CdIcc		*icc);
guint		 cd_icc_get_temperature			(CdIcc		*icc);
GArray		*cd_icc_get_warnings			(CdIcc		*icc);
gboolean	 cd_icc_create_from_edid		(CdIcc		*icc,
							 gdouble	 gamma_value,
							 const CdColorYxy *red,
							 const CdColorYxy *green,
							 const CdColorYxy *blue,
							 const CdColorYxy *white,
							 GError		**error);
GPtrArray	*cd_icc_get_vcgt			(CdIcc		*icc,
							 guint		 size,
							 GError		**error);
gboolean	 cd_icc_set_vcgt			(CdIcc		*icc,
							 GPtrArray	*vcgt,
							 GError		**error);
GPtrArray	*cd_icc_get_response			(CdIcc		*icc,
							 guint		 size,
							 GError		**error);

G_END_DECLS

#endif /* __CD_ICC_H */

