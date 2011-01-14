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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <locale.h>
#include <colord.h>

static gchar *current_device = NULL;
static gchar *current_profile = NULL;
static GDBusConnection *connection = NULL;
static GDBusProxy *proxy = NULL;
static GtkBuilder *builder = NULL;
static gboolean create_profile = FALSE;

enum {
	CD_COLUMN_DEVICES_OBJECT_PATH,
	CD_COLUMN_DEVICES_TITLE,
	CD_COLUMN_DEVICES_LAST
};

enum {
	CD_COLUMN_PROFILE_OBJECT_PATH,
	CD_COLUMN_PROFILE_TITLE,
	CD_COLUMN_PROFILE_LAST
};

enum {
	CD_COLUMN_DEVICE_PROFILES_OBJECT_PATH,
	CD_COLUMN_DEVICE_PROFILES_TITLE,
	CD_COLUMN_DEVICE_PROFILES_LAST
};

/**
 * cd_gui_create_device_cb:
 **/
static void
cd_gui_create_device_cb (GObject *source_object,
			 GAsyncResult *res,
			 gpointer user_data)
{
	GVariant *result;
	GError *error = NULL;
	GtkWidget *widget;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res, &error);
	if (result == NULL) {
		g_warning ("Error creating device: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	/* hide window */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_create"));
	gtk_widget_hide (widget);

	g_variant_unref (result);
}

/**
 * cd_gui_create_profile_cb:
 **/
static void
cd_gui_create_profile_cb (GObject *source_object,
			  GAsyncResult *res,
			  gpointer user_data)
{
	GVariant *result;
	GError *error = NULL;
	GtkWidget *widget;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res, &error);
	if (result == NULL) {
		g_warning ("Error creating profile: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	/* hide window */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_create"));
	gtk_widget_hide (widget);

	g_variant_unref (result);
}

/**
 * cd_gui_button_device_add_cb:
 **/
static void
cd_gui_button_device_add_cb (GtkWidget *widget, gpointer user_data)
{
	g_debug ("device add button");

	create_profile = FALSE;

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_qualifier"));
	gtk_widget_set_visible (widget, FALSE);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_create"));
	gtk_window_present (GTK_WINDOW (widget));
}

/**
 * cd_gui_button_profile_add_cb:
 **/
static void
cd_gui_button_profile_add_cb (GtkWidget *widget, gpointer user_data)
{
	g_debug ("profile add button");

	create_profile = TRUE;

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_qualifier"));
	gtk_widget_set_visible (widget, TRUE);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_create"));
	gtk_window_present (GTK_WINDOW (widget));
}

/**
 * cd_gui_button_create_cancel_cb:
 **/
static void
cd_gui_button_create_cancel_cb (GtkWidget *widget, gpointer user_data)
{
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_create"));
	gtk_widget_hide (widget);
}

/**
 * cd_gui_button_device_profile_add_cb:
 **/
static void
cd_gui_button_device_profile_add_cb (GtkWidget *widget, gpointer user_data)
{
//	GDBusProxy *proxy;
//	GError *error = NULL;

	g_debug ("Add %s to %s",
		 current_profile, current_device);
#if 0
	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  COLORD_DBUS_SERVICE,
				  current_profile,
				  COLORD_DBUS_INTERFACE_PROFILE,
				  NULL,
				  cd_gui_got_profile_proxy_full_cb2,
				  NULL,
				  &error);
	if (proxy == NULL) {
		g_warning ("failed to add profile: %s",
			   error->message);
		g_error_free (error);
		return;
	}
#endif
	g_object_unref (proxy);
}

/**
 * cd_gui_button_device_profile_remove_cb:
 **/
static void
cd_gui_button_device_profile_remove_cb (GtkWidget *widget, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	gboolean ret;
	gchar *id;

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_device_profiles"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	ret = gtk_tree_selection_get_selected (selection, &model, &iter);
	if (!ret)
		return;
	gtk_tree_model_get (model, &iter,
			    CD_COLUMN_DEVICE_PROFILES_OBJECT_PATH, &id,
			    -1);
	g_debug ("Remove %s from %s",
		 id, current_device);
	g_free (id);
}

/**
 * cd_gui_button_device_profile_make_default_cb:
 **/
static void
cd_gui_button_device_profile_make_default_cb (GtkWidget *widget, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	gboolean ret;
	gchar *id;

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_device_profiles"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	ret = gtk_tree_selection_get_selected (selection, &model, &iter);
	if (!ret)
		return;
	gtk_tree_model_get (model, &iter,
			    CD_COLUMN_DEVICE_PROFILES_OBJECT_PATH, &id,
			    -1);
	g_debug ("Make %s default on %s",
		 id, current_device);
	g_free (id);
}

/**
 * cd_gui_button_create_cb:
 **/
static void
cd_gui_button_create_cb (GtkWidget *widget, gpointer user_data)
{
	const gchar *id;
	const gchar *qualifier;
	guint options = G_MAXUINT;

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "entry_id"));
	id = gtk_entry_get_text (GTK_ENTRY (widget));

	/* get radio options */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "radiobutton_create_normal"));
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		options = CD_OBJECT_SCOPE_NORMAL;
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "radiobutton_create_temp"));
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		options = CD_OBJECT_SCOPE_TEMPORARY;
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "radiobutton_create_disk"));
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		options = CD_OBJECT_SCOPE_DISK;

	/* only valid for profiles */
	if (create_profile) {
		g_debug ("create profile");
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "entry_qualifier"));
		qualifier = gtk_entry_get_text (GTK_ENTRY (widget));

		g_dbus_proxy_call (proxy,
				   "CreateProfile",
				   g_variant_new ("(su)",
						  id,
						  options),
				   G_DBUS_CALL_FLAGS_NONE,
				   -1,
				   NULL,
				   cd_gui_create_profile_cb,
				   NULL);

	} else {
		g_debug ("create device");
		g_dbus_proxy_call (proxy,
				   "CreateDevice",
				   g_variant_new ("(su)",
						  id,
						  options),
				   G_DBUS_CALL_FLAGS_NONE,
				   -1,
				   NULL,
				   cd_gui_create_device_cb,
				   NULL);
	}
}

/**
 * cd_gui_delete_device_cb:
 **/
static void
cd_gui_delete_device_cb (GObject *source_object,
			 GAsyncResult *res,
			 gpointer user_data)
{
	GVariant *result;
	GError *error = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res, &error);
	if (result == NULL) {
		g_warning ("Error deleting device: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	g_variant_unref (result);
}

/**
 * cd_gui_delete_profile_cb:
 **/
static void
cd_gui_delete_profile_cb (GObject *source_object,
			  GAsyncResult *res,
			  gpointer user_data)
{
	GVariant *result;
	GError *error = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res, &error);
	if (result == NULL) {
		g_warning ("Error deleting profile: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	g_variant_unref (result);
}

/**
 * cd_gui_button_device_remove_cb:
 **/
static void
cd_gui_button_device_remove_cb (GtkWidget *widget, gpointer user_data)
{
	if (current_device == NULL)
		return;
	g_debug ("remove %s", current_device);
	g_dbus_proxy_call (proxy,
			   "DeleteDevice",
			   g_variant_new ("(s)",
					  current_device), //FIXME: needs to be an ID
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   NULL,
			   cd_gui_delete_device_cb,
			   NULL);
}

/**
 * cd_gui_button_profile_remove_cb:
 **/
static void
cd_gui_button_profile_remove_cb (GtkWidget *widget, gpointer user_data)
{
	if (current_profile == NULL)
		return;
	g_dbus_proxy_call (proxy,
			   "DeleteProfile",
			   g_variant_new ("(s)",
					  current_profile),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   NULL,
			   cd_gui_delete_profile_cb,
			   NULL);
}

/**
 * cd_gui_treeview_add_device_columns:
 **/
static void
cd_gui_treeview_add_device_columns (GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	/* TRANSLATORS: column for the source description */
	column = gtk_tree_view_column_new_with_attributes (_("Device"), renderer,
							   "markup", CD_COLUMN_DEVICES_TITLE, NULL);
	gtk_tree_view_column_set_sort_column_id (column, CD_COLUMN_DEVICES_TITLE);
	gtk_tree_view_append_column (treeview, column);
}

/**
 * cd_gui_treeview_add_device_profile_columns:
 **/
static void
cd_gui_treeview_add_device_profile_columns (GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	/* TRANSLATORS: column for the source description */
	column = gtk_tree_view_column_new_with_attributes (_("Profiles"), renderer,
							   "markup", CD_COLUMN_DEVICE_PROFILES_TITLE, NULL);
	gtk_tree_view_column_set_sort_column_id (column,
						 CD_COLUMN_DEVICE_PROFILES_TITLE);
	gtk_tree_view_append_column (treeview, column);
}

/**
 * cd_gui_treeview_add_profile_columns:
 **/
static void
cd_gui_treeview_add_profile_columns (GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	/* TRANSLATORS: column for the source description */
	column = gtk_tree_view_column_new_with_attributes (_("Title"), renderer,
							   "markup", CD_COLUMN_PROFILE_TITLE, NULL);
	gtk_tree_view_column_set_sort_column_id (column, CD_COLUMN_PROFILE_TITLE);
	gtk_tree_view_append_column (treeview, column);
}

/**
 * cd_gui_got_profile_proxy_full_cb:
 **/
static void
cd_gui_got_profile_proxy_full_cb (GObject *source_object,
			     GAsyncResult *res,
			     gpointer user_data)
{
	gchar *title = NULL;
	GDBusProxy *proxy_tmp;
	GError *error = NULL;
	GtkListStore *liststore_profiles;
	GtkTreeIter iter;
	GVariant *variant_title = NULL;

	proxy_tmp = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (proxy_tmp == NULL) {
		g_warning ("Error creating proxy: %s", error->message);
		g_error_free (error);
		return;
	}

	/* get title */
	variant_title = g_dbus_proxy_get_cached_property (proxy_tmp, "Title");
	if (variant_title != NULL)
		title = g_variant_dup_string (variant_title, NULL);
	if (title == NULL || title[0] == '\0') {
		title = g_path_get_basename (g_dbus_proxy_get_object_path (proxy_tmp));
		g_strdelimit (title, "_", ' ');
		g_strchomp (title);
	}

	liststore_profiles = GTK_LIST_STORE (gtk_builder_get_object (builder,
					     "liststore_profiles"));
	gtk_list_store_append (liststore_profiles, &iter);
	gtk_list_store_set (liststore_profiles,
			    &iter,
			    CD_COLUMN_PROFILE_TITLE, title,
			    CD_COLUMN_PROFILE_OBJECT_PATH, g_dbus_proxy_get_object_path (proxy_tmp),
			    -1);

	if (variant_title != NULL)
		g_variant_unref (variant_title);
	g_object_unref (proxy_tmp);
	g_free (title);
}

/**
 * cd_gui_add_profile_to_listview:
 **/
static void
cd_gui_add_profile_to_listview (const gchar *object_path)
{
	/* get initial icon state */
	g_debug ("add %s", object_path);
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  COLORD_DBUS_SERVICE,
				  object_path,
				  COLORD_DBUS_INTERFACE_PROFILE,
				  NULL,
				  cd_gui_got_profile_proxy_full_cb,
				  NULL);
}

/**
 * cd_gui_add_profile_to_device_listview:
 **/
static void
cd_gui_add_profile_to_device_listview (const gchar *object_path)
{
	/* get initial icon state */
	GtkListStore *liststore;
	GtkTreeIter iter;
	gchar *title = NULL;

	g_debug ("add %s", object_path);
	title = g_path_get_basename (object_path);
	g_strdelimit (title, "_", ' ');
	g_strchomp (title);
	liststore = GTK_LIST_STORE (gtk_builder_get_object (builder,
				    "liststore_device_profiles"));
	gtk_list_store_append (liststore, &iter);
	gtk_list_store_set (liststore,
			    &iter,
			    CD_COLUMN_DEVICE_PROFILES_TITLE, title,
			    CD_COLUMN_DEVICE_PROFILES_OBJECT_PATH, object_path,
			    -1);
	g_free (title);


}

/**
 * cd_gui_got_device_proxy_cb:
 **/
static void
cd_gui_got_device_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	GDBusProxy *proxy_tmp;
	const gchar *device_id = NULL;
//	const gchar *model;
	gchar *profile_tmp;
//	GDBusProxy *proxy;
	gsize len;
	gchar *created = NULL;
	guint i;
	GVariantIter iter;
	GtkWidget *widget;
	GVariant *variant_created = NULL;
	GVariant *variant_device_id = NULL;
//	GVariant *variant_model = NULL;
	GVariant *variant_profiles = NULL;
	GtkListStore *liststore_profiles;

	proxy_tmp = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (proxy_tmp == NULL) {
		g_warning ("Error creating proxy: %s", error->message);
		g_error_free (error);
		return;
	}

	/* print created date */
	variant_created = g_dbus_proxy_get_cached_property (proxy_tmp, "Created");
	created = g_strdup_printf ("%" G_GUINT64_FORMAT,
				   g_variant_get_uint64 (variant_created));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_created"));
	gtk_label_set_label (GTK_LABEL (widget), created);

#if 0
	/* print model */
	variant_model = g_dbus_proxy_get_cached_property (proxy_tmp, "Model");
	model = g_variant_get_string (variant_model, NULL);
	g_print ("Model:\t\t%s\n", model);
#endif

	/* print device id */
	variant_device_id = g_dbus_proxy_get_cached_property (proxy_tmp, "DeviceId");
	device_id = g_variant_get_string (variant_device_id, NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_deviceid"));
	gtk_label_set_label (GTK_LABEL (widget), device_id);

	/* print profiles */
	liststore_profiles = GTK_LIST_STORE (gtk_builder_get_object (builder,
					     "liststore_device_profiles"));
	gtk_list_store_clear (liststore_profiles);
	variant_profiles = g_dbus_proxy_get_cached_property (proxy_tmp, "Profiles");
	len = g_variant_iter_init (&iter, variant_profiles);
	if (len == 0)
		g_print ("No assigned profiles!\n");
	for (i=0; i<len; i++) {
		g_variant_get_child (variant_profiles, i,
				     "o", &profile_tmp);
		cd_gui_add_profile_to_device_listview (profile_tmp);
		g_free (profile_tmp);
	}
//out:
	g_free (created);
	g_variant_unref (variant_created);
//	g_variant_unref (variant_model);
	g_variant_unref (variant_device_id);
	g_variant_unref (variant_profiles);
	g_object_unref (proxy_tmp);
}

/**
 * cd_gui_get_profiles_cb:
 **/
static void
cd_gui_get_profiles_cb (GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	gchar *object_path_tmp = NULL;
	gsize len;
	guint i;
	GVariantIter iter;
	GVariant *response_child = NULL;
	GVariant *result;
	GError *error = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res, &error);
	if (result == NULL) {
		g_warning ("Error getting profiles: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	/* print each device */
	response_child = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, response_child);
	for (i=0; i < len; i++) {
		g_variant_get_child (response_child, i,
				     "o", &object_path_tmp);
		cd_gui_add_profile_to_listview (object_path_tmp);
		g_free (object_path_tmp);
	}

	g_variant_unref (result);
}

/**
 * cd_gui_treeview_device_clicked_cb:
 **/
static void
cd_gui_treeview_device_clicked_cb (GtkTreeSelection *selection, gpointer data)
{
	gchar *id;
	GtkTreeIter iter;
	GtkTreeModel *model;

	/* get entry */
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter,
				    CD_COLUMN_DEVICES_OBJECT_PATH, &id,
				    -1);
		g_debug ("selected row is: %s", id);

		/* save pointer */
		g_free (current_device);
		current_device = g_strdup (id);

		/* get initial icon state */
		g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
					  G_DBUS_PROXY_FLAGS_NONE,
					  NULL,
					  COLORD_DBUS_SERVICE,
					  id,
					  COLORD_DBUS_INTERFACE_DEVICE,
					  NULL,
					  cd_gui_got_device_proxy_cb,
					  NULL);

		g_free (id);
	} else {
		g_debug ("no row selected");
	}
}


/**
 * cd_gui_got_profile_proxy_full_cb2:
 **/
static void
cd_gui_got_profile_proxy_full_cb2 (GObject *source_object,
				  GAsyncResult *res,
				  gpointer user_data)
{
	GError *error = NULL;
	GDBusProxy *proxy_tmp;
	const gchar *profile_id = NULL;
	const gchar *title;
	const gchar *qualifier;
	const gchar *filename;
	GtkWidget *widget;
	GVariant *variant_profile_id = NULL;
	GVariant *variant_title = NULL;
	GVariant *variant_qualifier = NULL;
	GVariant *variant_filename = NULL;

	proxy_tmp = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (proxy_tmp == NULL) {
		g_warning ("Error creating proxy: %s", error->message);
		g_error_free (error);
		return;
	}

	/* print profile id */
	variant_profile_id = g_dbus_proxy_get_cached_property (proxy_tmp, "ProfileId");
	profile_id = g_variant_get_string (variant_profile_id, NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_profileid"));
	gtk_label_set_label (GTK_LABEL (widget), profile_id);

	/* print title */
	variant_title = g_dbus_proxy_get_cached_property (proxy_tmp, "Title");
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_profile_title"));
	if (variant_title != NULL) {
		title = g_variant_get_string (variant_title, NULL);
		gtk_label_set_label (GTK_LABEL (widget), title);
	} else {
		gtk_label_set_label (GTK_LABEL (widget), "");
	}

	/* print filename */
	variant_filename = g_dbus_proxy_get_cached_property (proxy_tmp, "Filename");
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "entry_profile_filename"));
	if (variant_filename != NULL) {
		filename = g_variant_get_string (variant_filename, NULL);
		gtk_entry_set_text (GTK_ENTRY (widget), filename);
	} else {
		gtk_entry_set_text (GTK_ENTRY (widget), "");
	}

	/* print qualifier */
	variant_qualifier = g_dbus_proxy_get_cached_property (proxy_tmp, "Qualifier");
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "entry_profile_qualifier"));
	if (variant_qualifier != NULL) {
		qualifier = g_variant_get_string (variant_qualifier, NULL);
		gtk_entry_set_text (GTK_ENTRY (widget), qualifier);
	} else {
		gtk_entry_set_text (GTK_ENTRY (widget), "");
	}

	g_variant_unref (variant_profile_id);
	if (variant_title != NULL)
		g_variant_unref (variant_title);
	if (variant_qualifier != NULL)
		g_variant_unref (variant_qualifier);
	if (variant_filename != NULL)
		g_variant_unref (variant_filename);
	g_object_unref (proxy_tmp);
}

/**
 * cd_gui_treeview_profile_clicked_cb:
 **/
static void
cd_gui_treeview_profile_clicked_cb (GtkTreeSelection *selection, gpointer data)
{
	gchar *id;
	GtkTreeIter iter;
	GtkTreeModel *model;

	/* get entry */
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter,
				    CD_COLUMN_PROFILE_OBJECT_PATH, &id,
				    -1);
		g_debug ("selected row is: %s", id);

		/* save pointer */
		g_free (current_profile);
		current_profile = g_strdup (id);

		/* get initial icon state */
		g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
					  G_DBUS_PROXY_FLAGS_NONE,
					  NULL,
					  COLORD_DBUS_SERVICE,
					  id,
					  COLORD_DBUS_INTERFACE_PROFILE,
					  NULL,
					  cd_gui_got_profile_proxy_full_cb2,
					  NULL);

		g_free (id);
	} else {
		g_debug ("no row selected");
	}
}

/**
 * cd_gui_close_cb:
 **/
static void
cd_gui_close_cb (GtkWidget *widget, gpointer data)
{
	GMainLoop *loop = (GMainLoop *) data;
	g_debug ("emitting action-close");
	g_main_loop_quit (loop);
}

/**
 * cd_gui_delete_event_cb:
 **/
static gboolean
cd_gui_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	cd_gui_close_cb (widget, data);
	return FALSE;
}

/**
 * cd_gui_add_device_to_listview:
 **/
static void
cd_gui_add_device_to_listview (const gchar *object_path)
{
	GtkListStore *liststore_devices;
	GtkTreeIter iter;
	gchar *title = NULL;

	g_debug ("add %s", object_path);

	/* TODO: need title */
	title = g_path_get_basename (object_path);
	g_strdelimit (title, "_", ' ');
	g_strchomp (title);
	liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (builder,
					    "liststore_devices"));
	gtk_list_store_append (liststore_devices, &iter);
	gtk_list_store_set (liststore_devices,
			    &iter,
			    CD_COLUMN_DEVICES_TITLE, title,
			    CD_COLUMN_DEVICES_OBJECT_PATH, object_path,
			    -1);
	g_free (title);
}

/**
 * cd_gui_get_devices_cb:
 **/
static void
cd_gui_get_devices_cb (GObject *source_object,
		       GAsyncResult *res,
		       gpointer user_data)
{
	gchar *object_path_tmp = NULL;
	gsize len;
	guint i;
	GVariantIter iter;
	GVariant *response_child = NULL;
	GVariant *result;
	GError *error = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res, &error);
	if (result == NULL) {
		g_warning ("Error getting devices: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	/* print each device */
	response_child = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, response_child);
	for (i=0; i < len; i++) {
		g_variant_get_child (response_child, i,
				     "o", &object_path_tmp);
		cd_gui_add_device_to_listview (object_path_tmp);
		g_free (object_path_tmp);
	}

	g_variant_unref (result);
}

/**
 * cd_gui_remove_device_from_listview:
 **/
static void
cd_gui_remove_device_from_listview (const gchar *object_path)
{
	gboolean ret;
	gchar *object_path_tmp;
	GtkListStore *liststore_devices;
	GtkTreeIter iter;

	liststore_devices = GTK_LIST_STORE (gtk_builder_get_object (builder,
					    "liststore_devices"));
	ret = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (liststore_devices),
					     &iter);
	while (ret) {
		/* Walk through the list, reading each row */
		gtk_tree_model_get (GTK_TREE_MODEL (liststore_devices), &iter,
				    CD_COLUMN_DEVICES_OBJECT_PATH, &object_path_tmp,
				    -1);
		if (g_strcmp0 (object_path,
			       object_path_tmp) == 0) {
			gtk_list_store_remove (liststore_devices, &iter);
			break;
		}
		g_free (object_path_tmp);
		ret = gtk_tree_model_iter_next (GTK_TREE_MODEL (liststore_devices),
						&iter);
	}
}

/**
 * cd_gui_remove_profile_from_listview:
 **/
static void
cd_gui_remove_profile_from_listview (const gchar *object_path)
{
	gboolean ret;
	gchar *object_path_tmp;
	GtkListStore *liststore_profiles;
	GtkTreeIter iter;

	liststore_profiles = GTK_LIST_STORE (gtk_builder_get_object (builder,
					     "liststore_profiles"));
	ret = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (liststore_profiles),
					     &iter);
	while (ret) {
		/* Walk through the list, reading each row */
		gtk_tree_model_get (GTK_TREE_MODEL (liststore_profiles), &iter,
				    CD_COLUMN_PROFILE_OBJECT_PATH, &object_path_tmp,
				    -1);
		if (g_strcmp0 (object_path,
			       object_path_tmp) == 0) {
			gtk_list_store_remove (liststore_profiles, &iter);
			break;
		}
		g_free (object_path_tmp);
		ret = gtk_tree_model_iter_next (GTK_TREE_MODEL (liststore_profiles),
						&iter);
	}
}

/**
 * cd_gui_dbus_signal_cb:
 **/
static void
cd_gui_dbus_signal_cb (GDBusProxy *_proxy,
		       gchar      *sender_name,
		       gchar      *signal_name,
		       GVariant   *parameters,
		       gpointer    user_data)
{
	gchar *object_path_tmp = NULL;

	if (g_strcmp0 (signal_name, "Changed") == 0) {
		g_warning ("changed");

	} else if (g_strcmp0 (signal_name, "DeviceAdded") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		cd_gui_add_device_to_listview (object_path_tmp);

	} else if (g_strcmp0 (signal_name, "DeviceRemoved") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		cd_gui_remove_device_from_listview (object_path_tmp);

	} else if (g_strcmp0 (signal_name, "ProfileAdded") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		cd_gui_add_profile_to_listview (object_path_tmp);

	} else if (g_strcmp0 (signal_name, "ProfileRemoved") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		cd_gui_remove_profile_from_listview (object_path_tmp);

	} else {
		g_error ("unhandled signal '%s'", signal_name);
	}
	g_free (object_path_tmp);
}

/**
 * cd_gui_got_proxy_cb:
 **/
static void
cd_gui_got_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;

	proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (proxy == NULL) {
		g_warning ("Error creating proxy: %s", error->message);
		g_error_free (error);
		return;
	}

	/* we want to change the primary device changes */
	g_signal_connect (proxy,
			  "g-signal",
			  G_CALLBACK (cd_gui_dbus_signal_cb),
			  user_data);

	g_dbus_proxy_call (proxy,
			   "GetDevices",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   NULL,
			   cd_gui_get_devices_cb,
			   NULL);

	g_dbus_proxy_call (proxy,
			   "GetProfiles",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   NULL,
			   cd_gui_get_profiles_cb,
			   NULL);
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GtkWidget *main_window;
	GtkWidget *widget;
	GtkTreeSelection *selection;
	GError *error = NULL;
	guint retval;
//	gboolean ret;
	GMainLoop *loop;

	const GOptionEntry options[] = {
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	if (! g_thread_supported ())
		g_thread_init (NULL);
	g_type_init ();
	gtk_init (&argc, &argv);

	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, _("Color GUI Tool"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	loop = g_main_loop_new (NULL, FALSE);

	/* get UI */
	builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (builder, "./cd-gui.ui", &error);
	if (retval == 0) {
		g_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
		goto out;
	}

	main_window = GTK_WIDGET (gtk_builder_get_object (builder, "window_colord"));
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (cd_gui_delete_event_cb), loop);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_device_add"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cd_gui_button_device_add_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_device_remove"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cd_gui_button_device_remove_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_profile_add"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cd_gui_button_profile_add_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_profile_remove"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cd_gui_button_profile_remove_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_create_add"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cd_gui_button_create_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_create_cancel"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cd_gui_button_create_cancel_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder,
						     "button_device_profile_add"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cd_gui_button_device_profile_add_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder,
						     "button_device_profile_remove"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cd_gui_button_device_profile_remove_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder,
						     "button_profile_make_default"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cd_gui_button_device_profile_make_default_cb), NULL);

	/* create tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (cd_gui_treeview_device_clicked_cb), NULL);

	/* add columns to the tree view */
	cd_gui_treeview_add_device_columns (GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget));

	/* create tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_profiles"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (cd_gui_treeview_profile_clicked_cb), NULL);

	/* add columns to the tree view */
	cd_gui_treeview_add_profile_columns (GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget));

	/* create tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (builder,
						     "treeview_device_profiles"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
//	g_signal_connect (selection, "changed",
//			  G_CALLBACK (cd_gui_treeview_device_clicked_cb), NULL);

	/* add columns to the tree view */
	cd_gui_treeview_add_device_profile_columns (GTK_TREE_VIEW (widget));

	/* get a session bus connection */
	connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
	if (connection == NULL) {
		/* TRANSLATORS: no DBus system bus */
		g_warning ("%s", error->message);
		g_error_free (error);
		goto out;
	}

	/* get initial icon state */
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  COLORD_DBUS_SERVICE,
				  COLORD_DBUS_PATH,
				  COLORD_DBUS_INTERFACE,
				  NULL,
				  cd_gui_got_proxy_cb,
				  NULL);

	/* show window */
	gtk_widget_show (main_window);

	/* wait */
	g_main_loop_run (loop);
out:
	g_object_unref (builder);
	g_main_loop_unref (loop);
	return 0;
}
