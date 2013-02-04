/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
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

#ifndef CH_DEVICE_H
#define CH_DEVICE_H

#include <glib.h>
#include <gusb.h>

#include "ch-common.h"

#define CH_DEVICE_ERROR		(ch_device_error_quark ())

GQuark		 ch_device_error_quark		(void);
gboolean	 ch_device_open			(GUsbDevice	*device,
						 GError		**error);
gboolean	 ch_device_is_colorhug		(GUsbDevice	*device);
ChDeviceMode	 ch_device_get_mode		(GUsbDevice	*device);
void		 ch_device_write_command_async	(GUsbDevice	*device,
						 guint8		 cmd,
						 const guint8	*buffer_in,
						 gsize		 buffer_in_len,
						 guint8		*buffer_out,
						 gsize		 buffer_out_len,
						 GCancellable	*cancellable,
						 GAsyncReadyCallback callback,
						 gpointer	 user_data);
gboolean	 ch_device_write_command_finish	(GUsbDevice	*device,
						 GAsyncResult	*res,
						 GError		**error);
gboolean	 ch_device_write_command	(GUsbDevice	*device,
						 guint8		 cmd,
						 const guint8	*buffer_in,
						 gsize		 buffer_in_len,
						 guint8		*buffer_out,
						 gsize		 buffer_out_len,
						 GCancellable	*cancellable,
						 GError		**error);

#endif
