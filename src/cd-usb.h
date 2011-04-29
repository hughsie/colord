/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

#ifndef __CD_USB_H
#define __CD_USB_H

#include <glib-object.h>

#include <libusb-1.0/libusb.h>

G_BEGIN_DECLS

#define CD_TYPE_USB			(cd_usb_get_type ())
#define CD_USB(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_USB, CdUsb))
#define CD_IS_USB(o)			(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_USB))

typedef struct _CdUsbPrivate		CdUsbPrivate;
typedef struct _CdUsb			CdUsb;
typedef struct _CdUsbClass		CdUsbClass;

/* dummy */
#define CD_USB_ERROR	1

/* only libusb > 1.0.8 has libusb_strerror */
#ifndef HAVE_NEW_USB
#define	libusb_strerror(f1)				"unknown"
#endif

/**
 * CdSensorError:
 *
 * The error code.
 **/
typedef enum {
	CD_USB_ERROR_INTERNAL
} CdUsbError;

struct _CdUsb
{
	 GObject			 parent;
	 CdUsbPrivate			*priv;
};

struct _CdUsbClass
{
	GObjectClass	parent_class;
};

GType			 cd_usb_get_type		(void);
gboolean		 cd_usb_load			(CdUsb		*usb,
							 GError		**error);
gboolean		 cd_usb_connect			(CdUsb		*usb,
							 guint		 vendor_id,
							 guint		 product_id,
							 guint		 configuration,
							 guint		 interface,
							 GError		**error);
gboolean		 cd_usb_disconnect		(CdUsb		*usb,
							 GError		**error);
gboolean		 cd_usb_get_connected		(CdUsb		*usb);

gboolean		 cd_usb_attach_to_context	(CdUsb		*usb,
							 GMainContext	*context,
							 GError		**error);
libusb_device_handle	*cd_usb_get_device_handle	(CdUsb		*usb);
CdUsb			*cd_usb_new			(void);

G_END_DECLS

#endif /* __CD_USB_H */

