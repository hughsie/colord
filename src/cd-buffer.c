/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

/**
 * SECTION:cd-buffer
 * @short_description: Routines to read and write LE and BE values to a data buffer.
 *
 * Functions to manipulate a raw data buffer.
 */

#include "config.h"

#include <glib-object.h>

#include <cd-buffer.h>

/**
 * cd_buffer_write_uint16_be:
 * @buffer: the writable data buffer
 * @value: the value to write
 *
 * Writes a big endian value into a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's big enough.
 **/
void
cd_buffer_write_uint16_be (guchar *buffer, guint16 value)
{
	buffer[0] = (value >> 8) & 0xff;
	buffer[1] = (value >> 0) & 0xff;
}

/**
 * cd_buffer_write_uint16_le:
 * @buffer: the writable data buffer
 * @value: the value to write
 *
 * Writes a little endian value into a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's big enough.
 **/
void
cd_buffer_write_uint16_le (guchar *buffer, guint16 value)
{
	buffer[0] = (value >> 0) & 0xff;
	buffer[1] = (value >> 8) & 0xff;
}

/**
 * cd_buffer_read_uint16_be:
 * @buffer: the writable data buffer
 *
 * Reads a big endian value from a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's valid.
 *
 * Return value: the value to read.
 **/
guint16
cd_buffer_read_uint16_be (const guchar *buffer)
{
	return GUINT16_FROM_BE (*(guint16*)buffer);
}

/**
 * cd_buffer_read_uint16_le:
 * @buffer: the writable data buffer
 *
 * Reads a big endian value from a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's valid.
 *
 * Return value: the value to read.
 **/
guint16
cd_buffer_read_uint16_le (const guchar *buffer)
{
	return GUINT16_FROM_LE (*(guint16*)buffer);
}

/**
 * cd_buffer_write_uint32_be:
 * @buffer: the writable data buffer
 * @value: the value to write
 *
 * Writes a big endian value into a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's big enough.
 **/
void
cd_buffer_write_uint32_be (guchar *buffer, guint32 value)
{
	buffer[0] = (value >> 24) & 0xff;
	buffer[1] = (value >> 16) & 0xff;
	buffer[2] = (value >> 8) & 0xff;
	buffer[3] = (value >> 0) & 0xff;
}

/**
 * cd_buffer_write_uint32_le:
 * @buffer: the writable data buffer
 * @value: the value to write
 *
 * Writes a little endian value into a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's big enough.
 **/
void
cd_buffer_write_uint32_le (guchar *buffer, guint32 value)
{
	buffer[0] = (value >> 0) & 0xff;
	buffer[1] = (value >> 8) & 0xff;
	buffer[2] = (value >> 16) & 0xff;
	buffer[3] = (value >> 24) & 0xff;
}

/**
 * cd_buffer_read_uint32_be:
 * @buffer: the writable data buffer
 *
 * Reads a big endian value from a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's valid.
 *
 * Return value: the value to read.
 **/
guint32
cd_buffer_read_uint32_be (const guchar *buffer)
{
	return GUINT32_FROM_BE (*(guint32*)buffer);
}

/**
 * cd_buffer_read_uint32_le:
 * @buffer: the writable data buffer
 *
 * Reads a big endian value from a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's valid.
 *
 * Return value: the value to read.
 **/
guint32
cd_buffer_read_uint32_le (const guchar *buffer)
{
	return GUINT32_FROM_LE (*(guint32*)buffer);
}
