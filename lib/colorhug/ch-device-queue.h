/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2013 Richard Hughes <richard@hughsie.com>
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

#ifndef __CH_DEVICE_QUEUE_H
#define __CH_DEVICE_QUEUE_H

#include <glib-object.h>
#include <gusb.h>
#include <colord-private.h>

#include "ch-common.h"
#include "ch-hash.h"

G_BEGIN_DECLS

#define CH_TYPE_DEVICE_QUEUE		(ch_device_queue_get_type ())
#define CH_DEVICE_QUEUE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CH_TYPE_DEVICE_QUEUE, ChDeviceQueue))
#define CH_DEVICE_QUEUE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CH_TYPE_DEVICE_QUEUE, ChDeviceQueueClass))
#define CH_IS_DEVICE_QUEUE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CH_TYPE_DEVICE_QUEUE))

typedef struct _ChDeviceQueuePrivate	ChDeviceQueuePrivate;
typedef struct _ChDeviceQueue		ChDeviceQueue;
typedef struct _ChDeviceQueueClass	ChDeviceQueueClass;

struct _ChDeviceQueue
{
	 GObject		 parent;
	 ChDeviceQueuePrivate	*priv;
};

struct _ChDeviceQueueClass
{
	GObjectClass		 parent_class;

	/* signals */
	void		(* device_failed)	(ChDeviceQueue	*device_queue,
						 GUsbDevice	*device,
						 const gchar	*error_message);
	void		(* progress_changed)	(ChDeviceQueue	*device_queue,
						 guint		 percentage);

	/* padding for future expansion */
	void (*_ch_reserved1) (void);
	void (*_ch_reserved2) (void);
	void (*_ch_reserved3) (void);
	void (*_ch_reserved4) (void);
	void (*_ch_reserved5) (void);
};

/**
 * ChDeviceQueueProcessFlags:
 *
 * CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE:
 * 	Normal operation, where a single device command failure makes
 *	the return value of the process %FALSE, but the queue contibues
 *	to run for other devices.
 *
 * CH_DEVICE_QUEUE_PROCESS_FLAGS_CONTINUE_ERRORS:
 * 	Continue to submit commands to a device that has failed a
 *	command, for example where one command might not be supported
 *	in the middle of a queue of commands.
 *
 * CH_DEVICE_QUEUE_PROCESS_FLAGS_NONFATAL_ERRORS:
 * 	Do not consider a device error to be fatal, but instead emit
 *	a signal and continue with the rest of the queue. If the flag
 *	%CH_DEVICE_QUEUE_PROCESS_FLAGS_CONTINUE_ERRORS is not used then
 *	other commands to the same device will not be submitted.
 *
 * Flags for controlling processing options
 **/
typedef enum {
	CH_DEVICE_QUEUE_PROCESS_FLAGS_NONE		= 0,
	CH_DEVICE_QUEUE_PROCESS_FLAGS_CONTINUE_ERRORS	= 1 << 0,
	CH_DEVICE_QUEUE_PROCESS_FLAGS_NONFATAL_ERRORS	= 1 << 1
} ChDeviceQueueProcessFlags;

GType		 ch_device_queue_get_type	(void);
ChDeviceQueue	*ch_device_queue_new		(void);

void		 ch_device_queue_add		(ChDeviceQueue	*device_queue,
						 GUsbDevice	*device,
						 guint8		 cmd,
						 const guint8	*buffer_in,
						 gsize		 buffer_in_len,
						 guint8		*buffer_out,
						 gsize		 buffer_out_len);

void		 ch_device_queue_process_async	(ChDeviceQueue	*device_queue,
						 ChDeviceQueueProcessFlags process_flags,
						 GCancellable	*cancellable,
						 GAsyncReadyCallback callback,
						 gpointer	 user_data);
gboolean	 ch_device_queue_process_finish	(ChDeviceQueue	*device_queue,
						 GAsyncResult	*res,
						 GError		**error);
gboolean	 ch_device_queue_process	(ChDeviceQueue	*device_queue,
						 ChDeviceQueueProcessFlags process_flags,
						 GCancellable	*cancellable,
						 GError		**error);

/* command submitting functions */
void		 ch_device_queue_get_color_select	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 ChColorSelect	*color_select);
void		 ch_device_queue_get_hardware_version	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint8		*hw_version);
void		 ch_device_queue_set_color_select	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 ChColorSelect	 color_select);
void		 ch_device_queue_get_multiplier		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 ChFreqScale	*multiplier);
void		 ch_device_queue_set_multiplier		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 ChFreqScale	 multiplier);
void		 ch_device_queue_get_integral_time	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	*integral_time);
void		 ch_device_queue_set_integral_time	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	 integral_time);
void		 ch_device_queue_get_calibration_map	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	*calibration_map);
void		 ch_device_queue_set_calibration_map	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 const guint16	*calibration_map);
void		 ch_device_queue_get_firmware_ver	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	*major,
							 guint16	*minor,
							 guint16	*micro);
void		 ch_device_queue_get_calibration	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	 calibration_index,
							 CdMat3x3	*calibration,
							 guint8		*types,
							 gchar		*description);
void		 ch_device_queue_set_calibration	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	 calibration_index,
							 const CdMat3x3	*calibration,
							 guint8		 types,
							 const gchar	*description);
void		 ch_device_queue_clear_calibration	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	 calibration_index);
void		 ch_device_queue_get_pre_scale		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 gdouble	*pre_scale);
void		 ch_device_queue_set_pre_scale		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 gdouble	 pre_scale);
void		 ch_device_queue_get_post_scale		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 gdouble	*post_scale);
void		 ch_device_queue_set_post_scale		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 gdouble	 post_scale);
void		 ch_device_queue_get_serial_number	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint32	*serial_number);
void		 ch_device_queue_set_serial_number	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint32	 serial_number);
void		 ch_device_queue_get_leds		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint8		*leds);
void		 ch_device_queue_set_leds		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint8		 leds,
							 guint8		 repeat,
							 guint8		 on_time,
							 guint8		 off_time);
void		 ch_device_queue_get_pcb_errata		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	*pcb_errata);
void		 ch_device_queue_set_pcb_errata		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	 pcb_errata);
void		 ch_device_queue_get_remote_hash	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 ChSha1		*remote_hash);
void		 ch_device_queue_set_remote_hash	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 ChSha1		*remote_hash);
void		 ch_device_queue_get_dark_offsets	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 CdColorRGB	*value);
void		 ch_device_queue_set_dark_offsets	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 CdColorRGB	*value);
void		 ch_device_queue_write_eeprom		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 const gchar	*magic);
void		 ch_device_queue_take_reading_raw	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint32	*take_reading);
void		 ch_device_queue_take_reading_full	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 ChSensorKind	 sensor_kind,
							 guint32	*take_reading);
void		 ch_device_queue_take_readings		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 CdColorRGB	*value);
void		 ch_device_queue_take_readings_full	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 ChSensorKind	 sensor_kind,
							 CdColorRGB	*value);
void		 ch_device_queue_take_readings_xyz	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	 calibration_index,
							 CdColorXYZ	*value);
void		 ch_device_queue_take_readings_xyz_full	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	 calibration_index,
							 ChSensorKind	 sensor_kind,
							 CdColorXYZ	*value);
void		 ch_device_queue_reset			(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device);
void		 ch_device_queue_boot_flash		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device);
void		 ch_device_queue_self_test		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device);
void		 ch_device_queue_set_flash_success	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint8		 value);
void		 ch_device_queue_write_flash		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	 address,
							 const guint8	*data,
							 gsize		 len);
void		 ch_device_queue_read_flash		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	 address,
							 guint8		*data,
							 gsize		 len);
void		 ch_device_queue_verify_flash		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	 address,
							 const guint8	*data,
							 gsize		 len);
void		 ch_device_queue_erase_flash		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	 address,
							 gsize		 len);
void		 ch_device_queue_set_owner_name		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 const gchar	*name);
void		 ch_device_queue_get_owner_name		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 gchar		*name);
void		 ch_device_queue_set_owner_email	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 const gchar	*email);
void		 ch_device_queue_get_owner_email	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 gchar		*email);
void		 ch_device_queue_take_reading_array	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint8		*reading_array);
void		 ch_device_queue_get_measure_mode	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 ChMeasureMode	*measure_mode);
void		 ch_device_queue_set_measure_mode	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 ChMeasureMode	 measure_mode);
void		 ch_device_queue_write_sram		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	 address,
							 const guint8	*data,
							 gsize		 len);
void		 ch_device_queue_read_sram		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	 address,
							 guint8		*data,
							 gsize		 len);
void		 ch_device_queue_get_temperature	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 gdouble	*temperature);
void		 ch_device_queue_get_adc_vref_pos	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 gdouble	*vref);
void		 ch_device_queue_get_adc_vref_neg	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 gdouble	*vref);
void		 ch_device_queue_take_reading_spectral	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	*sram_addr);
void		 ch_device_queue_get_ccd_calibration	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	*indexes);
void		 ch_device_queue_set_ccd_calibration	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 const guint16	*indexes);

/* command utility functions */
gboolean	 ch_device_queue_set_calibration_ccmx	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 guint16	 calibration_index,
							 CdIt8		*ccmx,
							 GError		**error);
void		 ch_device_queue_write_firmware		(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 const guint8	*data,
							 gsize		 len);
void		 ch_device_queue_verify_firmware	(ChDeviceQueue	*device_queue,
							 GUsbDevice	*device,
							 const guint8	*data,
							 gsize		 len);

G_END_DECLS

#endif /* __CH_DEVICE_QUEUE_H */
