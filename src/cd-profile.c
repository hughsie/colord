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

#include "config.h"

#include <gio/gio.h>
#include <glib-object.h>
#include <lcms2.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <math.h>

#include "cd-common.h"
#include "cd-profile.h"

static void     cd_profile_finalize	(GObject     *object);

#define CD_PROFILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_PROFILE, CdProfilePrivate))

/**
 * CdProfilePrivate:
 *
 * Private #CdProfile data
 **/
struct _CdProfilePrivate
{
	CdObjectScope			 object_scope;
	gchar				*filename;
	gchar				*id;
	gchar				*object_path;
	gchar				*qualifier;
	gchar				*format;
	gchar				*checksum;
	gchar				*title;
	GDBusConnection			*connection;
	guint				 registration_id;
	guint				 watcher_id;
	CdProfileKind			 kind;
	CdColorspace			 colorspace;
	GHashTable			*metadata;
	gboolean			 has_vcgt;
	gboolean			 is_system_wide;
	gint64				 created;
	guint				 owner;
	gchar				**warnings;
	GMappedFile			*mapped_file;
};

enum {
	SIGNAL_INVALIDATE,
	SIGNAL_LAST
};

enum {
	PROP_0,
	PROP_OBJECT_PATH,
	PROP_ID,
	PROP_QUALIFIER,
	PROP_TITLE,
	PROP_FILENAME,
	PROP_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };
G_DEFINE_TYPE (CdProfile, cd_profile, G_TYPE_OBJECT)

/**
 * cd_profile_error_quark:
 **/
GQuark
cd_profile_error_quark (void)
{
	guint i;
	static GQuark quark = 0;
	if (!quark) {
		quark = g_quark_from_static_string ("CdProfile");
		for (i = 0; i < CD_PROFILE_ERROR_LAST; i++) {
			g_dbus_error_register_error (quark,
						     i,
						     cd_profile_error_to_string (i));
		}
	}
	return quark;
}

/**
 * cd_profile_get_scope:
 **/
CdObjectScope
cd_profile_get_scope (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), 0);
	return profile->priv->object_scope;
}

/**
 * cd_profile_set_scope:
 **/
void
cd_profile_set_scope (CdProfile *profile, CdObjectScope object_scope)
{
	g_return_if_fail (CD_IS_PROFILE (profile));
	profile->priv->object_scope = object_scope;
}

/**
 * cd_profile_get_owner:
 **/
guint
cd_profile_get_owner (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), G_MAXUINT);
	return profile->priv->owner;
}

/**
 * cd_profile_set_owner:
 **/
void
cd_profile_set_owner (CdProfile *profile, guint owner)
{
	g_return_if_fail (CD_IS_PROFILE (profile));
	profile->priv->owner = owner;
}

/**
 * cd_profile_set_is_system_wide:
 **/
void
cd_profile_set_is_system_wide (CdProfile *profile, gboolean is_system_wide)
{
	g_return_if_fail (CD_IS_PROFILE (profile));
	profile->priv->is_system_wide = is_system_wide;
}

/**
 * cd_profile_get_is_system_wide:
 **/
gboolean
cd_profile_get_is_system_wide (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	return profile->priv->is_system_wide;
}

/**
 * cd_profile_get_object_path:
 **/
const gchar *
cd_profile_get_object_path (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return profile->priv->object_path;
}

/**
 * cd_profile_get_id:
 **/
const gchar *
cd_profile_get_id (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return profile->priv->id;
}

/**
 * cd_profile_set_object_path:
 **/
static void
cd_profile_set_object_path (CdProfile *profile)
{
	gchar *path_tmp;
	gchar *path_owner;
	struct passwd *pw;


	/* append the uid to the object path */
	pw = getpwuid (profile->priv->owner);
	if (profile->priv->owner == 0 ||
	    g_strcmp0 (pw->pw_name, DAEMON_USER) == 0) {
		path_tmp = g_strdup (profile->priv->id);
	} else {
		path_tmp = g_strdup_printf ("%s_%s_%d",
					    profile->priv->id,
					    pw->pw_name,
					    profile->priv->owner);
	}
	/* make sure object path is sane */
	path_owner = cd_main_ensure_dbus_path (path_tmp);

	profile->priv->object_path = g_build_filename (COLORD_DBUS_PATH,
						       "profiles",
						       path_owner,
						       NULL);
	g_free (path_owner);
	g_free (path_tmp);
}

/**
 * cd_profile_set_id:
 **/
void
cd_profile_set_id (CdProfile *profile, const gchar *id)
{
	g_return_if_fail (CD_IS_PROFILE (profile));

	g_free (profile->priv->id);
	profile->priv->id = g_strdup (id);

	/* now calculate this again */
	cd_profile_set_object_path (profile);
}

/**
 * cd_profile_get_filename:
 **/
const gchar *
cd_profile_get_filename (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return profile->priv->filename;
}

/**
 * cd_profile_dbus_emit_property_changed:
 **/
static void
cd_profile_dbus_emit_property_changed (CdProfile *profile,
				       const gchar *property_name,
				       GVariant *property_value)
{
	GVariantBuilder builder;
	GVariantBuilder invalidated_builder;

	/* not yet connected */
	if (profile->priv->connection == NULL)
		return;

	/* build the dict */
	g_variant_builder_init (&invalidated_builder, G_VARIANT_TYPE ("as"));
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add (&builder,
			       "{sv}",
			       property_name,
			       property_value);
	g_dbus_connection_emit_signal (profile->priv->connection,
				       NULL,
				       profile->priv->object_path,
				       "org.freedesktop.DBus.Properties",
				       "PropertiesChanged",
				       g_variant_new ("(sa{sv}as)",
				       COLORD_DBUS_INTERFACE_PROFILE,
				       &builder,
				       &invalidated_builder),
				       NULL);
}

/**
 * cd_profile_dbus_emit_profile_changed:
 **/
static void
cd_profile_dbus_emit_profile_changed (CdProfile *profile)
{
	/* not yet connected */
	if (profile->priv->connection == NULL)
		return;

	/* emit signal */
	g_debug ("CdProfile: emit Changed on %s",
		 cd_profile_get_object_path (profile));
	g_dbus_connection_emit_signal (profile->priv->connection,
				       NULL,
				       cd_profile_get_object_path (profile),
				       COLORD_DBUS_INTERFACE_PROFILE,
				       "Changed",
				       NULL,
				       NULL);

	/* emit signal */
	g_debug ("CdProfile: emit Changed");
	g_dbus_connection_emit_signal (profile->priv->connection,
				       NULL,
				       COLORD_DBUS_PATH,
				       COLORD_DBUS_INTERFACE,
				       "ProfileChanged",
				       g_variant_new ("(o)",
						      cd_profile_get_object_path (profile)),
				       NULL);
}

/**
 * cd_profile_install_system_wide:
 **/
static gboolean
cd_profile_install_system_wide (CdProfile *profile, GError **error)
{
	gboolean ret = TRUE;
	gchar *basename = NULL;
	gchar *filename = NULL;
	GError *error_local = NULL;
	GFile *file_dest = NULL;
	GFile *file = NULL;
	CdProfilePrivate *priv = profile->priv;

	/* is icc filename set? */
	if (priv->filename == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_INTERNAL,
			     "icc filename not set");
		goto out;
	}

	/* is profile already installed in /var/lib/color */
	if (g_str_has_prefix (priv->filename,
			      CD_SYSTEM_PROFILES_DIR)) {
		ret = FALSE;
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_ALREADY_INSTALLED,
			     "file %s already installed in /var",
			     priv->filename);
		goto out;
	}

	/* is profile already installed in /usr/share/color */
	if (g_str_has_prefix (priv->filename,
			      DATADIR "/color")) {
		ret = FALSE;
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_ALREADY_INSTALLED,
			     "file %s already installed in /usr",
			     priv->filename);
		goto out;
	}

	/* copy */
	basename = g_path_get_basename (priv->filename);
	filename = g_build_filename (CD_SYSTEM_PROFILES_DIR,
				     basename, NULL);
	file_dest = g_file_new_for_path (filename);

	/* try to write a mapped file first, else copy the file */
	if (priv->mapped_file != NULL) {
		g_debug ("writing mapped file to %s", filename);
		ret = g_file_replace_contents (file_dest,
					       g_mapped_file_get_contents (priv->mapped_file),
					       g_mapped_file_get_length (priv->mapped_file),
					       NULL,
					       FALSE,
					       G_FILE_CREATE_REPLACE_DESTINATION,
					       NULL,
					       NULL, /* cancellable */
					       &error_local);
		if (!ret) {
			ret = FALSE;
			g_set_error (error,
				     CD_PROFILE_ERROR,
				     CD_PROFILE_ERROR_FAILED_TO_WRITE,
				     "failed to write mapped file %s: %s",
				     priv->filename,
				     error_local->message);
			g_error_free (error_local);
			goto out;
		}
	} else {
		file = g_file_new_for_path (priv->filename);
		ret = g_file_copy (file, file_dest, G_FILE_COPY_OVERWRITE,
				   NULL, NULL, NULL, &error_local);
		if (!ret) {
			ret = FALSE;
			g_set_error (error,
				     CD_PROFILE_ERROR,
				     CD_PROFILE_ERROR_FAILED_TO_WRITE,
				     "failed to copy %s: %s",
				     priv->filename,
				     error_local->message);
			g_error_free (error_local);
			goto out;
		}
	}
out:
	g_free (filename);
	g_free (basename);
	if (file != NULL)
		g_object_unref (file);
	if (file_dest != NULL)
		g_object_unref (file_dest);
	return ret;
}

/**
 * cd_profile_get_metadata_as_variant:
 **/
static GVariant *
cd_profile_get_metadata_as_variant (CdProfile *profile)
{
	GList *list, *l;
	GVariantBuilder builder;
	GVariant *value;

	/* do not try to build an empty array */
	if (g_hash_table_size (profile->priv->metadata) == 0) {
		value = g_variant_new_array (G_VARIANT_TYPE ("{ss}"),
					     NULL, 0);
		goto out;
	}

	/* add all the keys in the dictionary to the variant builder */
	list = g_hash_table_get_keys (profile->priv->metadata);
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	for (l = list; l != NULL; l = l->next) {
		g_variant_builder_add (&builder,
				       "{ss}",
				       l->data,
				       g_hash_table_lookup (profile->priv->metadata,
							    l->data));
	}
	g_list_free (list);
	value = g_variant_builder_end (&builder);
out:
	return value;
}

/**
 * cd_profile_get_nullable_for_string:
 **/
static GVariant *
cd_profile_get_nullable_for_string (const gchar *value)
{
	if (value == NULL)
		return g_variant_new_string ("");
	return g_variant_new_string (value);
}

/**
 * cd_profile_set_property_internal:
 **/
gboolean
cd_profile_set_property_internal (CdProfile *profile,
				  const gchar *property,
				  const gchar *value,
				  GError **error)
{
	gboolean ret = TRUE;
	CdProfilePrivate *priv = profile->priv;

	if (g_strcmp0 (property, CD_PROFILE_PROPERTY_FILENAME) == 0) {
		ret = cd_profile_set_filename (profile,
					       value,
					       error);
		if (!ret)
			goto out;
	} else if (g_strcmp0 (property, CD_PROFILE_PROPERTY_QUALIFIER) == 0) {
		cd_profile_set_qualifier (profile, value);
		cd_profile_dbus_emit_property_changed (profile,
						       property,
						       g_variant_new_string (value));
	} else if (g_strcmp0 (property, CD_PROFILE_PROPERTY_FORMAT) == 0) {
		cd_profile_set_format (profile, value);
		cd_profile_dbus_emit_property_changed (profile,
						       property,
						       g_variant_new_string (value));
	} else if (g_strcmp0 (property, CD_PROFILE_PROPERTY_COLORSPACE) == 0) {
		priv->colorspace = cd_colorspace_from_string (value);
		cd_profile_dbus_emit_property_changed (profile,
						       property,
						       g_variant_new_string (value));
	} else {
		/* add to metadata */
		g_hash_table_insert (profile->priv->metadata,
				     g_strdup (property),
				     g_strdup (value));
		cd_profile_dbus_emit_property_changed (profile,
						       CD_PROFILE_PROPERTY_METADATA,
						       cd_profile_get_metadata_as_variant (profile));
		goto out;
	}

	/* emit global signal */
	cd_profile_dbus_emit_profile_changed (profile);
out:
	return ret;
}

/**
 * cd_profile_dbus_method_call:
 **/
static void
cd_profile_dbus_method_call (GDBusConnection *connection, const gchar *sender,
			    const gchar *object_path, const gchar *interface_name,
			    const gchar *method_name, GVariant *parameters,
			    GDBusMethodInvocation *invocation, gpointer user_data)
{
	gboolean ret;
	const gchar *property_name = NULL;
	const gchar *property_value = NULL;
	GError *error = NULL;
	CdProfile *profile = CD_PROFILE (user_data);

	/* return '' */
	if (g_strcmp0 (method_name, "SetProperty") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (connection,
						    sender,
						    "org.freedesktop.color-manager.modify-profile",
						    &error);
		if (!ret) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_PROFILE_ERROR,
							       CD_PROFILE_ERROR_FAILED_TO_AUTHENTICATE,
							       "%s", error->message);
			g_error_free (error);
			goto out;
		}

		/* set, and parse */
		g_variant_get (parameters, "(&s&s)",
			       &property_name,
			       &property_value);
		g_debug ("CdProfile %s:SetProperty(%s,%s)",
			 sender, property_name, property_value);
		ret = cd_profile_set_property_internal (profile,
							property_name,
							property_value,
							&error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			g_error_free (error);
			goto out;
		}
		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	/* return '' */
	if (g_strcmp0 (method_name, "InstallSystemWide") == 0) {

		/* require auth */
		g_debug ("CdProfile %s:InstallSystemWide() on %s",
			 sender, profile->priv->object_path);
		ret = cd_main_sender_authenticated (connection,
						    sender,
						    "org.freedesktop.color-manager.install-system-wide",
						    &error);
		if (!ret) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_PROFILE_ERROR,
							       CD_PROFILE_ERROR_FAILED_TO_AUTHENTICATE,
							       "%s", error->message);
			g_error_free (error);
			goto out;
		}

		/* copy systemwide */
		ret = cd_profile_install_system_wide (profile, &error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			g_error_free (error);
			goto out;
		}

		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}


	/* we suck */
	g_critical ("failed to process method %s", method_name);
out:
	return;
}

/**
 * cd_profile_dbus_get_property:
 **/
static GVariant *
cd_profile_dbus_get_property (GDBusConnection *connection_, const gchar *sender,
			     const gchar *object_path, const gchar *interface_name,
			     const gchar *property_name, GError **error,
			     gpointer user_data)
{
	GVariant *retval = NULL;
	CdProfile *profile = CD_PROFILE (user_data);

	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_TITLE) == 0) {
		retval = cd_profile_get_nullable_for_string (profile->priv->title);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_ID) == 0) {
		retval = cd_profile_get_nullable_for_string (profile->priv->id);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_QUALIFIER) == 0) {
		retval = cd_profile_get_nullable_for_string (profile->priv->qualifier);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_FORMAT) == 0) {
		retval = cd_profile_get_nullable_for_string (profile->priv->format);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_FILENAME) == 0) {
		retval = cd_profile_get_nullable_for_string (profile->priv->filename);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_KIND) == 0) {
		retval = g_variant_new_string (cd_profile_kind_to_string (profile->priv->kind));
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_COLORSPACE) == 0) {
		retval = g_variant_new_string (cd_colorspace_to_string (profile->priv->colorspace));
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_HAS_VCGT) == 0) {
		retval = g_variant_new_boolean (profile->priv->has_vcgt);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_IS_SYSTEM_WIDE) == 0) {
		retval = g_variant_new_boolean (profile->priv->is_system_wide);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_METADATA) == 0) {
		retval = cd_profile_get_metadata_as_variant (profile);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_CREATED) == 0) {
		retval = g_variant_new_int64 (profile->priv->created);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_SCOPE) == 0) {
		retval = g_variant_new_string (cd_object_scope_to_string (profile->priv->object_scope));
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_OWNER) == 0) {
		retval = g_variant_new_uint32 (profile->priv->owner);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_WARNINGS) == 0) {
		if (profile->priv->warnings != NULL) {
			retval = g_variant_new_strv ((const gchar * const *) profile->priv->warnings, -1);
		} else {
			const gchar *tmp[] = { NULL };
			retval = g_variant_new_strv (tmp, -1);
		}
		goto out;
	}

	g_critical ("failed to set property %s", property_name);
out:
	return retval;
}

/**
 * cd_profile_register_object:
 **/
gboolean
cd_profile_register_object (CdProfile *profile,
			    GDBusConnection *connection,
			    GDBusInterfaceInfo *info,
			    GError **error)
{
	GError *error_local = NULL;
	gboolean ret = FALSE;

	static const GDBusInterfaceVTable interface_vtable = {
		cd_profile_dbus_method_call,
		cd_profile_dbus_get_property,
		NULL
	};

	profile->priv->connection = connection;
	profile->priv->registration_id = g_dbus_connection_register_object (
		connection,
		profile->priv->object_path,
		info,
		&interface_vtable,
		profile,  /* user_data */
		NULL,  /* user_data_free_func */
		&error_local); /* GError** */
	if (profile->priv->registration_id == 0) {
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_INTERNAL,
			     "failed to register object: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cd_profile_get_fake_md5:
 *
 * this is a complete hack to work around the lack of DICT
 * support, and to give gnome-color-manager something to key on
 **/
static gchar *
cd_profile_get_fake_md5 (const gchar *filename)
{
	gchar *basename;
	gchar *md5 = NULL;

	basename = g_path_get_basename (filename);
	if (!g_str_has_prefix (basename, "edid-"))
		goto out;
	if (strlen (basename) != 41)
		goto out;

	/* parse edid-f467c2e85a0abdef9415d5028e240631.icc */
	basename[37] = '\0';
	md5 = g_strdup (&basename[5]);
out:
	g_free (basename);
	return md5;
}

/**
 * cd_profile_get_precooked_md5:
 **/
static gchar *
cd_profile_get_precooked_md5 (cmsHPROFILE lcms_profile)
{
	cmsUInt8Number profile_id[16];
	gboolean md5_precooked = FALSE;
	guint i;
	gchar *md5 = NULL;

	/* check to see if we have a pre-cooked MD5 */
	cmsGetHeaderProfileID (lcms_profile, profile_id);
	for (i = 0; i < 16; i++) {
		if (profile_id[i] != 0) {
			md5_precooked = TRUE;
			break;
		}
	}
	if (!md5_precooked)
		goto out;

	/* convert to a hex string */
	md5 = g_new0 (gchar, 32 + 1);
	for (i = 0; i < 16; i++)
		g_snprintf (md5 + i*2, 3, "%02x", profile_id[i]);
out:
	return md5;
}

/**
 * cd_profile_set_metadata_from_profile:
 **/
static void
cd_profile_set_metadata_from_profile (CdProfile *profile,
				      cmsHPROFILE lcms_profile)
{
	cmsHANDLE dict;
	const cmsDICTentry* entry;
	gchar ascii_name[1024];
	gchar ascii_value[1024];
	CdProfilePrivate *priv = profile->priv;

	/* does profile have metadata? */
	dict = cmsReadTag (lcms_profile, cmsSigMetaTag);
	if (dict == NULL) {
		g_debug ("%s (%s) has no DICT tag",
			 priv->id ? priv->id : "new profile",
			 priv->filename);
		return;
	}

	/* read each bit of metadata */
	for (entry = cmsDictGetEntryList (dict);
	     entry != NULL;
	     entry = cmsDictNextEntry (entry)) {

		/* convert from wchar_t to char */
		wcstombs (ascii_name, entry->Name, sizeof (ascii_name));
		wcstombs (ascii_value, entry->Value, sizeof (ascii_value));
		g_debug ("Adding metadata %s=%s",
			 ascii_name, ascii_value);
		g_hash_table_insert (priv->metadata,
				     g_strdup (ascii_name),
				     g_strdup (ascii_value));
	}
}

/**
 * cd_profile_fixup_title:
 **/
static gchar *
cd_profile_fixup_title (const gchar *text)
{
	gchar *title = NULL;
	gchar *tmp;
	guint len;

	/* nothing set */
	if (text == NULL)
		goto out;

	/* remove the hardcoded confusing title */
	if (g_str_has_prefix (text, "Default, "))
		text += 9;
	title = g_strdup (text);

	/* hack to make old profiles look nice */
	tmp = g_strstr_len (title, -1, " (201");
	if (tmp != NULL)
		*tmp = '\0';

	/* make underscores into spaces */
	g_strdelimit (title, "_", ' ');

	/* remove any shitty suffix */
	if (g_str_has_suffix (title, ".icc") ||
	    g_str_has_suffix (title, ".ICC") ||
	    g_str_has_suffix (title, ".icm") ||
	    g_str_has_suffix (title, ".ICM")) {
		len = strlen (title);
		if (len > 4)
			title[len - 4] = '\0';
	}
out:
	return title;
}

/**
 * cd_profile_check_vcgt:
 **/
static CdProfileWarning
cd_profile_check_vcgt (cmsHPROFILE profile)
{
	CdProfileWarning warning = CD_PROFILE_WARNING_NONE;
	cmsFloat32Number in;
	cmsFloat32Number now[3];
	cmsFloat32Number previous[3] = { -1, -1, -1};
	const cmsToneCurve **vcgt;
	const guint size = 32;
	guint i;

	/* does profile have monotonic VCGT */
	vcgt = cmsReadTag (profile, cmsSigVcgtTag);
	if (vcgt == NULL)
		goto out;
	for (i = 0; i < size; i++) {
		in = (gdouble) i / (gdouble) (size - 1);
		now[0] = cmsEvalToneCurveFloat(vcgt[0], in);
		now[1] = cmsEvalToneCurveFloat(vcgt[1], in);
		now[2] = cmsEvalToneCurveFloat(vcgt[2], in);

		/* check VCGT is increasing */
		if (i > 0) {
			if (now[0] < previous[0] ||
			    now[1] < previous[1] ||
			    now[2] < previous[2]) {
				warning = CD_PROFILE_WARNING_VCGT_NON_MONOTONIC;
				goto out;
			}
		}
		previous[0] = now[0];
		previous[1] = now[1];
		previous[2] = now[2];
	}
out:
	return warning;
}

/**
 * cd_profile_check_scum_dot:
 **/
static CdProfileWarning
cd_profile_check_scum_dot (cmsHPROFILE profile)
{
	CdProfileWarning warning = CD_PROFILE_WARNING_NONE;
	cmsCIELab white;
	cmsHPROFILE profile_lab;
	cmsHTRANSFORM transform;
	guint8 rgb[3] = { 0, 0, 0 };

	/* do Lab to RGB transform of 100,0,0 */
	profile_lab = cmsCreateLab2Profile (cmsD50_xyY ());
	transform = cmsCreateTransform (profile_lab, TYPE_Lab_DBL,
					profile, TYPE_RGB_8,
					INTENT_RELATIVE_COLORIMETRIC,
					cmsFLAGS_NOOPTIMIZE);
	if (transform == NULL) {
		g_warning ("failed to setup Lab -> RGB transform");
		goto out;
	}
	white.L = 100.0;
	white.a = 0.0;
	white.b = 0.0;
	cmsDoTransform (transform, &white, rgb, 1);
	if (rgb[0] != 255 || rgb[1] != 255 || rgb[2] != 255) {
		warning = CD_PROFILE_WARNING_SCUM_DOT;
		goto out;
	}
out:
	if (profile_lab != NULL)
		cmsCloseProfile (profile_lab);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	return warning;
}

/**
 * cd_profile_check_primaries:
 **/
static CdProfileWarning
cd_profile_check_primaries (cmsHPROFILE profile)
{
	CdProfileWarning warning = CD_PROFILE_WARNING_NONE;
	cmsCIEXYZ *tmp;

	/* check red */
	tmp = cmsReadTag (profile, cmsSigRedColorantTag);
	if (tmp == NULL)
		goto out;
	if (tmp->X < 0.0f || tmp->Y < 0.0f || tmp->Z < 0.0f) {
		warning = CD_PROFILE_WARNING_PRIMARIES_INVALID;
		goto out;
	}

	/* check green */
	tmp = cmsReadTag (profile, cmsSigGreenColorantTag);
	if (tmp == NULL)
		goto out;
	if (tmp->X < 0.0f || tmp->Y < 0.0f || tmp->Z < 0.0f) {
		warning = CD_PROFILE_WARNING_PRIMARIES_INVALID;
		goto out;
	}

	/* check blue */
	tmp = cmsReadTag (profile, cmsSigBlueColorantTag);
	if (tmp == NULL)
		goto out;
	if (tmp->X < 0.0f || tmp->Y < 0.0f || tmp->Z < 0.0f) {
		warning = CD_PROFILE_WARNING_PRIMARIES_INVALID;
		goto out;
	}
out:
	return warning;
}

/**
 * cd_profile_check_gray_axis:
 **/
static CdProfileWarning
cd_profile_check_gray_axis (cmsHPROFILE profile)
{
	CdProfileWarning warning = CD_PROFILE_WARNING_NONE;
	cmsCIELab gray[16];
	cmsHPROFILE profile_lab = NULL;
	cmsHTRANSFORM transform = NULL;
	const gdouble gray_error = 5.0f;
	gdouble last_l = -1;
	guint8 rgb[3*16];
	guint8 tmp;
	guint i;

	/* only do this for display profiles */
	if (cmsGetDeviceClass (profile) != cmsSigDisplayClass)
		goto out;

	/* do Lab to RGB transform of 100,0,0 */
	profile_lab = cmsCreateLab2Profile (cmsD50_xyY ());
	transform = cmsCreateTransform (profile, TYPE_RGB_8,
					profile_lab, TYPE_Lab_DBL,
					INTENT_RELATIVE_COLORIMETRIC,
					cmsFLAGS_NOOPTIMIZE);
	if (transform == NULL) {
		g_warning ("failed to setup RGB -> Lab transform");
		goto out;
	}

	/* run a 16 item gray ramp through the transform */
	for (i = 0; i < 16; i++) {
		tmp = (255.0f / (16.0f - 1)) * i;
		rgb[(i * 3) + 0] = tmp;
		rgb[(i * 3) + 1] = tmp;
		rgb[(i * 3) + 2] = tmp;
	}
	cmsDoTransform (transform, rgb, gray, 16);

	/* check a/b is small */
	for (i = 0; i < 16; i++) {
		if (gray[i].a > gray_error ||
		    gray[i].b > gray_error) {
			warning = CD_PROFILE_WARNING_GRAY_AXIS_INVALID;
			goto out;
		}
	}

	/* check it's monotonic */
	for (i = 0; i < 16; i++) {
		if (last_l > 0 && gray[i].L < last_l) {
			warning = CD_PROFILE_WARNING_GRAY_AXIS_NON_MONOTONIC;
			goto out;
		}
		last_l = gray[i].L;
	}
out:
	if (profile_lab != NULL)
		cmsCloseProfile (profile_lab);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	return warning;
}

/**
 * cd_profile_check_d50_whitepoint:
 **/
static CdProfileWarning
cd_profile_check_d50_whitepoint (cmsHPROFILE profile)
{
	CdProfileWarning warning = CD_PROFILE_WARNING_NONE;
	cmsCIExyY tmp;
	cmsCIEXYZ additive;
	cmsCIEXYZ primaries[4];
	cmsHPROFILE profile_lab;
	cmsHTRANSFORM transform;
	const cmsCIEXYZ *d50;
	const gdouble rgb_error = 0.05;
	const gdouble additive_error = 0.1f;
	const gdouble white_error = 0.05;
	guint8 rgb[3*4];
	guint i;

	/* do Lab to RGB transform to get primaries */
	profile_lab = cmsCreateXYZProfile ();
	transform = cmsCreateTransform (profile, TYPE_RGB_8,
					profile_lab, TYPE_XYZ_DBL,
					INTENT_RELATIVE_COLORIMETRIC,
					cmsFLAGS_NOOPTIMIZE);
	if (transform == NULL) {
		g_warning ("failed to setup RGB -> XYZ transform");
		goto out;
	}

	/* Run RGBW through the transform */
	rgb[0 + 0] = 255;
	rgb[0 + 1] = 0;
	rgb[0 + 2] = 0;
	rgb[3 + 0] = 0;
	rgb[3 + 1] = 255;
	rgb[3 + 2] = 0;
	rgb[6 + 0] = 0;
	rgb[6 + 1] = 0;
	rgb[6 + 2] = 255;
	rgb[9 + 0] = 255;
	rgb[9 + 1] = 255;
	rgb[9 + 2] = 255;
	cmsDoTransform (transform, rgb, primaries, 4);

	/* check red is in gamut */
	cmsXYZ2xyY (&tmp, &primaries[0]);
	if (tmp.x - 0.735 > rgb_error || 0.265 - tmp.y > rgb_error) {
		warning = CD_PROFILE_WARNING_PRIMARIES_UNLIKELY;
		goto out;
	}

	/* check green is in gamut */
	cmsXYZ2xyY (&tmp, &primaries[1]);
	if (0.160 - tmp.x > rgb_error || tmp.y - 0.840 > rgb_error) {
		warning = CD_PROFILE_WARNING_PRIMARIES_UNLIKELY;
		goto out;
	}

	/* check blue is in gamut */
	cmsXYZ2xyY (&tmp, &primaries[2]);
	if (0.037 - tmp.x > rgb_error || tmp.y - 0.358 > rgb_error) {
		warning = CD_PROFILE_WARNING_PRIMARIES_UNLIKELY;
		goto out;
	}

	/* only do the rest for display profiles */
	if (cmsGetDeviceClass (profile) != cmsSigDisplayClass)
		goto out;

	/* check white is D50 */
	d50 = cmsD50_XYZ();
	if (fabs (primaries[3].X - d50->X) > white_error ||
	    fabs (primaries[3].Y - d50->Y) > white_error ||
	    fabs (primaries[3].Z - d50->Z) > white_error) {
		warning = CD_PROFILE_WARNING_WHITEPOINT_INVALID;
		goto out;
	}

	/* check primaries add up to D50 */
	additive.X = 0;
	additive.Y = 0;
	additive.Z = 0;
	for (i = 0; i < 3; i++) {
		additive.X += primaries[i].X;
		additive.Y += primaries[i].Y;
		additive.Z += primaries[i].Z;
	}
	if (fabs (additive.X - d50->X) > additive_error ||
	    fabs (additive.Y - d50->Y) > additive_error ||
	    fabs (additive.Z - d50->Z) > additive_error) {
		warning = CD_PROFILE_WARNING_PRIMARIES_NON_ADDITIVE;
		goto out;
	}
out:
	if (profile_lab != NULL)
		cmsCloseProfile (profile_lab);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	return warning;
}

/**
 * cd_profile_get_warnings:
 **/
static GArray *
cd_profile_get_warnings (cmsHPROFILE profile)
{
	GArray *flags;
	gboolean ret;
	gchar ascii_name[1024];
	CdProfileWarning warning;

	flags = g_array_new (FALSE, FALSE, sizeof (CdProfileWarning));

	/* check that the profile has a description and a copyright */
	ret = cmsGetProfileInfoASCII (profile, cmsInfoDescription, "en", "US", ascii_name, 1024);
	if (!ret || ascii_name[0] == '\0') {
		warning = CD_PROFILE_WARNING_DESCRIPTION_MISSING;
		g_array_append_val (flags, warning);
	}
	ret = cmsGetProfileInfoASCII (profile, cmsInfoCopyright, "en", "US", ascii_name, 1024);
	if (!ret || ascii_name[0] == '\0') {
		warning = CD_PROFILE_WARNING_COPYRIGHT_MISSING;
		g_array_append_val (flags, warning);
	}

	/* not a RGB space */
	if (cmsGetColorSpace (profile) != cmsSigRgbData)
		goto out;

	/* does profile have monotonic VCGT */
	warning = cd_profile_check_vcgt (profile);
	if (warning != CD_PROFILE_WARNING_NONE)
		g_array_append_val (flags, warning);

	/* if Lab 100,0,0 does not map to RGB 255,255,255 for relative
	 * colorimetric then white it will not work on printers */
	warning = cd_profile_check_scum_dot (profile);
	if (warning != CD_PROFILE_WARNING_NONE)
		g_array_append_val (flags, warning);

	/* gray should give low a/b and should be monotonic */
	warning = cd_profile_check_gray_axis (profile);
	if (warning != CD_PROFILE_WARNING_NONE)
		g_array_append_val (flags, warning);

	/* tristimulus values cannot be negative */
	warning = cd_profile_check_primaries (profile);
	if (warning != CD_PROFILE_WARNING_NONE)
		g_array_append_val (flags, warning);

	/* check whitepoint works out to D50 */
	warning = cd_profile_check_d50_whitepoint (profile);
	if (warning != CD_PROFILE_WARNING_NONE)
		g_array_append_val (flags, warning);
out:
	return flags;
}

/**
 * cd_profile_set_from_profile:
 **/
static gboolean
cd_profile_set_from_profile (CdProfile *profile,
			     cmsHPROFILE lcms_profile,
			     GError **error)
{
	CdProfilePrivate *priv = profile->priv;
	CdProfileWarning warning;
	cmsColorSpaceSignature color_space;
	const gchar *value;
	GArray *flags = NULL;
	gboolean ret = FALSE;
	gchar text[1024];
	guint i;
	struct tm created;

	/* get the description as the title */
	cmsGetProfileInfoASCII (lcms_profile,
				cmsInfoDescription,
				"en", "US",
				text, 1024);
	priv->title = cd_profile_fixup_title (text);

	/* get the profile kind */
	switch (cmsGetDeviceClass (lcms_profile)) {
	case cmsSigInputClass:
		priv->kind = CD_PROFILE_KIND_INPUT_DEVICE;
		break;
	case cmsSigDisplayClass:
		priv->kind = CD_PROFILE_KIND_DISPLAY_DEVICE;
		break;
	case cmsSigOutputClass:
		priv->kind = CD_PROFILE_KIND_OUTPUT_DEVICE;
		break;
	case cmsSigLinkClass:
		priv->kind = CD_PROFILE_KIND_DEVICELINK;
		break;
	case cmsSigColorSpaceClass:
		priv->kind = CD_PROFILE_KIND_COLORSPACE_CONVERSION;
		break;
	case cmsSigAbstractClass:
		priv->kind = CD_PROFILE_KIND_ABSTRACT;
		break;
	case cmsSigNamedColorClass:
		priv->kind = CD_PROFILE_KIND_NAMED_COLOR;
		break;
	default:
		priv->kind = CD_PROFILE_KIND_UNKNOWN;
	}

	/* get colorspace */
	color_space = cmsGetColorSpace (lcms_profile);
	switch (color_space) {
	case cmsSigXYZData:
		priv->colorspace = CD_COLORSPACE_XYZ;
		break;
	case cmsSigLabData:
		priv->colorspace = CD_COLORSPACE_LAB;
		break;
	case cmsSigLuvData:
		priv->colorspace = CD_COLORSPACE_LUV;
		break;
	case cmsSigYCbCrData:
		priv->colorspace = CD_COLORSPACE_YCBCR;
		break;
	case cmsSigYxyData:
		priv->colorspace = CD_COLORSPACE_YXY;
		break;
	case cmsSigRgbData:
		priv->colorspace = CD_COLORSPACE_RGB;
		break;
	case cmsSigGrayData:
		priv->colorspace = CD_COLORSPACE_GRAY;
		break;
	case cmsSigHsvData:
		priv->colorspace = CD_COLORSPACE_HSV;
		break;
	case cmsSigCmykData:
		priv->colorspace = CD_COLORSPACE_CMYK;
		break;
	case cmsSigCmyData:
		priv->colorspace = CD_COLORSPACE_CMY;
		break;
	default:
		priv->colorspace = CD_COLORSPACE_UNKNOWN;
	}

	/* get metadata from the DICTionary */
	cd_profile_set_metadata_from_profile (profile, lcms_profile);

	/* set the format from the metadata */
	value = g_hash_table_lookup (priv->metadata,
				     CD_PROFILE_METADATA_MAPPING_FORMAT);
	if (value != NULL)
		cd_profile_set_format (profile, value);

	/* set the qualifier from the metadata */
	value = g_hash_table_lookup (priv->metadata,
				     CD_PROFILE_METADATA_MAPPING_QUALIFIER);
	if (value != NULL)
		cd_profile_set_qualifier (profile, value);

	/* set a generic qualifier if there was nothing set before */
	if (priv->colorspace == CD_COLORSPACE_RGB &&
	    priv->qualifier == NULL) {
		cd_profile_set_format (profile, "ColorSpace..");
		cd_profile_set_qualifier (profile, "RGB..");
	}

	/* get the profile created time and date */
	ret = cmsGetHeaderCreationDateTime (lcms_profile, &created);
	if (ret) {
		priv->created = mktime (&created);
	} else {
		g_warning ("failed to get created time");
		priv->created = 0;
	}

	/* do we have vcgt */
	priv->has_vcgt = cmsIsTag (lcms_profile, cmsSigVcgtTag);

	/* get the checksum for the profile if we can */
	priv->checksum = cd_profile_get_precooked_md5 (lcms_profile);

	/* get any warnings for the profile */
	flags = cd_profile_get_warnings (lcms_profile);
	priv->warnings = g_new0 (gchar *, flags->len + 1);
	if (flags->len > 0) {
		for (i = 0; i < flags->len; i++) {
			warning = g_array_index (flags, CdProfileWarning, i);
			priv->warnings[i] = g_strdup (cd_profile_warning_to_string (warning));
		}
	}

	/* success */
	ret = TRUE;
	if (flags != NULL)
		g_array_unref (flags);
	return ret;
}

/**
 * cd_profile_emit_parsed_property_changed:
 **/
static void
cd_profile_emit_parsed_property_changed (CdProfile *profile)
{
	CdProfilePrivate *priv = profile->priv;

	cd_profile_dbus_emit_property_changed (profile,
					       CD_PROFILE_PROPERTY_FILENAME,
					       cd_profile_get_nullable_for_string (priv->filename));
	cd_profile_dbus_emit_property_changed (profile,
					       CD_PROFILE_PROPERTY_TITLE,
					       cd_profile_get_nullable_for_string (priv->title));
	cd_profile_dbus_emit_property_changed (profile,
					       CD_PROFILE_PROPERTY_KIND,
					       g_variant_new_string (cd_profile_kind_to_string (priv->kind)));
	cd_profile_dbus_emit_property_changed (profile,
					       CD_PROFILE_PROPERTY_COLORSPACE,
					       g_variant_new_string (cd_colorspace_to_string (priv->colorspace)));
	cd_profile_dbus_emit_property_changed (profile,
					       CD_PROFILE_PROPERTY_HAS_VCGT,
					       g_variant_new_boolean (priv->has_vcgt));
	cd_profile_dbus_emit_property_changed (profile,
					       CD_PROFILE_PROPERTY_METADATA,
					       cd_profile_get_metadata_as_variant (profile));
	cd_profile_dbus_emit_property_changed (profile,
					       CD_PROFILE_PROPERTY_QUALIFIER,
					       cd_profile_get_nullable_for_string (priv->qualifier));
	cd_profile_dbus_emit_property_changed (profile,
					       CD_PROFILE_PROPERTY_FORMAT,
					       cd_profile_get_nullable_for_string (priv->format));
	cd_profile_dbus_emit_property_changed (profile,
					       CD_PROFILE_PROPERTY_CREATED,
					       g_variant_new_int64 (priv->created));
}

/**
 * cd_profile_set_filename:
 **/
gboolean
cd_profile_set_filename (CdProfile *profile,
			 const gchar *filename,
			 GError **error)
{
	cmsHPROFILE lcms_profile = NULL;
	gboolean ret = FALSE;
	gchar *fake_md5 = NULL;
	gchar *data = NULL;
	gsize len;
	const gchar *tmp;
	CdProfilePrivate *priv = profile->priv;

	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);

	/* save filename */
	g_free (priv->filename);
	priv->filename = g_strdup (filename);

	/* if we didn't get the metadata from the DICT tag then
	 * guess it from the filename.
	 * we can delete this hack when lcms2 >= 2.2 is a hard dep */
	tmp = g_hash_table_lookup (priv->metadata, CD_PROFILE_METADATA_EDID_MD5);
	if (tmp == NULL) {
		fake_md5 = cd_profile_get_fake_md5 (priv->filename);
		if (fake_md5 != NULL) {
			g_hash_table_insert (priv->metadata,
					     g_strdup (CD_PROFILE_METADATA_EDID_MD5),
					     g_strdup (fake_md5));
			cd_profile_dbus_emit_property_changed (profile,
							       CD_PROFILE_PROPERTY_METADATA,
							       cd_profile_get_metadata_as_variant (profile));
		}
	}

	/* check we're not already set using the fd */
	if (priv->kind != CD_PROFILE_KIND_UNKNOWN) {
		ret = TRUE;
		g_debug ("profile '%s' already set",
			 priv->object_path);
		goto out;
	} else if (!priv->is_system_wide) {
#ifndef HAVE_FD_FALLBACK
		/* we're not allowing the dameon to open the file */
		ret = FALSE;
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_INTERNAL,
			     "Failed to open %s as client did not send FD and "
			     "daemon is not compiled with --enable-fd-fallback",
			     filename);
		goto out;
#endif
	}

	/* parse the ICC file */
	lcms_profile = cmsOpenProfileFromFile (filename, "r");
	if (lcms_profile == NULL) {
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_FAILED_TO_PARSE,
			     "failed to parse %s",
			     filename);
		goto out;
	}

	/* set the virtual profile from the lcms profile */
	ret = cd_profile_set_from_profile (profile, lcms_profile, error);
	if (!ret)
		goto out;

	/* try the metadata if available */
	if (priv->checksum == NULL) {
		tmp = g_hash_table_lookup (profile->priv->metadata,
					   CD_PROFILE_METADATA_FILE_CHECKSUM);
		if (tmp != NULL &&
		    strlen (tmp) == 32) {
			priv->checksum = g_strdup (tmp);
		}
	}

	/* fall back to calculating it ourselves */
	if (priv->checksum == NULL) {
		g_debug ("%s has no profile-id nor %s, falling back "
			 "to slow MD5",
			 priv->filename,
			 CD_PROFILE_METADATA_FILE_CHECKSUM);
		ret = g_file_get_contents (priv->filename,
					   &data, &len, error);
		if (!ret)
			goto out;
		priv->checksum = g_compute_checksum_for_data (G_CHECKSUM_MD5,
							      (const guchar *) data,
							      len);
	}

	/* emit all the things that could have changed */
	cd_profile_emit_parsed_property_changed (profile);
out:
	g_free (data);
	g_free (fake_md5);
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	return ret;
}

/**
 * cd_profile_set_fd:
 **/
gboolean
cd_profile_set_fd (CdProfile *profile,
		   gint fd,
		   GError **error)
{
	cmsHPROFILE lcms_profile = NULL;
	FILE *stream = NULL;
	gboolean ret = FALSE;
	CdProfilePrivate *priv = profile->priv;

	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);

	/* check we're not already set */
	if (priv->kind != CD_PROFILE_KIND_UNKNOWN) {
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_INTERNAL,
			     "profile '%s' already set",
			     priv->object_path);
		goto out;
	}

	/* convert the file descriptor to a stream */
	stream = fdopen (fd, "r");
	if (stream == NULL) {
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_FAILED_TO_READ,
			     "failed to open stream from fd %i",
			     fd);
		goto out;
	}

	/* create a mapped file */
#if GLIB_CHECK_VERSION(2,31,0)
	priv->mapped_file = g_mapped_file_new_from_fd (fd, FALSE, error);
	if (priv->mapped_file == NULL) {
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_FAILED_TO_READ,
			     "failed to create mapped file from fd %i",
			     fd);
		goto out;
	}
#endif

	/* parse the ICC file */
	lcms_profile = cmsOpenProfileFromStream (stream, "r");
	if (lcms_profile == NULL) {
		g_set_error_literal (error,
				     CD_PROFILE_ERROR,
				     CD_PROFILE_ERROR_FAILED_TO_READ,
				     "failed to open stream");
		goto out;
	}

	/* set the virtual profile from the lcms profile */
	ret = cd_profile_set_from_profile (profile, lcms_profile, error);
	if (!ret)
		goto out;

	/* emit all the things that could have changed */
	cd_profile_emit_parsed_property_changed (profile);
out:
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	return ret;
}

/**
 * cd_profile_get_qualifier:
 **/
const gchar *
cd_profile_get_qualifier (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return profile->priv->qualifier;
}

/**
 * cd_profile_set_qualifier:
 **/
void
cd_profile_set_qualifier (CdProfile *profile, const gchar *qualifier)
{
	g_return_if_fail (CD_IS_PROFILE (profile));
	g_free (profile->priv->qualifier);
	profile->priv->qualifier = g_strdup (qualifier);
}

/**
 * cd_profile_set_format:
 **/
void
cd_profile_set_format (CdProfile *profile, const gchar *format)
{
	g_return_if_fail (CD_IS_PROFILE (profile));
	g_free (profile->priv->format);
	profile->priv->format = g_strdup (format);
}

/**
 * cd_profile_get_title:
 **/
const gchar *
cd_profile_get_title (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return profile->priv->title;
}

/**
 * cd_profile_get_metadata:
 **/
GHashTable *
cd_profile_get_metadata (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return profile->priv->metadata;
}

/**
 * cd_profile_get_metadata_item:
 **/
const gchar *
cd_profile_get_metadata_item (CdProfile *profile, const gchar *key)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return g_hash_table_lookup (profile->priv->metadata, key);
}

/**
 * cd_profile_get_kind:
 **/
CdProfileKind
cd_profile_get_kind (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), 0);
	return profile->priv->kind;
}

/**
 * cd_profile_get_colorspace:
 **/
CdColorspace
cd_profile_get_colorspace (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), 0);
	return profile->priv->colorspace;
}

/**
 * cd_profile_get_has_vcgt:
 **/
gboolean
cd_profile_get_has_vcgt (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	return profile->priv->has_vcgt;
}

/**
 * cd_profile_get_checksum:
 **/
const gchar *
cd_profile_get_checksum (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return profile->priv->checksum;
}

/**
 * cd_profile_name_vanished_cb:
 **/
static void
cd_profile_name_vanished_cb (GDBusConnection *connection,
			     const gchar *name,
			     gpointer user_data)
{
	CdProfile *profile = CD_PROFILE (user_data);
	g_debug ("CdProfile: emit 'invalidate' as %s vanished", name);
	g_signal_emit (profile, signals[SIGNAL_INVALIDATE], 0);
}

/**
 * cd_profile_watch_sender:
 **/
void
cd_profile_watch_sender (CdProfile *profile, const gchar *sender)
{
	g_return_if_fail (CD_IS_PROFILE (profile));
	profile->priv->watcher_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
						      sender,
						      G_BUS_NAME_WATCHER_FLAGS_NONE,
						      NULL,
						      cd_profile_name_vanished_cb,
						      profile,
						      NULL);
}

/**
 * cd_profile_get_property:
 **/
static void
cd_profile_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	CdProfile *profile = CD_PROFILE (object);
	CdProfilePrivate *priv = profile->priv;

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_value_set_string (value, priv->object_path);
		break;
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * cd_profile_set_property:
 **/
static void
cd_profile_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CdProfile *profile = CD_PROFILE (object);
	CdProfilePrivate *priv = profile->priv;

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_free (priv->object_path);
		priv->object_path = g_strdup (g_value_get_string (value));
		break;
	case PROP_ID:
		g_free (priv->id);
		priv->id = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * cd_profile_class_init:
 **/
static void
cd_profile_class_init (CdProfileClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_profile_finalize;
	object_class->get_property = cd_profile_get_property;
	object_class->set_property = cd_profile_set_property;

	/**
	 * CdProfile:object-path:
	 */
	pspec = g_param_spec_string ("object-path", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_OBJECT_PATH, pspec);

	/**
	 * CdProfile:id:
	 */
	pspec = g_param_spec_string ("id", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ID, pspec);

	/**
	 * CdProfile::invalidate:
	 **/
	signals[SIGNAL_INVALIDATE] =
		g_signal_new ("invalidate",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdProfileClass, invalidate),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (CdProfilePrivate));
}

/**
 * cd_profile_init:
 **/
static void
cd_profile_init (CdProfile *profile)
{
	profile->priv = CD_PROFILE_GET_PRIVATE (profile);
	profile->priv->metadata = g_hash_table_new_full (g_str_hash,
							 g_str_equal,
							 g_free,
							 g_free);
}

/**
 * cd_profile_finalize:
 **/
static void
cd_profile_finalize (GObject *object)
{
	CdProfile *profile = CD_PROFILE (object);
	CdProfilePrivate *priv = profile->priv;

	if (priv->watcher_id > 0)
		g_bus_unwatch_name (priv->watcher_id);
	if (priv->registration_id > 0) {
		g_dbus_connection_unregister_object (priv->connection,
						     priv->registration_id);
	}
	if (priv->mapped_file != NULL)
		g_mapped_file_unref (priv->mapped_file);
	g_free (priv->filename);
	g_free (priv->qualifier);
	g_free (priv->format);
	g_free (priv->title);
	g_free (priv->id);
	g_free (priv->checksum);
	g_free (priv->object_path);
	g_strfreev (priv->warnings);
	g_hash_table_unref (priv->metadata);

	G_OBJECT_CLASS (cd_profile_parent_class)->finalize (object);
}

/**
 * cd_profile_new:
 **/
CdProfile *
cd_profile_new (void)
{
	CdProfile *profile;
	profile = g_object_new (CD_TYPE_PROFILE, NULL);
	return CD_PROFILE (profile);
}

