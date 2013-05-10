/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2013 Richard Hughes <richard@hughsie.com>
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

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <locale.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <colord/colord.h>

typedef struct {
	CdClient		*client;
	GOptionContext		*context;
	GPtrArray		*cmd_array;
} CdUtilPrivate;

typedef gboolean (*CdUtilPrivateCb)	(CdUtilPrivate	*util,
					 gchar		**values,
					 GError		**error);

typedef struct {
	gchar		*name;
	gchar		*description;
	CdUtilPrivateCb	 callback;
} CdUtilItem;

/**
 * cd_util_print_field:
 **/
static void
cd_util_print_field (const gchar *title, const gchar *message)
{
	const guint padding = 15;
	guint i;
	guint len = 0;

	/* nothing useful to print */
	if (message == NULL || message[0] == '\0')
		return;

	if (title != NULL) {
		g_print ("%s:", title);
		len = strlen (title) + 1;
	}
	for (i = len; i < padding; i++)
		g_print (" ");
	g_print ("%s\n", message);
}

/**
 * cd_util_print_field_time:
 **/
static void
cd_util_print_field_time (const gchar *title, gint64 usecs)
{
	gchar *str;
	GDateTime *datetime;

	datetime = g_date_time_new_from_unix_utc (usecs / G_USEC_PER_SEC);
	/* TRANSLATORS: this is the profile creation date strftime format */
	str = g_date_time_format (datetime, _("%B %e %Y, %I:%M:%S %p"));
	cd_util_print_field (title, str);
	g_date_time_unref (datetime);
	g_free (str);
}

/**
 * cd_util_show_owner:
 **/
static void
cd_util_show_owner (guint uid)
{
	struct passwd *pw;
	pw = getpwuid (uid);
	/* TRANSLATORS: profile owner */
	cd_util_print_field (_("Owner"),
			     pw->pw_name);
}

/**
 * cd_util_show_profile:
 **/
static void
cd_util_show_profile (CdProfile *profile)
{
	CdColorspace colorspace;
	CdObjectScope scope;
	CdProfileKind kind;
	const gchar *tmp;
	gchar *str_tmp;
	gchar **warnings;
	GHashTable *metadata;
	GList *list, *l;
	guint i;
	guint size;

	/* TRANSLATORS: the internal DBus path */
	cd_util_print_field (_("Object Path"),
			     cd_profile_get_object_path (profile));
	cd_util_show_owner (cd_profile_get_owner (profile));
	tmp = cd_profile_get_format (profile);
	if (tmp != NULL && tmp[0] != '\0') {
		/* TRANSLATORS: the profile format, e.g.
		 * ColorModel.OutputMode.OutputResolution */
		cd_util_print_field (_("Format"), tmp);
	}
	tmp = cd_profile_get_title (profile);
	if (tmp != NULL && tmp[0] != '\0') {
		/* TRANSLATORS: the profile title, e.g.
		 * "ColorMunki, HP Deskjet d1300 Series" */
		cd_util_print_field (_("Title"), tmp);
	}
	tmp = cd_profile_get_qualifier (profile);
	if (tmp != NULL && tmp[0] != '\0') {
		/* TRANSLATORS: the profile qualifier, e.g. RGB.Plain.300dpi */
		cd_util_print_field (_("Qualifier"), tmp);
	}
	kind = cd_profile_get_kind (profile);
	if (kind != CD_PROFILE_KIND_UNKNOWN) {
		/* TRANSLATORS: the profile type, e.g. 'output' */
		cd_util_print_field (_("Type"),
			 cd_profile_kind_to_string (kind));
	}
	colorspace = cd_profile_get_colorspace (profile);
	if (colorspace != CD_COLORSPACE_UNKNOWN) {
		/* TRANSLATORS: the profile colorspace, e.g. 'rgb' */
		cd_util_print_field (_("Colorspace"),
			 cd_colorspace_to_string (colorspace));
	}
	scope = cd_profile_get_scope (profile);
	if (scope != CD_OBJECT_SCOPE_UNKNOWN) {
		/* TRANSLATORS: the object scope, e.g. temp, disk, etc */
		cd_util_print_field (_("Scope"),
			 cd_object_scope_to_string (scope));
	}

	/* TRANSLATORS: if the profile has a Video Card Gamma Table lookup */
	cd_util_print_field (_("Gamma Table"),
			     cd_profile_get_has_vcgt (profile) ? "Yes" : "No");

	/* TRANSLATORS: if the profile is installed for all users */
	cd_util_print_field (_("System Wide"),
			     cd_profile_get_is_system_wide (profile) ? "Yes" : "No");

	/* TRANSLATORS: profile filename */
	cd_util_print_field (_("Filename"),
			     cd_profile_get_filename (profile));

	/* TRANSLATORS: profile identifier */
	cd_util_print_field (_("Profile ID"),
			     cd_profile_get_id (profile));

	/* list all the items of metadata */
	metadata = cd_profile_get_metadata (profile);
	list = g_hash_table_get_keys (metadata);
	for (l = list; l != NULL; l = l->next) {
		if (g_strcmp0 (l->data, "CMS") == 0)
			continue;
		tmp = (const gchar *) g_hash_table_lookup (metadata,
							   l->data);
		str_tmp = g_strdup_printf ("%s=%s",
					   (const gchar *) l->data, tmp);
		/* TRANSLATORS: the metadata for the device */
		cd_util_print_field (_("Metadata"), str_tmp);
		g_free (str_tmp);
	}

	/* show warnings */
	warnings = cd_profile_get_warnings (profile);
	size = g_strv_length (warnings);
	for (i = 0; i < size; i++)
		cd_util_print_field (_("Warning"), warnings[i]);

	g_list_free (list);
	g_hash_table_unref (metadata);
}

/**
 * cd_util_show_device:
 **/
static void
cd_util_show_device (CdDevice *device)
{
	CdObjectScope scope;
	CdProfile *profile_tmp;
	const gchar *tmp;
	gboolean ret;
	gchar *str_tmp;
	GError *error = NULL;
	GHashTable *metadata;
	GList *list, *l;
	GPtrArray *profiles;
	guint i;

	/* TRANSLATORS: the internal DBus path */
	cd_util_print_field (_("Object Path"),
			     cd_device_get_object_path (device));
	cd_util_show_owner (cd_device_get_owner (device));

	/* TRANSLATORS: this is the time the device was registered
	 * with colord, and probably is the same as the system startup
	 * unless the device has been explicitly saved in the database */
	cd_util_print_field_time (_("Created"),
				  cd_device_get_created (device));

	/* TRANSLATORS: this is the time of the last calibration or when
	 * the manufacturer-provided profile was assigned by the user */
	cd_util_print_field_time (_("Modified"),
				  cd_device_get_modified (device));

	/* TRANSLATORS: the device type, e.g. "printer" */
	cd_util_print_field (_("Type"),
			     cd_device_kind_to_string (cd_device_get_kind (device)));

	/* TRANSLATORS: the device enabled state */
	cd_util_print_field (_("Enabled"),
			     cd_device_get_enabled (device) ? "Yes" : "No");

	/* TRANSLATORS: if the device is embedded into the computer and
	 * cannot be removed */
	cd_util_print_field (_("Embedded"),
			     cd_device_get_embedded (device) ? "Yes" : "No");

	/* TRANSLATORS: the device model */
	cd_util_print_field (_("Model"),
			     cd_device_get_model (device));

	/* TRANSLATORS: the device vendor */
	cd_util_print_field (_("Vendor"),
			     cd_device_get_vendor (device));

	/* TRANSLATORS: the device inhibitors */
	str_tmp = g_strjoinv (", ", (gchar **) cd_device_get_profiling_inhibitors (device));
	cd_util_print_field (_("Inhibitors"), str_tmp);
	g_free (str_tmp);

	/* TRANSLATORS: the device serial number */
	cd_util_print_field (_("Serial"),
			     cd_device_get_serial (device));

	/* TRANSLATORS: the device seat identifier, where a seat is
	 * defined as a monitor, keyboard and mouse.
	 * For instance, in a public library one central computer can
	 * have 3 keyboards, 3 displays and 3 mice plugged in and with
	 * systemd these can be setup as three independant seats with
	 * different sessions running on them */
	cd_util_print_field (_("Seat"),
			     cd_device_get_seat (device));

	tmp = cd_device_get_format (device);
	if (tmp != NULL && tmp[0] != '\0') {
		/* TRANSLATORS: the device format, e.g.
		 * ColorModel.OutputMode.OutputResolution */
		cd_util_print_field (_("Format"), tmp);
	}

	scope = cd_device_get_scope (device);
	if (scope != CD_OBJECT_SCOPE_UNKNOWN) {
		/* TRANSLATORS: the object scope, e.g. temp, disk, etc */
		cd_util_print_field (_("Scope"),
				     cd_object_scope_to_string (scope));
	}

	/* TRANSLATORS: the device colorspace, e.g. "rgb" */
	cd_util_print_field (_("Colorspace"),
			     cd_colorspace_to_string (cd_device_get_colorspace (device)));

	/* TRANSLATORS: the device identifier */
	cd_util_print_field (_("Device ID"),
			     cd_device_get_id (device));

	/* print profiles */
	profiles = cd_device_get_profiles (device);
	for (i = 0; i < profiles->len; i++) {
		profile_tmp = g_ptr_array_index (profiles, i);
		/* TRANSLATORS: the profile for the device */
		str_tmp = g_strdup_printf ("%s %i", _("Profile"), i+1);
		ret = cd_profile_connect_sync (profile_tmp, NULL, &error);
		if (!ret) {
			cd_util_print_field (str_tmp,
					     cd_profile_get_object_path (profile_tmp));
			cd_util_print_field (NULL,
					     error->message);
			g_clear_error (&error);
		} else {
			cd_util_print_field (str_tmp,
					     cd_profile_get_id (profile_tmp));
			cd_util_print_field (NULL,
					     cd_profile_get_filename(profile_tmp));
		}
		g_free (str_tmp);
	}

	/* list all the items of metadata */
	metadata = cd_device_get_metadata (device);
	list = g_hash_table_get_keys (metadata);
	for (l = list; l != NULL; l = l->next) {
		tmp = (const gchar *) g_hash_table_lookup (metadata,
							   l->data);
		str_tmp = g_strdup_printf ("%s=%s",
					   (const gchar *) l->data, tmp);
		/* TRANSLATORS: the metadata for the device */
		cd_util_print_field (_("Metadata"), str_tmp);
		g_free (str_tmp);
	}
	g_list_free (list);
	g_hash_table_unref (metadata);
}

/**
 * cd_util_idle_loop_quit_cb:
 **/
static gboolean
cd_util_idle_loop_quit_cb (gpointer user_data)
{
	GMainLoop *loop = (GMainLoop *) user_data;
	g_main_loop_quit (loop);
	return G_SOURCE_REMOVE;
}
/**
 * cd_util_sensor_cap_to_string:
 **/
static const gchar *
cd_util_sensor_cap_to_string (CdSensorCap sensor_cap)
{
	if (sensor_cap == CD_SENSOR_CAP_CRT) {
		/* TRANSLATORS: this is the display technology,
		 * and an abbreviation for "Cathode Ray Tube" */
		return _("CRT");
	}
	if (sensor_cap == CD_SENSOR_CAP_PRINTER) {
		/* TRANSLATORS: this is a desktop printer */
		return _("Printer");
	}
	if (sensor_cap == CD_SENSOR_CAP_PROJECTOR) {
		/* TRANSLATORS: a beamer used for presentations */
		return _("Projector");
	}
	if (sensor_cap == CD_SENSOR_CAP_SPOT) {
		/* TRANSLATORS: a spot measurement, e.g.
		 * getting the color from a color swatch */
		return _("Spot");
	}
	if (sensor_cap == CD_SENSOR_CAP_AMBIENT) {
		/* TRANSLATORS: the sensor can get a reading of the
		 * ambient light level */
		return _("Ambient");
	}
	if (sensor_cap == CD_SENSOR_CAP_CALIBRATION) {
		/* TRANSLATORS: this is the display technology */
		return _("Calibration");
	}
	if (sensor_cap == CD_SENSOR_CAP_LCD) {
		/* TRANSLATORS: this is the display technology,
		 * where LCD stands for 'Liquid Crystal Display' */
		return _("LCD Generic");
	}
	if (sensor_cap == CD_SENSOR_CAP_LED) {
		/* TRANSLATORS: this is the display technology where
		 * LED stands for 'Light Emitted Diode' */
		return _("LED Generic");
	}
	if (sensor_cap == CD_SENSOR_CAP_PLASMA) {
		/* TRANSLATORS: this is the display technology,
		 * sometimes called PDP displays. See
		 * http://en.wikipedia.org/wiki/Plasma_display */
		return _("Plasma");
	}
	if (sensor_cap == CD_SENSOR_CAP_LCD_CCFL) {
		/* TRANSLATORS: this is the display technology,
		 * where LCD stands for 'Liquid Crystal Display'
		 * and CCFL stands for 'Cold Cathode Fluorescent Lamp' */
		return _("LCD CCFL");
	}
	if (sensor_cap == CD_SENSOR_CAP_LCD_RGB_LED) {
		/* TRANSLATORS: this is the display technology where
		 * RGB stands for 'Red Green Blue' and LED stands for
		 * 'Light Emitted Diode' */
		return _("LCD RGB LED");
	}
	if (sensor_cap == CD_SENSOR_CAP_WIDE_GAMUT_LCD_CCFL) {
		/* TRANSLATORS: this is the display technology, where
		 * wide gamut means the display primaries are much
		 * better than normal consumer monitors */
		return _("Wide Gamut LCD CCFL");
	}
	if (sensor_cap == CD_SENSOR_CAP_WIDE_GAMUT_LCD_RGB_LED) {
		/* TRANSLATORS: this is the display technology */
		return _("Wide Gamut LCD RGB LED");
	}
	if (sensor_cap == CD_SENSOR_CAP_LCD_WHITE_LED) {
		/* TRANSLATORS: this is the display technology, where
		 * white means the color of the backlight, i.e. not
		 * RGB LED */
		return _("LCD White LED");
	}
	/* TRANSLATORS: this an unknown display technology */
	return _("Unknown");
}

/**
 * cd_util_show_sensor:
 **/
static void
cd_util_show_sensor (CdSensor *sensor)
{
	CdSensorKind kind;
	CdSensorState state;
	const gchar *tmp;
	gboolean ret;
	gchar *str_tmp;
	GError *error = NULL;
	GString *caps_str = NULL;
	GHashTable *options = NULL;
	GList *l;
	GList *list = NULL;
	GMainLoop *loop = NULL;
	guint caps;
	guint i;
	GVariant *value_tmp;
	GHashTable *metadata = NULL;

	/* TRANSLATORS: the internal DBus path */
	cd_util_print_field (_("Object Path"),
			     cd_sensor_get_object_path (sensor));

	/* lock */
	ret = cd_sensor_lock_sync (sensor,
				   NULL,
				   &error);
	if (!ret) {
		g_warning ("Failed to lock sensor: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* wait for updates */
	loop = g_main_loop_new (NULL, FALSE);
	g_idle_add (cd_util_idle_loop_quit_cb, loop);
	g_main_loop_run (loop);

	kind = cd_sensor_get_kind (sensor);
	if (kind != CD_SENSOR_KIND_UNKNOWN) {
		/* TRANSLATORS: the sensor type, e.g. 'output' */
		cd_util_print_field (_("Type"),
			 cd_sensor_kind_to_string (kind));
	}

	state = cd_sensor_get_state (sensor);
	if (state != CD_SENSOR_STATE_UNKNOWN) {
		/* TRANSLATORS: the sensor state, e.g. 'idle' */
		cd_util_print_field (_("State"),
			 cd_sensor_state_to_string (state));
	}

	tmp = cd_sensor_get_serial (sensor);
	if (tmp != NULL) {
		/* TRANSLATORS: sensor serial */
		cd_util_print_field (_("Serial number"),
			 tmp);
	}

	tmp = cd_sensor_get_model (sensor);
	if (tmp != NULL) {
		/* TRANSLATORS: sensor model */
		cd_util_print_field (_("Model"),
			 tmp);
	}

	tmp = cd_sensor_get_vendor (sensor);
	if (tmp != NULL) {
		/* TRANSLATORS: sensor vendor */
		cd_util_print_field (_("Vendor"),
			 tmp);
	}

	/* TRANSLATORS: sensor identifier */
	cd_util_print_field (_("Sensor ID"),
			     cd_sensor_get_id (sensor));

	/* print sensor options */
	options = cd_sensor_get_options (sensor);
	list = g_hash_table_get_keys (options);
	for (l = list; l != NULL; l = l->next) {
		value_tmp = (GVariant *) g_hash_table_lookup (options,
							      l->data);
		if (g_variant_is_of_type (value_tmp, G_VARIANT_TYPE_STRING)) {
			str_tmp = g_strdup_printf ("%s=%s",
						   (const gchar *) l->data,
						   g_variant_get_string (value_tmp, NULL));
		} else {
			str_tmp = g_strdup_printf ("%s=*unknown*",
						   (const gchar *) l->data);
		}

		/* TRANSLATORS: the options for the sensor */
		cd_util_print_field (_("Options"), str_tmp);
		g_free (str_tmp);
	}

	/* list all the items of metadata */
	metadata = cd_sensor_get_metadata (sensor);
	list = g_hash_table_get_keys (metadata);
	for (l = list; l != NULL; l = l->next) {
		tmp = (const gchar *) g_hash_table_lookup (metadata,
							   l->data);
		str_tmp = g_strdup_printf ("%s=%s",
					   (const gchar *) l->data, tmp);
		/* TRANSLATORS: the metadata for the sensor */
		cd_util_print_field (_("Metadata"), str_tmp);
		g_free (str_tmp);
	}

	/* TRANSLATORS: if the sensor has a colord native driver */
	cd_util_print_field (_("Native"),
			     cd_sensor_get_native (sensor) ? "Yes" : "No");

	/* TRANSLATORS: if the sensor is locked */
	cd_util_print_field (_("Locked"),
			     cd_sensor_get_locked (sensor) ? "Yes" : "No");

	/* get sensor caps */
	caps = cd_sensor_get_caps (sensor);
	caps_str = g_string_new ("");
	for (i = 0; i < CD_SENSOR_CAP_LAST; i++) {
		ret = cd_bitfield_contain (caps, i);
		if (ret) {
			g_string_append_printf (caps_str, "%s, ",
						cd_util_sensor_cap_to_string (i));
		}
	}
	if (caps_str->len > 0)
		g_string_set_size (caps_str, caps_str->len - 2);
	/* TRANSLATORS: if the sensor supports calibrating different
	 * display types, e.g. LCD, LED, Projector */
	cd_util_print_field (_("Capabilities"), caps_str->str);

	/* unlock */
	ret = cd_sensor_unlock_sync (sensor,
				     NULL,
				     &error);
	if (!ret) {
		g_warning ("Failed to unlock sensor: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (metadata != NULL)
		g_hash_table_unref (metadata);
	g_list_free (list);
	if (caps_str != NULL)
		g_string_free (caps_str, TRUE);
	if (options != NULL)
		g_hash_table_unref (options);
	if (loop != NULL)
		g_main_loop_unref (loop);
}

/**
 * cd_util_item_free:
 **/
static void
cd_util_item_free (CdUtilItem *item)
{
	g_free (item->name);
	g_free (item->description);
	g_free (item);
}

/*
 * cd_sort_command_name_cb:
 */
static gint
cd_sort_command_name_cb (CdUtilItem **item1, CdUtilItem **item2)
{
	return g_strcmp0 ((*item1)->name, (*item2)->name);
}

/**
 * cd_util_add:
 **/
static void
cd_util_add (GPtrArray *array, const gchar *name, const gchar *description, CdUtilPrivateCb callback)
{
	gchar **names;
	guint i;
	CdUtilItem *item;

	/* add each one */
	names = g_strsplit (name, ",", -1);
	for (i = 0; names[i] != NULL; i++) {
		item = g_new0 (CdUtilItem, 1);
		item->name = g_strdup (names[i]);
		if (i == 0) {
			item->description = g_strdup (description);
		} else {
			/* TRANSLATORS: this is a command alias */
			item->description = g_strdup_printf (_("Alias to %s"),
							     names[0]);
		}
		item->callback = callback;
		g_ptr_array_add (array, item);
	}
	g_strfreev (names);
}

/**
 * cd_util_get_descriptions:
 **/
static gchar *
cd_util_get_descriptions (GPtrArray *array)
{
	guint i;
	guint j;
	guint len;
	guint max_len = 0;
	CdUtilItem *item;
	GString *string;

	/* get maximum command length */
	for (i = 0; i < array->len; i++) {
		item = g_ptr_array_index (array, i);
		len = strlen (item->name);
		if (len > max_len)
			max_len = len;
	}

	/* ensure we're spaced by at least this */
	if (max_len < 19)
		max_len = 19;

	/* print each command */
	string = g_string_new ("");
	for (i = 0; i < array->len; i++) {
		item = g_ptr_array_index (array, i);
		g_string_append (string, "  ");
		g_string_append (string, item->name);
		len = strlen (item->name);
		for (j=len; j<max_len+3; j++)
			g_string_append_c (string, ' ');
		g_string_append (string, item->description);
		g_string_append_c (string, '\n');
	}

	/* remove trailing newline */
	if (string->len > 0)
		g_string_set_size (string, string->len - 1);

	return g_string_free (string, FALSE);
}

/**
 * cd_util_run:
 **/
static gboolean
cd_util_run (CdUtilPrivate *priv, const gchar *command, gchar **values, GError **error)
{
	gboolean ret = FALSE;
	guint i;
	CdUtilItem *item;
	GString *string;

	/* find command */
	for (i = 0; i < priv->cmd_array->len; i++) {
		item = g_ptr_array_index (priv->cmd_array, i);
		if (g_strcmp0 (item->name, command) == 0) {
			ret = item->callback (priv, values, error);
			goto out;
		}
	}

	/* not found */
	string = g_string_new ("");
	/* TRANSLATORS: error message */
	g_string_append_printf (string, "%s\n",
				_("Command not found, valid commands are:"));
	for (i = 0; i < priv->cmd_array->len; i++) {
		item = g_ptr_array_index (priv->cmd_array, i);
		g_string_append_printf (string, " * %s\n", item->name);
	}
	g_set_error_literal (error, 1, 0, string->str);
	g_string_free (string, TRUE);
out:
	return ret;
}

/**
 * cd_util_get_devices:
 **/
static gboolean
cd_util_get_devices (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device;
	gboolean ret = TRUE;
	GPtrArray *array = NULL;
	guint i;

	/* execute sync method */
	array = cd_client_get_devices_sync (priv->client, NULL, error);
	if (array == NULL) {
		ret = FALSE;
		goto out;
	}
	for (i = 0; i < array->len; i++) {
		device = g_ptr_array_index (array, i);
		ret = cd_device_connect_sync (device, NULL, error);
		if (!ret)
			goto out;
		cd_util_show_device (device);
		if (i != array->len - 1)
			g_print ("\n");
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	return ret;
}

/**
 * cd_util_get_devices_by_kind:
 **/
static gboolean
cd_util_get_devices_by_kind (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device;
	gboolean ret = TRUE;
	GPtrArray *array = NULL;
	guint i;

	if (g_strv_length (values) < 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device kind "
				     "e.g. 'printer'");
		goto out;
	}

	/* execute sync method */
	array = cd_client_get_devices_by_kind_sync (priv->client,
			cd_device_kind_from_string (values[0]),
			NULL,
			error);
	if (array == NULL) {
		ret = FALSE;
		goto out;
	}
	for (i = 0; i < array->len; i++) {
		device = g_ptr_array_index (array, i);
		ret = cd_device_connect_sync (device, NULL, error);
		if (!ret)
			goto out;
		cd_util_show_device (device);
		if (i != array->len - 1)
			g_print ("\n");
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	return ret;
}

/**
 * cd_util_get_profiles:
 **/
static gboolean
cd_util_get_profiles (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdProfile *profile;
	gboolean ret = TRUE;
	GPtrArray *array = NULL;
	guint i;

	/* execute sync method */
	array = cd_client_get_profiles_sync (priv->client, NULL, error);
	if (array == NULL) {
		ret = FALSE;
		goto out;
	}
	for (i = 0; i < array->len; i++) {
		profile = g_ptr_array_index (array, i);
		ret = cd_profile_connect_sync (profile, NULL, error);
		if (!ret)
			goto out;
		cd_util_show_profile (profile);
		if (i != array->len - 1)
			g_print ("\n");
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	return ret;
}

/**
 * cd_util_get_sensors:
 **/
static gboolean
cd_util_get_sensors (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdSensor *sensor;
	gboolean ret = TRUE;
	GPtrArray *array = NULL;
	guint i;

	/* execute sync method */
	array = cd_client_get_sensors_sync (priv->client, NULL, error);
	if (array == NULL) {
		ret = FALSE;
		goto out;
	}
	if (array->len == 0) {
		ret = FALSE;
		/* TRANSLATORS: the user does not have a colorimeter attached */
		g_set_error_literal (error, 1, 0,
				     _("There are no supported sensors attached"));
		goto out;
	}
	for (i = 0; i < array->len; i++) {
		sensor = g_ptr_array_index (array, i);
		ret = cd_sensor_connect_sync (sensor, NULL, error);
		if (!ret)
			goto out;
		cd_util_show_sensor (sensor);
		if (i != array->len - 1)
			g_print ("\n");
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	return ret;
}

/**
 * cd_util_sensor_lock:
 **/
static gboolean
cd_util_sensor_lock (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdSensor *sensor;
	gboolean ret = TRUE;
	GMainLoop *loop = NULL;
	GPtrArray *array = NULL;
	guint i;

	/* execute sync method */
	array = cd_client_get_sensors_sync (priv->client, NULL, error);
	if (array == NULL) {
		ret = FALSE;
		goto out;
	}
	if (array->len == 0) {
		ret = FALSE;
		/* TRANSLATORS: the user does not have a colorimeter attached */
		g_set_error_literal (error, 1, 0,
				     _("There are no supported sensors attached"));
		goto out;
	}
	for (i = 0; i < array->len; i++) {
		sensor = g_ptr_array_index (array, i);

		ret = cd_sensor_connect_sync (sensor, NULL, error);
		if (!ret)
			goto out;

		/* lock */
		ret = cd_sensor_lock_sync (sensor,
					   NULL,
					   error);
		if (!ret)
			goto out;
	}

	/* spin */
	loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (loop);
out:
	if (loop != NULL)
		g_main_loop_unref (loop);
	if (array != NULL)
		g_ptr_array_unref (array);
	return ret;
}

/**
 * cd_util_get_sensor_reading:
 **/
static gboolean
cd_util_get_sensor_reading (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdColorXYZ *xyz;
	CdSensorCap cap;
	CdSensor *sensor;
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GPtrArray *array = NULL;
	guint i;
	guint j;

	if (g_strv_length (values) < 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device type "
				     "e.g. 'lcd'");
		goto out;
	}

	/* execute sync method */
	array = cd_client_get_sensors_sync (priv->client, NULL, error);
	if (array == NULL) {
		ret = FALSE;
		goto out;
	}
	if (array->len == 0) {
		ret = FALSE;
		/* TRANSLATORS: the user does not have a colorimeter attached */
		g_set_error_literal (error, 1, 0,
				     _("There are no supported sensors attached"));
		goto out;
	}
	cap = cd_sensor_cap_from_string (values[0]);
	for (i = 0; i < array->len; i++) {
		sensor = g_ptr_array_index (array, i);

		ret = cd_sensor_connect_sync (sensor, NULL, error);
		if (!ret)
			goto out;

		/* TRANSLATORS: this is the sensor title */
		g_print ("%s: %s - %s\n", _("Sensor"),
			 cd_sensor_get_vendor (sensor),
			 cd_sensor_get_model (sensor));

		/* lock */
		ret = cd_sensor_lock_sync (sensor,
					   NULL,
					   error);
		if (!ret)
			goto out;

		/* get 3 samples sync */
		for (j = 1; j < 4; j++) {
			xyz = cd_sensor_get_sample_sync (sensor,
							 cap,
							 NULL,
							 &error_local);
			if (xyz == NULL) {
				if (g_error_matches (error_local,
						     CD_SENSOR_ERROR,
						     CD_SENSOR_ERROR_REQUIRED_POSITION_CALIBRATE)) {
					/* TRANSLATORS: the user needs to change something on the device */
					g_print ("%s\n", _("Set the device to the calibrate position and press enter."));
					getchar ();
					j--;
					g_clear_error (&error_local);
					continue;
				} else if (g_error_matches (error_local,
							    CD_SENSOR_ERROR,
							    CD_SENSOR_ERROR_REQUIRED_POSITION_SURFACE)) {
					/* TRANSLATORS: the user needs to change something on the device */
					g_print ("%s\n", _("Set the device to the surface position and press enter."));
					getchar ();
					j--;
					g_clear_error (&error_local);
					continue;
				} else {
					g_propagate_error (error,
							   error_local);
					ret = FALSE;
					goto out;
				}
			}

			/* TRANSLATORS: this is the XYZ color value */
			g_print ("%s XYZ : %f, %f, %f\n",
				 _("Color"),
				 xyz->X, xyz->Y, xyz->Z);
			cd_color_xyz_free (xyz);
		}

		/* unlock */
		ret = cd_sensor_unlock_sync (sensor,
					     NULL,
					     error);
		if (!ret)
			goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	return ret;
}

/**
 * cd_util_sensor_set_options:
 **/
static gboolean
cd_util_sensor_set_options (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdSensor *sensor;
	gboolean ret = TRUE;
	gchar *endptr = NULL;
	gdouble val;
	GHashTable *options = NULL;
	GPtrArray *array = NULL;
	guint i;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected key value "
				     "e.g. 'remote-profile-hash' 'deadbeef'");
		goto out;
	}

	/* execute sync method */
	array = cd_client_get_sensors_sync (priv->client, NULL, error);
	if (array == NULL) {
		ret = FALSE;
		goto out;
	}
	if (array->len == 0) {
		ret = FALSE;
		/* TRANSLATORS: the user does not have a colorimeter attached */
		g_set_error_literal (error, 1, 0,
				     _("There are no supported sensors attached"));
		goto out;
	}

	/* prepare options for each sensor
	 * NOTE: we're guessing the types here */
	options = g_hash_table_new (g_str_hash, g_str_equal);
	val = g_ascii_strtod (values[1], &endptr);
	if (endptr == NULL || endptr[0] == '\0') {
		g_hash_table_insert (options,
				     values[0],
				     g_variant_new_double (val));
	} else {
		g_hash_table_insert (options,
				     values[0],
				     g_variant_new_string (values[1]));
	}

	for (i = 0; i < array->len; i++) {
		sensor = g_ptr_array_index (array, i);

		ret = cd_sensor_connect_sync (sensor, NULL, error);
		if (!ret)
			goto out;

		/* lock */
		ret = cd_sensor_lock_sync (sensor,
					   NULL,
					   error);
		if (!ret)
			goto out;

		/* TRANSLATORS: this is the sensor title */
		g_print ("%s: %s - %s\n", _("Sensor"),
			 cd_sensor_get_vendor (sensor),
			 cd_sensor_get_model (sensor));

		/* set the options set */
		ret = cd_sensor_set_options_sync (sensor,
						  options,
						  NULL,
						  error);
		if (!ret)
			goto out;

		/* unlock */
		ret = cd_sensor_unlock_sync (sensor,
					     NULL,
					     error);
		if (!ret)
			goto out;
	}
out:
	if (options != NULL)
		g_hash_table_unref (options);
	if (array != NULL)
		g_ptr_array_unref (array);
	return ret;
}

/**
 * cd_util_create_device:
 **/
static gboolean
cd_util_create_device (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;
	guint mask;
	GHashTable *device_props = NULL;

	if (g_strv_length (values) < 3) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device id, scope, kind "
				     "e.g. 'epson-stylus-800 disk display'");
		goto out;
	}

	/* execute sync method */
	mask = cd_object_scope_from_string (values[1]);
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup (CD_DEVICE_PROPERTY_KIND),
			     g_strdup (values[2]));
	device = cd_client_create_device_sync (priv->client, values[0],
					       mask,
					       device_props,
					       NULL,
					       error);
	if (device == NULL) {
		ret = FALSE;
		goto out;
	}
	g_print ("Created device:\n");
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;
	cd_util_show_device (device);
out:
	if (device_props != NULL)
		g_hash_table_unref (device_props);
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_find_device:
 **/
static gboolean
cd_util_find_device (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device id "
				     "e.g. 'epson-stylus-800'");
		goto out;
	}

	/* execute sync method */
	device = cd_client_find_device_sync (priv->client, values[0],
					     NULL, error);
	if (device == NULL) {
		ret = FALSE;
		goto out;
	}
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;
	cd_util_show_device (device);
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_find_device_by_property:
 **/
static gboolean
cd_util_find_device_by_property (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected key value "
				     "e.g. 'XRANDR_name' 'lvds'");
		goto out;
	}

	/* execute sync method */
	device = cd_client_find_device_by_property_sync (priv->client,
							 values[0],
							 values[1],
							 NULL,
							 error);
	if (device == NULL) {
		ret = FALSE;
		goto out;
	}
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;
	cd_util_show_device (device);
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_find_profile:
 **/
static gboolean
cd_util_find_profile (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected profile id "
				     "e.g. 'epson-rgb'");
		goto out;
	}

	/* execute sync method */
	profile = cd_client_find_profile_sync (priv->client, values[0],
					       NULL, error);
	if (profile == NULL) {
		ret = FALSE;
		goto out;
	}
	ret = cd_profile_connect_sync (profile, NULL, error);
	if (!ret)
		goto out;
	cd_util_show_profile (profile);
out:
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_find_profile_by_filename:
 **/
static gboolean
cd_util_find_profile_by_filename (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected profile filename");
		goto out;
	}

	/* execute sync method */
	profile = cd_client_find_profile_by_filename_sync (priv->client,
							   values[0],
							   NULL, error);
	if (profile == NULL) {
		ret = FALSE;
		goto out;
	}
	ret = cd_profile_connect_sync (profile, NULL, error);
	if (!ret)
		goto out;
	cd_util_show_profile (profile);
out:
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_get_standard_space:
 **/
static gboolean
cd_util_get_standard_space (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected standard space "
				     "e.g. 'adobe-rgb'");
		goto out;
	}

	/* execute sync method */
	profile = cd_client_get_standard_space_sync (priv->client,
						     cd_standard_space_from_string (values[0]),
						     NULL,
						     error);
	if (profile == NULL) {
		ret = FALSE;
		goto out;
	}
	ret = cd_profile_connect_sync (profile, NULL, error);
	if (!ret)
		goto out;
	cd_util_show_profile (profile);
out:
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_create_profile:
 **/
static gboolean
cd_util_create_profile (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdProfile *profile = NULL;
	gboolean ret = TRUE;
	guint mask;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected profile id, scope "
				     "e.g. 'epson-rgb disk'");
		goto out;
	}

	/* execute sync method */
	mask = cd_object_scope_from_string (values[1]);
	profile = cd_client_create_profile_sync (priv->client, values[0],
						 mask, NULL, NULL, error);
	if (profile == NULL) {
		ret = FALSE;
		goto out;
	}
	ret = cd_profile_connect_sync (profile, NULL, error);
	if (!ret)
		goto out;
	g_print ("Created profile:\n");
	cd_util_show_profile (profile);
out:
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_device_add_profile:
 **/
static gboolean
cd_util_device_add_profile (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, profile path "
				     "e.g. '/org/device/foo /org/profile/bar'");
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[0])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[0]);
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[1])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[1]);
		goto out;
	}

	device = cd_device_new_with_object_path (values[0]);
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;
	profile = cd_profile_new_with_object_path (values[1]);
	ret = cd_device_add_profile_sync (device,
					  CD_DEVICE_RELATION_HARD,
					  profile,
					  NULL,
					  error);
	if (!ret)
		goto out;
out:
	if (device != NULL)
		g_object_unref (device);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_device_make_profile_default:
 **/
static gboolean
cd_util_device_make_profile_default (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, profile path "
				     "e.g. '/org/device/foo /org/profile/bar'");
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[0])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[0]);
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[1])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[1]);
		goto out;
	}

	device = cd_device_new_with_object_path (values[0]);
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;
	profile = cd_profile_new_with_object_path (values[1]);
	ret = cd_device_make_profile_default_sync (device, profile,
						   NULL, error);
	if (!ret)
		goto out;
out:
	if (device != NULL)
		g_object_unref (device);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_delete_device:
 **/
static gboolean
cd_util_delete_device (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;
	CdDevice *device = NULL;

	if (g_strv_length (values) < 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path "
				     "e.g. '/org/devices/foo'");
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[0])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[0]);
		goto out;
	}

	device = cd_device_new_with_object_path (values[0]);
	ret = cd_client_delete_device_sync (priv->client, device,
					    NULL, error);
	if (!ret)
		goto out;
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_delete_profile:
 **/
static gboolean
cd_util_delete_profile (CdUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;
	CdProfile *profile = NULL;

	if (g_strv_length (values) < 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected profile path "
				     "e.g. '/org/profiles/bar'");
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[0])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[0]);
		goto out;
	}

	profile = cd_profile_new_with_object_path (values[0]);
	ret = cd_client_delete_profile_sync (priv->client, profile,
					     NULL, error);
	if (!ret)
		goto out;
out:
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_profile_set_property:
 **/
static gboolean
cd_util_profile_set_property (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 3) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected profile path key value "
				     "e.g. '/org/profile/foo qualifier RGB.Matte.300dpi'");
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[0])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[0]);
		goto out;
	}

	profile = cd_profile_new_with_object_path (values[0]);
	ret = cd_profile_connect_sync (profile, NULL, error);
	if (!ret)
		goto out;
	ret = cd_profile_set_property_sync (profile,
					    values[1],
					    values[2],
					    NULL,
					    error);
	if (!ret)
		goto out;
out:
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_device_set_model:
 **/
static gboolean
cd_util_device_set_model (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, model "
				     "e.g. '/org/devices/bar \"Stylus 800\"'");
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[0])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[0]);
		goto out;
	}

	device = cd_device_new_with_object_path (values[0]);
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;
	ret = cd_device_set_model_sync (device, values[1],
					NULL, error);
	if (!ret)
		goto out;
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_device_set_enabled:
 **/
static gboolean
cd_util_device_set_enabled (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, True|False");
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[0])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[0]);
		goto out;
	}

	device = cd_device_new_with_object_path (values[0]);
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;
	ret = cd_device_set_enabled_sync (device,
					  g_strcmp0 (values[1], "True") == 0,
					  NULL,
					  error);
	if (!ret)
		goto out;
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_device_get_default_profile:
 **/
static gboolean
cd_util_device_get_default_profile (CdUtilPrivate *priv,
				    gchar **values,
				    GError **error)
{
	CdDevice *device = NULL;
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path "
				     "e.g. '/org/devices/bar'");
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[0])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[0]);
		goto out;
	}

	device = cd_device_new_with_object_path (values[0]);
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;
	profile = cd_device_get_default_profile (device);
	if (profile == NULL) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "There is no assigned profile for %s",
			     values[0]);
		goto out;
	}
	ret = cd_profile_connect_sync (profile, NULL, error);
	if (!ret)
		goto out;
	cd_util_show_profile (profile);
out:
	if (device != NULL)
		g_object_unref (device);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_device_set_vendor:
 **/
static gboolean
cd_util_device_set_vendor (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, vendor "
				     "e.g. '/org/devices/bar Epson'");
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[0])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[0]);
		goto out;
	}

	device = cd_device_new_with_object_path (values[0]);
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;
	ret = cd_device_set_vendor_sync (device, values[1],
					 NULL, error);
	if (!ret)
		goto out;
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_device_set_serial:
 **/
static gboolean
cd_util_device_set_serial (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, serial "
				     "e.g. '/org/devices/bar 00001234'");
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[0])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[0]);
		goto out;
	}

	device = cd_device_new_with_object_path (values[0]);
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;
	ret = cd_device_set_serial_sync (device, values[1],
					 NULL, error);
	if (!ret)
		goto out;
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_device_set_kind:
 **/
static gboolean
cd_util_device_set_kind (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, kind "
				     "e.g. '/org/devices/bar printer'");
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[0])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[0]);
		goto out;
	}

	device = cd_device_new_with_object_path (values[0]);
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;
	ret = cd_device_set_kind_sync (device, cd_device_kind_from_string (values[1]),
				       NULL, error);
	if (!ret)
		goto out;
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_device_inhibit:
 **/
static gboolean
cd_util_device_inhibit (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device = NULL;
	gboolean ret = TRUE;
	GMainLoop *loop = NULL;
	gint timeout;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path timeout (use 0 for 'never') "
				     "e.g. '/org/devices/epson-800' 60");
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[0])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[0]);
		goto out;
	}

	/* check timeout is valid */
	timeout = atoi (values[1]);
	if (timeout < 0) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid timeout: %s",
			     values[1]);
		goto out;
	}

	device = cd_device_new_with_object_path (values[0]);
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;
	ret = cd_device_profiling_inhibit_sync (device, NULL, error);
	if (!ret)
		goto out;

	/* wait for ctrl-c, as inhibit will be destroyed when the
	 * colormgr tool is finished */
	loop = g_main_loop_new (NULL, FALSE);
	if (timeout > 0) {
		g_timeout_add_seconds (timeout,
				       cd_util_idle_loop_quit_cb,
				       loop);
	}
	g_main_loop_run (loop);
out:
	if (loop != NULL)
		g_main_loop_unref (loop);
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cd_util_device_get_profile_for_qualifiers:
 **/
static gboolean
cd_util_device_get_profile_for_qualifiers (CdUtilPrivate *priv,
					   gchar **values,
					   GError **error)
{
	CdDevice *device = NULL;
	CdProfile *profile = NULL;
	gboolean ret = TRUE;

	if (g_strv_length (values) < 2) {
		ret = FALSE;
		g_set_error_literal (error,
				     1, 0,
				     "Not enough arguments, "
				     "expected device path, qualifier "
				     "e.g. '/org/devices/bar *.*.300dpi'");
		goto out;
	}

	/* check is valid object path */
	if (!g_variant_is_object_path (values[0])) {
		ret = FALSE;
		g_set_error (error,
			     1, 0,
			     "Not a valid object path: %s",
			     values[0]);
		goto out;
	}

	device = cd_device_new_with_object_path (values[0]);
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;
	profile = cd_device_get_profile_for_qualifiers_sync (device,
							     (const gchar **) &values[1],
							     NULL,
							     error);
	if (profile == NULL) {
		ret = FALSE;
		goto out;
	}
	ret = cd_profile_connect_sync (profile, NULL, error);
	if (!ret)
		goto out;
	cd_util_show_profile (profile);
out:
	if (device != NULL)
		g_object_unref (device);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_ignore_cb:
 **/
static void
cd_util_ignore_cb (const gchar *log_domain, GLogLevelFlags log_level,
		   const gchar *message, gpointer user_data)
{
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	CdUtilPrivate *priv;
	gboolean ret;
	gboolean verbose = FALSE;
	gboolean version = FALSE;
	gchar *cmd_descriptions = NULL;
	GError *error = NULL;
	guint retval = 1;
	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: command line option */
			_("Show extra debugging information"), NULL },
		{ "version", '\0', 0, G_OPTION_ARG_NONE, &version,
			/* TRANSLATORS: command line option */
			_("Show client and daemon versions"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();

	/* create helper object */
	priv = g_new0 (CdUtilPrivate, 1);

	/* add commands */
	priv->cmd_array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_util_item_free);
	cd_util_add (priv->cmd_array,
		     "get-devices",
		     /* TRANSLATORS: command description */
		     _("Gets all the color managed devices"),
		     cd_util_get_devices);
	cd_util_add (priv->cmd_array,
		     "get-devices-by-kind",
		     /* TRANSLATORS: command description */
		     _("Gets all the color managed devices of a specific kind"),
		     cd_util_get_devices_by_kind);
	cd_util_add (priv->cmd_array,
		     "get-profiles",
		     /* TRANSLATORS: command description */
		     _("Gets all the available color profiles"),
		     cd_util_get_profiles);
	cd_util_add (priv->cmd_array,
		     "get-sensors",
		     /* TRANSLATORS: command description */
		     _("Gets all the available color sensors"),
		     cd_util_get_sensors);
	cd_util_add (priv->cmd_array,
		     "get-sensor-reading",
		     /* TRANSLATORS: command description */
		     _("Gets a reading from a sensor"),
		     cd_util_get_sensor_reading);
	cd_util_add (priv->cmd_array,
		     "sensor-lock",
		     /* TRANSLATORS: command description */
		     _("Locks the color sensor"),
		     cd_util_sensor_lock);
	cd_util_add (priv->cmd_array,
		     "sensor-set-options",
		     /* TRANSLATORS: command description */
		     _("Sets one or more sensor options"),
		     cd_util_sensor_set_options);
	cd_util_add (priv->cmd_array,
		     "create-device",
		     /* TRANSLATORS: command description */
		     _("Create a device"),
		     cd_util_create_device);
	cd_util_add (priv->cmd_array,
		     "find-device",
		     /* TRANSLATORS: command description */
		     _("Find a device"),
		     cd_util_find_device);
	cd_util_add (priv->cmd_array,
		     "find-device-by-property",
		     /* TRANSLATORS: command description */
		     _("Find a device that has a specific property"),
		     cd_util_find_device_by_property);
	cd_util_add (priv->cmd_array,
		     "find-profile",
		     /* TRANSLATORS: command description */
		     _("Find a profile"),
		     cd_util_find_profile);
	cd_util_add (priv->cmd_array,
		     "find-profile-by-filename",
		     /* TRANSLATORS: command description */
		     _("Find a profile by filename"),
		     cd_util_find_profile_by_filename);
	cd_util_add (priv->cmd_array,
		     "get-standard-space",
		     /* TRANSLATORS: command description */
		     _("Get a standard colorspace"),
		     cd_util_get_standard_space);
	cd_util_add (priv->cmd_array,
		     "create-profile",
		     /* TRANSLATORS: command description */
		     _("Create a profile"),
		     cd_util_create_profile);
	cd_util_add (priv->cmd_array,
		     "device-add-profile",
		     /* TRANSLATORS: command description */
		     _("Add a profile to a device"),
		     cd_util_device_add_profile);
	cd_util_add (priv->cmd_array,
		     "device-make-profile-default",
		     /* TRANSLATORS: command description */
		     _("Makes a profile default for a device"),
		     cd_util_device_make_profile_default);
	cd_util_add (priv->cmd_array,
		     "delete-device",
		     /* TRANSLATORS: command description */
		     _("Deletes a device"),
		     cd_util_delete_device);
	cd_util_add (priv->cmd_array,
		     "delete-profile",
		     /* TRANSLATORS: command description */
		     _("Deletes a profile"),
		     cd_util_delete_profile);
	cd_util_add (priv->cmd_array,
		     "profile-set-property",
		     /* TRANSLATORS: command description */
		     _("Sets extra properties on the profile"),
		     cd_util_profile_set_property);
	cd_util_add (priv->cmd_array,
		     "device-set-model",
		     /* TRANSLATORS: command description */
		     _("Sets the device model"),
		     cd_util_device_set_model);
	cd_util_add (priv->cmd_array,
		     "device-set-enabled",
		     /* TRANSLATORS: command description */
		     _("Enables or disables the device"),
		     cd_util_device_set_enabled);
	cd_util_add (priv->cmd_array,
		     "device-get-default-profile",
		     /* TRANSLATORS: command description */
		     _("Gets the default profile for a device"),
		     cd_util_device_get_default_profile);
	cd_util_add (priv->cmd_array,
		     "device-set-vendor",
		     /* TRANSLATORS: command description */
		     _("Sets the device vendor"),
		     cd_util_device_set_vendor);
	cd_util_add (priv->cmd_array,
		     "device-set-serial",
		     /* TRANSLATORS: command description */
		     _("Sets the device serial"),
		     cd_util_device_set_serial);
	cd_util_add (priv->cmd_array,
		     "device-set-kind",
		     /* TRANSLATORS: command description */
		     _("Sets the device kind"),
		     cd_util_device_set_kind);
	cd_util_add (priv->cmd_array,
		     "device-inhibit",
		     /* TRANSLATORS: command description */
		     _("Inhibits color profiles for this device"),
		     cd_util_device_inhibit);
	cd_util_add (priv->cmd_array,
		     "device-get-profile-for-qualifier",
		     /* TRANSLATORS: command description */
		     _("Returns all the profiles that match a qualifier"),
		     cd_util_device_get_profile_for_qualifiers);

	/* sort by command name */
	g_ptr_array_sort (priv->cmd_array,
			  (GCompareFunc) cd_sort_command_name_cb);

	/* get a list of the commands */
	priv->context = g_option_context_new (NULL);
	cmd_descriptions = cd_util_get_descriptions (priv->cmd_array);
	g_option_context_set_summary (priv->context, cmd_descriptions);

	/* TRANSLATORS: program name */
	g_set_application_name (_("Color Management"));
	g_option_context_add_main_entries (priv->context, options, NULL);
	ret = g_option_context_parse (priv->context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: the user didn't read the man page */
		g_print ("%s: %s\n",
			 _("Failed to parse arguments"),
			 error->message);
		g_error_free (error);
		goto out;
	}

	/* set verbose? */
	if (verbose) {
		g_setenv ("COLORD_VERBOSE", "1", FALSE);
	} else {
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
				   cd_util_ignore_cb, NULL);
	}

	/* get connection to colord */
	priv->client = cd_client_new ();
	ret = cd_client_connect_sync (priv->client, NULL, &error);
	if (!ret) {
		/* TRANSLATORS: no colord available */
		g_print ("%s %s\n", _("No connection to colord:"),
			 error->message);
		g_error_free (error);
		goto out;
	}

	/* get version */
	if (version) {
		g_print ("%s\t%s\n", _("Client version:"),
			 PACKAGE_VERSION);
		g_print ("%s\t%s\n", _("Daemon version:"),
			 cd_client_get_daemon_version (priv->client));
		goto out;
	}

	/* run the specified command */
	ret = cd_util_run (priv, argv[1], (gchar**) &argv[2], &error);
	if (!ret) {
		g_print ("%s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	retval = 0;
out:
	if (priv != NULL) {
		if (priv->client != NULL)
			g_object_unref (priv->client);
		if (priv->cmd_array != NULL)
			g_ptr_array_unref (priv->cmd_array);
		g_option_context_free (priv->context);
		g_free (priv);
	}
	g_free (cmd_descriptions);
	return retval;
}

