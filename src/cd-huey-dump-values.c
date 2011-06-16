/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <math.h>
#include <glib.h>

#include "cd-buffer.h"

gint
main (gint argc, gchar *argv[])
{
	gboolean ret;
	GError *error = NULL;
	gchar *data = NULL;
	gchar **lines = NULL;
	guint i, j;
	guint addr;
	guint8 buffer[0xff+4];
	guint value_uint32;
	volatile gfloat value_float;

	if (argc != 2)
		goto out;

	for (i=0; i<0xff; i++)
		buffer[i] = 0;

	ret = g_file_get_contents (argv[1], &data, NULL, &error);
	if (!ret) {
		g_warning ("failed to open: %s", error->message);
		g_error_free (error);
		goto out;
	}
	lines = g_strsplit (data, "\n", -1);
	for (i=0; lines[i] != NULL; i++) {
		if (g_str_has_prefix (lines[i], "register[0x")) {
			addr = g_ascii_strtoull (lines[i] + 11, NULL, 16);
			if (addr > 0xff) {
				g_warning ("addr=%i", addr);
				continue;
			}
			buffer[addr] = g_ascii_strtoull (lines[i] + 15, NULL, 16);
		}
	}
	g_print ("*** find byte ***\n");
	for (i=0; i<0xff; i++) {
		g_print ("0x%02x\t0x%02x\t(%i)\n", i, (int) buffer[i], (int) buffer[i]);
	}
	g_print ("*** find uint32 ***\n");
	for (j=0; j<4; j++) {
		for (i=j; i<0xff-3; i+=4) {
			value_uint32 = cd_buffer_read_uint32_be (buffer+i);
			if (value_uint32 == G_MAXUINT32) {
				g_print ("0x%02x\t<invalid>\n", i);
				continue;
			}
			g_print ("0x%02x\t%u\n", i, value_uint32);
		}
	}
	g_print ("*** find float ***\n");
	for (j=0; j<4; j++) {
		for (i=j; i<0xff-3; i+=4) {
			value_uint32 = cd_buffer_read_uint32_be (buffer+i);
			value_float = *((volatile gfloat*) &value_uint32);
			if (isnan (value_float)) {
				g_print ("0x%02x\t<invalid>\n", i);
				continue;
			}
			g_print ("0x%02x\t%f\n", i, value_float);
		}
	}
	g_print ("*** find time/dates ***\n");
	for (i=0; i<0xff-3; i++) {
		GDate *date;
		time_t time_tmp;
		gchar text[128];
		time_tmp = (time_t) cd_buffer_read_uint32_be (buffer+i);
		date = g_date_new ();
		g_date_set_time_t (date, time_tmp);
		if (!g_date_valid(date) ||
		    date->year == 1970 ||
		    date->year > 2011 ||
		    date->year < 1999) {
			g_print ("0x%02x\t<invalid>\n", i);
			continue;
		}
		g_date_strftime (text, 128, "%F", date);
		g_print ("0x%02x\t%s\n", i, text);
		g_date_free (date);
	}

out:
	g_strfreev (lines);
	g_free (data);
	return 0;
}
