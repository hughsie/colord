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

/**
 * This object contains all the low level logic for the DTP94 hardware.
 */

#include "config.h"

#include <glib-object.h>
#include <gusb.h>
#include <string.h>

#include "cd-sensor.h"
#include "cd-sensor-dtp94-private.h"

typedef struct
{
	gboolean			 done_startup;
	GUsbContext			*usb_ctx;
	GUsbDevice			*device;
	GUsbDeviceList			*device_list;
} CdSensorDtp94Private;

/* async state for the sensor readings */
typedef struct {
	gboolean			 ret;
	CdColorXYZ			*sample;
	gulong				 cancellable_id;
	GCancellable			*cancellable;
	GSimpleAsyncResult		*res;
	CdSensor			*sensor;
	CdSensorCap			 current_cap;
} CdSensorAsyncState;

#define DTP94_CONTROL_MESSAGE_TIMEOUT	50000 /* ms */
#define DTP94_MAX_READ_RETRIES		5

static CdSensorDtp94Private *
cd_sensor_dtp94_get_private (CdSensor *sensor)
{
	return g_object_get_data (G_OBJECT (sensor), "priv");
}

static gboolean
cd_sensor_dtp94_send_data (CdSensorDtp94Private *priv,
			   const guint8 *request, gsize request_len,
			   guint8 *reply, gsize reply_len,
			   gsize *reply_read, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (request != NULL, FALSE);
	g_return_val_if_fail (request_len != 0, FALSE);
	g_return_val_if_fail (reply != NULL, FALSE);
	g_return_val_if_fail (reply_len != 0, FALSE);
	g_return_val_if_fail (reply_read != NULL, FALSE);

	/* request data from device */
	cd_sensor_debug_data (CD_SENSOR_DEBUG_MODE_REQUEST,
			      request, request_len);
	ret = g_usb_device_interrupt_transfer (priv->device,
					       0x2,
					       (guint8 *) request,
					       request_len,
					       NULL,
					       DTP94_CONTROL_MESSAGE_TIMEOUT,
					       NULL,
					       error);
	if (!ret)
		goto out;

	/* get sync response */
	ret = g_usb_device_interrupt_transfer (priv->device,
					       0x81,
					       (guint8 *) reply,
					       reply_len,
					       reply_read,
					       DTP94_CONTROL_MESSAGE_TIMEOUT,
					       NULL,
					       error);
	if (!ret)
		goto out;
	if (reply_read == 0) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_INTERNAL,
				     "failed to get data from device");
		goto out;
	}
	cd_sensor_debug_data (CD_SENSOR_DEBUG_MODE_RESPONSE,
			      reply, *reply_read);
out:
	return ret;
}

static gboolean
cd_sensor_dtp94_cm2 (CdSensorDtp94Private *priv,
		     const gchar *command,
		     GError **error)
{
	gboolean ret;
	gsize reply_read;
	guint8 buffer[128];
	guint8 rc;
	guint command_len;

	/* sent command raw */
	command_len = strlen (command);
	ret = cd_sensor_dtp94_send_data (priv,
					 (const guint8 *) command,
					 command_len,
					 buffer,
					 sizeof (buffer),
					 &reply_read,
					 error);
	if (!ret)
		goto out;

	/* device busy */
	rc = cd_sensor_dtp94_rc_parse (buffer, reply_read);
	if (rc == CD_SENSOR_DTP92_RC_BAD_COMMAND) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_SENSOR_ERROR,
				     CD_SENSOR_ERROR_NO_DATA,
				     "device busy");
		goto out;
	}

	/* no success */
	if (rc != CD_SENSOR_DTP92_RC_OK) {
		ret = FALSE;
		buffer[reply_read] = '\0';
		g_set_error (error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_INTERNAL,
			     "unexpected response from device: %s [%s]",
			     (const gchar *) buffer,
			     cd_sensor_dtp94_rc_to_string (rc));
		goto out;
	}
out:
	return ret;
}

static gboolean
cd_sensor_dtp94_cmd (CdSensorDtp94Private *priv,
		     const gchar *command,
		     GError **error)
{
	gboolean ret;
	GError *error_local = NULL;
	guint error_cnt = 0;

	/* repeat until the device is ready */
	for (error_cnt = 0; ret != TRUE; error_cnt++) {
		ret = cd_sensor_dtp94_cm2 (priv, command, &error_local);
		if (!ret) {
			if (error_cnt < DTP94_MAX_READ_RETRIES &&
			    g_error_matches (error_local,
					     CD_SENSOR_ERROR,
					     CD_SENSOR_ERROR_NO_DATA)) {
				g_debug ("ignoring %s", error_local->message);
				g_clear_error (&error_local);
				continue;
			}
			g_propagate_error (error, error_local);
			break;
		}
	};
	return ret;
}

static void
cd_sensor_dtp94_get_sample_state_finish (CdSensorAsyncState *state,
					 const GError *error)
{
	/* set result to temp memory location */
	if (state->ret) {
		g_simple_async_result_set_op_res_gpointer (state->res,
							   state->sample,
							   cd_color_xyz_free);
	} else {
		g_simple_async_result_set_from_error (state->res, error);
	}

	/* deallocate */
	if (state->cancellable != NULL) {
		g_cancellable_disconnect (state->cancellable, state->cancellable_id);
		g_object_unref (state->cancellable);
	}

	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_slice_free (CdSensorAsyncState, state);
}

static void
cd_sensor_dtp94_cancellable_cancel_cb (GCancellable *cancellable,
				      CdSensorAsyncState *state)
{
	g_warning ("cancelled dtp94");
}

static void
cd_sensor_dtp94_sample_thread_cb (GSimpleAsyncResult *res,
				 GObject *object,
				 GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorAsyncState *state = (CdSensorAsyncState *) g_object_get_data (G_OBJECT (cancellable), "state");
	CdSensorDtp94Private *priv = cd_sensor_dtp94_get_private (sensor);
	gboolean ret = FALSE;
	GError *error = NULL;
	guint8 buffer[128];
	gchar *tmp;
	gsize reply_read;

	/* set hardware support */
	switch (state->current_cap) {
	case CD_SENSOR_CAP_CRT:
	case CD_SENSOR_CAP_PLASMA:
		/* CRT = 01 */
		ret = cd_sensor_dtp94_cmd (priv, "0116CF\r", &error);
		break;
	case CD_SENSOR_CAP_LCD:
		/* LCD = 02 */
		ret = cd_sensor_dtp94_cmd (priv, "0216CF\r", &error);
		break;
	default:
		g_set_error (&error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_NO_SUPPORT,
			     "DTP94 cannot measure in %s mode",
			     cd_sensor_cap_to_string (state->current_cap));
		break;
	}
	if (!ret) {
		cd_sensor_dtp94_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* get sample */
	ret = cd_sensor_dtp94_send_data (priv,
					 (const guint8 *) "RM\r", 3,
					 buffer, sizeof (buffer),
					 &reply_read,
					 &error);
	if (!ret) {
		cd_sensor_dtp94_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}
	tmp = g_strstr_len ((const gchar *) buffer, reply_read, "\r");
	if (tmp == NULL || memcmp (tmp + 1, "<00>", 4) != 0) {
		ret = FALSE;
		buffer[reply_read] = '\0';
		g_set_error (&error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_INTERNAL,
			     "unexpected response from device: %s",
			     (const gchar *) buffer);
		cd_sensor_dtp94_get_sample_state_finish (state, error);
		g_error_free (error);
		goto out;
	}

	/* format is raw ASCII with fixed formatting:
	 * 'X     10.29	Y     10.33	Z      4.65\u000d<00>' */
	tmp = (gchar *) buffer;
	g_strdelimit (tmp, "\t\r", '\0');

	/* success */
	state->ret = TRUE;
	state->sample = cd_color_xyz_new ();
	cd_color_xyz_set (state->sample,
			  g_ascii_strtod (tmp + 1, NULL),
			  g_ascii_strtod (tmp + 13, NULL),
			  g_ascii_strtod (tmp + 25, NULL));
	cd_sensor_dtp94_get_sample_state_finish (state, NULL);
out:
	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
}

void
cd_sensor_get_sample_async (CdSensor *sensor,
			    CdSensorCap cap,
			    GCancellable *cancellable,
			    GAsyncReadyCallback callback,
			    gpointer user_data)
{
	CdSensorAsyncState *state;
	GCancellable *tmp;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* save state */
	state = g_slice_new0 (CdSensorAsyncState);
	state->res = g_simple_async_result_new (G_OBJECT (sensor),
						callback,
						user_data,
						cd_sensor_get_sample_async);
	state->sensor = g_object_ref (sensor);
	state->current_cap = cap;
	if (cancellable != NULL) {
		state->cancellable = g_object_ref (cancellable);
		state->cancellable_id = g_cancellable_connect (cancellable, G_CALLBACK (cd_sensor_dtp94_cancellable_cancel_cb), state, NULL);
	}

	/* run in a thread */
	tmp = g_cancellable_new ();
	g_object_set_data (G_OBJECT (tmp), "state", state);
	g_simple_async_result_run_in_thread (G_SIMPLE_ASYNC_RESULT (state->res),
					     cd_sensor_dtp94_sample_thread_cb,
					     0,
					     (GCancellable*) tmp);
	g_object_unref (tmp);
}

CdColorXYZ *
cd_sensor_get_sample_finish (CdSensor *sensor,
			     GAsyncResult *res,
			     GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), NULL);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* failed */
	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	/* grab detail */
	return cd_color_xyz_dup (g_simple_async_result_get_op_res_gpointer (simple));
}

static gboolean
cd_sensor_dtp94_startup (CdSensor *sensor, GError **error)
{
	CdSensorDtp94Private *priv = cd_sensor_dtp94_get_private (sensor);
	gboolean ret;
	gchar *tmp;
	gsize reply_read;
	guint8 buffer[128];

	/* reset device */
	ret = cd_sensor_dtp94_cmd (priv, "0PR\r", error);
	if (!ret)
		goto out;

	/* reset device again */
	ret = cd_sensor_dtp94_cmd (priv, "0PR\r", error);
	if (!ret)
		goto out;

	/* set color data separator to '\t' */
	ret = cd_sensor_dtp94_cmd (priv, "0207CF\r", error);
	if (!ret)
		goto out;

	/* set delimeter to CR */
	ret = cd_sensor_dtp94_cmd (priv, "0008CF\r", error);
	if (!ret)
		goto out;

	/* set extra digit resolution */
	ret = cd_sensor_dtp94_cmd (priv, "010ACF\r", error);
	if (!ret)
		goto out;

	/* no black point subtraction */
	ret = cd_sensor_dtp94_cmd (priv, "0019CF\r", error);
	if (!ret)
		goto out;

	/* set to factory calibration */
	ret = cd_sensor_dtp94_cmd (priv, "EFC\r", error);
	if (!ret)
		goto out;

	/* unknown command */
	ret = cd_sensor_dtp94_cmd (priv, "0117CF\r", error);
	if (!ret)
		goto out;

	/* get serial number */
	ret = cd_sensor_dtp94_send_data (priv,
					 (const guint8 *) "SV\r", 3,
					 buffer, sizeof (buffer),
					 &reply_read,
					 error);
	if (!ret)
		goto out;
	tmp = g_strstr_len ((const gchar *) buffer, reply_read, "\r");
	if (tmp == NULL || memcmp (tmp + 1, "<00>", 4) != 0) {
		ret = FALSE;
		buffer[reply_read] = '\0';
		g_set_error (error,
			     CD_SENSOR_ERROR,
			     CD_SENSOR_ERROR_INTERNAL,
			     "unexpected response from device: %s",
			     (const gchar *) buffer);
		goto out;
	}
	tmp[0] = '\0';
	cd_sensor_set_serial (sensor, (const gchar *) buffer);
out:
	return ret;
}

static void
cd_sensor_dtp94_lock_thread_cb (GSimpleAsyncResult *res,
			        GObject *object,
			        GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorDtp94Private *priv = cd_sensor_dtp94_get_private (sensor);
	gboolean ret = FALSE;
	GError *error = NULL;

	/* try to find the device */
	priv->device = g_usb_device_list_find_by_vid_pid (priv->device_list,
							  CD_SENSOR_DTP94_VENDOR_ID,
							  CD_SENSOR_DTP94_PRODUCT_ID,
							  &error);
	if (priv->device == NULL) {
		g_simple_async_result_set_from_error (res,
						      error);
		g_error_free (error);
		goto out;
	}

	/* load device */
	ret = g_usb_device_open (priv->device, &error);
	if (!ret) {
		cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
		g_simple_async_result_set_from_error (res,
						      error);
		g_error_free (error);
		goto out;
	}
	ret = g_usb_device_set_configuration (priv->device,
					      0x01,
					      &error);
	if (!ret) {
		cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
		g_simple_async_result_set_from_error (res,
						      error);
		g_error_free (error);
		goto out;
	}
	ret = g_usb_device_claim_interface (priv->device,
					    0x00,
					    G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
					    &error);
	if (!ret) {
		cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
		g_simple_async_result_set_from_error (res,
						      error);
		g_error_free (error);
		goto out;
	}

	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_STARTING);

	/* do startup sequence */
	ret = cd_sensor_dtp94_startup (sensor, &error);
	if (!ret) {
		cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
		g_simple_async_result_set_from_error (res,
						      error);
		g_error_free (error);
		goto out;
	}
out:
	/* set state */
	cd_sensor_set_state (sensor, CD_SENSOR_STATE_IDLE);
}

void
cd_sensor_lock_async (CdSensor *sensor,
		      GCancellable *cancellable,
		      GAsyncReadyCallback callback,
		      gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* run in a thread */
	res = g_simple_async_result_new (G_OBJECT (sensor),
					 callback,
					 user_data,
					 cd_sensor_lock_async);
	g_simple_async_result_run_in_thread (res,
					     cd_sensor_dtp94_lock_thread_cb,
					     0,
					     cancellable);
	g_object_unref (res);
}

gboolean
cd_sensor_lock_finish (CdSensor *sensor,
		       GAsyncResult *res,
		       GError **error)
{
	GSimpleAsyncResult *simple;
	gboolean ret = TRUE;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error)) {
		ret = FALSE;
		goto out;
	}
out:
	return ret;
}

static void
cd_sensor_unlock_thread_cb (GSimpleAsyncResult *res,
			    GObject *object,
			    GCancellable *cancellable)
{
	CdSensor *sensor = CD_SENSOR (object);
	CdSensorDtp94Private *priv = cd_sensor_dtp94_get_private (sensor);
	gboolean ret = FALSE;
	GError *error = NULL;

	/* close */
	if (priv->device != NULL) {
		ret = g_usb_device_close (priv->device, &error);
		if (!ret) {
			g_simple_async_result_set_from_error (res, error);
			g_error_free (error);
			goto out;
		}

		/* clear */
		g_object_unref (priv->device);
		priv->device = NULL;
	}
out:
	return;
}

void
cd_sensor_unlock_async (CdSensor *sensor,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data)
{
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_SENSOR (sensor));

	/* run in a thread */
	res = g_simple_async_result_new (G_OBJECT (sensor),
					 callback,
					 user_data,
					 cd_sensor_unlock_async);
	g_simple_async_result_run_in_thread (res,
					     cd_sensor_unlock_thread_cb,
					     0,
					     cancellable);
	g_object_unref (res);
}

gboolean
cd_sensor_unlock_finish (CdSensor *sensor,
			 GAsyncResult *res,
			 GError **error)
{
	GSimpleAsyncResult *simple;
	gboolean ret = TRUE;

	g_return_val_if_fail (CD_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error)) {
		ret = FALSE;
		goto out;
	}
out:
	return ret;
}

gboolean
cd_sensor_dump_device (CdSensor *sensor, GString *data, GError **error)
{
	g_string_append_printf (data, "dtp94-dump-version:%i\n", 1);
	return TRUE;
}

static void
cd_sensor_unref_private (CdSensorDtp94Private *priv)
{
	g_object_unref (priv->usb_ctx);
	g_object_unref (priv->device_list);
	g_free (priv);
}

gboolean
cd_sensor_coldplug (CdSensor *sensor, GError **error)
{
	CdSensorDtp94Private *priv;
	guint64 caps = cd_bitfield_from_enums (CD_SENSOR_CAP_LCD,
					       CD_SENSOR_CAP_CRT, -1);
	g_object_set (sensor,
		      "native", TRUE,
		      "kind", CD_SENSOR_KIND_DTP94,
		      "caps", caps,
		      NULL);
	/* create private data */
	priv = g_new0 (CdSensorDtp94Private, 1);
	g_object_set_data_full (G_OBJECT (sensor), "priv", priv,
				(GDestroyNotify) cd_sensor_unref_private);
	priv->usb_ctx = g_usb_context_new (NULL);
	priv->device_list = g_usb_device_list_new (priv->usb_ctx);
	return TRUE;
}
