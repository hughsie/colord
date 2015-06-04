/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
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

#if !defined (__OSPARK_H_INSIDE__) && !defined (OSPARK_COMPILATION)
#error "Only <ospark.h> can be included directly."
#endif

#ifndef __OSP_DEVICE_H
#define __OSP_DEVICE_H

#include <glib-object.h>
#include <gusb.h>
#include <colord-private.h>

G_BEGIN_DECLS

#define OSP_DEVICE_ERROR			 osp_device_error_quark()

typedef enum {
	OSP_DEVICE_ERROR_INTERNAL,
	OSP_DEVICE_ERROR_NO_DATA,
	OSP_DEVICE_ERROR_NO_SUPPORT,
	OSP_DEVICE_ERROR_LAST
} OspDeviceError;

GQuark		 osp_device_error_quark		(void);
CdSpectrum	*osp_device_take_spectrum	(GUsbDevice	*device,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gchar		*osp_device_get_serial		(GUsbDevice	*device,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gchar		*osp_device_get_fw_version	(GUsbDevice	*device,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 osp_device_open		(GUsbDevice	*device,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* __OSP_DEVICE_H */

