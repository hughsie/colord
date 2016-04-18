/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2012 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:cd-profile
 * @short_description: Client object for accessing information about colord profiles
 *
 * A helper GObject to use for accessing colord profiles, and to be notified
 * when it is changed.
 *
 * See also: #CdClient
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>

#include "cd-profile.h"

static void	cd_profile_class_init	(CdProfileClass	*klass);
static void	cd_profile_init		(CdProfile	*profile);
static void	cd_profile_finalize	(GObject	*object);

#define GET_PRIVATE(o) (cd_profile_get_instance_private (o))

#define COLORD_DBUS_SERVICE		"org.freedesktop.ColorManager"
#define COLORD_DBUS_INTERFACE_PROFILE	"org.freedesktop.ColorManager.Profile"

/**
 * CdProfilePrivate:
 *
 * Private #CdProfile data
 **/
typedef struct
{
	gchar			*filename;
	gchar			*id;
	gchar			*object_path;
	gchar			*qualifier;
	gchar			*format;
	gchar			*title;
	GDBusProxy		*proxy;
	CdProfileKind		 kind;
	CdColorspace		 colorspace;
	CdObjectScope		 scope;
	gint64			 created;
	gboolean		 has_vcgt;
	gboolean		 is_system_wide;
	guint			 owner;
	gchar			**warnings;
	GHashTable		*metadata;
} CdProfilePrivate;

enum {
	PROP_0,
	PROP_OBJECT_PATH,
	PROP_CONNECTED,
	PROP_ID,
	PROP_FILENAME,
	PROP_QUALIFIER,
	PROP_FORMAT,
	PROP_TITLE,
	PROP_KIND,
	PROP_COLORSPACE,
	PROP_CREATED,
	PROP_HAS_VCGT,
	PROP_IS_SYSTEM_WIDE,
	PROP_SCOPE,
	PROP_OWNER,
	PROP_WARNINGS,
	PROP_LAST
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (CdProfile, cd_profile, G_TYPE_OBJECT)

/**
 * cd_profile_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.0
 **/
GQuark
cd_profile_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("cd_profile_error");
	return quark;
}

/**
 * cd_profile_set_object_path:
 * @profile: a #CdProfile instance.
 * @object_path: The colord object path.
 *
 * Sets the object path of the profile.
 *
 * Since: 0.1.8
 **/
void
cd_profile_set_object_path (CdProfile *profile, const gchar *object_path)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_if_fail (CD_IS_PROFILE (profile));
	g_return_if_fail (priv->object_path == NULL);
	priv->object_path = g_strdup (object_path);
}

/**
 * cd_profile_get_id:
 * @profile: a #CdProfile instance.
 *
 * Gets the profile ID.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.0
 **/
const gchar *
cd_profile_get_id (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->id;
}

/**
 * cd_profile_get_filename:
 * @profile: a #CdProfile instance.
 *
 * Gets the profile filename.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.0
 **/
const gchar *
cd_profile_get_filename (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->filename;
}

/**
 * cd_profile_has_access:
 * @profile: a #CdProfile instance.
 *
 * Gets if the current user has access permissions to the profile.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.13
 **/
gboolean
cd_profile_has_access (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);

	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (priv->proxy != NULL, FALSE);

	/* virtual profile */
	if (priv->filename == NULL)
		return TRUE;

	/* profile on disk */
	return g_access (priv->filename, R_OK) == 0;
}

/**
 * cd_profile_get_qualifier:
 * @profile: a #CdProfile instance.
 *
 * Gets the profile qualifier.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.0
 **/
const gchar *
cd_profile_get_qualifier (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->qualifier;
}

/**
 * cd_profile_get_format:
 * @profile: a #CdProfile instance.
 *
 * Gets the profile format.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.4
 **/
const gchar *
cd_profile_get_format (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->format;
}

/**
 * cd_profile_get_title:
 * @profile: a #CdProfile instance.
 *
 * Gets the profile title.
 *
 * Return value: A string, or %NULL for invalid
 *
 * Since: 0.1.0
 **/
const gchar *
cd_profile_get_title (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->title;
}

/**
 * cd_profile_get_kind:
 * @profile: a #CdProfile instance.
 *
 * Gets the profile kind.
 *
 * Return value: A #CdProfileKind, e.g. %CD_PROFILE_KIND_DISPLAY_DEVICE
 *
 * Since: 0.1.1
 **/
CdProfileKind
cd_profile_get_kind (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), CD_PROFILE_KIND_UNKNOWN);
	g_return_val_if_fail (priv->proxy != NULL, CD_PROFILE_KIND_UNKNOWN);
	return priv->kind;
}

/**
 * cd_profile_get_scope:
 * @profile: a #CdProfile instance.
 *
 * Gets the profile scope.
 *
 * Return value: A #CdObjectScope, e.g. %CD_OBJECT_SCOPE_UNKNOWN
 *
 * Since: 0.1.10
 **/
CdObjectScope
cd_profile_get_scope (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), CD_OBJECT_SCOPE_UNKNOWN);
	g_return_val_if_fail (priv->proxy != NULL, CD_OBJECT_SCOPE_UNKNOWN);
	return priv->scope;
}

/**
 * cd_profile_get_owner:
 * @profile: a #CdProfile instance.
 *
 * Gets the profile owner.
 *
 * Return value: The UID of the user that created the device
 *
 * Since: 0.1.13
 **/
guint
cd_profile_get_owner (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), G_MAXUINT);
	g_return_val_if_fail (priv->proxy != NULL, G_MAXUINT);
	return priv->owner;
}

/**
 * cd_profile_get_warnings:
 * @profile: a #CdProfile instance.
 *
 * Gets the profile warnings as a string array.
 *
 * Return value: (transfer none): Any profile warnings, e.g. "vcgt-non-monotonic"
 *
 * Since: 0.1.25
 **/
gchar **
cd_profile_get_warnings (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return priv->warnings;
}

/**
 * cd_profile_get_created:
 * @profile: a #CdProfile instance.
 *
 * Gets the profile created date and time.
 *
 * Return value: A UNIX time, or 0 if the profile has no creation date
 *
 * Since: 0.1.8
 **/
gint64
cd_profile_get_created (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), 0);
	g_return_val_if_fail (priv->proxy != NULL, 0);
	return priv->created;
}

/**
 * cd_profile_get_age:
 * @profile: a #CdProfile instance.
 *
 * Gets the profile age in seconds relative to the current time.
 *
 * Return value: A UNIX time, or 0 if the profile has no creation date
 *
 * Since: 0.1.8
 **/
gint64
cd_profile_get_age (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);

	g_return_val_if_fail (CD_IS_PROFILE (profile), 0);
	g_return_val_if_fail (priv->proxy != NULL, 0);

	if (priv->created == 0)
		return 0;
	return (g_get_real_time () / G_USEC_PER_SEC) - priv->created;
}

/**
 * cd_profile_get_colorspace:
 * @profile: a #CdProfile instance.
 *
 * Gets the profile colorspace.
 *
 * Return value: A #CdColorspace, e.g. %CD_COLORSPACE_RGB
 *
 * Since: 0.1.2
 **/
CdColorspace
cd_profile_get_colorspace (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), CD_COLORSPACE_UNKNOWN);
	g_return_val_if_fail (priv->proxy != NULL, CD_COLORSPACE_UNKNOWN);
	return priv->colorspace;
}

/**
 * cd_profile_get_has_vcgt:
 * @profile: a #CdProfile instance.
 *
 * Returns if the profile has a VCGT table.
 *
 * Return value: %TRUE if VCGT is valid.
 *
 * Since: 0.1.2
 **/
gboolean
cd_profile_get_has_vcgt (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (priv->proxy != NULL, FALSE);
	return priv->has_vcgt;
}

/**
 * cd_profile_get_is_system_wide:
 * @profile: a #CdProfile instance.
 *
 * Returns if the profile is installed system wide and available for all
 * users.
 *
 * Return value: %TRUE if system wide.
 *
 * Since: 0.1.2
 **/
gboolean
cd_profile_get_is_system_wide (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (priv->proxy != NULL, FALSE);
	return priv->is_system_wide;
}

/**
 * cd_profile_get_metadata:
 * @profile: a #CdProfile instance.
 *
 * Returns the profile metadata.
 *
 * Return value: (transfer container) (element-type utf8 utf8): a
 *               #GHashTable.
 *
 * Since: 0.1.2
 **/
GHashTable *
cd_profile_get_metadata (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return g_hash_table_ref (priv->metadata);
}

/**
 * cd_profile_get_metadata_item:
 * @profile: a #CdProfile instance.
 * @key: a key for the metadata dictionary
 *
 * Returns the profile metadata for a specific key.
 *
 * Return value: the metadata value, or %NULL if not set.
 *
 * Since: 0.1.5
 **/
const gchar *
cd_profile_get_metadata_item (CdProfile *profile, const gchar *key)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (priv->proxy != NULL, NULL);
	return g_hash_table_lookup (priv->metadata, key);
}

/**
 * cd_profile_set_metadata_from_variant:
 **/
static void
cd_profile_set_metadata_from_variant (CdProfile *profile, GVariant *variant)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	GVariantIter iter;
	const gchar *prop_key;
	const gchar *prop_value;

	/* remove old entries */
	g_hash_table_remove_all (priv->metadata);

	/* insert the new metadata */
	g_variant_iter_init (&iter, variant);
	while (g_variant_iter_loop (&iter, "{ss}",
				    &prop_key, &prop_value)) {
		g_hash_table_insert (priv->metadata,
				     g_strdup (prop_key),
				     g_strdup (prop_value));

	}
}

/**
 * cd_profile_get_nullable_str:
 *
 * We can't get nullable types from a GVariant yet. Work around...
 **/
static gchar *
cd_profile_get_nullable_str (GVariant *value)
{
	const gchar *tmp;
	tmp = g_variant_get_string (value, NULL);
	if (tmp == NULL)
		return NULL;
	if (tmp[0] == '\0')
		return NULL;
	return g_strdup (tmp);
}

/**
 * cd_profile_dbus_properties_changed_cb:
 **/
static void
cd_profile_dbus_properties_changed_cb (GDBusProxy  *proxy,
				    GVariant    *changed_properties,
				    const gchar * const *invalidated_properties,
				    CdProfile   *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	guint i;
	guint len;
	GVariantIter iter;
	gchar *property_name;
	GVariant *property_value;

	g_return_if_fail (CD_IS_PROFILE (profile));

	len = g_variant_iter_init (&iter, changed_properties);
	for (i = 0; i < len; i++) {
		g_variant_get_child (changed_properties, i,
				     "{sv}",
				     &property_name,
				     &property_value);
		if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_QUALIFIER) == 0) {
			g_free (priv->qualifier);
			priv->qualifier = cd_profile_get_nullable_str (property_value);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_FORMAT) == 0) {
			g_free (priv->format);
			priv->format = cd_profile_get_nullable_str (property_value);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_FILENAME) == 0) {
			g_free (priv->filename);
			priv->filename = cd_profile_get_nullable_str (property_value);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_ID) == 0) {
			g_free (priv->id);
			priv->id = g_variant_dup_string (property_value, NULL);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_TITLE) == 0) {
			g_free (priv->title);
			priv->title = g_variant_dup_string (property_value, NULL);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_WARNINGS) == 0) {
			g_strfreev(priv->warnings);
			priv->warnings = g_variant_dup_strv (property_value, NULL);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_KIND) == 0) {
			priv->kind = cd_profile_kind_from_string (g_variant_get_string (property_value, NULL));
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_COLORSPACE) == 0) {
			priv->colorspace = cd_colorspace_from_string (g_variant_get_string (property_value, NULL));
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_SCOPE) == 0) {
			priv->scope = cd_object_scope_from_string (g_variant_get_string (property_value, NULL));
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_CREATED) == 0) {
			priv->created = g_variant_get_int64 (property_value);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_HAS_VCGT) == 0) {
			priv->has_vcgt = g_variant_get_boolean (property_value);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_OWNER) == 0) {
			priv->owner = g_variant_get_uint32 (property_value);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_IS_SYSTEM_WIDE) == 0) {
			priv->is_system_wide = g_variant_get_boolean (property_value);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_METADATA) == 0) {
			cd_profile_set_metadata_from_variant (profile, property_value);
		} else {
			g_warning ("%s property unhandled", property_name);
		}
		g_free (property_name);
		g_variant_unref (property_value);
	}
}

/**
 * cd_profile_dbus_signal_cb:
 **/
static void
cd_profile_dbus_signal_cb (GDBusProxy *proxy,
			   gchar      *sender_name,
			   gchar      *signal_name,
			   GVariant   *parameters,
			   CdProfile   *profile)
{
	gchar *object_path_tmp = NULL;

	g_return_if_fail (CD_IS_PROFILE (profile));

	if (g_strcmp0 (signal_name, "Changed") == 0) {
		g_signal_emit (profile, signals[SIGNAL_CHANGED], 0);
	} else {
		g_warning ("unhandled signal '%s'", signal_name);
	}
	g_free (object_path_tmp);
}

/**********************************************************************/

/**
 * cd_profile_connect_finish:
 * @profile: a #CdProfile instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: success
 *
 * Since: 0.1.8
 **/
gboolean
cd_profile_connect_finish (CdProfile *profile,
			   GAsyncResult *res,
			   GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, profile), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

/**
 * cd_profile_fixup_dbus_error:
 **/
static void
cd_profile_fixup_dbus_error (GError *error)
{
	g_autofree gchar *name = NULL;

	g_return_if_fail (error != NULL);

	/* is a remote error? */
	if (!g_dbus_error_is_remote_error (error))
		return;

	/* parse the remote error */
	name = g_dbus_error_get_remote_error (error);
	error->domain = CD_PROFILE_ERROR;
	error->code = cd_profile_error_from_string (name);
	g_dbus_error_strip_remote_error (error);
}

static void
cd_profile_connect_cb (GObject *source_object,
		       GAsyncResult *res,
		       gpointer user_data)
{
	CdProfile *profile;
	CdProfilePrivate *priv;
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) colorspace = NULL;
	g_autoptr(GVariant) created = NULL;
	g_autoptr(GVariant) filename = NULL;
	g_autoptr(GVariant) format = NULL;
	g_autoptr(GVariant) has_vcgt = NULL;
	g_autoptr(GVariant) id = NULL;
	g_autoptr(GVariant) is_system_wide = NULL;
	g_autoptr(GVariant) kind = NULL;
	g_autoptr(GVariant) metadata = NULL;
	g_autoptr(GVariant) owner = NULL;
	g_autoptr(GVariant) qualifier = NULL;
	g_autoptr(GVariant) scope = NULL;
	g_autoptr(GVariant) title = NULL;
	g_autoptr(GVariant) warnings = NULL;

	profile = CD_PROFILE (g_task_get_source_object (task));
	priv = GET_PRIVATE (profile);
	priv->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (priv->proxy == NULL) {
		g_task_return_new_error (task,
						 CD_PROFILE_ERROR,
						 CD_PROFILE_ERROR_INTERNAL,
						 "Failed to connect to profile %s: %s",
						 cd_profile_get_object_path (profile),
						 error->message);
		return;
	}

	/* get profile id */
	id = g_dbus_proxy_get_cached_property (priv->proxy,
					       CD_PROFILE_PROPERTY_ID);
	if (id != NULL)
		priv->id = g_variant_dup_string (id, NULL);

	/* if the profile is missing, then fail */
	if (id == NULL) {
		g_task_return_new_error (task,
						 CD_PROFILE_ERROR,
						 CD_PROFILE_ERROR_INTERNAL,
						 "Failed to connect to missing profile %s",
						 cd_profile_get_object_path (profile));
		return;
	}

	/* get filename */
	filename = g_dbus_proxy_get_cached_property (priv->proxy,
						     CD_PROFILE_PROPERTY_FILENAME);
	if (filename != NULL)
		priv->filename = cd_profile_get_nullable_str (filename);

	/* get qualifier */
	qualifier = g_dbus_proxy_get_cached_property (priv->proxy,
						      CD_PROFILE_PROPERTY_QUALIFIER);
	if (qualifier != NULL)
		priv->qualifier = cd_profile_get_nullable_str (qualifier);

	/* get format */
	format = g_dbus_proxy_get_cached_property (priv->proxy,
						   CD_PROFILE_PROPERTY_FORMAT);
	if (format != NULL)
		priv->format = cd_profile_get_nullable_str (format);

	/* get title */
	title = g_dbus_proxy_get_cached_property (priv->proxy,
						  CD_PROFILE_PROPERTY_TITLE);
	if (title != NULL)
		priv->title = cd_profile_get_nullable_str (title);

	/* get kind */
	kind = g_dbus_proxy_get_cached_property (priv->proxy,
						 CD_PROFILE_PROPERTY_KIND);
	if (kind != NULL)
		priv->kind = cd_profile_kind_from_string (g_variant_get_string (kind, NULL));

	/* get colorspace */
	colorspace = g_dbus_proxy_get_cached_property (priv->proxy,
						       CD_PROFILE_PROPERTY_COLORSPACE);
	if (colorspace != NULL)
		priv->colorspace = cd_colorspace_from_string (g_variant_get_string (colorspace, NULL));

	/* get scope */
	scope = g_dbus_proxy_get_cached_property (priv->proxy,
						  CD_PROFILE_PROPERTY_SCOPE);
	if (scope != NULL)
		priv->scope = cd_object_scope_from_string (g_variant_get_string (scope, NULL));

	/* get owner */
	owner = g_dbus_proxy_get_cached_property (priv->proxy,
						  CD_PROFILE_PROPERTY_OWNER);
	if (owner != NULL)
		priv->owner = g_variant_get_uint32 (owner);

	/* get warnings */
	warnings = g_dbus_proxy_get_cached_property (priv->proxy,
						  CD_PROFILE_PROPERTY_WARNINGS);
	if (warnings != NULL)
		priv->warnings = g_variant_dup_strv (warnings, NULL);

	/* get created */
	created = g_dbus_proxy_get_cached_property (priv->proxy,
						    CD_PROFILE_PROPERTY_CREATED);
	if (created != NULL)
		priv->created = g_variant_get_int64 (created);

	/* get VCGT */
	has_vcgt = g_dbus_proxy_get_cached_property (priv->proxy,
						     CD_PROFILE_PROPERTY_HAS_VCGT);
	if (has_vcgt != NULL)
		priv->has_vcgt = g_variant_get_boolean (has_vcgt);

	/* get if system wide */
	is_system_wide = g_dbus_proxy_get_cached_property (priv->proxy,
							   CD_PROFILE_PROPERTY_IS_SYSTEM_WIDE);
	if (is_system_wide != NULL)
		priv->is_system_wide = g_variant_get_boolean (is_system_wide);

	/* get if system wide */
	metadata = g_dbus_proxy_get_cached_property (priv->proxy,
						     CD_PROFILE_PROPERTY_METADATA);
	if (metadata != NULL)
		cd_profile_set_metadata_from_variant (profile, metadata);

	/* get signals from DBus */
	g_signal_connect_object (priv->proxy,
				 "g-signal",
				 G_CALLBACK (cd_profile_dbus_signal_cb),
				 profile, 0);

	/* watch if any remote properties change */
	g_signal_connect_object (priv->proxy,
				 "g-properties-changed",
				 G_CALLBACK (cd_profile_dbus_properties_changed_cb),
				 profile, 0);

	/* success */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_profile_connect:
 * @profile: a #CdProfile instance.
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Connects to the object and fills up initial properties.
 *
 * Since: 0.1.8
 **/
void
cd_profile_connect (CdProfile *profile,
		    GCancellable *cancellable,
		    GAsyncReadyCallback callback,
		    gpointer user_data)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_PROFILE (profile));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (profile, cancellable, callback, user_data);

	/* already connected */
	if (priv->proxy != NULL) {
		g_task_return_boolean (task, TRUE);
		return;
	}

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  COLORD_DBUS_SERVICE,
				  priv->object_path,
				  COLORD_DBUS_INTERFACE_PROFILE,
				  cancellable,
				  cd_profile_connect_cb,
				  task);
}

/**********************************************************************/

/**
 * cd_profile_set_property_finish:
 * @profile: a #CdProfile instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: success
 *
 * Since: 0.1.8
 **/
gboolean
cd_profile_set_property_finish (CdProfile *profile,
				GAsyncResult *res,
				GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, profile), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_profile_set_property_cb (GObject *source_object,
			    GAsyncResult *res,
			    gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_task_return_new_error (task,
						 CD_PROFILE_ERROR,
						 CD_PROFILE_ERROR_INTERNAL,
						 "Failed to SetProperty: %s",
						 error->message);
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_profile_set_property:
 * @profile: a #CdProfile instance.
 * @key: a key name
 * @value: a key value
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Deletes a color device.
 *
 * Since: 0.1.8
 **/
void
cd_profile_set_property (CdProfile *profile,
			 const gchar *key,
			 const gchar *value,
			 GCancellable *cancellable,
			 GAsyncReadyCallback callback,
			 gpointer user_data)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_PROFILE (profile));
	g_return_if_fail (key != NULL);
	g_return_if_fail (value != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (profile, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "SetProperty",
			   g_variant_new ("(ss)",
			   		  key,
			   		  value),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_profile_set_property_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_profile_install_system_wide_finish:
 * @profile: a #CdProfile instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: success
 *
 * Since: 0.1.8
 **/
gboolean
cd_profile_install_system_wide_finish (CdProfile *profile,
				       GAsyncResult *res,
				       GError **error)
{
	g_return_val_if_fail (g_task_is_valid (res, profile), FALSE);
	return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cd_profile_install_system_wide_cb (GObject *source_object,
				   GAsyncResult *res,
				   gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GTask) task = G_TASK (user_data);
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		cd_profile_fixup_dbus_error (error);
		g_task_return_error (task, error);
		error = NULL;
		return;
	}

	/* success */
	g_task_return_boolean (task, TRUE);
}

/**
 * cd_profile_install_system_wide:
 * @profile: a #CdProfile instance.
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Sets the profile system wide.
 *
 * Since: 0.1.8
 **/
void
cd_profile_install_system_wide (CdProfile *profile,
				GCancellable *cancellable,
				GAsyncReadyCallback callback,
				gpointer user_data)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	GTask *task = NULL;

	g_return_if_fail (CD_IS_PROFILE (profile));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (priv->proxy != NULL);

	task = g_task_new (profile, cancellable, callback, user_data);
	g_dbus_proxy_call (priv->proxy,
			   "InstallSystemWide",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_profile_install_system_wide_cb,
			   task);
}

/**********************************************************************/

/**
 * cd_profile_get_object_path:
 * @profile: a #CdProfile instance.
 *
 * Gets the object path for the profile.
 *
 * Return value: the object path, or %NULL
 *
 * Since: 0.1.0
 **/
const gchar *
cd_profile_get_object_path (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return priv->object_path;
}

/**
 * cd_profile_get_connected:
 * @profile: a #CdProfile instance.
 *
 * Gets if the profile has been connected.
 *
 * Return value: %TRUE if properties are valid
 *
 * Since: 0.1.9
 **/
gboolean
cd_profile_get_connected (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	return priv->proxy != NULL;
}

/**
 * cd_profile_to_string:
 * @profile: a #CdProfile instance.
 *
 * Converts the profile to a string description.
 *
 * Return value: text representation of #CdProfile
 *
 * Since: 0.1.0
 **/
gchar *
cd_profile_to_string (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	GString *string;

	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);

	string = g_string_new ("");
	g_string_append_printf (string, "  object-path:          %s\n",
				priv->object_path);

	return g_string_free (string, FALSE);
}

/**
 * cd_profile_equal:
 * @profile1: one #CdProfile instance.
 * @profile2: another #CdProfile instance.
 *
 * Tests two profiles for equality.
 *
 * Return value: %TRUE if the profiles are the same device
 *
 * Since: 0.1.8
 **/
gboolean
cd_profile_equal (CdProfile *profile1, CdProfile *profile2)
{
	CdProfilePrivate *priv1 = GET_PRIVATE (profile1);
	CdProfilePrivate *priv2 = GET_PRIVATE (profile2);
	g_return_val_if_fail (CD_IS_PROFILE (profile1), FALSE);
	g_return_val_if_fail (CD_IS_PROFILE (profile2), FALSE);
	if (priv1->id == NULL || priv2->id == NULL)
		g_critical ("need to connect");
	return g_strcmp0 (priv1->id, priv2->id) == 0;
}

/**
 * cd_profile_load_icc:
 * @profile: a #CdProfile instance.
 * @flags: options for loading the profile
 * @cancellable: A #GCancellable, or %NULL
 * @error: A #GError or %NULL
 *
 * Loads a local ICC object from the abstract profile.
 *
 * Return value: (transfer full): A new #CdIcc object, or %NULL for error
 *
 * Since: 0.1.32
 **/
CdIcc *
cd_profile_load_icc (CdProfile *profile,
		     CdIccLoadFlags flags,
		     GCancellable *cancellable,
		     GError **error)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	g_autoptr(CdIcc) icc = NULL;
	g_autoptr(GFile) file = NULL;

	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);

	/* not a local profile */
	if (priv->filename == NULL) {
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_INTERNAL,
			     "%s has no local instance",
			     priv->id);
		return NULL;
	}

	/* load local instance */
	icc = cd_icc_new ();
	file = g_file_new_for_path (priv->filename);
	if (!cd_icc_load_file (icc, file, flags, cancellable, error))
		return NULL;

	/* success */
	return g_object_ref (icc);
}

/*
 * _cd_profile_set_property:
 */
static void
_cd_profile_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CdProfile *profile = CD_PROFILE (object);
	CdProfilePrivate *priv = GET_PRIVATE (profile);

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_free (priv->object_path);
		priv->object_path = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/*
 * cd_profile_get_property:
 */
static void
cd_profile_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	CdProfile *profile = CD_PROFILE (object);
	CdProfilePrivate *priv = GET_PRIVATE (profile);

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_value_set_string (value, priv->object_path);
		break;
	case PROP_CONNECTED:
		g_value_set_boolean (value, priv->proxy != NULL);
		break;
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_FILENAME:
		g_value_set_string (value, priv->filename);
		break;
	case PROP_QUALIFIER:
		g_value_set_string (value, priv->qualifier);
		break;
	case PROP_FORMAT:
		g_value_set_string (value, priv->format);
		break;
	case PROP_TITLE:
		g_value_set_string (value, priv->title);
		break;
	case PROP_KIND:
		g_value_set_uint (value, priv->kind);
		break;
	case PROP_COLORSPACE:
		g_value_set_uint (value, priv->colorspace);
		break;
	case PROP_CREATED:
		g_value_set_int64 (value, priv->created);
		break;
	case PROP_HAS_VCGT:
		g_value_set_boolean (value, priv->has_vcgt);
		break;
	case PROP_IS_SYSTEM_WIDE:
		g_value_set_boolean (value, priv->is_system_wide);
		break;
	case PROP_SCOPE:
		g_value_set_uint (value, priv->scope);
		break;
	case PROP_OWNER:
		g_value_set_uint (value, priv->owner);
		break;
	case PROP_WARNINGS:
		g_value_set_boxed (value, priv->warnings);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
    }
}

/*
 * cd_profile_class_init:
 */
static void
cd_profile_class_init (CdProfileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_profile_finalize;
	object_class->set_property = _cd_profile_set_property;
	object_class->get_property = cd_profile_get_property;

	/**
	 * CdProfile::changed:
	 * @profile: the #CdProfile instance that emitted the signal
	 *
	 * The ::changed signal is emitted when the profile data has changed.
	 *
	 * Since: 0.1.0
	 **/
	signals [SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdProfileClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/**
	 * CdProfile:object-path:
	 *
	 * The object path of the remote object
	 *
	 * Since: 0.1.8
	 **/
	g_object_class_install_property (object_class,
					 PROP_OBJECT_PATH,
					 g_param_spec_string ("object-path",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * CdProfile:connected:
	 *
	 * The if the object path has been connected as is valid for use.
	 *
	 * Since: 0.1.9
	 **/
	g_object_class_install_property (object_class,
					 PROP_CONNECTED,
					 g_param_spec_string ("connected",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
	/**
	 * CdProfile:id:
	 *
	 * The profile ID.
	 *
	 * Since: 0.1.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_string ("id",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
	/**
	 * CdProfile:filename:
	 *
	 * The profile filename.
	 *
	 * Since: 0.1.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_FILENAME,
					 g_param_spec_string ("filename",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
	/**
	 * CdProfile:qualifier:
	 *
	 * The profile qualifier.
	 *
	 * Since: 0.1.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_QUALIFIER,
					 g_param_spec_string ("qualifier",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdProfile:format:
	 *
	 * The profile format.
	 *
	 * Since: 0.1.4
	 **/
	g_object_class_install_property (object_class,
					 PROP_FORMAT,
					 g_param_spec_string ("format",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdProfile:title:
	 *
	 * The profile title.
	 *
	 * Since: 0.1.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_TITLE,
					 g_param_spec_string ("title",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
	/**
	 * CdProfile:kind:
	 *
	 * The profile kind.
	 *
	 * Since: 0.1.1
	 **/
	g_object_class_install_property (object_class,
					 PROP_KIND,
					 g_param_spec_string ("kind",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdProfile:colorspace:
	 *
	 * The profile colorspace.
	 *
	 * Since: 0.1.2
	 **/
	g_object_class_install_property (object_class,
					 PROP_COLORSPACE,
					 g_param_spec_string ("colorspace",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdProfile:created:
	 *
	 * When the profile was created.
	 *
	 * Since: 0.1.8
	 **/
	g_object_class_install_property (object_class,
					 PROP_CREATED,
					 g_param_spec_int64 ("created",
							     NULL, NULL,
							     0, G_MAXINT64,
							     0,
							     G_PARAM_READABLE));

	/**
	 * CdProfile:has-vcgt:
	 *
	 * If the profile has a VCGT table.
	 *
	 * Since: 0.1.2
	 **/
	g_object_class_install_property (object_class,
					 PROP_HAS_VCGT,
					 g_param_spec_string ("has-vcgt",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdProfile:is-system-wide:
	 *
	 * If the profile is installed system wide for all users.
	 *
	 * Since: 0.1.2
	 **/
	g_object_class_install_property (object_class,
					 PROP_IS_SYSTEM_WIDE,
					 g_param_spec_string ("is-system-wide",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdProfile:scope:
	 *
	 * The profile scope, e.g. %CD_OBJECT_SCOPE_TEMP.
	 *
	 * Since: 0.1.10
	 **/
	g_object_class_install_property (object_class,
					 PROP_SCOPE,
					 g_param_spec_uint ("scope",
							    NULL, NULL,
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READABLE));

	/**
	 * CdProfile:owner:
	 *
	 * The profile owner, e.g. %500.
	 *
	 * Since: 0.1.13
	 **/
	g_object_class_install_property (object_class,
					 PROP_OWNER,
					 g_param_spec_uint ("owner",
							    NULL, NULL,
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READABLE));

	/**
	 * CdProfile:warnings:
	 *
	 * The profile warnings, e.g. "vcgt-non-monotonic".
	 *
	 * Since: 0.1.25
	 **/
	g_object_class_install_property (object_class,
					 PROP_WARNINGS,
					 g_param_spec_boxed ("warnings",
							     NULL, NULL,
							     G_TYPE_STRV,
							     G_PARAM_READABLE));
}

/*
 * cd_profile_init:
 */
static void
cd_profile_init (CdProfile *profile)
{
	CdProfilePrivate *priv = GET_PRIVATE (profile);
	priv->metadata = g_hash_table_new_full (g_str_hash,
							 g_str_equal,
							 g_free,
							 g_free);
}

/*
 * cd_profile_finalize:
 */
static void
cd_profile_finalize (GObject *object)
{
	CdProfile *profile = CD_PROFILE (object);
	CdProfilePrivate *priv = GET_PRIVATE (profile);

	g_return_if_fail (CD_IS_PROFILE (object));

	g_hash_table_unref (priv->metadata);
	g_free (priv->object_path);
	g_free (priv->id);
	g_free (priv->filename);
	g_free (priv->qualifier);
	g_free (priv->format);
	g_free (priv->title);
	g_strfreev (priv->warnings);
	if (priv->proxy != NULL)
		g_object_unref (priv->proxy);

	G_OBJECT_CLASS (cd_profile_parent_class)->finalize (object);
}

/**
 * cd_profile_new:
 *
 * Creates a new #CdProfile object.
 *
 * Return value: a new CdProfile object.
 *
 * Since: 0.1.0
 **/
CdProfile *
cd_profile_new (void)
{
	CdProfile *profile;
	profile = g_object_new (CD_TYPE_PROFILE, NULL);
	return CD_PROFILE (profile);
}

/**
 * cd_profile_new_with_object_path:
 * @object_path: The colord object path.
 *
 * Creates a new #CdProfile object with a known object path.
 *
 * Return value: a new profile object.
 *
 * Since: 0.1.8
 **/
CdProfile *
cd_profile_new_with_object_path (const gchar *object_path)
{
	CdProfile *profile;
	profile = g_object_new (CD_TYPE_PROFILE,
				"object-path", object_path,
				NULL);
	return CD_PROFILE (profile);
}
