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

//gcc -o cd-gtk-demo cd-gtk-demo.c `pkg-config --cflags --libs colord-gtk` -Wall

#include <stdlib.h>
#include <colord-gtk.h>
#include <gtk/gtk.h>

static void
cd_window_get_profile_cb (GObject *source,
			  GAsyncResult *res,
			  gpointer user_data)
{
	CdProfile *profile;
	CdWindow *window = CD_WINDOW (source);
	GError *error = NULL;

	profile = cd_window_get_profile_finish (window,
					        res,
					        &error);
	if (profile == NULL) {
		g_warning ("failed to get output profile: %s", error->message);
		g_error_free (error);
		return;
	}

	g_debug ("screen profile to use %s", cd_profile_get_filename (profile));
	g_object_unref (profile);
}

static void
map_cb (GtkWidget *this_widget, gpointer user_data)
{
	CdWindow *window = CD_WINDOW (user_data);

	/* get the profile for this widget */
	cd_window_get_profile (window,
			       this_widget,
			       NULL,
			       cd_window_get_profile_cb,
			       this_widget);
}

int
main (int argc, char *argv[])
{
	CdWindow *window;
	gint retval = EXIT_FAILURE;
	GtkWidget *dialog;

	gtk_init (&argc, &argv);
	window = cd_window_new ();
	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_INFO,
					 GTK_BUTTONS_OK,
					 "%s", "Hello world");
	g_signal_connect (dialog, "map",
			  G_CALLBACK (map_cb),
			  window);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	g_object_unref (window);

	retval = EXIT_SUCCESS;
	return retval;
}
