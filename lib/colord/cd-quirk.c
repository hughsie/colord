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
 * SECTION:cd-quirk
 * @short_description: Device and vendor quirks
 */

#include "config.h"

#include <glib.h>
#include <string.h>

#include "cd-quirk.h"

/**
 * cd_quirk_vendor_name:
 * @vendor: The vendor name
 *
 * Correct and quirk vendor names.
 *
 * Return value: the repaired vendor name
 */
gchar *
cd_quirk_vendor_name (const gchar *vendor)
{
	GString *display_name;
	guint i;
	const gchar *suffixes[] =
		{ "Co.", "Co", "Inc.", "Inc", "Ltd.", "Ltd",
		  "Corporation", "Incorporated", "Limited",
		  "GmbH", "corp.",
		  NULL };
	struct {
		const gchar *old;
		const gchar *new;
	} vendor_names[] = {
		{ "Acer, inc.", "Acer" },
		{ "Acer Technologies", "Acer" },
		{ "AOC Intl", "AOC" },
		{ "Apple Computer Inc", "Apple" },
		{ "Arnos Insturments & Computer Systems", "Arnos" },
		{ "ASUSTeK Computer Inc.", "ASUSTeK" },
		{ "ASUSTeK Computer INC", "ASUSTeK" },
		{ "ASUSTeK COMPUTER INC.", "ASUSTeK" },
		{ "BTC Korea Co., Ltd", "BTC" },
		{ "CASIO COMPUTER CO.,LTD", "Casio" },
		{ "CLEVO", "Clevo" },
		{ "Delta Electronics", "Delta" },
		{ "Eizo Nanao Corporation", "Eizo" },
		{ "Envision Peripherals,", "Envision" },
		{ "FUJITSU", "Fujitsu" },
		{ "Fujitsu Siemens Computers GmbH", "Fujitsu Siemens" },
		{ "Funai Electric Co., Ltd.", "Funai" },
		{ "Gigabyte Technology Co., Ltd.", "Gigabyte" },
		{ "Goldstar Company Ltd", "Goldstar" },
		{ "Hewlett-Packard", "Hewlett Packard" },
		{ "Hitachi America Ltd", "Hitachi" },
		{ "HP", "Hewlett Packard" },
		{ "HWP", "Hewlett Packard" },
		{ "IBM France", "IBM" },
		{ "Lenovo Group Limited", "Lenovo" },
		{ "LENOVO", "Lenovo" },
		{ "Iiyama North America", "Iiyama" },
		{ "MARANTZ JAPAN, INC.", "Marantz" },
		{ "Mitsubishi Electric Corporation", "Mitsubishi" },
		{ "Nexgen Mediatech Inc.,", "Nexgen Mediatech" },
		{ "NIKON", "Nikon" },
		{ "Panasonic Industry Company", "Panasonic" },
		{ "Philips Consumer Electronics Company", "Philips" },
		{ "RGB Systems, Inc. dba Extron Electronics", "Extron" },
		{ "SAM", "Samsung" },
		{ "Samsung Electric Company", "Samsung" },
		{ "Samsung Electronics America", "Samsung" },
		{ "samsung", "Samsung" },
		{ "SAMSUNG", "Samsung" },
		{ "Sanyo Electric Co.,Ltd.", "Sanyo" },
		{ "Sonix Technology Co.", "Sonix" },
		{ "System manufacturer", "Unknown" },
		{ "To Be Filled By O.E.M.", "Unknown" },
		{ "Toshiba America Info Systems Inc", "Toshiba" },
		{ "Toshiba Matsushita Display Technology Co.,", "Toshiba" },
		{ "TOSHIBA", "Toshiba" },
		{ "Unknown vendor", "Unknown" },
		{ "Westinghouse Digital Electronics", "Westinghouse Digital" },
		{ "Zalman Tech Co., Ltd.", "Zalman" },
		{ NULL, NULL }
	};

	/* correct some company names */
	for (i = 0; vendor_names[i].old != NULL; i++) {
		if (g_str_has_prefix (vendor, vendor_names[i].old)) {
			display_name = g_string_new (vendor_names[i].new);
			goto out;
		}
	}

	/* get rid of suffixes */
	display_name = g_string_new (vendor);
	for (i = 0; suffixes[i] != NULL; i++) {
		if (g_str_has_suffix (display_name->str, suffixes[i]))
			g_string_truncate (display_name,
					   display_name->len - strlen (suffixes[i]));
	}
	g_strchomp (display_name->str);
out:
	return g_string_free (display_name, FALSE);
}
