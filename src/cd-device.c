/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>

#include "cd-common.h"
#include "cd-device.h"
#include "cd-profile-array.h"
#include "cd-profile.h"

static void     cd_device_finalize	(GObject     *object);

#define CD_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_DEVICE, CdDevicePrivate))

/**
 * CdDevicePrivate:
 *
 * Private #CdDevice data
 **/
struct _CdDevicePrivate
{
	gchar				*id;
	gchar				*object_path;
	GPtrArray			*profiles;
	guint				 registration_id;
	GDBusConnection			*connection;
	CdProfileArray			*profile_array;
};

enum {
	PROP_0,
	PROP_OBJECT_PATH,
	PROP_ID,
	PROP_LAST
};

G_DEFINE_TYPE (CdDevice, cd_device, G_TYPE_OBJECT)

/**
 * cd_device_get_object_path:
 **/
const gchar *
cd_device_get_object_path (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	return device->priv->object_path;
}

/**
 * cd_device_get_id:
 **/
const gchar *
cd_device_get_id (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	return device->priv->id;
}

/**
 * cd_device_set_id:
 **/
void
cd_device_set_id (CdDevice *device, const gchar *id)
{
	g_return_if_fail (CD_IS_DEVICE (device));
	g_free (device->priv->id);
	device->priv->id = g_strdup (id);
	device->priv->object_path = g_build_filename (COLORD_DBUS_PATH, id, NULL);
}

/**
 * cd_device_get_profiles:
 **/
GPtrArray *
cd_device_get_profiles (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	return device->priv->profiles;
}

/**
 * cd_device_set_profiles:
 **/
void
cd_device_set_profiles (CdDevice *device, GPtrArray *profiles)
{
	g_return_if_fail (CD_IS_DEVICE (device));
	if (device->priv->profiles != NULL)
		g_ptr_array_unref (device->priv->profiles);
	device->priv->profiles = g_ptr_array_ref (profiles);
}

/**
 * cd_device_dbus_emit_changed:
 **/
static void
cd_device_dbus_emit_changed (CdDevice *device)
{
	gboolean ret;
	GError *error = NULL;

	/* emit signal */
	ret = g_dbus_connection_emit_signal (device->priv->connection,
					     NULL,
					     cd_device_get_object_path (device),
					     COLORD_DBUS_INTERFACE_DEVICE,
					     "Changed",
					     NULL,
					     &error);
	if (!ret) {
		g_warning ("failed to send signal %s", error->message);
		g_error_free (error);
	}
}

/**
 * cd_device_find_by_qualifier:
 **/
static CdProfile *
cd_device_find_by_qualifier (const gchar *regex, GPtrArray *array)
{
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;
	guint i;
	gboolean ret;

	/* find using a wildcard */
	for (i=0; i<array->len; i++) {
		profile_tmp = g_ptr_array_index (array, i);
		ret = g_regex_match_simple (regex,
					    cd_profile_get_qualifier (profile_tmp),
					    0, 0);
		if (ret) {
			profile = profile_tmp;
			goto out;
		}
	}
out:
	return  profile;
}

/**
 * cd_device_dbus_method_call:
 **/
static void
cd_device_dbus_method_call (GDBusConnection *connection_, const gchar *sender,
			    const gchar *object_path, const gchar *interface_name,
			    const gchar *method_name, GVariant *parameters,
			    GDBusMethodInvocation *invocation, gpointer user_data)
{
	CdDevice *device = CD_DEVICE (user_data);
	CdDevicePrivate *priv = device->priv;
	CdProfile *profile;
	CdProfile *item_profile_tmp;
	gboolean ret;
	gchar **devices = NULL;
	gchar *profile_object_path = NULL;
	gchar *regex = NULL;
	guint i;
	GVariant *tuple = NULL;
	GVariant *value = NULL;

	/* return '' */
	if (g_strcmp0 (method_name, "AddProfile") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation, sender);
		if (!ret)
			goto out;

		/* check the profile_object_path exists */
		g_variant_get (parameters, "(o)",
			       &profile_object_path);
		profile = cd_profile_array_get_by_object_path (priv->profile_array,
							       profile_object_path);
		if (profile == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "profile object path '%s' does not exist",
							       profile_object_path);
			goto out;
		}

		/* check it does not already exist */
		for (i=0; i<priv->profiles->len; i++) {
			item_profile_tmp = g_ptr_array_index (priv->profiles, i);
			if (g_strcmp0 (cd_profile_get_object_path (profile),
				       cd_profile_get_object_path (item_profile_tmp)) == 0) {
				g_dbus_method_invocation_return_error (invocation,
								       CD_MAIN_ERROR,
								       CD_MAIN_ERROR_FAILED,
								       "profile object path '%s' has already been added",
								       profile_object_path);
				goto out;
			}
		}

		/* add to the array */
		g_ptr_array_add (priv->profiles, profile);

		/* emit */
		cd_device_dbus_emit_changed (device);

		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	/* return 'o' */
	if (g_strcmp0 (method_name, "GetProfileForQualifier") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation, sender);
		if (!ret)
			goto out;

		/* find the profile by the qualifier search string */
		g_variant_get (parameters, "(s)", &regex);
		profile = cd_device_find_by_qualifier (regex,
						       priv->profiles);
		if (profile == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "nothing matched expression '%s'",
							       regex);
			goto out;
		}

		value = g_variant_new_object_path (cd_profile_get_object_path (profile));
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* we suck */
	g_critical ("failed to process method %s", method_name);
out:
	g_free (regex);
	g_free (profile_object_path);
#if 0
	if (tuple != NULL)
		g_variant_unref (tuple);
	if (value != NULL)
		g_variant_unref (value);
#endif
	g_strfreev (devices);
	return;
}

/**
 * cd_device_dbus_get_property:
 **/
static GVariant *
cd_device_dbus_get_property (GDBusConnection *connection_, const gchar *sender,
			     const gchar *object_path, const gchar *interface_name,
			     const gchar *property_name, GError **error,
			     gpointer user_data)
{
	CdDevicePrivate *priv = CD_DEVICE (user_data)->priv;
	CdProfile *profile;
	guint i;
	GVariant **profiles = NULL;
	GVariant *retval = NULL;

	if (g_strcmp0 (property_name, "Created") == 0) {
		retval = g_variant_new_uint64 (0);
		goto out;
	}
	if (g_strcmp0 (property_name, "Model") == 0) {
		retval = g_variant_new_string ("hello dave");
		goto out;
	}
	if (g_strcmp0 (property_name, "DeviceId") == 0) {
		retval = g_variant_new_string (priv->id);
		goto out;
	}
	if (g_strcmp0 (property_name, "Profiles") == 0) {

		/* copy the object paths */
		profiles = g_new0 (GVariant *, priv->profiles->len + 1);
		for (i=0; i<priv->profiles->len; i++) {
			profile = g_ptr_array_index (priv->profiles, i);
			profiles[i] = g_variant_new_object_path (cd_profile_get_object_path (profile));
		}

		/* format the value */
		retval = g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
					      profiles,
					      priv->profiles->len);
		goto out;
	}

	g_critical ("failed to set property %s", property_name);
out:
	return retval;
}

/**
 * cd_device_register_object:
 **/
gboolean
cd_device_register_object (CdDevice *device,
			   GDBusConnection *connection,
			   GDBusInterfaceInfo *info,
			   GError **error)
{
	GError *error_local = NULL;
	gboolean ret = FALSE;

	static const GDBusInterfaceVTable interface_vtable = {
		cd_device_dbus_method_call,
		cd_device_dbus_get_property,
		NULL
	};

	device->priv->connection = connection;
	device->priv->registration_id = g_dbus_connection_register_object (
		connection,
		device->priv->object_path,
		info,
		&interface_vtable,
		device,  /* user_data */
		NULL,  /* user_data_free_func */
		&error_local); /* GError** */
	if (device->priv->registration_id == 0) {
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
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
 * cd_device_get_property:
 **/
static void
cd_device_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	CdDevice *device = CD_DEVICE (object);
	CdDevicePrivate *priv = device->priv;

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
 * cd_device_set_property:
 **/
static void
cd_device_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CdDevice *device = CD_DEVICE (object);
	CdDevicePrivate *priv = device->priv;

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
 * cd_device_class_init:
 **/
static void
cd_device_class_init (CdDeviceClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_device_finalize;
	object_class->get_property = cd_device_get_property;
	object_class->set_property = cd_device_set_property;

	/**
	 * CdDevice:object-path:
	 */
	pspec = g_param_spec_string ("object-path", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_OBJECT_PATH, pspec);

	/**
	 * CdDevice:id:
	 */
	pspec = g_param_spec_string ("id", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ID, pspec);

	g_type_class_add_private (klass, sizeof (CdDevicePrivate));
}

/**
 * cd_device_init:
 **/
static void
cd_device_init (CdDevice *device)
{
	device->priv = CD_DEVICE_GET_PRIVATE (device);
	device->priv->profiles = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	device->priv->profile_array = cd_profile_array_new ();
}

/**
 * cd_device_finalize:
 **/
static void
cd_device_finalize (GObject *object)
{
	CdDevice *device = CD_DEVICE (object);
	CdDevicePrivate *priv = device->priv;

	if (priv->registration_id > 0) {
		g_dbus_connection_unregister_object (priv->connection,
						     priv->registration_id);
	}
	g_free (priv->id);
	g_free (priv->object_path);
	if (priv->profiles != NULL)
		g_ptr_array_unref (priv->profiles);
	g_object_unref (priv->profile_array);

	G_OBJECT_CLASS (cd_device_parent_class)->finalize (object);
}

/**
 * cd_device_new:
 **/
CdDevice *
cd_device_new (void)
{
	CdDevice *device;
	device = g_object_new (CD_TYPE_DEVICE, NULL);
	return CD_DEVICE (device);
}

