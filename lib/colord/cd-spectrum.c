/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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
 * SECTION:cd-spectrum
 * @short_description: A single set of spectral values
 *
 * Functions to manipulate spectral values.
 */

#include "config.h"

#include <math.h>
#include <glib-object.h>

#include "cd-color.h"
#include "cd-interp-linear.h"
#include "cd-spectrum.h"

/* this is private */
struct _CdSpectrum {
	gchar			*id;
	gdouble			 start;
	gdouble			 end;
	GArray			*data;
};

/**
 * cd_spectrum_dup:
 * @spectrum: a #CdSpectrum instance.
 *
 * Since: 1.1.6
 **/
CdSpectrum *
cd_spectrum_dup (const CdSpectrum *spectrum)
{
	CdSpectrum *dest;
	g_return_val_if_fail (spectrum != NULL, NULL);
	dest = cd_spectrum_new ();
	dest->id = g_strdup (spectrum->id);
	dest->start = spectrum->start;
	dest->end = spectrum->end;
	dest->data = g_array_ref (spectrum->data);
	return dest;
}

/**
 * cd_spectrum_get_id:
 * @spectrum: a #CdSpectrum instance.
 *
 * Gets the spectral data.
 *
 * Return value: the textual ID of the sample
 *
 * Since: 1.1.6
 **/
const gchar *
cd_spectrum_get_id (const CdSpectrum *spectrum)
{
	g_return_val_if_fail (spectrum != NULL, NULL);
	return spectrum->id;
}

/**
 * cd_spectrum_get_value:
 * @spectrum: a #CdSpectrum instance.
 * @idx: an index into the data
 *
 * Gets the spectrum data at a specified index.
 *
 * Return value: spectral data value, or -1 for invalid
 *
 * Since: 1.1.6
 **/
gdouble
cd_spectrum_get_value (const CdSpectrum *spectrum, guint idx)
{
	g_return_val_if_fail (spectrum != NULL, -1.0f);
	g_return_val_if_fail (idx < spectrum->data->len, -1.0f);
	return g_array_index (spectrum->data, gdouble, idx);
}

/**
 * cd_spectrum_get_wavelength:
 * @spectrum: a #CdSpectrum instance.
 * @idx: an index into the data
 *
 * Gets the wavelenth that corresponds to the specified index.
 *
 * Return value: wavelenth value in nm, or -1 for invalid
 *
 * Since: 1.1.6
 **/
gdouble
cd_spectrum_get_wavelength (const CdSpectrum *spectrum, guint idx)
{
	gdouble step;

	g_return_val_if_fail (spectrum != NULL, -1.0f);
	g_return_val_if_fail (idx < spectrum->data->len, -1.0f);

	step = (spectrum->end - spectrum->start) / (spectrum->data->len - 1);
	return spectrum->start + (step * (gdouble) idx);
}

/**
 * cd_spectrum_get_size:
 * @spectrum: a #CdSpectrum instance.
 *
 * Gets the size of the spectrum data.
 *
 * Return value: number of data items in this spectrum
 *
 * Since: 1.1.6
 **/
guint
cd_spectrum_get_size (const CdSpectrum *spectrum)
{
	g_return_val_if_fail (spectrum != NULL, G_MAXUINT);
	return spectrum->data->len;
}

/**
 * cd_spectrum_get_data:
 * @spectrum: a #CdSpectrum instance.
 *
 * Gets the spectral data.
 *
 * Return value: (transfer none) (element-type gdouble): spectral data
 *
 * Since: 1.1.6
 **/
GArray *
cd_spectrum_get_data (const CdSpectrum *spectrum)
{
	g_return_val_if_fail (spectrum != NULL, NULL);
	return spectrum->data;
}

/**
 * cd_spectrum_get_start:
 * @spectrum: a #CdSpectrum instance.
 *
 * Gets the start value of the spectral data.
 *
 * Return value: the value in nm
 *
 * Since: 1.1.6
 **/
gdouble
cd_spectrum_get_start (const CdSpectrum *spectrum)
{
	g_return_val_if_fail (spectrum != NULL, 0.0f);
	return spectrum->start;
}

/**
 * cd_spectrum_get_end:
 * @spectrum: a #CdSpectrum instance.
 *
 * Gets the end value of the spectral data.
 *
 * Return value: the value in nm
 *
 * Since: 1.1.6
 **/
gdouble
cd_spectrum_get_end (const CdSpectrum *spectrum)
{
	g_return_val_if_fail (spectrum != NULL, 0.0f);
	return spectrum->end;
}

/**
 * cd_spectrum_get_type:
 *
 * Gets a specific type.
 *
 * Return value: a #GType
 *
 * Since: 1.1.6
 **/
GType
cd_spectrum_get_type (void)
{
	static GType type_id = 0;
	if (!type_id)
		type_id = g_boxed_type_register_static ("CdSpectrum",
							(GBoxedCopyFunc) cd_spectrum_dup,
							(GBoxedFreeFunc) cd_spectrum_free);
	return type_id;
}

/**
 * cd_spectrum_new:
 *
 * Allocates a spectrum.
 *
 * Return value: A newly allocated #CdSpectrum object
 *
 * Since: 1.1.6
 **/
CdSpectrum *
cd_spectrum_new (void)
{
	CdSpectrum *spectrum;
	spectrum = g_slice_new0 (CdSpectrum);
	spectrum->data = g_array_new (FALSE, FALSE, sizeof (gdouble));
	return spectrum;
}

/**
 * cd_spectrum_sized_new:
 * @reserved_size: the future size of the spectrum
 *
 * Allocates a spectrum with a preallocated size.
 *
 * Return value: A newly allocated #CdSpectrum object
 *
 * Since: 1.1.6
 **/
CdSpectrum *
cd_spectrum_sized_new (guint reserved_size)
{
	CdSpectrum *spectrum;
	spectrum = g_slice_new0 (CdSpectrum);
	spectrum->data = g_array_sized_new (FALSE, FALSE, sizeof (gdouble), reserved_size);
	return spectrum;
}

/**
 * cd_spectrum_add_value:
 * @spectrum: the spectrum
 *
 * Adds a value in nm to the spectrum.
 *
 * Since: 1.1.6
 **/
void
cd_spectrum_add_value (CdSpectrum *spectrum, gdouble data)
{
	g_return_if_fail (spectrum != NULL);
	g_array_append_val (spectrum->data, data);
}

/**
 * cd_spectrum_free:
 * @spectrum: the spectrum
 *
 * Deallocates a color spectrum.
 *
 * Since: 1.1.6
 **/
void
cd_spectrum_free (CdSpectrum *spectrum)
{
	g_return_if_fail (spectrum != NULL);
	g_free (spectrum->id);
	g_array_unref (spectrum->data);
	g_slice_free (CdSpectrum, spectrum);
}

/**
 * cd_spectrum_set_id:
 * @spectrum: the destination spectrum
 * @id: component id
 *
 * Sets a spectrum id.
 *
 * Since: 1.1.6
 **/
void
cd_spectrum_set_id (CdSpectrum *spectrum, const gchar *id)
{
	g_return_if_fail (spectrum != NULL);
	g_return_if_fail (id != NULL);
	g_free (spectrum->id);
	spectrum->id = g_strdup (id);
}

/**
 * cd_spectrum_set_data:
 * @spectrum: the destination spectrum
 * @value: (element-type gdouble): component value
 *
 * Sets the spectrum data.
 *
 * Since: 1.1.6
 **/
void
cd_spectrum_set_data (CdSpectrum *spectrum, GArray *value)
{
	g_return_if_fail (spectrum != NULL);
	g_return_if_fail (value != NULL);
	g_array_unref (spectrum->data);
	spectrum->data = g_array_ref (value);
}

/**
 * cd_spectrum_set_start:
 * @spectrum: a #CdSpectrum instance.
 * @start: the start value of the spectral data
 *
 * Set the start value of the spectal data in nm.
 *
 * Since: 1.1.6
 **/
void
cd_spectrum_set_start (CdSpectrum *spectrum, gdouble start)
{
	g_return_if_fail (spectrum != NULL);
	spectrum->start = start;
}

/**
 * cd_spectrum_set_end:
 * @spectrum: a #CdSpectrum instance.
 * @end: the end value of the spectral data
 *
 * Set the end value of the spectal data in nm.
 *
 * Since: 1.1.6
 **/
void
cd_spectrum_set_end (CdSpectrum *spectrum, gdouble end)
{
	g_return_if_fail (spectrum != NULL);
	spectrum->end = end;
}

/**
 * cd_spectrum_get_value_for_nm:
 * @spectrum: a #CdSpectrum instance.
 * @wavelength: the wavelength in nm
 *
 * Gets the value from the spectral data for a given wavelength.
 *
 * Return value: the value for the wavelength
 *
 * Since: 1.1.6
 **/
gdouble
cd_spectrum_get_value_for_nm (const CdSpectrum *spectrum, gdouble wavelength)
{
	CdInterp *interp;
	gboolean ret;
	gdouble val = -1.0;
	guint i;

	/* add all the data points */
	interp = cd_interp_linear_new ();
	for (i = 0; i < cd_spectrum_get_size (spectrum); i++) {
		cd_interp_insert (interp,
				  cd_spectrum_get_wavelength (spectrum, i),
				  cd_spectrum_get_value (spectrum, i));
	}

	/* get the interpolated value */
	ret = cd_interp_prepare (interp, NULL);
	if (!ret)
		goto out;
	val = cd_interp_eval (interp, wavelength, NULL);
out:
	return val;
}
