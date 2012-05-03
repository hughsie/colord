/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
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

//gcc -o cd-libcolord-find-device-by-property cd-libcolord-find-device-by-property.c `pkg-config --cflags --libs colord` -Wall

#include <stdlib.h>
#include <colord.h>

int
main (int argc, char *argv[])
{
        CdClient *client = NULL;
        CdDevice *device = NULL;
        CdProfile *profile = NULL;
        const gchar *filename;
        gboolean ret;
        gint retval = EXIT_FAILURE;
        GError *error = NULL;

        /* check user sanity */
        if (argc < 2) {
                g_warning ("usage: cd-libcolord-find-device-by-property.c <output-name>, e.g. LVDS1");
                goto out;
        }

        /* setup GType system */
        g_type_init ();

        /* connects to the daemon */
        client = cd_client_new ();
        ret = cd_client_connect_sync (client, NULL, &error);
        if (!ret) {
                g_warning ("failed to contact colord: %s", error->message);
                g_error_free (error);
                goto out;
        }

        /* finds the colord device which has a specific property */
        device = cd_client_find_device_by_property_sync (client,
                                                         CD_DEVICE_METADATA_XRANDR_NAME,
                                                         argv[1],
                                                         NULL,
                                                         &error);
        if (device == NULL) {
                g_warning ("no device with that property: %s", error->message);
                g_error_free (error);
                goto out;
        }
   
        /* get details about the device */
        ret = cd_device_connect_sync (device, NULL, &error);
        if (!ret) {
                g_warning ("failed to get properties from the device: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* get the default profile for the device */
        profile = cd_device_get_default_profile (device);
        if (profile == NULL) {
                g_warning ("no default profile for device: %s", argv[1]);
                goto out;
        }

        /* get details about the profile */
        ret = cd_profile_connect_sync (profile, NULL, &error);
        if (!ret) {
                g_warning ("failed to get properties from the profile: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* get the filename of the profile */
        filename = cd_profile_get_filename (profile);
        if (filename == NULL) {
                g_warning ("profile has no physical file, must be virtual");
                goto out;
        }

        /* SUCCESS! */
        g_debug ("profile filename for %s: %s", argv[1], filename);
        retval = EXIT_SUCCESS;

out:
        if (device != NULL)
                g_object_unref (device);
        if (profile != NULL)
                g_object_unref (profile);
        if (client != NULL)
                g_object_unref (client);
        return retval;
}
