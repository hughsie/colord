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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUAL_COLORSPACE		0
#define QUAL_MEDIA		1
#define QUAL_RESOLUTION		2
#define QUAL_SIZE		3

static char **
split_qualifier (const char *qualifier)
{
	char *qualifier_copy;
	char **split = NULL;
	char *tmp;

	/* allocate enough for a null termination */
	split = calloc(QUAL_SIZE + 1, sizeof(char*));

	/* work on a copy as we muller the string */
	qualifier_copy = strdup(qualifier);

	/* get colorspace */
	tmp = strstr(qualifier_copy, ".");
	if (tmp == NULL)
		goto out;
	tmp[0] = '\0';
	split[QUAL_COLORSPACE] = strdup(qualifier_copy);

	/* get paper type */
	split[QUAL_MEDIA] = strdup(tmp + 1);
	tmp = strstr(split[QUAL_MEDIA], ".");
	if (tmp == NULL)
		goto out;
	tmp[0] = '\0';

	/* get resulution */
	split[QUAL_RESOLUTION] = strdup(tmp + 1);
out:
	free(qualifier_copy);
	return split;
}

static char *
get_filename_for_profile_path (DBusConnection *con,
			       const char *object_path)
{
	char *filename = NULL;
	const char *interface = "org.freedesktop.ColorManager.Profile";
	const char *property = "Filename";
	const char *tmp;
	DBusError error;
	DBusMessageIter args;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	DBusMessageIter sub;

	message = dbus_message_new_method_call("org.freedesktop.ColorManager",
					       object_path,
					       "org.freedesktop.DBus.Properties",
					       "Get");

	dbus_message_iter_init_append(message, &args);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &interface);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &property);

	/* send syncronous */
	dbus_error_init(&error);
	printf("Calling %s.Get(%s)\n", interface, property);
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
	if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_VARIANT) {
		printf("incorrect reply type\n");
		goto out;
	}

	dbus_message_iter_recurse(&args, &sub);
	dbus_message_iter_get_basic(&sub, &tmp);
	filename = strdup(tmp);
out:
	if (message != NULL)
		dbus_message_unref(message);
	if (reply != NULL)
		dbus_message_unref(reply);
	return filename;
}

static char *
get_profile_for_device_path (DBusConnection *con,
			     const char *object_path,
			     const char *qualifier)
{
	char **key = NULL;
	char *profile = NULL;
	char **split = NULL;
	char str[256];
	const char *tmp;
	DBusError error;
	DBusMessageIter args;
	DBusMessageIter entry;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	int i = 0;

	message = dbus_message_new_method_call("org.freedesktop.ColorManager",
					       object_path,
					       "org.freedesktop.ColorManager.Device",
					       "GetProfileForQualifiers");
	dbus_message_iter_init_append(message, &args);

	/* split qualifier */
	split = split_qualifier(qualifier);

	/* create the fallbacks */
	key = calloc(6, sizeof(char*));

	/* exact match */
	snprintf(str, 256, "%s.%s.%s",
		 split[QUAL_COLORSPACE],
		 split[QUAL_MEDIA],
		 split[QUAL_RESOLUTION]);
	key[i++] = strdup(str);
	snprintf(str, 256, "%s.%s.*",
		 split[QUAL_COLORSPACE],
		 split[QUAL_MEDIA]);
	key[i++] = strdup(str);
	snprintf(str, 256, "%s.*.%s",
		 split[QUAL_COLORSPACE],
		 split[QUAL_RESOLUTION]);
	key[i++] = strdup(str);
	snprintf(str, 256, "%s.*.*",
		 split[QUAL_COLORSPACE]);
	key[i++] = strdup(str);
	key[i++] = strdup("*");
	printf("specified %i qualifiers\n", i);
	dbus_message_iter_open_container(&args,
					 DBUS_TYPE_ARRAY,
					 "s",
					 &entry);
	for (i=0; key[i] != NULL; i++) {
		dbus_message_iter_append_basic(&entry,
					       DBUS_TYPE_STRING,
					       &key[i]);
	}
	dbus_message_iter_close_container(&args, &entry);

	/* send syncronous */
	dbus_error_init(&error);
	printf("Calling GetProfileForQualifiers(%s...)\n", key[0]);
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
        dbus_message_iter_get_basic(&args, &tmp);
	printf("found profile %s\n", tmp);

	/* get filename */
	profile = get_filename_for_profile_path(con, tmp);

out:
	if (message != NULL)
		dbus_message_unref(message);
	if (reply != NULL)
		dbus_message_unref(reply);
	if (key != NULL) {
		for (i=0; key[i] != NULL; i++)
			free(key[i]);
		free(key);
	}
	if (split != NULL) {
		for (i=0; split[i] != NULL; i++)
			free(split[i]);
		free(split);
	}
	return profile;
}

static char *
get_profile_for_device_id (DBusConnection *con,
			   const char *device_id,
			   const char *qualifier)
{
	char *profile = NULL;
	const char *device_path_tmp;
	DBusError error;
	DBusMessageIter args;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;

	message = dbus_message_new_method_call("org.freedesktop.ColorManager",
					       "/org/freedesktop/ColorManager",
					       "org.freedesktop.ColorManager",
					       "FindDeviceById");
	dbus_message_iter_init_append(message, &args);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &device_id);

	/* send syncronous */
	dbus_error_init(&error);
	printf("Calling FindDeviceById(%s)\n", device_id);
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
	printf("found device %s\n", device_path_tmp);
	profile = get_profile_for_device_path(con, device_path_tmp, qualifier);
out:
	if (message != NULL)
		dbus_message_unref(message);
	if (reply != NULL)
		dbus_message_unref(reply);
	return profile;
}

int
main (int argc, char **argv)
{
	DBusConnection *con = NULL;
	char *filename = NULL;

	/* check number of arguments */
	if (argc != 3) {
		printf("expected [device-id] [qualifier]\n");
		printf(" e.g. \"cups-Photosmart-B109a-m\" \"RGB.Glossy.300dpi\"\n");
		goto out;
	}

	/* connect to system bus */
	con = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (con == NULL) {
		printf("failed to connect to system bus\n");
		goto out;
	}

	/* get the best profile for the device */
	filename = get_profile_for_device_id (con, argv[1], argv[2]);
	if (filename == NULL) {
		printf("failed to get profile filename!\n");
		goto out;
	}
	printf("Use profile filename: %s\n", filename);
out:
	free (filename);
	if (con != NULL)
		dbus_connection_unref(con);
	return 0;
}
