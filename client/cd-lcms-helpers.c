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

/*
 * _cmsWriteTagTextAscii:
 */
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

#ifdef HAVE_NEW_LCMS
/*
 * _cmsDictAddEntryAscii:
 */
cmsBool
_cmsDictAddEntryAscii (cmsHANDLE dict,
		       const gchar *key,
		       const gchar *value)
{
	cmsBool ret;
	wchar_t mb_key[1024];
	wchar_t mb_value[1024];
	mbstowcs (mb_key, key, sizeof (mb_key));
	mbstowcs (mb_value, value, sizeof (mb_value));
	ret = cmsDictAddEntry (dict, mb_key, mb_value, NULL, NULL);
	return ret;
}
#endif


/*
 * _cmsProfileWriteMetadataString:
 */
cmsBool
_cmsProfileWriteMetadataString (cmsHPROFILE lcms_profile,
				const gchar *metadata,
				GError **error)
{
	gboolean ret = FALSE;
#ifdef HAVE_NEW_LCMS
	cmsHANDLE dict;
	gchar **metadata_split = NULL;
	gchar *tmp;
	guint i;

	/* just create a new dict */
	dict = cmsDictAlloc (NULL);

	/* parse string */
	metadata_split = g_strsplit (metadata, ",", -1);
	for (i=0; metadata_split[i] != NULL; i++) {
		tmp = g_strstr_len (metadata_split[i], -1, "=");
		if (tmp == NULL) {
			g_set_error (error, 1, 0,
				     "invalid metadata format: '%s' "
				     "expected 'key=value'",
				     metadata_split[i]);
			goto out;
		}
		*tmp = '\0';
		_cmsDictAddEntryAscii (dict, metadata_split[i], tmp+1);
	}

	/* just write dict */
	ret = cmsWriteTag (lcms_profile, cmsSigMetaTag, dict);
out:
	cmsDictFree (dict);
	g_strfreev (metadata_split);
#else
	g_set_error (error, 1, 0,
		     "no LCMS2 DICT support, so cannot write %s",
		     metadata);
#endif
	return ret;
}
