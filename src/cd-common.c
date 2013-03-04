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

#include <string.h>

#ifdef USE_POLKIT
#include <polkit/polkit.h>
#endif

#include "cd-common.h"

/**
 * cd_client_error_quark:
 **/
GQuark
cd_client_error_quark (void)
{
	guint i;
	static GQuark quark = 0;
	if (!quark) {
		quark = g_quark_from_static_string ("colord");
		for (i = 0; i < CD_CLIENT_ERROR_LAST; i++) {
			g_dbus_error_register_error (quark,
						     i,
						     cd_client_error_to_string (i));
		}
	}
	return quark;
}

/**
 * cd_main_ensure_dbus_path:
 **/
gchar *
cd_main_ensure_dbus_path (const gchar *object_path)
{
	gchar *object_path_tmp;
	object_path_tmp = g_strdup (object_path);
	g_strcanon (object_path_tmp,
		    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		    "abcdefghijklmnopqrstuvwxyz"
		    "1234567890_",
		    '_');
	return object_path_tmp;
}

/**
 * cd_main_get_sender_uid:
 **/
guint
cd_main_get_sender_uid (GDBusConnection *connection,
			const gchar *sender,
			GError **error)
{
	guint uid = G_MAXUINT;
	GVariant *value;

	/* call into DBus to get the user ID that issued the request */
	value = g_dbus_connection_call_sync (connection,
					     "org.freedesktop.DBus",
					     "/org/freedesktop/DBus",
					     "org.freedesktop.DBus",
					     "GetConnectionUnixUser",
					     g_variant_new ("(s)",
							    sender),
					     NULL,
					     G_DBUS_CALL_FLAGS_NONE,
					     200,
					     NULL,
					     error);
	if (value != NULL) {
		g_variant_get (value, "(u)", &uid);
		g_variant_unref (value);
	}
	return uid;
}

/**
 * cd_main_get_sender_pid:
 **/
guint
cd_main_get_sender_pid (GDBusConnection *connection,
			const gchar *sender,
			GError **error)
{
	guint pid = G_MAXUINT;
	GVariant *value;

	/* call into DBus to get the user ID that issued the request */
	value = g_dbus_connection_call_sync (connection,
					     "org.freedesktop.DBus",
					     "/org/freedesktop/DBus",
					     "org.freedesktop.DBus",
					     "GetConnectionUnixProcessID",
					     g_variant_new ("(s)",
							    sender),
					     NULL,
					     G_DBUS_CALL_FLAGS_NONE,
					     200,
					     NULL,
					     error);
	if (value != NULL) {
		g_variant_get (value, "(u)", &pid);
		g_variant_unref (value);
	}
	return pid;
}

/**
 * cd_main_sender_authenticated:
 **/
gboolean
cd_main_sender_authenticated (GDBusConnection *connection,
			      const gchar *sender,
			      const gchar *action_id,
			      GError **error)
{
	gboolean ret = FALSE;
	GError *error_local = NULL;
	guint uid;
#ifdef USE_POLKIT
	PolkitAuthorizationResult *result = NULL;
	PolkitSubject *subject = NULL;
	PolkitAuthority *authority = NULL;
#endif

	/* uid 0 is allowed to do all actions */
	uid = cd_main_get_sender_uid (connection, sender, &error_local);
	if (uid == G_MAXUINT) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED_TO_AUTHENTICATE,
			     "could not get uid to authenticate %s: %s",
			     action_id,
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* the root user can always do all actions */
	if (uid == 0) {
		g_debug ("CdCommon: not checking %s for %s as uid 0",
			 action_id, sender);
		ret = TRUE;
		goto out;
	}

	/* a client running as the daemon user may also do all actions */
	if (uid == getuid ()) {
		g_debug ("CdCommon: not checking %s for %s as running as daemon user",
			 action_id, sender);
		ret = TRUE;
		goto out;
	}

#ifdef USE_POLKIT
	/* get authority */
	authority = polkit_authority_get_sync (NULL, &error_local);
	if (authority == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED_TO_AUTHENTICATE,
			     "failed to get polkit authorit: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* do authorization async */
	subject = polkit_system_bus_name_new (sender);
	result = polkit_authority_check_authorization_sync (authority, subject,
			action_id,
			NULL,
			POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
			NULL,
			&error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED_TO_AUTHENTICATE,
			     "could not check %s for auth: %s",
			     action_id,
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* did not auth */
	if (!polkit_authorization_result_get_is_authorized (result)) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED_TO_AUTHENTICATE,
			     "failed to obtain %s auth",
			     action_id);
		goto out;
	}
#else
	g_warning ("CdCommon: not checking %s for %s as no PolicyKit support",
		   action_id, sender);
#endif

	/* success */
	ret = TRUE;
out:
#ifdef USE_POLKIT
	if (authority != NULL)
		g_object_unref (authority);
	if (result != NULL)
		g_object_unref (result);
	if (subject != NULL)
		g_object_unref (subject);
#endif
	return ret;
}


/**
 * cd_main_mkdir_with_parents:
 **/
gboolean
cd_main_mkdir_with_parents (const gchar *filename, GError **error)
{
	gboolean ret;
	GFile *file = NULL;

	/* ensure desination exists */
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (!ret) {
		file = g_file_new_for_path (filename);
		ret = g_file_make_directory_with_parents (file, NULL, error);
		if (!ret)
			goto out;
	}
out:
	if (file != NULL)
		g_object_unref (file);
	return ret;
}

/**
 * cd_main_vendor_display_name:
 **/
gchar *
cd_main_vendor_display_name (const gchar *vendor)
{
	GString *display_name;
	guint i;
	const gchar *suffixes[] =
		{ "Co.", "Co", "Inc.", "Inc", "Ltd.", "Ltd",
		  "Corporation", "Incorporated", "Limited",
		  NULL };
	struct {
		const gchar *old;
		const gchar *new;
	} vendor_names[] = {
		{ "Acer Technologies", "Acer" },
		{ "Apple Computer Inc", "Apple" },
		{ "BTC Korea Co., Ltd", "BTC" },
		{ "Eizo Nanao Corporation", "Eizo" },
		{ "Fujitsu Siemens Computers GmbH", "Fujitsu Siemens" },
		{ "Goldstar Company Ltd", "Goldstar" },
		{ "Hewlett-Packard", "Hewlett Packard" },
		{ "HP", "Hewlett Packard" },
		{ "HWP", "Hewlett Packard" },
		{ "Lenovo Group Limited", "Lenovo" },
		{ "LENOVO", "Lenovo" },
		{ "MARANTZ JAPAN, INC.", "Marantz" },
		{ "NIKON", "Nikon" },
		{ "Philips Consumer Electronics Company", "Philips" },
		{ "SAM", "Samsung" },
		{ "Samsung Electric Company", "Samsung" },
		{ "samsung", "Samsung" },
		{ "Toshiba America Info Systems Inc", "Toshiba" },
		{ NULL, NULL }
	};

	/* correct some company names */
	for (i = 0; vendor_names[i].old != NULL; i++) {
		if (g_str_has_prefix (vendor, vendor_names[i].old)) {
			display_name = g_string_new (vendor_names[i].new);
			goto out;
		}
	}

	/* get rid of suffixes */
	display_name = g_string_new (vendor);
	for (i = 0; suffixes[i] != NULL; i++) {
		if (g_str_has_suffix (display_name->str, suffixes[i]))
			g_string_truncate (display_name,
					   display_name->len - strlen (suffixes[i]));
	}
	g_strchomp (display_name->str);
out:
	return g_string_free (display_name, FALSE);
}
