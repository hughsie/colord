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

/**
 * SECTION:colord
 * @short_description: Client objects for accessing colord
 *
 * These objects allow client programs to get access to devices that can
 * be color managed and profiles for changing them.
 *
 * See also: #CdClient, #CdDevice
 */

#ifndef __COLORD_H__
#define __COLORD_H__

#define __COLORD_H_INSIDE__

#include <libcolord/cd-client.h>
#include <libcolord/cd-client-sync.h>
#include <libcolord/cd-color.h>
#include <libcolord/cd-device.h>
#include <libcolord/cd-enum.h>
#include <libcolord/cd-profile.h>
#include <libcolord/cd-sensor.h>
#include <libcolord/cd-version.h>

#undef __COLORD_H_INSIDE__

#endif /* __COLORD_H__ */

