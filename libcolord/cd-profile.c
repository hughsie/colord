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
#include <string.h>

#include "cd-profile.h"

static void	cd_profile_class_init	(CdProfileClass	*klass);
static void	cd_profile_init		(CdProfile	*profile);
static void	cd_profile_finalize	(GObject	*object);

#define CD_PROFILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_PROFILE, CdProfilePrivate))

/**
 * CdProfilePrivate:
 *
 * Private #PkProfile data
 **/
struct _CdProfilePrivate
{
	gchar			*filename;
	gchar			*id;
	gchar			*object_path;
	gchar			*qualifier;
	gchar			*title;
	GDBusProxy		*proxy;
	CdProfileKind		 kind;
	CdColorspace		 colorspace;
	gboolean		 has_vcgt;
	gboolean		 is_system_wide;
};

enum {
	PROP_0,
	PROP_ID,
	PROP_FILENAME,
	PROP_QUALIFIER,
	PROP_TITLE,
	PROP_KIND,
	PROP_COLORSPACE,
	PROP_HAS_VCGT,
	PROP_IS_SYSTEM_WIDE,
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
	return profile->priv->filename;
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
	return profile->priv->qualifier;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), 0);
	return profile->priv->kind;
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
	g_return_val_if_fail (CD_IS_PROFILE (profile), 0);
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
	return profile->priv->is_system_wide;
}

/**
 * cd_profile_dbus_properties_changed:
 **/
static void
cd_profile_dbus_properties_changed (GDBusProxy  *proxy,
				    GVariant    *changed_properties,
				    const gchar * const *invalidated_properties,
				    CdProfile   *profile)
{
	guint i;
	guint len;
	GVariantIter iter;
	gchar *property_name;
	GVariant *property_value;

	len = g_variant_iter_init (&iter, changed_properties);
	for (i=0; i < len; i++) {
		g_variant_get_child (changed_properties, i,
				     "{sv}",
				     &property_name,
				     &property_value);
		if (g_strcmp0 (property_name, "Qualifier") == 0) {
			g_free (profile->priv->qualifier);
			profile->priv->qualifier = g_variant_dup_string (property_value, NULL);
		} else if (g_strcmp0 (property_name, "Filename") == 0) {
			g_free (profile->priv->filename);
			profile->priv->filename = g_variant_dup_string (property_value, NULL);
		} else if (g_strcmp0 (property_name, "ProfileId") == 0) {
			g_free (profile->priv->id);
			profile->priv->id = g_variant_dup_string (property_value, NULL);
		} else if (g_strcmp0 (property_name, "Title") == 0) {
			g_free (profile->priv->title);
			profile->priv->title = g_variant_dup_string (property_value, NULL);
		} else if (g_strcmp0 (property_name, "Kind") == 0) {
			profile->priv->kind = g_variant_get_uint32 (property_value);
		} else if (g_strcmp0 (property_name, "Colorspace") == 0) {
			profile->priv->colorspace = g_variant_get_uint32 (property_value);
		} else if (g_strcmp0 (property_name, "HasVcgt") == 0) {
			profile->priv->has_vcgt = g_variant_get_boolean (property_value);
		} else if (g_strcmp0 (property_name, "IsSystemWide") == 0) {
			profile->priv->is_system_wide = g_variant_get_boolean (property_value);
		} else {
			g_warning ("%s property unhandled", property_name);
		}
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

	if (g_strcmp0 (signal_name, "Changed") == 0) {
		g_debug ("emit Changed on %s", profile->priv->object_path);
		g_signal_emit (profile, signals[SIGNAL_CHANGED], 0);
	} else {
		g_warning ("unhandled signal '%s'", signal_name);
	}
	g_free (object_path_tmp);
}

/**
 * cd_profile_set_object_path_sync:
 * @profile: a #CdProfile instance.
 * @object_path: The colord object path.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the object path of the object and fills up initial properties.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.0
 **/
gboolean
cd_profile_set_object_path_sync (CdProfile *profile,
				const gchar *object_path,
				GCancellable *cancellable,
				GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *filename = NULL;
	GVariant *id = NULL;
	GVariant *profiles = NULL;
	GVariant *qualifier = NULL;
	GVariant *title = NULL;
	GVariant *kind = NULL;
	GVariant *colorspace = NULL;
	GVariant *has_vcgt = NULL;
	GVariant *is_system_wide = NULL;

	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (profile->priv->proxy == NULL, FALSE);

	/* connect to the daemon */
	profile->priv->proxy =
		g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					       G_DBUS_PROXY_FLAGS_NONE,
					       NULL,
					       COLORD_DBUS_SERVICE,
					       object_path,
					       COLORD_DBUS_INTERFACE_PROFILE,
					       cancellable,
					       &error_local);
	if (profile->priv->proxy == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_FAILED,
			     "Failed to connect to profile %s: %s",
			     object_path,
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* save object path */
	profile->priv->object_path = g_strdup (object_path);

	/* get profile id */
	id = g_dbus_proxy_get_cached_property (profile->priv->proxy,
					       "ProfileId");
	if (id != NULL)
		profile->priv->id = g_variant_dup_string (id, NULL);

	/* get filename */
	filename = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						     "Filename");
	if (filename != NULL)
		profile->priv->filename = g_variant_dup_string (filename, NULL);

	/* get qualifier */
	qualifier = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						      "Qualifier");
	if (qualifier != NULL)
		profile->priv->qualifier = g_variant_dup_string (qualifier, NULL);

	/* get title */
	title = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						  "Title");
	if (title != NULL)
		profile->priv->title = g_variant_dup_string (title, NULL);

	/* get kind */
	kind = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						 "Kind");
	if (kind != NULL)
		profile->priv->kind = g_variant_get_uint32 (kind);

	/* get colorspace */
	colorspace = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						       "Colorspace");
	if (colorspace != NULL)
		profile->priv->colorspace = g_variant_get_uint32 (colorspace);

	/* get VCGT */
	has_vcgt = g_dbus_proxy_get_cached_property (profile->priv->proxy,
						     "HasVcgt");
	if (has_vcgt != NULL)
		profile->priv->has_vcgt = g_variant_get_boolean (has_vcgt);

	/* get if system wide */
	is_system_wide = g_dbus_proxy_get_cached_property (profile->priv->proxy,
							   "IsSystemWide");
	if (is_system_wide != NULL)
		profile->priv->is_system_wide = g_variant_get_boolean (is_system_wide);

	/* get signals from DBus */
	g_signal_connect (profile->priv->proxy,
			  "g-signal",
			  G_CALLBACK (cd_profile_dbus_signal_cb),
			  profile);

	/* watch if any remote properties change */
	g_signal_connect (profile->priv->proxy,
			  "g-properties-changed",
			  G_CALLBACK (cd_profile_dbus_properties_changed),
			  profile);

	/* success */
	g_debug ("Connected to profile %s",
		 profile->priv->id);
out:
	if (id != NULL)
		g_variant_unref (id);
	if (kind != NULL)
		g_variant_unref (kind);
	if (colorspace != NULL)
		g_variant_unref (colorspace);
	if (has_vcgt != NULL)
		g_variant_unref (has_vcgt);
	if (is_system_wide != NULL)
		g_variant_unref (is_system_wide);
	if (filename != NULL)
		g_variant_unref (filename);
	if (qualifier != NULL)
		g_variant_unref (qualifier);
	if (title != NULL)
		g_variant_unref (title);
	if (profiles != NULL)
		g_variant_unref (profiles);
	return ret;
}

/**
 * cd_profile_set_property_sync:
 **/
static gboolean
cd_profile_set_property_sync (CdProfile *profile,
			     const gchar *name,
			     const gchar *value,
			     GCancellable *cancellable,
			     GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *response = NULL;

	/* execute sync method */
	response = g_dbus_proxy_call_sync (profile->priv->proxy,
					   "SetProperty",
					   g_variant_new ("(ss)",
						       name,
						       value),
					   G_DBUS_CALL_FLAGS_NONE,
					   -1, NULL, &error_local);
	if (response == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_FAILED,
			     "Failed to set property: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	if (response != NULL)
		g_variant_unref (response);
	return ret;
}

/**
 * cd_profile_set_filename_sync:
 * @profile: a #CdProfile instance.
 * @value: The filename.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the profile model.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.0
 **/
gboolean
cd_profile_set_filename_sync (CdProfile *profile,
			      const gchar *value,
			      GCancellable *cancellable,
			      GError **error)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (profile->priv->proxy != NULL, FALSE);

	/* execute sync helper */
	return cd_profile_set_property_sync (profile, "Filename", value,
					     cancellable, error);
}

/**
 * cd_profile_install_system_wide_sync:
 * @profile: a #CdProfile instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the profile system wide.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.1
 **/
gboolean
cd_profile_install_system_wide_sync (CdProfile *profile,
				     GCancellable *cancellable,
				     GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *response = NULL;

	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (profile->priv->proxy != NULL, FALSE);

	/* execute sync method */
	response = g_dbus_proxy_call_sync (profile->priv->proxy,
					   "InstallSystemWide",
					   NULL,
					   G_DBUS_CALL_FLAGS_NONE,
					   -1, NULL, &error_local);
	if (response == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_PROFILE_ERROR,
			     CD_PROFILE_ERROR_FAILED,
			     "Failed to install system wide: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	if (response != NULL)
		g_variant_unref (response);
	return ret;
}

/**
 * cd_profile_set_qualifier_sync:
 * @profile: a #CdProfile instance.
 * @value: The qualifier.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the profile model.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.1.0
 **/
gboolean
cd_profile_set_qualifier_sync (CdProfile *profile,
			       const gchar *value,
			       GCancellable *cancellable,
			       GError **error)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (profile->priv->proxy != NULL, FALSE);

	/* execute sync helper */
	return cd_profile_set_property_sync (profile, "Qualifier", value,
					     cancellable, error);
}

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

/*
 * cd_profile_set_property:
 */
static void
cd_profile_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CdProfile *profile = CD_PROFILE (object);

	switch (prop_id) {
	case PROP_ID:
		g_free (profile->priv->id);
		profile->priv->id = g_strdup (g_value_get_string (value));
		break;
	case PROP_FILENAME:
		g_free (profile->priv->filename);
		profile->priv->filename = g_strdup (g_value_get_string (value));
		break;
	case PROP_QUALIFIER:
		g_free (profile->priv->qualifier);
		profile->priv->qualifier = g_strdup (g_value_get_string (value));
		break;
	case PROP_TITLE:
		g_free (profile->priv->title);
		profile->priv->title = g_strdup (g_value_get_string (value));
		break;
	case PROP_KIND:
		profile->priv->kind = g_value_get_uint (value);
		break;
	case PROP_COLORSPACE:
		profile->priv->colorspace = g_value_get_uint (value);
		break;
	case PROP_HAS_VCGT:
		profile->priv->has_vcgt = g_value_get_boolean (value);
		break;
	case PROP_IS_SYSTEM_WIDE:
		profile->priv->is_system_wide = g_value_get_boolean (value);
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
	case PROP_ID:
		g_value_set_string (value, profile->priv->id);
		break;
	case PROP_FILENAME:
		g_value_set_string (value, profile->priv->filename);
		break;
	case PROP_QUALIFIER:
		g_value_set_string (value, profile->priv->qualifier);
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
	case PROP_HAS_VCGT:
		g_value_set_boolean (value, profile->priv->has_vcgt);
		break;
	case PROP_IS_SYSTEM_WIDE:
		g_value_set_boolean (value, profile->priv->is_system_wide);
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
	object_class->set_property = cd_profile_set_property;
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
							      G_PARAM_READWRITE));
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
							      G_PARAM_READWRITE));
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
							      G_PARAM_READWRITE));

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
							      G_PARAM_READWRITE));
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
							      G_PARAM_READWRITE));

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
							      G_PARAM_READWRITE));

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
							      G_PARAM_READWRITE));

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
							      G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (CdProfilePrivate));
}

/*
 * cd_profile_init:
 */
static void
cd_profile_init (CdProfile *profile)
{
	profile->priv = CD_PROFILE_GET_PRIVATE (profile);
}

/*
 * cd_profile_finalize:
 */
static void
cd_profile_finalize (GObject *object)
{
	CdProfile *profile;

	g_return_if_fail (CD_IS_PROFILE (object));

	profile = CD_PROFILE (object);

	g_free (profile->priv->object_path);
	g_free (profile->priv->id);
	g_free (profile->priv->filename);
	g_free (profile->priv->qualifier);
	g_free (profile->priv->title);
	if (profile->priv->proxy != NULL)
		g_object_unref (profile->priv->proxy);

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

