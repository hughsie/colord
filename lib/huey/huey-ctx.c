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

#include "config.h"

#include <glib.h>
#include <lcms2.h>
#include <stdlib.h>

#include "huey-ctx.h"
#include "huey-device.h"
#include "huey-enum.h"

static void	huey_ctx_class_init	(HueyCtxClass	*klass);
static void	huey_ctx_init		(HueyCtx	*ctx);
static void	huey_ctx_finalize	(GObject	*object);

#define HUEY_CTX_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), HUEY_TYPE_CTX, HueyCtxPrivate))

#define HUEY_CONTROL_MESSAGE_TIMEOUT	50000 /* ms */
#define HUEY_MAX_READ_RETRIES		5

/* The CY7C63001 is paired with a 6.00Mhz crystal */
#define HUEY_CLOCK_FREQUENCY		6e6

/* It takes 6 clock pulses to process a single 16bit increment (INC)
 * instruction and check for the carry so this is the fastest a loop
 * can be processed. */
#define HUEY_POLL_FREQUENCY		1e6

/* Picked out of thin air, just to try to match reality...
 * I have no idea why we need to do this, although it probably
 * indicates we doing something wrong. */
#define HUEY_XYZ_POST_MULTIPLY_FACTOR	3.428

/**
 * HueyCtxPrivate:
 *
 * Private #HueyCtx data
 **/
struct _HueyCtxPrivate
{
	CdMat3x3		 calibration_crt;
	CdMat3x3		 calibration_lcd;
	CdVec3			 dark_offset;
	gchar			*unlock_string;
	gfloat			 calibration_value;
	GUsbDevice		*device;
};

enum {
	PROP_0,
	PROP_DEVICE,
	PROP_LAST
};

G_DEFINE_TYPE (HueyCtx, huey_ctx, G_TYPE_OBJECT)

/**
 * huey_ctx_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.0
 **/
GQuark
huey_ctx_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark) {
		quark = g_quark_from_static_string ("huey_ctx_error");
	}
	return quark;
}


/**
 * huey_ctx_get_device:
 *
 * Since: 0.1.29
 **/
GUsbDevice *
huey_ctx_get_device (HueyCtx *ctx)
{
	g_return_val_if_fail (HUEY_IS_CTX (ctx), NULL);
	return ctx->priv->device;
}

/**
 * huey_ctx_set_device:
 *
 * Since: 0.1.29
 **/
void
huey_ctx_set_device (HueyCtx *ctx, GUsbDevice *device)
{
	g_return_if_fail (HUEY_IS_CTX (ctx));
	ctx->priv->device = g_object_ref (device);
}

/**
 * huey_ctx_setup:
 *
 * Since: 0.1.29
 **/
gboolean
huey_ctx_setup (HueyCtx *ctx, GError **error)
{
	gboolean ret;
	HueyCtxPrivate *priv = ctx->priv;

	g_return_val_if_fail (HUEY_IS_CTX (ctx), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* get matrix */
	cd_mat33_clear (&priv->calibration_lcd);
	ret = huey_device_read_register_matrix (priv->device,
						HUEY_EEPROM_ADDR_CALIBRATION_DATA_LCD,
						&priv->calibration_lcd,
						error);
	if (!ret)
		goto out;
	g_debug ("device calibration LCD: %s",
		 cd_mat33_to_string (&priv->calibration_lcd));

	/* get another matrix, although this one is different... */
	cd_mat33_clear (&priv->calibration_crt);
	ret = huey_device_read_register_matrix (priv->device,
						HUEY_EEPROM_ADDR_CALIBRATION_DATA_CRT,
						&priv->calibration_crt,
						error);
	if (!ret)
		goto out;
	g_debug ("device calibration CRT: %s",
		 cd_mat33_to_string (&priv->calibration_crt));

	/* this number is different on all three hueys */
	ret = huey_device_read_register_float (priv->device,
					       HUEY_EEPROM_ADDR_AMBIENT_CALIB_VALUE,
					       &priv->calibration_value,
					       error);
	if (!ret)
		goto out;

	/* this vector changes between sensor 1 and 3 */
	ret = huey_device_read_register_vector (priv->device,
						HUEY_EEPROM_ADDR_DARK_OFFSET,
						&priv->dark_offset,
						error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * huey_ctx_get_calibration_lcd:
 *
 * Since: 0.1.29
 **/
const CdMat3x3 *
huey_ctx_get_calibration_lcd (HueyCtx *ctx)
{
	g_return_val_if_fail (HUEY_IS_CTX (ctx), NULL);
	return &ctx->priv->calibration_lcd;
}

/**
 * huey_ctx_get_calibration_crt:
 *
 * Since: 0.1.29
 **/
const CdMat3x3 *
huey_ctx_get_calibration_crt (HueyCtx *ctx)
{
	g_return_val_if_fail (HUEY_IS_CTX (ctx), NULL);
	return &ctx->priv->calibration_crt;
}

/**
 * huey_ctx_get_calibration_value:
 *
 * Since: 0.1.29
 **/
gfloat
huey_ctx_get_calibration_value (HueyCtx *ctx)
{
	g_return_val_if_fail (HUEY_IS_CTX (ctx), -1);
	return ctx->priv->calibration_value;
}

/**
 * huey_ctx_get_dark_offset:
 *
 * Since: 0.1.29
 **/
const CdVec3 *
huey_ctx_get_dark_offset (HueyCtx *ctx)
{
	g_return_val_if_fail (HUEY_IS_CTX (ctx), NULL);
	return &ctx->priv->dark_offset;
}

/**
 * huey_ctx_get_unlock_string:
 *
 * Since: 0.1.29
 **/
const gchar *
huey_ctx_get_unlock_string (HueyCtx *ctx)
{
	g_return_val_if_fail (HUEY_IS_CTX (ctx), NULL);
	return ctx->priv->unlock_string;
}

typedef struct {
	guint16	R;
	guint16	G;
	guint16	B;
} HueyCtxMultiplier;

typedef struct {
	guint32	R;
	guint32	G;
	guint32	B;
} HueyCtxDeviceRaw;

static gboolean
huey_ctx_sample_for_threshold (HueyCtx *ctx,
			       HueyCtxMultiplier *threshold,
			       HueyCtxDeviceRaw *raw,
			       GError **error)
{
	guint8 request[] = { HUEY_CMD_SENSOR_MEASURE_RGB,
			     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	guint8 reply[8];
	gboolean ret;
	gsize reply_read;

	/* these are 16 bit gain values */
	cd_buffer_write_uint16_be (request + 1, threshold->R);
	cd_buffer_write_uint16_be (request + 3, threshold->G);
	cd_buffer_write_uint16_be (request + 5, threshold->B);

	/* measure, and get red */
	ret = huey_device_send_data (ctx->priv->device,
				     request, 8,
				     reply, 8,
				     &reply_read,
				     error);
	if (!ret)
		goto out;

	/* get value */
	raw->R = cd_buffer_read_uint32_be (reply+2);

	/* get green */
	request[0] = HUEY_CMD_READ_GREEN;
	ret = huey_device_send_data (ctx->priv->device,
				     request, 8,
				     reply, 8,
				     &reply_read,
				     error);
	if (!ret)
		goto out;

	/* get value */
	raw->G = cd_buffer_read_uint32_be (reply+2);

	/* get blue */
	request[0] = HUEY_CMD_READ_BLUE;
	ret = huey_device_send_data (ctx->priv->device,
				     request, 8,
				     reply, 8,
				     &reply_read,
				     error);
	if (!ret)
		goto out;

	/* get value */
	raw->B = cd_buffer_read_uint32_be (reply+2);
out:
	return ret;
}

/**
 * huey_ctx_convert_device_RGB_to_XYZ:
 *
 * / X \   ( / R \    / c a l \ )
 * | Y | = ( | G |  * | m a t | ) x post_scale
 * \ Z /   ( \ B /    \ l c d / )
 *
 **/
static void
huey_ctx_convert_device_RGB_to_XYZ (CdColorRGB *src,
				    CdColorXYZ *dest,
				    CdMat3x3 *calibration,
				    gdouble post_scale)
{
	CdVec3 *result;

	/* convolve */
	result = (CdVec3 *) dest;
	cd_mat33_vector_multiply (calibration, (CdVec3 *) src, result);

	/* post-multiply */
	cd_vec3_scalar_multiply (result,
				 post_scale,
				 result);
}


/**
 * huey_ctx_take_sample:
 *
 * Since: 0.1.29
 **/
CdColorXYZ *
huey_ctx_take_sample (HueyCtx *ctx, CdSensorCap cap, GError **error)
{
	CdColorRGB values;
	CdColorXYZ color_result;
	CdColorXYZ *result = NULL;
	CdMat3x3 *device_calibration;
	CdVec3 *temp;
	gboolean ret = FALSE;
	HueyCtxDeviceRaw color_native;
	HueyCtxMultiplier multiplier;

	g_return_val_if_fail (HUEY_IS_CTX (ctx), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* no hardware support */
	if (cap == CD_SENSOR_CAP_PROJECTOR) {
		g_set_error_literal (error,
				     HUEY_CTX_ERROR,
				     HUEY_CTX_ERROR_NO_SUPPORT,
				     "Huey cannot measure in projector mode");
		goto out;
	}

	/* set this to one value for a quick approximate value */
	multiplier.R = 1;
	multiplier.G = 1;
	multiplier.B = 1;
	ret = huey_ctx_sample_for_threshold (ctx,
					     &multiplier,
					     &color_native,
					     error);
	if (!ret)
		goto out;
	g_debug ("initial values: red=%i, green=%i, blue=%i",
		 color_native.R, color_native.G, color_native.B);

	/* try to fill the 16 bit register for accuracy */
	multiplier.R = HUEY_POLL_FREQUENCY / color_native.R;
	multiplier.G = HUEY_POLL_FREQUENCY / color_native.G;
	multiplier.B = HUEY_POLL_FREQUENCY / color_native.B;

	/* don't allow a value of zero */
	if (multiplier.R == 0)
		multiplier.R = 1;
	if (multiplier.G == 0)
		multiplier.G = 1;
	if (multiplier.B == 0)
		multiplier.B = 1;
	g_debug ("using multiplier factor: red=%i, green=%i, blue=%i",
		 multiplier.R, multiplier.G, multiplier.B);
	ret = huey_ctx_sample_for_threshold (ctx,
					     &multiplier,
					     &color_native,
					     error);
	if (!ret)
		goto out;
	g_debug ("raw values: red=%i, green=%i, blue=%i",
		 color_native.R, color_native.G, color_native.B);

	/* get DeviceRGB values */
	values.R = (gdouble) multiplier.R * 0.5f * HUEY_POLL_FREQUENCY / ((gdouble) color_native.R);
	values.G = (gdouble) multiplier.G * 0.5f * HUEY_POLL_FREQUENCY / ((gdouble) color_native.G);
	values.B = (gdouble) multiplier.B * 0.5f * HUEY_POLL_FREQUENCY / ((gdouble) color_native.B);
	g_debug ("scaled values: red=%0.6lf, green=%0.6lf, blue=%0.6lf",
		 values.R, values.G, values.B);

	/* remove dark offset */
	temp = (CdVec3*) &values;
	cd_vec3_subtract (temp,
			  &ctx->priv->dark_offset,
			  temp);

	g_debug ("dark offset values: red=%0.6lf, green=%0.6lf, blue=%0.6lf",
		 values.R, values.G, values.B);

	/* negative values don't make sense (device needs recalibration) */
	if (values.R < 0.0f)
		values.R = 0.0f;
	if (values.G < 0.0f)
		values.G = 0.0f;
	if (values.B < 0.0f)
		values.B = 0.0f;

	/* we use different calibration matrices for each output type */
	switch (cap) {
	case CD_SENSOR_CAP_CRT:
	case CD_SENSOR_CAP_PLASMA:
		g_debug ("using CRT calibration matrix");
		device_calibration = &ctx->priv->calibration_crt;
		break;
	default:
		g_debug ("using LCD calibration matrix");
		device_calibration = &ctx->priv->calibration_lcd;
		break;
	}

	/* convert from device RGB to XYZ */
	huey_ctx_convert_device_RGB_to_XYZ (&values,
					    &color_result,
					    device_calibration,
					    HUEY_XYZ_POST_MULTIPLY_FACTOR);
	g_debug ("finished values: red=%0.6lf, green=%0.6lf, blue=%0.6lf",
		 color_result.X, color_result.Y, color_result.Z);

	/* save result */
	result = cd_color_xyz_dup (&color_result);
out:
	return result;
}

/**
 * huey_ctx_get_property:
 **/
static void
huey_ctx_get_property (GObject *object,
		       guint prop_id,
		       GValue *value,
		       GParamSpec *pspec)
{
	HueyCtx *ctx = HUEY_CTX (object);

	switch (prop_id) {
	case PROP_DEVICE:
		g_value_set_object (value, ctx->priv->device);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


/**
 * huey_ctx_set_property:
 **/
static void
huey_ctx_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	HueyCtx *ctx = HUEY_CTX (object);

	switch (prop_id) {
	case PROP_DEVICE:
		ctx->priv->device = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/*
 * huey_ctx_class_init:
 */
static void
huey_ctx_class_init (HueyCtxClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = huey_ctx_get_property;
	object_class->set_property = huey_ctx_set_property;
	object_class->finalize = huey_ctx_finalize;

	/**
	 * HueyCtx:device:
	 *
	 * Since: 0.1.29
	 **/
	g_object_class_install_property (object_class,
					 PROP_DEVICE,
					 g_param_spec_object ("device",
							      NULL, NULL,
							      G_USB_TYPE_DEVICE,
							      G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (HueyCtxPrivate));
}

/*
 * huey_ctx_init:
 */
static void
huey_ctx_init (HueyCtx *ctx)
{
	ctx->priv = HUEY_CTX_GET_PRIVATE (ctx);

	cd_mat33_clear (&ctx->priv->calibration_lcd);
	cd_mat33_clear (&ctx->priv->calibration_crt);

	/* ensure the remote errors are registered */
	huey_ctx_error_quark ();
}

/**
 * huey_ctx_finalize:
 **/
static void
huey_ctx_finalize (GObject *object)
{
	HueyCtx *ctx = HUEY_CTX (object);

	g_return_if_fail (HUEY_IS_CTX (object));

	g_free (ctx->priv->unlock_string);

	G_OBJECT_CLASS (huey_ctx_parent_class)->finalize (object);
}

/**
 * huey_ctx_new:
 *
 * Creates a new #HueyCtx object.
 *
 * Return value: a new HueyCtx object.
 *
 * Since: 0.1.29
 **/
HueyCtx *
huey_ctx_new (void)
{
	HueyCtx *ctx;
	ctx = g_object_new (HUEY_TYPE_CTX, NULL);
	return HUEY_CTX (ctx);
}
