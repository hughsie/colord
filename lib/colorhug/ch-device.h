/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2016 Richard Hughes <richard@hughsie.com>
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
#include <colord-private.h>

#include "ch-common.h"

G_BEGIN_DECLS

#define CH_DEVICE_ERROR		(ch_device_error_quark ())

GQuark		 ch_device_error_quark		(void);
gboolean	 ch_device_open			(GUsbDevice	*device,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 ch_device_close		(GUsbDevice	*device,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
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
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 ch_device_write_command	(GUsbDevice	*device,
						 guint8		 cmd,
						 const guint8	*buffer_in,
						 gsize		 buffer_in_len,
						 guint8		*buffer_out,
						 gsize		 buffer_out_len,
						 GCancellable	*cancellable,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 ch_device_check_firmware	(GUsbDevice	*device,
						 const guint8	*data,
						 gsize		 data_len,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
guint16		 ch_device_get_runcode_address	(GUsbDevice	*device);
const gchar	*ch_device_get_guid		(GUsbDevice	*device);


// FIXME: add to spec
typedef enum {
	CH_SPECTRUM_KIND_RAW		= 0x00,
	CH_SPECTRUM_KIND_DARK_CAL	= 0x01,
	CH_SPECTRUM_KIND_TEMP_CAL	= 0x02,
	CH_SPECTRUM_KIND_LAST
} ChSpectrumKind;

gboolean	 ch_device_open_full		(GUsbDevice	*device,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_self_test		(GUsbDevice	*device,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_set_serial_number	(GUsbDevice	*device,
						 guint32	 value,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_get_serial_number	(GUsbDevice	*device,
						 guint32	*value,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_set_leds		(GUsbDevice	*device,
						 ChStatusLed	 value,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_get_leds		(GUsbDevice	*device,
						 ChStatusLed	*value,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_set_pcb_errata	(GUsbDevice	*device,
						 ChPcbErrata	 value,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_get_pcb_errata	(GUsbDevice	*device,
						 ChPcbErrata	*value,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_set_ccd_calibration	(GUsbDevice	*device,
						 gdouble	 nm_start,
						 gdouble	 c0,
						 gdouble	 c1,
						 gdouble	 c2,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_set_crypto_key	(GUsbDevice	*device,
						 guint32	 keys[4],
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_get_ccd_calibration	(GUsbDevice	*device,
						 gdouble	*nm_start,
						 gdouble	*c0,
						 gdouble	*c1,
						 gdouble	*c2,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_set_integral_time	(GUsbDevice	*device,
						 guint16	 value,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_get_integral_time	(GUsbDevice	*device,
						 guint16	*value,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_get_temperature	(GUsbDevice	*device,
						 gdouble	*value,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_get_error		(GUsbDevice	*device,
						 ChError	*status,
						 ChCmd		*cmd,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 ch_device_take_reading_spectral(GUsbDevice	*device,
						 ChSpectrumKind value,
						 GCancellable	*cancellable,
						 GError		**error);
CdColorXYZ	*ch_device_take_reading_xyz	(GUsbDevice	*device,
						 guint16	 calibration_idx,
						 GCancellable	*cancellable,
						 GError		**error);
CdSpectrum	*ch_device_get_spectrum		(GUsbDevice	*device,
						 GCancellable	*cancellable,
						 GError		**error);

G_END_DECLS

#endif
