/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2012 Richard Hughes <richard@hughsie.com>
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

#include "cd-enum.h"
#include "cd-common.h"
#include "cd-lcms-helpers.h"

typedef struct {
	GOptionContext		*context;
	GPtrArray		*cmd_array;
} ChUtilPrivate;

typedef gboolean (*ChUtilPrivateCb)	(ChUtilPrivate	*util,
					 gchar		**values,
					 GError		**error);

typedef struct {
	gchar		*name;
	gchar		*description;
	ChUtilPrivateCb	 callback;
} ChUtilItem;

/**
 * ch_util_item_free:
 **/
static void
ch_util_item_free (ChUtilItem *item)
{
	g_free (item->name);
	g_free (item->description);
	g_free (item);
}

/*
 * cd_sort_command_name_cb:
 */
static gint
cd_sort_command_name_cb (ChUtilItem **item1, ChUtilItem **item2)
{
	return g_strcmp0 ((*item1)->name, (*item2)->name);
}

/**
 * ch_util_add:
 **/
static void
ch_util_add (GPtrArray *array, const gchar *name, const gchar *description, ChUtilPrivateCb callback)
{
	ChUtilItem *item;
	gchar **names;
	guint i;

	/* add each one */
	names = g_strsplit (name, ",", -1);
	for (i=0; names[i] != NULL; i++) {
		item = g_new0 (ChUtilItem, 1);
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
 * ch_util_get_descriptions:
 **/
static gchar *
ch_util_get_descriptions (GPtrArray *array)
{
	ChUtilItem *item;
	GString *string;
	guint i;
	guint j;
	guint len;
	guint max_len = 0;

	/* get maximum command length */
	for (i=0; i<array->len; i++) {
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
	for (i=0; i<array->len; i++) {
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
 * ch_util_run:
 **/
static gboolean
ch_util_run (ChUtilPrivate *priv, const gchar *command, gchar **values, GError **error)
{
	ChUtilItem *item;
	gboolean ret = FALSE;
	GString *string;
	guint i;

	/* find command */
	for (i=0; i<priv->cmd_array->len; i++) {
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
	for (i=0; i<priv->cmd_array->len; i++) {
		item = g_ptr_array_index (priv->cmd_array, i);
		g_string_append_printf (string, " * %s\n", item->name);
	}
	g_set_error_literal (error, 1, 0, string->str);
	g_string_free (string, TRUE);
out:
	return ret;
}

/**
 * ch_util_profile_read:
 **/
static cmsHPROFILE
ch_util_profile_read (const gchar *filename, GError **error)
{
	cmsHPROFILE lcms_profile = NULL;
	gboolean ret;
	gchar *data = NULL;
	gsize len;

	ret = g_file_get_contents (filename, &data, &len, error);
	if (!ret)
		goto out;

	lcms_profile = cmsOpenProfileFromMem (data, len);
	if (lcms_profile == NULL) {
		g_set_error (error, 1, 0,
			     "failed to open profile %s",
			     filename);
		goto out;
	}
out:
	g_free (data);
	return lcms_profile;
}

/**
 * ch_util_profile_write:
 **/
static gboolean
ch_util_profile_write (cmsHPROFILE lcms_profile, const gchar *filename, GError **error)
{
	gboolean ret;

	/* write profile id */
	ret = cmsMD5computeID (lcms_profile);
	if (!ret) {
		g_set_error (error, 1, 0,
			     "failed to compute profile id for %s",
			     filename);
		goto out;
	}

	/* save file */
	ret = cmsSaveProfileToFile (lcms_profile, filename);
	if (!ret) {
		g_set_error (error, 1, 0,
			     "failed to save profile to %s",
			     filename);
		goto out;
	}
out:
	return ret;
}

/**
 * ch_util_profile_set_text_acsii:
 **/
static gboolean
ch_util_profile_set_text_acsii (cmsHPROFILE lcms_profile,
				cmsTagSignature sig,
				const gchar *value,
				GError **error)
{
	gboolean ret;

	ret = _cmsWriteTagTextAscii (lcms_profile, sig, value);
	if (!ret) {
		g_set_error (error, 1, 0,
			     "failed to write '%s'",
			     value);
		goto out;
	}
out:
	return ret;
}

/**
 * ch_util_set_info_text:
 **/
static gboolean
ch_util_set_info_text (ChUtilPrivate *priv, cmsTagSignature sig, gchar **values, GError **error)
{
	cmsHPROFILE lcms_profile = NULL;
	gboolean ret = TRUE;

	/* check arguments */
	if (g_strv_length (values) != 2) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'value' 'filename'");
		goto out;
	}

	/* open profile */
	lcms_profile = ch_util_profile_read (values[1], error);
	if (lcms_profile == NULL) {
		ret = FALSE;
		goto out;
	}

	/* update value */
	ret = ch_util_profile_set_text_acsii (lcms_profile,
					      sig,
					      values[0],
					      error);
	if (!ret)
		goto out;

	/* write new file */
	ret = ch_util_profile_write (lcms_profile, values[1], error);
	if (!ret)
		goto out;
out:
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	return ret;
}

/**
 * ch_util_set_copyright:
 **/
static gboolean
ch_util_set_copyright (ChUtilPrivate *priv, gchar **values, GError **error)
{
	return ch_util_set_info_text (priv, cmsInfoCopyright, values, error);
}

/**
 * ch_util_set_description:
 **/
static gboolean
ch_util_set_description (ChUtilPrivate *priv, gchar **values, GError **error)
{
	return ch_util_set_info_text (priv, cmsInfoDescription, values, error);
}

/**
 * ch_util_set_manufacturer:
 **/
static gboolean
ch_util_set_manufacturer (ChUtilPrivate *priv, gchar **values, GError **error)
{
	return ch_util_set_info_text (priv, cmsInfoManufacturer, values, error);
}

/**
 * ch_util_set_model:
 **/
static gboolean
ch_util_set_model (ChUtilPrivate *priv, gchar **values, GError **error)
{
	return ch_util_set_info_text (priv, cmsInfoModel, values, error);
}

/**
 * ch_util_clear_metadata:
 **/
static gboolean
ch_util_clear_metadata (ChUtilPrivate *priv, gchar **values, GError **error)
{
	cmsHANDLE dict = NULL;
	cmsHPROFILE lcms_profile = NULL;
	gboolean ret = TRUE;

	/* check arguments */
	if (g_strv_length (values) != 1) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename'");
		goto out;
	}

	/* open profile */
	lcms_profile = ch_util_profile_read (values[0], error);
	if (lcms_profile == NULL) {
		ret = FALSE;
		goto out;
	}

	/* clear dict */
	ret = cmsWriteTag (lcms_profile, cmsSigMetaTag, dict);
	if (!ret) {
		g_set_error_literal (error, 1, 0,
				     "cannot write empty dict tag");
		goto out;
	}

	/* write new file */
	ret = ch_util_profile_write (lcms_profile, values[0], error);
	if (!ret)
		goto out;
out:
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	return ret;
}

/**
 * ch_util_init_metadata:
 **/
static gboolean
ch_util_init_metadata (ChUtilPrivate *priv, gchar **values, GError **error)
{
	cmsHANDLE dict_new = NULL;
	cmsHANDLE dict_old = NULL;
	cmsHPROFILE lcms_profile = NULL;
	const cmsDICTentry *entry;
	gboolean ret = TRUE;
	gchar name[1024];

	/* check arguments */
	if (g_strv_length (values) != 1) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename'");
		goto out;
	}

	/* open profile */
	lcms_profile = ch_util_profile_read (values[0], error);
	if (lcms_profile == NULL) {
		ret = FALSE;
		goto out;
	}

	/* copy everything except the CMF keys */
	dict_old = cmsReadTag (lcms_profile, cmsSigMetaTag);
	if (dict_old == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "no metadata present");
		goto out;
	}

	/* copy, but ignore the key */
	dict_new = cmsDictAlloc (NULL);
	for (entry = cmsDictGetEntryList (dict_old);
	     entry != NULL;
	     entry = cmsDictNextEntry (entry)) {
		wcstombs (name, entry->Name, sizeof (name));
		if (g_strcmp0 (name, CD_PROFILE_METADATA_CMF_PRODUCT) == 0)
			continue;
		if (g_strcmp0 (name, CD_PROFILE_METADATA_CMF_BINARY) == 0)
			continue;
		if (g_strcmp0 (name, CD_PROFILE_METADATA_CMF_VERSION) == 0)
			continue;
		cmsDictAddEntry (dict_new,
				 entry->Name,
				 entry->Value,
				 NULL,
				 NULL);
	}

	/* add CMS defines */
	_cmsDictAddEntryAscii (dict_new,
			       CD_PROFILE_METADATA_CMF_PRODUCT,
			       PACKAGE_NAME);
	_cmsDictAddEntryAscii (dict_new,
			       CD_PROFILE_METADATA_CMF_BINARY,
			       "cd-fix-profile");
	_cmsDictAddEntryAscii (dict_new,
			       CD_PROFILE_METADATA_CMF_VERSION,
			       PACKAGE_VERSION);
	ret = cmsWriteTag (lcms_profile, cmsSigMetaTag, dict_new);
	if (!ret) {
		g_set_error_literal (error, 1, 0,
				     "cannot initialize dict tag");
		goto out;
	}

	/* write new file */
	ret = ch_util_profile_write (lcms_profile, values[0], error);
	if (!ret)
		goto out;
out:
	if (dict_new != NULL)
		cmsDictFree (dict_new);
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	return ret;
}

/**
 * ch_util_remove_metadata:
 **/
static gboolean
ch_util_remove_metadata (ChUtilPrivate *priv, gchar **values, GError **error)
{
	cmsHANDLE dict_new = NULL;
	cmsHANDLE dict_old = NULL;
	cmsHPROFILE lcms_profile = NULL;
	const cmsDICTentry *entry;
	gboolean ret = TRUE;
	gchar name[1024];

	/* check arguments */
	if (g_strv_length (values) != 2) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'key' 'filename'");
		goto out;
	}

	/* open profile */
	lcms_profile = ch_util_profile_read (values[1], error);
	if (lcms_profile == NULL) {
		ret = FALSE;
		goto out;
	}

	/* copy everything except the key */
	dict_old = cmsReadTag (lcms_profile, cmsSigMetaTag);
	if (dict_old == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "no metadata present");
		goto out;
	}

	/* copy, but ignore the key */
	dict_new = cmsDictAlloc (NULL);
	for (entry = cmsDictGetEntryList (dict_old);
	     entry != NULL;
	     entry = cmsDictNextEntry (entry)) {
		wcstombs (name, entry->Name, sizeof (name));
		if (g_strcmp0 (name, values[0]) == 0)
			continue;
		cmsDictAddEntry (dict_new,
				 entry->Name,
				 entry->Value,
				 NULL,
				 NULL);
	}

	/* write the new dict */
	ret = cmsWriteTag (lcms_profile, cmsSigMetaTag, dict_new);
	if (!ret) {
		g_set_error_literal (error, 1, 0,
				     "cannot initialize dict tag");
		goto out;
	}

	/* write new file */
	ret = ch_util_profile_write (lcms_profile, values[1], error);
	if (!ret)
		goto out;
out:
	if (dict_new != NULL)
		cmsDictFree (dict_new);
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	return ret;
}

/**
 * ch_util_add_metadata:
 **/
static gboolean
ch_util_add_metadata (ChUtilPrivate *priv, gchar **values, GError **error)
{
	cmsHANDLE dict_new = NULL;
	cmsHANDLE dict_old = NULL;
	cmsHPROFILE lcms_profile = NULL;
	const cmsDICTentry *entry;
	gboolean ret = TRUE;
	gchar name[1024];

	/* check arguments */
	if (g_strv_length (values) != 3) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'key' 'value' 'filename'");
		goto out;
	}

	/* open profile */
	lcms_profile = ch_util_profile_read (values[2], error);
	if (lcms_profile == NULL) {
		ret = FALSE;
		goto out;
	}

	/* add CMS defines */
	dict_old = cmsReadTag (lcms_profile, cmsSigMetaTag);
	if (dict_old == NULL)
		dict_old = cmsDictAlloc (NULL);

	/* copy, but ignore the key */
	dict_new = cmsDictAlloc (NULL);
	for (entry = cmsDictGetEntryList (dict_old);
	     entry != NULL;
	     entry = cmsDictNextEntry (entry)) {
		wcstombs (name, entry->Name, sizeof (name));
		if (g_strcmp0 (name, values[0]) == 0)
			continue;
		cmsDictAddEntry (dict_new,
				 entry->Name,
				 entry->Value,
				 NULL,
				 NULL);
	}

	/* add new entry */
	_cmsDictAddEntryAscii (dict_new,
			       values[0],
			       values[1]);
	ret = cmsWriteTag (lcms_profile, cmsSigMetaTag, dict_new);
	if (!ret) {
		g_set_error_literal (error, 1, 0,
				     "cannot write new dict tag");
		goto out;
	}

	/* write new file */
	ret = ch_util_profile_write (lcms_profile, values[2], error);
	if (!ret)
		goto out;
out:
	if (dict_new != NULL)
		cmsDictFree (dict_new);
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	return ret;
}

/**
 * ch_util_dump:
 **/
static gboolean
ch_util_dump (ChUtilPrivate *priv, gchar **values, GError **error)
{
	cmsHANDLE dict;
	cmsHPROFILE lcms_profile = NULL;
	const cmsDICTentry* entry;
	gboolean ret = TRUE;
	gchar ascii_name[1024];
	gchar ascii_value[1024];

	/* check arguments */
	if (g_strv_length (values) != 1) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename'");
		goto out;
	}

	/* set value */
	g_print ("Using filename %s\n", values[0]);
	lcms_profile = cmsOpenProfileFromFile (values[0], "r");
	if (lcms_profile == NULL) {
		g_warning ("failed to open profile %s", values[0]);
		ret = FALSE;
		goto out;
	}
	ret = cmsGetProfileInfoASCII (lcms_profile, cmsInfoDescription, "en", "US", ascii_name, 1024);
	if (ret)
		g_print ("%s\t%s\n", _("Description"), ascii_name);
	ret = cmsGetProfileInfoASCII (lcms_profile, cmsInfoManufacturer, "en", "US", ascii_name, 1024);
	if (ret)
		g_print ("%s\t%s\n", _("Manufacturer"), ascii_name);
	ret = cmsGetProfileInfoASCII (lcms_profile, cmsInfoModel, "en", "US", ascii_name, 1024);
	if (ret)
		g_print ("%s\t%s\n", _("Model"), ascii_name);
	ret = cmsGetProfileInfoASCII (lcms_profile, cmsInfoCopyright, "en", "US", ascii_name, 1024);
	if (ret)
		g_print ("%s\t%s\n", _("Copyright"), ascii_name);

	/* does profile have metadata? */
	dict = cmsReadTag (lcms_profile, cmsSigMetaTag);
	if (dict == NULL) {
		g_print ("%s\n", _("No metadata"));
	} else {
		for (entry = cmsDictGetEntryList (dict);
		     entry != NULL;
		     entry = cmsDictNextEntry (entry)) {

			/* convert from wchar_t to char */
			wcstombs (ascii_name, entry->Name, sizeof (ascii_name));
			wcstombs (ascii_value, entry->Value, sizeof (ascii_value));
			g_print ("%s %s\t=\t%s\n",
				 _("Metadata"), ascii_name, ascii_value);
		}
	}

	/* success */
	ret = TRUE;
out:
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	return ret;
}

/**
 * ch_util_ignore_cb:
 **/
static void
ch_util_ignore_cb (const gchar *log_domain, GLogLevelFlags log_level,
		   const gchar *message, gpointer user_data)
{
}

/**
 * ch_util_lcms_error_cb:
 **/
static void
ch_util_lcms_error_cb (cmsContext ContextID,
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
	ChUtilPrivate *priv;
	gboolean ret;
	gboolean verbose = FALSE;
	gchar *cmd_descriptions = NULL;
	GError *error = NULL;
	guint retval = 1;
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
	cmsSetLogErrorHandler (ch_util_lcms_error_cb);

	g_type_init ();

	/* create helper object */
	priv = g_new0 (ChUtilPrivate, 1);

	/* add commands */
	priv->cmd_array = g_ptr_array_new_with_free_func ((GDestroyNotify) ch_util_item_free);
	ch_util_add (priv->cmd_array,
		     "dump",
		     /* TRANSLATORS: command description */
		     _("Show all the details about the profile"),
		     ch_util_dump);
	ch_util_add (priv->cmd_array,
		     "md-clear",
		     /* TRANSLATORS: command description */
		     _("Clear any metadata in the profile"),
		     ch_util_clear_metadata);
	ch_util_add (priv->cmd_array,
		     "md-init",
		     /* TRANSLATORS: command description */
		     _("Initialize any metadata for the profile"),
		     ch_util_init_metadata);
	ch_util_add (priv->cmd_array,
		     "md-add",
		     /* TRANSLATORS: command description */
		     _("Add a metadata item to the profile"),
		     ch_util_add_metadata);
	ch_util_add (priv->cmd_array,
		     "md-remove",
		     /* TRANSLATORS: command description */
		     _("Remove a metadata item from the profile"),
		     ch_util_remove_metadata);
	ch_util_add (priv->cmd_array,
		     "set-copyright",
		     /* TRANSLATORS: command description */
		     _("Sets the copyright string"),
		     ch_util_set_copyright);
	ch_util_add (priv->cmd_array,
		     "set-description",
		     /* TRANSLATORS: command description */
		     _("Sets the description string"),
		     ch_util_set_description);
	ch_util_add (priv->cmd_array,
		     "set-manufacturer",
		     /* TRANSLATORS: command description */
		     _("Sets the manufacturer string"),
		     ch_util_set_manufacturer);
	ch_util_add (priv->cmd_array,
		     "set-model",
		     /* TRANSLATORS: command description */
		     _("Sets the model string"),
		     ch_util_set_model);

	/* sort by command name */
	g_ptr_array_sort (priv->cmd_array,
			  (GCompareFunc) cd_sort_command_name_cb);

	/* get a list of the commands */
	priv->context = g_option_context_new (NULL);
	cmd_descriptions = ch_util_get_descriptions (priv->cmd_array);
	g_option_context_set_summary (priv->context, cmd_descriptions);

	/* TRANSLATORS: program name */
	g_set_application_name (_("Color Management"));
	g_option_context_add_main_entries (priv->context, options, NULL);
	g_option_context_parse (priv->context, &argc, &argv, NULL);

	/* set verbose? */
	if (verbose) {
		g_setenv ("COLORD_VERBOSE", "1", FALSE);
	} else {
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
				   ch_util_ignore_cb, NULL);
	}

	/* get connection to colord */

	/* run the specified command */
	ret = ch_util_run (priv, argv[1], (gchar**) &argv[2], &error);
	if (!ret) {
		g_print ("%s\n", error->message);
		g_error_free (error);
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
	g_free (cmd_descriptions);
	return retval;
}

