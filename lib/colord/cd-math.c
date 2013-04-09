/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-12 Richard Hughes <richard@hughsie.com>
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
 * SECTION:cd-math
 * @short_description: Common maths functionality
 *
 * A GObject to use for common maths functionality like vectors and matrices.
 */

#include "config.h"

#include <math.h>
#include <string.h>
#include <glib-object.h>

#include <cd-math.h>

/**
 * cd_vec3_clear:
 * @src: the source vector
 *
 * Clears a vector, setting all it's values to zero.
 **/
void
cd_vec3_clear (CdVec3 *src)
{
	src->v0 = 0.0f;
	src->v1 = 0.0f;
	src->v2 = 0.0f;
}

/**
 * cd_vec3_init:
 * @dest: the destination vector
 * @v0: component value
 * @v1: component value
 * @v2: component value
 *
 * Initialises a vector.
 **/
void
cd_vec3_init (CdVec3 *dest, gdouble v0, gdouble v1, gdouble v2)
{
	g_return_if_fail (dest != NULL);

	dest->v0 = v0;
	dest->v1 = v1;
	dest->v2 = v2;
}

/**
 * cd_vec3_scalar_multiply:
 * @src: the source
 * @value: the scalar multiplier
 * @dest: the destination
 *
 * Multiplies a vector with a scalar.
 * The arguments @src and @dest can be the same value.
 **/
void
cd_vec3_scalar_multiply (const CdVec3 *src, gdouble value, CdVec3 *dest)
{
	dest->v0 = src->v0 * value;
	dest->v1 = src->v1 * value;
	dest->v2 = src->v2 * value;
}

/**
 * cd_vec3_copy:
 * @src: the source
 * @dest: the destination
 *
 * Copies the vector into another vector.
 * The arguments @src and @dest cannot be the same value.
 **/
void
cd_vec3_copy (const CdVec3 *src, CdVec3 *dest)
{
	g_return_if_fail (src != dest);
	memcpy (dest, src, sizeof (CdVec3));
}

/**
 * cd_vec3_add:
 * @src1: the source
 * @src2: the other source
 * @dest: the destination
 *
 * Adds two vector quantaties
 * The arguments @src and @dest can be the same value.
 **/
void
cd_vec3_add (const CdVec3 *src1, const CdVec3 *src2, CdVec3 *dest)
{
	dest->v0 = src1->v0 + src2->v0;
	dest->v1 = src1->v1 + src2->v1;
	dest->v2 = src1->v2 + src2->v2;
}

/**
 * cd_vec3_subtract:
 * @src1: the source
 * @src2: the other source
 * @dest: the destination
 *
 * Subtracts one vector quantaty from another
 * The arguments @src and @dest can be the same value.
 **/
void
cd_vec3_subtract (const CdVec3 *src1, const CdVec3 *src2, CdVec3 *dest)
{
	dest->v0 = src1->v0 - src2->v0;
	dest->v1 = src1->v1 - src2->v1;
	dest->v2 = src1->v2 - src2->v2;
}

/**
 * cd_vec3_to_string:
 * @src: the source
 *
 * Obtains a string representaton of a vector.
 *
 * Return value: the string. Free with g_free()
 **/
gchar *
cd_vec3_to_string (const CdVec3 *src)
{
	return g_strdup_printf ("\n/ %0 .6f \\\n"
				"| %0 .6f |\n"
				"\\ %0 .6f /",
				src->v0, src->v1, src->v2);
}

/**
 * cd_vec3_get_data:
 * @src: the vector source
 *
 * Gets the raw data for the vector.
 *
 * Return value: the pointer to the data segment.
 **/
gdouble *
cd_vec3_get_data (const CdVec3 *src)
{
	return (gdouble *) src;
}

/**
 * cd_vec3_squared_error:
 * @src1: the vector source
 * @src2: another vector source
 *
 * Gets the mean squared error for a pair of vectors
 *
 * Return value: the floating point MSE.
 **/
gdouble
cd_vec3_squared_error (const CdVec3 *src1, const CdVec3 *src2)
{
	CdVec3 tmp;
	cd_vec3_subtract (src1, src2, &tmp);
	return (tmp.v0 * tmp.v0) +
	       (tmp.v1 * tmp.v1) +
	       (tmp.v2 * tmp.v2);
}

/**
 * cd_mat33_clear:
 * @src: the source
 *
 * Clears a matrix value, setting all it's values to zero.
 **/
void
cd_mat33_clear (const CdMat3x3 *src)
{
	guint i;
	gdouble *temp = (gdouble *) src;
	for (i = 0; i < 3*3; i++)
		temp[i] = 0.0f;
}

/**
 * cd_mat33_to_string:
 * @src: the source
 *
 * Obtains a string representaton of a matrix.
 *
 * Return value: the string. Free with g_free()
 **/
gchar *
cd_mat33_to_string (const CdMat3x3 *src)
{
	return g_strdup_printf ("\n/ %0 .6f  %0 .6f  %0 .6f \\\n"
				"| %0 .6f  %0 .6f  %0 .6f |\n"
				"\\ %0 .6f  %0 .6f  %0 .6f /",
				src->m00, src->m01, src->m02,
				src->m10, src->m11, src->m12,
				src->m20, src->m21, src->m22);
}

/**
 * cd_mat33_get_data:
 * @src: the matrix source
 *
 * Gets the raw data for the matrix.
 *
 * Return value: the pointer to the data segment.
 **/
gdouble *
cd_mat33_get_data (const CdMat3x3 *src)
{
	return (gdouble *) src;
}

/**
 * cd_mat33_set_identity:
 * @src: the source
 *
 * Sets the matrix to an identity value.
 **/
void
cd_mat33_set_identity (CdMat3x3 *src)
{
	cd_mat33_clear (src);
	src->m00 = 1.0f;
	src->m11 = 1.0f;
	src->m22 = 1.0f;
}

/**
 * cd_mat33_determinant:
 * @src: the source
 *
 * Gets the determinant of the matrix.
 **/
gdouble
cd_mat33_determinant (const CdMat3x3 *src)
{
	return src->m00 * src->m11 * src->m22 +
	       src->m01 * src->m12 * src->m20 +
	       src->m02 * src->m10 * src->m21 -
	       src->m02 * src->m11 * src->m20 -
	       src->m01 * src->m10 * src->m22 -
	       src->m00 * src->m12 * src->m21;
}

/**
 * cd_mat33_normalize:
 * @src: the source matrix
 * @dest: the destination matrix
 *
 * Normalizes a matrix
 *
 * The arguments @src and @dest can be the same value.
 **/
void
cd_mat33_normalize (const CdMat3x3 *src, CdMat3x3 *dest)
{
	gdouble *data_dest;
	gdouble *data_src;
	gdouble det;
	guint i;

	data_src = cd_mat33_get_data (src);
	data_dest = cd_mat33_get_data (dest);
	det = cd_mat33_determinant (src);
	for (i = 0; i < 9; i++)
		data_dest[i] = data_src[i] / det;
}


/**
 * cd_mat33_vector_multiply:
 * @mat_src: the matrix source
 * @vec_src: the vector source
 * @vec_dest: the destination vector
 *
 * Multiplies a matrix with a vector.
 * The arguments @vec_src and @vec_dest cannot be the same value.
 **/
void
cd_mat33_vector_multiply (const CdMat3x3 *mat_src, const CdVec3 *vec_src, CdVec3 *vec_dest)
{
	g_return_if_fail (vec_src != vec_dest);
	vec_dest->v0 = mat_src->m00 * vec_src->v0 +
		       mat_src->m01 * vec_src->v1 +
		       mat_src->m02 * vec_src->v2;
	vec_dest->v1 = mat_src->m10 * vec_src->v0 +
		       mat_src->m11 * vec_src->v1 +
		       mat_src->m12 * vec_src->v2;
	vec_dest->v2 = mat_src->m20 * vec_src->v0 +
		       mat_src->m21 * vec_src->v1 +
		       mat_src->m22 * vec_src->v2;
}

/**
 * cd_mat33_scalar_multiply:
 * @mat_src: the source
 * @value: the scalar
 * @mat_dest: the destination
 *
 * Multiplies a matrix with a scalar.
 * The arguments @vec_src and @vec_dest can be the same value.
 **/
void
cd_mat33_scalar_multiply (const CdMat3x3 *mat_src,
			  gdouble value,
			  CdMat3x3 *mat_dest)
{
	gdouble *tmp_src;
	gdouble *tmp_dest;
	guint i;
	tmp_src = cd_mat33_get_data (mat_src);
	tmp_dest = cd_mat33_get_data (mat_dest);
	for (i = 0; i < 9; i++)
		tmp_dest[i] = tmp_src[i] * value;
}

/**
 * cd_mat33_matrix_multiply:
 * @mat_src1: the matrix source
 * @mat_src2: the other matrix source
 * @mat_dest: the destination
 *
 * Multiply (convolve) one matrix with another.
 * The arguments @mat_src1 cannot be the same as @mat_dest, and
 * @mat_src2 cannot be the same as @mat_dest.
 **/
void
cd_mat33_matrix_multiply (const CdMat3x3 *mat_src1, const CdMat3x3 *mat_src2, CdMat3x3 *mat_dest)
{
	guint8 i, j, k;
	gdouble *src1 = cd_mat33_get_data (mat_src1);
	gdouble *src2 = cd_mat33_get_data (mat_src2);
	gdouble *dest = cd_mat33_get_data (mat_dest);
	g_return_if_fail (mat_src1 != mat_dest);
	g_return_if_fail (mat_src2 != mat_dest);

	cd_mat33_clear (mat_dest);
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			for (k = 0; k < 3; k++) {
				dest[3 * i + j] += src1[i * 3 + k] * src2[k * 3 + j];
			}
		}
	}
}

/**
 * cd_mat33_reciprocal:
 * @src: the source
 * @dest: the destination
 *
 * Inverts the matrix.
 * The arguments @src and @dest cannot be the same value.
 *
 * Return value: %FALSE if det is zero (singular).
 **/
gboolean
cd_mat33_reciprocal (const CdMat3x3 *src, CdMat3x3 *dest)
{
	double det = 0;

	g_return_val_if_fail (src != dest, FALSE);

	det  = src->m00 * (src->m11 * src->m22 - src->m12 * src->m21);
	det -= src->m01 * (src->m10 * src->m22 - src->m12 * src->m20);
	det += src->m02 * (src->m10 * src->m21 - src->m11 * src->m20);

	/* division by zero */
	if (fabs (det) < 1e-6)
		return FALSE;

	dest->m00 = (src->m11 * src->m22 - src->m12 * src->m21) / det;
	dest->m01 = (src->m02 * src->m21 - src->m01 * src->m22) / det;
	dest->m02 = (src->m01 * src->m12 - src->m02 * src->m11) / det;

	dest->m10 = (src->m12 * src->m20 - src->m10 * src->m22) / det;
	dest->m11 = (src->m00 * src->m22 - src->m02 * src->m20) / det;
	dest->m12 = (src->m02 * src->m10 - src->m00 * src->m12) / det;

	dest->m20 = (src->m10 * src->m21 - src->m11 * src->m20) / det;
	dest->m21 = (src->m01 * src->m20 - src->m00 * src->m21) / det;
	dest->m22 = (src->m00 * src->m11 - src->m01 * src->m10) / det;

	return TRUE;
}

/**
 * cd_mat33_copy:
 * @src: the source
 * @dest: the destination
 *
 * Copies the matrix.
 * The arguments @src and @dest cannot be the same value.
 **/
void
cd_mat33_copy (const CdMat3x3 *src, CdMat3x3 *dest)
{
	g_return_if_fail (src != dest);
	memcpy (dest, src, sizeof (CdMat3x3));
}
