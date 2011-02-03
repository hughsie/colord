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

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <locale.h>
#include <colord.h>

typedef struct {
	CdClient		*client;
	GOptionContext		*context;
	GPtrArray		*cmd_array;
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
 * cd_util_show_profile:
 **/
static void
cd_util_show_profile (CdProfile *profile)
{
	CdColorspace colorspace;
	CdProfileKind kind;
	const gchar *tmp;
	GHashTable *metadata;
	GList *list, *l;

	g_print ("Object Path:\t%s\n",
		 cd_profile_get_object_path (profile));
	tmp = cd_profile_get_qualifier (profile);
	if (tmp != NULL && tmp[0] != '\0')
		g_print ("Qualifier:\t\t%s\n", tmp);
	kind = cd_profile_get_kind (profile);
	if (kind != CD_PROFILE_KIND_UNKNOWN) {
		g_print ("Kind:\t\t%s\n",
			 cd_profile_kind_to_string (kind));
	}
	colorspace = cd_profile_get_colorspace (profile);
	if (colorspace != CD_COLORSPACE_UNKNOWN) {
		g_print ("Colorspace:\t%s\n",
			 cd_colorspace_to_string (colorspace));
	}
	g_print ("Has VCGT:\t%s\n",
		 cd_profile_get_has_vcgt (profile) ? "Yes" : "No");
	g_print ("Filename:\t%s\n",
		 cd_profile_get_filename (profile));
	g_print ("Profile ID:\t%s\n",
		 cd_profile_get_id (profile));

	/* list all the items of metadata */
	metadata = cd_profile_get_metadata (profile);
	list = g_hash_table_get_keys (metadata);
	for (l = list; l != NULL; l = l->next) {
		if (g_strcmp0 (l->data, "CMS") == 0)
			continue;
		g_print ("Metadata:\t%s=%s\n",
			 (const gchar *) l->data,
			 (const gchar *) g_hash_table_lookup (metadata,
							      l->data));
	}
	g_list_free (list);
	g_hash_table_unref (metadata);
}

/**
 * cd_util_show_device:
 **/
static void
cd_util_show_device (CdDevice *device)
{
	CdProfile *profile_tmp;
	GPtrArray *profiles;
	guint i;

	g_print ("Object Path: %s\n",
		 cd_device_get_object_path (device));
	g_print ("Created:\t%" G_GUINT64_FORMAT "\n",
		 cd_device_get_created (device));
	g_print ("Modified:\t%" G_GUINT64_FORMAT "\n",
		 cd_device_get_modified (device));
	g_print ("Kind:\t\t%s\n",
		 cd_device_kind_to_string (cd_device_get_kind (device)));
	g_print ("Model:\t\t%s\n",
		 cd_device_get_model (device));
	g_print ("Vendor:\t\t%s\n",
		 cd_device_get_vendor (device));
	g_print ("Serial:\t\t%s\n",
		 cd_device_get_serial (device));
	g_print ("Colorspace:\t%s\n",
		 cd_colorspace_to_string (cd_device_get_colorspace (device)));
	g_print ("Device ID:\t%s\n",
		 cd_device_get_id (device));

	/* print profiles */
	profiles = cd_device_get_profiles (device);
	for (i=0; i<profiles->len; i++) {
		profile_tmp = g_ptr_array_index (profiles, i);
		g_print ("Profile %i:\t%s\n",
			 i+1,
			 cd_profile_get_object_path (profile_tmp));
	}
}

/**
 * cd_util_mask_from_string:
 **/
static guint
cd_util_mask_from_string (const gchar *value)
{
	if (g_strcmp0 (value, "normal") == 0)
		return CD_OBJECT_SCOPE_NORMAL;
	if (g_strcmp0 (value, "temp") == 0)
		return CD_OBJECT_SCOPE_TEMPORARY;
	if (g_strcmp0 (value, "disk") == 0)
		return CD_OBJECT_SCOPE_DISK;
	g_warning ("mask string '%s' unknown", value);
	return CD_OBJECT_SCOPE_NORMAL;
}


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
	gchar **names;
	guint i;
	CdUtilItem *item;

	/* add each one */
	names = g_strsplit (name, ",", -1);
	for (i=0; names[i] != NULL; i++) {
		item = g_new0 (CdUtilItem, 1);
		item->name = g_strdup (names[i]);
		if (i == 0) {
			item->description = g_strdup (description);
		} else {
			/* TRANSLATORS: this is a command alias */
			item->description = g_strdup_printf ("Alias to %s",
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
	guint i;
	guint j;
	guint len;
	guint max_len = 0;
	CdUtilItem *item;
	GString *string;

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
 * cd_util_run:
 **/
static gboolean
cd_util_run (CdUtilPrivate *priv, const gchar *command, gchar **values, GError **error)
{
	gboolean ret = FALSE;
	guint i;
	CdUtilItem *item;
	GString *string;

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
 * cd_util_get_devices:
 **/
static gboolean
cd_util_get_devices (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device;
	gboolean ret = TRUE;
	GPtrArray *array = NULL;
	guint i;

	/* execute sync method */
	array = cd_client_get_devices_sync (priv->client, NULL, error);
	if (array == NULL) {
		ret = FALSE;
		goto out;
	}
	for (i=0; i < array->len; i++) {
		device = g_ptr_array_index (array, i);
		cd_util_show_device (device);
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	return ret;
}

/**
 * cd_util_get_devices_by_kind:
 **/
static gboolean
cd_util_get_devices_by_kind (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device;
	gboolean ret = TRUE;
	GPtrArray *array = NULL;
	guint i;

	if (g_strv_length (values) < 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device kind "
				     "e.g. 'printer'");
		goto out;
	}

	/* execute sync method */
	array = cd_client_get_devices_by_kind_sync (priv->client,
			cd_device_kind_from_string (values[0]),
			NULL,
			error);
	if (array == NULL) {
		ret = FALSE;
		goto out;
	}
	for (i=0; i < array->len; i++) {
		device = g_ptr_array_index (array, i);
		cd_util_show_device (device);
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	return ret;
}

/**
 * cd_util_get_profiles:
 **/
static gboolean
cd_util_get_profiles (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdProfile *profile;
	gboolean ret = TRUE;
	GPtrArray *array = NULL;
	guint i;

	/* execute sync method */
	array = cd_client_get_profiles_sync (priv->client, NULL, error);
	if (array == NULL) {
		ret = FALSE;
		goto out;
	}
	for (i=0; i < array->len; i++) {
		profile = g_ptr_array_index (array, i);
		cd_util_show_profile (profile);
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	return ret;
}

/**
 * cd_util_create_device:
 **/
static gboolean
cd_util_create_device (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;
	guint mask;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device id, scope "
				     "e.g. 'epson-stylus-800 disk'");
		goto out;
	}

	/* execute sync method */
	mask = cd_util_mask_from_string (values[1]);
	device = cd_client_create_device_sync (priv->client, values[0],
					       mask, NULL, NULL, error);
	if (device == NULL) {
		ret = FALSE;
		goto out;
	}
	g_print ("Created device:\n");
	cd_util_show_device (device);
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_find_device:
 **/
static gboolean
cd_util_find_device (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device id "
				     "e.g. 'epson-stylus-800'");
		goto out;
	}

	/* execute sync method */
	device = cd_client_find_device_sync (priv->client, values[0],
					     NULL, error);
	if (device == NULL) {
		ret = FALSE;
		goto out;
	}
	cd_util_show_device (device);
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_find_profile:
 **/
static gboolean
cd_util_find_profile (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected profile id "
				     "e.g. 'epson-rgb'");
		goto out;
	}

	/* execute sync method */
	profile = cd_client_find_profile_sync (priv->client, values[0],
					       NULL, error);
	if (profile == NULL) {
		ret = FALSE;
		goto out;
	}
	cd_util_show_profile (profile);
out:
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_create_profile:
 **/
static gboolean
cd_util_create_profile (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdProfile *profile = NULL;
	gboolean ret = TRUE;
	guint mask;

	if (g_strv_length (values) < 3) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected profile id, scope "
				     "e.g. 'epson-rgb disk'");
		goto out;
	}

	/* execute sync method */
	mask = cd_util_mask_from_string (values[1]);
	profile = cd_client_create_profile_sync (priv->client, values[0],
						 mask, NULL, NULL, error);
	if (profile == NULL) {
		ret = FALSE;
		goto out;
	}
	g_print ("Created profile:\n");
	cd_util_show_profile (profile);
out:
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_device_add_profile:
 **/
static gboolean
cd_util_device_add_profile (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, profile path "
				     "e.g. '/org/device/foo /org/profile/bar'");
		goto out;
	}

	device = cd_device_new ();
	ret = cd_device_set_object_path_sync (device,
					      values[0],
					      NULL,
					      error);
	if (!ret)
		goto out;
	profile = cd_profile_new ();
	ret = cd_profile_set_object_path_sync (profile,
					       values[1],
					       NULL,
					       error);
	if (!ret)
		goto out;
	ret = cd_device_add_profile_sync (device, profile, NULL, error);
	if (!ret)
		goto out;
out:
	if (device != NULL)
		g_object_unref (device);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_device_make_profile_default:
 **/
static gboolean
cd_util_device_make_profile_default (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, profile path "
				     "e.g. '/org/device/foo /org/profile/bar'");
		goto out;
	}

	device = cd_device_new ();
	ret = cd_device_set_object_path_sync (device,
					      values[0],
					      NULL,
					      error);
	if (!ret)
		goto out;
	profile = cd_profile_new ();
	ret = cd_profile_set_object_path_sync (profile,
					       values[1],
					       NULL,
					       error);
	if (!ret)
		goto out;
	ret = cd_device_make_profile_default_sync (device, profile,
						   NULL, error);
	if (!ret)
		goto out;
out:
	if (device != NULL)
		g_object_unref (device);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_delete_device:
 **/
static gboolean
cd_util_delete_device (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;

	if (g_strv_length (values) < 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device id "
				     "e.g. 'epson-stylus-800'");
		goto out;
	}

	ret = cd_client_delete_device_sync (priv->client, values[0],
					    NULL, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * cd_util_delete_profile:
 **/
static gboolean
cd_util_delete_profile (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;

	if (g_strv_length (values) < 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected profile id "
				     "e.g. 'epson-rgb'");
		goto out;
	}

	ret = cd_client_delete_profile_sync (priv->client, values[0],
					     NULL, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * cd_util_profile_set_qualifier:
 **/
static gboolean
cd_util_profile_set_qualifier (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected profile path, qualifier "
				     "e.g. '/org/profile/foo epson.rgb.300dpi'");
		goto out;
	}

	profile = cd_profile_new ();
	ret = cd_profile_set_object_path_sync (profile,
					       values[0],
					       NULL,
					       error);
	if (!ret)
		goto out;
	ret = cd_profile_set_qualifier_sync (profile, values[1],
					     NULL, error);
	if (!ret)
		goto out;
out:
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_profile_set_filename:
 **/
static gboolean
cd_util_profile_set_filename (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected profile path, filename "
				     "e.g. '/org/profile/foo bar.icc'");
		goto out;
	}

	profile = cd_profile_new ();
	ret = cd_profile_set_object_path_sync (profile,
					       values[0],
					       NULL,
					       error);
	if (!ret)
		goto out;
	ret = cd_profile_set_filename_sync (profile, values[1],
					    NULL, error);
	if (!ret)
		goto out;
out:
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_device_set_model:
 **/
static gboolean
cd_util_device_set_model (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, model "
				     "e.g. '/org/devices/bar \"Stylus 800\"'");
		goto out;
	}

	device = cd_device_new ();
	ret = cd_device_set_object_path_sync (device,
					      values[0],
					      NULL,
					      error);
	if (!ret)
		goto out;
	ret = cd_device_set_model_sync (device, values[1],
					NULL, error);
	if (!ret)
		goto out;
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_device_get_default_profile:
 **/
static gboolean
cd_util_device_get_default_profile (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path "
				     "e.g. '/org/devices/bar'");
		goto out;
	}

	device = cd_device_new ();
	ret = cd_device_set_object_path_sync (device,
					      values[0],
					      NULL,
					      error);
	if (!ret)
		goto out;
	profile = cd_device_get_default_profile (device);
	if (profile == NULL) {
		ret = FALSE;
		goto out;
	}
	cd_util_show_profile (profile);
out:
	if (device != NULL)
		g_object_unref (device);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_device_set_vendor:
 **/
static gboolean
cd_util_device_set_vendor (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, vendor "
				     "e.g. '/org/devices/bar Epson'");
		goto out;
	}

	device = cd_device_new ();
	ret = cd_device_set_object_path_sync (device,
					      values[0],
					      NULL,
					      error);
	if (!ret)
		goto out;
	ret = cd_device_set_vendor_sync (device, values[1],
					 NULL, error);
	if (!ret)
		goto out;
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_device_set_serial:
 **/
static gboolean
cd_util_device_set_serial (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, serial "
				     "e.g. '/org/devices/bar 00001234'");
		goto out;
	}

	device = cd_device_new ();
	ret = cd_device_set_object_path_sync (device,
					      values[0],
					      NULL,
					      error);
	if (!ret)
		goto out;
	ret = cd_device_set_serial_sync (device, values[1],
					 NULL, error);
	if (!ret)
		goto out;
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_device_set_kind:
 **/
static gboolean
cd_util_device_set_kind (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, kind "
				     "e.g. '/org/devices/bar printer'");
		goto out;
	}

	device = cd_device_new ();
	ret = cd_device_set_object_path_sync (device,
					      values[0],
					      NULL,
					      error);
	if (!ret)
		goto out;
	ret = cd_device_set_kind_sync (device, cd_device_kind_from_string (values[1]),
				       NULL, error);
	if (!ret)
		goto out;
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_device_get_profile_for_qualifier:
 **/
static gboolean
cd_util_device_get_profile_for_qualifier (CdUtilPrivate *priv,
					  gchar **values,
					  GError **error)
{
	CdDevice *device = NULL;
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, qualifier "
				     "e.g. '/org/devices/bar *.*.300dpi'");
		goto out;
	}

	device = cd_device_new ();
	ret = cd_device_set_object_path_sync (device,
					      values[0],
					      NULL,
					      error);
	if (!ret)
		goto out;
	profile = cd_device_get_profile_for_qualifier_sync (device,
							    values[1],
							    NULL,
							    error);
	if (profile == NULL) {
		ret = FALSE;
		goto out;
	}
	cd_util_show_profile (profile);
out:
	if (device != NULL)
		g_object_unref (device);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	gboolean ret;
	GError *error = NULL;
	guint retval = 1;
	CdUtilPrivate *priv;
	gchar *cmd_descriptions = NULL;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();

	/* create helper object */
	priv = g_new0 (CdUtilPrivate, 1);

	/* add commands */
	priv->cmd_array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_util_item_free);
	cd_util_add (priv->cmd_array,
		     "get-devices",
		     /* TRANSLATORS: command description */
		     _("Gets all the color managed devices"),
		     cd_util_get_devices);
	cd_util_add (priv->cmd_array,
		     "get-devices-by-kind",
		     /* TRANSLATORS: command description */
		     _("Gets all the color managed devices of a specific kind"),
		     cd_util_get_devices_by_kind);
	cd_util_add (priv->cmd_array,
		     "get-profiles",
		     /* TRANSLATORS: command description */
		     _("Gets all the available color profiles"),
		     cd_util_get_profiles);
	cd_util_add (priv->cmd_array,
		     "create-device",
		     /* TRANSLATORS: command description */
		     _("Create a device"),
		     cd_util_create_device);
	cd_util_add (priv->cmd_array,
		     "find-device",
		     /* TRANSLATORS: command description */
		     _("Find a device"),
		     cd_util_find_device);
	cd_util_add (priv->cmd_array,
		     "find-profile",
		     /* TRANSLATORS: command description */
		     _("Find a profile"),
		     cd_util_find_profile);
	cd_util_add (priv->cmd_array,
		     "create-profile",
		     /* TRANSLATORS: command description */
		     _("Create a profile"),
		     cd_util_create_profile);
	cd_util_add (priv->cmd_array,
		     "device-add-profile",
		     /* TRANSLATORS: command description */
		     _("Add a profile to a device"),
		     cd_util_device_add_profile);
	cd_util_add (priv->cmd_array,
		     "device-make-profile-default",
		     /* TRANSLATORS: command description */
		     _("Makes a profile default for a device"),
		     cd_util_device_make_profile_default);
	cd_util_add (priv->cmd_array,
		     "delete-device",
		     /* TRANSLATORS: command description */
		     _("Deletes a device"),
		     cd_util_delete_device);
	cd_util_add (priv->cmd_array,
		     "delete-profile",
		     /* TRANSLATORS: command description */
		     _("Deletes a profile"),
		     cd_util_delete_profile);
	cd_util_add (priv->cmd_array,
		     "profile-set-qualifier",
		     /* TRANSLATORS: command description */
		     _("Sets the profile qualifier"),
		     cd_util_profile_set_qualifier);
	cd_util_add (priv->cmd_array,
		     "profile-set-filename",
		     /* TRANSLATORS: command description */
		     _("Sets the profile filename"),
		     cd_util_profile_set_filename);
	cd_util_add (priv->cmd_array,
		     "device-set-model",
		     /* TRANSLATORS: command description */
		     _("Sets the device model"),
		     cd_util_device_set_model);
	cd_util_add (priv->cmd_array,
		     "device-get-default-profile",
		     /* TRANSLATORS: command description */
		     _("Gets the default profile for a device"),
		     cd_util_device_get_default_profile);
	cd_util_add (priv->cmd_array,
		     "device-set-vendor",
		     /* TRANSLATORS: command description */
		     _("Sets the device vendor"),
		     cd_util_device_set_vendor);
	cd_util_add (priv->cmd_array,
		     "device-set-serial",
		     /* TRANSLATORS: command description */
		     _("Sets the device serial"),
		     cd_util_device_set_serial);
	cd_util_add (priv->cmd_array,
		     "device-set-kind",
		     /* TRANSLATORS: command description */
		     _("Sets the device kind"),
		     cd_util_device_set_kind);
	cd_util_add (priv->cmd_array,
		     "device-get-profile-for-qualifier",
		     /* TRANSLATORS: command description */
		     _("Returns all the profiles that match a qualifier"),
		     cd_util_device_get_profile_for_qualifier);

	/* sort by command name */
	g_ptr_array_sort (priv->cmd_array,
			  (GCompareFunc) cd_sort_command_name_cb);

	/* get a list of the commands */
	priv->context = g_option_context_new (NULL);
	cmd_descriptions = cd_util_get_descriptions (priv->cmd_array);
	g_option_context_set_summary (priv->context, cmd_descriptions);

	/* TRANSLATORS: program name */
	g_set_application_name (_("Color Management"));
	g_option_context_parse (priv->context, &argc, &argv, NULL);

	/* get connection to colord */
	priv->client = cd_client_new ();
	ret = cd_client_connect_sync (priv->client, NULL, &error);
	if (!ret) {
		/* TRANSLATORS: no colord available */
		g_print ("%s %s\n", _("No connection to colord:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* run the specified command */
	ret = cd_util_run (priv, argv[1], (gchar**) &argv[2], &error);
	if (!ret) {
		g_print ("%s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	retval = 0;
out:
	if (priv != NULL) {
		g_object_unref (priv->client);
		if (priv->cmd_array != NULL)
			g_ptr_array_unref (priv->cmd_array);
		g_option_context_free (priv->context);
		g_free (priv);
	}
	g_free (cmd_descriptions);
	return retval;
}

