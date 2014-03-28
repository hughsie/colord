/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Soren Sandmann <sandmann@redhat.com>
 * Copyright (C) 2009-2013 Richard Hughes <richard@hughsie.com>
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

#include <string.h>
#include <glib-object.h>
#include <math.h>
#ifdef HAVE_UDEV
  #include <libudev.h>
#endif

#include "cd-edid.h"
#include "cd-quirk.h"

static void	cd_edid_finalize	(GObject	*object);

#define CD_EDID_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_EDID, CdEdidPrivate))

struct _CdEdidPrivate
{
	CdColorYxy		*red;
	CdColorYxy		*green;
	CdColorYxy		*blue;
	CdColorYxy		*white;
	gchar			*checksum;
	gchar			*eisa_id;
	gchar			*monitor_name;
	gchar			*pnp_id;
	gchar			*serial_number;
	gchar			*vendor_name;
	gdouble			 gamma;
	guint			 height;
	guint			 width;
};

G_DEFINE_TYPE (CdEdid, cd_edid, G_TYPE_OBJECT)

#define CD_EDID_OFFSET_PNPID				0x08
#define CD_EDID_OFFSET_SERIAL				0x0c
#define CD_EDID_OFFSET_SIZE				0x15
#define CD_EDID_OFFSET_GAMMA				0x17
#define CD_EDID_OFFSET_DATA_BLOCKS			0x36
#define CD_EDID_OFFSET_LAST_BLOCK			0x6c
#define CD_EDID_OFFSET_EXTENSION_BLOCK_COUNT		0x7e

#define CD_DESCRIPTOR_DISPLAY_PRODUCT_NAME		0xfc
#define CD_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER	0xff
#define CD_DESCRIPTOR_COLOR_MANAGEMENT_DATA		0xf9
#define CD_DESCRIPTOR_ALPHANUMERIC_DATA_STRING		0xfe
#define CD_DESCRIPTOR_COLOR_POINT			0xfb

/**
 * cd_edid_error_quark:
 *
 * Gets the #CdEdid error quark.
 *
 * Return value: a #GQuark
 *
 * Since: 1.1.2
 **/
GQuark
cd_edid_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("cd_edid_error");
	return quark;
}

/**
 * cd_edid_get_monitor_name:
 * @edid: A valid #CdEdid
 *
 * Gets the EDID monitor name.
 *
 * Return value: string value
 *
 * Since: 1.1.2
 **/
const gchar *
cd_edid_get_monitor_name (CdEdid *edid)
{
	g_return_val_if_fail (CD_IS_EDID (edid), NULL);
	return edid->priv->monitor_name;
}

/**
 * cd_edid_convert_pnp_id_to_string:
 **/
static gchar *
cd_edid_convert_pnp_id_to_string (const gchar *pnp_id)
{
#ifdef HAVE_UDEV
	gchar *modalias = NULL;
	gchar *vendor = NULL;
	struct udev_hwdb *hwdb = NULL;
	struct udev_list_entry *e;
	struct udev_list_entry *v;
	struct udev *udev;

	/* connect to the hwdb */
	udev = udev_new ();
	if (udev == NULL)
		goto out;
	hwdb = udev_hwdb_new (udev);
	if (hwdb == NULL)
		goto out;

	/* search the hash */
	modalias = g_strdup_printf ("acpi:%s:", pnp_id);
	e = udev_hwdb_get_properties_list_entry (hwdb, modalias, 0);
	if (e == NULL)
		goto out;

	/* the hwdb only contains this key at the moment, but future proof */
	v = udev_list_entry_get_by_name (e, "ID_VENDOR_FROM_DATABASE");
	if (v == NULL)
		goto out;

	/* quirk the name */
	vendor = cd_quirk_vendor_name (udev_list_entry_get_value (v));
out:
	g_free (modalias);
	if (hwdb != NULL)
		udev_hwdb_unref (hwdb);
	if (udev != NULL)
		udev_unref (udev);
#else
	gboolean ret;
	gchar *data = NULL;
	gchar *idx2;
	gchar *idx;
	gchar *vendor = NULL;
	guint i;
	const gchar *pnp_ids[] = { "/usr/share/hwdata/pnp.ids",
				   "/usr/share/misc/pnp.ids",
				   "/usr/share/libgnome-desktop/pnp.ids",
				   NULL };

	for (i = 0; pnp_ids[i] != NULL; i++) {
		ret = g_file_get_contents (pnp_ids[i], &data, NULL, NULL);
		if (ret)
			break;
	}
	if (data == NULL)
		goto out;

	/* get the vendor name from the tab delimited data */
	for (idx = data; idx != NULL; ) {
		if (strncmp (idx, pnp_id, 3) == 0) {
			idx2 = g_strstr_len (idx, -1, "\n");
			if (idx2 != NULL)
				*idx2 = '\0';
			vendor = g_strdup (idx + 4);
			break;
		}
		idx = g_strstr_len (idx, -1, "\n");
		if (idx != NULL)
			idx++;
	}
out:
	g_free (data);
#endif
	return vendor;
}

/**
 * cd_edid_get_vendor_name:
 * @edid: A valid #CdEdid
 *
 * Gets the EDID vendor name.
 *
 * Return value: string value
 *
 * Since: 1.1.2
 **/
const gchar *
cd_edid_get_vendor_name (CdEdid *edid)
{
	CdEdidPrivate *priv = edid->priv;
	g_return_val_if_fail (CD_IS_EDID (edid), NULL);
	if (priv->vendor_name == NULL)
		priv->vendor_name = cd_edid_convert_pnp_id_to_string (priv->pnp_id);
	return priv->vendor_name;
}

/**
 * cd_edid_get_serial_number:
 * @edid: A valid #CdEdid
 *
 * Gets the EDID serial number.
 *
 * Return value: string value
 *
 * Since: 1.1.2
 **/
const gchar *
cd_edid_get_serial_number (CdEdid *edid)
{
	g_return_val_if_fail (CD_IS_EDID (edid), NULL);
	return edid->priv->serial_number;
}

/**
 * cd_edid_get_eisa_id:
 * @edid: A valid #CdEdid
 *
 * Gets the EDID EISA ID.
 *
 * Return value: string value
 *
 * Since: 1.1.2
 **/
const gchar *
cd_edid_get_eisa_id (CdEdid *edid)
{
	g_return_val_if_fail (CD_IS_EDID (edid), NULL);
	return edid->priv->eisa_id;
}

/**
 * cd_edid_get_checksum:
 * @edid: A valid #CdEdid
 *
 * Gets the EDID MD5 checksum.
 *
 * Return value: string value
 *
 * Since: 1.1.2
 **/
const gchar *
cd_edid_get_checksum (CdEdid *edid)
{
	g_return_val_if_fail (CD_IS_EDID (edid), NULL);
	return edid->priv->checksum;
}

/**
 * cd_edid_get_pnp_id:
 * @edid: A valid #CdEdid
 *
 * Gets the EDID PNP ID.
 *
 * Return value: string value
 *
 * Since: 1.1.2
 **/
const gchar *
cd_edid_get_pnp_id (CdEdid *edid)
{
	g_return_val_if_fail (CD_IS_EDID (edid), NULL);
	return edid->priv->pnp_id;
}

/**
 * cd_edid_get_width:
 * @edid: A valid #CdEdid
 *
 * Gets the panel width in inches.
 *
 * Return value: integer value
 *
 * Since: 1.1.2
 **/
guint
cd_edid_get_width (CdEdid *edid)
{
	g_return_val_if_fail (CD_IS_EDID (edid), 0);
	return edid->priv->width;
}

/**
 * cd_edid_get_height:
 * @edid: A valid #CdEdid
 *
 * Gets the panel height in inches.
 *
 * Return value: integer value
 *
 * Since: 1.1.2
 **/
guint
cd_edid_get_height (CdEdid *edid)
{
	g_return_val_if_fail (CD_IS_EDID (edid), 0);
	return edid->priv->height;
}

/**
 * cd_edid_get_gamma:
 * @edid: A valid #CdEdid
 *
 * Gets the native panel gamma.
 *
 * Return value: floating point value
 *
 * Since: 1.1.2
 **/
gdouble
cd_edid_get_gamma (CdEdid *edid)
{
	g_return_val_if_fail (CD_IS_EDID (edid), 0.0f);
	return edid->priv->gamma;
}

/**
 * cd_edid_get_red:
 * @edid: A valid #CdEdid
 *
 * Gets the red primary.
 *
 * Return value: %TRUE for success
 *
 * Since: 1.1.2
 **/
const CdColorYxy *
cd_edid_get_red (CdEdid *edid)
{
	g_return_val_if_fail (CD_IS_EDID (edid), NULL);
	return edid->priv->red;
}

/**
 * cd_edid_get_green:
 * @edid: A valid #CdEdid
 *
 * Gets the green primary.
 *
 * Return value: #CdColorYxy chromaticity
 *
 * Since: 1.1.2
 **/
const CdColorYxy *
cd_edid_get_green (CdEdid *edid)
{
	g_return_val_if_fail (CD_IS_EDID (edid), NULL);
	return edid->priv->green;
}

/**
 * cd_edid_get_blue:
 * @edid: A valid #CdEdid
 *
 * Gets the blue primary.
 *
 * Return value: #CdColorYxy chromaticity
 *
 * Since: 1.1.2
 **/
const CdColorYxy *
cd_edid_get_blue (CdEdid *edid)
{
	g_return_val_if_fail (CD_IS_EDID (edid), NULL);
	return edid->priv->blue;
}

/**
 * cd_edid_get_white:
 * @edid: A valid #CdEdid
 *
 * Gets the whitepoint.
 *
 * Return value: #CdColorYxy chromaticity
 *
 * Since: 1.1.2
 **/
const CdColorYxy *
cd_edid_get_white (CdEdid *edid)
{
	g_return_val_if_fail (CD_IS_EDID (edid), NULL);
	return edid->priv->white;
}

/**
 * cd_edid_reset:
 * @edid: A valid #CdEdid
 *
 * Resets all cached data.
 *
 * Since: 1.1.2
 **/
void
cd_edid_reset (CdEdid *edid)
{
	CdEdidPrivate *priv = edid->priv;

	g_return_if_fail (CD_IS_EDID (edid));

	/* free old data */
	g_free (priv->monitor_name);
	g_free (priv->vendor_name);
	g_free (priv->serial_number);
	g_free (priv->eisa_id);
	g_free (priv->checksum);

	/* do not deallocate, just blank */
	priv->pnp_id[0] = '\0';

	/* set to default values */
	priv->monitor_name = NULL;
	priv->vendor_name = NULL;
	priv->serial_number = NULL;
	priv->eisa_id = NULL;
	priv->checksum = NULL;
	priv->width = 0;
	priv->height = 0;
	priv->gamma = 0.0f;
}

/**
 * cd_edid_get_bit:
 **/
static gint
cd_edid_get_bit (gint in, gint bit)
{
	return (in & (1 << bit)) >> bit;
}

/**
 * cd_edid_get_bits:
 **/
static gint
cd_edid_get_bits (gint in, gint begin, gint end)
{
	gint mask = (1 << (end - begin + 1)) - 1;

	return (in >> begin) & mask;
}

/**
 * cd_edid_decode_fraction:
 **/
static gdouble
cd_edid_decode_fraction (gint high, gint low)
{
	gdouble result = 0.0;
	gint i;

	high = (high << 2) | low;
	for (i = 0; i < 10; ++i)
		result += cd_edid_get_bit (high, i) * pow (2, i - 10);
	return result;
}

/**
 * cd_edid_parse_string:
 **/
static gchar *
cd_edid_parse_string (const guint8 *data)
{
	gchar *text;
	guint i;
	guint replaced = 0;

	/* this is always 13 bytes, but we can't guarantee it's null
	 * terminated or not junk. */
	text = g_strndup ((const gchar *) data, 13);

	/* remove insane newline chars */
	g_strdelimit (text, "\n\r", '\0');

	/* remove spaces */
	g_strchomp (text);

	/* nothing left? */
	if (text[0] == '\0') {
		g_free (text);
		text = NULL;
		goto out;
	}

	/* ensure string is printable */
	for (i = 0; text[i] != '\0'; i++) {
		if (!g_ascii_isprint (text[i])) {
			text[i] = '-';
			replaced++;
		}
	}

	/* not valid UTF-8 */
	if (!g_utf8_validate (text, -1, NULL)) {
		g_free (text);
		text = NULL;
		goto out;
	}

	/* if the string is junk, ignore the string */
	if (replaced > 4) {
		g_free (text);
		text = NULL;
		goto out;
	}
out:
	return text;
}

/**
 * cd_edid_parse:
 * @edid: A valid #CdEdid
 * @edid_data: data to parse
 * @error: A #GError, or %NULL
 *
 * Parses the EDID.
 *
 * Return value: %TRUE for success
 *
 * Since: 1.1.2
 **/
gboolean
cd_edid_parse (CdEdid *edid, GBytes *edid_data, GError **error)
{
	CdEdidPrivate *priv = edid->priv;
	const guint8 *data;
	gboolean ret = TRUE;
	gchar *tmp;
	gsize length;
	guint32 serial;
	guint i;

	/* check header */
	data = g_bytes_get_data (edid_data, &length);
	if (length < 128) {
		g_set_error_literal (error,
				     CD_EDID_ERROR,
				     CD_EDID_ERROR_FAILED_TO_PARSE,
				     "EDID length is too small");
		ret = FALSE;
		goto out;
	}
	if (data[0] != 0x00 || data[1] != 0xff) {
		g_set_error_literal (error,
				     CD_EDID_ERROR,
				     CD_EDID_ERROR_FAILED_TO_PARSE,
				     "Failed to parse EDID header");
		ret = FALSE;
		goto out;
	}

	/* free old data */
	cd_edid_reset (edid);

	/* decode the PNP ID from three 5 bit words packed into 2 bytes
	 * /--08--\/--09--\
	 * 7654321076543210
	 * |\---/\---/\---/
	 * R  C1   C2   C3 */
	priv->pnp_id[0] = 'A' + ((data[CD_EDID_OFFSET_PNPID+0] & 0x7c) / 4) - 1;
	priv->pnp_id[1] = 'A' + ((data[CD_EDID_OFFSET_PNPID+0] & 0x3) * 8) + ((data[CD_EDID_OFFSET_PNPID+1] & 0xe0) / 32) - 1;
	priv->pnp_id[2] = 'A' + (data[CD_EDID_OFFSET_PNPID+1] & 0x1f) - 1;

	/* maybe there isn't a ASCII serial number descriptor, so use this instead */
	serial = (guint32) data[CD_EDID_OFFSET_SERIAL+0];
	serial += (guint32) data[CD_EDID_OFFSET_SERIAL+1] * 0x100;
	serial += (guint32) data[CD_EDID_OFFSET_SERIAL+2] * 0x10000;
	serial += (guint32) data[CD_EDID_OFFSET_SERIAL+3] * 0x1000000;
	if (serial > 0)
		priv->serial_number = g_strdup_printf ("%" G_GUINT32_FORMAT, serial);

	/* get the size */
	priv->width = data[CD_EDID_OFFSET_SIZE+0];
	priv->height = data[CD_EDID_OFFSET_SIZE+1];

	/* we don't care about aspect */
	if (priv->width == 0 || priv->height == 0) {
		priv->width = 0;
		priv->height = 0;
	}

	/* get gamma */
	if (data[CD_EDID_OFFSET_GAMMA] == 0xff) {
		priv->gamma = 1.0f;
	} else {
		priv->gamma = ((gfloat) data[CD_EDID_OFFSET_GAMMA] / 100) + 1;
	}

	/* get color red */
	priv->red->x = cd_edid_decode_fraction (data[0x1b], cd_edid_get_bits (data[0x19], 6, 7));
	priv->red->y = cd_edid_decode_fraction (data[0x1c], cd_edid_get_bits (data[0x19], 4, 5));

	/* get color green */
	priv->green->x = cd_edid_decode_fraction (data[0x1d], cd_edid_get_bits (data[0x19], 2, 3));
	priv->green->y = cd_edid_decode_fraction (data[0x1e], cd_edid_get_bits (data[0x19], 0, 1));

	/* get color blue */
	priv->blue->x = cd_edid_decode_fraction (data[0x1f], cd_edid_get_bits (data[0x1a], 6, 7));
	priv->blue->y = cd_edid_decode_fraction (data[0x20], cd_edid_get_bits (data[0x1a], 4, 5));

	/* get color white */
	priv->white->x = cd_edid_decode_fraction (data[0x21], cd_edid_get_bits (data[0x1a], 2, 3));
	priv->white->y = cd_edid_decode_fraction (data[0x22], cd_edid_get_bits (data[0x1a], 0, 1));

	/* parse EDID data */
	for (i = CD_EDID_OFFSET_DATA_BLOCKS;
	     i <= CD_EDID_OFFSET_LAST_BLOCK;
	     i += 18) {
		/* ignore pixel clock data */
		if (data[i] != 0)
			continue;
		if (data[i+2] != 0)
			continue;

		/* any useful blocks? */
		if (data[i+3] == CD_DESCRIPTOR_DISPLAY_PRODUCT_NAME) {
			tmp = cd_edid_parse_string (&data[i+5]);
			if (tmp != NULL) {
				g_free (priv->monitor_name);
				priv->monitor_name = tmp;
			}
		} else if (data[i+3] == CD_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER) {
			tmp = cd_edid_parse_string (&data[i+5]);
			if (tmp != NULL) {
				g_free (priv->serial_number);
				priv->serial_number = tmp;
			}
		} else if (data[i+3] == CD_DESCRIPTOR_COLOR_MANAGEMENT_DATA) {
			g_warning ("failing to parse color management data");
		} else if (data[i+3] == CD_DESCRIPTOR_ALPHANUMERIC_DATA_STRING) {
			tmp = cd_edid_parse_string (&data[i+5]);
			if (tmp != NULL) {
				g_free (priv->eisa_id);
				priv->eisa_id = tmp;
			}
		} else if (data[i+3] == CD_DESCRIPTOR_COLOR_POINT) {
			if (data[i+3+9] != 0xff) {
				/* extended EDID block(1) which contains
				 * a better gamma value */
				priv->gamma = ((gfloat) data[i+3+9] / 100) + 1;
			}
			if (data[i+3+14] != 0xff) {
				/* extended EDID block(2) which contains
				 * a better gamma value */
				priv->gamma = ((gfloat) data[i+3+9] / 100) + 1;
			}
		}
	}

	/* calculate checksum */
	priv->checksum = g_compute_checksum_for_data (G_CHECKSUM_MD5, data, length);
out:
	return ret;
}

/**
 * cd_edid_class_init:
 **/
static void
cd_edid_class_init (CdEdidClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_edid_finalize;
	g_type_class_add_private (klass, sizeof (CdEdidPrivate));
}

/**
 * cd_edid_init:
 **/
static void
cd_edid_init (CdEdid *edid)
{
	edid->priv = CD_EDID_GET_PRIVATE (edid);
	edid->priv->pnp_id = g_new0 (gchar, 4);
	edid->priv->red = cd_color_yxy_new ();
	edid->priv->green = cd_color_yxy_new ();
	edid->priv->blue = cd_color_yxy_new ();
	edid->priv->white = cd_color_yxy_new ();
}

/**
 * cd_edid_finalize:
 **/
static void
cd_edid_finalize (GObject *object)
{
	CdEdid *edid = CD_EDID (object);
	CdEdidPrivate *priv = edid->priv;

	g_free (priv->monitor_name);
	g_free (priv->vendor_name);
	g_free (priv->serial_number);
	g_free (priv->eisa_id);
	g_free (priv->checksum);
	g_free (priv->pnp_id);
	cd_color_yxy_free (priv->white);
	cd_color_yxy_free (priv->red);
	cd_color_yxy_free (priv->green);
	cd_color_yxy_free (priv->blue);

	G_OBJECT_CLASS (cd_edid_parent_class)->finalize (object);
}

/**
 * cd_edid_new:
 *
 * Creates an object suitable for parsing an EDID.
 *
 * Return value: A new #CdEdid
 *
 * Since: 1.1.2
 **/
CdEdid *
cd_edid_new (void)
{
	CdEdid *edid;
	edid = g_object_new (CD_TYPE_EDID, NULL);
	return CD_EDID (edid);
}
