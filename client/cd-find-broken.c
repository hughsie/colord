/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Richard Hughes <richard@hughsie.com>
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

#include <stdlib.h>
#include <colord.h>
#include <locale.h>

typedef struct {
	GHashTable	*cmfbinary;
	GHashTable	*vendors;
	GHashTable	*vendors_broken;
	GHashTable	*vendors_no_serial;
	GString		*csv_all;
	GString		*csv_fail;
	guint		 has_serial_numbers;
} CdFindBrokenPriv;

/**
 * cd_find_broken_parse_filename:
 */
static gboolean
cd_find_broken_parse_filename (CdFindBrokenPriv *priv,
			       const gchar *filename,
			       GError **error)
{
	CdIcc *icc = NULL;
	CdProfileWarning warning;
	const gchar *tmp;
	GArray *warnings = NULL;
	gboolean ret;
	gchar *vendor = NULL;
	GFile *file = NULL;
	guint i;
	guint *val;

	/* load file */
	icc = cd_icc_new ();
	file = g_file_new_for_path (filename);
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_METADATA | CD_ICC_LOAD_FLAGS_PRIMARIES,
				NULL,
				error);
	if (!ret)
		goto out;

	/* append to CSV file */
	g_string_append_printf (priv->csv_all, "%s,\"%s\",\"%s\",%s,%s,%.1f,%s,%s\n",
				cd_icc_get_filename (icc),
				cd_icc_get_manufacturer (icc, NULL, NULL),
				cd_icc_get_model (icc, NULL, NULL),
				cd_icc_get_metadata_item (icc, CD_PROFILE_METADATA_DATA_SOURCE),
				cd_icc_get_metadata_item (icc, CD_PROFILE_METADATA_EDID_SERIAL),
				cd_icc_get_version (icc),
				cd_icc_get_metadata_item (icc, CD_PROFILE_METADATA_CMF_BINARY),
				cd_icc_get_metadata_item (icc, CD_PROFILE_METADATA_CMF_VERSION));

	/* get quirked vendor */
	tmp = cd_icc_get_manufacturer (icc, NULL, NULL);
	if (tmp == NULL)
		vendor = g_strdup ("Unknown");
	else
		vendor = cd_quirk_vendor_name (tmp);
	val = g_hash_table_lookup (priv->vendors, vendor);
	if (val == NULL) {
		val = g_new0 (guint, 1);
		g_hash_table_insert (priv->vendors, g_strdup (vendor), val);
	}
	(*val)++;

	/* count those with serial numbers */
	tmp = cd_icc_get_metadata_item (icc, CD_PROFILE_METADATA_EDID_SERIAL);
	if (tmp != NULL && tmp[0] != '\0') {
		priv->has_serial_numbers++;
	} else {
		val = g_hash_table_lookup (priv->vendors_no_serial, vendor);
		if (val == NULL) {
			val = g_new0 (guint, 1);
			g_hash_table_insert (priv->vendors_no_serial, g_strdup (vendor), val);
		}
		(*val)++;
	}

	/* get CMF binary */
	tmp = cd_icc_get_metadata_item (icc, CD_PROFILE_METADATA_CMF_BINARY);
	if (tmp == NULL)
		tmp = "Unknown";
	val = g_hash_table_lookup (priv->cmfbinary, tmp);
	if (val == NULL) {
		val = g_new0 (guint, 1);
		g_hash_table_insert (priv->cmfbinary, g_strdup (tmp), val);
	}
	(*val)++;

	/* any problems */
	warnings = cd_icc_get_warnings (icc);
	if (warnings->len == 0)
		goto out;

	/* count those with problems */
	val = g_hash_table_lookup (priv->vendors_broken, vendor);
	if (val == NULL) {
		val = g_new0 (guint, 1);
		g_hash_table_insert (priv->vendors_broken, g_strdup (vendor), val);
	}
	(*val)++;

	/* append to CSV file */
	g_string_append_printf (priv->csv_fail, "%s,\"%s\",\"%s\",",
				cd_icc_get_filename (icc),
				cd_icc_get_manufacturer (icc, NULL, NULL),
				cd_icc_get_model (icc, NULL, NULL));
	for (i = 0; i < warnings->len; i++) {
		warning = g_array_index (warnings, CdProfileWarning, i);
		g_string_append_printf (priv->csv_fail, "%s|",
					cd_profile_warning_to_string (warning));
	}
	priv->csv_fail->str[priv->csv_fail->len - 1] = '\n';
out:
	if (warnings != NULL)
		g_array_unref (warnings);
	g_object_unref (file);
	g_object_unref (icc);
	g_free (vendor);
	return ret;
}

/**
 * cd_find_broken_strcmp_func:
 */
static gint
cd_find_broken_strcmp_func (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 ((const gchar *) a, (const gchar *) b);
}

/**
 * main:
 */
int
main (int argc, char *argv[])
{
	CdFindBrokenPriv *priv = NULL;
	const gchar *fn_all = "./all.csv";
	const gchar *fn_failures = "./results.csv";
	gboolean ret;
	GError *error = NULL;
	gint retval = EXIT_FAILURE;
	GList *l;
	GList *list;
	guint i;
	guint total = 0;
	guint total_with_warnings = 0;
	guint *val;

	if (argc < 2) {
		g_warning ("usage: cd-find-broken.c filename, e.g. 'uploads/*'");
		goto out;
	}

	setlocale (LC_ALL, "");

	/* create CSV headers */
	priv = g_new0 (CdFindBrokenPriv, 1);
	priv->csv_all = g_string_new ("filename,vendor,model,serial,data_source,version,cmf_binary,cmf_version\n");
	priv->csv_fail = g_string_new ("filename,vendor,model,warnings\n");
	priv->vendors = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->vendors_no_serial = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->vendors_broken = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->cmfbinary = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->has_serial_numbers = 0;

	/* scan each file */
	for (i = 1; i < (guint) argc; i++) {
		ret = cd_find_broken_parse_filename (priv, argv[i], &error);
		if (!ret) {
			g_warning ("failed to parse %s: %s",
				   argv[i], error->message);
			g_clear_error (&error);
		}
	}

	/* print stats */
	total = argc - 1;
	for (i = 0; i < priv->csv_fail->len; i++) {
		if (priv->csv_fail->str[i] == '\n')
			total_with_warnings++;
	}
	total_with_warnings--;
	g_print ("Total profiles scanned: %i\n",  total);
	g_print ("Profiles with invalid or unlikely primaries: %i [%.1f%%]\n",
		 total_with_warnings,
		 (gdouble) 100.0f * total_with_warnings / total);
	g_print ("Profiles with valid serial numbers: %i [%.1f%%]\n",
		 priv->has_serial_numbers,
		 (gdouble) 100.0f * priv->has_serial_numbers / total);

	/* extract vendors */
	if (FALSE) {
		list = g_hash_table_get_keys (priv->vendors);
		list = g_list_sort (list, cd_find_broken_strcmp_func);
		g_print ("Vendor list:\n");
		for (l = list; l != NULL; l = l->next) {
			val = (guint *) g_hash_table_lookup (priv->vendors, l->data);
			g_print ("\"%s\",%i\n", (const gchar *) l->data, *val);
		}
		g_list_free (list);
	}

	/* extract vendors that ship broken primaries */
	if (FALSE) {
		list = g_hash_table_get_keys (priv->vendors_broken);
		list = g_list_sort (list, cd_find_broken_strcmp_func);
		g_print ("Vendors who ship broken primaries:\n");
		for (l = list; l != NULL; l = l->next) {
			val = (guint *) g_hash_table_lookup (priv->vendors_broken, l->data);
			g_print ("\"%s\",%i\n", (const gchar *) l->data, *val);
		}
		g_list_free (list);
	}

	/* extract vendors without serial numbers */
	if (FALSE) {
		list = g_hash_table_get_keys (priv->vendors_no_serial);
		list = g_list_sort (list, cd_find_broken_strcmp_func);
		g_print ("Vendors who don't write serial numbers:\n");
		for (l = list; l != NULL; l = l->next) {
			val = (guint *) g_hash_table_lookup (priv->vendors_no_serial, l->data);
			g_print ("\"%s\",%i\n", (const gchar *) l->data, *val);
		}
		g_list_free (list);
	}

	/* extract cmfbinary */
	if (FALSE) {
		list = g_hash_table_get_keys (priv->cmfbinary);
		g_print ("CMF list:\n");
		for (l = list; l != NULL; l = l->next) {
			val = (guint *) g_hash_table_lookup (priv->cmfbinary, l->data);
			g_print ("\"%s\",%i\n", (const gchar *) l->data, *val);
		}
		g_list_free (list);
	}

	/* save the files */
	ret = g_file_set_contents (fn_all, priv->csv_all->str, -1, &error);
	if (!ret) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto out;
	}
	ret = g_file_set_contents (fn_failures, priv->csv_fail->str, -1, &error);
	if (!ret) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	g_print ("Written to %s and %s\n", fn_failures, fn_all);
	retval = EXIT_SUCCESS;
out:
	if (priv != NULL) {
		g_hash_table_unref (priv->cmfbinary);
		g_hash_table_unref (priv->vendors);
		g_hash_table_unref (priv->vendors_no_serial);
		g_hash_table_unref (priv->vendors_broken);
		g_string_free (priv->csv_all, TRUE);
		g_string_free (priv->csv_fail, TRUE);
	}
	g_free (priv);
	return retval;
}
