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

#include <glib-object.h>
#include <gio/gio.h>
#include <sys/time.h>

#include "cd-common.h"
#include "cd-device.h"
#include "cd-mapping-db.h"
#include "cd-device-db.h"
#include "cd-profile-array.h"
#include "cd-profile.h"
#include "cd-inhibit.h"

static void cd_device_finalize			 (GObject *object);
static void cd_device_dbus_emit_property_changed (CdDevice *device,
						  const gchar *property_name,
						  GVariant *property_value);
static void cd_device_dbus_emit_device_changed	 (CdDevice *device);

#define CD_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_DEVICE, CdDevicePrivate))

/**
 * CdDevicePrivate:
 *
 * Private #CdDevice data
 **/
struct _CdDevicePrivate
{
	CdObjectScope			 object_scope;
	CdProfileArray			*profile_array;
	CdMappingDb			*mapping_db;
	CdDeviceDb			*device_db;
	CdInhibit			*inhibit;
	gchar				*id;
	gchar				*model;
	gchar				*serial;
	gchar				*vendor;
	gchar				*colorspace;
	gchar				*mode;
	gchar				*kind;
	gchar				*object_path;
	GDBusConnection			*connection;
	GPtrArray			*profiles;
	guint				 registration_id;
	guint				 watcher_id;
	guint64				 created;
	guint64				 modified;
	gboolean			 is_virtual;
};

enum {
	SIGNAL_INVALIDATE,
	SIGNAL_LAST
};

enum {
	PROP_0,
	PROP_OBJECT_PATH,
	PROP_ID,
	PROP_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };
G_DEFINE_TYPE (CdDevice, cd_device, G_TYPE_OBJECT)

/**
 * cd_device_get_scope:
 **/
CdObjectScope
cd_device_get_scope (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), 0);
	return device->priv->object_scope;
}

/**
 * cd_device_set_scope:
 **/
void
cd_device_set_scope (CdDevice *device, CdObjectScope object_scope)
{
	g_return_if_fail (CD_IS_DEVICE (device));
	device->priv->object_scope = object_scope;
}


/**
 * cd_device_mode_to_string:
 **/
static const gchar *
_cd_device_mode_to_string (CdDeviceMode device_mode)
{
	if (device_mode == CD_DEVICE_MODE_PHYSICAL)
		return "physical";
	if (device_mode == CD_DEVICE_MODE_VIRTUAL)
		return "virtual";
	return "unknown";
}

/**
 * cd_device_set_mode:
 **/
void
cd_device_set_mode (CdDevice *device, CdDeviceMode mode)
{
	g_return_if_fail (CD_IS_DEVICE (device));
	g_free (device->priv->mode);
	device->priv->mode = g_strdup (_cd_device_mode_to_string (mode));
}

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
 * cd_device_get_model:
 **/
const gchar *
cd_device_get_model (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	return device->priv->model;
}

/**
 * cd_device_get_kind:
 **/
const gchar *
cd_device_get_kind (CdDevice *device)
{
	g_return_val_if_fail (CD_IS_DEVICE (device), NULL);
	return device->priv->kind;
}

/**
 * cd_device_set_id:
 **/
void
cd_device_set_id (CdDevice *device, const gchar *id)
{
	gchar *id_tmp;

	g_return_if_fail (CD_IS_DEVICE (device));
	g_free (device->priv->id);

	/* make sure object path is sane */
	id_tmp = cd_main_ensure_dbus_path (id);
	device->priv->object_path = g_build_filename (COLORD_DBUS_PATH,
						      "devices",
						      id_tmp,
						      NULL);
	device->priv->id = g_strdup (id);
	g_free (id_tmp);
}

/**
 * cd_device_reset_modified:
 **/
static void
cd_device_reset_modified (CdDevice *device)
{
	g_debug ("CdDevice: set device Modified");
#if !GLIB_CHECK_VERSION (2, 25, 0)
	device->priv->modified = g_get_real_time ();
#endif
	cd_device_dbus_emit_property_changed (device,
					      "Modified",
					      g_variant_new_uint64 (device->priv->modified));
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

	/* reset modification time */
	cd_device_reset_modified (device);

	/* emit global signal */
	cd_device_dbus_emit_device_changed (device);
}

/**
 * cd_device_dbus_emit_property_changed:
 **/
static void
cd_device_dbus_emit_property_changed (CdDevice *device,
				      const gchar *property_name,
				      GVariant *property_value)
{
	GError *error_local = NULL;
	GVariantBuilder builder;
	GVariantBuilder invalidated_builder;

	/* not yet connected */
	if (device->priv->connection == NULL)
		return;

	/* build the dict */
	g_variant_builder_init (&invalidated_builder, G_VARIANT_TYPE ("as"));
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add (&builder,
			       "{sv}",
			       property_name,
			       property_value);
	g_dbus_connection_emit_signal (device->priv->connection,
				       NULL,
				       device->priv->object_path,
				       "org.freedesktop.DBus.Properties",
				       "PropertiesChanged",
				       g_variant_new ("(sa{sv}as)",
				       COLORD_DBUS_INTERFACE_DEVICE,
				       &builder,
				       &invalidated_builder),
				       &error_local);
	g_assert_no_error (error_local);
}

/**
 * cd_device_dbus_emit_device_changed:
 **/
static void
cd_device_dbus_emit_device_changed (CdDevice *device)
{
	gboolean ret;
	GError *error_local = NULL;

	/* not yet connected */
	if (device->priv->connection == NULL)
		return;

	/* emit signal */
	g_debug ("CdDevice: emit Changed on %s",
		 cd_device_get_object_path (device));
	ret = g_dbus_connection_emit_signal (device->priv->connection,
					     NULL,
					     cd_device_get_object_path (device),
					     COLORD_DBUS_INTERFACE_DEVICE,
					     "Changed",
					     NULL,
					     &error_local);
	if (!ret) {
		g_warning ("CdDevice: failed to send signal %s", error_local->message);
		g_error_free (error_local);
	}

	/* emit signal */
	g_debug ("CdDevice: emit Changed");
	ret = g_dbus_connection_emit_signal (device->priv->connection,
					     NULL,
					     COLORD_DBUS_PATH,
					     COLORD_DBUS_INTERFACE,
					     "DeviceChanged",
					     g_variant_new ("(o)",
							    cd_device_get_object_path (device)),
					     &error_local);
	if (!ret) {
		g_warning ("CdDevice: failed to send signal %s", error_local->message);
		g_error_free (error_local);
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
 * cd_device_find_profile_by_id:
 **/
static CdProfile *
cd_device_find_profile_by_id (GPtrArray *array, const gchar *object_path)
{
	CdProfile *profile = NULL;
	CdProfile *profile_tmp;
	guint i;
	gboolean ret;

	/* find using an object path */
	for (i=0; i<array->len; i++) {
		profile_tmp = g_ptr_array_index (array, i);
		ret = (g_strcmp0 (object_path,
				  cd_profile_get_id (profile_tmp)) == 0);
		if (ret) {
			profile = profile_tmp;
			goto out;
		}
	}
out:
	return  profile;
}

/**
 * cd_device_get_profiles_as_variant:
 **/
static GVariant *
cd_device_get_profiles_as_variant (CdDevice *device)
{
	CdProfile *profile;
	guint i;
	GVariant **profiles = NULL;
	GVariant *value;

	/* copy the object paths */
	profiles = g_new0 (GVariant *, device->priv->profiles->len + 1);
	for (i=0; i<device->priv->profiles->len; i++) {
		profile = g_ptr_array_index (device->priv->profiles, i);
		profiles[i] = g_variant_new_object_path (cd_profile_get_object_path (profile));
	}

	/* format the value */
	value = g_variant_new_array (G_VARIANT_TYPE_OBJECT_PATH,
				     profiles,
				     device->priv->profiles->len);
	g_free (profiles);
	return value;
}

/**
 * cd_device_remove_profile:
 **/
gboolean
cd_device_remove_profile (CdDevice *device,
			  const gchar *profile_object_path,
			  GError **error)
{
	CdDevicePrivate *priv = device->priv;
	CdProfile *profile_tmp;
	gboolean ret = FALSE;
	guint i;

	/* check the profile exists on this device */
	for (i=0; i<priv->profiles->len; i++) {
		profile_tmp = g_ptr_array_index (priv->profiles, i);
		if (g_strcmp0 (profile_object_path,
			       cd_profile_get_object_path (profile_tmp)) == 0) {
			ret = TRUE;
			break;
		}
	}
	if (!ret) {
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
			     "profile object path '%s' does not exist on '%s'",
			     profile_object_path,
			     priv->object_path);
		goto out;
	}

	/* remove from the array */
	ret = g_ptr_array_remove (priv->profiles, profile_tmp);
	g_assert (ret);

	/* emit */
	cd_device_dbus_emit_property_changed (device,
					      "Profiles",
					      cd_device_get_profiles_as_variant (device));

	/* reset modification time */
	cd_device_reset_modified (device);

	/* emit global signal */
	cd_device_dbus_emit_device_changed (device);
out:
	return ret;
}

/**
 * cd_device_add_profile:
 **/
gboolean
cd_device_add_profile (CdDevice *device,
		       const gchar *profile_object_path,
		       GError **error)
{
	CdDevicePrivate *priv = device->priv;
	CdProfile *profile;
	CdProfile *profile_tmp;
	gboolean ret = TRUE;
	guint i;

	/* is it available */
	profile = cd_profile_array_get_by_object_path (priv->profile_array,
						       profile_object_path);
	if (profile == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
			     "profile object path '%s' does not exist",
			     profile_object_path);
		goto out;
	}

	/* check it does not already exist */
	for (i=0; i<priv->profiles->len; i++) {
		profile_tmp = g_ptr_array_index (priv->profiles, i);
		if (g_strcmp0 (cd_profile_get_object_path (profile),
			       cd_profile_get_object_path (profile_tmp)) == 0) {
			ret = FALSE;
			g_set_error (error,
				     CD_MAIN_ERROR,
				     CD_MAIN_ERROR_FAILED,
				     "profile object path '%s' has already been added",
				     profile_object_path);
			goto out;
		}
	}

	/* add to the array */
	g_ptr_array_add (priv->profiles, g_object_ref (profile));

	/* emit */
	cd_device_dbus_emit_property_changed (device,
					      "Profiles",
					      cd_device_get_profiles_as_variant (device));

	/* reset modification time */
	cd_device_reset_modified (device);

	/* emit global signal */
	cd_device_dbus_emit_device_changed (device);
out:
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_device_set_property_to_db:
 **/
static void
cd_device_set_property_to_db (CdDevice *device,
			      const gchar *property,
			      const gchar *value)
{
	gboolean ret;
	GError *error = NULL;

	if (device->priv->object_scope != CD_OBJECT_SCOPE_DISK)
		return;

	ret = cd_device_db_set_property (device->priv->device_db,
					 device->priv->id,
					 property,
					 value,
					 &error);
	if (!ret) {
		g_warning ("CdDevice: failed to save property to database: %s",
			   error->message);
		g_error_free (error);
	}
}

/**
 * cd_device_set_property_internal:
 **/
gboolean
cd_device_set_property_internal (CdDevice *device,
				 const gchar *property,
				 const gchar *value,
				 gboolean save_in_db,
				 GError **error)
{
	gboolean ret = TRUE;
	CdDevicePrivate *priv = device->priv;

	g_debug ("CdDevice: Attempting to set %s to %s on %s",
		 property, value, device->priv->id);
	if (g_strcmp0 (property, "Model") == 0) {
		g_free (priv->model);
		priv->model = g_strdup (value);
	} else if (g_strcmp0 (property, "Kind") == 0) {
		g_free (priv->kind);
		priv->kind = g_strdup (value);
	} else if (g_strcmp0 (property, "Vendor") == 0) {
		g_free (priv->vendor);
		priv->vendor = g_strdup (value);
	} else if (g_strcmp0 (property, "Serial") == 0) {
		g_free (priv->serial);
		priv->serial = g_strdup (value);
	} else if (g_strcmp0 (property, "Colorspace") == 0) {
		g_free (priv->colorspace);
		priv->colorspace = g_strdup (value);
	} else if (g_strcmp0 (property, "Mode") == 0) {
		g_free (priv->mode);
		priv->mode = g_strdup (value);
	} else {
		ret = FALSE;
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
			     "property %s not understood on CdDevice",
			     property);
		goto out;
	}

	/* set this externally so we can add disk devices at startup
	 * without re-adding */
	if (save_in_db) {
		cd_device_set_property_to_db (device,
					      property,
					      value);
	}
	cd_device_dbus_emit_property_changed (device,
					      property,
					      g_variant_new_string (value));
out:
	return ret;
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
	CdProfile *profile_tmp;
	gboolean ret;
	gchar **devices = NULL;
	gchar *profile_object_path = NULL;
	gchar *property_name = NULL;
	gchar *property_value = NULL;
	gchar *regex = NULL;
	GError *error = NULL;
	guint i;
	GVariant *tuple = NULL;
	GVariant *value = NULL;

	/* return '' */
	if (g_strcmp0 (method_name, "AddProfile") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation,
						    sender,
						    "org.freedesktop.color-manager.modify-device");
		if (!ret)
			goto out;

		/* check the profile_object_path exists */
		g_variant_get (parameters, "(o)",
			       &profile_object_path);
		g_debug ("CdDevice %s:AddProfile(%s)",
			 sender, profile_object_path);

		/* add it */
		ret = cd_device_add_profile (device,
					     profile_object_path,
					     &error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			g_error_free (error);
			goto out;
		}

		/* save this to the permanent database */
		ret = cd_mapping_db_add (priv->mapping_db,
					 priv->object_path,
					 profile_object_path,
					 &error);
		if (!ret) {
			g_warning ("CdDevice: failed to save mapping to database: %s",
				   error->message);
			g_error_free (error);
		}

		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	if (g_strcmp0 (method_name, "RemoveProfile") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation,
						    sender,
						    "org.freedesktop.color-manager.modify-device");
		if (!ret)
			goto out;

		/* try to remove */
		g_variant_get (parameters, "(o)",
			       &profile_object_path);
		g_debug ("CdDevice %s:RemoveProfile(%s)",
			 sender, profile_object_path);
		ret = cd_device_remove_profile (device,
						profile_object_path,
						&error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror (invocation,
								error);
			g_error_free (error);
			goto out;
		}

		/* save this to the permanent database */
		ret = cd_mapping_db_remove (priv->mapping_db,
					    priv->object_path,
					    profile_object_path,
					    &error);
		if (!ret) {
			g_warning ("CdDevice: failed to save mapping to database: %s",
				   error->message);
			g_error_free (error);
		}

		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	/* return 'o' */
	if (g_strcmp0 (method_name, "GetProfileForQualifier") == 0) {

		/* find the profile by the qualifier search string */
		g_variant_get (parameters, "(s)", &regex);
		g_debug ("CdDevice %s:GetProfileForQualifier(%s)",
			 sender, regex);

		/* are we profiling? */
		ret = cd_inhibit_valid (priv->inhibit);
		if (!ret) {
			g_debug ("CdDevice: returning no results for profiling");
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "profiling, so ignoring '%s'",
							       regex);
			goto out;
		}

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

	/* return '' */
	if (g_strcmp0 (method_name, "MakeProfileDefault") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation,
						    sender,
						    "org.freedesktop.color-manager.modify-device");
		if (!ret)
			goto out;

		/* check the profile_object_path exists */
		g_variant_get (parameters, "(s)",
			       &profile_object_path);
		g_debug ("CdDevice %s:MakeProfileDefault(%s)",
			 sender, profile_object_path);
		profile = cd_device_find_profile_by_id (priv->profiles,
							profile_object_path);
		if (profile == NULL) {
			g_dbus_method_invocation_return_error (invocation,
							       CD_MAIN_ERROR,
							       CD_MAIN_ERROR_FAILED,
							       "profile object path '%s' does not exist for this device",
							       profile_object_path);
			goto out;
		}

		/* if this profile already default */
		if (g_ptr_array_index (priv->profiles, 0) == profile) {
			g_debug ("CdDevice: %s is already the default on %s",
				 profile_object_path,
				 priv->object_path);
			g_dbus_method_invocation_return_value (invocation, NULL);
			goto out;
		}

		/* make the profile first in the array */
		for (i=1; i<priv->profiles->len; i++) {
			profile_tmp = g_ptr_array_index (priv->profiles, i);
			if (profile_tmp == profile) {
				/* swap [0] and [i] */
				g_debug ("CdDevice: making %s the default on %s",
					 profile_object_path,
					 priv->object_path);
				profile_tmp = priv->profiles->pdata[0];
				priv->profiles->pdata[0] = profile;
				priv->profiles->pdata[i] = profile_tmp;
				break;
			}
		}

		/* emit */
		cd_device_dbus_emit_property_changed (device,
						      "Profiles",
						      cd_device_get_profiles_as_variant (device));

		/* reset modification time */
		cd_device_reset_modified (device);

		/* emit global signal */
		cd_device_dbus_emit_device_changed (device);

		g_dbus_method_invocation_return_value (invocation, NULL);
		goto out;
	}

	/* return '' */
	if (g_strcmp0 (method_name, "SetProperty") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation,
						    sender,
						    "org.freedesktop.color-manager.modify-device");
		if (!ret)
			goto out;

		/* set, and parse */
		g_variant_get (parameters, "(ss)",
			       &property_name,
			       &property_value);
		g_debug ("CdDevice %s:SetProperty(%s,%s)",
			 sender, property_name, property_value);
		ret = cd_device_set_property_internal (device,
						       property_name,
						       property_value,
						       (priv->object_scope == CD_OBJECT_SCOPE_DISK),
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
	if (g_strcmp0 (method_name, "ProfilingInhibit") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation,
						    sender,
						    "org.freedesktop.color-manager.device-inhibit");
		if (!ret)
			goto out;

		/* inhbit all profiles */
		g_debug ("CdDevice %s:ProfilingInhibit()",
			 sender);
		ret = cd_inhibit_add (priv->inhibit,
				      sender,
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
	if (g_strcmp0 (method_name, "ProfilingUninhibit") == 0) {

		/* perhaps uninhibit all profiles */
		g_debug ("CdDevice %s:ProfilingUninhibit()",
			 sender);
		ret = cd_inhibit_remove (priv->inhibit,
					 sender,
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

	/* we suck */
	g_critical ("failed to process device method %s", method_name);
out:
	g_free (profile_object_path);
	g_free (property_name);
	g_free (property_value);
	g_free (regex);
	g_strfreev (devices);
}

/**
 * cd_device_inhibit_changed_cb:
 **/
static void
cd_device_inhibit_changed_cb (CdInhibit *inhibit,
			      gpointer user_data)
{
	CdDevice *device = CD_DEVICE (user_data);

	/* emit */
	g_debug ("Emitting Device.Profiles as inhibit changed");
	cd_device_dbus_emit_property_changed (device,
					      "Profiles",
					      cd_device_get_profiles_as_variant (device));

	/* emit global signal */
	cd_device_dbus_emit_device_changed (device);
}

/**
 * cd_device_get_nullable_for_string:
 **/
static GVariant *
cd_device_get_nullable_for_string (const gchar *value)
{
	if (value == NULL)
		return g_variant_new_string ("");
	return g_variant_new_string (value);
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
	CdDevice *device = CD_DEVICE (user_data);
	CdDevicePrivate *priv = device->priv;
	gboolean ret;
	GVariant *retval = NULL;

	g_debug ("CdDevice %s:GetProperty '%s'",
		 sender, property_name);
	if (g_strcmp0 (property_name, "Created") == 0) {
		retval = g_variant_new_uint64 (priv->created);
		goto out;
	}
	if (g_strcmp0 (property_name, "Modified") == 0) {
		retval = g_variant_new_uint64 (priv->modified);
		goto out;
	}
	if (g_strcmp0 (property_name, "Model") == 0) {
		retval = cd_device_get_nullable_for_string (priv->model);
		goto out;
	}
	if (g_strcmp0 (property_name, "Vendor") == 0) {
		retval = cd_device_get_nullable_for_string (priv->vendor);
		goto out;
	}
	if (g_strcmp0 (property_name, "Serial") == 0) {
		retval = cd_device_get_nullable_for_string (priv->serial);
		goto out;
	}
	if (g_strcmp0 (property_name, "Colorspace") == 0) {
		retval = cd_device_get_nullable_for_string (priv->colorspace);
		goto out;
	}
	if (g_strcmp0 (property_name, "Mode") == 0) {
		retval = cd_device_get_nullable_for_string (priv->mode);
		goto out;
	}
	if (g_strcmp0 (property_name, "Kind") == 0) {
		retval = cd_device_get_nullable_for_string (priv->kind);
		goto out;
	}
	if (g_strcmp0 (property_name, "DeviceId") == 0) {
		retval = g_variant_new_string (priv->id);
		goto out;
	}
	if (g_strcmp0 (property_name, "Profiles") == 0) {

		/* are we profiling? */
		ret = cd_inhibit_valid (priv->inhibit);
		if (!ret) {
			const char *list = NULL;
			g_debug ("CdDevice: returning no profiles for profiling");
			/* work around a GVariant bug:
			 * Ideally we want to do g_variant_new("(ao)", NULL);
			 * but this explodes in an ugly ball of fire */
			retval = g_variant_new ("(^as)", &list);
		} else {
			retval = cd_device_get_profiles_as_variant (device);
		}
		goto out;
	}

	g_critical ("failed to get property %s", property_name);
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
	g_debug ("CdDevice: Register interface %i on %s",
		 device->priv->registration_id,
		 device->priv->object_path);

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cd_device_name_vanished_cb:
 **/
static void
cd_device_name_vanished_cb (GDBusConnection *connection,
			    const gchar *name,
			    gpointer user_data)
{
	CdDevice *device = CD_DEVICE (user_data);
	g_debug ("CdDevice: emit 'invalidate' as %s vanished", name);
	g_signal_emit (device, signals[SIGNAL_INVALIDATE], 0);
}

/**
 * cd_device_watch_sender:
 **/
void
cd_device_watch_sender (CdDevice *device, const gchar *sender)
{
	g_return_if_fail (CD_IS_DEVICE (device));
	device->priv->watcher_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
						     sender,
						     G_BUS_NAME_WATCHER_FLAGS_NONE,
						     NULL,
						     cd_device_name_vanished_cb,
						     device,
						     NULL);
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

	/**
	 * CdDevice::invalidate:
	 **/
	signals[SIGNAL_INVALIDATE] =
		g_signal_new ("invalidate",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdDeviceClass, invalidate),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

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
#if !GLIB_CHECK_VERSION (2, 25, 0)
	device->priv->created = g_get_real_time ();
	device->priv->modified = g_get_real_time ();
#else
	{
		struct timeval tm;
		gettimeofday (&tm, NULL);
		device->priv->created = tm.tv_sec;
		device->priv->modified = tm.tv_sec;
	}
#endif
	device->priv->mapping_db = cd_mapping_db_new ();
	device->priv->device_db = cd_device_db_new ();
	device->priv->inhibit = cd_inhibit_new ();
	g_signal_connect (device->priv->inhibit,
			  "changed",
			  G_CALLBACK (cd_device_inhibit_changed_cb),
			  device);
}

/**
 * cd_device_finalize:
 **/
static void
cd_device_finalize (GObject *object)
{
	CdDevice *device = CD_DEVICE (object);
	CdDevicePrivate *priv = device->priv;

	if (priv->watcher_id > 0)
		g_bus_unwatch_name (priv->watcher_id);
	if (priv->registration_id > 0) {
		g_debug ("CdDevice: Unregister interface %i on %s",
			  priv->registration_id,
			  priv->object_path);
		g_dbus_connection_unregister_object (priv->connection,
						     priv->registration_id);
	}
	g_free (priv->id);
	g_free (priv->model);
	g_free (priv->vendor);
	g_free (priv->colorspace);
	g_free (priv->mode);
	g_free (priv->serial);
	g_free (priv->kind);
	g_free (priv->object_path);
	if (priv->profiles != NULL)
		g_ptr_array_unref (priv->profiles);
	g_object_unref (priv->profile_array);
	g_object_unref (priv->mapping_db);
	g_object_unref (priv->device_db);
	g_object_unref (priv->inhibit);

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

