/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2013 Richard Hughes <richard@hughsie.com>
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
#include <lcms2.h>
#include <stdlib.h>
#include <colord/colord.h>

#define CD_PROFILE_DEFAULT_COPYRIGHT_STRING	"This profile is free of known copyright restrictions."

typedef struct {
	GOptionContext		*context;
	GPtrArray		*cmd_array;
	CdClient		*client;
	CdIcc			*icc;
	gchar			*locale;
	gboolean		 rewrite_file;
} CdUtilPrivate;

typedef gboolean (*CdUtilPrivateCb)	(CdUtilPrivate	*util,
					 gchar		**values,
					 GError		**error);

typedef struct {
	gchar		*name;
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
cd_util_add (GPtrArray *array, const gchar *name, const gchar *description, CdUtilPrivateCb callback)
{
	CdUtilItem *item;
	gchar **names;
	guint i;

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
		item->callback = callback;
		g_ptr_array_add (array, item);
	}
	g_strfreev (names);
}

/**
 * cd_util_get_descriptions:
 **/
static gchar *
cd_util_get_descriptions (GPtrArray *array)
{
	CdUtilItem *item;
	GString *string;
	guint i;
	guint j;
	guint len;
	guint max_len = 0;

	/* get maximum command length */
	for (i = 0; i < array->len; i++) {
		item = g_ptr_array_index (array, i);
		len = strlen (item->name);
		if (len > max_len)
			max_len = len;
	}

	/* ensure we're spaced by at least this */
	if (max_len < 19)
		max_len = 19;

	/* print each command */
	string = g_string_new ("");
	for (i = 0; i < array->len; i++) {
		item = g_ptr_array_index (array, i);
		g_string_append (string, "  ");
		g_string_append (string, item->name);
		len = strlen (item->name);
		for (j=len; j<max_len+3; j++)
			g_string_append_c (string, ' ');
		g_string_append (string, item->description);
		g_string_append_c (string, '\n');
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
	CdUtilItem *item;
	gboolean ret = FALSE;
	GString *string;
	guint i;

	/* find command */
	for (i = 0; i < priv->cmd_array->len; i++) {
		item = g_ptr_array_index (priv->cmd_array, i);
		if (g_strcmp0 (item->name, command) == 0) {
			ret = item->callback (priv, values, error);
			goto out;
		}
	}

	/* not found */
	string = g_string_new ("");
	/* TRANSLATORS: error message */
	g_string_append_printf (string, "%s\n",
				_("Command not found, valid commands are:"));
	for (i = 0; i < priv->cmd_array->len; i++) {
		item = g_ptr_array_index (priv->cmd_array, i);
		g_string_append_printf (string, " * %s\n", item->name);
	}
	g_set_error_literal (error, 1, 0, string->str);
	g_string_free (string, TRUE);
out:
	return ret;
}

/**
 * cd_util_set_copyright:
 **/
static gboolean
cd_util_set_copyright (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;

	/* check arguments */
	if (g_strv_length (values) != 2) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename' 'value'");
		goto out;
	}

	/* set new value */
	if (values[1][0] == '\0') {
		cd_icc_set_copyright (priv->icc,
				      priv->locale,
				      CD_PROFILE_DEFAULT_COPYRIGHT_STRING);
	} else {
		cd_icc_set_copyright (priv->icc,
				      priv->locale,
				      values[1]);
	}
out:
	return ret;
}

/**
 * cd_util_set_description:
 **/
static gboolean
cd_util_set_description (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;

	/* check arguments */
	if (g_strv_length (values) != 2) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename' 'value'");
		goto out;
	}

	/* set new value */
	cd_icc_set_description (priv->icc, priv->locale, values[1]);
out:
	return ret;
}

/**
 * cd_util_set_manufacturer:
 **/
static gboolean
cd_util_set_manufacturer (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;

	/* check arguments */
	if (g_strv_length (values) != 2) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename' 'value'");
		goto out;
	}

	/* set new value */
	cd_icc_set_manufacturer (priv->icc, priv->locale, values[1]);
out:
	return ret;
}

/**
 * cd_util_set_model:
 **/
static gboolean
cd_util_set_model (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;

	/* check arguments */
	if (g_strv_length (values) != 2) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename' 'value'");
		goto out;
	}

	/* set new value */
	cd_icc_set_model (priv->icc, priv->locale, values[1]);
out:
	return ret;
}

/**
 * cd_util_clear_metadata:
 **/
static gboolean
cd_util_clear_metadata (CdUtilPrivate *priv, gchar **values, GError **error)
{
	GHashTable *md;
	md = cd_icc_get_metadata (priv->icc);
	if (md == NULL)
		goto out;
	g_hash_table_remove_all (md);
	g_hash_table_unref (md);
out:
	return TRUE;
}

/**
 * cd_util_get_standard_space_filename:
 **/
static gchar *
cd_util_get_standard_space_filename (CdUtilPrivate *priv,
				     CdStandardSpace standard_space,
				     GError **error)
{
	CdProfile *profile_tmp = NULL;
	gboolean ret;
	gchar *filename = NULL;

	/* try to find */
	ret = cd_client_connect_sync (priv->client, NULL, error);
	if (!ret)
		goto out;
	profile_tmp = cd_client_get_standard_space_sync (priv->client,
							 standard_space,
							 NULL,
							 error);
	if (profile_tmp == NULL)
		goto out;

	/* get filename */
	ret = cd_profile_connect_sync (profile_tmp, NULL, error);
	if (!ret)
		goto out;
	filename = g_strdup (cd_profile_get_filename (profile_tmp));
out:
	if (profile_tmp != NULL)
		g_object_unref (profile_tmp);
	return filename;
}

/**
 * cd_util_get_coverage:
 * @profile: A valid #CdUtil
 * @filename_in: A filename to proof against
 * @error: A #GError, or %NULL
 *
 * Gets the gamut coverage of two profiles
 *
 * Return value: A positive value for success, or -1.0 for error.
 **/
static gdouble
cd_util_get_coverage (cmsHPROFILE profile_proof,
		      const gchar *filename_in,
		      GError **error)
{
	gdouble coverage = -1.0;
	gboolean ret;
	GFile *file = NULL;
	CdIcc *icc = NULL;
	CdIcc *icc_ref;

	/* load proofing profile */
	icc_ref = cd_icc_new ();
	ret = cd_icc_load_handle (icc_ref, profile_proof,
				  CD_ICC_LOAD_FLAGS_NONE, error);
	if (!ret)
		goto out;

	/* load target profile */
	icc = cd_icc_new ();
	file = g_file_new_for_path (filename_in);
	ret = cd_icc_load_file (icc, file, CD_ICC_LOAD_FLAGS_NONE, NULL, error);
	if (!ret)
		goto out;

	/* get coverage */
	ret = cd_icc_utils_get_coverage (icc, icc_ref, &coverage, error);
	if (!ret)
		goto out;
out:
	if (file != NULL)
		g_object_unref (file);
	if (icc != NULL)
		g_object_unref (icc);
	g_object_unref (icc_ref);
	return ret;
}

/**
 * cd_util_get_profile_coverage:
 **/
static gdouble
cd_util_get_profile_coverage (CdUtilPrivate *priv,
			      CdStandardSpace standard_space,
			      GError **error)
{
	gchar *filename = NULL;
	gdouble coverage = -1.0f;
	cmsHPROFILE profile;

	/* get the correct standard space */
	filename = cd_util_get_standard_space_filename (priv,
							standard_space,
							error);
	if (filename == NULL)
		goto out;

	/* work out the coverage */
	profile = cd_icc_get_handle (priv->icc);
	coverage = cd_util_get_coverage (profile, filename, error);
	if (coverage < 0.0f)
		goto out;
out:
	g_free (filename);
	return coverage;
}

/**
 * cd_util_set_version:
 **/
static gboolean
cd_util_set_version (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;
	gchar *endptr = NULL;
	gdouble version;

	/* check arguments */
	if (g_strv_length (values) != 2) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename' 'version'");
		goto out;
	}

	/* get version */
	version = g_ascii_strtod (values[1], &endptr);
	if (endptr != NULL && endptr[0] != '\0') {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to parse version: '%s'",
			     values[1]);
		goto out;
	}
	if (version < 1.0 || version > 6.0) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "invalid version %f", version);
		goto out;
	}

	/* set version */
	cd_icc_set_version (priv->icc, version);
out:
	return ret;
}

/**
 * cd_util_export_tag_data:
 **/
static gboolean
cd_util_export_tag_data (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;
	GBytes *data = NULL;
	gchar *out_fn = NULL;

	/* check arguments */
	if (g_strv_length (values) != 2) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename' 'tag'");
		goto out;
	}

	/* get data */
	data = cd_icc_get_tag_data (priv->icc, values[1], error);
	if (data == NULL) {
		ret = FALSE;
		goto out;
	}

	/* save to file */
	out_fn = g_strdup_printf ("./%s.bin", values[1]);
	ret = g_file_set_contents (out_fn,
				   g_bytes_get_data (data, NULL),
				   g_bytes_get_size (data),
				   error);
	if (!ret)
		goto out;
	g_print ("Wrote %s\n", out_fn);
	priv->rewrite_file = FALSE;
out:
	if (data != NULL)
		g_bytes_unref (data);
	g_free (out_fn);
	return ret;
}

/**
 * cd_util_set_fix_metadata:
 **/
static gboolean
cd_util_set_fix_metadata (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;
	gchar *coverage_tmp;
	gdouble coverage;

	/* check arguments */
	if (g_strv_length (values) != 1) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename'");
		goto out;
	}

	/* get coverages of common spaces */
	if (cd_icc_get_colorspace (priv->icc) == CD_COLORSPACE_RGB) {

		/* get the gamut coverage for sRGB */
		coverage = cd_util_get_profile_coverage (priv,
							 CD_STANDARD_SPACE_ADOBE_RGB,
							 error);
		if (coverage < 0.0) {
			ret = FALSE;
			goto out;
		}
		coverage_tmp = g_strdup_printf ("%f", coverage);
		cd_icc_add_metadata (priv->icc,
				     "GAMUT_coverage(adobe-rgb)",
				     coverage_tmp);
		g_free (coverage_tmp);
		g_debug ("coverage of AdobeRGB: %f%%", coverage * 100.0f);

		/* get the gamut coverage for AdobeRGB */
		coverage = cd_util_get_profile_coverage (priv,
							 CD_STANDARD_SPACE_SRGB,
							 error);
		if (coverage < 0.0) {
			ret = FALSE;
			goto out;
		}
		coverage_tmp = g_strdup_printf ("%.2f", coverage);
		cd_icc_add_metadata (priv->icc,
				     "GAMUT_coverage(srgb)",
				     coverage_tmp);
		g_free (coverage_tmp);
		g_debug ("coverage of sRGB: %f%%", coverage * 100.0f);
	}

	/* add CMS defines */
	cd_icc_add_metadata (priv->icc,
			     CD_PROFILE_METADATA_CMF_VERSION,
			     PACKAGE_VERSION);
out:
	return ret;
}

/**
 * cd_util_init_metadata:
 **/
static gboolean
cd_util_init_metadata (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;

	/* check arguments */
	if (g_strv_length (values) != 1) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename'");
		goto out;
	}

	/* add CMS defines */
	cd_icc_add_metadata (priv->icc,
			     CD_PROFILE_METADATA_CMF_PRODUCT,
			     PACKAGE_NAME);
	cd_icc_add_metadata (priv->icc,
			     CD_PROFILE_METADATA_CMF_BINARY,
			     "cd-fix-profile");
	cd_icc_add_metadata (priv->icc,
			     CD_PROFILE_METADATA_CMF_VERSION,
			     PACKAGE_VERSION);
out:
	return ret;
}

/**
 * cd_util_remove_metadata:
 **/
static gboolean
cd_util_remove_metadata (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;

	/* check arguments */
	if (g_strv_length (values) != 2) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename' 'key'");
		goto out;
	}

	/* remove entry */
	cd_icc_remove_metadata (priv->icc, values[1]);
out:
	return ret;
}

/**
 * cd_util_add_metadata:
 **/
static gboolean
cd_util_add_metadata (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;

	/* check arguments */
	if (g_strv_length (values) != 3) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename' 'key' 'value'");
		goto out;
	}

	/* add new entry */
	cd_icc_add_metadata (priv->icc, values[1], values[2]);
out:
	return ret;
}

/**
 * cd_util_extract_vcgt:
 **/
static gboolean
cd_util_extract_vcgt (CdUtilPrivate *priv, gchar **values, GError **error)
{
	cmsFloat32Number in;
	cmsHPROFILE lcms_profile;
	const cmsToneCurve **vcgt;
	gboolean ret = TRUE;
	guint i;
	guint size;

	/* check arguments */
	if (g_strv_length (values) != 2) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename' size'");
		goto out;
	}

	/* invalid size */
	size = atoi (values[1]);
	if (size <= 1 || size > 1024) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid size,expected 2-1024");
		goto out;
	}

	/* does profile have VCGT */
	lcms_profile = cd_icc_get_handle (priv->icc);
	vcgt = cmsReadTag (lcms_profile, cmsSigVcgtTag);
	if (vcgt == NULL || vcgt[0] == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "profile does not have any VCGT data");
		goto out;
	}

	/* output data */
	g_print ("idx,red,green,blue\n");
	for (i = 0; i < size; i++) {
		in = (gdouble) i / (gdouble) (size - 1);
		g_print ("%i,", i);
		g_print ("%f,", cmsEvalToneCurveFloat(vcgt[0], in));
		g_print ("%f,", cmsEvalToneCurveFloat(vcgt[1], in));
		g_print ("%f\n", cmsEvalToneCurveFloat(vcgt[2], in));
	}

	/* success */
	priv->rewrite_file = FALSE;
	ret = TRUE;
out:
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
 * cd_util_lcms_error_cb:
 **/
static void
cd_util_lcms_error_cb (cmsContext ContextID,
		       cmsUInt32Number errorcode,
		       const char *text)
{
	g_warning ("LCMS error %i: %s", errorcode, text);
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	CdUtilPrivate *priv;
	gboolean ret = TRUE;
	gboolean verbose = FALSE;
	gchar *cmd_descriptions = NULL;
	gchar *locale = NULL;
	GError *error = NULL;
	GFile *file = NULL;
	guint retval = 1;
	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: command line option */
			_("Show extra debugging information"), NULL },
		{ "locale", '\0', 0, G_OPTION_ARG_STRING, &locale,
			/* TRANSLATORS: command line option */
			_("The locale to use when setting localized text"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
	cmsSetLogErrorHandler (cd_util_lcms_error_cb);

	/* create helper object */
	priv = g_new0 (CdUtilPrivate, 1);
	priv->rewrite_file = TRUE;
	priv->client = cd_client_new ();

	/* add commands */
	priv->cmd_array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_util_item_free);
	cd_util_add (priv->cmd_array,
		     "extract-vcgt",
		     /* TRANSLATORS: command description */
		     _("Generate the VCGT calibration of a given size"),
		     cd_util_extract_vcgt);
	cd_util_add (priv->cmd_array,
		     "md-clear",
		     /* TRANSLATORS: command description */
		     _("Clear any metadata in the profile"),
		     cd_util_clear_metadata);
	cd_util_add (priv->cmd_array,
		     "md-init",
		     /* TRANSLATORS: command description */
		     _("Initialize any metadata for the profile"),
		     cd_util_init_metadata);
	cd_util_add (priv->cmd_array,
		     "md-add",
		     /* TRANSLATORS: command description */
		     _("Add a metadata item to the profile"),
		     cd_util_add_metadata);
	cd_util_add (priv->cmd_array,
		     "md-remove",
		     /* TRANSLATORS: command description */
		     _("Remove a metadata item from the profile"),
		     cd_util_remove_metadata);
	cd_util_add (priv->cmd_array,
		     "set-copyright",
		     /* TRANSLATORS: command description */
		     _("Sets the copyright string"),
		     cd_util_set_copyright);
	cd_util_add (priv->cmd_array,
		     "set-description",
		     /* TRANSLATORS: command description */
		     _("Sets the description string"),
		     cd_util_set_description);
	cd_util_add (priv->cmd_array,
		     "set-manufacturer",
		     /* TRANSLATORS: command description */
		     _("Sets the manufacturer string"),
		     cd_util_set_manufacturer);
	cd_util_add (priv->cmd_array,
		     "set-model",
		     /* TRANSLATORS: command description */
		     _("Sets the model string"),
		     cd_util_set_model);
	cd_util_add (priv->cmd_array,
		     "md-fix",
		     /* TRANSLATORS: command description */
		     _("Automatically fix metadata in the profile"),
		     cd_util_set_fix_metadata);
	cd_util_add (priv->cmd_array,
		     "set-version",
		     /* TRANSLATORS: command description */
		     _("Set the ICC profile version"),
		     cd_util_set_version);
	cd_util_add (priv->cmd_array,
		     "export-tag-data",
		     /* TRANSLATORS: command description */
		     _("Export the tag data"),
		     cd_util_export_tag_data);

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
		g_error_free (error);
		goto out;
	}

	/* use explicit locale */
	priv->locale = g_strdup (locale);

	/* set verbose? */
	if (verbose) {
		g_setenv ("COLORD_VERBOSE", "1", FALSE);
	} else {
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
				   cd_util_ignore_cb, NULL);
	}

	/* the first option is always the filename */
	if (argc < 2) {
		g_print ("%s\n", "Filename must be the first argument");
		goto out;
	}

	/* open file */
	file = g_file_new_for_path (argv[1]);
	priv->icc = cd_icc_new ();
	ret = cd_icc_load_file (priv->icc,
				file,
				CD_ICC_LOAD_FLAGS_ALL,
				NULL,
				&error);
	if (!ret) {
		g_print ("%s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* run the specified command */
	ret = cd_util_run (priv, argv[2], (gchar**) &argv[2], &error);
	if (!ret) {
		g_print ("%s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* save file */
	if (priv->rewrite_file) {
		ret = cd_icc_save_file (priv->icc,
					file,
					CD_ICC_SAVE_FLAGS_NONE,
					NULL,
					&error);
		if (!ret) {
			g_print ("%s\n", error->message);
			g_error_free (error);
			goto out;
		}
	}

	/* success */
	retval = 0;
out:
	if (priv != NULL) {
		if (priv->cmd_array != NULL)
			g_ptr_array_unref (priv->cmd_array);
		g_option_context_free (priv->context);
		if (priv->icc != NULL)
			g_object_unref (priv->icc);
		g_object_unref (priv->client);
		g_free (priv->locale);
		g_free (priv);
	}
	if (file != NULL)
		g_object_unref (file);
	g_free (locale);
	g_free (cmd_descriptions);
	return retval;
}

