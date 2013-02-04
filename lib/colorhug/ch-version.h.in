/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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

#if !defined (__COLORHUG_H_INSIDE__) && !defined (CH_COMPILATION)
#error "Only <colorhug.h> can be included directly."
#endif

#ifndef __CH_VERSION_H
#define __CH_VERSION_H

/* compile time version
 */
#define CH_MAJOR_VERSION				(@CH_MAJOR_VERSION@)
#define CH_MINOR_VERSION				(@CH_MINOR_VERSION@)
#define CH_MICRO_VERSION				(@CH_MICRO_VERSION@)

/* check whether a Ch version equal to or greater than
 * major.minor.micro.
 */
#define CH_CHECK_VERSION(major,minor,micro)    \
    (CH_MAJOR_VERSION > (major) || \
     (CH_MAJOR_VERSION == (major) && CH_MINOR_VERSION > (minor)) || \
     (CH_MAJOR_VERSION == (major) && CH_MINOR_VERSION == (minor) && \
      CH_MICRO_VERSION >= (micro)))

#endif /* __CH_VERSION_H */
