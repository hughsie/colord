/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:cd-transform
 * @short_description: An ICC transform
 *
 * This object is a simple ICC transform that allows mapping of simple RGB
 * spaces to other simple RGB spaces using one, two or three ICC profiles.
 *
 * This object is not supposed to re-implement LCMS, and if you need anything more
 * complicated than simple RGB buffers (for instance, floating point, CMYK, BPC,
 * etc.) then you are better off using lcms2 directly.
 */

#include "config.h"

#include <glib.h>
#include <lcms2.h>

#include "cd-transform.h"

static void	cd_transform_class_init		(CdTransformClass	*klass);
static void	cd_transform_init		(CdTransform		*transform);
static void	cd_transform_finalize		(GObject		*object);

#define CD_TRANSFORM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_TRANSFORM, CdTransformPrivate))

/**
 * CdTransformPrivate:
 *
 * Private #CdTransform data
 **/
struct _CdTransformPrivate
{
	CdIcc			*input;
	CdIcc			*output;
	CdIcc			*abstract;
	CdPixelFormat		 pixel_format;
	CdRenderingIntent	 rendering_intent;
	cmsHPROFILE		 srgb;
	cmsHTRANSFORM		 lcms_transform;
};

G_DEFINE_TYPE (CdTransform, cd_transform, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_RENDERING_INTENT,
	PROP_PIXEL_FORMAT,
	PROP_INPUT,
	PROP_OUTPUT,
	PROP_ABSTRACT,
	PROP_LAST
};

/**
 * cd_transform_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.34
 **/
GQuark
cd_transform_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("cd_transform_error");
	return quark;
}

/**
 * cd_transform_invalidate:
 **/
static void
cd_transform_invalidate (CdTransform *transform)
{
	if (transform->priv->lcms_transform != NULL)
		cmsDeleteTransform (transform->priv->lcms_transform);
	transform->priv->lcms_transform = NULL;
}

/**
 * cd_transform_set_input:
 * @transform: a #CdTransform instance.
 * @icc: a #CdIcc instance or %NULL.
 *
 * Sets the input profile to use for the transform.
 *
 * Since: 0.1.34
 **/
void
cd_transform_set_input (CdTransform *transform, CdIcc *icc)
{
	g_return_if_fail (CD_IS_TRANSFORM (transform));
	g_return_if_fail (icc == NULL || CD_IS_ICC (icc));

	if (transform->priv->input != NULL)
		g_clear_object (&transform->priv->input);
	if (icc != NULL)
		transform->priv->input = g_object_ref (icc);
	cd_transform_invalidate (transform);
}

/**
 * cd_transform_get_input:
 * @transform: a #CdTransform instance.
 *
 * Gets the input profile to use for the transform.
 *
 * Return value: (transfer none): The input profile
 *
 * Since: 0.1.34
 **/
CdIcc *
cd_transform_get_input (CdTransform *transform)
{
	g_return_val_if_fail (CD_IS_TRANSFORM (transform), NULL);
	return transform->priv->input;
}

/**
 * cd_transform_set_output:
 * @transform: a #CdTransform instance.
 * @icc: a #CdIcc instance or %NULL.
 *
 * Sets the output profile to use for the transform.
 *
 * Since: 0.1.34
 **/
void
cd_transform_set_output (CdTransform *transform, CdIcc *icc)
{
	g_return_if_fail (CD_IS_TRANSFORM (transform));
	g_return_if_fail (icc == NULL || CD_IS_ICC (icc));

	if (transform->priv->output != NULL)
		g_clear_object (&transform->priv->output);
	if (icc != NULL)
		transform->priv->output = g_object_ref (icc);
	cd_transform_invalidate (transform);
}

/**
 * cd_transform_get_output:
 * @transform: a #CdTransform instance.
 *
 * Gets the input profile to use for the transform.
 *
 * Return value: (transfer none): The output profile
 *
 * Since: 0.1.34
 **/
CdIcc *
cd_transform_get_output (CdTransform *transform)
{
	g_return_val_if_fail (CD_IS_TRANSFORM (transform), NULL);
	return transform->priv->output;
}

/**
 * cd_transform_set_abstract:
 * @transform: a #CdTransform instance.
 * @icc: a #CdIcc instance or %NULL.
 *
 * Sets the abstract profile to use for the transform.
 * This is typically only needed for soft-proofing.
 *
 * Since: 0.1.34
 **/
void
cd_transform_set_abstract (CdTransform *transform, CdIcc *icc)
{
	g_return_if_fail (CD_IS_TRANSFORM (transform));
	g_return_if_fail (icc == NULL || CD_IS_ICC (icc));

	if (transform->priv->abstract != NULL)
		g_clear_object (&transform->priv->abstract);
	if (icc != NULL)
		transform->priv->abstract = g_object_ref (icc);
	cd_transform_invalidate (transform);
}

/**
 * cd_transform_get_abstract:
 * @transform: a #CdTransform instance.
 *
 * Gets the abstract profile to use for the transform.
 *
 * Return value: (transfer none): The abstract profile
 *
 * Since: 0.1.34
 **/
CdIcc *
cd_transform_get_abstract (CdTransform *transform)
{
	g_return_val_if_fail (CD_IS_TRANSFORM (transform), NULL);
	return transform->priv->abstract;
}

/**
 * cd_transform_set_format:
 * @transform: a #CdTransform instance.
 * @pixel_format: The pixel format, e.g. %CD_PIXEL_FORMAT_RGBA_8
 *
 * Sets the pixel format to use for the transform.
 *
 * Since: 0.1.34
 **/
void
cd_transform_set_format (CdTransform *transform, CdPixelFormat pixel_format)
{
	g_return_if_fail (CD_IS_TRANSFORM (transform));
	g_return_if_fail (pixel_format != CD_PIXEL_FORMAT_UNKNOWN);

	transform->priv->pixel_format = pixel_format;
	cd_transform_invalidate (transform);
}

/**
 * cd_transform_get_format:
 * @transform: a #CdTransform instance.
 *
 * Gets the pixel format to use for the transform.
 *
 * Return value: the pixel format, e.g. %CD_PIXEL_FORMAT_RGBA_8
 *
 * Since: 0.1.34
 **/
CdPixelFormat
cd_transform_get_format (CdTransform *transform)
{
	g_return_val_if_fail (CD_IS_TRANSFORM (transform), CD_PIXEL_FORMAT_UNKNOWN);
	return transform->priv->pixel_format;
}

/**
 * cd_transform_set_intent:
 * @transform: a #CdTransform instance.
 * @rendering_intent: the rendering intent, e.g. %CD_RENDERING_INTENT_PERCEPTUAL
 *
 * Sets the rendering intent to use for the transform.
 *
 * Since: 0.1.34
 **/
void
cd_transform_set_intent (CdTransform *transform, CdRenderingIntent rendering_intent)
{
	g_return_if_fail (CD_IS_TRANSFORM (transform));
	g_return_if_fail (rendering_intent != CD_RENDERING_INTENT_UNKNOWN);

	transform->priv->rendering_intent = rendering_intent;
	cd_transform_invalidate (transform);
}

/**
 * cd_transform_get_intent:
 * @transform: a #CdTransform instance.
 *
 * Gets the rendering intent to use for the transform.
 *
 * Return value: The rendering intent, e.g. %CD_RENDERING_INTENT_PERCEPTUAL
 *
 * Since: 0.1.34
 **/
CdRenderingIntent
cd_transform_get_intent (CdTransform *transform)
{
	g_return_val_if_fail (CD_IS_TRANSFORM (transform), CD_RENDERING_INTENT_UNKNOWN);
	return transform->priv->rendering_intent;
}

/* map lcms profile class to colord type */
const struct {
	guint32				lcms;
	CdPixelFormat			colord;
} map_pixel_format[] = {
	{ TYPE_RGB_8,			CD_PIXEL_FORMAT_RGB_8 },
	{ TYPE_RGBA_8,			CD_PIXEL_FORMAT_RGBA_8 },
	{ TYPE_RGB_16,			CD_PIXEL_FORMAT_RGB_16 },
	{ TYPE_RGBA_16,			CD_PIXEL_FORMAT_RGBA_16 },
	{ 0,				CD_PIXEL_FORMAT_LAST }
};

/* map lcms intent to colord type */
const struct {
	gint					lcms;
	CdRenderingIntent			colord;
} map_rendering_intent[] = {
	{ INTENT_PERCEPTUAL,		CD_RENDERING_INTENT_PERCEPTUAL },
	{ INTENT_ABSOLUTE_COLORIMETRIC,	CD_RENDERING_INTENT_ABSOLUTE_COLORIMETRIC },
	{ INTENT_RELATIVE_COLORIMETRIC,	CD_RENDERING_INTENT_RELATIVE_COLORIMETRIC },
	{ INTENT_SATURATION,		CD_RENDERING_INTENT_SATURATION },
	{ 0,				CD_RENDERING_INTENT_LAST }
};

/**
 * cd_transform_setup:
 **/
static gboolean
cd_transform_setup (CdTransform *transform, GError **error)
{
	CdTransformPrivate *priv = transform->priv;
	cmsHPROFILE profile_in;
	cmsHPROFILE profile_out;
	gboolean ret = TRUE;
	gint lcms_intent = -1;
	guint32 lcms_format = 0;
	guint i;

	/* find native rendering intent */
	for (i = 0; map_rendering_intent[i].colord != CD_RENDERING_INTENT_LAST; i++) {
		if (map_rendering_intent[i].colord == priv->rendering_intent) {
			lcms_intent = map_rendering_intent[i].lcms;
			break;
		}
	}
	g_assert (lcms_intent != -1);

	/* find native pixel format */
	for (i = 0; map_pixel_format[i].colord != CD_PIXEL_FORMAT_LAST; i++) {
		if (map_pixel_format[i].colord == priv->pixel_format) {
			lcms_format = map_pixel_format[i].lcms;
			break;
		}
	}
	g_assert (lcms_format > 0);

	/* get input profile */
	if (priv->input != NULL) {
		if (cd_icc_get_colorspace (priv->input) != CD_COLORSPACE_RGB) {
			ret = FALSE;
			g_set_error_literal (error,
					     CD_TRANSFORM_ERROR,
					     CD_TRANSFORM_ERROR_INVALID_COLORSPACE,
					     "input colorspace has to be RGB");
			goto out;
		}
		g_debug ("using input profile of %s",
			 cd_icc_get_filename (priv->input));
		profile_in = cd_icc_get_handle (priv->input);
	} else {
		g_debug ("no input profile, assume sRGB");
		profile_in = priv->srgb;
	}

	/* get output profile */
	if (priv->output != NULL) {
		if (cd_icc_get_colorspace (priv->output) != CD_COLORSPACE_RGB) {
			ret = FALSE;
			g_set_error_literal (error,
					     CD_TRANSFORM_ERROR,
					     CD_TRANSFORM_ERROR_INVALID_COLORSPACE,
					     "output colorspace has to be RGB");
			goto out;
		}
		g_debug ("using output profile of %s",
			 cd_icc_get_filename (priv->output));
		profile_out = cd_icc_get_handle (priv->output);
	} else {
		g_debug ("no output profile, assume sRGB");
		profile_out = priv->srgb;
	}

	/* get abstract profile */
	if (priv->abstract != NULL) {
		cmsHPROFILE profiles[3];

		if (cd_icc_get_colorspace (priv->abstract) != CD_COLORSPACE_LAB) {
			ret = FALSE;
			g_set_error_literal (error,
					     CD_TRANSFORM_ERROR,
					     CD_TRANSFORM_ERROR_INVALID_COLORSPACE,
					     "abstract colorspace has to be Lab");
			goto out;
		}

		/* generate a devicelink */
		profiles[0] = profile_in;
		profiles[1] = cd_icc_get_handle (priv->abstract);
		profiles[2] = profile_out;
		priv->lcms_transform = cmsCreateMultiprofileTransform (profiles,
								       3,
								       lcms_format,
								       lcms_format,
								       lcms_intent,
								       0);

	} else {
		/* create basic transform */
		priv->lcms_transform = cmsCreateTransform (profile_in,
							   lcms_format,
							   profile_out,
							   lcms_format,
							   lcms_intent,
							   0);
	}

	/* failed? */
	if (priv->lcms_transform == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_TRANSFORM_ERROR,
				     CD_TRANSFORM_ERROR_FAILED_TO_SETUP_TRANSFORM,
				     "failed to setup transform, unspecified error");
		goto out;
	}
out:
	return ret;
}

/**
 * cd_transform_process:
 * @transform: a #CdTransform instance.
 * @data_in: the data buffer to convert
 * @data_out: the data buffer to return, which can be the same as @data_in
 * @width: the width of @data_in
 * @height: the height of @data_in
 * @rowstride: the rowstride of @data_in, typically the same as @width
 * @cancellable: A %GError, or %NULL
 * @error: A %GCancellable, or %NULL
 *
 * Processes a block of data through the transform.
 * Once the transform has been setup it is cached and only re-created if any
 * of the formats, input, output or abstract profiles are changed.
 *
 * Return value: %TRUE if the pixels were successfully transformed.
 *
 * Since: 0.1.34
 **/
gboolean
cd_transform_process (CdTransform *transform,
		      gpointer data_in,
		      gpointer data_out,
		      guint width,
		      guint height,
		      guint rowstride,
		      GCancellable *cancellable,
		      GError **error)
{
	CdTransformPrivate *priv = transform->priv;
	gboolean ret = TRUE;
	guint i;
	guint8 *p_in;
	guint8 *p_out;

	g_return_val_if_fail (CD_IS_TRANSFORM (transform), FALSE);
	g_return_val_if_fail (data_in != NULL, FALSE);
	g_return_val_if_fail (data_out != NULL, FALSE);
	g_return_val_if_fail (width != 0, FALSE);
	g_return_val_if_fail (height != 0, FALSE);
	g_return_val_if_fail (rowstride != 0, FALSE);

	/* check stuff that should have been set */
	if (priv->rendering_intent == CD_RENDERING_INTENT_UNKNOWN) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_TRANSFORM_ERROR,
				     CD_TRANSFORM_ERROR_FAILED_TO_SETUP_TRANSFORM,
				     "rendering intent not set");
		goto out;
	}
	if (priv->pixel_format == CD_PIXEL_FORMAT_UNKNOWN) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_TRANSFORM_ERROR,
				     CD_TRANSFORM_ERROR_FAILED_TO_SETUP_TRANSFORM,
				     "pixel format not set");
		goto out;
	}

	/* setup the transform if required */
	if (priv->lcms_transform == NULL) {
		ret = cd_transform_setup (transform, error);
		if (!ret)
			goto out;
	}

	/* do conversion */
	p_in = data_in;
	p_out = data_out;
	for (i = 0; i < height; i++) {
		cmsDoTransform (priv->lcms_transform, p_in, p_out, width);
		p_in += rowstride;
		p_out += rowstride;
	}
out:
	return ret;
}

/**
 * cd_transform_get_property:
 **/
static void
cd_transform_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	CdTransform *transform = CD_TRANSFORM (object);
	CdTransformPrivate *priv = transform->priv;

	switch (prop_id) {
	case PROP_INPUT:
		g_value_set_object (value, priv->input);
		break;
	case PROP_OUTPUT:
		g_value_set_object (value, priv->output);
		break;
	case PROP_ABSTRACT:
		g_value_set_object (value, priv->abstract);
		break;
	case PROP_RENDERING_INTENT:
		g_value_set_uint (value, priv->rendering_intent);
		break;
	case PROP_PIXEL_FORMAT:
		g_value_set_uint (value, priv->pixel_format);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * cd_transform_set_property:
 **/
static void
cd_transform_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CdTransform *transform = CD_TRANSFORM (object);
	switch (prop_id) {
	case PROP_RENDERING_INTENT:
		cd_transform_set_intent (transform, g_value_get_uint (value));
		break;
	case PROP_PIXEL_FORMAT:
		cd_transform_set_format (transform, g_value_get_uint (value));
		break;
	case PROP_INPUT:
		cd_transform_set_input (transform, g_value_get_object (value));
		break;
	case PROP_OUTPUT:
		cd_transform_set_output (transform, g_value_get_object (value));
		break;
	case PROP_ABSTRACT:
		cd_transform_set_abstract (transform, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * cd_transform_class_init:
 */
static void
cd_transform_class_init (CdTransformClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = cd_transform_finalize;
	object_class->get_property = cd_transform_get_property;
	object_class->set_property = cd_transform_set_property;

	/**
	 * CdTransform: rendering-intent:
	 */
	pspec = g_param_spec_uint ("rendering-intent", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_RENDERING_INTENT, pspec);

	/**
	 * CdTransform: pixel-format:
	 */
	pspec = g_param_spec_uint ("pixel-format", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_PIXEL_FORMAT, pspec);

	/**
	 * CdTransform: input:
	 */
	pspec = g_param_spec_object ("input", NULL, NULL,
				     CD_TYPE_ICC,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_INPUT, pspec);

	/**
	 * CdTransform: output:
	 */
	pspec = g_param_spec_object ("output", NULL, NULL,
				     CD_TYPE_ICC,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_OUTPUT, pspec);

	/**
	 * CdTransform: abstract:
	 */
	pspec = g_param_spec_object ("abstract", NULL, NULL,
				     CD_TYPE_ICC,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ABSTRACT, pspec);

	g_type_class_add_private (klass, sizeof (CdTransformPrivate));
}

/**
 * cd_transform_init:
 */
static void
cd_transform_init (CdTransform *transform)
{
	transform->priv = CD_TRANSFORM_GET_PRIVATE (transform);
	transform->priv->rendering_intent = CD_RENDERING_INTENT_UNKNOWN;
	transform->priv->pixel_format = CD_PIXEL_FORMAT_UNKNOWN;
	transform->priv->srgb = cmsCreate_sRGBProfile ();
}

/**
 * cd_transform_finalize:
 */
static void
cd_transform_finalize (GObject *object)
{
	CdTransform *transform = CD_TRANSFORM (object);
	CdTransformPrivate *priv = transform->priv;

	cmsCloseProfile (transform->priv->srgb);
	if (priv->input != NULL)
		g_object_unref (priv->input);
	if (priv->output != NULL)
		g_object_unref (priv->output);
	if (priv->abstract != NULL)
		g_object_unref (priv->abstract);
	if (priv->lcms_transform != NULL)
		cmsDeleteTransform (priv->lcms_transform);

	G_OBJECT_CLASS (cd_transform_parent_class)->finalize (object);
}

/**
 * cd_transform_new:
 *
 * Creates a new #CdTransform object.
 *
 * Return value: a new CdTransform object.
 *
 * Since: 0.1.34
 **/
CdTransform *
cd_transform_new (void)
{
	CdTransform *transform;
	transform = g_object_new (CD_TYPE_TRANSFORM, NULL);
	return CD_TRANSFORM (transform);
}
