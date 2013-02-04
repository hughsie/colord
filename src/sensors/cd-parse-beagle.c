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
#include <munki/munki.h>

#include "cd-sensor.h"

typedef enum {
	CD_PARSE_SECTION_LEVEL,
	CD_PARSE_SECTION_SP,
	CD_PARSE_SECTION_MS_US,
	CD_PARSE_SECTION_DUR,
	CD_PARSE_SECTION_LEN,
	CD_PARSE_SECTION_ERR,
	CD_PARSE_SECTION_DEV,
	CD_PARSE_SECTION_EP,
	CD_PARSE_SECTION_RECORD,
	CD_PARSE_SECTION_SUMMARY
} CdParseSection;

typedef enum {
	CD_PARSE_ENTRY_DIRECTION_UNKNOWN,
	CD_PARSE_ENTRY_DIRECTION_REQUEST,
	CD_PARSE_ENTRY_DIRECTION_REPLY
} CdParseEntryDirection;

typedef struct {
	const gchar		*record;
	const gchar		*summary;
	const gchar		*summary_pretty;
	gint			 dev;
	gint			 ep;
	const gchar		*ep_description;
	CdParseEntryDirection	 direction;
} CdParseEntry;

/**
 * cd_parse_beagle_process_entry_huey:
 **/
static void
cd_parse_beagle_process_entry_huey (CdParseEntry *entry)
{
	gchar **tok;
	guint j;
	guint8 cmd;
	guint8 instruction = 0;
	const gchar *command_as_text;
	GString *output = NULL;

	entry->ep_description = "default";

	/* only know how to parse 8 bytes */
	tok = g_strsplit (entry->summary, " ", -1);
	if (g_strv_length (tok) != 8) {
		g_print ("not 8 tokens: %s\n", entry->summary);
		goto out;
	}

	output = g_string_new ("");
	for (j = 0; j < 8; j++) {
		command_as_text = NULL;
		cmd = g_ascii_strtoll (tok[j], NULL, 16);
		if (j == 0 && entry->direction == CD_PARSE_ENTRY_DIRECTION_REPLY) {
			command_as_text = huey_rc_to_string (cmd);
			if (command_as_text == NULL)
				g_warning ("return code 0x%02x not known in %s", cmd, entry->summary);
		}
		if ((j == 0 && entry->direction == CD_PARSE_ENTRY_DIRECTION_REQUEST) ||
		    (j == 1 && entry->direction == CD_PARSE_ENTRY_DIRECTION_REPLY)) {
			instruction = cmd;
			command_as_text = huey_cmd_code_to_string (instruction);
			if (command_as_text == NULL)
				g_warning ("command code 0x%02x not known", cmd);
		}

		/* some requests are filled with junk data */
		if (entry->direction == CD_PARSE_ENTRY_DIRECTION_REQUEST &&
		    instruction == HUEY_CMD_REGISTER_READ && j > 1)
			g_string_append_printf (output, "xx ");
		else if (entry->direction == CD_PARSE_ENTRY_DIRECTION_REQUEST &&
			 instruction == HUEY_CMD_SET_LEDS && j > 4)
			g_string_append_printf (output, "xx ");
		else if (entry->direction == CD_PARSE_ENTRY_DIRECTION_REQUEST &&
			 instruction == HUEY_CMD_GET_AMBIENT && j > 3)
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
	if (output != NULL)
		entry->summary_pretty = g_string_free (output, FALSE);
	g_strfreev (tok);
}

/**
 * cd_parse_beagle_process_entry_colormunki:
 **/
static void
cd_parse_beagle_process_entry_colormunki (CdParseEntry *entry)
{
	gchar **tok;
	guint j;
	guint8 cmd;
	guint tok_len;
	GString *output;

	/* set ep description */
	entry->ep_description = munki_endpoint_to_string (entry->ep);

	output = g_string_new ("");

	/* only know how to parse 8 bytes */
	tok = g_strsplit (entry->summary, " ", -1);
	tok_len = g_strv_length (tok);

	/* status */
	if (entry->ep == MUNKI_EP_CONTROL &&
	    entry->direction == CD_PARSE_ENTRY_DIRECTION_REPLY &&
	    tok_len == 2) {

		/* dial position */
		cmd = g_ascii_strtoll (tok[0], NULL, 16);
		g_string_append_printf (output, "%s(dial-position-%s) ",
					tok[0],
					munki_dial_position_to_string (cmd));

		/* button value */
		cmd = g_ascii_strtoll (tok[1], NULL, 16);
		g_string_append_printf (output, "%s(button-state-%s)",
					tok[1],
					munki_button_state_to_string (cmd));
		goto out;
	}

	/* event */
	if (entry->ep == MUNKI_EP_EVENT &&
	    entry->direction == CD_PARSE_ENTRY_DIRECTION_REPLY &&
	    tok_len == 8) {
		g_print ("process 8: %s\n", entry->summary);

		/* cmd */
		cmd = g_ascii_strtoll (tok[0], NULL, 16);
		g_string_append_printf (output, "%s(%s) ",
					tok[0],
					munki_command_value_to_string (cmd));

		for (j=1; j<8; j++) {
			cmd = g_ascii_strtoll (tok[j], NULL, 16);
			g_string_append_printf (output, "%02x ", cmd);
		}
		if (output->len > 1)
			g_string_set_size (output, output->len - 1);
		goto out;
	}

	/* unknown command */
	for (j = 0; j < tok_len; j++) {
		cmd = g_ascii_strtoll (tok[j], NULL, 16);
		g_string_append_printf (output, "%02x ", cmd);
	}
	if (output->len > 1)
		g_string_set_size (output, output->len - 1);
out:
	if (output != NULL)
		entry->summary_pretty = g_string_free (output, FALSE);
	g_strfreev (tok);
}

/**
 * cd_parse_beagle_process_entry:
 **/
static gchar *
cd_parse_beagle_process_entry (CdSensorKind kind, CdParseEntry *entry)
{
	gchar *retval = NULL;
	const gchar *direction = "??";

	/* timeout */
	if (g_str_has_suffix (entry->record, "IN-NAK]"))
		goto out;

	/* device closed */
	if (g_strcmp0 (entry->record, "[1 ORPHANED]") == 0)
		goto out;

	/* usb error */
	if (g_strcmp0 (entry->record, "[53 SYNC ERRORS]") == 0)
		goto out;

	/* other event to ignore */
	if (g_strcmp0 (entry->record, "Bus event") == 0)
		goto out;
	if (g_strcmp0 (entry->record, "Get Configuration Descriptor") == 0)
		goto out;
	if (g_strcmp0 (entry->record, "Set Configuration") == 0)
		goto out;

	/* not sure what these are */
	if (g_str_has_suffix (entry->record, " SOF]"))
		goto out;
	if (g_strcmp0 (entry->record, "Clear Endpoint Feature") == 0)
		goto out;

	/* start or end of file */
	if (g_str_has_prefix (entry->record, "Capture started"))
		goto out;
	if (g_strcmp0 (entry->record, "Capture stopped") == 0)
		goto out;

	/* get direction */
	if (g_str_has_prefix (entry->record, "IN txn"))
		entry->direction = CD_PARSE_ENTRY_DIRECTION_REPLY;
	else if (g_strcmp0 (entry->record, "Control Transfer") == 0)
		entry->direction = CD_PARSE_ENTRY_DIRECTION_REQUEST;

	/* get correct string */
	if (entry->direction == CD_PARSE_ENTRY_DIRECTION_REQUEST)
		direction = ">>";
	else if (entry->direction == CD_PARSE_ENTRY_DIRECTION_REPLY)
		direction = "<<";

	/* sexify the output */
	if (kind == CD_SENSOR_KIND_HUEY)
		cd_parse_beagle_process_entry_huey (entry);
	else if (kind == CD_SENSOR_KIND_COLOR_MUNKI_PHOTO)
		cd_parse_beagle_process_entry_colormunki (entry);
	retval = g_strdup_printf ("dev%02i ep%02i(%s)\t%s\t%s\n",
				  entry->dev, entry->ep,
				  entry->ep_description,
				  direction,
				  entry->summary_pretty != NULL ? entry->summary_pretty : entry->summary);
out:
	return retval;
}

/**
 * main:
 **/
gint
main (gint argc, gchar *argv[])
{
	gboolean ret;
	gchar *data = NULL;
	gchar **split = NULL;
	gchar **sections = NULL;
	GString *output = NULL;
	GError *error = NULL;
	guint i;
	CdParseEntry entry;
	gchar *part;
	gint retval = 1;
	CdSensorKind kind;

	if (argc != 4) {
		g_print ("need to specify [huey|colormunki] input output\n");
		goto out;
	}
	kind = cd_sensor_kind_from_string (argv[1]);
	if (kind != CD_SENSOR_KIND_HUEY &&
	    kind != CD_SENSOR_KIND_DTP94 &&
	    kind != CD_SENSOR_KIND_COLOR_MUNKI_PHOTO) {
		g_print ("only huey and colormunki device kinds supported\n");
		goto out;
	}

	/* read file */
	ret = g_file_get_contents (argv[2], &data, NULL, &error);
	if (!ret) {
		g_print ("failed to read: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* parse string */
	output = g_string_new ("// automatically generated, do not edit\n");

	/* parse string */
	split = g_strsplit (data, "\n", -1);
	for (i = 0; split[i] != NULL; i++) {

		/* comment or blank line */
		if (split[i][0] == '#' ||
		    split[i][0] == '\0')
			continue;

		g_print ("@@%i:%s\n", i, split[i]);

		/* populate a CdParseEntry */
		sections = g_strsplit (split[i], ",", -1);
		entry.record = sections[CD_PARSE_SECTION_RECORD];
		entry.summary = sections[CD_PARSE_SECTION_SUMMARY];
		entry.dev = atoi (sections[CD_PARSE_SECTION_DEV]);
		entry.ep = atoi (sections[CD_PARSE_SECTION_EP]);
		entry.direction = CD_PARSE_ENTRY_DIRECTION_UNKNOWN;
		entry.summary_pretty = NULL;
		entry.ep_description = NULL;
		part = cd_parse_beagle_process_entry (kind, &entry);
		if (part != NULL) {
			g_string_append (output, part);
//			g_print ("%s\n", part);
		}
		g_free (part);
		g_strfreev (sections);
	}

	/* write file */
	ret = g_file_set_contents (argv[3], output->str, -1, &error);
	if (!ret) {
		g_print ("failed to read: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	g_print ("done!\n");
	retval = 0;
out:
	if (output != NULL)
		g_string_free (output, TRUE);
	g_free (data);
	g_strfreev (split);
	return retval;
}

