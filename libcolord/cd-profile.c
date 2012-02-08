/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

#define CD_PROFILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_PROFILE, CdProfilePrivate))

#define COLORD_DBUS_SERVICE		"org.freedesktop.ColorManager"
#define COLORD_DBUS_INTERFACE_PROFILE	"org.freedesktop.ColorManager.Profile"

/**
 * CdProfilePrivate:
 *
 * Private #CdProfile data
 **/
struct _CdProfilePrivate
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
	GHashTable		*metadata;
};

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
	PROP_LAST
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (CdProfile, cd_profile, G_TYPE_OBJECT)

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
	g_return_if_fail (CD_IS_PROFILE (profile));
	g_return_if_fail (profile->priv->object_path == NULL);
	profile->priv->object_path = g_strdup (object_path);
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (profile->priv->proxy != NULL, NULL);
	return profile->priv->id;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (profile->priv->proxy != NULL, NULL);
	return profile->priv->filename;
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
	gboolean ret = TRUE;

	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (profile->priv->proxy != NULL, FALSE);

	/* virtual profile */
	if (profile->priv->filename == NULL)
		goto out;

	/* profile on disk */
	ret = g_access (profile->priv->filename, R_OK) == 0;
out:
	return ret;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (profile->priv->proxy != NULL, NULL);
	return profile->priv->qualifier;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (profile->priv->proxy != NULL, NULL);
	return profile->priv->format;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (profile->priv->proxy != NULL, NULL);
	return profile->priv->title;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), CD_PROFILE_KIND_UNKNOWN);
	g_return_val_if_fail (profile->priv->proxy != NULL, CD_PROFILE_KIND_UNKNOWN);
	return profile->priv->kind;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), CD_OBJECT_SCOPE_UNKNOWN);
	g_return_val_if_fail (profile->priv->proxy != NULL, CD_OBJECT_SCOPE_UNKNOWN);
	return profile->priv->scope;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), G_MAXUINT);
	g_return_val_if_fail (profile->priv->proxy != NULL, G_MAXUINT);
	return profile->priv->owner;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), 0);
	g_return_val_if_fail (profile->priv->proxy != NULL, 0);
	return profile->priv->created;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), 0);
	g_return_val_if_fail (profile->priv->proxy != NULL, 0);

	if (profile->priv->created == 0)
		return 0;
	return (g_get_real_time () / G_USEC_PER_SEC) - profile->priv->created;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), CD_COLORSPACE_UNKNOWN);
	g_return_val_if_fail (profile->priv->proxy != NULL, CD_COLORSPACE_UNKNOWN);
	return profile->priv->colorspace;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (profile->priv->proxy != NULL, FALSE);
	return profile->priv->has_vcgt;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (profile->priv->proxy != NULL, FALSE);
	return profile->priv->is_system_wide;
}

/**
 * cd_profile_get_metadata:
 * @profile: a #CdProfile instance.
 *
 * Returns the profile metadata.
 *
 * Return value: (transfer full): a #GHashTable.
 *
 * Since: 0.1.2
 **/
GHashTable *
cd_profile_get_metadata (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (profile->priv->proxy != NULL, NULL);
	return g_hash_table_ref (profile->priv->metadata);
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (profile->priv->proxy != NULL, NULL);
	return g_hash_table_lookup (profile->priv->metadata, key);
}

/**
 * cd_profile_set_metadata_from_variant:
 **/
static void
cd_profile_set_metadata_from_variant (CdProfile *profile, GVariant *variant)
{
	GVariantIter iter;
	const gchar *prop_key;
	const gchar *prop_value;

	/* remove old entries */
	g_hash_table_remove_all (profile->priv->metadata);

	/* insert the new metadata */
	g_variant_iter_init (&iter, variant);
	while (g_variant_iter_loop (&iter, "{ss}",
				    &prop_key, &prop_value)) {
		g_hash_table_insert (profile->priv->metadata,
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
	guint i;
	guint len;
	GVariantIter iter;
	gchar *property_name;
	GVariant *property_value;

	g_return_if_fail (CD_IS_PROFILE (profile));

	len = g_variant_iter_init (&iter, changed_properties);
	for (i=0; i < len; i++) {
		g_variant_get_child (changed_properties, i,
				     "{sv}",
				     &property_name,
				     &property_value);
		if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_QUALIFIER) == 0) {
			g_free (profile->priv->qualifier);
			profile->priv->qualifier = cd_profile_get_nullable_str (property_value);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_FORMAT) == 0) {
			g_free (profile->priv->format);
			profile->priv->format = cd_profile_get_nullable_str (property_value);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_FILENAME) == 0) {
			g_free (profile->priv->filename);
			profile->priv->filename = cd_profile_get_nullable_str (property_value);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_ID) == 0) {
			g_free (profile->priv->id);
			profile->priv->id = g_variant_dup_string (property_value, NULL);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_TITLE) == 0) {
			g_free (profile->priv->title);
			profile->priv->title = g_variant_dup_string (property_value, NULL);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_KIND) == 0) {
			profile->priv->kind = cd_profile_kind_from_string (g_variant_get_string (property_value, NULL));
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_COLORSPACE) == 0) {
			profile->priv->colorspace = cd_colorspace_from_string (g_variant_get_string (property_value, NULL));
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_SCOPE) == 0) {
			profile->priv->scope = cd_object_scope_from_string (g_variant_get_string (property_value, NULL));
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_CREATED) == 0) {
			profile->priv->created = g_variant_get_int64 (property_value);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_HAS_VCGT) == 0) {
			profile->priv->has_vcgt = g_variant_get_boolean (property_value);
		} else if (g_strcmp0 (property_name, CD_PROFILE_PROPERTY_IS_SYSTEM_WIDE) == 0) {
			profile->priv->is_system_wide = g_variant_get_boolean (property_value);
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
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_simple_async_result_get_op_res_gboolean (simple);
}

static void
cd_profile_connect_cb (GObject *source_object,
		       GAsyncResult *res,
		       gpointer user_data)
{
	GError *error = NULL;
	GVariant *filename = NULL;
	GVariant *id = NULL;
	GVariant *profiles = NULL;
	GVariant *qualifier = NULL;
	GVariant *format = NULL;
	GVariant *title = NULL;
	GVariant *kind = NULL;
	GVariant *colorspace = NULL;
	GVariant *scope = NULL;
	GVariant *owner = NULL;
	GVariant *created = NULL;
	GVariant *has_vcgt = NULL;
	GVariant *is_system_wide = NULL;
	GVariant *metadata = NULL;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);
	CdProfile *profile;

	profile = CD_PROFILE (g_async_result_get_source_object (G_ASYNC_RESULT (user_data)));
	profile->priv->proxy = g_dbus_proxy_new_for_bus_finish (res,
								&error);
	if (profile->priv->proxy == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_PROFILE_ERROR,
						 CD_PROFILE_ERROR_FAILED,
						 "Failed to connect to profile %s: %s",
						 cd_profile_get_object_path (profile),
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* get profile id */
	id = g_dbus_proxy_get_cached_property (profile->priv->proxy,
					       CD_PROFILE_PROPERTY_ID);
	if (id != NULL)
		profile->priv->id = g_variant_dup_string (id, NULL);

	/* if the profile is missing, then fail */
	if (id == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_PROFILE_ERROR,
						 CD_PROFILE_ERROR_FAILED,
						 "Failed to connect to missing profile %s",
						 cd_profile_get_object_path (profile));
		goto out;
	}

	/* get filename */
	filename = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						     CD_PROFILE_PROPERTY_FILENAME);
	if (filename != NULL)
		profile->priv->filename = cd_profile_get_nullable_str (filename);

	/* get qualifier */
	qualifier = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						      CD_PROFILE_PROPERTY_QUALIFIER);
	if (qualifier != NULL)
		profile->priv->qualifier = cd_profile_get_nullable_str (qualifier);

	/* get format */
	format = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						   CD_PROFILE_PROPERTY_FORMAT);
	if (format != NULL)
		profile->priv->format = cd_profile_get_nullable_str (format);

	/* get title */
	title = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						  CD_PROFILE_PROPERTY_TITLE);
	if (title != NULL)
		profile->priv->title = cd_profile_get_nullable_str (title);

	/* get kind */
	kind = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						 CD_PROFILE_PROPERTY_KIND);
	if (kind != NULL)
		profile->priv->kind = cd_profile_kind_from_string (g_variant_get_string (kind, NULL));

	/* get colorspace */
	colorspace = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						       CD_PROFILE_PROPERTY_COLORSPACE);
	if (colorspace != NULL)
		profile->priv->colorspace = cd_colorspace_from_string (g_variant_get_string (colorspace, NULL));

	/* get scope */
	scope = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						  CD_PROFILE_PROPERTY_SCOPE);
	if (scope != NULL)
		profile->priv->scope = cd_object_scope_from_string (g_variant_get_string (scope, NULL));

	/* get owner */
	owner = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						  CD_PROFILE_PROPERTY_OWNER);
	if (owner != NULL)
		profile->priv->owner = g_variant_get_uint32 (owner);

	/* get created */
	created = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						    CD_PROFILE_PROPERTY_CREATED);
	if (created != NULL)
		profile->priv->created = g_variant_get_int64 (created);

	/* get VCGT */
	has_vcgt = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						     CD_PROFILE_PROPERTY_HAS_VCGT);
	if (has_vcgt != NULL)
		profile->priv->has_vcgt = g_variant_get_boolean (has_vcgt);

	/* get if system wide */
	is_system_wide = g_dbus_proxy_get_cached_property (profile->priv->proxy,
							   CD_PROFILE_PROPERTY_IS_SYSTEM_WIDE);
	if (is_system_wide != NULL)
		profile->priv->is_system_wide = g_variant_get_boolean (is_system_wide);

	/* get if system wide */
	metadata = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						     CD_PROFILE_PROPERTY_METADATA);
	if (metadata != NULL)
		cd_profile_set_metadata_from_variant (profile, metadata);

	/* get signals from DBus */
	g_signal_connect (profile->priv->proxy,
			  "g-signal",
			  G_CALLBACK (cd_profile_dbus_signal_cb),
			  profile);

	/* watch if any remote properties change */
	g_signal_connect (profile->priv->proxy,
			  "g-properties-changed",
			  G_CALLBACK (cd_profile_dbus_properties_changed_cb),
			  profile);

	/* success */
	g_simple_async_result_set_op_res_gboolean (res_source, TRUE);
out:
	if (id != NULL)
		g_variant_unref (id);
	if (kind != NULL)
		g_variant_unref (kind);
	if (colorspace != NULL)
		g_variant_unref (colorspace);
	if (scope != NULL)
		g_variant_unref (scope);
	if (owner != NULL)
		g_variant_unref (owner);
	if (created != NULL)
		g_variant_unref (created);
	if (has_vcgt != NULL)
		g_variant_unref (has_vcgt);
	if (is_system_wide != NULL)
		g_variant_unref (is_system_wide);
	if (metadata != NULL)
		g_variant_unref (metadata);
	if (filename != NULL)
		g_variant_unref (filename);
	if (qualifier != NULL)
		g_variant_unref (qualifier);
	if (format != NULL)
		g_variant_unref (format);
	if (title != NULL)
		g_variant_unref (title);
	if (profiles != NULL)
		g_variant_unref (profiles);
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
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
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_PROFILE (profile));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	res = g_simple_async_result_new (G_OBJECT (profile),
					 callback,
					 user_data,
					 cd_profile_connect);

	/* already connected */
	if (profile->priv->proxy != NULL) {
		g_simple_async_result_set_op_res_gboolean (res, TRUE);
		g_simple_async_result_complete_in_idle (res);
		return;
	}

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  COLORD_DBUS_SERVICE,
				  profile->priv->object_path,
				  COLORD_DBUS_INTERFACE_PROFILE,
				  cancellable,
				  cd_profile_connect_cb,
				  res);
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
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_simple_async_result_get_op_res_gboolean (simple);
}

static void
cd_profile_set_property_cb (GObject *source_object,
			    GAsyncResult *res,
			    gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_PROFILE_ERROR,
						 CD_PROFILE_ERROR_FAILED,
						 "Failed to SetProperty: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	g_simple_async_result_set_op_res_gboolean (res_source, TRUE);
	g_variant_unref (result);
out:
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
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
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_PROFILE (profile));
	g_return_if_fail (key != NULL);
	g_return_if_fail (value != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (profile->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (profile),
					 callback,
					 user_data,
					 cd_profile_set_property);
	g_dbus_proxy_call (profile->priv->proxy,
			   "SetProperty",
			   g_variant_new ("(ss)",
			   		  key,
			   		  value),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_profile_set_property_cb,
			   res);
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
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_simple_async_result_get_op_res_gboolean (simple);
}

static void
cd_profile_install_system_wide_cb (GObject *source_object,
				   GAsyncResult *res,
				   gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	GSimpleAsyncResult *res_source = G_SIMPLE_ASYNC_RESULT (user_data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_simple_async_result_set_error (res_source,
						 CD_PROFILE_ERROR,
						 CD_PROFILE_ERROR_FAILED,
						 "Failed to InstallSystemWide: %s",
						 error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	g_simple_async_result_set_op_res_gboolean (res_source, TRUE);
	g_variant_unref (result);
out:
	g_simple_async_result_complete_in_idle (res_source);
	g_object_unref (res_source);
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
	GSimpleAsyncResult *res;

	g_return_if_fail (CD_IS_PROFILE (profile));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (profile->priv->proxy != NULL);

	res = g_simple_async_result_new (G_OBJECT (profile),
					 callback,
					 user_data,
					 cd_profile_install_system_wide);
	g_dbus_proxy_call (profile->priv->proxy,
			   "InstallSystemWide",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   cancellable,
			   cd_profile_install_system_wide_cb,
			   res);
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return profile->priv->object_path;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	return profile->priv->proxy != NULL;
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
	GString *string;

	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);

	string = g_string_new ("");
	g_string_append_printf (string, "  object-path:          %s\n",
				profile->priv->object_path);

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
	g_return_val_if_fail (CD_IS_PROFILE (profile1), FALSE);
	g_return_val_if_fail (CD_IS_PROFILE (profile2), FALSE);
	if (profile1->priv->id == NULL ||
	    profile2->priv->id == NULL)
		g_critical ("need to connect");
	return g_strcmp0 (profile1->priv->id, profile2->priv->id) == 0;
}

/*
 * _cd_profile_set_property:
 */
static void
_cd_profile_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CdProfile *profile = CD_PROFILE (object);

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_free (profile->priv->object_path);
		profile->priv->object_path = g_value_dup_string (value);
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

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_value_set_string (value, profile->priv->object_path);
		break;
	case PROP_CONNECTED:
		g_value_set_boolean (value, profile->priv->proxy != NULL);
		break;
	case PROP_ID:
		g_value_set_string (value, profile->priv->id);
		break;
	case PROP_FILENAME:
		g_value_set_string (value, profile->priv->filename);
		break;
	case PROP_QUALIFIER:
		g_value_set_string (value, profile->priv->qualifier);
		break;
	case PROP_FORMAT:
		g_value_set_string (value, profile->priv->format);
		break;
	case PROP_TITLE:
		g_value_set_string (value, profile->priv->title);
		break;
	case PROP_KIND:
		g_value_set_uint (value, profile->priv->kind);
		break;
	case PROP_COLORSPACE:
		g_value_set_uint (value, profile->priv->colorspace);
		break;
	case PROP_CREATED:
		g_value_set_int64 (value, profile->priv->created);
		break;
	case PROP_HAS_VCGT:
		g_value_set_boolean (value, profile->priv->has_vcgt);
		break;
	case PROP_IS_SYSTEM_WIDE:
		g_value_set_boolean (value, profile->priv->is_system_wide);
		break;
	case PROP_SCOPE:
		g_value_set_uint (value, profile->priv->scope);
		break;
	case PROP_OWNER:
		g_value_set_uint (value, profile->priv->owner);
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

	g_type_class_add_private (klass, sizeof (CdProfilePrivate));
}

/*
 * cd_profile_init:
 */
static void
cd_profile_init (CdProfile *profile)
{
	profile->priv = CD_PROFILE_GET_PRIVATE (profile);
	profile->priv->metadata = g_hash_table_new_full (g_str_hash,
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
	CdProfile *profile;
	guint ret;

	g_return_if_fail (CD_IS_PROFILE (object));

	profile = CD_PROFILE (object);

	g_hash_table_unref (profile->priv->metadata);
	g_free (profile->priv->object_path);
	g_free (profile->priv->id);
	g_free (profile->priv->filename);
	g_free (profile->priv->qualifier);
	g_free (profile->priv->format);
	g_free (profile->priv->title);
	if (profile->priv->proxy != NULL) {
		ret = g_signal_handlers_disconnect_by_func (profile->priv->proxy,
							    G_CALLBACK (cd_profile_dbus_signal_cb),
							    profile);
		g_assert (ret > 0);
		ret = g_signal_handlers_disconnect_by_func (profile->priv->proxy,
							    G_CALLBACK (cd_profile_dbus_properties_changed_cb),
							    profile);
		g_assert (ret > 0);
		g_object_unref (profile->priv->proxy);
	}

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
