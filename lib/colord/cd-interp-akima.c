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
 * SECTION:cd-interp-akima
 * @short_description: Interpolate data using the Akima method
 *
 * This object implements Akima interpolation of 2D ordered data.
 */

#include "config.h"

#include <glib.h>
#include <math.h>

#include "cd-interp-akima.h"

static void	cd_interp_akima_class_init	(CdInterpAkimaClass	*klass);
static void	cd_interp_akima_init		(CdInterpAkima		*interp_akima);
static void	cd_interp_akima_finalize	(GObject		*object);

#define CD_INTERP_AKIMA_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_INTERP_AKIMA, CdInterpAkimaPrivate))

/**
 * CdInterpAkimaPrivate:
 *
 * Private #CdInterpAkima data
 **/
struct _CdInterpAkimaPrivate
{
	gdouble			*slope_t;	/* slope */
	gdouble			*polynom_c;	/* coefficient C */
	gdouble			*polynom_d;	/* coefficient D */
};

G_DEFINE_TYPE (CdInterpAkima, cd_interp_akima, CD_TYPE_INTERP)

/**
 * cd_interp_akima_prepare:
 **/
static gboolean
cd_interp_akima_prepare (CdInterp *interp, GError **error)
{
	CdInterpAkima *interp_akima = CD_INTERP_AKIMA (interp);
	CdInterpAkimaPrivate *priv = interp_akima->priv;
	gdouble tmp = 0.0;
	gdouble *dx;
	gdouble *dy;
	gdouble *slope_m;
	gdouble *x;
	gdouble *y;
	gint i;
	gint n;
	GArray *array_x;
	GArray *array_y;

	/* only add the points if they are going to be used */
	if (cd_interp_get_size (interp) <= 2)
		return TRUE;

	/* leading and trailing extrapolation points */
	array_x = cd_interp_get_x (interp);
	array_y = cd_interp_get_y (interp);
	g_array_prepend_val (array_x, tmp);
	g_array_prepend_val (array_x, tmp);
	g_array_prepend_val (array_y, tmp);
	g_array_prepend_val (array_y, tmp);
	g_array_append_val (array_x, tmp);
	g_array_append_val (array_x, tmp);
	g_array_append_val (array_y, tmp);
	g_array_append_val (array_y, tmp);

	x = (gdouble *) array_x->data;
	y = (gdouble *) array_y->data;
	n = cd_interp_get_size (interp) + 4;

	/* calculate Akima coefficients of the spline */
	dx = g_new0 (gdouble, n);
	dy = g_new0 (gdouble, n);
	slope_m = g_new0 (gdouble, n);
	priv->slope_t = g_new0 (gdouble, n);

	/* calc the differences and the slope_m[i]. */
	for (i = 2; i < n-3; i++) {
		dx[i] = x[i+1] - x[i]; 
		dy[i] = y[i+1] - y[i];
		slope_m[i] = dy[i] / dx[i];
	}

	/* interpolate ends */
	x[1] = x[2] + x[3] - x[4];
	dx[1] = x[2] - x[1];
	y[1] = dx[1] * (slope_m[3] - 2 * slope_m[2]) + y[2];
	dy[1] = y[2] - y[1];
	slope_m[1] = dy[1] / dx[1];

	x[0] = 2 * x[2] - x[4];
	dx[0] = x[1] - x[0];
	y[0] = dx[0] * (slope_m[2] - 2 * slope_m[1]) + y[1];
	dy[0] = y[1] - y[0];
	slope_m[0] = dy[0] / dx[0];

	x[n-2] = x[n-3] + x[n-4] - x[n-5];
	y[n-2] = (2 * slope_m[n-4] - slope_m[n-5]) * (x[n-2] - x[n-3l]) + y[n-3];

	x[n-1] = 2 * x[n-3] - x[n-5];
	y[n-1] = (2 * slope_m[n-3] - slope_m[n-4]) * (x[n-1] - x[n-2]) + y[n-2];

	/* interpolate middle */
	for (i = n-3; i < n-1; i++) {
		dx[i] = x[i+1] - x[i];
		dy[i] = y[i+1] - y[i];
		slope_m[i] = dy[i] / dx[i];
	}

	/* the first x slopes and the last y ones are extrapolated: */
	priv->slope_t[0] = 0.0;
	priv->slope_t[1] = 0.0;
	for (i = 2; i < n-2; i++) {
		gdouble num, den;
		num = fabs (slope_m[i+1] - slope_m[i]) * slope_m[i-1] + fabs (slope_m[i-1] - slope_m[i-2]) * slope_m[i];
		den = fabs (slope_m[i+1] - slope_m[i]) + fabs (slope_m[i-1] - slope_m[i-2]);
		if (fpclassify (den) != FP_ZERO)
			priv->slope_t[i] = num / den;
		else
			priv->slope_t[i] = 0.0;
	}

	/* calculate polynomial coefficients */
	priv->polynom_c = g_new0 (gdouble, n);
	priv->polynom_d = g_new0 (gdouble, n);
	for (i = 2; i < n-2; i++) {
		priv->polynom_c[i] = (3 * slope_m[i] - 2 * priv->slope_t[i] - priv->slope_t[i+1]) / dx[i];
		priv->polynom_d[i] = (priv->slope_t[i] + priv->slope_t[i+1] - 2 * slope_m[i]) / (dx[i] * dx[i]);
	}

	g_free (dx);
	g_free (dy);
	g_free (slope_m);
	return TRUE;
}

/**
 * cd_interp_akima_eval:
 **/
static gdouble
cd_interp_akima_eval (CdInterp *interp, gdouble value, GError **error)
{
	CdInterpAkima *interp_akima = CD_INTERP_AKIMA (interp);
	CdInterpAkimaPrivate *priv = interp_akima->priv;
	const gdouble *x;
	const gdouble *y;
	gdouble result;
	gdouble xd;
	gint p = 2;

	/* find first point to interpolate from */
	x = (gdouble *) cd_interp_get_x (interp)->data;
	y = (gdouble *) cd_interp_get_y (interp)->data;
	while (value >= x[p]) p++;

	/* evaluate polynomials */
	xd = value - x[p-1];
	result = y[p-1] + (priv->slope_t[p-1] + (priv->polynom_c[p-1] + priv->polynom_d[p-1] * xd) * xd) * xd;

	return result;
}

/*
 * cd_interp_akima_class_init:
 */
static void
cd_interp_akima_class_init (CdInterpAkimaClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CdInterpClass *interp_class = CD_INTERP_CLASS (klass);

	interp_class->prepare = cd_interp_akima_prepare;
	interp_class->eval = cd_interp_akima_eval;
	object_class->finalize = cd_interp_akima_finalize;

	g_type_class_add_private (klass, sizeof (CdInterpAkimaPrivate));
}

/**
 * cd_interp_akima_init:
 **/
static void
cd_interp_akima_init (CdInterpAkima *interp_akima)
{
	interp_akima->priv = CD_INTERP_AKIMA_GET_PRIVATE (interp_akima);
}

/**
 * cd_interp_akima_finalize:
 **/
static void
cd_interp_akima_finalize (GObject *object)
{
	CdInterpAkima *interp_akima = CD_INTERP_AKIMA (object);

	g_return_if_fail (CD_IS_INTERP_AKIMA (object));

	g_free (interp_akima->priv->slope_t);
	g_free (interp_akima->priv->polynom_c);
	g_free (interp_akima->priv->polynom_d);

	G_OBJECT_CLASS (cd_interp_akima_parent_class)->finalize (object);
}

/**
 * cd_interp_akima_new:
 *
 * Creates a new #CdInterpAkima object.
 *
 * Return value: a new CdInterp object.
 *
 * Since: 0.1.31
 **/
CdInterp *
cd_interp_akima_new (void)
{
	CdInterpAkima *interp_akima;
	interp_akima = g_object_new (CD_TYPE_INTERP_AKIMA,
				     "kind", CD_INTERP_KIND_AKIMA,
				     NULL);
	return CD_INTERP (interp_akima);
}
