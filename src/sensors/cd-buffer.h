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

#ifndef __CD_BUFFER_H__
#define __CD_BUFFER_H__

#define __CD_BUFFER_H_INSIDE__

guint16		 cd_buffer_read_uint16_be	(const guchar	*buffer);
guint16		 cd_buffer_read_uint16_le	(const guchar	*buffer);
void		 cd_buffer_write_uint16_be	(guchar		*buffer,
						 guint16	 value);
void		 cd_buffer_write_uint16_le	(guchar		*buffer,
						 guint16	 value);
guint32		 cd_buffer_read_uint32_be	(const guchar	*buffer);
guint32		 cd_buffer_read_uint32_le	(const guchar	*buffer);
void		 cd_buffer_write_uint32_be	(guchar		*buffer,
						 guint32	 value);
void		 cd_buffer_write_uint32_le	(guchar		*buffer,
						 guint32	 value);

#undef __CD_BUFFER_H_INSIDE__

#endif /* __CD_BUFFER_H__ */

