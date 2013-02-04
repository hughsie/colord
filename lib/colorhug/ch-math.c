/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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
#include <math.h>

#include "ch-math.h"
#include "ch-common.h"

/**
 * ch_packed_float_get_value:
 *
 * @pf: A %ChPackedFloat
 *
 * Return value: packed value to host byte order
 */
gint32
ch_packed_float_get_value (const ChPackedFloat *pf)
{
	return (pf->bytes[3] << 24) |
	       (pf->bytes[2] << 16) |
	       (pf->bytes[1] << 8) |
	       (pf->bytes[0]);
}

/**
 * ch_packed_float_set_value:
 *
 * @pf: A %ChPackedFloat
 * @value: a value in packed format in host byte order.
 *
 * Stores value in host byte order into packed format for device.
 */
void
ch_packed_float_set_value (ChPackedFloat *pf, const gint32 value)
{
	pf->bytes[0] = (value & 0xff);
	pf->bytes[1] = ((value >> 8) & 0xff);
	pf->bytes[2] = ((value >> 16) & 0xff);
	pf->bytes[3] = ((value >> 24) & 0xff);
}

/**
 * ch_packed_float_to_double:
 *
 * @pf: A %ChPackedFloat
 * @value: a value in IEEE floating point format
 *
 * Converts a packed float to a double.
 **/
void
ch_packed_float_to_double (const ChPackedFloat *pf, gdouble *value)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (pf != NULL);
	*value = ch_packed_float_get_value (pf) / (gdouble) 0x10000;
}

/**
 * ch_double_to_packed_float:
 *
 * @pf: A %ChPackedFloat
 * @value: a value in IEEE floating point format
 *
 * Converts a double number to a packed float.
 **/
void
ch_double_to_packed_float (gdouble value, ChPackedFloat *pf)
{
	g_return_if_fail (pf != NULL);
	g_return_if_fail (value <= 0x8000);
	g_return_if_fail (value >= -0x8000);
	ch_packed_float_set_value (pf, value * (gdouble) 0x10000);
}

/**
 * ch_packed_float_add:
 *
 * @pf1: A %ChPackedFloat
 * @pf1: A %ChPackedFloat
 * @result: A %ChPackedFloat
 *
 * Adds two packed floats together using only integer maths.
 *
 * Return value: an error code
 **/
ChError
ch_packed_float_add (const ChPackedFloat *pf1,
		     const ChPackedFloat *pf2,
		     ChPackedFloat *result)
{
	gint32 pf1_tmp;
	gint32 pf2_tmp;

	g_return_val_if_fail (pf1 != NULL, CH_ERROR_INVALID_VALUE);
	g_return_val_if_fail (pf2 != NULL, CH_ERROR_INVALID_VALUE);
	g_return_val_if_fail (result != NULL, CH_ERROR_INVALID_VALUE);

	/* check overflow */
	pf1_tmp = ch_packed_float_get_value (pf1) / 0x10000;
	pf2_tmp = ch_packed_float_get_value (pf2) / 0x10000;
	if (pf1_tmp + pf2_tmp > 0x8000)
		return CH_ERROR_OVERFLOW_ADDITION;

	/* do the proper result */
	ch_packed_float_set_value (result,
				   ch_packed_float_get_value (pf1) +
				   ch_packed_float_get_value (pf2));
	return CH_ERROR_NONE;
}

/**
 * ch_packed_float_multiply:
 *
 * @pf1: A %ChPackedFloat
 * @pf1: A %ChPackedFloat
 * @result: A %ChPackedFloat
 *
 * Multiplies two packed floats together using only integer maths.
 *
 * Return value: an error code
 **/
ChError
ch_packed_float_multiply (const ChPackedFloat *pf1,
			  const ChPackedFloat *pf2,
			  ChPackedFloat *result)
{
	ChPackedFloat pf1_tmp;
	ChPackedFloat pf2_tmp;
	gint32 tmp;

	g_return_val_if_fail (pf1 != NULL, CH_ERROR_INVALID_VALUE);
	g_return_val_if_fail (pf2 != NULL, CH_ERROR_INVALID_VALUE);
	g_return_val_if_fail (result != NULL, CH_ERROR_INVALID_VALUE);

	/* make positive */
	pf1_tmp.raw = ABS(ch_packed_float_get_value (pf1));
	pf2_tmp.raw = ABS(ch_packed_float_get_value (pf2));

	/* check for overflow */
	if (pf1_tmp.offset > 0 &&
	    0x8000 / pf1_tmp.offset < pf2_tmp.offset)
		return CH_ERROR_OVERFLOW_MULTIPLY;

	/* do long multiplication on each 16 bit part */
	tmp = ((guint32) pf1_tmp.fraction *
	       (guint32) pf2_tmp.fraction) / 0x10000;
	tmp += ((guint32) pf1_tmp.offset *
		(guint32) pf2_tmp.offset) * 0x10000;
	tmp += (guint32) pf1_tmp.fraction *
	       (guint32) pf2_tmp.offset;
	tmp += (guint32) pf1_tmp.offset *
	       (guint32) pf2_tmp.fraction;

	/* correct sign bit */
	if ((pf1->raw < 0) ^ (pf2->raw < 0))
		tmp = -tmp;
	ch_packed_float_set_value (result, tmp);
	return CH_ERROR_NONE;
}
