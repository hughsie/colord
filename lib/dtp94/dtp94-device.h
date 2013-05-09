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

#if !defined (__DTP94_H_INSIDE__) && !defined (DTP94_COMPILATION)
#error "Only <dtp94.h> can be included directly."
#endif

#ifndef __DTP94_DEVICE_H
#define __DTP94_DEVICE_H

#include <glib-object.h>
#include <gusb.h>
#include <colord-private.h>

G_BEGIN_DECLS

#define DTP94_DEVICE_ERROR			 dtp94_device_error_quark()

typedef enum {
	DTP94_DEVICE_ERROR_INTERNAL,
	DTP94_DEVICE_ERROR_NO_DATA,
	DTP94_DEVICE_ERROR_NO_SUPPORT,
	DTP94_DEVICE_ERROR_LAST
} Dtp94DeviceError;

GQuark		 dtp94_device_error_quark	(void);
gboolean	 dtp94_device_send_data		(GUsbDevice	*device,
						 const guint8	*request,
						 gsize		 request_len,
						 guint8		*reply,
						 gsize		 reply_len,
						 gsize		*reply_read,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 dtp94_device_send_cmd		(GUsbDevice	*device,
						 const gchar	*command,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
CdColorXYZ	*dtp94_device_take_sample	(GUsbDevice	*device,
						 CdSensorCap	 cap,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gchar		*dtp94_device_get_serial	(GUsbDevice	*device,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 dtp94_device_setup		(GUsbDevice	*device,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* __DTP94_DEVICE_H */

