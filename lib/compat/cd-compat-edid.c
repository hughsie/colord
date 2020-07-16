/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Richard Hughes <richard@hughsie.com>
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
 * SECTION:cd-edid
 * @short_description: Compatibility routines for applications that process the EDID
 *
 * These functions here are designed as a ucmm-like wrapper for ArgyllCMS.
 * Other software should use the native functionality in libcolord rather than
 * this shim functionality.
 */

#include "config.h"

#include <colord.h>
#include <string.h>

#include "cd-compat-edid.h"

/**
 * cd_edid_install_profile:
 * @scope: where to install the profile, e.g. %CD_EDID_SCOPE_USER
 * @edid: the EDID data, typically just 128 bytes in size
 * @edid_len: the size in bytes of @edid_len
 * @profile_fn: the profile filename to install
 *
 * Install a profile for a given monitor.
 *
 * Return value: a %CdEdidError, e.g. %CD_EDID_ERROR_OK
 *
 * Since: 0.1.34
 **/
CdEdidError
cd_edid_install_profile (unsigned char *edid,
			 int edid_len,
			 CdEdidScope scope,
			 char *profile_fn)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *md5 = NULL;
	g_autoptr(CdClient) client = NULL;
	g_autoptr(CdDevice) device = NULL;
	g_autoptr(CdProfile) profile = NULL;
	g_autoptr(GFile) file = NULL;

	g_return_val_if_fail (profile_fn != NULL, CD_EDID_ERROR_RESOURCE);

	/* bad input */
	if (edid == NULL || edid_len == 0)
		return CD_EDID_ERROR_NO_DATA;

	/* connect to daemon */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	if (!ret) {
		g_printerr ("Failed to connect to colord: %s", error->message);
		return CD_EDID_ERROR_ACCESS_CONFIG;
	}

	/* find device that matches the output EDID */
	md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5,
					     (const gchar *)edid,
					     (gssize) edid_len);
	device = cd_client_find_device_by_property_sync (client,
							 CD_DEVICE_METADATA_OUTPUT_EDID_MD5,
							 md5,
							 NULL,
							 &error);
	if (device == NULL) {
		g_printerr ("Failed to find device that matches %s: %s",
			    md5, error->message);
		return CD_EDID_ERROR_MONITOR_NOT_FOUND;
	}

	/* read device properties */
	ret = cd_device_connect_sync (device, NULL, &error);
	if (!ret) {
		g_printerr ("device disappeared: %s", error->message);
		return CD_EDID_ERROR_ACCESS_CONFIG;
	}

	/* import profile */
	file = g_file_new_for_path (profile_fn);
	profile = cd_client_import_profile_sync (client, file, NULL, &error);
	if (profile == NULL &&
	    g_error_matches (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_ALREADY_EXISTS)) {
		g_clear_error (&error);
		profile = cd_client_find_profile_by_property_sync (client,
								   CD_PROFILE_PROPERTY_FILENAME,
								   profile_fn,
								   NULL,
								   &error);
	}
	if (profile == NULL) {
		g_printerr ("Could not import profile %s: %s",
			    profile_fn,
			    error->message);
		return CD_EDID_ERROR_NO_PROFILE;
	}

	/* read profile properties */
	ret = cd_profile_connect_sync (profile, NULL, &error);
	if (!ret) {
		g_printerr ("profile disappeared: %s", error->message);
		return CD_EDID_ERROR_ACCESS_CONFIG;
	}

	/* add profile to device */
	ret = cd_device_add_profile_sync (device,
					  CD_DEVICE_RELATION_HARD,
					  profile,
					  NULL,
					  &error);
	if (!ret) {
		if (g_error_matches (error,
				     CD_DEVICE_ERROR,
				     CD_DEVICE_ERROR_PROFILE_ALREADY_ADDED)) {
			/* ignore this */
			g_clear_error (&error);
		} else {
			g_printerr ("could not add profile %s to device %s: %s",
				    cd_profile_get_id(profile),
				    cd_device_get_id(device),
				    error->message);
			return CD_EDID_ERROR_SET_CONFIG;
		}
	}

	/* make default */
	ret = cd_device_make_profile_default_sync (device,
						   profile,
						   NULL,
						   &error);
	if (!ret) {
		g_printerr ("could not add default profile %s to device %s: %s",
			    cd_profile_get_id(profile),
			    cd_device_get_id(device),
			    error->message);
		return CD_EDID_ERROR_SET_CONFIG;
	}

	/* install system-wide */
	if (scope == CD_EDID_SCOPE_SYSTEM) {
		ret = cd_profile_install_system_wide_sync (profile, NULL, &error);
		if (!ret) {
			g_printerr ("could not set profile %s systemwide: %s",
				    cd_profile_get_id (profile),
				    error->message);
			return CD_EDID_ERROR_PROFILE_COPY;
		}
	}
	return CD_EDID_ERROR_OK;
}

/**
 * cd_edid_remove_profile:
 * @edid: the EDID data, typically just 128 bytes in size
 * @edid_len: the size in bytes of @edid_len
 * @profile_fn: the profile filename to remove
 *
 * Un-install a profile for a given monitor.
 *
 * Return value: a %CdEdidError, e.g. %CD_EDID_ERROR_OK
 *
 * Since: 0.1.34
 **/
CdEdidError
cd_edid_remove_profile (unsigned char *edid,
			int edid_len,
			char *profile_fn)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *md5 = NULL;
	g_autoptr(CdClient) client = NULL;
	g_autoptr(CdDevice) device = NULL;
	g_autoptr(CdProfile) profile = NULL;
	g_autoptr(GFile) file = NULL;

	g_return_val_if_fail (profile_fn != NULL, CD_EDID_ERROR_RESOURCE);

	/* bad input */
	if (edid == NULL || edid_len == 0)
		return CD_EDID_ERROR_NO_DATA;

	/* connect to daemon */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	if (!ret) {
		g_printerr ("Failed to connect to colord: %s", error->message);
		return CD_EDID_ERROR_ACCESS_CONFIG;
	}

	/* find device that matches the output EDID */
	md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5,
					     (const gchar *)edid,
					     (gssize) edid_len);
	device = cd_client_find_device_by_property_sync (client,
							 CD_DEVICE_METADATA_OUTPUT_EDID_MD5,
							 md5,
							 NULL,
							 &error);
	if (device == NULL) {
		g_printerr ("Failed to find device that matches %s: %s",
			    md5, error->message);
		return CD_EDID_ERROR_MONITOR_NOT_FOUND;
	}

	/* read device properties */
	ret = cd_device_connect_sync (device, NULL, &error);
	if (!ret) {
		g_printerr ("device disappeared: %s", error->message);
		return CD_EDID_ERROR_ACCESS_CONFIG;
	}

	/* find profile */
	file = g_file_new_for_path (profile_fn);
	profile = cd_client_find_profile_by_filename_sync (client,
							   profile_fn,
							   NULL,
							   &error);
	if (profile == NULL) {
		g_printerr ("Could not find profile %s: %s",
			    profile_fn,
			    error->message);
		return CD_EDID_ERROR_NO_PROFILE;
	}

	/* read profile properties */
	ret = cd_profile_connect_sync (profile, NULL, &error);
	if (!ret) {
		g_printerr ("profile disappeared: %s", error->message);
		return CD_EDID_ERROR_ACCESS_CONFIG;
	}

	/* remove profile from device */
	ret = cd_device_remove_profile_sync (device,
					     profile,
					     NULL,
					     &error);
	if (!ret) {
		g_printerr ("could not remove profile %s from device %s: %s",
			    cd_profile_get_id (profile),
			    cd_device_get_id (device),
			    error->message);
		return CD_EDID_ERROR_SET_CONFIG;
	}
	return CD_EDID_ERROR_OK;
}

/**
 * cd_edid_get_profile:
 * @edid: the EDID data, typically just 128 bytes in size
 * @edid_len: the size in bytes of @edid_len
 * @profile_fn: the returned profile filename, use free() when done
 *
 * Get an associated monitor profile.
 *
 * Return value: a %CdEdidError, e.g. %CD_EDID_ERROR_OK
 *
 * Since: 0.1.34
 **/
CdEdidError
cd_edid_get_profile (unsigned char *edid,
		     int edid_len,
		     char **profile_fn)
{
	const gchar *filename;
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *md5 = NULL;
	g_autoptr(CdClient) client = NULL;
	g_autoptr(CdDevice) device = NULL;
	g_autoptr(CdProfile) profile = NULL;

	/* bad input */
	if (edid == NULL || edid_len == 0)
		return CD_EDID_ERROR_NO_DATA;

	/* connect to daemon */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	if (!ret) {
		g_printerr ("Failed to connect to colord: %s", error->message);
		return CD_EDID_ERROR_ACCESS_CONFIG;
	}

	/* find device that matches the output EDID */
	md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5,
					     (const gchar *)edid,
					     (gssize) edid_len);
	device = cd_client_find_device_by_property_sync (client,
							 CD_DEVICE_METADATA_OUTPUT_EDID_MD5,
							 md5,
							 NULL,
							 &error);
	if (device == NULL) {
		g_printerr ("Failed to find device that matches %s: %s",
			    md5, error->message);
		return CD_EDID_ERROR_MONITOR_NOT_FOUND;
	}

	/* read device properties */
	ret = cd_device_connect_sync (device, NULL, &error);
	if (!ret) {
		g_printerr ("device disappeared: %s", error->message);
		return CD_EDID_ERROR_ACCESS_CONFIG;
	}

	/* get the default profile for the device */
	profile = cd_device_get_default_profile (device);
	if (profile == NULL) {
		g_printerr ("No profile for %s",
			    cd_device_get_id (device));
		return CD_EDID_ERROR_NO_PROFILE;
	}

	/* read profile properties */
	ret = cd_profile_connect_sync (profile, NULL, &error);
	if (!ret) {
		g_printerr ("profile disappeared: %s", error->message);
		return CD_EDID_ERROR_ACCESS_CONFIG;
	}

	/* get filename */
	filename = cd_profile_get_filename (profile);
	if (filename == NULL)
		return CD_EDID_ERROR_INVALID_PROFILE;

	/* return filename of profile */
	if (profile_fn != NULL)
		*profile_fn = strdup (filename);
	return CD_EDID_ERROR_OK;
}
