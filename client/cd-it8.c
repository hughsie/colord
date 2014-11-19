/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2014 Richard Hughes <richard@hughsie.com>
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

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <colord/colord.h>

#include "cd-cleanup.h"

#define CD_ERROR			1
#define CD_ERROR_INVALID_ARGUMENTS	0
#define CD_ERROR_NO_SUCH_CMD		1

typedef struct {
	GOptionContext		*context;
	GPtrArray		*cmd_array;
} CdUtilPrivate;

typedef gboolean (*CdUtilPrivateCb)	(CdUtilPrivate	*util,
					 gchar		**values,
					 GError		**error);

typedef struct {
	gchar		*name;
	gchar		*arguments;
	gchar		*description;
	CdUtilPrivateCb	 callback;
} CdUtilItem;

/**
 * cd_util_item_free:
 **/
static void
cd_util_item_free (CdUtilItem *item)
{
	g_free (item->name);
	g_free (item->arguments);
	g_free (item->description);
	g_free (item);
}

/*
 * cd_sort_command_name_cb:
 */
static gint
cd_sort_command_name_cb (CdUtilItem **item1, CdUtilItem **item2)
{
	return g_strcmp0 ((*item1)->name, (*item2)->name);
}

/**
 * cd_util_add:
 **/
static void
cd_util_add (GPtrArray *array,
	     const gchar *name,
	     const gchar *arguments,
	     const gchar *description,
	     CdUtilPrivateCb callback)
{
	guint i;
	CdUtilItem *item;
	_cleanup_strv_free_ gchar **names = NULL;

	g_return_if_fail (name != NULL);
	g_return_if_fail (description != NULL);
	g_return_if_fail (callback != NULL);

	/* add each one */
	names = g_strsplit (name, ",", -1);
	for (i = 0; names[i] != NULL; i++) {
		item = g_new0 (CdUtilItem, 1);
		item->name = g_strdup (names[i]);
		if (i == 0) {
			item->description = g_strdup (description);
		} else {
			/* TRANSLATORS: this is a command alias */
			item->description = g_strdup_printf (_("Alias to %s"),
							     names[0]);
		}
		item->arguments = g_strdup (arguments);
		item->callback = callback;
		g_ptr_array_add (array, item);
	}
}

/**
 * cd_util_get_descriptions:
 **/
static gchar *
cd_util_get_descriptions (GPtrArray *array)
{
	guint i;
	guint j;
	guint len;
	const guint max_len = 35;
	CdUtilItem *item;
	GString *string;

	/* print each command */
	string = g_string_new ("");
	for (i = 0; i < array->len; i++) {
		item = g_ptr_array_index (array, i);
		g_string_append (string, "  ");
		g_string_append (string, item->name);
		len = strlen (item->name) + 2;
		if (item->arguments != NULL) {
			g_string_append (string, " ");
			g_string_append (string, item->arguments);
			len += strlen (item->arguments) + 1;
		}
		if (len < max_len) {
			for (j = len; j < max_len + 1; j++)
				g_string_append_c (string, ' ');
			g_string_append (string, item->description);
			g_string_append_c (string, '\n');
		} else {
			g_string_append_c (string, '\n');
			for (j = 0; j < max_len + 1; j++)
				g_string_append_c (string, ' ');
			g_string_append (string, item->description);
			g_string_append_c (string, '\n');
		}
	}

	/* remove trailing newline */
	if (string->len > 0)
		g_string_set_size (string, string->len - 1);

	return g_string_free (string, FALSE);
}

/**
 * cd_util_run:
 **/
static gboolean
cd_util_run (CdUtilPrivate *priv, const gchar *command, gchar **values, GError **error)
{
	guint i;
	CdUtilItem *item;
	_cleanup_string_free_ GString *string = NULL;

	/* find command */
	for (i = 0; i < priv->cmd_array->len; i++) {
		item = g_ptr_array_index (priv->cmd_array, i);
		if (g_strcmp0 (item->name, command) == 0)
			return item->callback (priv, values, error);
	}

	/* not found */
	string = g_string_new ("");
	/* TRANSLATORS: error message */
	g_string_append_printf (string, "%s\n",
				_("Command not found, valid commands are:"));
	for (i = 0; i < priv->cmd_array->len; i++) {
		item = g_ptr_array_index (priv->cmd_array, i);
		g_string_append_printf (string, " * %s %s\n",
					item->name,
					item->arguments ? item->arguments : "");
	}
	g_set_error_literal (error, CD_ERROR, CD_ERROR_NO_SUCH_CMD, string->str);
	return FALSE;
}

typedef struct {
	guint		 nm;
	CdColorXYZ	 xyz;
} CdSpectrumData;

/**
 * cd_csv2cmf_data_free:
 **/
static void
cd_csv2cmf_data_free (CdSpectrumData *data)
{
	g_slice_free (CdSpectrumData, data);
}

/**
 * cd_util_create_cmf:
 **/
static gboolean
cd_util_create_cmf (CdUtilPrivate *priv,
		    gchar **values,
		    GError **error)
{
	gboolean ret = TRUE;
	CdSpectrum *spectrum[3] = { NULL, NULL, NULL };
	CdSpectrumData *tmp;
	gchar *dot;
	guint i;
	gdouble norm;
	_cleanup_free_ gchar *data = NULL;
	_cleanup_free_ gchar *title = NULL;
	_cleanup_object_unref_ CdIt8 *cmf = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *array = NULL;
	_cleanup_strv_free_ gchar **lines = NULL;

	if (g_strv_length (values) != 3) {
		g_set_error_literal (error,
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected: "
				     "file.cmf file.csv norm");
		return FALSE;
	}

	/* get data */
	if (!g_file_get_contents (values[1], &data, NULL, error))
		return FALSE;

	/* parse lines */
	norm = g_strtod (values[2], NULL);
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_csv2cmf_data_free);
	lines = g_strsplit (data, "\n", -1);
	for (i = 0; lines[i] != NULL; i++) {
		_cleanup_strv_free_ gchar **split = NULL;
		if (lines[i][0] == '\0')
			continue;
		if (lines[i][0] == '#')
			continue;
		split = g_strsplit_set (lines[i], ", \t", -1);
		if (g_strv_length (split) == 4) {
			tmp = g_slice_new0 (CdSpectrumData);
			tmp->nm = atoi (split[0]);
			cd_color_xyz_set (&tmp->xyz,
					  g_strtod (split[1], NULL) / norm,
					  g_strtod (split[2], NULL) / norm,
					  g_strtod (split[3], NULL) / norm);
			g_ptr_array_add (array, tmp);
		} else {
			g_printerr ("Ignoring data line: %s", lines[i]);
		}
	}

	/* did we get enough data */
	if (array->len < 3) {
		g_set_error_literal (error,
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough data in the CSV file");
		return FALSE;
	}

	for (i = 0; i < 3; i++) {
		spectrum[i] = cd_spectrum_sized_new (array->len);
		cd_spectrum_set_norm (spectrum[i], 1.f);
	}

	/* set ID */
	cd_spectrum_set_id (spectrum[0], "X");
	cd_spectrum_set_id (spectrum[1], "Y");
	cd_spectrum_set_id (spectrum[2], "Z");

	/* get the first point */
	tmp = g_ptr_array_index (array, 0);
	for (i = 0; i < 3; i++)
		cd_spectrum_set_start (spectrum[i], tmp->nm);

	/* get the last point */
	tmp = g_ptr_array_index (array, array->len - 1);
	for (i = 0; i < 3; i++)
		cd_spectrum_set_end (spectrum[i], tmp->nm);

	/* add data to the spectra */
	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		cd_spectrum_add_value (spectrum[0], tmp->xyz.X);
		cd_spectrum_add_value (spectrum[1], tmp->xyz.Y);
		cd_spectrum_add_value (spectrum[2], tmp->xyz.Z);
	}

	/* add spectra to the CMF file */
	cmf = cd_it8_new_with_kind (CD_IT8_KIND_CMF);
	for (i = 0; i < 3; i++)
		cd_it8_add_spectrum (cmf, spectrum[i]);

	cd_it8_set_originator (cmf, "cd-it8");
	title = g_path_get_basename (values[1]);
	dot = g_strrstr (title, ".csv");
	if (dot != NULL)
		*dot = '\0';
	cd_it8_set_title (cmf, title);

	/* save */
	file = g_file_new_for_path (values[0]);
	ret = cd_it8_save_to_file (cmf, file, error);
	if (!ret)
		goto out;
out:
	for (i = 0; i < 3; i++) {
		if (spectrum[i] != NULL)
			cd_spectrum_free (spectrum[i]);
	}
	return ret;
}

/**
 * cd_util_calculate_ccmx:
 **/
static gboolean
cd_util_calculate_ccmx (CdUtilPrivate *priv,
			gchar **values,
			GError **error)
{
	gchar *tmp;
	_cleanup_free_ gchar *basename = NULL;
	_cleanup_object_unref_ CdIt8 *it8_ccmx = NULL;
	_cleanup_object_unref_ CdIt8 *it8_meas = NULL;
	_cleanup_object_unref_ CdIt8 *it8_ref = NULL;
	_cleanup_object_unref_ GFile *file_ccmx = NULL;
	_cleanup_object_unref_ GFile *file_meas = NULL;
	_cleanup_object_unref_ GFile *file_ref = NULL;

	/* check args */
	if (g_strv_length (values) != 3) {
		g_set_error_literal (error,
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected: file, file, file");
		return FALSE;
	}

	/* load reference */
	it8_ref = cd_it8_new ();
	file_ref = g_file_new_for_path (values[0]);
	if (!cd_it8_load_from_file (it8_ref, file_ref, error))
		return FALSE;

	/* load measurement */
	it8_meas = cd_it8_new ();
	file_meas = g_file_new_for_path (values[1]);
	if (!cd_it8_load_from_file (it8_meas, file_meas, error))
		return FALSE;

	/* caclulate correction matrix */
	it8_ccmx = cd_it8_new_with_kind (CD_IT8_KIND_CCMX);
	if (!cd_it8_utils_calculate_ccmx (it8_ref, it8_meas, it8_ccmx, error))
		return FALSE;

	/* save CCMX file */
	basename = g_path_get_basename (values[2]);
	tmp = g_strrstr (basename, ".");
	if (tmp != NULL)
		*tmp = '\0';
	cd_it8_add_option (it8_ccmx, "TYPE_FACTORY");
	cd_it8_set_title (it8_ccmx, basename);
	file_ccmx = g_file_new_for_path (values[2]);
	if (!cd_it8_save_to_file (it8_ccmx, file_ccmx, error))
		return FALSE;
	return TRUE;
}

/**
 * cd_util_create_sp:
 **/
static gboolean
cd_util_create_sp (CdUtilPrivate *priv,
		   gchar **values,
		   GError **error)
{
	CdSpectrum *spectrum = NULL;
	CdSpectrumData *tmp;
	gboolean ret = TRUE;
	gchar *dot;
	gdouble norm;
	guint i;
	_cleanup_free_ gchar *data = NULL;
	_cleanup_free_ gchar *title = NULL;
	_cleanup_object_unref_ CdIt8 *cmf = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *array = NULL;
	_cleanup_strv_free_ gchar **lines = NULL;

	if (g_strv_length (values) < 1) {
		g_set_error_literal (error,
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected: file");
		return FALSE;
	}

	/* get data */
	if (!g_file_get_contents (values[1], &data, NULL, error))
		return FALSE;

	/* parse lines */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_csv2cmf_data_free);
	lines = g_strsplit (data, "\n", -1);
	norm = g_strtod (values[2], NULL);
	for (i = 0; lines[i] != NULL; i++) {
		_cleanup_strv_free_ gchar **split = NULL;
		if (lines[i][0] == '\0')
			continue;
		if (lines[i][0] == '#')
			continue;
		split = g_strsplit_set (lines[i], ", \t", -1);
		if (g_strv_length (split) == 2) {
			tmp = g_slice_new0 (CdSpectrumData);
			tmp->nm = atoi (split[0]);
			cd_color_xyz_set (&tmp->xyz,
					  g_strtod (split[1], NULL) / norm,
					  0.f,
					  0.f);
			g_ptr_array_add (array, tmp);
		} else {
			g_printerr ("Ignoring data line: %s", lines[i]);
		}
	}

	/* did we get enough data */
	if (array->len < 3) {
		g_set_error_literal (error,
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough data in the CSV file");
		return FALSE;
	}

	spectrum = cd_spectrum_sized_new (array->len);
	cd_spectrum_set_norm (spectrum, 1.f);

	/* get the first point */
	tmp = g_ptr_array_index (array, 0);
	cd_spectrum_set_start (spectrum, tmp->nm);

	/* get the last point */
	tmp = g_ptr_array_index (array, array->len - 1);
	cd_spectrum_set_end (spectrum, tmp->nm);

	/* add data to the spectra */
	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		cd_spectrum_add_value (spectrum, tmp->xyz.X);
	}

	/* add spectra to the CMF file */
	cmf = cd_it8_new_with_kind (CD_IT8_KIND_SPECT);
	cd_it8_add_spectrum (cmf, spectrum);
	cd_it8_set_originator (cmf, "cd-it8");
	title = g_path_get_basename (values[1]);
	dot = g_strrstr (title, ".csv");
	if (dot != NULL)
		*dot = '\0';
	cd_it8_set_title (cmf, title);

	/* save */
	file = g_file_new_for_path (values[0]);
	ret = cd_it8_save_to_file (cmf, file, error);
	if (!ret)
		goto out;
out:
	if (spectrum != NULL)
		cd_spectrum_free (spectrum);
	return ret;
}

/**
 * cd_util_ignore_cb:
 **/
static void
cd_util_ignore_cb (const gchar *log_domain, GLogLevelFlags log_level,
		   const gchar *message, gpointer user_data)
{
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	CdUtilPrivate *priv;
	gboolean ret;
	gboolean verbose = FALSE;
	guint retval = 1;
	_cleanup_error_free_ GError *error = NULL;
	_cleanup_free_ gchar *cmd_descriptions = NULL;
	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: command line option */
			_("Show extra debugging information"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* create helper object */
	priv = g_new0 (CdUtilPrivate, 1);

	/* add commands */
	priv->cmd_array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_util_item_free);
	cd_util_add (priv->cmd_array,
		     "create-cmf",
		     "[OUTPUT.cmf] [INPUT.csv] [norm]",
		     /* TRANSLATORS: command description */
		     _("Create a CMF from CSV data"),
		     cd_util_create_cmf);
	cd_util_add (priv->cmd_array,
		     "create-sp",
		     "[OUTPUT.sp] [INPUT.csv] [norm]",
		     /* TRANSLATORS: command description */
		     _("Create a spectrum from CSV data"),
		     cd_util_create_sp);
	cd_util_add (priv->cmd_array,
		     "calculate-ccmx",
		     "[REFERENCE.ti3] [MEASURED.ti3] [OUTPUT.ccmx]",
		     /* TRANSLATORS: command description */
		     _("Create a CCMX from reference and measurement data"),
		     cd_util_calculate_ccmx);

	/* sort by command name */
	g_ptr_array_sort (priv->cmd_array,
			  (GCompareFunc) cd_sort_command_name_cb);

	/* get a list of the commands */
	priv->context = g_option_context_new (NULL);
	cmd_descriptions = cd_util_get_descriptions (priv->cmd_array);
	g_option_context_set_summary (priv->context, cmd_descriptions);

	/* TRANSLATORS: program name */
	g_set_application_name (_("Color Management"));
	g_option_context_add_main_entries (priv->context, options, NULL);
	ret = g_option_context_parse (priv->context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: the user didn't read the man page */
		g_print ("%s: %s\n",
			 _("Failed to parse arguments"),
			 error->message);
		goto out;
	}

	/* set verbose? */
	if (verbose) {
		g_setenv ("COLORD_VERBOSE", "1", FALSE);
	} else {
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
				   cd_util_ignore_cb, NULL);
	}

	/* run the specified command */
	ret = cd_util_run (priv, argv[1], (gchar**) &argv[2], &error);
	if (!ret) {
		if (g_error_matches (error, CD_ERROR, CD_ERROR_NO_SUCH_CMD)) {
			gchar *tmp;
			tmp = g_option_context_get_help (priv->context, TRUE, NULL);
			g_print ("%s", tmp);
			g_free (tmp);
		} else {
			g_print ("%s\n", error->message);
		}
		goto out;
	}

	/* success */
	retval = 0;
out:
	if (priv != NULL) {
		if (priv->cmd_array != NULL)
			g_ptr_array_unref (priv->cmd_array);
		g_option_context_free (priv->context);
		g_free (priv);
	}
	return retval;
}

