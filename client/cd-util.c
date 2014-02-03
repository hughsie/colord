/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2014 Richard Hughes <richard@hughsie.com>
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

#define CD_ERROR			1
#define CD_ERROR_INVALID_ARGUMENTS	0
#define CD_ERROR_NO_SUCH_CMD		1

typedef struct {
	CdClient		*client;
	GOptionContext		*context;
	GPtrArray		*cmd_array;
	gboolean		 value_only;
	gchar			**filters;
} CdUtilPrivate;

typedef gboolean (*CdUtilPrivateCb)	(CdUtilPrivate	*util,
					 gchar		**values,
					 GError		**error);

typedef struct {
	gchar		*name;
	gchar		*arguments;
	gchar		*description;
	CdUtilPrivateCb	 callback;
} CdUtilItem;

/**
 * cd_util_filter_attribute:
 **/
static gboolean
cd_util_filter_attribute (gchar **filters, const gchar *id)
{
	guint i;
	if (filters == NULL)
		return TRUE;
	for (i = 0; filters[i] != NULL; i++) {
		if (g_strcmp0 (filters[i], id) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * cd_util_print_field:
 **/
static void
cd_util_print_field (const gchar *title,
		     const gchar *filter_id,
		     CdUtilPrivate *priv,
		     const gchar *message)
{
	const guint padding = 15;
	guint i;
	guint len = 0;

	/* filter this id */
	if (!cd_util_filter_attribute (priv->filters, filter_id))
		return;

	/* bare value */
	if (priv->value_only) {
		g_print ("%s\n", message);
		return;
	}

	/* untranslated header and value */
	if (priv->filters != NULL) {
		g_print ("%s:%s\n", filter_id, message);
		return;
	}

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
cd_util_print_field_time (const gchar *title,
			  const gchar *filter_id,
			  CdUtilPrivate *priv,
			  gint64 usecs)
{
	gchar *str;
	GDateTime *datetime;

	datetime = g_date_time_new_from_unix_utc (usecs / G_USEC_PER_SEC);
	/* TRANSLATORS: this is the profile creation date strftime format */
	str = g_date_time_format (datetime, _("%B %e %Y, %I:%M:%S %p"));
	cd_util_print_field (title, filter_id, priv, str);
	g_date_time_unref (datetime);
	g_free (str);
}

/**
 * cd_util_show_owner:
 **/
static void
cd_util_show_owner (CdUtilPrivate *priv, guint uid)
{
	struct passwd *pw;
	pw = getpwuid (uid);
	/* TRANSLATORS: profile owner */
	cd_util_print_field (_("Owner"), "owner", priv, pw->pw_name);
}

/**
 * cd_util_show_profile:
 **/
static void
cd_util_show_profile (CdUtilPrivate *priv, CdProfile *profile)
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
			     "object-path", priv,
			     cd_profile_get_object_path (profile));
	cd_util_show_owner (priv, cd_profile_get_owner (profile));
	tmp = cd_profile_get_format (profile);
	if (tmp != NULL && tmp[0] != '\0') {
		/* TRANSLATORS: the profile format, e.g.
		 * ColorModel.OutputMode.OutputResolution */
		cd_util_print_field (_("Format"),
				     "format", priv,
				     tmp);
	}
	tmp = cd_profile_get_title (profile);
	if (tmp != NULL && tmp[0] != '\0') {
		/* TRANSLATORS: the profile title, e.g.
		 * "ColorMunki, HP Deskjet d1300 Series" */
		cd_util_print_field (_("Title"), "title", priv, tmp);
	}
	tmp = cd_profile_get_qualifier (profile);
	if (tmp != NULL && tmp[0] != '\0') {
		/* TRANSLATORS: the profile qualifier, e.g. RGB.Plain.300dpi */
		cd_util_print_field (_("Qualifier"), "qualifier", priv, tmp);
	}
	kind = cd_profile_get_kind (profile);
	if (kind != CD_PROFILE_KIND_UNKNOWN) {
		/* TRANSLATORS: the profile type, e.g. 'output' */
		cd_util_print_field (_("Type"),
				     "type", priv,
				     cd_profile_kind_to_string (kind));
	}
	colorspace = cd_profile_get_colorspace (profile);
	if (colorspace != CD_COLORSPACE_UNKNOWN) {
		/* TRANSLATORS: the profile colorspace, e.g. 'rgb' */
		cd_util_print_field (_("Colorspace"),
				     "colorspace", priv,
				     cd_colorspace_to_string (colorspace));
	}
	scope = cd_profile_get_scope (profile);
	if (scope != CD_OBJECT_SCOPE_UNKNOWN) {
		/* TRANSLATORS: the object scope, e.g. temp, disk, etc */
		cd_util_print_field (_("Scope"),
				     "scope", priv,
				     cd_object_scope_to_string (scope));
	}

	/* TRANSLATORS: if the profile has a Video Card Gamma Table lookup */
	cd_util_print_field (_("Gamma Table"),
			     "gamma-table", priv,
			     cd_profile_get_has_vcgt (profile) ? "Yes" : "No");

	/* TRANSLATORS: if the profile is installed for all users */
	cd_util_print_field (_("System Wide"),
			     "system-wide", priv,
			     cd_profile_get_is_system_wide (profile) ? "Yes" : "No");

	/* TRANSLATORS: profile filename */
	cd_util_print_field (_("Filename"),
			     "filename", priv,
			     cd_profile_get_filename (profile));

	/* TRANSLATORS: profile identifier */
	cd_util_print_field (_("Profile ID"),
			     "profile-id", priv,
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
		cd_util_print_field (_("Metadata"), "metadata", priv, str_tmp);
		g_free (str_tmp);
	}

	/* show warnings */
	warnings = cd_profile_get_warnings (profile);
	size = g_strv_length (warnings);
	for (i = 0; i < size; i++)
		cd_util_print_field (_("Warning"), "warnings", priv, warnings[i]);

	g_list_free (list);
	g_hash_table_unref (metadata);
}

/**
 * cd_util_show_device:
 **/
static void
cd_util_show_device (CdUtilPrivate *priv, CdDevice *device)
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
			     "object-path", priv,
			     cd_device_get_object_path (device));
	cd_util_show_owner (priv, cd_device_get_owner (device));

	/* TRANSLATORS: this is the time the device was registered
	 * with colord, and probably is the same as the system startup
	 * unless the device has been explicitly saved in the database */
	cd_util_print_field_time (_("Created"),
				  "created", priv,
				  cd_device_get_created (device));

	/* TRANSLATORS: this is the time of the last calibration or when
	 * the manufacturer-provided profile was assigned by the user */
	cd_util_print_field_time (_("Modified"),
				  "modified", priv,
				  cd_device_get_modified (device));

	/* TRANSLATORS: the device type, e.g. "printer" */
	cd_util_print_field (_("Type"),
			    "type", priv,
			     cd_device_kind_to_string (cd_device_get_kind (device)));

	/* TRANSLATORS: the device enabled state */
	cd_util_print_field (_("Enabled"),
			     "enabled", priv,
			     cd_device_get_enabled (device) ? "Yes" : "No");

	/* TRANSLATORS: if the device is embedded into the computer and
	 * cannot be removed */
	cd_util_print_field (_("Embedded"),
			     "embedded", priv,
			     cd_device_get_embedded (device) ? "Yes" : "No");

	/* TRANSLATORS: the device model */
	cd_util_print_field (_("Model"),
			     "model", priv,
			     cd_device_get_model (device));

	/* TRANSLATORS: the device vendor */
	cd_util_print_field (_("Vendor"),
			     "vendor", priv,
			     cd_device_get_vendor (device));

	/* TRANSLATORS: the device inhibitors */
	str_tmp = g_strjoinv (", ", (gchar **) cd_device_get_profiling_inhibitors (device));
	cd_util_print_field (_("Inhibitors"), "inhibitors", priv, str_tmp);
	g_free (str_tmp);

	/* TRANSLATORS: the device serial number */
	cd_util_print_field (_("Serial"),
			     "serial", priv,
			     cd_device_get_serial (device));

	/* TRANSLATORS: the device seat identifier, where a seat is
	 * defined as a monitor, keyboard and mouse.
	 * For instance, in a public library one central computer can
	 * have 3 keyboards, 3 displays and 3 mice plugged in and with
	 * systemd these can be setup as three independant seats with
	 * different sessions running on them */
	cd_util_print_field (_("Seat"),
			     "seat", priv,
			     cd_device_get_seat (device));

	tmp = cd_device_get_format (device);
	if (tmp != NULL && tmp[0] != '\0') {
		/* TRANSLATORS: the device format, e.g.
		 * ColorModel.OutputMode.OutputResolution */
		cd_util_print_field (_("Format"), "format", priv, tmp);
	}

	scope = cd_device_get_scope (device);
	if (scope != CD_OBJECT_SCOPE_UNKNOWN) {
		/* TRANSLATORS: the object scope, e.g. temp, disk, etc */
		cd_util_print_field (_("Scope"),
				     "scope", priv,
				     cd_object_scope_to_string (scope));
	}

	/* TRANSLATORS: the device colorspace, e.g. "rgb" */
	cd_util_print_field (_("Colorspace"),
			     "colorspace", priv,
			     cd_colorspace_to_string (cd_device_get_colorspace (device)));

	/* TRANSLATORS: the device identifier */
	cd_util_print_field (_("Device ID"),
			     "device-id", priv,
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
					     "profiles", priv,
					     cd_profile_get_object_path (profile_tmp));
			cd_util_print_field (NULL,
					     "profiles-error", priv,
					     error->message);
			g_clear_error (&error);
		} else {
			cd_util_print_field (str_tmp,
					     "profiles", priv,
					     cd_profile_get_id (profile_tmp));
			cd_util_print_field (NULL,
					     "profiles-filename", priv,
					     cd_profile_get_filename (profile_tmp));
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
		cd_util_print_field (_("Metadata"), "metadata", priv, str_tmp);
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
cd_util_show_sensor (CdUtilPrivate *priv, CdSensor *sensor)
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
			     "object-path", priv,
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
				     "type", priv,
				     cd_sensor_kind_to_string (kind));
	}

	state = cd_sensor_get_state (sensor);
	if (state != CD_SENSOR_STATE_UNKNOWN) {
		/* TRANSLATORS: the sensor state, e.g. 'idle' */
		cd_util_print_field (_("State"),
				     "state", priv,
				     cd_sensor_state_to_string (state));
	}

	tmp = cd_sensor_get_serial (sensor);
	if (tmp != NULL) {
		/* TRANSLATORS: sensor serial */
		cd_util_print_field (_("Serial number"),
				     "serial-number", priv,
				     tmp);
	}

	tmp = cd_sensor_get_model (sensor);
	if (tmp != NULL) {
		/* TRANSLATORS: sensor model */
		cd_util_print_field (_("Model"),
				     "model", priv,
				     tmp);
	}

	tmp = cd_sensor_get_vendor (sensor);
	if (tmp != NULL) {
		/* TRANSLATORS: sensor vendor */
		cd_util_print_field (_("Vendor"),
				     "vendor", priv,
				     tmp);
	}

	/* TRANSLATORS: sensor identifier */
	cd_util_print_field (_("Sensor ID"),
			     "sensor-id", priv,
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
		cd_util_print_field (_("Options"), "options", priv, str_tmp);
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
		cd_util_print_field (_("Metadata"), "metadata", priv, str_tmp);
		g_free (str_tmp);
	}

	/* TRANSLATORS: if the sensor has a colord native driver */
	cd_util_print_field (_("Native"),
			     "native", priv,
			     cd_sensor_get_native (sensor) ? "Yes" : "No");

	/* TRANSLATORS: if the sensor is locked */
	cd_util_print_field (_("Locked"),
			     "locked", priv,
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
	cd_util_print_field (_("Capabilities"),
			     "capabilities", priv,
			     caps_str->str);

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
	g_free (item->arguments);
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
cd_util_add (GPtrArray *array,
	     const gchar *name,
	     const gchar *arguments,
	     const gchar *description,
	     CdUtilPrivateCb callback)
{
	gchar **names;
	guint i;
	CdUtilItem *item;

	g_return_if_fail (name != NULL);
	g_return_if_fail (description != NULL);
	g_return_if_fail (callback != NULL);

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
		item->arguments = g_strdup (arguments);
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
	const guint max_len = 35;
	CdUtilItem *item;
	GString *string;

	/* print each command */
	string = g_string_new ("");
	for (i = 0; i < array->len; i++) {
		item = g_ptr_array_index (array, i);
		g_string_append (string, "  ");
		g_string_append (string, item->name);
		len = strlen (item->name) + 2;
		if (item->arguments != NULL) {
			g_string_append (string, " ");
			g_string_append (string, item->arguments);
			len += strlen (item->arguments) + 1;
		}
		if (len < max_len) {
			for (j = len; j < max_len + 1; j++)
				g_string_append_c (string, ' ');
			g_string_append (string, item->description);
			g_string_append_c (string, '\n');
		} else {
			g_string_append_c (string, '\n');
			for (j = 0; j < max_len + 1; j++)
				g_string_append_c (string, ' ');
			g_string_append (string, item->description);
			g_string_append_c (string, '\n');
		}
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
		g_string_append_printf (string, " * %s %s\n",
					item->name,
					item->arguments ? item->arguments : "");
	}
	g_set_error_literal (error, CD_ERROR, CD_ERROR_NO_SUCH_CMD, string->str);
	g_string_free (string, TRUE);
out:
	return ret;
}

/**
 * cd_util_dump:
 **/
static gboolean
cd_util_dump (CdUtilPrivate *priv, gchar **values, GError **error)
{
	CdDevice *device;
	CdProfile *profile;
	const gchar *argv[] = { "sqlite3", "/var/lib/colord/mapping.db", ".dump", NULL };
	gboolean ret = TRUE;
	gchar *mapping_db = NULL;
	gchar *tmp;
	GDateTime *dt;
	GError *error_local = NULL;
	GPtrArray *devices = NULL;
	GPtrArray *profiles = NULL;
	GString *str;
	guint i;

	/* header */
	str = g_string_new ("");
	dt = g_date_time_new_now_local ();
	tmp = g_date_time_format (dt, "%F %T");
	g_string_append_printf (str, "date-time\t%s\n", tmp);
	g_free (tmp);

	/* dump versions and system info */
	g_string_append_printf (str, "client-version\t%s\n", PACKAGE_VERSION);
	g_string_append_printf (str, "daemon-version\t%s\n",
				cd_client_get_daemon_version (priv->client));
	g_string_append_printf (str, "system-vendor\t%s\n",
				cd_client_get_system_vendor (priv->client));
	g_string_append_printf (str, "system-model\t%s\n",
				cd_client_get_system_model (priv->client));

	/* get profiles */
	profiles = cd_client_get_profiles_sync (priv->client, NULL, error);
	if (profiles == NULL) {
		ret = FALSE;
		goto out;
	}
	g_string_append_printf (str, "no-profile\t%i\n", profiles->len);
	for (i = 0; i < profiles->len; i++) {
		profile = g_ptr_array_index (profiles, i);
		ret = cd_profile_connect_sync (profile, NULL, &error_local);
		if (!ret) {
			g_string_append_printf (str, "profile-%02i\t%s\tERROR: %s\n",
						i,
						cd_profile_get_object_path (profile),
						error_local->message);
			g_clear_error (&error_local);
		}
		g_string_append_printf (str, "profile-%02i\t%s\t%s\n",
					i,
					cd_profile_get_id (profile),
					cd_profile_get_filename (profile));
	}

	/* get devices */
	devices = cd_client_get_devices_sync (priv->client, NULL, error);
	if (devices == NULL) {
		ret = FALSE;
		goto out;
	}
	g_string_append_printf (str, "no-devices\t%i\n", devices->len);
	for (i = 0; i < devices->len; i++) {
		device = g_ptr_array_index (devices, i);
		ret = cd_device_connect_sync (device, NULL, &error_local);
		if (!ret) {
			g_string_append_printf (str, "device-%02i\t%s\tERROR: %s\n",
						i,
						cd_device_get_object_path (device),
						error_local->message);
			g_clear_error (&error_local);
		}
		profile = cd_device_get_default_profile (device);
		if (profile != NULL) {
			ret = cd_profile_connect_sync (profile, NULL, error);
			if (!ret)
				goto out;
		}
		g_string_append_printf (str, "device-%02i\t%s\t%s\n",
					i,
					profile != NULL ?
					 cd_profile_get_id (profile) :
					 "                                    ",
					cd_device_get_id (device));
	}

	/* get hard mapping data */
	ret = g_spawn_sync (NULL, (gchar **) argv, NULL,
			    G_SPAWN_SEARCH_PATH, NULL, NULL,
			    &mapping_db, NULL, NULL, error);
	if (!ret)
		goto out;
	g_string_append_printf (str, "mapping-db:\n%s\n", mapping_db);

	/* save file */
	g_string_truncate (str, str->len - 1);
	tmp = g_date_time_format (dt, "colord-%s.txt");
	ret = g_file_set_contents (tmp, str->str, -1, error);
	if (!ret)
		goto out;
	g_print ("%s", str->str);
	g_print ("--- Output file also saved to '%s' ---\n", tmp);
	g_free (tmp);
out:
	g_date_time_unref (dt);
	g_free (mapping_db);
	g_string_free (str, FALSE);
	if (devices != NULL)
		g_ptr_array_unref (devices);
	if (profiles != NULL)
		g_ptr_array_unref (profiles);
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
		cd_util_show_device (priv, device);
		if (i != array->len - 1 && !priv->value_only)
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
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
		cd_util_show_device (priv, device);
		if (i != array->len - 1 && !priv->value_only)
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
		cd_util_show_profile (priv, profile);
		if (i != array->len - 1 && !priv->value_only)
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
		g_set_error_literal (error, CD_ERROR, CD_ERROR_INVALID_ARGUMENTS,
				     _("There are no supported sensors attached"));
		goto out;
	}
	for (i = 0; i < array->len; i++) {
		sensor = g_ptr_array_index (array, i);
		ret = cd_sensor_connect_sync (sensor, NULL, error);
		if (!ret)
			goto out;
		cd_util_show_sensor (priv, sensor);
		if (i != array->len - 1 && !priv->value_only)
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
		g_set_error_literal (error, CD_ERROR, CD_ERROR_INVALID_ARGUMENTS,
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
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
		g_set_error_literal (error, CD_ERROR, CD_ERROR_INVALID_ARGUMENTS,
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
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
		g_set_error_literal (error, CD_ERROR, CD_ERROR_INVALID_ARGUMENTS,
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
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
	cd_util_show_device (priv, device);
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
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
	cd_util_show_device (priv, device);
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
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
	cd_util_show_device (priv, device);
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
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
	cd_util_show_profile (priv, profile);
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
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
	cd_util_show_profile (priv, profile);
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
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
	cd_util_show_profile (priv, profile);
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
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
	cd_util_show_profile (priv, profile);
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected device path, profile path "
				     "e.g. '/org/device/foo /org/profile/bar'");
		goto out;
	}

	/* find the device */
	if (g_variant_is_object_path (values[0])) {
		device = cd_device_new_with_object_path (values[0]);
	} else {
		device = cd_client_find_device_sync (priv->client, values[0],
						     NULL, error);
		if (device == NULL) {
			ret = FALSE;
			goto out;
		}
	}
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;

	/* find the profile */
	if (g_variant_is_object_path (values[1])) {
		profile = cd_profile_new_with_object_path (values[1]);
	} else {
		profile = cd_client_find_profile_sync (priv->client, values[1],
						       NULL, error);
		if (profile == NULL) {
			ret = FALSE;
			goto out;
		}
	}
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected device path, profile path "
				     "e.g. '/org/device/foo /org/profile/bar'");
		goto out;
	}

	/* find the device */
	if (g_variant_is_object_path (values[0])) {
		device = cd_device_new_with_object_path (values[0]);
	} else {
		device = cd_client_find_device_sync (priv->client, values[0],
						     NULL, error);
		if (device == NULL) {
			ret = FALSE;
			goto out;
		}
	}
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;

	/* find the profile */
	if (g_variant_is_object_path (values[1])) {
		profile = cd_profile_new_with_object_path (values[1]);
	} else {
		profile = cd_client_find_profile_sync (priv->client, values[1],
						       NULL, error);
		if (profile == NULL) {
			ret = FALSE;
			goto out;
		}
	}
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected device path "
				     "e.g. '/org/devices/foo'");
		goto out;
	}

	/* find the device */
	if (g_variant_is_object_path (values[0])) {
		device = cd_device_new_with_object_path (values[0]);
	} else {
		device = cd_client_find_device_sync (priv->client, values[0],
						     NULL, error);
		if (device == NULL) {
			ret = FALSE;
			goto out;
		}
	}
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected profile path "
				     "e.g. '/org/profiles/bar'");
		goto out;
	}

	/* find the profile */
	if (g_variant_is_object_path (values[0])) {
		profile = cd_profile_new_with_object_path (values[0]);
	} else {
		profile = cd_client_find_profile_sync (priv->client, values[0],
						       NULL, error);
		if (profile == NULL)
			goto out;
	}
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected profile path key value "
				     "e.g. '/org/profile/foo qualifier RGB.Matte.300dpi'");
		goto out;
	}

	/* find the profile */
	if (g_variant_is_object_path (values[0])) {
		profile = cd_profile_new_with_object_path (values[0]);
	} else {
		profile = cd_client_find_profile_sync (priv->client, values[0],
						       NULL, error);
		if (profile == NULL)
			goto out;
	}
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected device path, model "
				     "e.g. '/org/devices/bar \"Stylus 800\"'");
		goto out;
	}

	/* find the device */
	if (g_variant_is_object_path (values[0])) {
		device = cd_device_new_with_object_path (values[0]);
	} else {
		device = cd_client_find_device_sync (priv->client, values[0],
						     NULL, error);
		if (device == NULL) {
			ret = FALSE;
			goto out;
		}
	}
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected device path, True|False");
		goto out;
	}

	/* find the device */
	if (g_variant_is_object_path (values[0])) {
		device = cd_device_new_with_object_path (values[0]);
	} else {
		device = cd_client_find_device_sync (priv->client, values[0],
						     NULL, error);
		if (device == NULL) {
			ret = FALSE;
			goto out;
		}
	}
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected device path "
				     "e.g. '/org/devices/bar'");
		goto out;
	}

	/* find the device */
	if (g_variant_is_object_path (values[0])) {
		device = cd_device_new_with_object_path (values[0]);
	} else {
		device = cd_client_find_device_sync (priv->client, values[0],
						     NULL, error);
		if (device == NULL) {
			ret = FALSE;
			goto out;
		}
	}
	ret = cd_device_connect_sync (device, NULL, error);
	if (!ret)
		goto out;
	profile = cd_device_get_default_profile (device);
	if (profile == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_ERROR, CD_ERROR_INVALID_ARGUMENTS,
			     "There is no assigned profile for %s",
			     values[0]);
		goto out;
	}
	ret = cd_profile_connect_sync (profile, NULL, error);
	if (!ret)
		goto out;
	cd_util_show_profile (priv, profile);
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected device path, vendor "
				     "e.g. '/org/devices/bar Epson'");
		goto out;
	}

	/* find the device */
	if (g_variant_is_object_path (values[0])) {
		device = cd_device_new_with_object_path (values[0]);
	} else {
		device = cd_client_find_device_sync (priv->client, values[0],
						     NULL, error);
		if (device == NULL) {
			ret = FALSE;
			goto out;
		}
	}
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected device path, serial "
				     "e.g. '/org/devices/bar 00001234'");
		goto out;
	}

	/* find the device */
	if (g_variant_is_object_path (values[0])) {
		device = cd_device_new_with_object_path (values[0]);
	} else {
		device = cd_client_find_device_sync (priv->client, values[0],
						     NULL, error);
		if (device == NULL) {
			ret = FALSE;
			goto out;
		}
	}
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected device path, kind "
				     "e.g. '/org/devices/bar printer'");
		goto out;
	}

	/* find the device */
	if (g_variant_is_object_path (values[0])) {
		device = cd_device_new_with_object_path (values[0]);
	} else {
		device = cd_client_find_device_sync (priv->client, values[0],
						     NULL, error);
		if (device == NULL) {
			ret = FALSE;
			goto out;
		}
	}
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected device path timeout (use 0 for 'never') "
				     "e.g. '/org/devices/epson-800' 60");
		goto out;
	}

	/* check timeout is valid */
	timeout = atoi (values[1]);
	if (timeout < 0) {
		ret = FALSE;
		g_set_error (error,
			     CD_ERROR, CD_ERROR_INVALID_ARGUMENTS,
			     "Not a valid timeout: %s",
			     values[1]);
		goto out;
	}

	/* find the device */
	if (g_variant_is_object_path (values[0])) {
		device = cd_device_new_with_object_path (values[0]);
	} else {
		device = cd_client_find_device_sync (priv->client, values[0],
						     NULL, error);
		if (device == NULL) {
			ret = FALSE;
			goto out;
		}
	}
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
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected device path, qualifier "
				     "e.g. '/org/devices/bar *.*.300dpi'");
		goto out;
	}

	/* find the device */
	if (g_variant_is_object_path (values[0])) {
		device = cd_device_new_with_object_path (values[0]);
	} else {
		device = cd_client_find_device_sync (priv->client, values[0],
						     NULL, error);
		if (device == NULL) {
			ret = FALSE;
			goto out;
		}
	}
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
	cd_util_show_profile (priv, profile);
out:
	if (device != NULL)
		g_object_unref (device);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * cd_util_import_profile:
 **/
static gboolean
cd_util_import_profile (CdUtilPrivate *priv,
			gchar **values,
			GError **error)
{
	CdProfile *profile = NULL;
	gboolean ret = TRUE;
	GFile *file = NULL;

	if (g_strv_length (values) < 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_ERROR,
				     CD_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected: file");
		goto out;
	}

	/* import the profile */
	file = g_file_new_for_path (values[0]);
	profile = cd_client_import_profile_sync (priv->client,
						 file,
						 NULL,
						 error);
	if (profile == NULL) {
		ret = FALSE;
		goto out;
	}
	ret = cd_profile_connect_sync (profile, NULL, error);
	if (!ret)
		goto out;
	cd_util_show_profile (priv, profile);
out:
	if (file != NULL)
		g_object_unref (file);
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
	gboolean value_only = FALSE;
	gboolean verbose = FALSE;
	gboolean version = FALSE;
	gchar *cmd_descriptions = NULL;
	gchar *filter = NULL;
	GError *error = NULL;
	guint retval = 1;
	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: command line option */
			_("Show extra debugging information"), NULL },
		{ "version", '\0', 0, G_OPTION_ARG_NONE, &version,
			/* TRANSLATORS: command line option */
			_("Show client and daemon versions"), NULL },
		{ "value-only", '\0', 0, G_OPTION_ARG_NONE, &value_only,
			/* TRANSLATORS: command line option */
			_("Show the value without any header"), NULL },
		{ "filter", '\0', 0, G_OPTION_ARG_STRING, &filter,
			/* TRANSLATORS: command line option */
			_("Filter object properties when displaying"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* create helper object */
	priv = g_new0 (CdUtilPrivate, 1);

	/* add commands */
	priv->cmd_array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_util_item_free);
	cd_util_add (priv->cmd_array,
		     "dump",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Dump all debug data to a file"),
		     cd_util_dump);
	cd_util_add (priv->cmd_array,
		     "get-devices",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Gets all the color managed devices"),
		     cd_util_get_devices);
	cd_util_add (priv->cmd_array,
		     "get-devices-by-kind",
		     "[KIND]",
		     /* TRANSLATORS: command description */
		     _("Gets all the color managed devices of a specific kind"),
		     cd_util_get_devices_by_kind);
	cd_util_add (priv->cmd_array,
		     "get-profiles",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Gets all the available color profiles"),
		     cd_util_get_profiles);
	cd_util_add (priv->cmd_array,
		     "get-sensors",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Gets all the available color sensors"),
		     cd_util_get_sensors);
	cd_util_add (priv->cmd_array,
		     "get-sensor-reading",
		     "[KIND]",
		     /* TRANSLATORS: command description */
		     _("Gets a reading from a sensor"),
		     cd_util_get_sensor_reading);
	cd_util_add (priv->cmd_array,
		     "sensor-lock",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Locks the color sensor"),
		     cd_util_sensor_lock);
	cd_util_add (priv->cmd_array,
		     "sensor-set-options",
		     "[KEY] [VALUE]",
		     /* TRANSLATORS: command description */
		     _("Sets one or more sensor options"),
		     cd_util_sensor_set_options);
	cd_util_add (priv->cmd_array,
		     "create-device",
		     "[ID] [SCOPE] [KIND]",
		     /* TRANSLATORS: command description */
		     _("Create a device"),
		     cd_util_create_device);
	cd_util_add (priv->cmd_array,
		     "find-device",
		     "[ID]",
		     /* TRANSLATORS: command description */
		     _("Find a device from the device ID"),
		     cd_util_find_device);
	cd_util_add (priv->cmd_array,
		     "find-device-by-property",
		     "[KEY] [VALUE]",
		     /* TRANSLATORS: command description */
		     _("Find a device with a given property value"),
		     cd_util_find_device_by_property);
	cd_util_add (priv->cmd_array,
		     "find-profile",
		     "[ID]",
		     /* TRANSLATORS: command description */
		     _("Find a profile from the profile ID"),
		     cd_util_find_profile);
	cd_util_add (priv->cmd_array,
		     "find-profile-by-filename",
		     "[FILENAME]",
		     /* TRANSLATORS: command description */
		     _("Find a profile by filename"),
		     cd_util_find_profile_by_filename);
	cd_util_add (priv->cmd_array,
		     "get-standard-space",
		     "[TYPE]",
		     /* TRANSLATORS: command description */
		     _("Get a standard colorspace"),
		     cd_util_get_standard_space);
	cd_util_add (priv->cmd_array,
		     "create-profile",
		     "[ID] [SCOPE]",
		     /* TRANSLATORS: command description */
		     _("Create a profile"),
		     cd_util_create_profile);
	cd_util_add (priv->cmd_array,
		     "device-add-profile",
		     "[ID|PATH] [ID|PATH]",
		     /* TRANSLATORS: command description */
		     _("Add a profile to a device that already exists"),
		     cd_util_device_add_profile);
	cd_util_add (priv->cmd_array,
		     "device-make-profile-default",
		     "[ID|PATH] [ID|PATH]",
		     /* TRANSLATORS: command description */
		     _("Makes a profile default for a device"),
		     cd_util_device_make_profile_default);
	cd_util_add (priv->cmd_array,
		     "delete-device",
		     "[ID|PATH]",
		     /* TRANSLATORS: command description */
		     _("Deletes a device"),
		     cd_util_delete_device);
	cd_util_add (priv->cmd_array,
		     "delete-profile",
		     "[ID|PATH]",
		     /* TRANSLATORS: command description */
		     _("Deletes a profile"),
		     cd_util_delete_profile);
	cd_util_add (priv->cmd_array,
		     "profile-set-property",
		     "[ID|PATH] [KEY] [VALUE]",
		     /* TRANSLATORS: command description */
		     _("Sets extra properties on the profile"),
		     cd_util_profile_set_property);
	cd_util_add (priv->cmd_array,
		     "device-set-model",
		     "[ID|PATH] [MODEL]",
		     /* TRANSLATORS: command description */
		     _("Sets the device model"),
		     cd_util_device_set_model);
	cd_util_add (priv->cmd_array,
		     "device-set-enabled",
		     "[ID|PATH] [TRUE|FALSE]",
		     /* TRANSLATORS: command description */
		     _("Enables or disables the device"),
		     cd_util_device_set_enabled);
	cd_util_add (priv->cmd_array,
		     "device-get-default-profile",
		     "[ID|PATH]",
		     /* TRANSLATORS: command description */
		     _("Gets the default profile for a device"),
		     cd_util_device_get_default_profile);
	cd_util_add (priv->cmd_array,
		     "device-set-vendor",
		     "[ID|PATH] [VENDOR]",
		     /* TRANSLATORS: command description */
		     _("Sets the device vendor"),
		     cd_util_device_set_vendor);
	cd_util_add (priv->cmd_array,
		     "device-set-serial",
		     "[ID|PATH] [SERIAL]",
		     /* TRANSLATORS: command description */
		     _("Sets the device serial"),
		     cd_util_device_set_serial);
	cd_util_add (priv->cmd_array,
		     "device-set-kind",
		     "[ID|PATH] [KIND]",
		     /* TRANSLATORS: command description */
		     _("Sets the device kind"),
		     cd_util_device_set_kind);
	cd_util_add (priv->cmd_array,
		     "device-inhibit",
		     "[ID|PATH] [TIMEOUT|0]",
		     /* TRANSLATORS: command description */
		     _("Inhibits color profiles for this device"),
		     cd_util_device_inhibit);
	cd_util_add (priv->cmd_array,
		     "device-get-profile-for-qualifier",
		     "[ID|PATH] [QUALIFIER]",
		     /* TRANSLATORS: command description */
		     _("Returns all the profiles that match a qualifier"),
		     cd_util_device_get_profile_for_qualifiers);
	cd_util_add (priv->cmd_array,
		     "import-profile",
		     "[FILENAME]",
		     /* TRANSLATORS: command description */
		     _("Import a profile and install it for the user"),
		     cd_util_import_profile);

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

	/* add filter if specified */
	priv->value_only = value_only;
	if (filter != NULL)
		priv->filters = g_strsplit (filter, ",", -1);

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
		g_print ("%s\t%s\n", _("System vendor:"),
			 cd_client_get_system_vendor (priv->client));
		g_print ("%s\t%s\n", _("System model:"),
			 cd_client_get_system_model (priv->client));
		goto out;
	}

	/* run the specified command */
	ret = cd_util_run (priv, argv[1], (gchar**) &argv[2], &error);
	if (!ret) {
		if (g_error_matches (error, CD_ERROR, CD_ERROR_NO_SUCH_CMD)) {
			gchar *tmp;
			tmp = g_option_context_get_help (priv->context, TRUE, NULL);
			g_print ("%s", tmp);
			g_free (tmp);
		} else {
			g_print ("%s\n", error->message);
		}
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
		g_strfreev (priv->filters);
		g_option_context_free (priv->context);
		g_free (priv);
	}
	g_free (filter);
	g_free (cmd_descriptions);
	return retval;
}

