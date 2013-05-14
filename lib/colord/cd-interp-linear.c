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
 * SECTION:cd-interp-linear
 * @short_description: Interpolate data using a linear method
 *
 * This object implements linear interpolation of 2D ordered data.
 */

#include "config.h"

#include <glib.h>
#include <math.h>

#include "cd-interp-linear.h"

static void	cd_interp_linear_class_init	(CdInterpLinearClass	*klass);
static void	cd_interp_linear_init		(CdInterpLinear		*interp_linear);

G_DEFINE_TYPE (CdInterpLinear, cd_interp_linear, CD_TYPE_INTERP)

/**
 * cd_interp_linear_eval:
 **/
static gdouble
cd_interp_linear_eval (CdInterp *interp, gdouble value, GError **error)
{
	const gdouble *x;
	const gdouble *y;
	gint p;
	gint size;

	/* find first point to interpolate from */
	x = &g_array_index (cd_interp_get_x (interp), gdouble, 0);
	y = &g_array_index (cd_interp_get_y (interp), gdouble, 0);
	size = cd_interp_get_y (interp)->len;
	for (p = 0; p < size - 2; p++) {
		if (x[p+1] >= value)
			break;
	}
	return y[p] + ((value - x[p]) / (x[p+1] - x[p])) * (y[p+1] - y[p]);
}

/*
 * cd_interp_linear_class_init:
 */
static void
cd_interp_linear_class_init (CdInterpLinearClass *klass)
{
	CdInterpClass *interp_class = CD_INTERP_CLASS (klass);
	interp_class->eval = cd_interp_linear_eval;
}

/**
 * cd_interp_linear_init:
 **/
static void
cd_interp_linear_init (CdInterpLinear *interp_linear)
{
}

/**
 * cd_interp_linear_new:
 *
 * Creates a new #CdInterpLinear object.
 *
 * Return value: a new CdInterp object.
 *
 * Since: 0.1.31
 **/
CdInterp *
cd_interp_linear_new (void)
{
	CdInterpLinear *interp_linear;
	interp_linear = g_object_new (CD_TYPE_INTERP_LINEAR,
				     "kind", CD_INTERP_KIND_LINEAR,
				     NULL);
	return CD_INTERP (interp_linear);
}
