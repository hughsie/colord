/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

#include <stdlib.h>

#include "cd-lcms-helpers.h"

/**
 * _cmsWriteTagTextAscii:
 **/
cmsBool
_cmsWriteTagTextAscii (cmsHPROFILE lcms_profile,
		       cmsTagSignature sig,
		       const gchar *text)
{
	cmsBool ret;
	cmsMLU *mlu = cmsMLUalloc (0, 1);
	cmsMLUsetASCII (mlu, "EN", "us", text);
	ret = cmsWriteTag (lcms_profile, sig, mlu);
	cmsMLUfree (mlu);
	return ret;
}

/**
 * utf8_to_wchar_t:
 **/
static wchar_t *
utf8_to_wchar_t (const char *src)
{
	gssize len;
	gssize converted;
	wchar_t *buf = NULL;

	len = mbstowcs (NULL, src, 0);
	if (len < 0) {
		g_warning ("Invalid UTF-8 in string %s", src);
		goto out;
	}
	len += 1;
	buf = g_malloc (sizeof (wchar_t) * len);
	converted = mbstowcs (buf, src, len - 1);
	g_assert (converted != -1);
	buf[converted] = '\0';
out:
	return buf;
}

/**
 * _cmsDictAddEntryAscii:
 **/
cmsBool
_cmsDictAddEntryAscii (cmsHANDLE dict,
		       const gchar *key,
		       const gchar *value)
{
	cmsBool ret = FALSE;
	wchar_t *mb_key = NULL;
	wchar_t *mb_value = NULL;

	mb_key = utf8_to_wchar_t (key);
	if (mb_key == NULL)
		goto out;
	mb_value = utf8_to_wchar_t (value);
	if (mb_value == NULL)
		goto out;
	ret = cmsDictAddEntry (dict, mb_key, mb_value, NULL, NULL);
out:
	g_free (mb_key);
	g_free (mb_value);
	return ret;
}
