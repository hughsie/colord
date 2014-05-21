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

#include <gio/gio.h>
#include <glib-object.h>
#include <lcms2.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <math.h>

#include "cd-common.h"
#include "cd-profile.h"
#include "cd-profile-db.h"
#include "cd-resources.h"

static void	cd_profile_finalize	(GObject	*object);
static void	cd_profile_set_filename	(CdProfile	*profile,
					 const gchar	*filename);

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
	guint				 score;
	CdProfileDb			*db;
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

	/* by default, prefer systemwide profiles over user profiles */
	profile->priv->score += 1;
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
 * cd_profile_set_metadata:
 **/
static void
cd_profile_set_metadata (CdProfile *profile,
			 const gchar *property,
			 const gchar *value)
{
	/* i1Profiler sets this */
	if (g_strcmp0 (property, "CreatorApp") == 0)
		property = CD_PROFILE_METADATA_CMF_PRODUCT;
	g_hash_table_insert (profile->priv->metadata,
			     g_strdup (property),
			     g_strdup (value));
}

/**
 * cd_profile_set_id:
 **/
void
cd_profile_set_id (CdProfile *profile, const gchar *id)
{
	CdStandardSpace standard_space = CD_STANDARD_SPACE_UNKNOWN;

	g_return_if_fail (CD_IS_PROFILE (profile));

	g_free (profile->priv->id);
	profile->priv->id = g_strdup (id);

	/* all profiles have a score initially */
	profile->priv->score = 1;

	/* http://www.color.org/srgbprofiles.xalter */
	if (g_strcmp0 (id, "icc-34562abf994ccd066d2c5721d0d68c5d") == 0) {
		/* sRGB_v4_ICC_preference */
		standard_space = CD_STANDARD_SPACE_SRGB;
		profile->priv->score = 10;
	}
	if (g_strcmp0 (id, "icc-fc66337837e2886bfd72e9838228f1b8") == 0) {
		/* sRGB_v4_ICC_preference_displayclass */
		standard_space = CD_STANDARD_SPACE_SRGB;
		profile->priv->score = 8;
	}
	if (g_strcmp0 (id, "icc-29f83ddeaff255ae7842fae4ca83390d") == 0) {
		/* sRGB_IEC61966-2-1_black_scaled */
		standard_space = CD_STANDARD_SPACE_SRGB;
		profile->priv->score = 6;
	}
	if (g_strcmp0 (id, "icc-c95bd637e95d8a3b0df38f99c1320389") == 0) {
		/* sRGB_IEC61966-2-1_no_black_scaling */
		standard_space = CD_STANDARD_SPACE_SRGB;
		profile->priv->score = 4;
	}

	/* from http://download.adobe.com/pub/adobe/iccprofiles/ */
	if (g_strcmp0 (id, "icc-dea88382d899d5f6e573b432473ae138") == 0) {
		/* AdobeRGB1998 */
		standard_space = CD_STANDARD_SPACE_ADOBE_RGB;
		profile->priv->score = 10;
	}
	if (g_strcmp0 (id, "icc-91cf26c58e07eda724fdbf3eadce4505") == 0) {
		/* ColorMatchRGB */
		standard_space = CD_STANDARD_SPACE_LAST;
	}
	if (g_strcmp0 (id, "icc-2e54d10b392cac47226469ba2ea95bd8") == 0) {
		/* AppleRGB */
		standard_space = CD_STANDARD_SPACE_LAST;
	}

	/* add additional metadata to fix the GUIs */
	if (standard_space != CD_STANDARD_SPACE_UNKNOWN) {
		cd_profile_set_metadata (profile,
					 CD_PROFILE_METADATA_STANDARD_SPACE,
					 cd_standard_space_to_string (standard_space));
		cd_profile_set_metadata (profile,
					 CD_PROFILE_METADATA_DATA_SOURCE,
					 CD_PROFILE_METADATA_DATA_SOURCE_STANDARD);
	}

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
 * cd_profile_set_title:
 **/
static gboolean
cd_profile_set_title (CdProfile *profile,
		      const gchar *value,
		      guint sender_uid,
		      GError **error)
{
	gboolean ret = TRUE;
	CdProfilePrivate *priv = profile->priv;

	/* check title is suitable */
	if (value == NULL || strlen (value) < 3 ||
	    !g_utf8_validate (value, -1, NULL)) {
		ret = FALSE;
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_INPUT_INVALID,
			     "'Title' value input invalid: %s", value);
		goto out;
	}

	/* save in database */
	ret = cd_profile_db_set_property (priv->db, priv->id,
					  CD_PROFILE_PROPERTY_TITLE, sender_uid,
					  value, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * cd_profile_set_property_internal:
 **/
gboolean
cd_profile_set_property_internal (CdProfile *profile,
				  const gchar *property,
				  const gchar *value,
				  guint sender_uid,
				  GError **error)
{
	gboolean ret = TRUE;

	CdProfilePrivate *priv = profile->priv;

	/* sanity check the length of the key and value */
	if (strlen (property) > CD_DBUS_METADATA_KEY_LEN_MAX) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_CLIENT_ERROR,
				     CD_CLIENT_ERROR_INPUT_INVALID,
				     "metadata key length invalid");
		goto out;
	}
	if (value != NULL && strlen (value) > CD_DBUS_METADATA_VALUE_LEN_MAX) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_CLIENT_ERROR,
				     CD_CLIENT_ERROR_INPUT_INVALID,
				     "metadata value length invalid");
		goto out;
	}

	if (g_strcmp0 (property, CD_PROFILE_PROPERTY_FILENAME) == 0) {
		cd_profile_set_filename (profile, value);
		cd_profile_dbus_emit_property_changed (profile,
						       property,
						       g_variant_new_string (value));
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
	} else if (g_strcmp0 (property, CD_PROFILE_PROPERTY_TITLE) == 0) {
		ret = cd_profile_set_title (profile, value, sender_uid, error);
		if (!ret)
			goto out;
		cd_profile_dbus_emit_property_changed (profile, property,
						       g_variant_new_string (value));
	} else {
		/* add to metadata */
		cd_profile_set_metadata (profile, property, value);
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
	guint uid;
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

		/* get UID */
		uid = cd_main_get_sender_uid (connection, sender, &error);
		if (uid == G_MAXUINT) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_PROFILE_ERROR,
							       CD_PROFILE_ERROR_FAILED_TO_GET_UID,
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
		if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_FILENAME) == 0) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_PROFILE_ERROR,
							       CD_PROFILE_ERROR_PROPERTY_INVALID,
							       "Setting the %s property after "
							       "profile creation is no longer supported",
							       property_name);
			goto out;
		}
		ret = cd_profile_set_property_internal (profile,
							property_name,
							property_value,
							uid,
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
cd_profile_dbus_get_property (GDBusConnection *connection, const gchar *sender,
			     const gchar *object_path, const gchar *interface_name,
			     const gchar *property_name, GError **error,
			     gpointer user_data)
{
	GVariant *retval = NULL;
	CdProfile *profile = CD_PROFILE (user_data);
	CdProfilePrivate *priv = profile->priv;
	gboolean ret;
	gchar *title_db = NULL;
	guint uid;

	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_TITLE) == 0) {
		uid = cd_main_get_sender_uid (connection, sender, error);
		if (uid == G_MAXUINT)
			goto out;
		ret = cd_profile_db_get_property (priv->db, priv->id,
						  property_name, uid,
						  &title_db, error);
		if (!ret)
			goto out;
		if (title_db != NULL) {
			retval = cd_profile_get_nullable_for_string (title_db);
			goto out;
		}
		retval = cd_profile_get_nullable_for_string (priv->title);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_ID) == 0) {
		retval = cd_profile_get_nullable_for_string (priv->id);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_QUALIFIER) == 0) {
		retval = cd_profile_get_nullable_for_string (priv->qualifier);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_FORMAT) == 0) {
		retval = cd_profile_get_nullable_for_string (priv->format);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_FILENAME) == 0) {
		retval = cd_profile_get_nullable_for_string (priv->filename);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_KIND) == 0) {
		retval = g_variant_new_string (cd_profile_kind_to_string (priv->kind));
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_COLORSPACE) == 0) {
		retval = g_variant_new_string (cd_colorspace_to_string (priv->colorspace));
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_HAS_VCGT) == 0) {
		retval = g_variant_new_boolean (priv->has_vcgt);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_IS_SYSTEM_WIDE) == 0) {
		retval = g_variant_new_boolean (priv->is_system_wide);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_METADATA) == 0) {
		retval = cd_profile_get_metadata_as_variant (profile);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_CREATED) == 0) {
		retval = g_variant_new_int64 (priv->created);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_SCOPE) == 0) {
		retval = g_variant_new_string (cd_object_scope_to_string (priv->object_scope));
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_OWNER) == 0) {
		retval = g_variant_new_uint32 (priv->owner);
		goto out;
	}
	if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_WARNINGS) == 0) {
		if (priv->warnings != NULL) {
			retval = g_variant_new_strv ((const gchar * const *) priv->warnings, -1);
		} else {
			const gchar *tmp[] = { NULL };
			retval = g_variant_new_strv (tmp, -1);
		}
		goto out;
	}

	/* return an error */
	g_set_error (error,
		     CD_PROFILE_ERROR,
		     CD_PROFILE_ERROR_INTERNAL,
		     "failed to get profile property %s",
		     property_name);
out:
	g_free (title_db);
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
 * cd_profile_set_from_profile:
 **/
static gboolean
cd_profile_set_from_profile (CdProfile *profile,
			     CdIcc *icc,
			     GError **error)
{
	CdProfilePrivate *priv = profile->priv;
	CdProfileWarning warning;
	cmsHPROFILE lcms_profile;
	const gchar *key;
	const gchar *value;
	GArray *flags = NULL;
	gboolean ret = FALSE;
	GHashTable *metadata = NULL;
	GList *keys = NULL;
	GList *l;
	guint i;
	struct tm created;

	/* get the description as the title */
	value = cd_icc_get_description (icc, NULL, error);
	if (value == NULL)
		goto out;
	priv->title =  cd_profile_fixup_title (value);

	/* get the profile kind */
	priv->kind = cd_icc_get_kind (icc);
	priv->colorspace = cd_icc_get_colorspace (icc);

	/* get metadata */
	metadata = cd_icc_get_metadata (icc);
	keys = g_hash_table_get_keys (metadata);
	for (l = keys; l != NULL; l = l->next) {
		key = l->data;
		value = g_hash_table_lookup (metadata, key);
		g_debug ("Adding metadata %s=%s", key, value);
		g_hash_table_insert (priv->metadata,
				     g_strdup (key),
				     g_strdup (value));
	}

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
	lcms_profile = cd_icc_get_handle (icc);
	ret = cmsGetHeaderCreationDateTime (lcms_profile, &created);
	if (ret) {
		created.tm_isdst = -1;
		priv->created = mktime (&created);
	} else {
		g_warning ("failed to get created time");
		priv->created = 0;
	}

	/* do we have vcgt */
	priv->has_vcgt = cmsIsTag (lcms_profile, cmsSigVcgtTag);

	/* get the checksum for the profile if we can */
	priv->checksum = g_strdup (cd_icc_get_checksum (icc));

	/* get any warnings for the profile */
	flags = cd_icc_get_warnings (icc);
	priv->warnings = g_new0 (gchar *, flags->len + 1);
	if (flags->len > 0) {
		for (i = 0; i < flags->len; i++) {
			warning = g_array_index (flags, CdProfileWarning, i);
			priv->warnings[i] = g_strdup (cd_profile_warning_to_string (warning));
		}
	}

	/* success */
	ret = TRUE;
out:
	g_list_free (keys);
	if (metadata != NULL)
		g_hash_table_unref (metadata);
	if (flags != NULL)
		g_array_unref (flags);
	return ret;
}

/**
 * cd_profile_get_warnings:
 **/
const gchar **
cd_profile_get_warnings (CdProfile *profile)
{
	return (const gchar **) profile->priv->warnings;
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
 * cd_profile_load_from_icc:
 **/
gboolean
cd_profile_load_from_icc (CdProfile *profile, CdIcc *icc, GError **error)
{
	gboolean ret = FALSE;

	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);

	/* save filename */
	cd_profile_set_filename (profile, cd_icc_get_filename (icc));

	/* set the virtual profile from the lcms profile */
	ret = cd_profile_set_from_profile (profile, icc, error);
	if (!ret)
		goto out;

	/* emit all the things that could have changed */
	cd_profile_emit_parsed_property_changed (profile);
out:
	return ret;
}

/**
 * cd_profile_load_from_fd:
 **/
gboolean
cd_profile_load_from_fd (CdProfile *profile,
			 gint fd,
			 GError **error)
{
	CdIcc *icc = NULL;
	GError *error_local = NULL;
	CdProfilePrivate *priv = profile->priv;
	gboolean ret = FALSE;

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

	/* open fd and parse the file */
	icc = cd_icc_new ();
	ret = cd_icc_load_fd (icc,
			      fd,
			      CD_ICC_LOAD_FLAGS_METADATA,
			      &error_local);
	if (!ret) {
		g_set_error_literal (error,
				     CD_PROFILE_ERROR,
				     CD_PROFILE_ERROR_FAILED_TO_READ,
				     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* create a mapped file */
	priv->mapped_file = g_mapped_file_new_from_fd (fd, FALSE, error);
	if (priv->mapped_file == NULL) {
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_FAILED_TO_READ,
			     "failed to create mapped file from fd %i",
			     fd);
		goto out;
	}

	/* set the virtual profile from the lcms profile */
	ret = cd_profile_set_from_profile (profile, icc, error);
	if (!ret)
		goto out;

	/* emit all the things that could have changed */
	cd_profile_emit_parsed_property_changed (profile);
out:
	if (icc != NULL)
		g_object_unref (icc);
	return ret;
}

/**
 * cd_profile_load_from_filename:
 **/
gboolean
cd_profile_load_from_filename (CdProfile *profile,
			 const gchar *filename,
			 GError **error)
{
	CdIcc *icc = NULL;
	GError *error_local = NULL;
	CdProfilePrivate *priv = profile->priv;
	gboolean ret = FALSE;
	GFile *file = NULL;

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

	/* open fd and parse the file */
	icc = cd_icc_new ();
	file = g_file_new_for_path (filename);
	ret = cd_icc_load_file (icc,
				file,
				CD_ICC_LOAD_FLAGS_METADATA,
				NULL,
				&error_local);
	if (!ret) {
		g_set_error_literal (error,
				     CD_PROFILE_ERROR,
				     CD_PROFILE_ERROR_FAILED_TO_READ,
				     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* create a mapped file */
	priv->mapped_file = g_mapped_file_new (filename, FALSE, error);
	if (priv->mapped_file == NULL) {
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_FAILED_TO_READ,
			     "failed to create mapped file from filname %s",
			     filename);
		goto out;
	}

	/* set the virtual profile from the lcms profile */
	ret = cd_profile_set_from_profile (profile, icc, error);
	if (!ret)
		goto out;

	/* emit all the things that could have changed */
	cd_profile_emit_parsed_property_changed (profile);
out:
	if (icc != NULL)
		g_object_unref (icc);
	if (file != NULL)
		g_object_unref (file);
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
 * cd_profile_set_filename:
 **/
static void
cd_profile_set_filename (CdProfile *profile, const gchar *filename)
{
	g_return_if_fail (CD_IS_PROFILE (profile));
	g_free (profile->priv->filename);
	profile->priv->filename = g_strdup (filename);
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
 * cd_profile_get_score:
 *
 * Profiles from official vendors such as http://www.color.org/ should be
 * more important than profiles generated from source data.
 *
 * Return value: A number which corresponds to the importance of the profile,
 * where larger numbers have more importance than lower numbers.
 **/
guint
cd_profile_get_score (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), 0);
	return profile->priv->score;
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
	profile->priv->db = cd_profile_db_new ();
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
	g_object_unref (priv->db);
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

