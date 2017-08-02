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

#ifndef __HUEY_DEVICE_H
#define __HUEY_DEVICE_H

#include <glib-object.h>
#include <gusb.h>
#include <colord-private.h>

G_BEGIN_DECLS

gboolean	 huey_device_send_data		(GUsbDevice	*device,
						 const guchar	*request,
						 gsize		 request_len,
						 guchar		*reply,
						 gsize		 reply_len,
						 gsize		*reply_read,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 huey_device_set_leds		(GUsbDevice	*device,
						 guint8		 value,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gchar		*huey_device_get_serial_number	(GUsbDevice	*device,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gchar		*huey_device_get_status		(GUsbDevice	*device,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gchar		*huey_device_get_unlock_string	(GUsbDevice	*device,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 huey_device_unlock		(GUsbDevice	*device,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gdouble		 huey_device_get_ambient	(GUsbDevice	*device,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 huey_device_read_register_byte (GUsbDevice	*device,
						 guint8		 addr,
						 guint8		 *value,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 huey_device_read_register_string (GUsbDevice	*device,
						 guint8		 addr,
						 gchar		*value,
						 gsize		 len,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 huey_device_read_register_word (GUsbDevice	*device,
						 guint8		 addr,
						 guint32	*value,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 huey_device_read_register_float (GUsbDevice	*device,
						 guint8		 addr,
						 gfloat		*value,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 huey_device_read_register_vector (GUsbDevice	*device,
						 guint8		 addr,
						 CdVec3		*value,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 huey_device_read_register_matrix (GUsbDevice	*device,
						 guint8		 addr,
						 CdMat3x3	*value,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* __HUEY_DEVICE_H */

