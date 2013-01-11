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

#include "cd-client-sync.h"
#include "cd-common.h"
#include "cd-enum.h"
#include "cd-lcms-helpers.h"
#include "cd-profile-sync.h"

typedef struct {
	GOptionContext		*context;
	GPtrArray		*cmd_array;
	CdClient		*client;
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
 * cd_util_profile_read:
 **/
static cmsHPROFILE
cd_util_profile_read (const gchar *filename, GError **error)
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
 * cd_util_profile_write:
 **/
static gboolean
cd_util_profile_write (cmsHPROFILE lcms_profile, const gchar *filename, GError **error)
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
 * cd_util_profile_set_text_acsii:
 **/
static gboolean
cd_util_profile_set_text_acsii (cmsHPROFILE lcms_profile,
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
 * cd_util_set_info_text:
 **/
static gboolean
cd_util_set_info_text (CdUtilPrivate *priv,
		       cmsTagSignature sig,
		       gchar **values,
		       GError **error)
{
	cmsHPROFILE lcms_profile = NULL;
	const gchar *value;
	gboolean ret = TRUE;

	/* check arguments */
	if (g_strv_length (values) != 2) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'value' 'filename'");
		goto out;
	}

	/* open profile */
	lcms_profile = cd_util_profile_read (values[1], error);
	if (lcms_profile == NULL) {
		ret = FALSE;
		goto out;
	}

	/* these are default values */
	if (sig == cmsSigCopyrightTag &&
	    g_strcmp0 (values[0], "") == 0) {
		value = CD_PROFILE_DEFAULT_COPYRIGHT_STRING;
	} else {
		value = values[0];
	}

	/* update value */
	ret = cd_util_profile_set_text_acsii (lcms_profile,
					      sig,
					      value,
					      error);
	if (!ret)
		goto out;

	/* write new file */
	ret = cd_util_profile_write (lcms_profile, values[1], error);
	if (!ret)
		goto out;
out:
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	return ret;
}

/**
 * cd_util_set_copyright:
 **/
static gboolean
cd_util_set_copyright (CdUtilPrivate *priv, gchar **values, GError **error)
{
	return cd_util_set_info_text (priv, cmsSigCopyrightTag, values, error);
}

/**
 * cd_util_set_description:
 **/
static gboolean
cd_util_set_description (CdUtilPrivate *priv, gchar **values, GError **error)
{
	return cd_util_set_info_text (priv, cmsInfoDescription, values, error);
}

/**
 * cd_util_set_manufacturer:
 **/
static gboolean
cd_util_set_manufacturer (CdUtilPrivate *priv, gchar **values, GError **error)
{
	return cd_util_set_info_text (priv, cmsInfoManufacturer, values, error);
}

/**
 * cd_util_set_model:
 **/
static gboolean
cd_util_set_model (CdUtilPrivate *priv, gchar **values, GError **error)
{
	return cd_util_set_info_text (priv, cmsInfoModel, values, error);
}

/**
 * cd_util_clear_metadata:
 **/
static gboolean
cd_util_clear_metadata (CdUtilPrivate *priv, gchar **values, GError **error)
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
	lcms_profile = cd_util_profile_read (values[0], error);
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
	ret = cd_util_profile_write (lcms_profile, values[0], error);
	if (!ret)
		goto out;
out:
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	return ret;
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

typedef struct {
	guint			 idx;
	cmsFloat32Number	*data;
} CdUtilGamutCdeckHelper;

/**
 * cd_util_get_coverage_sample_cb:
 **/
static cmsInt32Number
cd_util_get_coverage_sample_cb (const cmsFloat32Number in[],
				cmsFloat32Number out[],
				void *user_data)
{
	CdUtilGamutCdeckHelper *helper = (CdUtilGamutCdeckHelper *) user_data;
	helper->data[helper->idx++] = in[0];
	helper->data[helper->idx++] = in[1];
	helper->data[helper->idx++] = in[2];
	return 1;
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
	const guint cube_size = 33;
	cmsFloat32Number *data = NULL;
	cmsHPROFILE profile_in = NULL;
	cmsHPROFILE profile_null = NULL;
	cmsHTRANSFORM transform = NULL;
	cmsUInt16Number *alarm_codes = NULL;
	cmsUInt32Number dimensions[] = { cube_size, cube_size, cube_size };
	CdUtilGamutCdeckHelper helper;
	gboolean ret;
	gdouble coverage = -1.0;
	guint cnt = 0;
	guint data_len = cube_size * cube_size * cube_size;
	guint i;

	/* load profiles into lcms */
	profile_null = cmsCreateNULLProfile ();
	profile_in = cmsOpenProfileFromFile (filename_in, "r");
	if (profile_in == NULL) {
		g_set_error (error, 1, 0, "Failed to open %s", filename_in);
		goto out;
	}

	/* create a proofing transform with gamut check */
	transform = cmsCreateProofingTransform (profile_in,
						TYPE_RGB_FLT,
						profile_null,
						TYPE_GRAY_FLT,
						profile_proof,
						INTENT_ABSOLUTE_COLORIMETRIC,
						INTENT_ABSOLUTE_COLORIMETRIC,
						cmsFLAGS_GAMUTCHECK |
						cmsFLAGS_SOFTPROOFING);
	if (transform == NULL) {
		g_set_error (error, 1, 0,
			     "Failed to setup transform for %s",
			     filename_in);
		goto out;
	}

	/* set gamut alarm to 0xffff */
	alarm_codes = g_new0 (cmsUInt16Number, cmsMAXCHANNELS);
	alarm_codes[0] = 0xffff;
	cmsSetAlarmCodes (alarm_codes);

	/* slice profile in regular intevals */
	data = g_new0 (cmsFloat32Number, data_len * 3);
	helper.data = data;
	helper.idx = 0;
	ret = cmsSliceSpaceFloat (3, dimensions,
				  cd_util_get_coverage_sample_cb,
				  &helper);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "Failed to slice data");
		goto out;
	}

	/* transform each one of those nodes across the proofing transform */
	cmsDoTransform (transform, helper.data, helper.data, data_len);

	/* count the nodes that gives you zero and divide by total number */
	for (i = 0; i < data_len; i++) {
		if (helper.data[i] == 0.0)
			cnt++;
	}

	/* success */
	coverage = (gdouble) cnt / (gdouble) data_len;
	g_assert (coverage >= 0.0);
out:
	g_free (data);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	if (profile_null != NULL)
		cmsCloseProfile (profile_null);
	if (profile_in != NULL)
		cmsCloseProfile (profile_in);
	return coverage;
}

/**
 * cd_util_get_profile_coverage:
 **/
static gdouble
cd_util_get_profile_coverage (CdUtilPrivate *priv,
			      cmsHPROFILE profile,
			      CdStandardSpace standard_space,
			      GError **error)
{
	gchar *filename = NULL;
	gdouble coverage = -1.0f;

	/* get the correct standard space */
	filename = cd_util_get_standard_space_filename (priv,
							standard_space,
							error);
	if (filename == NULL)
		goto out;

	/* work out the coverage */
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
	cmsHPROFILE lcms_profile = NULL;
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

	/* open profile and set version */
	lcms_profile = cd_util_profile_read (values[0], error);
	if (lcms_profile == NULL) {
		ret = FALSE;
		goto out;
	}
	cmsSetProfileVersion (lcms_profile, version);

	/* write new file */
	ret = cd_util_profile_write (lcms_profile, values[0], error);
	if (!ret)
		goto out;
out:
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	return ret;
}

/**
 * cd_util_set_fix_metadata:
 **/
static gboolean
cd_util_set_fix_metadata (CdUtilPrivate *priv, gchar **values, GError **error)
{
	cmsHANDLE dict_new = NULL;
	cmsHANDLE dict_old = NULL;
	cmsHPROFILE lcms_profile = NULL;
	const cmsDICTentry *entry;
	gboolean ret = TRUE;
	gchar name[1024];
	gdouble coverage;
	gchar *coverage_tmp;

	/* check arguments */
	if (g_strv_length (values) != 1) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "invalid input, expect 'filename'");
		goto out;
	}

	/* open profile */
	lcms_profile = cd_util_profile_read (values[0], error);
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

	/* copy, but ignore the gamut keys */
	dict_new = cmsDictAlloc (NULL);
	for (entry = cmsDictGetEntryList (dict_old);
	     entry != NULL;
	     entry = cmsDictNextEntry (entry)) {
		wcstombs (name, entry->Name, sizeof (name));
		if (g_str_has_prefix (name, "GAMUT_volume") == 0)
			continue;
		cmsDictAddEntry (dict_new,
				 entry->Name,
				 entry->Value,
				 NULL,
				 NULL);
	}

	/* get coverages of common spaces */
	if (cmsGetColorSpace (lcms_profile) == cmsSigRgbData) {

		/* get the gamut coverage for sRGB */
		coverage = cd_util_get_profile_coverage (priv,
							  lcms_profile,
							  CD_STANDARD_SPACE_ADOBE_RGB,
							  error);
		if (coverage < 0.0) {
			ret = FALSE;
			goto out;
		}
		coverage_tmp = g_strdup_printf ("%f", coverage);
		_cmsDictAddEntryAscii (dict_new,
				       "GAMUT_coverage(adobe-rgb)",
				       coverage_tmp);
		g_free (coverage_tmp);
		g_debug ("coverage of AdobeRGB: %f%%", coverage * 100.0f);

		/* get the gamut coverage for AdobeRGB */
		coverage = cd_util_get_profile_coverage (priv,
							 lcms_profile,
							 CD_STANDARD_SPACE_SRGB,
							 error);
		if (coverage < 0.0) {
			ret = FALSE;
			goto out;
		}
		coverage_tmp = g_strdup_printf ("%f", coverage);
		_cmsDictAddEntryAscii (dict_new,
				       "GAMUT_coverage(srgb)",
				       coverage_tmp);
		g_free (coverage_tmp);
		g_debug ("coverage of sRGB: %f%%", coverage * 100.0f);
	}

	/* add CMS defines */
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
	ret = cd_util_profile_write (lcms_profile, values[0], error);
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
 * cd_util_init_metadata:
 **/
static gboolean
cd_util_init_metadata (CdUtilPrivate *priv, gchar **values, GError **error)
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
	lcms_profile = cd_util_profile_read (values[0], error);
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
	ret = cd_util_profile_write (lcms_profile, values[0], error);
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
 * cd_util_remove_metadata:
 **/
static gboolean
cd_util_remove_metadata (CdUtilPrivate *priv, gchar **values, GError **error)
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
	lcms_profile = cd_util_profile_read (values[1], error);
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
	ret = cd_util_profile_write (lcms_profile, values[1], error);
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
 * cd_util_add_metadata:
 **/
static gboolean
cd_util_add_metadata (CdUtilPrivate *priv, gchar **values, GError **error)
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
	lcms_profile = cd_util_profile_read (values[2], error);
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
	ret = cd_util_profile_write (lcms_profile, values[2], error);
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
 * cd_util_dump:
 **/
static gboolean
cd_util_dump (CdUtilPrivate *priv, gchar **values, GError **error)
{
	cmsCIExyY yxy;
	cmsHANDLE dict;
	cmsHPROFILE lcms_profile = NULL;
	const cmsCIEXYZ *xyz;
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
		g_set_error (error, 1, 0,
			     "failed to open profile %s",
			     values[0]);
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

	/* show Yxy primaries */
	xyz = cmsReadTag (lcms_profile, cmsSigRedColorantTag);
	if (xyz != NULL) {
		cmsXYZ2xyY (&yxy, xyz);
		g_print ("%s:\t%0.3f, %0.3f\n", _("Red primary"), yxy.x, yxy.y);
	}
	xyz = cmsReadTag (lcms_profile, cmsSigGreenColorantTag);
	if (xyz != NULL) {
		cmsXYZ2xyY (&yxy, xyz);
		g_print ("%s:\t%0.3f, %0.3f\n", _("Green primary"), yxy.x, yxy.y);
	}
	xyz = cmsReadTag (lcms_profile, cmsSigBlueColorantTag);
	if (xyz != NULL) {
		cmsXYZ2xyY (&yxy, xyz);
		g_print ("%s:\t%0.3f, %0.3f\n", _("Blue primary"), yxy.x, yxy.y);
	}
	xyz = cmsReadTag (lcms_profile, cmsSigMediaWhitePointTag);
	if (xyz != NULL) {
		cmsXYZ2xyY (&yxy, xyz);
		g_print ("%s:\t%0.3f, %0.3f\n", _("Whitepoint"), yxy.x, yxy.y);
	}

	/* success */
	ret = TRUE;
out:
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	return ret;
}

/**
 * cd_util_generate_vcgt:
 **/
static gboolean
cd_util_generate_vcgt (CdUtilPrivate *priv, gchar **values, GError **error)
{
	cmsHPROFILE lcms_profile = NULL;
	gboolean ret = TRUE;
	const cmsToneCurve **vcgt;
	cmsFloat32Number in;
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

	/* set value */
	lcms_profile = cmsOpenProfileFromFile (values[0], "r");
	if (lcms_profile == NULL) {
		g_set_error (error, 1, 0,
			     "failed to open profile %s",
			     values[0]);
		ret = FALSE;
		goto out;
	}

	/* does profile have VCGT */
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
	ret = TRUE;
out:
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
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
	cmsSetLogErrorHandler (cd_util_lcms_error_cb);

	g_type_init ();

	/* create helper object */
	priv = g_new0 (CdUtilPrivate, 1);
	priv->client = cd_client_new ();

	/* add commands */
	priv->cmd_array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_util_item_free);
	cd_util_add (priv->cmd_array,
		     "dump",
		     /* TRANSLATORS: command description */
		     _("Show all the details about the profile"),
		     cd_util_dump);
	cd_util_add (priv->cmd_array,
		     "extract-vcgt",
		     /* TRANSLATORS: command description */
		     _("Generate the VCGT calibration of a given size"),
		     cd_util_generate_vcgt);
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

	/* set verbose? */
	if (verbose) {
		g_setenv ("COLORD_VERBOSE", "1", FALSE);
	} else {
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
				   cd_util_ignore_cb, NULL);
	}

	/* get connection to colord */

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
		if (priv->cmd_array != NULL)
			g_ptr_array_unref (priv->cmd_array);
		g_option_context_free (priv->context);
		g_object_unref (priv->client);
		g_free (priv);
	}
	g_free (cmd_descriptions);
	return retval;
}

