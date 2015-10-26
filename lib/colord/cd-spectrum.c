/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Richard Hughes <richard@hughsie.com>
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
	guint			 reserved_size;
	gchar			*id;
	gdouble			 start;
	gdouble			 end;
	gdouble			 norm;
	gdouble			 wavelength_cal[3];
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
	gdouble tmp;
	guint i;

	g_return_val_if_fail (spectrum != NULL, NULL);

	dest = cd_spectrum_new ();
	dest->id = g_strdup (spectrum->id);
	dest->start = spectrum->start;
	dest->end = spectrum->end;
	dest->norm = spectrum->norm;
	for (i = 0; i < spectrum->data->len; i++) {
		tmp = cd_spectrum_get_value_raw (spectrum, i);
		cd_spectrum_add_value (dest, tmp);
	}
	for (i = 0; i < 3; i++)
		dest->wavelength_cal[i] = spectrum->wavelength_cal[i];
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
	return g_array_index (spectrum->data, gdouble, idx) * spectrum->norm;
}

/**
 * cd_spectrum_get_value_max:
 * @spectrum: a #CdSpectrum instance.
 *
 * Gets the largest normalised value in the spectrum.
 *
 * Since: 1.3.1
 **/
gdouble
cd_spectrum_get_value_max (const CdSpectrum *spectrum)
{
	gdouble max = 0.f;
	guint i;
	for (i = 0; i < cd_spectrum_get_size (spectrum); i++)
		max = MAX (max, cd_spectrum_get_value (spectrum, i));
	return max;
}

/**
 * cd_spectrum_get_value_min:
 * @spectrum: a #CdSpectrum instance.
 *
 * Gets the smallest normalised value in the spectrum.
 *
 * Since: 1.3.1
 **/
gdouble
cd_spectrum_get_value_min (const CdSpectrum *spectrum)
{
	gdouble min = G_MAXDOUBLE;
	guint i;
	for (i = 0; i < cd_spectrum_get_size (spectrum); i++)
		min = MIN (min, cd_spectrum_get_value (spectrum, i));
	return min;
}

/**
 * cd_spectrum_set_value:
 * @spectrum: a #CdSpectrum instance.
 * @idx: an index into the data
 * @data: a data value
 *
 * Overwrites the spectrum data at a specified index.
 *
 * Since: 1.2.6
 **/
void
cd_spectrum_set_value (CdSpectrum *spectrum, guint idx, gdouble data)
{
	g_return_if_fail (spectrum != NULL);
	g_return_if_fail (idx < spectrum->data->len);
	g_array_index (spectrum->data, gdouble, idx) = data / spectrum->norm;
}

/**
 * cd_spectrum_get_value_raw:
 * @spectrum: a #CdSpectrum instance.
 * @idx: an index into the data
 *
 * Gets the spectrum data at a specified index, without any normalization
 * applied. Most people should use cd_spectrum_get_value() instead.
 *
 * Return value: spectral data value, or -1 for invalid
 *
 * Since: 1.2.6
 **/
gdouble
cd_spectrum_get_value_raw (const CdSpectrum *spectrum, guint idx)
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
	g_return_val_if_fail (spectrum != NULL, -1.0f);

	/* fall back to the old method */
	if (spectrum->wavelength_cal[0] < 0) {
		gdouble step;
		guint number_points;
		/* if we used cd_spectrum_size_new() and there is no data we can infer
		 * the wavelenth based on the declared initial size */
		if (spectrum->reserved_size > 0)
			number_points = spectrum->reserved_size;
		else
			number_points = spectrum->data->len;
		step = (spectrum->end - spectrum->start) / (number_points - 1);
		return spectrum->start + (step * (gdouble) idx);
	}

	/* use wavelength_cal to work out wavelength */
	return spectrum->start +
		spectrum->wavelength_cal[0] * (gdouble) idx +
		spectrum->wavelength_cal[1] * pow (idx, 2) +
		spectrum->wavelength_cal[2] * pow (idx, 3);
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
 * NOTE: This is not normalized
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
 * cd_spectrum_get_norm:
 * @spectrum: a #CdSpectrum instance.
 *
 * Gets the normalization value of the spectral data.
 * NOTE: This affects every value in the spectrum.
 *
 * Return value: the value
 *
 * Since: 1.1.6
 **/
gdouble
cd_spectrum_get_norm (const CdSpectrum *spectrum)
{
	g_return_val_if_fail (spectrum != NULL, 0.0f);
	return spectrum->norm;
}

/**
 * cd_spectrum_get_resolution:
 * @spectrum: a #CdSpectrum instance.
 *
 * Gets the divisor of the spectra, for instance a .
 *
 * Return value: the value
 *
 * Since: 1.2.6
 **/
gdouble
cd_spectrum_get_resolution (const CdSpectrum *spectrum)
{
	g_return_val_if_fail (spectrum != NULL, 0.0f);
	return (spectrum->end - spectrum->start) / (gdouble) spectrum->data->len;
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
	spectrum->norm = 1.f;
	spectrum->data = g_array_new (FALSE, FALSE, sizeof (gdouble));
	spectrum->wavelength_cal[0] = -1.f;
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
	spectrum->norm = 1.f;
	spectrum->reserved_size = reserved_size;
	spectrum->data = g_array_sized_new (FALSE, FALSE, sizeof (gdouble), reserved_size);
	spectrum->wavelength_cal[0] = -1.f;
	return spectrum;
}

/**
 * cd_spectrum_planckian_new_full:
 * @temperature: the temperature in Kelvin
 * @start: the new spectrum start
 * @end: the new spectrum end
 * @resolution: the resolution to use when resampling
 *
 * Allocates a Planckian spectrum at a specific temperature.
 *
 * Return value: A newly allocated #CdSpectrum object
 *
 * Since: 1.3.1
 **/
CdSpectrum *
cd_spectrum_planckian_new_full (gdouble temperature,
				gdouble start,
				gdouble end,
				gdouble resolution)
{
	CdSpectrum *s = NULL;
	const gdouble c1 = 3.74183e-16;	/* 2pi * h * c^2 */
	const gdouble c2 = 1.4388e-2;	/* h * c / k */
	gdouble wl;
	gdouble norm;
	gdouble tmp;
	guint i;

	/* sanity check */
	if (temperature < 1.0 || temperature > 1e6)
		return NULL;

	/* create spectrum with 1nm resolution */
	s = cd_spectrum_sized_new (531);
	s->id = g_strdup_printf ("Planckian@%.0fK", temperature);
	cd_spectrum_set_start (s, start);
	cd_spectrum_set_end (s, end);

	/* see http://www.create.uwe.ac.uk/ardtalks/Schanda_paper.pdf, page 42 */
	wl = 560 * 1e-9;
	norm = 0.01 * (c1 * pow (wl, -5.0)) / (exp (c2 / (wl * temperature)) - 1.0);
	for (i = 0; i < s->reserved_size; i++) {
		wl = cd_spectrum_get_wavelength (s, i) * 1e-9;
		tmp = (c1 * pow (wl, -5.0)) / (exp (c2 / (wl * temperature)) - 1.0);
		cd_spectrum_add_value (s, tmp / norm);
	}
	return s;
}

/**
 * cd_spectrum_planckian_new:
 * @temperature: the temperature in Kelvin
 *
 * Allocates a Planckian spectrum at a specific temperature.
 *
 * Return value: A newly allocated #CdSpectrum object
 *
 * Since: 1.1.6
 **/
CdSpectrum *
cd_spectrum_planckian_new (gdouble temperature)
{
	return cd_spectrum_planckian_new_full (temperature, 300, 830, 1);
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
	if (spectrum == NULL)
		return;
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
 * If there is already spectral data, the wavelength calibration will
 * also be set automatically.
 *
 * Since: 1.1.6
 **/
void
cd_spectrum_set_end (CdSpectrum *spectrum, gdouble end)
{
	g_return_if_fail (spectrum != NULL);

	/* calculate the calibration co-efficients */
	if (spectrum->data->len > 1) {
		spectrum->wavelength_cal[0] = (end - spectrum->start) /
						(spectrum->data->len - 1);
		spectrum->wavelength_cal[1] = 0.f;
		spectrum->wavelength_cal[2] = 0.f;
	}

	/* set this for later */
	spectrum->end = end;
}

/**
 * cd_spectrum_set_norm:
 * @spectrum: a #CdSpectrum instance.
 * @norm: the end value of the spectral data
 *
 * Set the normalization value of the spectrum.
 * NOTE: This affects every value in the spectrum.
 *
 * Since: 1.1.6
 **/
void
cd_spectrum_set_norm (CdSpectrum *spectrum, gdouble norm)
{
	g_return_if_fail (spectrum != NULL);
	spectrum->norm = norm;
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
	guint i;
	guint size;
	g_autoptr(CdInterp) interp = NULL;

	g_return_val_if_fail (spectrum != NULL, -1.f);

	/* out of bounds */
	size = cd_spectrum_get_size (spectrum);
	if (size == 0)
		return 1.f;
	if (wavelength < spectrum->start)
		return cd_spectrum_get_value (spectrum, 0);
	if (wavelength > spectrum->end)
		return cd_spectrum_get_value (spectrum, size - 1);

	/* add all the data points */
	interp = cd_interp_linear_new ();
	for (i = 0; i < size; i++) {
		cd_interp_insert (interp,
				  cd_spectrum_get_wavelength (spectrum, i),
				  cd_spectrum_get_value (spectrum, i));
	}

	/* get the interpolated value */
	if (!cd_interp_prepare (interp, NULL))
		return -1.f;
	return cd_interp_eval (interp, wavelength, NULL);
}

/**
 * cd_spectrum_limit_min:
 * @spectrum: a #CdSpectrum instance
 * @value: the threshold value to limit the spectrum
 *
 * Ensures no values in the spectrum fall below a set limit. If they
 * are found, set them to @value.
 *
 * Since: 1.3.1
 **/
void
cd_spectrum_limit_min (CdSpectrum *spectrum, gdouble value)
{
	gdouble tmp;
	guint i;
	for (i = 0; i < spectrum->data->len; i++) {
		tmp = cd_spectrum_get_value (spectrum, i);
		if (tmp < value)
			cd_spectrum_set_value (spectrum, i, value);
	}
}

/**
 * cd_spectrum_limit_max:
 * @spectrum: a #CdSpectrum instance
 * @value: the threshold value to limit the spectrum
 *
 * Ensures no values in the spectrum fall above a set limit. If they
 * are found, set them to @value.
 *
 * Since: 1.3.1
 **/
void
cd_spectrum_limit_max (CdSpectrum *spectrum, gdouble value)
{
	gdouble tmp;
	guint i;
	for (i = 0; i < spectrum->data->len; i++) {
		tmp = cd_spectrum_get_value (spectrum, i);
		if (tmp > value)
			cd_spectrum_set_value (spectrum, i, value);
	}
}

/**
 * cd_spectrum_normalize:
 * @spectrum: a #CdSpectrum instance
 * @wavelength: the wavelength in nm
 * @value: the value to normalize to
 *
 * Normalizes a spectrum to a specific value at a specific wavelength.
 *
 * Since: 1.1.6
 **/
void
cd_spectrum_normalize (CdSpectrum *spectrum, gdouble wavelength, gdouble value)
{
	gdouble tmp;
	tmp = cd_spectrum_get_value_for_nm (spectrum, wavelength);
	spectrum->norm *= value / tmp;
}

/**
 * cd_spectrum_normalize_max:
 * @spectrum: a #CdSpectrum instance
 * @value: the value to normalize to
 *
 * Normalizes a spectrum to a specific value at its maximum value.
 *
 * Since: 1.2.6
 **/
void
cd_spectrum_normalize_max (CdSpectrum *spectrum, gdouble value)
{
	gdouble max = 0.f;
	gdouble tmp;
	guint i;

	for (i = 0; i < spectrum->data->len; i++) {
		tmp = cd_spectrum_get_value_raw (spectrum, i);
		if (tmp > max)
			max = tmp;
	}
	if (max > 0.f)
		spectrum->norm = value / max;
}

/**
 * cd_spectrum_multiply:
 * @s1: a #CdSpectrum instance, possibly an illuminant.
 * @s2: a #CdSpectrum instance, possibly an absorption spectrum.
 * @resolution: the step size in nm
 *
 * Multiplies two spectra together.
 *
 * Return value: a #CdSpectrum instance
 *
 * Since: 1.1.6
 **/
CdSpectrum *
cd_spectrum_multiply (CdSpectrum *s1, CdSpectrum *s2, gdouble resolution)
{
	CdSpectrum *s;
	gdouble i;

	s = cd_spectrum_new ();
	s->id = g_strdup_printf ("%s✕%s", s1->id, s2->id);
	s->start = MAX (s1->start, s2->start);
	s->end = MIN (s1->end, s2->end);
	for (i = s->start; i <= s->end; i += resolution) {
		cd_spectrum_add_value (s, cd_spectrum_get_value_for_nm (s1, i) *
					  cd_spectrum_get_value_for_nm (s2, i));
	}
	return s;
}

/**
 * cd_spectrum_subtract:
 * @s1: a #CdSpectrum instance, e.g. a sample
 * @s2: a #CdSpectrum instance, e.g. a dark calibration
 * @resolution: the resolution to use when resampling
 *
 * Subtracts one spectral plot from another. If the spectra have the same start,
 * end and the same number of data points they are not resampled.
 *
 * Return value: a #CdSpectrum instance
 *
 * Since: 1.3.1
 **/
CdSpectrum *
cd_spectrum_subtract (CdSpectrum *s1, CdSpectrum *s2, gdouble resolution)
{
	CdSpectrum *s;
	gdouble max;
	gdouble min;
	gdouble nm;
	guint i;

	g_return_val_if_fail (s1 != NULL, NULL);
	g_return_val_if_fail (s2 != NULL, NULL);

	/* we can do this without resampling */
	if (fabs (s1->start - s2->start) < 0.01f &&
	    fabs (s1->end - s2->end) < 0.01f &&
	    s1->data->len == s2->data->len) {
		s = cd_spectrum_sized_new (s1->data->len);
		s->id = g_strdup_printf ("%s-%s", s1->id, s2->id);
		s->start = s1->start;
		s->end = s1->end;
		for (i = 0; i < 3; i++)
			s->wavelength_cal[i] = s1->wavelength_cal[i];
		for (i = 0; i < s1->data->len; i++) {
			gdouble tmp;
			tmp = cd_spectrum_get_value (s1, i) - cd_spectrum_get_value (s2, i);
			cd_spectrum_add_value (s, tmp);
		}
		return s;
	}

	/* resample */
	min = MIN (cd_spectrum_get_start (s1), cd_spectrum_get_start (s2));
	max = MAX (cd_spectrum_get_end (s1), cd_spectrum_get_end (s2));
	s = cd_spectrum_new ();
	s->id = g_strdup_printf ("%s-%s", s1->id, s2->id);
	s->start = min;
	s->end = max;
	for (nm = min; nm <= max; nm += resolution) {
		gdouble tmp;
		tmp = cd_spectrum_get_value_for_nm (s1, nm) -
			cd_spectrum_get_value_for_nm (s2, nm);
		cd_spectrum_add_value (s, tmp);
	}
	return s;
}

/**
 * cd_spectrum_to_string:
 * @spectrum: a #CdSpectrum instance
 * @max_width: the terminal width
 * @max_height: the terminal height
 *
 * Returns a graphical representation of the spectrum.
 *
 * Return value: a printable ASCII string
 *
 * Since: 1.3.1
 **/
gchar *
cd_spectrum_to_string (CdSpectrum *spectrum, guint max_width, guint max_height)
{
	GString *str = g_string_new ("");
	guint i, j;
	gdouble val_max;
	gdouble nm_scale;

	/* make space for the axes */
	max_width -= 9;
	max_height -= 2;

	/* find value maximum */
	val_max = cd_spectrum_get_value_max (spectrum);
	if (val_max < 0.001)
		val_max = 0.001;
	nm_scale = (cd_spectrum_get_end (spectrum) -
		    cd_spectrum_get_start (spectrum)) / (gdouble) (max_width - 1);

	/* draw grid */
	for (i = 0; i < max_height; i++) {
		gdouble val;
		val = val_max / (gdouble) max_height * (max_height - i);
		g_string_append_printf (str, "%7.3f |", val);
		for (j = 0; j < max_width; j++) {
			gdouble nm;
			nm = ((gdouble) j * nm_scale) + cd_spectrum_get_start (spectrum);
			if (cd_spectrum_get_value_for_nm (spectrum, nm) >= val)
				g_string_append (str, "#");
			else
				g_string_append (str, "_");
		}
		g_string_append (str, "\n");
	}

	/* draw x axis */
	g_string_append_printf (str, "%7.3f  ", 0.f);
	for (j = 0; j < max_width; j++)
		g_string_append (str, "-");
	g_string_append (str, "\n");

	/* draw X labels */
	g_string_append_printf (str, "         %.0fnm",
				cd_spectrum_get_start (spectrum));
	for (j = 0; j < max_width - 10; j++)
		g_string_append (str, " ");
	g_string_append_printf (str, "%.0fnm",
				cd_spectrum_get_end (spectrum));
	g_string_append (str, "\n");

	/* success */
	return g_string_free (str, FALSE);
}

/**
 * cd_spectrum_set_wavelength_cal:
 * @spectrum: a #CdSpectrum instance
 * @c1: the 1st coefficient
 * @c2: the 2nd coefficient
 * @c3: the 3rd coefficient
 *
 * Sets the calibration coefficients used to map pixel indexes to
 * wavelengths.
 *
 * This function will set the 'end' wavelength automatically,
 * potentially overwriting the value set by cd_spectrum_set_end().
 *
 * Since: 1.3.1
 **/
void
cd_spectrum_set_wavelength_cal (CdSpectrum *spectrum,
				gdouble c1, gdouble c2, gdouble c3)
{
	spectrum->wavelength_cal[0] = c1;
	spectrum->wavelength_cal[1] = c2;
	spectrum->wavelength_cal[2] = c3;

	/* recalculate the end wavelength */
	spectrum->end = cd_spectrum_get_wavelength (spectrum,
						    cd_spectrum_get_size (spectrum) - 1);
}

/**
 * cd_spectrum_get_wavelength_cal:
 * @spectrum: a #CdSpectrum instance
 * @c1: the 1st coefficient
 * @c2: the 2nd coefficient
 * @c3: the 3rd coefficient
 *
 * Gets the calibration coefficients used to map pixel indexes to
 * wavelengths.
 *
 * Since: 1.3.1
 **/
void
cd_spectrum_get_wavelength_cal (CdSpectrum *spectrum,
				gdouble *c1, gdouble *c2, gdouble *c3)
{
	if (c1 != NULL)
		*c1 = spectrum->wavelength_cal[0];
	if (c2 != NULL)
		*c2 = spectrum->wavelength_cal[1];
	if (c3 != NULL)
		*c3 = spectrum->wavelength_cal[2];
}

/**
 * cd_spectrum_resample:
 * @spectrum: a #CdSpectrum instance
 * @start: the new spectrum start
 * @end: the new spectrum end
 * @resolution: the resolution to use when resampling
 *
 * Resample a new spectrum with linear index to wavelength coefficients.
 *
 * Return value: a #CdSpectrum instance
 *
 * Since: 1.3.1
 **/
CdSpectrum *
cd_spectrum_resample (CdSpectrum *spectrum,
		      gdouble start,
		      gdouble end,
		      gdouble resolution)
{
	gdouble nm;
	CdSpectrum *sp;

	sp = cd_spectrum_new ();
	cd_spectrum_set_start (sp, start);
	for (nm = start; nm <= end; nm += resolution) {
		gdouble tmp;
		tmp = cd_spectrum_get_value_for_nm (spectrum, nm);
		cd_spectrum_add_value (sp, tmp);
	}
	cd_spectrum_set_end (sp, end);
	return sp;
}
