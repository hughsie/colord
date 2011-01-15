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

#include <polkit/polkit.h>

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
	}
	return quark;
}

/**
 * cd_main_ensure_dbus_path:
 **/
void
cd_main_ensure_dbus_path (gchar *object_path)
{
	g_strcanon (object_path,
		    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		    "abcdefghijklmnopqrstuvwxyz"
		    "1234567890_/",
		    '_');
}

/**
 * cd_main_sender_authenticated:
 **/
gboolean
cd_main_sender_authenticated (GDBusMethodInvocation *invocation,
			      const gchar *sender,
			      const gchar *action_id)
{
#ifdef USE_POLKIT
	gboolean ret = FALSE;
	GError *error = NULL;
	PolkitAuthorizationResult *result = NULL;
	PolkitSubject *subject;
	PolkitAuthority *authority = NULL;

	/* get authority */
	authority = polkit_authority_get_sync (NULL, &error);
	if (authority == NULL) {
		g_error ("failed to get pokit authority: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* do authorization async */
	subject = polkit_system_bus_name_new (sender);
	result = polkit_authority_check_authorization_sync (authority, subject,
			action_id,
			NULL,
			POLKIT_CHECK_AUTHORIZATION_FLAGS_NONE,
			NULL,
			&error);

	/* failed */
	if (result == NULL) {
		g_dbus_method_invocation_return_error (invocation,
						       CD_MAIN_ERROR,
						       CD_MAIN_ERROR_FAILED,
						       "could not check for auth: %s",
						       error->message);
		g_error_free (error);
		goto out;
	}

	/* did not auth */
	if (!polkit_authorization_result_get_is_authorized (result)) {
		g_dbus_method_invocation_return_error (invocation,
						       CD_MAIN_ERROR,
						       CD_MAIN_ERROR_FAILED,
						       "failed to obtain auth");
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	if (authority != NULL)
		g_object_unref (authority);
	if (result != NULL)
		g_object_unref (result);
	g_object_unref (subject);
	return ret;
#else
	g_warning ("CdCommon: not checking %s for %s as no PolicyKit support",
		   action_id, sender);
	return TRUE;
#endif
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
 * cd_object_scope_to_string:
 *
 * Converts a #CdObjectScope to a string.
 *
 * Return value: identifier string
 *
 * Since: 0.1.0
 **/
const gchar *
cd_object_scope_to_string (CdObjectScope kind_enum)
{
	const gchar *kind = NULL;
	switch (kind_enum) {
	case CD_OBJECT_SCOPE_NORMAL:
		kind = "normal";
		break;
	case CD_OBJECT_SCOPE_TEMPORARY:
		kind = "temp";
		break;
	case CD_OBJECT_SCOPE_DISK:
		kind = "disk";
		break;
	default:
		kind = "unknown";
		break;
	}
	return kind;
}

/**
 * cd_object_scope_from_string:
 *
 * Converts a string to a #CdObjectScope.
 *
 * Return value: enumerated value
 *
 * Since: 0.1.0
 **/
CdObjectScope
cd_object_scope_from_string (const gchar *type)
{
	if (type == NULL)
		return CD_OBJECT_SCOPE_NORMAL;
	if (g_strcmp0 (type, "display") == 0)
		return CD_OBJECT_SCOPE_TEMPORARY;
	if (g_strcmp0 (type, "scanner") == 0)
		return CD_OBJECT_SCOPE_DISK;
	if (g_strcmp0 (type, "normal") == 0)
		return CD_OBJECT_SCOPE_NORMAL;
	return CD_OBJECT_SCOPE_NORMAL;
}
