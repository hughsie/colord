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

#ifndef __CD_COMPAT_EDID_H
#define __CD_COMPAT_EDID_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * CdEdidError:
 *
 * The error code.
 * NOTE: this enum has to be kept in sync with ucmm_error which is found
 * in ucmm/ucmm.h in the ArgyllCMS project.
 *
 * Since: 0.1.34
 **/
typedef enum {
	CD_EDID_ERROR_OK = 0,
	CD_EDID_ERROR_RESOURCE,
	CD_EDID_ERROR_INVALID_PROFILE,
	CD_EDID_ERROR_NO_PROFILE,
	CD_EDID_ERROR_UNUSED1,
	CD_EDID_ERROR_NO_DATA,
	CD_EDID_ERROR_PROFILE_COPY,
	CD_EDID_ERROR_UNUSED2,
	CD_EDID_ERROR_ACCESS_CONFIG,
	CD_EDID_ERROR_SET_CONFIG,
	CD_EDID_ERROR_UNUSED3,
	CD_EDID_ERROR_MONITOR_NOT_FOUND,
	CD_EDID_ERROR_UNUSED4,
	CD_EDID_ERROR_UNUSED5,
} CdEdidError;

/**
 * CdEdidScope:
 *
 * The scope of the profile.
 * NOTE: this enum has to be kept in sync with ucmm_scope which is found
 * in ucmm/ucmm.h in the ArgyllCMS project.
 *
 * Since: 0.1.34
 **/
typedef enum {
	CD_EDID_SCOPE_USER,
	CD_EDID_SCOPE_SYSTEM,
	CD_EDID_SCOPE_LAST
} CdEdidScope;

CdEdidError	 cd_edid_install_profile	(unsigned char	*edid,
						 int		 edid_len,
						 CdEdidScope	 scope,
						 char		*profile_fn);
CdEdidError	 cd_edid_remove_profile		(unsigned char	*edid,
						 int		 edid_len,
						 char		*profile_fn);
CdEdidError	 cd_edid_get_profile		(unsigned char	*edid,
						 int		 edid_len,
						 char		**profile_fn);

G_END_DECLS

#endif /* __CD_COMPAT_EDID_H */
