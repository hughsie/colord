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

#include "cd-enum.h"
#include "cd-common.h"
#include "cd-lcms-helpers.h"

#ifdef HAVE_NEW_LCMS
/**
 * cd_profile_copy_metadata_dict:
 */
static void
cd_profile_copy_metadata_dict (cmsHANDLE dict,
			       cmsHANDLE dict_tmp)
{
	const cmsDICTentry* entry;
	gchar name[1024];

	/* copy, but ignore some metadata we're about to write */
	for (entry = cmsDictGetEntryList (dict_tmp);
	     entry != NULL;
	     entry = cmsDictNextEntry (entry)) {
		wcstombs (name, entry->Name, sizeof (name));
		if (g_strcmp0 (name, CD_PROFILE_METADATA_CMF_PRODUCT) == 0)
			continue;
		if (g_strcmp0 (name, CD_PROFILE_METADATA_CMF_BINARY) == 0)
			continue;
		if (g_strcmp0 (name, CD_PROFILE_METADATA_CMF_VERSION) == 0)
			continue;
		cmsDictAddEntry (dict,
				 entry->Name,
				 entry->Value,
				 NULL,
				 NULL);
	}
}
#endif

/**
 * cd_profile_write_metadata_string:
 */
gboolean
cd_profile_write_metadata_string (cmsHPROFILE lcms_profile,
				  const gchar *metadata,
				  gboolean clear_existing,
				  const gchar *binary_name,
				  GError **error)
{
	gboolean ret = FALSE;
#ifdef HAVE_NEW_LCMS
	cmsHANDLE dict = NULL;
	cmsHANDLE dict_tmp = NULL;
	gchar **metadata_split = NULL;
	gchar *tmp;
	guint i;

	/* always write metadata */
	dict = cmsDictAlloc (NULL);

	/* read existing metadata */
	if (!clear_existing) {
		dict_tmp = cmsReadTag (lcms_profile, cmsSigMetaTag);
		if (dict_tmp != NULL)
			cd_profile_copy_metadata_dict (dict, dict_tmp);
	}

	/* parse string */
	if (metadata != NULL) {
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
	}

	/* add CMS defines */
	_cmsDictAddEntryAscii (dict,
			       CD_PROFILE_METADATA_CMF_PRODUCT,
			       PACKAGE_NAME);
	if (binary_name != NULL) {
		_cmsDictAddEntryAscii (dict,
				       CD_PROFILE_METADATA_CMF_BINARY,
				       binary_name);
	}
	_cmsDictAddEntryAscii (dict,
			       CD_PROFILE_METADATA_CMF_VERSION,
			       PACKAGE_VERSION);

	/* just write dict */
	ret = cmsWriteTag (lcms_profile, cmsSigMetaTag, dict);
	if (!ret) {
		g_set_error (error, 1, 0,
			     "cannot write %s", metadata);
	}
out:
	cmsDictFree (dict);
	g_strfreev (metadata_split);
#else
	if (metadata != NULL) {
		g_set_error (error, 1, 0,
			     "no LCMS2 DICT support, so cannot write %s",
			     metadata);
		goto out;
	}

	/* no metadata, so no problem */
	ret = TRUE;
out:
#endif
	return ret;
}
