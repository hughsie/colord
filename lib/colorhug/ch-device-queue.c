/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>
#include <gusb.h>
#include <string.h>
#include <lcms2.h>

#include "ch-common.h"
#include "ch-device.h"
#include "ch-device-queue.h"
#include "ch-math.h"

static void	ch_device_queue_finalize	(GObject     *object);

#define CH_DEVICE_QUEUE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CH_TYPE_DEVICE_QUEUE, ChDeviceQueuePrivate))

/**
 * ChDeviceQueuePrivate:
 *
 * Private #ChDeviceQueue data
 **/
struct _ChDeviceQueuePrivate
{
	GPtrArray		*data_array;
	GHashTable		*devices_in_use;
};

enum {
	SIGNAL_DEVICE_FAILED,
	SIGNAL_PROGRESS_CHANGED,
	SIGNAL_LAST
};

G_DEFINE_TYPE (ChDeviceQueue, ch_device_queue, G_TYPE_OBJECT)

typedef gboolean (*ChDeviceQueueParseFunc)	(guint8		*output_buffer,
						 gsize		 output_buffer_size,
						 gpointer	 user_data,
						 GError		**error);

typedef enum {
	CH_DEVICE_QUEUE_DATA_STATE_PENDING,
	CH_DEVICE_QUEUE_DATA_STATE_WAITING_FOR_HW,
	CH_DEVICE_QUEUE_DATA_STATE_CANCELLED,
	CH_DEVICE_QUEUE_DATA_STATE_COMPLETE,
	CH_DEVICE_QUEUE_DATA_STATE_UNKNOWN
} ChDeviceQueueDataState;

typedef struct {
	ChDeviceQueueDataState	 state;
	GUsbDevice		*device;
	guint8			 cmd;
	guint8			*buffer_in;
	gsize			 buffer_in_len;
	guint8			*buffer_out;
	gsize			 buffer_out_len;
	GDestroyNotify		 buffer_out_destroy_func;
	ChDeviceQueueParseFunc	 parse_func;
	gpointer		 user_data;
	GDestroyNotify		 user_data_destroy_func;
} ChDeviceQueueData;

typedef struct {
	ChDeviceQueue		*device_queue;
	ChDeviceQueueProcessFlags process_flags;
	GCancellable		*cancellable;
	GSimpleAsyncResult	*res;
	GPtrArray		*failures;
} ChDeviceQueueHelper;

static guint signals[SIGNAL_LAST] = { 0 };

static gboolean ch_device_queue_process_data (ChDeviceQueueHelper *helper, ChDeviceQueueData *data);

/**
 * ch_device_queue_data_free:
 **/
static void
ch_device_queue_data_free (ChDeviceQueueData *data)
{
	if (data->buffer_out_destroy_func != NULL)
		data->buffer_out_destroy_func (data->buffer_out);
	if (data->user_data_destroy_func != NULL)
		data->user_data_destroy_func (data->user_data);
	g_free (data->buffer_in);
	g_object_unref (data->device);
	g_free (data);
}

/**
 * ch_device_queue_free_helper:
 **/
static void
ch_device_queue_free_helper (ChDeviceQueueHelper *helper)
{
	if (helper->cancellable != NULL)
		g_object_unref (helper->cancellable);
	g_object_unref (helper->device_queue);
	g_object_unref (helper->res);
	g_ptr_array_unref (helper->failures);
	g_free (helper);
}

/**
 * ch_device_queue_device_force_complete:
 **/
static void
ch_device_queue_device_force_complete (ChDeviceQueue *device_queue, GUsbDevice *device)
{
	ChDeviceQueueData *data;
	const gchar *device_id;
	const gchar *device_id_tmp;
	guint i;

	/* go through the list of commands and cancel them all */
	device_id = g_usb_device_get_platform_id (device);
	for (i = 0; i < device_queue->priv->data_array->len; i++) {
		data = g_ptr_array_index (device_queue->priv->data_array, i);
		device_id_tmp = g_usb_device_get_platform_id (data->device);
		if (g_strcmp0 (device_id_tmp, device_id) == 0)
			data->state = CH_DEVICE_QUEUE_DATA_STATE_CANCELLED;
	}
}

/**
 * ch_device_queue_update_progress:
 **/
static void
ch_device_queue_update_progress (ChDeviceQueue *device_queue)
{
	guint complete = 0;
	guint i;
	guint percentage;
	ChDeviceQueueData *data;

	/* no devices */
	if (device_queue->priv->data_array->len == 0)
		return;

	/* find out how many commands are complete */
	for (i = 0; i < device_queue->priv->data_array->len; i++) {
		data = g_ptr_array_index (device_queue->priv->data_array, i);
		if (data->state == CH_DEVICE_QUEUE_DATA_STATE_COMPLETE ||
		    data->state == CH_DEVICE_QUEUE_DATA_STATE_CANCELLED)
			complete++;
	}

	/* emit a signal with our progress */
	percentage = (complete * 100) / device_queue->priv->data_array->len;
	g_signal_emit (device_queue,
		       signals[SIGNAL_PROGRESS_CHANGED], 0,
		       percentage);
}

/**
 * ch_device_queue_count_in_state:
 **/
static guint
ch_device_queue_count_in_state (ChDeviceQueue *device_queue,
				ChDeviceQueueDataState state)
{
	guint i;
	guint cnt = 0;
	ChDeviceQueueData *data;

	/* find any data objects in a specific state */
	for (i = 0; i < device_queue->priv->data_array->len; i++) {
		data = g_ptr_array_index (device_queue->priv->data_array, i);
		if (data->state == state)
			cnt++;
	}
	return cnt;
}

/**
 * ch_device_queue_process_write_command_cb:
 **/
static void
ch_device_queue_process_write_command_cb (GObject *source,
					  GAsyncResult *res,
					  gpointer user_data)
{
	ChDeviceQueueData *data;
	ChDeviceQueueHelper *helper = (ChDeviceQueueHelper *) user_data;
	const gchar *device_id;
	const gchar *tmp;
	gboolean ret;
	gchar *error_msg = NULL;
	GError *error = NULL;
	guint i;
	guint pending_commands;
	ChError last_error_code = 0;
	GUsbDevice *device = G_USB_DEVICE (source);

	/* mark it as not in use */
	device_id = g_usb_device_get_platform_id (device);
	data = g_hash_table_lookup (helper->device_queue->priv->devices_in_use,
				    device_id);
	g_hash_table_remove (helper->device_queue->priv->devices_in_use,
			     device_id);

	/* get data */
	ret = ch_device_write_command_finish (device, res, &error);
	if (ret && data->parse_func != NULL) {
		/* do any conversion function */
		ret = data->parse_func (data->buffer_out,
					data->buffer_out_len,
					data->user_data,
					&error);
	}
	if (!ret) {
		/* tell the client the device has failed */
		g_debug ("emit device-failed: %s", error->message);
		g_signal_emit (helper->device_queue,
			       signals[SIGNAL_DEVICE_FAILED], 0,
			       device,
			       error->message);

		/* save this so we can possibly use when we're done */
		last_error_code = error->code;
		g_ptr_array_add (helper->failures,
				 g_strdup_printf ("%s: %s",
						  g_usb_device_get_platform_id (device),
						  error->message));
		g_error_free (error);

		/* should we mark complete other commands as complete */
		if ((helper->process_flags & CH_DEVICE_QUEUE_PROCESS_FLAGS_CONTINUE_ERRORS) == 0) {
			ch_device_queue_device_force_complete (helper->device_queue, device);
			ch_device_queue_update_progress (helper->device_queue);
			goto out;
		}
	}

	/* update progress */
	data->state = CH_DEVICE_QUEUE_DATA_STATE_COMPLETE;
	ch_device_queue_update_progress (helper->device_queue);

	/* is there another pending command for this device */
	for (i = 0; i < helper->device_queue->priv->data_array->len; i++) {
		data = g_ptr_array_index (helper->device_queue->priv->data_array, i);
		ret = ch_device_queue_process_data (helper, data);
		if (ret)
			break;
	}
out:
	/* any more pending commands? */
	pending_commands = ch_device_queue_count_in_state (helper->device_queue,
							   CH_DEVICE_QUEUE_DATA_STATE_PENDING);
	pending_commands += ch_device_queue_count_in_state (helper->device_queue,
							    CH_DEVICE_QUEUE_DATA_STATE_WAITING_FOR_HW);
	g_debug ("Pending commands: %i", pending_commands);
	if (pending_commands == 0) {

		/* should we return the process with an error, or just
		 * rely on the signal? */
		if (helper->failures->len == 1 &&
		    (helper->process_flags & CH_DEVICE_QUEUE_PROCESS_FLAGS_NONFATAL_ERRORS) == 0) {
			tmp = g_ptr_array_index (helper->failures, 0);
			g_simple_async_result_set_error (helper->res,
							 CH_DEVICE_ERROR,
							 last_error_code,
							 "%s", tmp);
		} else if (helper->failures->len > 1 &&
			   (helper->process_flags & CH_DEVICE_QUEUE_PROCESS_FLAGS_NONFATAL_ERRORS) == 0) {
			g_ptr_array_add (helper->failures, NULL);
			error_msg = g_strjoinv (", ", (gchar**) helper->failures->pdata);
			g_simple_async_result_set_error (helper->res,
							 CH_DEVICE_ERROR,
							 last_error_code,
							 "There were %i failures: %s",
							 helper->failures->len - 1,
							 error_msg);
		} else {
			g_simple_async_result_set_op_res_gboolean (helper->res, TRUE);
		}

		/* remove all commands from the queue, as they are done */
		g_ptr_array_set_size (helper->device_queue->priv->data_array, 0);
		g_simple_async_result_complete_in_idle (helper->res);
		ch_device_queue_free_helper (helper);
	}
	g_free (error_msg);
}

/**
 * ch_device_queue_process_data:
 *
 * Returns TRUE if the command was submitted
 **/
static gboolean
ch_device_queue_process_data (ChDeviceQueueHelper *helper,
			      ChDeviceQueueData *data)
{
	ChDeviceQueueData *data_tmp;
	const gchar *device_id;
	gboolean ret = FALSE;

	/* is this command already complete? */
	if (data->state == CH_DEVICE_QUEUE_DATA_STATE_COMPLETE)
		goto out;

	/* is this device already busy? */
	device_id = g_usb_device_get_platform_id (data->device);
	data_tmp = g_hash_table_lookup (helper->device_queue->priv->devices_in_use,
					device_id);
	if (data_tmp != NULL)
		goto out;

	/* write this command and wait for a response */
	ch_device_write_command_async (data->device,
				       data->cmd,
				       data->buffer_in,
				       data->buffer_in_len,
				       data->buffer_out,
				       data->buffer_out_len,
				       helper->cancellable,
				       ch_device_queue_process_write_command_cb,
				       helper);
	/* mark this as in use */
	g_hash_table_insert (helper->device_queue->priv->devices_in_use,
			     g_strdup (device_id),
			     data);

	/* success */
	ret = TRUE;

	/* remove this from the command queue -- TODO: retries? */
	data->state = CH_DEVICE_QUEUE_DATA_STATE_WAITING_FOR_HW;
out:
	return ret;
}

/**
 * ch_device_queue_process_async:
 * @device_queue:		A #ChDeviceQueue
 * @cancellable:	A #GCancellable, or %NULL
 * @callback:		A #GAsyncReadyCallback that will be called when finished.
 * @user_data:		User data passed to @callback
 *
 * Processes all commands in the command queue.
 **/
void
ch_device_queue_process_async (ChDeviceQueue		*device_queue,
			       ChDeviceQueueProcessFlags process_flags,
			       GCancellable		*cancellable,
			       GAsyncReadyCallback	 callback,
			       gpointer			 user_data)
{
	ChDeviceQueueHelper *helper;
	ChDeviceQueueData *data;
	guint i;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	helper = g_new0 (ChDeviceQueueHelper, 1);
	helper->process_flags = process_flags;
	helper->failures = g_ptr_array_new_with_free_func (g_free);
	helper->device_queue = g_object_ref (device_queue);
	helper->res = g_simple_async_result_new (G_OBJECT (device_queue),
						 callback,
						 user_data,
						 ch_device_queue_process_async);
	if (cancellable != NULL)
		helper->cancellable = g_object_ref (cancellable);

	/* go through the list of commands and try to submit them all */
	ch_device_queue_update_progress (helper->device_queue);
	for (i = 0; i < device_queue->priv->data_array->len; i++) {
		data = g_ptr_array_index (device_queue->priv->data_array, i);
		ch_device_queue_process_data (helper, data);
	}

	/* is anything pending? */
	if (g_hash_table_size (device_queue->priv->devices_in_use) == 0) {
		g_simple_async_result_set_op_res_gboolean (helper->res, TRUE);
		g_simple_async_result_complete_in_idle (helper->res);
		ch_device_queue_free_helper (helper);
	}
}

/**
 * ch_device_queue_process_finish:
 * @device_queue: a #ChDeviceQueue instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: %TRUE if the request was fulfilled.
 **/
gboolean
ch_device_queue_process_finish (ChDeviceQueue	*device_queue,
				GAsyncResult	*res,
				GError		**error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CH_DEVICE_QUEUE (device_queue), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_simple_async_result_get_op_res_gboolean (simple);
}

/**********************************************************************/

/* tiny helper to help us do the async operation */
typedef struct {
	GError		**error;
	GMainLoop	*loop;
	gboolean	 ret;
} ChDeviceQueueSyncHelper;

/**
 * ch_device_queue_process_finish_cb:
 **/
static void
ch_device_queue_process_finish_cb (GObject *source,
				   GAsyncResult *res,
				   gpointer user_data)
{
	ChDeviceQueue *device_queue = CH_DEVICE_QUEUE (source);
	ChDeviceQueueSyncHelper *helper = (ChDeviceQueueSyncHelper *) user_data;
	helper->ret = ch_device_queue_process_finish (device_queue, res, helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * ch_device_queue_process:
 * @device_queue:	A #ChDeviceQueue
 * @process_flags:	Flags how to process the queue, e.g. %CH_DEVICE_QUEUE_PROCESS_FLAGS_CONTINUE_ERRORS
 * @cancellable:	#GCancellable or %NULL
 * @error:		A #GError, or %NULL
 *
 * Processes all commands in the command queue.
 * WARNING: this function is syncronous and will block.
 *
 * Return value: %TRUE if the commands were executed successfully.
 **/
gboolean
ch_device_queue_process (ChDeviceQueue	*device_queue,
			 ChDeviceQueueProcessFlags process_flags,
			 GCancellable	*cancellable,
			 GError		**error)
{
	ChDeviceQueueSyncHelper helper;

	g_return_val_if_fail (CH_IS_DEVICE_QUEUE (device_queue), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create temp object */
	helper.ret = FALSE;
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;

	/* run async method */
	ch_device_queue_process_async (device_queue,
				       process_flags,
				       cancellable,
				       ch_device_queue_process_finish_cb,
				       &helper);
	g_main_loop_run (helper.loop);

	/* free temp object */
	g_main_loop_unref (helper.loop);

	return helper.ret;
}

/**********************************************************************/

/**
 * ch_device_queue_add_internal:
 **/
static void
ch_device_queue_add_internal (ChDeviceQueue		*device_queue,
			      GUsbDevice		*device,
			      guint8			 cmd,
			      const guint8		*buffer_in,
			      gsize			 buffer_in_len,
			      guint8			*buffer_out,
			      gsize			 buffer_out_len,
			      GDestroyNotify		 buffer_out_destroy_func,
			      ChDeviceQueueParseFunc	 parse_func,
			      gpointer			 user_data,
			      GDestroyNotify		 user_data_destroy_func)
{
	ChDeviceQueueData *data;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));

	data = g_new0 (ChDeviceQueueData, 1);
	data->state = CH_DEVICE_QUEUE_DATA_STATE_PENDING;
	data->parse_func = parse_func;
	data->user_data = user_data;
	data->user_data_destroy_func = user_data_destroy_func;
	data->cmd = cmd;
	data->device = g_object_ref (device);
	if (buffer_in != NULL)
		data->buffer_in = g_memdup (buffer_in, buffer_in_len);
	data->buffer_in_len = buffer_in_len;
	data->buffer_out = buffer_out;
	data->buffer_out_len = buffer_out_len;
	data->buffer_out_destroy_func = buffer_out_destroy_func;
	g_ptr_array_add (device_queue->priv->data_array, data);
}

/**
 * ch_device_queue_add:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @cmd:		The command, e.g. %CH_CMD_TAKE_READINGS
 * @buffer_in:		The input buffer, or %NULL
 * @buffer_in_len:	The size of @buffer_in
 * @buffer_out:		The output buffer, or %NULL
 * @buffer_out_len:	The size of @buffer_out
 *
 * Adds a raw command to the device queue.
 **/
void
ch_device_queue_add (ChDeviceQueue	*device_queue,
		     GUsbDevice		*device,
		     guint8		 cmd,
		     const guint8	*buffer_in,
		     gsize		 buffer_in_len,
		     guint8		*buffer_out,
		     gsize		 buffer_out_len)
{
	ch_device_queue_add_internal (device_queue,
				      device,
				      cmd,
				      buffer_in,
				      buffer_in_len,
				      buffer_out,
				      buffer_out_len,
				      NULL,
				      NULL,
				      NULL,
				      NULL);
}

/**********************************************************************/



/**
 * ch_device_queue_get_color_select:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @color_select:	The color select, e.g. %CH_COLOR_SELECT_RED
 *
 * Gets the selected sensor color.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_get_color_select (ChDeviceQueue *device_queue,
				  GUsbDevice *device,
				  ChColorSelect *color_select)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (color_select != NULL);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_GET_COLOR_SELECT,
			     NULL,
			     0,
			     (guint8 *) color_select,
			     1);
}

/**
 * ch_device_queue_set_color_select:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @color_select:	The color select, e.g. %CH_COLOR_SELECT_RED
 *
 * Sets the sensor measurement color.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_set_color_select (ChDeviceQueue *device_queue,
				  GUsbDevice *device,
				  ChColorSelect color_select)
{
	guint8 csel8 = color_select;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_COLOR_SELECT,
			     &csel8,
			     1,
			     NULL,
			     0);
}

/**
 * ch_device_queue_get_multiplier:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @multiplier:		The device multiplier, e.g. %CH_FREQ_SCALE_100
 *
 * Gets the sensor multiplier.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_get_multiplier (ChDeviceQueue *device_queue,
				GUsbDevice *device,
				ChFreqScale *multiplier)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (multiplier != NULL);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_GET_MULTIPLIER,
			     NULL,
			     0,
			     (guint8 *) multiplier,
			     1);
}

/**
 * ch_device_queue_set_multiplier:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @multiplier:		The device multiplier, e.g. %CH_FREQ_SCALE_100
 *
 * Sets the sensor multiplier.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_set_multiplier (ChDeviceQueue *device_queue,
				GUsbDevice *device,
				ChFreqScale multiplier)
{
	guint8 mult8 = multiplier;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_MULTIPLIER,
			     &mult8,
			     1,
			     NULL,
			     0);
}

/**
 * ch_device_queue_buffer_uint16_from_le_cb:
 **/
static gboolean
ch_device_queue_buffer_uint16_from_le_cb (guint8 *output_buffer,
					  gsize output_buffer_size,
					  gpointer user_data,
					  GError **error)
{
	gboolean ret = TRUE;
	guint16 *tmp = (guint16 *) output_buffer;

	/* check buffer size */
	if (output_buffer_size != sizeof (guint16)) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Wrong output buffer size, expected %" G_GSIZE_FORMAT ", got %" G_GSIZE_FORMAT,
			     sizeof (guint16), output_buffer_size);
		goto out;
	}
	*tmp = GUINT16_FROM_LE (*tmp);
out:
	return ret;
}

/**
 * ch_device_queue_buffer_uint32_from_le_cb:
 **/
static gboolean
ch_device_queue_buffer_uint32_from_le_cb (guint8 *output_buffer,
					  gsize output_buffer_size,
					  gpointer user_data,
					  GError **error)
{
	gboolean ret = TRUE;
	guint32 *tmp = (guint32 *) output_buffer;

	/* check buffer size */
	if (output_buffer_size != sizeof (guint32)) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Wrong output buffer size, expected %" G_GSIZE_FORMAT ", got %" G_GSIZE_FORMAT,
			     sizeof (guint32), output_buffer_size);
		goto out;
	}
	*tmp = GUINT32_FROM_LE (*tmp);
out:
	return ret;
}

/**
 * ch_device_queue_get_integral_time:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @integral_time:	The sensor integral time in device units
 *
 * Gets the reading integral time.

 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_get_integral_time (ChDeviceQueue *device_queue,
				   GUsbDevice *device,
				   guint16 *integral_time)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (integral_time != NULL);

	ch_device_queue_add_internal (device_queue,
				      device,
				      CH_CMD_GET_INTEGRAL_TIME,
				      NULL,
				      0,
				      (guint8 *) integral_time,
				      2,
				      NULL,
				      ch_device_queue_buffer_uint16_from_le_cb,
				      NULL,
				      NULL);
}

/**
 * ch_device_queue_set_integral_time:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @integral_time:	The sensor integral time in device units
 *
 * Sets the reading integral time.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_set_integral_time (ChDeviceQueue *device_queue,
				   GUsbDevice *device,
				   guint16 integral_time)
{
	guint16 integral_le;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (integral_time > 0);

	integral_le = GUINT16_TO_LE (integral_time);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_INTEGRAL_TIME,
			     (const guint8 *) &integral_le,
			     sizeof(guint16),
			     NULL,
			     0);
}

/**
 * ch_device_queue_get_calibration_map:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @calibration_map:	An array of slot positions
 *
 * Gets the calibration map.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_get_calibration_map (ChDeviceQueue *device_queue,
				     GUsbDevice *device,
				     guint16 *calibration_map)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (calibration_map != NULL);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_GET_CALIBRATION_MAP,
			     NULL,
			     0,
			     (guint8 *) calibration_map,
			     6 * sizeof(guint16));
}

/**
 * ch_device_queue_set_calibration_map:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @calibration_map:	An array of slot positions
 *
 * Sets the calibration map.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_set_calibration_map (ChDeviceQueue *device_queue,
				     GUsbDevice *device,
				     guint16 *calibration_map)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (calibration_map != NULL);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_CALIBRATION_MAP,
			     (const guint8 *) calibration_map,
			     6 * sizeof(guint16),
			     NULL,
			     0);
}

/* tiny helper */
typedef struct {
	guint16		*major;
	guint16		*minor;
	guint16		*micro;
} ChDeviceQueueGetFirmwareVerHelper;

/**
 * ch_device_queue_buffer_to_firmware_ver_cb:
 **/
static gboolean
ch_device_queue_buffer_to_firmware_ver_cb (guint8 *output_buffer,
					   gsize output_buffer_size,
					   gpointer user_data,
					   GError **error)
{
	ChDeviceQueueGetFirmwareVerHelper *helper = (void *) user_data;
	gboolean ret = TRUE;
	guint16 *tmp = (guint16 *) output_buffer;

	/* check buffer size */
	if (output_buffer_size != sizeof (guint16) * 3) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Wrong output buffer size, expected %" G_GSIZE_FORMAT ", got %" G_GSIZE_FORMAT,
			     sizeof (guint16) * 3, output_buffer_size);
		goto out;
	}

	*helper->major = GUINT16_FROM_LE (tmp[0]);
	*helper->minor = GUINT16_FROM_LE (tmp[1]);
	*helper->micro = GUINT16_FROM_LE (tmp[2]);
out:
	return ret;
}

/**
 * ch_device_queue_get_firmware_ver:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @major:		The firmware major version
 * @minor:		The firmware minor version
 * @micro:		The firmware micro version
 *
 * Gets the firmware version.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_get_firmware_ver (ChDeviceQueue *device_queue,
				  GUsbDevice *device,
				  guint16 *major,
				  guint16 *minor,
				  guint16 *micro)
{
	guint8 *buffer;
	ChDeviceQueueGetFirmwareVerHelper *helper;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (major != NULL);
	g_return_if_fail (minor != NULL);
	g_return_if_fail (micro != NULL);

	/* create a helper structure */
	helper = g_new0 (ChDeviceQueueGetFirmwareVerHelper, 1);
	helper->major = major;
	helper->minor = minor;
	helper->micro = micro;

	buffer = g_new0 (guint8, sizeof (guint16) * 3);
	ch_device_queue_add_internal (device_queue,
				      device,
				      CH_CMD_GET_FIRMWARE_VERSION,
				      NULL,
				      0,
				      buffer,
				      sizeof (guint16) * 3,
				      g_free,
				      ch_device_queue_buffer_to_firmware_ver_cb,
				      helper,
				      g_free);
}

/* tiny helper */
typedef struct {
	CdMat3x3	*calibration;
	guint8		*types;
	gchar		*description;
} ChDeviceQueueGetCalibrationHelper;

/**
 * ch_device_queue_buffer_to_get_calibration_cb:
 **/
static gboolean
ch_device_queue_buffer_to_get_calibration_cb (guint8 *output_buffer,
					      gsize output_buffer_size,
					      gpointer user_data,
					      GError **error)
{
	ChDeviceQueueGetCalibrationHelper *helper = (void *) user_data;
	gboolean ret = TRUE;
	gdouble *calibration_tmp;
	guint i;

	/* check buffer size */
	if (output_buffer_size != 60) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Wrong output buffer size, expected %i, got %" G_GSIZE_FORMAT,
			     60, output_buffer_size);
		goto out;
	}

	/* convert back into floating point */
	if (helper->calibration != NULL) {
		calibration_tmp = cd_mat33_get_data (helper->calibration);
		for (i = 0; i < 9; i++) {
			ch_packed_float_to_double ((ChPackedFloat *) &output_buffer[i*4],
						   &calibration_tmp[i]);
		}
	}

	/* get the supported types */
	if (helper->types != NULL)
		*helper->types = output_buffer[9*4];

	/* get the description */
	if (helper->description != NULL) {
		strncpy (helper->description,
			 (const char *) output_buffer + 9*4 + 1,
			 CH_CALIBRATION_DESCRIPTION_LEN);
	}
out:
	return ret;
}

/**
 * ch_device_queue_get_calibration:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @calibration_index:	The slot position
 * @calibration:	the 3x3 calibration matrix
 * @types:		The types the matrix supports
 * @description:	The description of the calibration
 *
 * Gets the calibration data.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_get_calibration (ChDeviceQueue *device_queue,
				 GUsbDevice *device,
				 guint16 calibration_index,
				 CdMat3x3 *calibration,
				 guint8 *types,
				 gchar *description)
{
	guint8 *buffer;
	ChDeviceQueueGetCalibrationHelper *helper;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (calibration_index < CH_CALIBRATION_MAX);

	/* create a helper structure */
	helper = g_new0 (ChDeviceQueueGetCalibrationHelper, 1);
	helper->calibration = calibration;
	helper->types = types;
	helper->description = description;

	buffer = g_new0 (guint8, 9*4 + 1 + CH_CALIBRATION_DESCRIPTION_LEN);
	ch_device_queue_add_internal (device_queue,
				      device,
				      CH_CMD_GET_CALIBRATION,
				      (guint8 *) &calibration_index,
				      sizeof(guint16),
				      (guint8 *) buffer,
				      9*4 + 1 + CH_CALIBRATION_DESCRIPTION_LEN,
				      g_free,
				      ch_device_queue_buffer_to_get_calibration_cb,
				      helper,
				      g_free);
}

/**
 * ch_device_queue_set_calibration:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @calibration_index:	The slot position
 * @calibration:	the 3x3 calibration matrix
 * @types:		The types the matrix supports
 * @description:	The description of the calibration
 *
 * Sets the calibration data.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_set_calibration (ChDeviceQueue *device_queue,
				 GUsbDevice *device,
				 guint16 calibration_index,
				 const CdMat3x3 *calibration,
				 guint8 types,
				 const gchar *description)
{
	gdouble *calibration_tmp;
	guint8 buffer[9*4 + 2 + 1 + CH_CALIBRATION_DESCRIPTION_LEN];
	guint i;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (calibration_index < CH_CALIBRATION_MAX);
	g_return_if_fail (calibration != NULL);
	g_return_if_fail (description != NULL);

	/* write index */
	memcpy (buffer, &calibration_index, sizeof(guint16));

	/* convert from float to signed value */
	for (i = 0; i < 9; i++) {
		calibration_tmp = cd_mat33_get_data (calibration);
		ch_double_to_packed_float (calibration_tmp[i],
					   (ChPackedFloat *) &buffer[i*4 + 2]);
	}

	/* write types */
	buffer[9*4 + 2] = types;

	/* write description */
	strncpy ((gchar *) buffer + 9*4 + 2 + 1,
		 description,
		 CH_CALIBRATION_DESCRIPTION_LEN);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_CALIBRATION,
			     (guint8 *) buffer,
			     sizeof(buffer),
			     NULL,
			     0);
}

/**
 * ch_device_queue_set_calibration_ccmx:
 **/
gboolean
ch_device_queue_set_calibration_ccmx (ChDeviceQueue *device_queue,
				      GUsbDevice *device,
				      guint16 calibration_index,
				      CdIt8 *ccmx,
				      GError **error)
{
	const CdMat3x3 *calibration;
	const gchar *description;
	gboolean ret = TRUE;
	gdouble *calibration_tmp;
	guint8 types = 0;
	guint i;

	g_return_val_if_fail (CD_IS_IT8 (ccmx), FALSE);
	g_return_val_if_fail (CH_IS_DEVICE_QUEUE (device_queue), FALSE);
	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);

	/* ensure correct kind */
	if (cd_it8_get_kind (ccmx) != CD_IT8_KIND_CCMX) {
		ret = FALSE;
		g_set_error (error, 1, 0, "is not a CCMX file");
		goto out;
	}

	/* get the supported display types */
	if (cd_it8_has_option (ccmx, "TYPE_FACTORY")) {
		types = CH_CALIBRATION_TYPE_ALL;
	} else {
		if (cd_it8_has_option (ccmx, "TYPE_LCD"))
			types += CH_CALIBRATION_TYPE_LCD;
		if (cd_it8_has_option (ccmx, "TYPE_LED"))
			types += CH_CALIBRATION_TYPE_LED;
		if (cd_it8_has_option (ccmx, "TYPE_CRT"))
			types += CH_CALIBRATION_TYPE_CRT;
		if (cd_it8_has_option (ccmx, "TYPE_PROJECTOR"))
			types += CH_CALIBRATION_TYPE_PROJECTOR;
	}

	/* no types set in CCMX file */
	if (types == 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "No TYPE_x in ccmx file");
		goto out;
	}

	/* get the description from the ccmx file */
	description = cd_it8_get_title (ccmx);
	if (description == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "CCMX file does not have DISPLAY");
		goto out;
	}

	/* get the values and check for sanity */
	calibration = cd_it8_get_matrix (ccmx);
	calibration_tmp = cd_mat33_get_data (calibration);
	for (i = 0; i < 9; i++) {
		if (calibration_tmp[i] < -10.0f ||
		    calibration_tmp[i] > 10.0f) {
			ret = FALSE;
			g_set_error (error, 1, 0,
				     "Matrix value %i out of range %f",
				     i, calibration_tmp[i]);
			goto out;
		}
	}

	/* set to HW */
	ch_device_queue_set_calibration (device_queue,
					 device,
					 calibration_index,
					 calibration,
					 types,
					 description);
out:
	return ret;
}

/**
 * ch_device_queue_write_firmware:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @data:		Firmware binary data
 * @len:		Size of @data
 *
 * Writes new firmware to the device.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_write_firmware (ChDeviceQueue	*device_queue,
				GUsbDevice	*device,
				const guint8	*data,
				gsize		 len)
{
	gsize chunk_len;
	guint idx;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (data != NULL);

	/* erase flash */
	g_debug ("Erasing at %04x size %" G_GSIZE_FORMAT,
		 CH_EEPROM_ADDR_RUNCODE, len);
	ch_device_queue_erase_flash (device_queue,
				     device,
				     CH_EEPROM_ADDR_RUNCODE,
				     len);

	/* just write in 32 byte chunks, as we're sure that the firmware
	 * image has been prepared to end on a 64 byte chunk with
	 * colorhug-inhx32-to-bin >= 0.1.5 */
	idx = 0;
	chunk_len = CH_FLASH_TRANSFER_BLOCK_SIZE;
	do {
		if (idx + chunk_len > len)
			chunk_len = len - idx;
		g_debug ("Writing at %04x size %" G_GSIZE_FORMAT,
			 CH_EEPROM_ADDR_RUNCODE + idx,
			 chunk_len);
		ch_device_queue_write_flash (device_queue,
					     device,
					     CH_EEPROM_ADDR_RUNCODE + idx,
					     (guint8 *) data + idx,
					     chunk_len);
		idx += chunk_len;
	} while (idx < len);
}

/**
 * ch_device_queue_verify_firmware:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @data:		Firmware binary data
 * @len:		Size of @data
 *
 * Verifies firmware on the device.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_verify_firmware (ChDeviceQueue	*device_queue,
				 GUsbDevice	*device,
				 const guint8	*data,
				 gsize		 len)
{
	gsize chunk_len;
	guint idx;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (data != NULL);

	/* read in 60 byte chunks */
	idx = 0;
	chunk_len = 60;
	do {
		if (idx + chunk_len > len)
			chunk_len = len - idx;
		g_debug ("Verifying at %04x size %" G_GSIZE_FORMAT,
			 CH_EEPROM_ADDR_RUNCODE + idx,
			 chunk_len);
		ch_device_queue_verify_flash (device_queue,
					      device,
					      CH_EEPROM_ADDR_RUNCODE + idx,
					      data + idx,
					      chunk_len);
		idx += chunk_len;
	} while (idx < len);
}

/**
 * ch_device_queue_clear_calibration:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @calibration_index:	Slot
 *
 * Clears a calibration slot.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_clear_calibration (ChDeviceQueue *device_queue,
				   GUsbDevice *device,
				   guint16 calibration_index)
{
	guint8 buffer[9*4 + 2 + 1 + CH_CALIBRATION_DESCRIPTION_LEN];

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (calibration_index < CH_CALIBRATION_MAX);

	/* write index */
	memcpy (buffer, &calibration_index, sizeof(guint16));

	/* clear data */
	memset (buffer + 2, 0xff, sizeof (buffer) - 2);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_CALIBRATION,
			     (guint8 *) buffer,
			     sizeof(buffer),
			     NULL,
			     0);
}

/**
 * ch_device_queue_buffer_to_double_cb:
 **/
static gboolean
ch_device_queue_buffer_to_double_cb (guint8 *output_buffer,
				     gsize output_buffer_size,
				     gpointer user_data,
				     GError **error)
{
	ChPackedFloat *buffer = (ChPackedFloat *) output_buffer;
	gboolean ret = TRUE;
	gdouble *value = (gdouble *) user_data;

	/* check buffer size */
	if (output_buffer_size != sizeof (ChPackedFloat)) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Wrong output buffer size, expected %" G_GSIZE_FORMAT ", got %" G_GSIZE_FORMAT,
			     sizeof (ChPackedFloat), output_buffer_size);
		goto out;
	}

	/* convert back into floating point */
	ch_packed_float_to_double (buffer, value);
out:
	return ret;
}

/**
 * ch_device_queue_get_pre_scale:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @pre_scale:		Pre-scale value
 *
 * Gets the pre scale value.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_get_pre_scale (ChDeviceQueue *device_queue,
			       GUsbDevice *device,
			       gdouble *pre_scale)
{
	guint8 *buffer;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (pre_scale != NULL);

	*pre_scale = 0.0f;
	buffer = g_new0 (guint8, sizeof (ChPackedFloat));
	ch_device_queue_add_internal (device_queue,
				     device,
				     CH_CMD_GET_PRE_SCALE,
				     NULL,
				     0,
				     buffer,
				     sizeof(ChPackedFloat),
				     g_free,
				     ch_device_queue_buffer_to_double_cb,
				     pre_scale,
				     NULL);
}

/**
 * ch_device_queue_set_pre_scale:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @pre_scale:		Pre-scale value
 *
 * Sets the pre-scale value.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_set_pre_scale (ChDeviceQueue *device_queue,
			       GUsbDevice *device,
			       gdouble pre_scale)
{
	ChPackedFloat buffer;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));

	/* convert from float to signed value */
	ch_double_to_packed_float (pre_scale, &buffer);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_PRE_SCALE,
			     (guint8 *) &buffer,
			     sizeof(buffer),
			     NULL,
			     0);
}

/**
 * ch_device_queue_get_temperature:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @temperature:	Temperature in Celcius
 *
 * Gets the device temperature.
 *
 * NOTE: This command is available on hardware version: 2
 **/
void
ch_device_queue_get_temperature (ChDeviceQueue *device_queue,
				 GUsbDevice *device,
				 gdouble *temperature)
{
	guint8 *buffer;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (temperature != NULL);

	*temperature = 0.0f;
	buffer = g_new0 (guint8, sizeof (ChPackedFloat));
	ch_device_queue_add_internal (device_queue,
				     device,
				     CH_CMD_GET_TEMPERATURE,
				     NULL,
				     0,
				     buffer,
				     sizeof(ChPackedFloat),
				     g_free,
				     ch_device_queue_buffer_to_double_cb,
				     temperature,
				     NULL);
}

/**
 * ch_device_queue_get_post_scale:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @post_scale:		The post-scale value
 *
 * Gets the post scale value.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_get_post_scale (ChDeviceQueue *device_queue,
				GUsbDevice *device,
				gdouble *post_scale)
{
	guint8 *buffer;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (post_scale != NULL);

	*post_scale = 0.0f;
	buffer = g_new0 (guint8, sizeof (ChPackedFloat));
	ch_device_queue_add_internal (device_queue,
				      device,
				      CH_CMD_GET_POST_SCALE,
				      NULL,
				      0,
				      buffer,
				      sizeof(ChPackedFloat),
				      g_free,
				      ch_device_queue_buffer_to_double_cb,
				      post_scale,
				      NULL);
}

/**
 * ch_device_queue_set_post_scale:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @post_scale:		The post-scale value
 *
 * Sets the post scale value.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_set_post_scale (ChDeviceQueue *device_queue,
				GUsbDevice *device,
				gdouble post_scale)
{
	ChPackedFloat buffer;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));

	/* convert from float to signed value */
	ch_double_to_packed_float (post_scale, &buffer);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_POST_SCALE,
			     (guint8 *) &buffer,
			     sizeof(buffer),
			     NULL,
			     0);
}

/**
 * ch_device_queue_get_serial_number:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @serial_number:	The device serial number
 *
 * Gets the device serial number.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_get_serial_number (ChDeviceQueue *device_queue,
				   GUsbDevice *device,
				   guint32 *serial_number)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (serial_number != NULL);

	*serial_number = 0;
	ch_device_queue_add_internal (device_queue,
				      device,
				      CH_CMD_GET_SERIAL_NUMBER,
				      NULL,
				      0,
				      (guint8 *) serial_number,
				      sizeof(guint32),
				      NULL,
				      ch_device_queue_buffer_uint32_from_le_cb,
				      NULL,
				      NULL);
}

/**
 * ch_device_queue_set_serial_number:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @serial_number:	The device serial number
 *
 * Sets the device serial number.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_set_serial_number (ChDeviceQueue *device_queue,
				   GUsbDevice *device,
				   guint32 serial_number)
{
	guint32 serial_le;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (serial_number > 0);

	serial_le = GUINT32_TO_LE (serial_number);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_SERIAL_NUMBER,
			     (const guint8 *) &serial_le,
			     sizeof(serial_le),
			     NULL,
			     0);
}

/**
 * ch_device_queue_get_leds:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @leds:		The LED bitfield
 *
 * Gets the LED status.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_get_leds (ChDeviceQueue *device_queue,
			  GUsbDevice *device,
			  guint8 *leds)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (leds != NULL);

	*leds = 0;
	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_GET_LEDS,
			     NULL,
			     0,
			     leds,
			     1);
}

/**
 * ch_device_queue_set_leds:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @leds:		The LEDs bitfield
 * @repeat:		Sets the number of times to repeat the pattern
 * @on_time:		Set the on time
 * @off_time:		Set the off time
 *
 * Sets the LED status.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_set_leds (ChDeviceQueue *device_queue,
			  GUsbDevice *device,
			  guint8 leds,
			  guint8 repeat,
			  guint8 on_time,
			  guint8 off_time)
{
	guint8 buffer[4];

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (leds < 0x04);

	buffer[0] = leds;
	buffer[1] = repeat;
	buffer[2] = on_time;
	buffer[3] = off_time;
	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_LEDS,
			     (const guint8 *) buffer,
			     sizeof (buffer),
			     NULL,
			     0);
}

/**
 * ch_device_queue_get_pcb_errata:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @pcb_errata:		The PCB errata, e.g. %CH_PCB_ERRATA_SWAPPED_LEDS
 *
 * Gets the PCB errata level.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_get_pcb_errata (ChDeviceQueue *device_queue,
				GUsbDevice *device,
				guint16 *pcb_errata)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (pcb_errata != NULL);

	*pcb_errata = CH_PCB_ERRATA_NONE;
	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_GET_PCB_ERRATA,
			     NULL,
			     0,
			     (guint8 *) pcb_errata,
			     sizeof (guint16));
}

/**
 * ch_device_queue_set_pcb_errata:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @pcb_errata:		The PCB errata, e.g. %CH_PCB_ERRATA_SWAPPED_LEDS
 *
 * Sets the PCB board errata.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_set_pcb_errata (ChDeviceQueue *device_queue,
				GUsbDevice *device,
				guint16 pcb_errata)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_PCB_ERRATA,
			     (const guint8 *) &pcb_errata,
			     sizeof (guint16),
			     NULL,
			     0);
}

/**
 * ch_device_queue_get_remote_hash:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @remote_hash:	A #ChSha1
 *
 * Gets the remote hash stored on the device.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_get_remote_hash (ChDeviceQueue *device_queue,
				 GUsbDevice *device,
				 ChSha1 *remote_hash)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (remote_hash != NULL);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_GET_REMOTE_HASH,
			     NULL,
			     0,
			     (guint8 *) remote_hash,
			     sizeof (ChSha1));
}

/**
 * ch_device_queue_set_remote_hash:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @remote_hash:	A #ChSha1
 *
 * Sets the remote hash on the device.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_set_remote_hash (ChDeviceQueue *device_queue,
				 GUsbDevice *device,
				 ChSha1 *remote_hash)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_REMOTE_HASH,
			     (const guint8 *) remote_hash,
			     sizeof (ChSha1),
			     NULL,
			     0);
}

/**
 * ch_device_queue_write_eeprom:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @magic:		The magic sekret string
 *
 * Writes values to the firmware to be set at device startup.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_write_eeprom (ChDeviceQueue *device_queue,
			      GUsbDevice *device,
			      const gchar *magic)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (magic != NULL);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_WRITE_EEPROM,
			     (const guint8 *) magic,
			     strlen(magic),
			     NULL,
			     0);
}

/**
 * ch_device_queue_buffer_dark_offsets_cb:
 **/
static gboolean
ch_device_queue_buffer_dark_offsets_cb (guint8 *output_buffer,
					gsize output_buffer_size,
					gpointer user_data,
					GError **error)
{
	CdColorRGB *value = (CdColorRGB *) user_data;
	gboolean ret = TRUE;
	guint16 *buffer = (guint16 *) output_buffer;

	/* check buffer size */
	if (output_buffer_size != sizeof (guint16) * 3) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Wrong output buffer size, expected %" G_GSIZE_FORMAT ", got %" G_GSIZE_FORMAT,
			     sizeof (guint16) * 3, output_buffer_size);
		goto out;
	}

	/* convert back into floating point */
	value->R = (gdouble) buffer[0] / (gdouble) 0xffff;
	value->G = (gdouble) buffer[1] / (gdouble) 0xffff;
	value->B = (gdouble) buffer[2] / (gdouble) 0xffff;
out:
	return ret;
}

/**
 * ch_device_queue_get_dark_offsets:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @value:		A #CdColorRGB
 *
 * Gets the device dark offsets.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_get_dark_offsets (ChDeviceQueue *device_queue,
				  GUsbDevice *device,
				  CdColorRGB *value)
{
	guint8 *buffer;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (value != NULL);

	buffer = g_new0 (guint8, sizeof(guint16) * 3);
	ch_device_queue_add_internal (device_queue,
				      device,
				      CH_CMD_GET_DARK_OFFSETS,
				      NULL,
				      0,
				      buffer,
				      sizeof(guint16) * 3,
				      g_free,
				      ch_device_queue_buffer_dark_offsets_cb,
				      value,
				      NULL);
}

/**
 * ch_device_queue_set_dark_offsets:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @value:		A #CdColorRGB
 *
 * Sets the device dark offsets.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_set_dark_offsets (ChDeviceQueue *device_queue,
				  GUsbDevice *device,
				  CdColorRGB *value)
{
	guint16 buffer[3];

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));

	buffer[0] = value->R * (gdouble) 0xffff;
	buffer[1] = value->G * (gdouble) 0xffff;
	buffer[2] = value->B * (gdouble) 0xffff;
	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_DARK_OFFSETS,
			     (const guint8 *) buffer,
			     sizeof(buffer),
			     NULL,
			     0);
}

/**
 * ch_device_queue_take_reading_raw:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @take_reading:	A raw reading value
 *
 * Take a raw reading from the sensor.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_take_reading_raw (ChDeviceQueue *device_queue,
				  GUsbDevice *device,
				  guint32 *take_reading)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (take_reading != NULL);

	ch_device_queue_add_internal (device_queue,
				      device,
				      CH_CMD_TAKE_READING_RAW,
				      NULL,
				      0,
				      (guint8 *) take_reading,
				      sizeof(guint32),
				      NULL,
				      ch_device_queue_buffer_uint32_from_le_cb,
				      NULL,
				      NULL);
}

/**
 * ch_device_queue_buffer_triple_rgb_cb:
 **/
static gboolean
ch_device_queue_buffer_triple_rgb_cb (guint8 *output_buffer,
				      gsize output_buffer_size,
				      gpointer user_data,
				      GError **error)
{
	CdColorRGB *value = (CdColorRGB *) user_data;
	ChPackedFloat *buffer = (ChPackedFloat *) output_buffer;
	gboolean ret = TRUE;

	/* check buffer size */
	if (output_buffer_size != sizeof (ChPackedFloat) * 3) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Wrong output buffer size, expected %" G_GSIZE_FORMAT ", got %" G_GSIZE_FORMAT,
			     sizeof (ChPackedFloat) * 3, output_buffer_size);
		goto out;
	}

	/* convert back into floating point */
	ch_packed_float_to_double (&buffer[0], &value->R);
	ch_packed_float_to_double (&buffer[1], &value->G);
	ch_packed_float_to_double (&buffer[2], &value->B);
out:
	return ret;
}

/**
 * ch_device_queue_take_readings:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @value:		The #CdColorRGB of the raw reading
 *
 * Take a RGB triplet of readings from the sensor without applying the
 * calibration matrix.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_take_readings (ChDeviceQueue *device_queue,
			       GUsbDevice *device,
			       CdColorRGB *value)
{
	guint8 *buffer;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (value != NULL);

	buffer = g_new0 (guint8, sizeof(ChPackedFloat) * 3);
	ch_device_queue_add_internal (device_queue,
				      device,
				      CH_CMD_TAKE_READINGS,
				      NULL,
				      0,
				      buffer,
				      sizeof(ChPackedFloat) * 3,
				      g_free,
				      ch_device_queue_buffer_triple_rgb_cb,
				      value,
				      NULL);
}

/**
 * ch_device_queue_buffer_triple_xyz_cb:
 **/
static gboolean
ch_device_queue_buffer_triple_xyz_cb (guint8 *output_buffer,
				      gsize output_buffer_size,
				      gpointer user_data,
				      GError **error)
{
	CdColorXYZ *value = (CdColorXYZ *) user_data;
	ChPackedFloat *buffer = (ChPackedFloat *) output_buffer;
	gboolean ret = TRUE;

	/* check buffer size */
	if (output_buffer_size != sizeof (ChPackedFloat) * 3) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Wrong output buffer size, expected %" G_GSIZE_FORMAT ", got %" G_GSIZE_FORMAT,
			     sizeof (ChPackedFloat) * 3, output_buffer_size);
		goto out;
	}

	/* convert back into floating point */
	ch_packed_float_to_double (&buffer[0], &value->X);
	ch_packed_float_to_double (&buffer[1], &value->Y);
	ch_packed_float_to_double (&buffer[2], &value->Z);
out:
	return ret;
}

/**
 * ch_device_queue_take_readings_xyz:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @value:		The #CdColorXYZ for a given calibration slot
 *
 * Take an XYZ fully cooked reading from the sensor.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_take_readings_xyz (ChDeviceQueue *device_queue,
				   GUsbDevice *device,
				   guint16 calibration_index,
				   CdColorXYZ *value)
{
	guint8 *buffer;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (value != NULL);

	buffer = g_new0 (guint8, sizeof(ChPackedFloat) * 3);
	ch_device_queue_add_internal (device_queue,
				     device,
				     CH_CMD_TAKE_READING_XYZ,
				     (guint8 *) &calibration_index,
				     sizeof(guint16),
				     buffer,
				     sizeof(ChPackedFloat) * 3,
				     g_free,
				     ch_device_queue_buffer_triple_xyz_cb,
				     value,
				     NULL);
}

/**
 * ch_device_queue_reset:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 *
 * Resets the device back to bootloader mode.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_reset (ChDeviceQueue *device_queue,
		       GUsbDevice *device)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_RESET,
			     NULL,
			     0,
			     NULL,
			     0);
}

/**
 * ch_device_queue_calculate_checksum:
 **/
static guint8
ch_device_queue_calculate_checksum (guint8 *data,
				    gsize len)
{
	guint8 checksum = 0xff;
	guint i;
	for (i = 0; i < len; i++)
		checksum ^= data[i];
	return checksum;
}

/**
 * ch_device_queue_write_flash:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @address:		The device EEPROM address
 * @data:		Binary data
 * @len:		The length of @data
 *
 * Write flash code to the device.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_write_flash (ChDeviceQueue *device_queue,
			     GUsbDevice *device,
			     guint16 address,
			     guint8 *data,
			     gsize len)
{
	guint16 addr_le;
	guint8 buffer_tx[64];

	/* set address, length, checksum, data */
	addr_le = GUINT16_TO_LE (address);
	memcpy (buffer_tx + 0, &addr_le, 2);
	buffer_tx[2] = len;
	buffer_tx[3] = ch_device_queue_calculate_checksum (data, len);
	memcpy (buffer_tx + 4, data, len);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_WRITE_FLASH,
			     buffer_tx,
			     len + 4,
			     NULL,
			     0);
}

/* tiny helper */
typedef struct {
	guint16		 address;
	guint8		*data;
	gsize		 len;
} ChDeviceQueueReadFlashHelper;

/**
 * ch_device_queue_buffer_read_flash_cb:
 **/
static gboolean
ch_device_queue_buffer_read_flash_cb (guint8 *output_buffer,
				      gsize output_buffer_size,
				      gpointer user_data,
				      GError **error)
{
	ChDeviceQueueReadFlashHelper *helper = (ChDeviceQueueReadFlashHelper *) user_data;
	gboolean ret = TRUE;
	guint8 expected_checksum;

	/* check buffer size */
	if (output_buffer_size != helper->len + 1) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Wrong output buffer size, expected %" G_GSIZE_FORMAT ", got %" G_GSIZE_FORMAT,
			     helper->len + 1, output_buffer_size);
		goto out;
	}

	/* verify checksum */
	expected_checksum = ch_device_queue_calculate_checksum (output_buffer + 1,
								helper->len);
	if (output_buffer[0] != expected_checksum) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Checksum @0x%04x invalid",
			     helper->address);
		goto out;
	}

	/* copy data to final location */
	memcpy (helper->data, output_buffer + 1, helper->len);
out:
	return ret;
}

/**
 * ch_device_queue_read_flash:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @address:		The device EEPROM address
 * @data:		Binary data
 * @len:		The length of @data
 *
 * Read flash code from the device.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_read_flash (ChDeviceQueue *device_queue,
			    GUsbDevice *device,
			    guint16 address,
			    guint8 *data,
			    gsize len)
{
	ChDeviceQueueReadFlashHelper *helper;
	guint16 addr_le;
	guint8 *buffer;
	guint8 buffer_tx[3];

	/* set address, length, checksum, data */
	addr_le = GUINT16_TO_LE (address);
	memcpy (buffer_tx + 0, &addr_le, 2);
	buffer_tx[2] = len;

	/* create a helper structure as the checksum needs an extra
	 * byte for the checksum */
	helper = g_new0 (ChDeviceQueueReadFlashHelper, 1);
	helper->data = data;
	helper->len = len;
	helper->address = address;

	buffer = g_new0 (guint8, len + 1);
	ch_device_queue_add_internal (device_queue,
				      device,
				      CH_CMD_READ_FLASH,
				      buffer_tx,
				      sizeof(buffer_tx),
				      buffer,
				      len + 1,
				      g_free,
				      ch_device_queue_buffer_read_flash_cb,
				      helper,
				      g_free);
}

/**
 * ch_device_queue_buffer_verify_flash_cb:
 **/
static gboolean
ch_device_queue_buffer_verify_flash_cb (guint8 *output_buffer,
					gsize output_buffer_size,
					gpointer user_data,
					GError **error)
{
	ChDeviceQueueReadFlashHelper *helper = (ChDeviceQueueReadFlashHelper *) user_data;
	gboolean ret = TRUE;
	guint8 expected_checksum;

	/* check buffer size */
	if (output_buffer_size != helper->len + 1) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Wrong output buffer size, expected %" G_GSIZE_FORMAT ", got %" G_GSIZE_FORMAT,
			     helper->len + 1, output_buffer_size);
		goto out;
	}

	/* verify checksum */
	expected_checksum = ch_device_queue_calculate_checksum (output_buffer + 1,
								helper->len);
	if (output_buffer[0] != expected_checksum) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Checksum @0x%04x invalid",
			     helper->address);
		goto out;
	}

	/* verify data */
	if (memcmp (helper->data,
		    output_buffer + 1,
		    helper->len) != 0) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "Failed to verify at @0x%04x",
			     helper->address);
		goto out;
	}
out:
	return ret;
}

static void
ch_device_queue_verify_flash_helper_destroy (gpointer data)
{
	ChDeviceQueueReadFlashHelper *helper = (ChDeviceQueueReadFlashHelper *) data;
	g_free (helper->data);
	g_free (helper);
}

/**
 * ch_device_queue_verify_flash:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @address:		The device EEPROM address
 * @data:		Binary data
 * @len:		The length of @data
 *
 * Verify flash code from the device.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_verify_flash (ChDeviceQueue *device_queue,
			      GUsbDevice *device,
			      guint16 address,
			      const guint8 *data,
			      gsize len)
{
	ChDeviceQueueReadFlashHelper *helper;
	guint16 addr_le;
	guint8 *buffer;
	guint8 buffer_tx[3];

	/* set address, length, checksum, data */
	addr_le = GUINT16_TO_LE (address);
	memcpy (buffer_tx + 0, &addr_le, 2);
	buffer_tx[2] = len;

	/* create a helper structure as the checksum needs an extra
	 * byte for the checksum */
	helper = g_new0 (ChDeviceQueueReadFlashHelper, 1);
	helper->data = g_memdup (data, len + 1);
	helper->len = len;
	helper->address = address;

	buffer = g_new0 (guint8, len + 1);
	ch_device_queue_add_internal (device_queue,
				      device,
				      CH_CMD_READ_FLASH,
				      buffer_tx,
				      sizeof(buffer_tx),
				      buffer,
				      len + 1,
				      g_free,
				      ch_device_queue_buffer_verify_flash_cb,
				      helper,
				      ch_device_queue_verify_flash_helper_destroy);
}

/**
 * ch_device_queue_erase_flash:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @address:		The device EEPROM address
 * @len:		The length of @data
 *
 * Erase program code on the device.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_erase_flash (ChDeviceQueue *device_queue,
			     GUsbDevice *device,
			     guint16 address,
			     gsize len)
{
	guint8 buffer_tx[4];
	guint16 addr_le;
	guint16 len_le;

	/* set address, length, checksum, data */
	addr_le = GUINT16_TO_LE (address);
	memcpy (buffer_tx + 0, &addr_le, 2);
	len_le = GUINT16_TO_LE (len);
	memcpy (buffer_tx + 2, &len_le, 2);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_ERASE_FLASH,
			     buffer_tx,
			     sizeof(buffer_tx),
			     NULL,
			     0);
}

/**
 * ch_device_queue_set_flash_success:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @value:		Success value
 *
 * Sets the firmware flash sucess value.
 * Be careful using this function as misuse can result in a 'bricked'
 * ColorHug device.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_set_flash_success (ChDeviceQueue *device_queue,
				   GUsbDevice *device,
				   guint8 value)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));

	/* set flash success true */
	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_FLASH_SUCCESS,
			     (guint8 *) &value, 1,
			     NULL, 0);
}

/**
 * ch_device_queue_boot_flash:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 *
 * Boots the device from bootloader to firmware mode.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_boot_flash (ChDeviceQueue *device_queue,
			    GUsbDevice *device)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));

	/* boot into new code */
	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_BOOT_FLASH,
			     NULL, 0,
			     NULL, 0);
}

/**
 * ch_device_queue_self_test:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 *
 * Performs some self tests on the device.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_self_test (ChDeviceQueue *device_queue,
			    GUsbDevice *device)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));

	/* do a really simple self test */
	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SELF_TEST,
			     NULL, 0,
			     NULL, 0);
}

/**
 * ch_device_queue_get_hardware_version:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @hw_version:		The hardware version
 *
 * Gets the hardware version.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_get_hardware_version (ChDeviceQueue *device_queue,
				      GUsbDevice *device,
				      guint8 *hw_version)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (hw_version != NULL);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_GET_HARDWARE_VERSION,
			     NULL,
			     0,
			     hw_version,
			     1);
}

/**
 * ch_device_queue_get_owner_name:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @name:		The owner name
 *
 * Gets the owner name.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_get_owner_name (ChDeviceQueue *device_queue,
				GUsbDevice *device,
				gchar *name)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (name != NULL);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_GET_OWNER_NAME,
			     NULL,
			     0,
			     (guint8 *) name,
			     sizeof(gchar) * CH_OWNER_LENGTH_MAX);
	name[CH_OWNER_LENGTH_MAX-1] = 0;
}

/**
 * ch_device_queue_set_owner_name:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @name:		The owner name
 *
 * Sets the owner name.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_set_owner_name (ChDeviceQueue *device_queue,
				GUsbDevice *device,
				const gchar *name)
{
	gchar buf[CH_OWNER_LENGTH_MAX];

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (name != NULL);

	memset(buf, 0, CH_OWNER_LENGTH_MAX);
	g_strlcpy(buf, name, CH_OWNER_LENGTH_MAX);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_OWNER_NAME,
			     (const guint8 *) buf,
			     sizeof(gchar) * CH_OWNER_LENGTH_MAX,
			     NULL,
			     0);
}

/**
 * ch_device_queue_get_owner_email:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @email:		An email address
 *
 * Gets the owner email address.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_get_owner_email (ChDeviceQueue *device_queue,
				 GUsbDevice *device,
				 gchar *email)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (email != NULL);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_GET_OWNER_EMAIL,
			     NULL,
			     0,
			     (guint8 *) email,
			     sizeof(gchar) * CH_OWNER_LENGTH_MAX);
	email[CH_OWNER_LENGTH_MAX-1] = 0;
}

/**
 * ch_device_queue_set_owner_email:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @email:		An email address
 *
 * Sets the owner email address.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_set_owner_email (ChDeviceQueue *device_queue,
				 GUsbDevice *device,
				 const gchar *email)
{
	gchar buf[CH_OWNER_LENGTH_MAX];

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (email != NULL);

	memset (buf, 0, CH_OWNER_LENGTH_MAX);
	g_strlcpy (buf, email, CH_OWNER_LENGTH_MAX);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_OWNER_EMAIL,
			     (const guint8 *) buf,
			     sizeof(gchar) * CH_OWNER_LENGTH_MAX,
			     NULL,
			     0);
}

/**
 * ch_device_queue_take_reading_array:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @reading_array:	An array of raw readings
 *
 * Get an array of raw readings in quick succession.
 *
 * NOTE: This command is available on hardware version: 1 & 2
 **/
void
ch_device_queue_take_reading_array (ChDeviceQueue *device_queue,
				    GUsbDevice *device,
				    guint8 *reading_array)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (reading_array != NULL);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_TAKE_READING_ARRAY,
			     NULL,
			     0,
			     reading_array,
			     30);
}

/**
 * ch_device_queue_get_measure_mode:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @measure_mode:	The #ChMeasureMode, e.g. %CH_MEASURE_MODE_DURATION
 *
 * Gets the measurement mode.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_get_measure_mode (ChDeviceQueue *device_queue,
				  GUsbDevice *device,
				  ChMeasureMode *measure_mode)
{
	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (measure_mode != NULL);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_GET_MEASURE_MODE,
			     NULL,
			     0,
			     (guint8 *) measure_mode,
			     1);
}

/**
 * ch_device_queue_set_measure_mode:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @measure_mode:	The #ChMeasureMode, e.g. %CH_MEASURE_MODE_DURATION
 *
 * Sets the measurement mode.
 *
 * NOTE: This command is available on hardware version: 1
 **/
void
ch_device_queue_set_measure_mode (ChDeviceQueue *device_queue,
				  GUsbDevice *device,
				  ChMeasureMode measure_mode)
{
	guint8 tmp = measure_mode;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_SET_MEASURE_MODE,
			     &tmp,
			     1,
			     NULL,
			     0);
}

/**
 * ch_device_queue_write_sram_internal:
 **/
static void
ch_device_queue_write_sram_internal (ChDeviceQueue *device_queue,
				     GUsbDevice *device,
				     guint16 address,
				     guint8 *data,
				     gsize len)
{
	guint16 addr_le;
	guint8 buffer_tx[CH_USB_HID_EP_SIZE];

	/* set address, length, checksum, data */
	addr_le = GUINT16_TO_LE (address);
	memcpy (buffer_tx + 0, &addr_le, 2);
	buffer_tx[2] = len;
	memcpy (buffer_tx + 3, data, len);

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_WRITE_SRAM,
			     buffer_tx,
			     len + 3,
			     NULL,
			     0);
}

/**
 * ch_device_queue_write_sram:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @address:		The device memory address
 * @data:		The binary data
 * @len:		Size of @data
 *
 * Writes binary data to the SRAM.
 *
 * NOTE: This command is available on hardware version: 2
 **/
void
ch_device_queue_write_sram (ChDeviceQueue *device_queue,
			    GUsbDevice *device,
			    guint16 address,
			    guint8 *data,
			    gsize len)
{
	gsize chunk_len = 60;
	guint idx = 0;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (data != NULL);
	g_return_if_fail (len > 0);

	/* write in 60 byte chunks */
	do {
		if (idx + chunk_len > len)
			chunk_len = len - idx;
		g_debug ("Writing SRAM at %04x size %" G_GSIZE_FORMAT,
			 idx, chunk_len);
		ch_device_queue_write_sram_internal (device_queue,
						     device,
						     idx,
						     data + idx,
						     chunk_len);
		idx += chunk_len;
	} while (idx < len);
}

/**
 * ch_device_queue_read_sram_internal:
 **/
static void
ch_device_queue_read_sram_internal (ChDeviceQueue *device_queue,
				    GUsbDevice *device,
				    guint16 address,
				    guint8 *data,
				    gsize len)
{
	guint16 addr_le;
	guint8 buffer_tx[3];

	/* set address, length, data */
	addr_le = GUINT16_TO_LE (address);
	memcpy (buffer_tx + 0, &addr_le, 2);
	buffer_tx[2] = len;

	ch_device_queue_add (device_queue,
			     device,
			     CH_CMD_READ_SRAM,
			     buffer_tx,
			     sizeof(buffer_tx),
			     data,
			     len);
}

/**
 * ch_device_queue_read_sram:
 * @device_queue:	A #ChDeviceQueue
 * @device:		A #GUsbDevice
 * @address:		The device memory address
 * @data:		The binary data
 * @len:		Size of @data
 *
 * Reads binary data from the SRAM.
 *
 * NOTE: This command is available on hardware version: 2
 **/
void
ch_device_queue_read_sram (ChDeviceQueue *device_queue,
			   GUsbDevice *device,
			   guint16 address,
			   guint8 *data,
			   gsize len)
{
	gsize chunk_len = 60;
	guint idx = 0;

	g_return_if_fail (CH_IS_DEVICE_QUEUE (device_queue));
	g_return_if_fail (G_USB_IS_DEVICE (device));
	g_return_if_fail (data != NULL);
	g_return_if_fail (len > 0);

	/* write in 60 byte chunks */
	do {
		if (idx + chunk_len > len)
			chunk_len = len - idx;
		g_debug ("Reading SRAM at %04x size %" G_GSIZE_FORMAT,
			 idx, chunk_len);
		ch_device_queue_read_sram_internal (device_queue,
						    device,
						    idx,
						    data + idx,
						    chunk_len);
		idx += chunk_len;
	} while (idx < len);
}

/**********************************************************************/

/**
 * ch_device_queue_class_init:
 **/
static void
ch_device_queue_class_init (ChDeviceQueueClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = ch_device_queue_finalize;

	/**
	 * ChDeviceQueueClass::device-failed:
	 * @device_queue: the #ChDeviceQueue instance that emitted the signal
	 * @device: the device that failed
	 * @error_message: the error that caused the failure
	 *
	 * The ::device-failed signal is emitted when a device has failed.
	 **/
	signals[SIGNAL_DEVICE_FAILED] =
		g_signal_new ("device-failed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ChDeviceQueueClass, device_failed),
			      NULL, NULL, g_cclosure_marshal_generic,
			      G_TYPE_NONE, 2, G_TYPE_OBJECT, G_TYPE_STRING);

	/**
	 * ChDeviceQueueClass::progress-changed:
	 * @device_queue: the #ChDeviceQueue instance that emitted the signal
	 * @percentage: the percentage complete the action is
	 * @error_message: the error that caused the failure
	 *
	 * The ::progress-changed signal is emitted when a the commands
	 * are being submitted.
	 **/
	signals[SIGNAL_PROGRESS_CHANGED] =
		g_signal_new ("progress-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ChDeviceQueueClass, progress_changed),
			      NULL, NULL, g_cclosure_marshal_generic,
			      G_TYPE_NONE, 1, G_TYPE_UINT);

	g_type_class_add_private (klass, sizeof (ChDeviceQueuePrivate));
}

/**
 * ch_device_queue_init:
 **/
static void
ch_device_queue_init (ChDeviceQueue *device_queue)
{
	device_queue->priv = CH_DEVICE_QUEUE_GET_PRIVATE (device_queue);
	device_queue->priv->data_array = g_ptr_array_new_with_free_func ((GDestroyNotify) ch_device_queue_data_free);
	device_queue->priv->devices_in_use = g_hash_table_new_full (g_str_hash,
								    g_str_equal,
								    g_free,
								    NULL);
}

/**
 * ch_device_queue_finalize:
 **/
static void
ch_device_queue_finalize (GObject *object)
{
	ChDeviceQueue *device_queue = CH_DEVICE_QUEUE (object);
	ChDeviceQueuePrivate *priv = device_queue->priv;

	g_ptr_array_unref (priv->data_array);
	g_hash_table_unref (priv->devices_in_use);

	G_OBJECT_CLASS (ch_device_queue_parent_class)->finalize (object);
}

/**
 * ch_device_queue_new:
 **/
ChDeviceQueue *
ch_device_queue_new (void)
{
	ChDeviceQueue *device_queue;
	device_queue = g_object_new (CH_TYPE_DEVICE_QUEUE, NULL);
	return CH_DEVICE_QUEUE (device_queue);
}
