/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2013 Richard Hughes <richard@hughsie.com>
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
 * SECTION:cd-buffer
 * @short_description: Routines to read and write LE and BE values to a data buffer.
 *
 * Functions to manipulate a raw data buffer.
 */

#include "config.h"

#include <string.h>

#include "cd-buffer.h"

/**
 * cd_buffer_write_uint16_be:
 * @buffer: the writable data buffer
 * @value: the value to write
 *
 * Writes a native endian value into a big endian data buffer.
 * NOTE: No validation is done on the buffer to ensure it's big enough.
 **/
void
cd_buffer_write_uint16_be (guint8 *buffer, guint16 value)
{
	guint16 tmp;
	tmp = GUINT16_TO_BE (value);
	memcpy (buffer, &tmp, sizeof(tmp));
}

/**
 * cd_buffer_write_uint16_le:
 * @buffer: the writable data buffer
 * @value: the value to write
 *
 * Writes a native endian value into a little endian data buffer.
 * NOTE: No validation is done on the buffer to ensure it's big enough.
 **/
void
cd_buffer_write_uint16_le (guint8 *buffer, guint16 value)
{
	guint16 tmp;
	tmp = GUINT16_TO_LE (value);
	memcpy (buffer, &tmp, sizeof(tmp));
}

/**
 * cd_buffer_read_uint16_be:
 * @buffer: the writable data buffer
 *
 * Reads a native endian value from a big endian data buffer.
 * NOTE: No validation is done on the buffer to ensure it's valid.
 *
 * Return value: the value to read.
 **/
guint16
cd_buffer_read_uint16_be (const guint8 *buffer)
{
	guint16 tmp;
	memcpy (&tmp, buffer, sizeof(tmp));
	return GUINT16_FROM_BE (tmp);
}

/**
 * cd_buffer_read_uint16_le:
 * @buffer: the writable data buffer
 *
 * Reads a native endian value from a big endian data buffer.
 * NOTE: No validation is done on the buffer to ensure it's valid.
 *
 * Return value: the value to read.
 **/
guint16
cd_buffer_read_uint16_le (const guint8 *buffer)
{
	guint16 tmp;
	memcpy (&tmp, buffer, sizeof(tmp));
	return GUINT16_FROM_LE (tmp);
}

/**
 * cd_buffer_write_uint32_be:
 * @buffer: the writable data buffer
 * @value: the value to write
 *
 * Writes a native endian value into a big endian data buffer.
 * NOTE: No validation is done on the buffer to ensure it's big enough.
 **/
void
cd_buffer_write_uint32_be (guint8 *buffer, guint32 value)
{
	guint32 tmp;
	tmp = GUINT32_TO_BE (value);
	memcpy (buffer, &tmp, sizeof(tmp));
}

/**
 * cd_buffer_write_uint32_le:
 * @buffer: the writable data buffer
 * @value: the value to write
 *
 * Writes a native endian value into a little endian data buffer.
 * NOTE: No validation is done on the buffer to ensure it's big enough.
 **/
void
cd_buffer_write_uint32_le (guint8 *buffer, guint32 value)
{
	guint32 tmp;
	tmp = GUINT32_TO_LE (value);
	memcpy (buffer, &tmp, sizeof(tmp));
}

/**
 * cd_buffer_read_uint32_be:
 * @buffer: the writable data buffer
 *
 * Reads a native endian value from a big endian data buffer.
 * NOTE: No validation is done on the buffer to ensure it's valid.
 *
 * Return value: the value to read.
 **/
guint32
cd_buffer_read_uint32_be (const guint8 *buffer)
{
	guint32 tmp;
	memcpy (&tmp, buffer, sizeof(tmp));
	return GUINT32_FROM_BE (tmp);
}

/**
 * cd_buffer_read_uint32_le:
 * @buffer: the writable data buffer
 *
 * Reads a native endian value from a big endian data buffer.
 * NOTE: No validation is done on the buffer to ensure it's valid.
 *
 * Return value: the value to read.
 **/
guint32
cd_buffer_read_uint32_le (const guint8 *buffer)
{
	guint32 tmp;
	memcpy (&tmp, buffer, sizeof(tmp));
	return GUINT32_FROM_LE (tmp);
}

/**
 * cd_buffer_debug:
 * @buffer_kind: the debug mode, e.g %CD_BUFFER_KIND_REQUEST
 * @data: the data of size @length
 * @length: the size of data
 *
 * Prints some debugging of the request to the console.
 **/
void
cd_buffer_debug (CdBufferKind buffer_kind,
		 const guint8 *data,
		 gsize length)
{
	guint i;
	if (buffer_kind == CD_BUFFER_KIND_REQUEST)
		g_print ("%c[%dmrequest\t", 0x1B, 31);
	else if (buffer_kind == CD_BUFFER_KIND_RESPONSE)
		g_print ("%c[%dmresponse\t", 0x1B, 34);
	for (i = 0; i <  length; i++) {
		g_print ("%02x [%c]\t",
			 data[i],
			 g_ascii_isprint (data[i]) ? data[i] : '?');
	}
	g_print ("%c[%dm\n", 0x1B, 0);
}
