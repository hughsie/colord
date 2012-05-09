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

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "../libcolord/cd-color.h"
#include "../libcolord/cd-profile.h"
#include "cd-sample-window.h"
#include "cd-window.h"

static void
cd_window_get_profile_cb (GObject *source,
			  GAsyncResult *res,
			  gpointer user_data)
{
	CdWindow *window = CD_WINDOW (source);
	GtkWidget *widget = GTK_WIDGET (user_data);
	GError *error = NULL;
	CdProfile *profile;

	profile = cd_window_get_profile_finish (window,
					        res,
					        &error);
	g_assert_no_error (error);
	g_assert (profile != NULL);
	g_debug ("profile was %s", cd_profile_get_filename (profile));
	g_object_unref (profile);

	/* kill the dialog */
	gtk_widget_destroy (widget);
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

static void
colord_window_func (void)
{
	CdWindow *window;
	GtkWidget *dialog;

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
	g_object_unref (window);
}

static gboolean
colord_sample_window_loop_cb (GMainLoop *loop)
{
	g_main_loop_quit (loop);
	return FALSE;
}

static void
colord_sample_window_func (void)
{
	GtkWindow *window;
	GMainLoop *loop;
	CdColorRGB source;

	window = cd_sample_window_new ();
	g_assert (window != NULL);
	source.R = 1.0f;
	source.G = 1.0f;
	source.B = 0.0f;
	cd_sample_window_set_color (CD_SAMPLE_WINDOW (window), &source);
	cd_sample_window_set_fraction (CD_SAMPLE_WINDOW (window), -1);

	/* move to the center of device lvds1 */
	gtk_window_present (window);

	loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add_seconds (2, (GSourceFunc) colord_sample_window_loop_cb, loop);
	g_main_loop_run (loop);

	g_main_loop_unref (loop);
	gtk_widget_destroy (GTK_WIDGET (window));
}

int
main (int argc, char **argv)
{
	g_type_init ();
	g_test_init (&argc, &argv, NULL);
	gtk_init (&argc, &argv);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func ("/colord/window", colord_window_func);
	g_test_add_func ("/colors/sample-window", colord_sample_window_func);
	return g_test_run ();
}

