/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#include <dbus/dbus.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
helper_dict_add_property (DBusMessageIter *dict,
			  const char *key,
			  const char *value)
{

	DBusMessageIter entry;
	dbus_message_iter_open_container(dict,
					 DBUS_TYPE_DICT_ENTRY,
					 NULL,
					 &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &value);
	dbus_message_iter_close_container(dict, &entry);
}

int
main (int argc, char **argv)
{
	const char *device_id;
	const char *device_path_tmp;
	const gchar *scope = "temp";
	DBusConnection *con;
	DBusError error;
	DBusMessageIter args;
	DBusMessageIter dict;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	GMainLoop *loop;

	/* connect to system bus */
	con = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (con == NULL) {
		printf("failed to connect to system bus\n");
		goto out;
	}

	/* this is unique to the device */
	device_id = "hello-dave";
	message = dbus_message_new_method_call("org.freedesktop.ColorManager",
					       "/org/freedesktop/ColorManager",
					       "org.freedesktop.ColorManager",
					       "CreateDevice");
	dbus_message_iter_init_append(message, &args);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &device_id);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &scope);

	/* set initial properties */
	dbus_message_iter_open_container(&args,
					 DBUS_TYPE_ARRAY,
					 "{ss}",
					 &dict);
	helper_dict_add_property(&dict, "Colorspace", "RGB");
	helper_dict_add_property(&dict, "Kind", "scanner");
	dbus_message_iter_close_container(&args, &dict);

	/* send syncronous */
	dbus_error_init(&error);
	printf("Calling CreateDevice(%s,%s)\n",
		device_id, scope);
	reply = dbus_connection_send_with_reply_and_block(con,
							  message,
							  -1,
							  &error);
	if (reply == NULL) {
		printf("failed to send: %s:%s\n",
			  error.name, error.message);
		dbus_error_free(&error);
		goto out;
	}

	/* get reply data */
        dbus_message_iter_init(reply, &args);
	if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_OBJECT_PATH) {
		printf("incorrect reply type\n");
		goto out;
	}
        dbus_message_iter_get_basic(&args, &device_path_tmp);
	printf("created device %s\n", device_path_tmp);

	/* just spin in a main loop */
	loop = g_main_loop_new(NULL, TRUE);
	g_main_loop_run(loop);
	g_main_loop_unref(loop);
out:
	if (con != NULL)
		dbus_connection_unref(con);
	if (message != NULL)
		dbus_message_unref(message);
	if (reply != NULL)
		dbus_message_unref(reply);
	return 0;
}
