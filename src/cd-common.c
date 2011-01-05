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
}
