/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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

#include <colord-private.h>
#include <colord-gtk.h>
#include <gio/gio.h>
#include <glib.h>
#include <math.h>
#include <stdlib.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-rr.h>

typedef struct {
	GnomeRROutput		*output;
	GnomeRRScreen		*x11_screen;
	guint			 gamma_size;
	GMainLoop		*loop;
	GtkWidget		*sample_widget;
	GtkBuilder		*builder;
	CdDevice		*device;
	GDBusProxy		*proxy;
} CdExamplePrivate;

static guint
gnome_rr_output_get_gamma_size (GnomeRROutput *output)
{
	GnomeRRCrtc *crtc;
	gint len = 0;

	crtc = gnome_rr_output_get_crtc (output);
	if (crtc == NULL)
		return 0;
	gnome_rr_crtc_get_gamma (crtc,
				 &len,
				 NULL, NULL, NULL);
	return (guint) len;
}

/**
 * cd_example_calib_setup_screen:
 **/
static gboolean
cd_example_calib_setup_screen (CdExamplePrivate *priv,
			       const gchar *name,
			       GError **error)
{
	gboolean ret = TRUE;

	/* get screen */
	priv->x11_screen = gnome_rr_screen_new (gdk_screen_get_default (), error);
	if (priv->x11_screen == NULL) {
		ret = FALSE;
		goto out;
	}

	/* get the output */
	priv->output = gnome_rr_screen_get_output_by_name (priv->x11_screen,
							   name);
	if (priv->output == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "failed to get output");
		goto out;
	}

	/* create a lookup table */
	priv->gamma_size = gnome_rr_output_get_gamma_size (priv->output);
	if (priv->gamma_size == 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "gamma size is zero");
		goto out;
	}
out:
	return ret;
}

/**
 * cd_example_calib_set_output_gamma:
 **/
static gboolean
cd_example_calib_set_output_gamma (CdExamplePrivate *priv,
				   GPtrArray *array,
				   GError **error)
{
	CdColorRGB *p1;
	CdColorRGB *p2;
	CdColorRGB result;
	gboolean ret = TRUE;
	gdouble mix;
	GnomeRRCrtc *crtc;
	guint16 *blue = NULL;
	guint16 *green = NULL;
	guint16 *red = NULL;
	guint i;

	/* no length? */
	if (array->len == 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "no data in the CLUT array");
		goto out;
	}

	/* convert to a type X understands of the right size */
	red = g_new (guint16, priv->gamma_size);
	green = g_new (guint16, priv->gamma_size);
	blue = g_new (guint16, priv->gamma_size);
	cd_color_rgb_set (&result, 1.0, 1.0, 1.0);
	for (i = 0; i < priv->gamma_size; i++) {
		mix = (gdouble) (array->len - 1) /
			(gdouble) (priv->gamma_size - 1) *
			(gdouble) i;
		p1 = g_ptr_array_index (array, (guint) floor (mix));
		p2 = g_ptr_array_index (array, (guint) ceil (mix));
		cd_color_rgb_interpolate (p1,
					  p2,
					  mix - (gint) mix,
					  &result);
		red[i] = result.R * 0xffff;
		green[i] = result.G * 0xffff;
		blue[i] = result.B * 0xffff;
	}

	/* send to LUT */
	crtc = gnome_rr_output_get_crtc (priv->output);
	if (crtc == NULL) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to get ctrc for %s",
			     gnome_rr_output_get_name (priv->output));
		goto out;
	}
	gnome_rr_crtc_set_gamma (crtc, priv->gamma_size,
				 red, green, blue);
out:
	g_free (red);
	g_free (green);
	g_free (blue);
	return ret;
}

/**
 * cd_example_property_changed_cb:
 **/
static void
cd_example_property_changed_cb (GDBusProxy *proxy,
				GVariant *changed_properties,
				GStrv invalidated_properties,
				CdExamplePrivate *priv)
{
	const gchar *key;
	GVariantIter *iter;
	GVariant *value;
	GtkWidget *widget;

	if (g_variant_n_children (changed_properties) > 0) {
		g_variant_get (changed_properties,
				"a{sv}",
				&iter);
		while (g_variant_iter_loop (iter, "{&sv}", &key, &value)) {
			if (g_strcmp0 (key, "Progress") == 0) {
				widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
									     "progressbar_status"));
				gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (widget),
							       g_variant_get_uint32 (value) / 100.0f);
			}
		}
		g_variant_iter_free (iter);
	}
}

typedef enum {
	CD_MAIN_INTERACTION_CODE_ATTACH_TO_SCREEN,
	CD_MAIN_INTERACTION_CODE_MOVE_TO_CALIBRATION,
	CD_MAIN_INTERACTION_CODE_MOVE_TO_SURFACE,
	CD_MAIN_INTERACTION_CODE_NONE
} CdMainInteractionCode;

/**
 * cd_example_interaction_required:
 **/
static void
cd_example_interaction_required (CdExamplePrivate *priv,
				 CdMainInteractionCode code,
				 const gchar *message,
				 const gchar *image)
{
	GtkLabel *label;
	GtkImage *img;
	GdkPixbuf *pixbuf;

	/* set image */
	img = GTK_IMAGE (gtk_builder_get_object (priv->builder,
						 "image_status"));
	if (image != NULL && image[0] != '\0') {
		g_debug ("showing image %s", image);
		pixbuf = gdk_pixbuf_new_from_file_at_size (image,
							   400, 400,
							   NULL);
		if (pixbuf != NULL) {
			gtk_image_set_from_pixbuf (img, pixbuf);
			g_object_unref (pixbuf);
		}
		gtk_widget_set_visible (GTK_WIDGET (img), TRUE);
		gtk_widget_set_visible (GTK_WIDGET (priv->sample_widget), FALSE);
	} else {
		g_debug ("hiding image");
		gtk_widget_set_visible (GTK_WIDGET (img), FALSE);
		gtk_widget_set_visible (GTK_WIDGET (priv->sample_widget), TRUE);
	}
	label = GTK_LABEL (gtk_builder_get_object (priv->builder,
						  "label_status"));
	gtk_label_set_label (label, message);
}

/**
 * cd_example_signal_cb:
 **/
static void
cd_example_signal_cb (GDBusProxy *proxy,
		      const gchar *sender_name,
		      const gchar *signal_name,
		      GVariant *parameters,
		      CdExamplePrivate *priv)
{
	CdColorRGB color;
	CdColorRGB *color_tmp;
	CdMainInteractionCode code;
	const gchar *image;
	const gchar *message;
	const gchar *profile_id = NULL;
	const gchar *profile_path = NULL;
	const gchar *str = NULL;
	gboolean ret;
	GError *error = NULL;
	GPtrArray *array = NULL;
	GtkImage *img;
	GtkLabel *label;
	GVariantIter *iter;
	GVariant *dict = NULL;

	if (g_strcmp0 (signal_name, "Finished") == 0) {

		g_variant_get (parameters, "(u@a{sv})", &code, &dict);
		g_variant_lookup (dict, "ErrorDetails", "&s", &str);
		g_variant_lookup (dict, "ProfileId", "&s", &profile_id);
		g_variant_lookup (dict, "ProfilePath", "&s", &profile_path);
		if (code == 0) {
			g_debug ("calibration succeeded with profile %s created at %s",
				 profile_id, profile_path);
		} else {
			g_warning ("calibration failed with code %i: %s",
				   code, str);
		}
		g_main_loop_quit (priv->loop);
		goto out;
	}
	if (g_strcmp0 (signal_name, "UpdateSample") == 0) {
		g_variant_get (parameters, "(ddd)",
			       &color.R,
			       &color.G,
			       &color.B);
		img = GTK_IMAGE (gtk_builder_get_object (priv->builder,
							 "image_status"));
		gtk_widget_set_visible (GTK_WIDGET (img), FALSE);
		gtk_widget_set_visible (GTK_WIDGET (priv->sample_widget), TRUE);
		cd_sample_widget_set_color (CD_SAMPLE_WIDGET (priv->sample_widget),
					    &color);
		/* set the generic label too */
		label = GTK_LABEL (gtk_builder_get_object (priv->builder,
							   "label_status"));
		gtk_label_set_label (label, "Do not disturb the calibration device while in progress");
		goto out;
	}
	if (g_strcmp0 (signal_name, "InteractionRequired") == 0) {
		g_variant_get (parameters, "(u&s&s)",
			       &code,
			       &message,
			       &image);
		g_print ("Interaction required type %i: %s\n",
			 code, message);
		cd_example_interaction_required (priv,
						 code,
						 message,
						 image);
		goto out;
	}
	if (g_strcmp0 (signal_name, "UpdateGamma") == 0) {
		g_variant_get (parameters,
				"(a(ddd))",
				&iter);
		array = g_ptr_array_new_with_free_func (g_free);
		while (g_variant_iter_loop (iter, "(ddd)",
					    &color.R,
					    &color.G,
					    &color.B)) {
			color_tmp = cd_color_rgb_new ();
			cd_color_rgb_copy (&color, color_tmp);
			g_ptr_array_add (array, color_tmp);
		}
		g_variant_iter_free (iter);
		ret = cd_example_calib_set_output_gamma (priv,
							 array,
							 &error);
		if (!ret) {
			g_warning ("failed to update gamma: %s",
				   error->message);
			g_error_free (error);
			goto out;
		}
		goto out;
	}
	g_warning ("got unknown signal %s", signal_name);
out:
	if (dict != NULL)
		g_variant_unref (dict);
}

/**
 * cd_example_move_and_resize_window:
 **/
static gboolean
cd_example_move_and_resize_window (GtkWindow *window,
				   CdDevice *device,
				   GError **error)
{
	const gchar *xrandr_name;
	gboolean ret = TRUE;
	gchar *plug_name;
	GdkRectangle rect;
	GdkScreen *screen;
	gint i;
	gint monitor_num = -1;
	gint num_monitors;

	/* find the monitor num of the device output */
	screen = gdk_screen_get_default ();
	num_monitors = gdk_screen_get_n_monitors (screen);
	xrandr_name = cd_device_get_metadata_item (device,
						   CD_DEVICE_METADATA_XRANDR_NAME);
	for (i = 0; i < num_monitors; i++) {
		plug_name = gdk_screen_get_monitor_plug_name (screen, i);
		if (g_strcmp0 (plug_name, xrandr_name) == 0)
			monitor_num = i;
		g_free (plug_name);
	}
	if (monitor_num == -1) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "failed to find output %s",
			     xrandr_name);
		goto out;
	}

	/* move the window, and set it to the right size */
	gdk_screen_get_monitor_geometry (screen, monitor_num, &rect);
	gtk_window_move (window, rect.x, rect.y);
	gtk_window_set_default_size (window, rect.width, rect.height);
	g_debug ("Setting window to %ix%i with size %ix%i",
		 rect.x, rect.y, rect.width, rect.height);
out:
	return ret;
}

/**
 * cd_example_window_realize_cb:
 **/
static void
cd_example_window_realize_cb (GtkWidget *widget, CdExamplePrivate *priv)
{
	gtk_window_fullscreen (GTK_WINDOW (widget));
}

/**
 * cd_example_window_realize_cb:
 **/
static gboolean
cd_example_window_state_cb (GtkWidget *widget,
			    GdkEvent *event,
			    CdExamplePrivate *priv)
{
	gboolean ret;
	GError *error = NULL;
	GdkEventWindowState *event_state = (GdkEventWindowState *) event;
	GtkWindow *window = GTK_WINDOW (widget);

	/* check event */
	if (event->type != GDK_WINDOW_STATE)
		return TRUE;
	if (event_state->changed_mask != GDK_WINDOW_STATE_FULLSCREEN)
		return TRUE;

	/* resize to the correct screen */
	ret = cd_example_move_and_resize_window (window,
						 priv->device,
						 &error);

	if (!ret) {
		g_warning ("Failed to resize window: %s", error->message);
		g_error_free (error);
	}
	return TRUE;
}

/**
 * cd_example_button_start_cb:
 **/
static void
cd_example_button_start_cb (GtkWidget *widget, CdExamplePrivate *priv)
{
	GVariant *retval;
	GError *error = NULL;

	/* continue */
	retval = g_dbus_proxy_call_sync (priv->proxy,
					 "Resume",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	if (retval == NULL) {
		g_warning ("Failed to send Resume: %s", error->message);
		goto out;
	}
out:
	if (retval != NULL)
		g_variant_unref (retval);
}

/**
 * cd_example_button_cancel_cb:
 **/
static void
cd_example_button_cancel_cb (GtkWidget *widget, CdExamplePrivate *priv)
{
	g_main_loop_quit (priv->loop);
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	CdClient *client = NULL;
	CdExamplePrivate *priv = NULL;
	const gchar *name;
	gboolean ret;
	gchar *device_id = NULL;
	gchar *quality = NULL;
	gchar *sensor_id = NULL;
	gchar *title = NULL;
	GDBusConnection *connection = NULL;
	GError *error = NULL;
	gint retval = EXIT_FAILURE;
	GOptionContext *context;
	GtkBox *box;
	GtkSettings *settings;
	GtkWidget *widget;
	GtkWindow *window;
	guint quality_value = 0;
	guint whitepoint = 0;
	GVariantBuilder builder;
	GVariant *retvax = NULL;

	const GOptionEntry options[] = {
		{ "device", '\0', 0, G_OPTION_ARG_STRING, &device_id,
			/* TRANSLATORS: command line option */
			"Use this device for profiling", NULL },
		{ "sensor", '\0', 0, G_OPTION_ARG_STRING, &sensor_id,
			/* TRANSLATORS: command line option */
			"Use this sensor for profiling", NULL },
		{ "title", '\0', 0, G_OPTION_ARG_STRING, &title,
			/* TRANSLATORS: command line option */
			"Use this title for the profile", NULL },
		{ "quality", '\0', 0, G_OPTION_ARG_STRING, &quality,
			/* TRANSLATORS: command line option */
			"Use this quality setting: low,medium,high", NULL },
		{ "whitepoint", '\0', 0, G_OPTION_ARG_INT, &whitepoint,
			/* TRANSLATORS: command line option */
			"Target this specific whitepoint, or 0 for native", NULL },
		{ NULL}
	};

	gtk_init (&argc, &argv);

	context = g_option_context_new ("colord-session example client");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	ret = g_option_context_parse (context, &argc, &argv, &error);
	if (!ret)
		goto out;

	priv = g_new0 (CdExamplePrivate, 1);
	priv->loop = g_main_loop_new (NULL, FALSE);
	priv->sample_widget = cd_sample_widget_new ();

	/* set the dark theme */
	settings = gtk_settings_get_default ();
	g_object_set (G_OBJECT (settings),
		      "gtk-application-prefer-dark-theme", TRUE,
		      NULL);

	/* load UI */
	priv->builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (priv->builder,
					    "./cd-example.ui",
					    &error);
	if (retval == 0) {
		ret = FALSE;
		goto out;
	}

	/* get xrandr device name */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	if (!ret)
		goto out;
	if (device_id == NULL) {
		ret = FALSE;
		g_set_error (&error, 1, 0, "--device is required");
		goto out;
	}
	priv->device = cd_client_find_device_sync (client,
						   device_id,
						   NULL,
						   &error);
	if (priv->device == NULL) {
		ret = FALSE;
		goto out;
	}
	ret = cd_device_connect_sync (priv->device,
				      NULL,
				      &error);
	if (!ret)
		goto out;
	name = cd_device_get_metadata_item (priv->device,
					    CD_DEVICE_METADATA_XRANDR_NAME);

	/* get sensor */
	if (sensor_id == NULL) {
		ret = FALSE;
		g_set_error (&error, 1, 0, "--sensor is required");
		goto out;
	}

	/* get screen */
	ret = cd_example_calib_setup_screen (priv, name, &error);
	if (!ret)
		goto out;

	/* parse quality string */
	if (g_strcmp0 (quality, "low") == 0) {
		quality_value = 0;
	} else if (g_strcmp0 (quality, "medium") == 0) {
		quality_value = 1;
	} else if (g_strcmp0 (quality, "high") == 0) {
		quality_value = 2;
	} else {
		ret = FALSE;
		g_set_error (&error, 1, 0, "--quality value not known");
		goto out;
	}

	/* check title */
	if (title == NULL) {
		ret = FALSE;
		g_set_error (&error, 1, 0, "--title is required");
		goto out;
	}

	/* start the calibration session daemon */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (connection == NULL) {
		ret = FALSE;
		goto out;
	}
	priv->proxy = g_dbus_proxy_new_sync (connection,
					     G_DBUS_PROXY_FLAGS_NONE,
					     NULL,
					     "org.freedesktop.ColorHelper",
					     "/",
					     "org.freedesktop.ColorHelper.Display",
					     NULL,
					     &error);
	if (priv->proxy == NULL) {
		ret = FALSE;
		goto out;
	}
	g_signal_connect (priv->proxy,
			  "g-properties-changed",
			  G_CALLBACK (cd_example_property_changed_cb),
			  priv);
	g_signal_connect (priv->proxy,
			  "g-signal",
			  G_CALLBACK (cd_example_signal_cb),
			  priv);
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add (&builder,
			       "{sv}",
			       "Quality",
			       g_variant_new_uint32 (quality_value));
	g_variant_builder_add (&builder,
			       "{sv}",
			       "Whitepoint",
			       g_variant_new_uint32 (whitepoint));
	g_variant_builder_add (&builder,
			       "{sv}",
			       "Title",
			       g_variant_new_string (title));
	g_variant_builder_add (&builder,
			       "{sv}",
			       "DeviceKind",
			       g_variant_new_uint32 (CD_SENSOR_CAP_LCD));
	retvax = g_dbus_proxy_call_sync (priv->proxy,
					 "Start",
					 g_variant_new ("(ssa{sv})",
							device_id,
							sensor_id,
							&builder),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	if (retvax == NULL) {
		ret = FALSE;
		goto out;
	}

	/* add sample widget */
	box = GTK_BOX (gtk_builder_get_object (priv->builder,
					       "vbox_status"));
	priv->sample_widget = cd_sample_widget_new ();
	gtk_widget_set_size_request (priv->sample_widget, 400, 400);
	gtk_box_pack_start (box, priv->sample_widget, FALSE, FALSE, 0);
	gtk_box_reorder_child (box, priv->sample_widget, 0);
	gtk_widget_set_vexpand (priv->sample_widget, FALSE);
	gtk_widget_set_hexpand (priv->sample_widget, FALSE);

	/* connect to buttons */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "button_start"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cd_example_button_start_cb), priv);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
						     "button_cancel"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cd_example_button_cancel_cb), priv);
	gtk_widget_show (widget);

	/* move to the right screen */
	window = GTK_WINDOW (gtk_builder_get_object (priv->builder,
						     "dialog_calibrate"));
	g_signal_connect (window, "realize",
			  G_CALLBACK (cd_example_window_realize_cb), priv);
	g_signal_connect (window, "window-state-event",
			  G_CALLBACK (cd_example_window_state_cb), priv);
	gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);
	gtk_window_set_keep_above (window, TRUE);
	gtk_widget_show (GTK_WIDGET (window));
	g_main_loop_run (priv->loop);

	/* success */
	retval = EXIT_SUCCESS;
out:
	if (!ret) {
		g_print ("%s: %s\n",
			 "Failed to calibrate",
			 error->message);
		g_error_free (error);
	}
	g_option_context_free (context);
	if (priv != NULL) {
		if (priv->loop != NULL)
			g_main_loop_unref (priv->loop);
		if (priv->x11_screen != NULL)
			g_object_unref (priv->x11_screen);
		if (priv->device != NULL)
			g_object_unref (priv->device);
		if (priv->proxy != NULL)
			g_object_unref (priv->proxy);
		g_free (priv);
	}
	if (client != NULL)
		g_object_unref (client);
	if (connection != NULL)
		g_object_unref (connection);
	if (retvax != NULL)
		g_variant_unref (retvax);
	g_free (device_id);
	g_free (sensor_id);
	g_free (quality);
	g_free (title);
	return retval;
}
