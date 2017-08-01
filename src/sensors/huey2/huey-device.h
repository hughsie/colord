/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
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

#define HUEY_USB_VID				0x0765
#define HUEY_USB_PID				0x5010

gboolean	 huey_device_open		(GUsbDevice *device,
						 GError **error);
gchar		*huey_device_get_status		(GUsbDevice *device,
						 GError **error)
						 G_GNUC_WARN_UNUSED_RESULT;
GBytes		*huey_device_read_eeprom	(GUsbDevice *device,
						 GError **error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 huey_device_take_sample	(GUsbDevice *device,
						 gdouble *val,
						 GError **error);

G_END_DECLS

#endif /* __HUEY_DEVICE_H */

