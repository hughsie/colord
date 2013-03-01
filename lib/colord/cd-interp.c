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
 * SECTION:cd-interp
 * @short_description: Interpolate 2D data using a variety of methods
 *
 * This object is a superclass of different interpolation methods.
 */

#include "config.h"

#include <glib.h>

#include "cd-interp.h"

static void	cd_interp_class_init	(CdInterpClass	*klass);
static void	cd_interp_init		(CdInterp	*interp);
static void	cd_interp_finalize	(GObject	*object);

#define CD_INTERP_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_INTERP, CdInterpPrivate))

/**
 * CdInterpPrivate:
 *
 * Private #CdInterp data
 **/
struct _CdInterpPrivate
{
	CdInterpKind		 kind;
	GArray			*x;
	GArray			*y;
	gboolean		 prepared;
	guint			 size;
};

enum {
	PROP_0,
	PROP_KIND,
	PROP_LAST
};

G_DEFINE_TYPE (CdInterp, cd_interp, G_TYPE_OBJECT)

/**
 * cd_interp_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.31
 **/
GQuark
cd_interp_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("cd_interp_error");
	return quark;
}

/**
 * cd_interp_get_x:
 * @interp: a #CdInterp instance.
 *
 * Gets the X data used the interpolation.
 *
 * NOTE: this is only guaranteed to return the data inserted by
 * cd_interp_insert() *BEFORE* calling cd_interp_prepare().
 *
 * After prepating the #CdInterp this data should be considered an
 * implementation detail.
 *
 * Return value: (transfer none) (element-type gdouble): The X co-ordinate data
 *
 * Since: 0.1.31
 **/
GArray *
cd_interp_get_x (CdInterp *interp)
{
	g_return_val_if_fail (CD_IS_INTERP (interp), NULL);
	return interp->priv->x;
}

/**
 * cd_interp_get_y:
 * @interp: a #CdInterp instance.
 *
 * Gets the Y data used the interpolation.
 *
 * NOTE: this is only guaranteed to return the data inserted by
 * cd_interp_insert() *BEFORE* calling cd_interp_prepare().
 *
 * After prepating the #CdInterp this data should be considered an
 * implementation detail.
 *
 * Return value: (transfer none) (element-type gdouble): The Y co-ordinate data
 *
 * Since: 0.1.31
 **/
GArray *
cd_interp_get_y (CdInterp *interp)
{
	g_return_val_if_fail (CD_IS_INTERP (interp), NULL);
	return interp->priv->y;
}

/**
 * cd_interp_get_size:
 * @interp: a #CdInterp instance.
 *
 * Gets the number of items of data added with cd_interp_insert().
 *
 * NOTE: This method has to be called after cd_interp_prepare() and it returns
 * the number of items originally in the array.
 * If you need the updated private size then just use cd_interp_get_x() and
 * read the size from that.
 *
 * Return value: The original array size
 *
 * Since: 0.1.31
 **/
guint
cd_interp_get_size (CdInterp *interp)
{
	g_return_val_if_fail (CD_IS_INTERP (interp), 0);
	return interp->priv->size;
}

/**
 * cd_interp_insert:
 * @interp: a #CdInterp instance.
 * @x: X data
 * @y: Y data
 *
 * Inserts data to be interpolated.
 *
 * Since: 0.1.31
 **/
void
cd_interp_insert (CdInterp *interp, gdouble x, gdouble y)
{
	g_return_if_fail (CD_IS_INTERP (interp));
	g_return_if_fail (!interp->priv->prepared);
	g_array_append_val (interp->priv->x, x);
	g_array_append_val (interp->priv->y, y);
}

/**
 * cd_interp_prepare:
 * @interp: a #CdInterp instance.
 *
 * Prepares the data set so that cd_interp_eval() can be used.
 *
 * Return value: %TRUE for success
 *
 * Since: 0.1.31
 **/
gboolean
cd_interp_prepare (CdInterp *interp, GError **error)
{
	CdInterpClass *klass = CD_INTERP_GET_CLASS (interp);
	gboolean ret = TRUE;

	g_return_val_if_fail (CD_IS_INTERP (interp), FALSE);
	g_return_val_if_fail (!interp->priv->prepared, FALSE);

	/* save number of data points before the klass method messes around
	 * with them */
	interp->priv->size = interp->priv->x->len;
	if (interp->priv->size == 0) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_INTERP_ERROR,
				     CD_INTERP_ERROR_FAILED,
				     "no data to prepare");
		goto out;
	}

	/* call if support */
	if (klass->prepare != NULL) {
		ret = klass->prepare (interp, error);
		if (!ret)
			goto out;
	}

	/* success */
	interp->priv->prepared = TRUE;
out:
	return ret;
}

/**
 * cd_interp_eval:
 * @interp: a #CdInterp instance.
 * @value: The X co-ordinate
 * @error: a #GError or %NULL
 *
 * Evaluate the interpolation function at a specific point.
 * You must have called cd_interp_insert() and cd_interp_prepare() before
 * calling this method.
 *
 * Return value: The Y co-ordinate, or -1 for error
 *
 * Since: 0.1.31
 **/
gdouble
cd_interp_eval (CdInterp *interp, gdouble value, GError **error)
{
	CdInterpClass *klass = CD_INTERP_GET_CLASS (interp);
	CdInterpPrivate *priv = interp->priv;
	const gdouble *x;
	const gdouble *y;
	gdouble result = -1.0f;

	g_return_val_if_fail (CD_IS_INTERP (interp), -1);
	g_return_val_if_fail (priv->prepared, -1);

	/* no points */
	if (priv->size == 0)
		goto out;

	/* only one point */
	x = (gdouble *) cd_interp_get_x (interp)->data;
	y = (gdouble *) cd_interp_get_y (interp)->data;
	if (priv->size == 1) {
		result = y[0];
		goto out;
	}

	/* only 2 points */
	if (priv->size == 2) {
		gdouble dx, dy, m;
		dx = x[1] - x[0];
		dy = y[1] - y[0];
		m = dy / dx;
		result = y[0] + m * value;
		goto out;
	}

	/* no support */
	if (klass->eval == NULL) {
		g_set_error_literal (error,
				     CD_INTERP_ERROR,
				     CD_INTERP_ERROR_FAILED,
				     "no superclass");
		goto out;
	}

	/* call the klass function */
	result = klass->eval (interp, value, error);
out:
	return result;
}

/**
 * cd_interp_get_kind:
 * @interp: a #CdInterp instance.
 *
 * Gets the kind of INTERP file.
 *
 * Return value: a #CdInterpKind, e.g %CD_INTERP_KIND_AKIMA.
 *
 * Since: 0.1.31
 **/
CdInterpKind
cd_interp_get_kind (CdInterp *interp)
{
	g_return_val_if_fail (CD_IS_INTERP (interp), CD_INTERP_KIND_LAST);
	return interp->priv->kind;
}

/**
 * cd_interp_kind_to_string:
 **/
const gchar *
cd_interp_kind_to_string (CdInterpKind kind)
{
	if (kind == CD_INTERP_KIND_LINEAR)
		return "linear";
	if (kind == CD_INTERP_KIND_AKIMA)
		return "akima";
	return "unknown";
}

/**
 * cd_interp_get_property:
 **/
static void
cd_interp_get_property (GObject *object,
			guint prop_id,
			GValue *value,
			GParamSpec *pspec)
{
	CdInterp *interp = CD_INTERP (object);

	switch (prop_id) {
	case PROP_KIND:
		g_value_set_uint (value, interp->priv->kind);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


/**
 * cd_interp_set_property:
 **/
static void
cd_interp_set_property (GObject *object,
			guint prop_id,
			const GValue *value,
			GParamSpec *pspec)
{
	CdInterp *interp = CD_INTERP (object);

	switch (prop_id) {
	case PROP_KIND:
		interp->priv->kind = g_value_get_uint (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/*
 * cd_interp_class_init:
 */
static void
cd_interp_class_init (CdInterpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = cd_interp_get_property;
	object_class->set_property = cd_interp_set_property;
	object_class->finalize = cd_interp_finalize;

	/**
	 * CdInterp:kind:
	 *
	 * The kind of interpolation.
	 *
	 * Since: 0.1.20
	 **/
	g_object_class_install_property (object_class,
					 PROP_KIND,
					 g_param_spec_uint ("kind",
							    NULL, NULL,
							    0, G_MAXUINT, 0,
							    G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (CdInterpPrivate));
}

/**
 * cd_interp_init:
 **/
static void
cd_interp_init (CdInterp *interp)
{
	interp->priv = CD_INTERP_GET_PRIVATE (interp);
	interp->priv->x = g_array_new (FALSE, TRUE, sizeof (gdouble));
	interp->priv->y = g_array_new (FALSE, TRUE, sizeof (gdouble));
}

/**
 * cd_interp_finalize:
 **/
static void
cd_interp_finalize (GObject *object)
{
	CdInterp *interp = CD_INTERP (object);

	g_return_if_fail (CD_IS_INTERP (object));

	g_array_unref (interp->priv->x);
	g_array_unref (interp->priv->y);

	G_OBJECT_CLASS (cd_interp_parent_class)->finalize (object);
}
