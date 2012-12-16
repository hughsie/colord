/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2012 Richard Hughes <richard@hughsie.com>
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

#ifndef __CD_STATE_H
#define __CD_STATE_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define CD_TYPE_STATE		(cd_state_get_type ())
#define CD_STATE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_STATE, CdState))
#define CD_STATE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_STATE, CdStateClass))
#define CD_IS_STATE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_STATE))
#define CD_IS_STATE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_STATE))
#define CD_STATE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_STATE, CdStateClass))
#define CD_STATE_ERROR		(cd_state_error_quark ())

typedef struct _CdState		CdState;
typedef struct _CdStatePrivate	CdStatePrivate;
typedef struct _CdStateClass	CdStateClass;

struct _CdState
{
	GObject			 parent;
	CdStatePrivate		*priv;
};

struct _CdStateClass
{
	GObjectClass	 parent_class;
	void		(* percentage_changed)		(CdState	*state,
							 guint		 value);
	void		(* subpercentage_changed)	(CdState	*state,
							 guint		 value);
};

typedef enum {
	CD_STATE_ERROR_CANCELLED,
	CD_STATE_ERROR_INVALID,
	CD_STATE_ERROR_LAST
} CdStateError;

#define cd_state_done(state, error)			cd_state_done_real(state, error, G_STRLOC)
#define cd_state_finished(state, error)			cd_state_finished_real(state, error, G_STRLOC)
#define cd_state_set_number_steps(state, steps)		cd_state_set_number_steps_real(state, steps, G_STRLOC)
#define cd_state_set_steps(state, error, value, args...) cd_state_set_steps_real(state, error, G_STRLOC, value, ## args)

GType		 cd_state_get_type			(void);
GQuark		 cd_state_error_quark			(void);
CdState		*cd_state_new				(void);
CdState		*cd_state_get_child			(CdState		*state);

/* percentage changed */
gboolean	 cd_state_set_number_steps_real		(CdState		*state,
							 guint			 steps,
							 const gchar		*strloc);
gboolean	 cd_state_set_steps_real		(CdState		*state,
							 GError			**error,
							 const gchar		*strloc,
							 gint			 value, ...);
gboolean	 cd_state_set_percentage		(CdState		*state,
							 guint			 percentage);
guint		 cd_state_get_percentage		(CdState		*state);
gboolean	 cd_state_reset				(CdState		*state);
gboolean	 cd_state_done_real			(CdState		*state,
							 GError			 **error,
							 const gchar		*strloc)
							 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 cd_state_finished_real			(CdState		*state,
							 GError			 **error,
							 const gchar		*strloc)
							 G_GNUC_WARN_UNUSED_RESULT;
void		 cd_state_set_enable_profile		(CdState		*state,
							 gboolean		 enable_profile);

G_END_DECLS

#endif /* __CD_STATE_H */

