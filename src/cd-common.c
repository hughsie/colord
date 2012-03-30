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

#ifdef USE_POLKIT
#include <polkit/polkit.h>
#endif

#include "cd-common.h"

/**
 * cd_main_error_quark:
 **/
GQuark
cd_main_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark) {
		quark = g_quark_from_static_string ("colord");
		g_dbus_error_register_error (quark,
					     CD_MAIN_ERROR_FAILED,
					     COLORD_DBUS_SERVICE ".Failed");
		g_dbus_error_register_error (quark,
					     CD_MAIN_ERROR_ALREADY_EXISTS,
					     COLORD_DBUS_SERVICE ".AlreadyExists");
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
cd_main_get_sender_uid (GDBusMethodInvocation *invocation, GError **error)
{
	const gchar *sender;
	GDBusConnection *connection;
	guint uid = G_MAXUINT;
	GVariant *value;

	/* call into DBus to get the user ID that issued the request */
	connection = g_dbus_method_invocation_get_connection (invocation);
	sender = g_dbus_method_invocation_get_sender (invocation);
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
 * cd_main_sender_authenticated:
 **/
gboolean
cd_main_sender_authenticated (GDBusMethodInvocation *invocation,
			      const gchar *action_id)
{
	const gchar *sender;
	gboolean ret = FALSE;
	GError *error = NULL;
	guint uid;
#ifdef USE_POLKIT
	PolkitAuthorizationResult *result = NULL;
	PolkitSubject *subject = NULL;
	PolkitAuthority *authority = NULL;
#endif

	/* uid 0 is allowed to do all actions */
	sender = g_dbus_method_invocation_get_sender (invocation);
	uid = cd_main_get_sender_uid (invocation, &error);
	if (uid == G_MAXUINT) {
		g_dbus_method_invocation_return_error (invocation,
						       CD_MAIN_ERROR,
						       CD_MAIN_ERROR_FAILED,
						       "could not get uid to authenticate %s: %s",
						       action_id,
						       error->message);
		g_error_free (error);
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
	authority = polkit_authority_get_sync (NULL, &error);
	if (authority == NULL) {
		g_warning ("failed to get pokit authority: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* do authorization async */
	subject = polkit_system_bus_name_new (sender);
	result = polkit_authority_check_authorization_sync (authority, subject,
			action_id,
			NULL,
			POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
			NULL,
			&error);

	/* failed */
	if (result == NULL) {
		g_dbus_method_invocation_return_error (invocation,
						       CD_MAIN_ERROR,
						       CD_MAIN_ERROR_FAILED,
						       "could not check %s for auth: %s",
						       action_id,
						       error->message);
		g_error_free (error);
		goto out;
	}

	/* did not auth */
	if (!polkit_authorization_result_get_is_authorized (result)) {
		g_dbus_method_invocation_return_error (invocation,
						       CD_MAIN_ERROR,
						       CD_MAIN_ERROR_FAILED,
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
