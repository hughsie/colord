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

#if !defined (__COLORHUG_H_INSIDE__) && !defined (CH_COMPILATION)
#error "Only <colorhug.h> can be included directly."
#endif

#ifndef __CH_MATH_H
#define __CH_MATH_H

#include <glib.h>

#include "ch-common.h"

G_BEGIN_DECLS

/* a 32 bit struct to hold numbers from the range -32767 to +32768
 * with a precision of at least 0.000015 */
typedef union {
	struct {
		guint8	bytes[4];
	};
	struct {
		guint16	fraction;
		gint16	offset;
	};
	struct {
		gint32	raw;
	};
} ChPackedFloat;

gint32		 ch_packed_float_get_value	(const ChPackedFloat	*pf);
void		 ch_packed_float_set_value	(ChPackedFloat		*pf,
						 const gint32		 value);

void		 ch_packed_float_to_double	(const ChPackedFloat	*pf,
						 gdouble		*value);
void		 ch_double_to_packed_float	(gdouble		 value,
						 ChPackedFloat		*pf);

ChError		 ch_packed_float_add		(const ChPackedFloat	*pf1,
						 const ChPackedFloat	*pf2,
						 ChPackedFloat		*result);
ChError		 ch_packed_float_multiply	(const ChPackedFloat	*pf1,
						 const ChPackedFloat	*pf2,
						 ChPackedFloat		*result);

G_END_DECLS

#endif /* __CH_MATH_H */
