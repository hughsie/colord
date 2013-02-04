/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Richard Hughes <richard@hughsie.com>
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

#if !defined (__HUEY_H_INSIDE__) && !defined (HUEY_COMPILATION)
#error "Only <huey.h> can be included directly."
#endif

#ifndef __HUEY_CTX_H
#define __HUEY_CTX_H

#include <glib-object.h>
#include <gio/gio.h>
#include <gusb.h>
#include <colord-private.h>

G_BEGIN_DECLS

#define HUEY_TYPE_CTX		(huey_ctx_get_type ())
#define HUEY_CTX(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), HUEY_TYPE_CTX, HueyCtx))
#define HUEY_CTX_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), HUEY_TYPE_CTX, HueyCtxClass))
#define HUEY_IS_CTX(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), HUEY_TYPE_CTX))
#define HUEY_IS_CTX_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), HUEY_TYPE_CTX))
#define HUEY_CTX_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), HUEY_TYPE_CTX, HueyCtxClass))
#define HUEY_CTX_ERROR		(huey_ctx_error_quark ())
#define HUEY_CTX_TYPE_ERROR	(huey_ctx_error_get_type ())

typedef struct _HueyCtxPrivate HueyCtxPrivate;

typedef struct
{
	 GObject		 parent;
	 HueyCtxPrivate		*priv;
} HueyCtx;

typedef struct
{
	GObjectClass		 parent_class;
	/*< private >*/
	/* Padding for future expansion */
	void (*_huey_ctx_reserved1) (void);
	void (*_huey_ctx_reserved2) (void);
	void (*_huey_ctx_reserved3) (void);
	void (*_huey_ctx_reserved4) (void);
	void (*_huey_ctx_reserved5) (void);
	void (*_huey_ctx_reserved6) (void);
	void (*_huey_ctx_reserved7) (void);
	void (*_huey_ctx_reserved8) (void);
} HueyCtxClass;

/**
 * HueyCtxError:
 * @HUEY_CTX_ERROR_FAILED: the request failed for an unknown reason
 *
 * Errors that can be thrown
 */
typedef enum
{
	HUEY_CTX_ERROR_FAILED,
	HUEY_CTX_ERROR_NO_SUPPORT,
	HUEY_CTX_ERROR_LAST
} HueyCtxError;

GType		 huey_ctx_get_type		(void);
GQuark		 huey_ctx_error_quark		(void);
HueyCtx		*huey_ctx_new			(void);

CdColorXYZ	*huey_ctx_take_sample		(HueyCtx	*ctx,
						 CdSensorCap	 cap,
						 GError		**error);
GUsbDevice	*huey_ctx_get_device		(HueyCtx	*ctx);
void		 huey_ctx_set_device		(HueyCtx	*ctx,
						 GUsbDevice	*device);
gboolean	 huey_ctx_setup			(HueyCtx	*ctx,
						 GError		**error);
const CdMat3x3	*huey_ctx_get_calibration_lcd	(HueyCtx	*ctx);
const CdMat3x3	*huey_ctx_get_calibration_crt	(HueyCtx	*ctx);
gfloat		 huey_ctx_get_calibration_value	(HueyCtx	*ctx);
const CdVec3	*huey_ctx_get_dark_offset	(HueyCtx	*ctx);
const gchar	*huey_ctx_get_unlock_string	(HueyCtx	*ctx);

G_END_DECLS

#endif /* __HUEY_CTX_H */

