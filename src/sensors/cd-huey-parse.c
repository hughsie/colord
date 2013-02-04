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

#include <glib.h>
#include <huey/huey.h>

static void
parse_command_sequence (GString *output, const gchar *line, gboolean reply)
{
	gchar **tok;
	guint j;
	guint8 cmd;
	guint8 instruction = 0;
	const gchar *command_as_text;
	tok = g_strsplit (line, " ", -1);

	/* only know how to parse 8 bytes */
	if (g_strv_length (tok) != 8)
		goto out;
	for (j = 0; j < 8; j++) {
		command_as_text = NULL;
		cmd = g_ascii_strtoll (tok[j], NULL, 16);
		if (j == 0 && reply) {
			command_as_text = huey_rc_to_string (cmd);
			if (command_as_text == NULL)
				g_warning ("return code 0x%02x not known in %s", cmd, line);
		}
		if ((j == 0 && !reply) ||
		    (j == 1 && reply)) {
			instruction = cmd;
			command_as_text = huey_cmd_code_to_string (instruction);
			if (command_as_text == NULL)
				g_warning ("command code 0x%02x not known", cmd);
		}
		/* some requests are filled with junk data */
		if (!reply && instruction == 0x08 && j > 1)
			g_string_append_printf (output, "xx ");
		else if (!reply && instruction == 0x18 && j > 4)
			g_string_append_printf (output, "xx ");
		else if (!reply && instruction == 0x17 && j > 3)
			g_string_append_printf (output, "xx ");
		else if (command_as_text != NULL)
			g_string_append_printf (output, "%02x(%s) ", cmd, command_as_text);
		else
			g_string_append_printf (output, "%02x ", cmd);
	}
	/* remove trailing space */
	if (output->len > 1)
		g_string_set_size (output, output->len - 1);
out:
	g_strfreev (tok);
}

typedef enum {
	CD_PARSE_MODE_USBDUMP,
	CD_PARSE_MODE_ARGYLLD9,
	CD_PARSE_MODE_STRACEUSB
} CdParseMode;

static void
parse_line_argyll (GString *output, gchar *line, gboolean *reply)
{
	if (g_str_has_prefix (line, "huey: Sending cmd")) {
		g_string_append (output, " ---> ");
		*reply = FALSE;
	}
	if (g_str_has_prefix (line, "huey: Reading response")) {
		g_string_append (output, " <--- ");
		*reply = TRUE;
	}
	if (g_str_has_prefix (line, "icoms: Writing control data")) {
		*reply = FALSE;
		g_string_append (output, " ---> ");
		parse_command_sequence (output, &line[28], *reply);
	}
	if (g_str_has_prefix (line, " '")) {
		line[21] = '\0';
		/* argyll 'helpfully' removes the two bytes */
		g_string_append (output, "00(success) xx(cmd) ");
		g_string_append (output, &line[2]);
		g_string_append (output, "\n");
	}
	if (g_strcmp0 (line, " ICOM err 0x0") == 0)
		g_string_append (output, "\n");
}

static void
parse_line_usbdump (GString *output, const gchar *line, gboolean *reply)
{

	/* timestamp */
	if (line[0] == '[')
		return;

	/* function */
	if (line[0] == '-') {
		g_string_append (output, "\n");
		if (g_str_has_suffix (line, "URB_FUNCTION_CLASS_INTERFACE:"))
			g_string_append (output, "[class-interface]     ");
		else if (g_str_has_suffix (line, "URB_FUNCTION_CONTROL_TRANSFER:"))
			g_string_append (output, "[control-transfer]    ");
		else if (g_str_has_suffix (line, "URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:"))
			g_string_append (output, "[interrupt-transfer]  ");
		else
			g_string_append (output, "[unknown]             ");
	}

	if (g_strstr_len (line, -1, "USBD_TRANSFER_DIRECTION_IN") != NULL) {
		g_string_append (output, " <--- ");
		*reply = TRUE;
	}
	if (g_strstr_len (line, -1, "USBD_TRANSFER_DIRECTION_OUT") != NULL) {
		g_string_append (output, " ---> ");
		*reply = FALSE;
	}

	if (g_strstr_len (line, -1, "00000000:") != NULL) {
		parse_command_sequence (output, &line[14], *reply);
	}
}

static void
parse_line_straceusb (GString *output, const gchar *line, gboolean *reply)
{
	gchar *tmp;

	if (g_strstr_len (line, -1, "USBDEVFS") == NULL)
		return;
	if (g_strstr_len (line, -1, "EAGAIN") != NULL)
		return;
//make && ./cd-parse-huey straceusb strace-spotread-usb.txt strace-spotread-usb.parsed && cat ./strace-spotread-usb.parsed
	tmp = g_strstr_len (line, -1, "data=");
	if (tmp == NULL) {
		*reply = TRUE;
		tmp = g_strstr_len (line, -1, "buffer=");
		if (tmp == NULL)
			return;
	} else {
		*reply = FALSE;
	}

	tmp[28] = '\0';
	parse_command_sequence (output, tmp+5, *reply);
	g_string_append (output, "\n");

	return;

	g_error ("@%s@", tmp+5);

	g_string_append (output, line);
	g_string_append (output, "\n");
}

gint
main (gint argc, gchar *argv[])
{
	gboolean ret;
	gchar *data = NULL;
	gchar **split = NULL;
	GString *output = NULL;
	GError *error = NULL;
	guint i;
	gchar *line;
	gboolean reply = FALSE;
	CdParseMode mode = 0;

	if (argc != 4) {
		g_print ("need to specify mode then two files\n");
		goto out;
	}

	/* get the mode */
	if (g_strcmp0 (argv[1], "usbdump") == 0)
		mode = CD_PARSE_MODE_USBDUMP;
	else if (g_strcmp0 (argv[1], "argylld9") == 0)
		mode = CD_PARSE_MODE_ARGYLLD9;
	else if (g_strcmp0 (argv[1], "straceusb") == 0)
		mode = CD_PARSE_MODE_STRACEUSB;
	else {
		g_print ("mode unrecognised, use strace, argylld9 or straceusb");
		goto out;
	}

	g_print ("parsing %s into %s... ", argv[2], argv[3]);

	/* read file */
	ret = g_file_get_contents (argv[2], &data, NULL, &error);
	if (!ret) {
		g_print ("failed to read: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* parse string */
	output = g_string_new ("// automatically generated, do not edit\n");
	split = g_strsplit (data, "\n", -1);
	for (i = 0; split[i] != NULL; i++) {
		line = split[i];

		if (mode == CD_PARSE_MODE_ARGYLLD9)
			parse_line_argyll (output, line, &reply);
		else if (mode == CD_PARSE_MODE_USBDUMP)
			parse_line_usbdump (output, line, &reply);
		else if (mode == CD_PARSE_MODE_STRACEUSB)
			parse_line_straceusb (output, line, &reply);
		else
			g_print ("%i:%s\n", i, split[i]);
	}

	/* write file */
	ret = g_file_set_contents (argv[3], output->str, -1, &error);
	if (!ret) {
		g_print ("failed to read: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	g_print ("done!\n");

out:
	if (output != NULL)
		g_string_free (output, TRUE);
	g_free (data);
	g_strfreev (split);
	return 0;
}

