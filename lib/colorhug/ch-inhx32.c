/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
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
#include <stdio.h>
#include <string.h>

#include "ch-inhx32.h"

#define	CH_RECORD_TYPE_DATA		0
#define	CH_RECORD_TYPE_EOF		1
#define	CH_RECORD_TYPE_EXTENDED		4

/**
 * ch_inhx32_parse_uint8:
 **/
static guint8
ch_inhx32_parse_uint8 (const gchar *data, guint pos)
{
	gchar buffer[3];
	buffer[0] = data[pos+0];
	buffer[1] = data[pos+1];
	buffer[2] = '\0';
	return g_ascii_strtoull (buffer, NULL, 16);
}

/**
 * ch_inhx32_to_bin:
 * @in_buffer: A %NULL terminated Intel hex byte string
 * @out_buffer: The output byte buffer
 * @out_size: The size of @out_buffer
 * @error: A #GError or %NULL
 *
 * Converts the Intel hex byte string into a binary packed
 * representation suitable for direct flashing the ColorHug.
 *
 * Return value: packed value to host byte order
 */
gboolean
ch_inhx32_to_bin (const gchar *in_buffer,
		  guint8 **out_buffer,
		  gsize *out_size,
		  GError **error)
{
	gboolean ret;
	gboolean verbose;
	gchar *ptr;
	gint checksum;
	gint end;
	gint i;
	gint offset = 0;
	GString *string = NULL;
	guint8 data_tmp;
	guint addr32 = 0;
	guint addr32_last = 0;
	guint addr_high = 0;
	guint addr_low = 0;
	guint j;
	guint len_tmp;
	guint type;

	g_return_val_if_fail (in_buffer != NULL, FALSE);

	/* only if set */
	verbose = g_getenv ("VERBOSE") != NULL;
	string = g_string_new ("");
	while (TRUE) {

		/* length, 16-bit address, type */
		if (sscanf (&in_buffer[offset], ":%02x%04x%02x",
			    &len_tmp, &addr_low, &type) != 3) {
			ret = FALSE;
			g_set_error_literal (error, 1, 0,
					     "invalid inhx32 syntax");
			goto out;
		}

		/* position of checksum */
		end = offset + 9 + len_tmp * 2;

		/* verify checksum */
		checksum = 0;
		for (i = offset + 1; i < end; i += 2) {
			data_tmp = ch_inhx32_parse_uint8 (in_buffer, i);
			checksum = (checksum + (0x100 - data_tmp)) & 0xff;
		}
		if (ch_inhx32_parse_uint8 (in_buffer, end) != checksum)  {
			ret = FALSE;
			g_set_error_literal (error, 1, 0,
					     "invalid checksum");
			goto out;
		}

		/* process different record types */
		switch (type) {
		case CH_RECORD_TYPE_DATA:
			/* if not contiguous with previous record,
			 * issue accumulated hex data (if any) and start anew. */
			if ((addr_high + addr_low) != addr32)
				addr32 = addr_high + addr_low;

			/* Parse bytes from line into hexBuf */
			for (i = offset + 9; i < end; i += 2) {
				if (addr32 >= CH_EEPROM_ADDR_RUNCODE &&
				    addr32 < 0xfff0) {

					/* find out if there are any
					 * holes in the hex record */
					len_tmp = addr32 - addr32_last;
					if (addr32_last > 0x0 && len_tmp > 1) {
						for (j = 1; j < len_tmp; j++) {
							if (verbose) {
								g_debug ("Filling address 0x%04x",
									 addr32_last + j);
							}
							g_string_append_c (string, 0xff);
						}
					}
					data_tmp = ch_inhx32_parse_uint8 (in_buffer, i);
					g_string_append_c (string, data_tmp);
					if (verbose)
						g_debug ("Writing address 0x%04x", addr32);
					addr32_last = addr32;
				} else {
					if (verbose)
						g_debug ("Ignoring address 0x%04x", addr32);
				}
				addr32++;
			}
			break;
		case CH_RECORD_TYPE_EOF:
			break;
		case CH_RECORD_TYPE_EXTENDED:
			if (sscanf (&in_buffer[offset+9], "%04x", &addr_high) != 1) {
				ret = FALSE;
				g_set_error_literal (error, 1, 0,
						     "invalid hex syntax");
				goto out;
			}
			addr_high <<= 16;
			addr32 = addr_high + addr_low;
			break;
		default:
			ret = FALSE;
			g_set_error_literal (error, 1, 0,
					     "invalid record type");
			goto out;
		}

		/* advance to start of next line */
		ptr = strchr (&in_buffer[end+2], ':');
		if (ptr == NULL)
			break;
		offset = ptr - in_buffer;
	}

	/* ensure flash finishes on a 64 byte boundary */
	end = string->len % CH_FLASH_WRITE_BLOCK_SIZE;
	for (i = 0; i < CH_FLASH_WRITE_BLOCK_SIZE - end; i++)
		g_string_append_len (string, "\0", 1);

	/* save data */
	if (out_size != NULL)
		*out_size = string->len;
	if (out_buffer != NULL)
		*out_buffer = g_memdup (string->str, string->len);

	/* success */
	ret = TRUE;
out:
	if (string != NULL)
		g_string_free (string, TRUE);
	return ret;
}
