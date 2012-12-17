/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#include <glib-object.h>
#include <gio/gio.h>

#include "cd-inhibit.h"

static void     cd_inhibit_finalize	(GObject     *object);

#define CD_INHIBIT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_INHIBIT, CdInhibitPrivate))

/**
 * CdInhibitPrivate:
 *
 * Private #CdInhibit data
 **/
struct _CdInhibitPrivate
{
	GPtrArray			*array;
};

typedef struct {
	gchar				*sender;
	guint				 watcher_id;
} CdInhibitItem;


enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (CdInhibit, cd_inhibit, G_TYPE_OBJECT)

/**
 * cd_inhibit_valid:
 **/
gboolean
cd_inhibit_valid (CdInhibit *inhibit)
{
	g_return_val_if_fail (CD_IS_INHIBIT (inhibit), FALSE);
	return inhibit->priv->array->len == 0;
}

/**
 * cd_inhibit_get_bus_names:
 **/
gchar **
cd_inhibit_get_bus_names (CdInhibit *inhibit)
{
	CdInhibitItem *item_tmp;
	CdInhibitPrivate *priv = inhibit->priv;
	gchar **bus_names;
	guint i;

	/* just copy senders */
	bus_names = g_new0 (gchar *, priv->array->len + 1);
	for (i = 0; i < priv->array->len; i++) {
		item_tmp = g_ptr_array_index (priv->array, i);
		bus_names[i] = g_strdup (item_tmp->sender);
	}
	return bus_names;
}

/**
 * cd_inhibit_valid:
 **/
static void
cd_inhibit_item_free (CdInhibitItem *item)
{
	if (item->watcher_id > 0)
		g_bus_unwatch_name (item->watcher_id);
	g_free (item->sender);
	g_free (item);
}

/**
 * cd_inhibit_get_by_sender:
 **/
static CdInhibitItem *
cd_inhibit_get_by_sender (CdInhibit *inhibit,
			  const gchar *sender)
{
	CdInhibitItem *item = NULL;
	CdInhibitItem *item_tmp;
	CdInhibitPrivate *priv = inhibit->priv;
	guint i;

	/* find sender */
	for (i = 0; i < priv->array->len; i++) {
		item_tmp = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (item_tmp->sender, sender) == 0) {
			item = item_tmp;
			break;
		}
	}
	return item;
}

/**
 * cd_inhibit_remove:
 **/
gboolean
cd_inhibit_remove (CdInhibit *inhibit, const gchar *sender, GError **error)
{
	CdInhibitItem *item;
	gboolean ret = TRUE;

	g_return_val_if_fail (CD_IS_INHIBIT (inhibit), FALSE);
	g_return_val_if_fail (sender != NULL, FALSE);

	/* do we already exist */
	item = cd_inhibit_get_by_sender (inhibit, sender);
	if (item == NULL) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "not set inhibitor for %s",
			     sender);
		goto out;
	}
 
	/* remove */
	ret = g_ptr_array_remove (inhibit->priv->array, item);
	if (!ret)
		goto out;

	/* emit signal */
	g_debug ("CdInhibit: emit changed");
	g_signal_emit (inhibit, signals[SIGNAL_CHANGED], 0);

out:
	return ret;
}

/**
 * cd_inhibit_name_vanished_cb:
 **/
static void
cd_inhibit_name_vanished_cb (GDBusConnection *connection,
			     const gchar *name,
			     gpointer user_data)
{
	CdInhibit *inhibit = CD_INHIBIT (user_data);
	gboolean ret;
	GError *error = NULL;

	/* just remove */
	ret = cd_inhibit_remove (inhibit, name, &error);
	if (!ret) {
		g_warning ("CdInhibit: failed to remove when %s vanished: %s",
			   name, error->message);
		g_error_free (error);
	} else {
		g_debug ("CdInhibit: remove inhibit as %s vanished",
			 name);
	}
}

/**
 * cd_inhibit_add:
 **/
gboolean
cd_inhibit_add (CdInhibit *inhibit, const gchar *sender, GError **error)
{
	CdInhibitItem *item;
	gboolean ret = TRUE;

	g_return_val_if_fail (CD_IS_INHIBIT (inhibit), FALSE);
	g_return_val_if_fail (sender != NULL, FALSE);

	/* do we already exist */
	item = cd_inhibit_get_by_sender (inhibit, sender);
	if (item != NULL) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "already set inhibitor for %s",
			     sender);
		goto out;
	}

	/* add */
	item = g_new0 (CdInhibitItem, 1);
	item->sender = g_strdup (sender);
	item->watcher_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
					     item->sender,
					     G_BUS_NAME_WATCHER_FLAGS_NONE,
					     NULL,
					     cd_inhibit_name_vanished_cb,
					     g_object_ref (inhibit),
					     g_object_unref);
	g_ptr_array_add (inhibit->priv->array, item);

	/* emit signal */
	g_debug ("CdInhibit: emit changed");
	g_signal_emit (inhibit, signals[SIGNAL_CHANGED], 0);

out:
	return ret;
}

/**
 * cd_inhibit_class_init:
 **/
static void
cd_inhibit_class_init (CdInhibitClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_inhibit_finalize;

	signals [SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdInhibitClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (CdInhibitPrivate));
}

/**
 * cd_inhibit_init:
 **/
static void
cd_inhibit_init (CdInhibit *inhibit)
{
	inhibit->priv = CD_INHIBIT_GET_PRIVATE (inhibit);
	inhibit->priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_inhibit_item_free);
}

/**
 * cd_inhibit_finalize:
 **/
static void
cd_inhibit_finalize (GObject *object)
{
	CdInhibit *inhibit = CD_INHIBIT (object);
	CdInhibitPrivate *priv = inhibit->priv;

	g_ptr_array_unref (priv->array);

	G_OBJECT_CLASS (cd_inhibit_parent_class)->finalize (object);
}

/**
 * cd_inhibit_new:
 **/
CdInhibit *
cd_inhibit_new (void)
{
	CdInhibit *inhibit;
	inhibit = g_object_new (CD_TYPE_INHIBIT, NULL);
	return CD_INHIBIT (inhibit);
}

