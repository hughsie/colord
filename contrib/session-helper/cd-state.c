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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <signal.h>
#include <gio/gio.h>

#include "cd-state.h"

#define CD_STATE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_STATE, CdStatePrivate))

struct _CdStatePrivate
{
	gboolean		 enable_profile;
	gboolean		 process_event_sources;
	gchar			*id;
	gdouble			 global_share;
	gdouble			*step_profile;
	GTimer			*timer;
	guint			 current;
	guint			 last_percentage;
	guint			*step_data;
	guint			 steps;
	gulong			 percentage_child_id;
	gulong			 subpercentage_child_id;
	CdState			*child;
	CdState			*parent;
};

enum {
	SIGNAL_PERCENTAGE_CHANGED,
	SIGNAL_SUBPERCENTAGE_CHANGED,
	SIGNAL_LAST
};

enum {
	PROP_0,
	PROP_SPEED,
	PROP_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (CdState, cd_state, G_TYPE_OBJECT)

#define CD_STATE_SPEED_SMOOTHING_ITEMS		5

/**
 * cd_state_error_quark:
 **/
GQuark
cd_state_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("cd_state_error");
	return quark;
}

/**
 * cd_state_set_enable_profile:
 **/
void
cd_state_set_enable_profile (CdState *state, gboolean enable_profile)
{
	g_return_if_fail (CD_IS_STATE (state));
	state->priv->enable_profile = enable_profile;
}

/**
 * cd_state_discrete_to_percent:
 **/
static gfloat
cd_state_discrete_to_percent (guint discrete, guint steps)
{
	/* check we are in range */
	if (discrete > steps)
		return 100;
	if (steps == 0) {
		g_warning ("steps is 0!");
		return 0;
	}
	return ((gfloat) discrete * (100.0f / (gfloat) (steps)));
}

/**
 * cd_state_print_parent_chain:
 **/
static void
cd_state_print_parent_chain (CdState *state, guint level)
{
	if (state->priv->parent != NULL)
		cd_state_print_parent_chain (state->priv->parent, level + 1);
	g_print ("%i) %s (%i/%i)\n",
		 level, state->priv->id, state->priv->current, state->priv->steps);
}

/**
 * cd_state_set_percentage:
 **/
gboolean
cd_state_set_percentage (CdState *state, guint percentage)
{
	gboolean ret = FALSE;

	/* is it the same */
	if (percentage == state->priv->last_percentage)
		goto out;

	/* is it invalid */
	if (percentage > 100) {
		cd_state_print_parent_chain (state, 0);
		g_warning ("percentage %i%% is invalid on %p!",
			   percentage, state);
		goto out;
	}

	/* is it less */
	if (percentage < state->priv->last_percentage) {
		if (state->priv->enable_profile) {
			cd_state_print_parent_chain (state, 0);
			g_critical ("percentage should not go down from %i to %i on %p!",
				    state->priv->last_percentage, percentage, state);
		}
		goto out;
	}

	/* save */
	state->priv->last_percentage = percentage;

	/* are we so low we don't care */
	if (state->priv->global_share < 0.001)
		goto out;

	/* emit */
	g_signal_emit (state, signals [SIGNAL_PERCENTAGE_CHANGED], 0, percentage);

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cd_state_get_percentage:
 **/
guint
cd_state_get_percentage (CdState *state)
{
	return state->priv->last_percentage;
}

/**
 * cd_state_set_subpercentage:
 **/
static gboolean
cd_state_set_subpercentage (CdState *state, guint percentage)
{
	/* are we so low we don't care */
	if (state->priv->global_share < 0.01)
		goto out;

	/* just emit */
	g_signal_emit (state, signals [SIGNAL_SUBPERCENTAGE_CHANGED], 0, percentage);
out:
	return TRUE;
}

/**
 * cd_state_child_percentage_changed_cb:
 **/
static void
cd_state_child_percentage_changed_cb (CdState *child, guint percentage, CdState *state)
{
	gfloat offset;
	gfloat range;
	gfloat extra;
	guint parent_percentage;

	/* propagate up the stack if CdState has only one step */
	if (state->priv->steps == 1) {
		cd_state_set_percentage (state, percentage);
		return;
	}

	/* did we call done on a state that did not have a size set? */
	if (state->priv->steps == 0)
		return;

	/* always provide two levels of signals */
	cd_state_set_subpercentage (state, percentage);

	/* already at >= 100% */
	if (state->priv->current >= state->priv->steps) {
		g_warning ("already at %i/%i steps on %p", state->priv->current, state->priv->steps, state);
		return;
	}

	/* we have to deal with non-linear steps */
	if (state->priv->step_data != NULL) {
		/* we don't store zero */
		if (state->priv->current == 0) {
			parent_percentage = percentage * state->priv->step_data[state->priv->current] / 100;
		} else {
			/* bilinearly interpolate for XXXXXXXXXXXXXXXXXXXX */
			parent_percentage = (((100 - percentage) * state->priv->step_data[state->priv->current-1]) +
					     (percentage * state->priv->step_data[state->priv->current])) / 100;
		}
		goto out;
	}

	/* get the offset */
	offset = cd_state_discrete_to_percent (state->priv->current, state->priv->steps);

	/* get the range between the parent step and the next parent step */
	range = cd_state_discrete_to_percent (state->priv->current+1, state->priv->steps) - offset;
	if (range < 0.01) {
		g_warning ("range=%f (from %i to %i), should be impossible", range, state->priv->current+1, state->priv->steps);
		return;
	}

	/* get the extra contributed by the child */
	extra = ((gfloat) percentage / 100.0f) * range;

	/* emit from the parent */
	parent_percentage = (guint) (offset + extra);
out:
	cd_state_set_percentage (state, parent_percentage);
}

/**
 * cd_state_child_subpercentage_changed_cb:
 **/
static void
cd_state_child_subpercentage_changed_cb (CdState *child, guint percentage, CdState *state)
{
	/* discard this, unless the CdState has only one step */
	if (state->priv->steps != 1)
		return;

	/* propagate up the stack as if the parent didn't exist */
	cd_state_set_subpercentage (state, percentage);
}

/**
 * cd_state_reset:
 **/
gboolean
cd_state_reset (CdState *state)
{
	gboolean ret = TRUE;

	g_return_val_if_fail (CD_IS_STATE (state), FALSE);

	/* reset values */
	state->priv->steps = 0;
	state->priv->current = 0;
	state->priv->last_percentage = 0;

	/* only use the timer if profiling; it's expensive */
	if (state->priv->enable_profile)
		g_timer_start (state->priv->timer);

	/* disconnect client */
	if (state->priv->percentage_child_id != 0) {
		g_signal_handler_disconnect (state->priv->child,
					     state->priv->percentage_child_id);
		state->priv->percentage_child_id = 0;
	}
	if (state->priv->subpercentage_child_id != 0) {
		g_signal_handler_disconnect (state->priv->child,
					     state->priv->subpercentage_child_id);
		state->priv->subpercentage_child_id = 0;
	}

	/* unref child */
	if (state->priv->child != NULL) {
		g_object_unref (state->priv->child);
		state->priv->child = NULL;
	}

	/* no more step data */
	g_free (state->priv->step_data);
	g_free (state->priv->step_profile);
	state->priv->step_data = NULL;
	state->priv->step_profile = NULL;
	return ret;
}

/**
 * cd_state_set_global_share:
 **/
static void
cd_state_set_global_share (CdState *state, gdouble global_share)
{
	state->priv->global_share = global_share;
}

/**
 * cd_state_get_child:
 **/
CdState *
cd_state_get_child (CdState *state)
{
	CdState *child = NULL;

	g_return_val_if_fail (CD_IS_STATE (state), NULL);

	/* already set child */
	if (state->priv->child != NULL) {
		g_signal_handler_disconnect (state->priv->child,
					     state->priv->percentage_child_id);
		g_signal_handler_disconnect (state->priv->child,
					     state->priv->subpercentage_child_id);
		g_object_unref (state->priv->child);
	}

	/* connect up signals */
	child = cd_state_new ();
	child->priv->parent = state; /* do not ref! */
	state->priv->child = child;
	state->priv->percentage_child_id =
		g_signal_connect (child, "percentage-changed",
				  G_CALLBACK (cd_state_child_percentage_changed_cb),
				  state);
	state->priv->subpercentage_child_id =
		g_signal_connect (child, "subpercentage-changed",
				  G_CALLBACK (cd_state_child_subpercentage_changed_cb),
				  state);
	/* reset child */
	child->priv->current = 0;
	child->priv->last_percentage = 0;

	/* set the global share on the new child */
	cd_state_set_global_share (child, state->priv->global_share);

	/* set the profile state */
	cd_state_set_enable_profile (child,
				      state->priv->enable_profile);

	return child;
}

/**
 * cd_state_set_number_steps_real:
 **/
gboolean
cd_state_set_number_steps_real (CdState *state, guint steps, const gchar *strloc)
{
	gboolean ret = FALSE;

	g_return_val_if_fail (state != NULL, FALSE);

	/* nothing to do for 0 steps */
	if (steps == 0) {
		ret = TRUE;
		goto out;
	}

	/* did we call done on a state that did not have a size set? */
	if (state->priv->steps != 0) {
		g_warning ("steps already set to %i, can't set %i! [%s]",
			     state->priv->steps, steps, strloc);
		cd_state_print_parent_chain (state, 0);
		goto out;
	}

	/* set id */
	g_free (state->priv->id);
	state->priv->id = g_strdup_printf ("%s", strloc);

	/* only use the timer if profiling; it's expensive */
	if (state->priv->enable_profile)
		g_timer_start (state->priv->timer);

	/* imply reset */
	cd_state_reset (state);

	/* set steps */
	state->priv->steps = steps;

	/* global share just got smaller */
	state->priv->global_share /= steps;

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cd_state_set_steps_real:
 **/
gboolean
cd_state_set_steps_real (CdState *state, GError **error, const gchar *strloc, gint value, ...)
{
	va_list args;
	guint i;
	gint value_temp;
	guint total;
	gboolean ret = FALSE;

	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* we must set at least one thing */
	total = value;

	/* process the valist */
	va_start (args, value);
	for (i = 0;; i++) {
		value_temp = va_arg (args, gint);
		if (value_temp == -1)
			break;
		total += (guint) value_temp;
	}
	va_end (args);

	/* does not sum to 100% */
	if (total != 100) {
		g_set_error (error,
			     CD_STATE_ERROR,
			     CD_STATE_ERROR_INVALID,
			     "percentage not 100: %i",
			     total);
		goto out;
	}

	/* set step number */
	ret = cd_state_set_number_steps_real (state, i+1, strloc);
	if (!ret) {
		g_set_error (error,
			     CD_STATE_ERROR,
			     CD_STATE_ERROR_INVALID,
			     "failed to set number steps: %i",
			     i+1);
		goto out;
	}

	/* save this data */
	total = value;
	state->priv->step_data = g_new0 (guint, i+2);
	state->priv->step_profile = g_new0 (gdouble, i+2);
	state->priv->step_data[0] = total;
	va_start (args, value);
	for (i = 0;; i++) {
		value_temp = va_arg (args, gint);
		if (value_temp == -1)
			break;

		/* we pre-add the data to make access simpler */
		total += (guint) value_temp;
		state->priv->step_data[i+1] = total;
	}
	va_end (args);

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cd_state_show_profile:
 **/
static void
cd_state_show_profile (CdState *state)
{
	gdouble division;
	gdouble total_time = 0.0f;
	GString *result;
	guint i;
	guint uncumalitive = 0;

	/* get the total time so we can work out the divisor */
	for (i = 0; i < state->priv->steps; i++)
		total_time += state->priv->step_profile[i];
	division = total_time / 100.0f;

	/* what we set */
	result = g_string_new ("steps were set as [ ");
	for (i = 0; i < state->priv->steps; i++) {
		g_string_append_printf (result, "%i, ",
					state->priv->step_data[i] - uncumalitive);
		uncumalitive = state->priv->step_data[i];
	}

	/* what we _should_ have set */
	g_string_append_printf (result, "-1 ] but should have been: [ ");
	for (i = 0; i < state->priv->steps; i++) {
		g_string_append_printf (result, "%.0f, ",
					state->priv->step_profile[i] / division);
	}
	g_printerr ("\n\n%s-1 ] at %s\n\n", result->str, state->priv->id);
	g_string_free (result, TRUE);
}

/**
 * cd_state_done_real:
 **/
gboolean
cd_state_done_real (CdState *state, GError **error, const gchar *strloc)
{
	gboolean ret = TRUE;
	gdouble elapsed;
	gfloat percentage;

	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* did we call done on a state that did not have a size set? */
	if (state->priv->steps == 0) {
		g_set_error (error, CD_STATE_ERROR, CD_STATE_ERROR_INVALID,
			     "done on a state %p that did not have a size set! [%s]",
			     state, strloc);
		cd_state_print_parent_chain (state, 0);
		ret = FALSE;
		goto out;
	}

	/* save the step interval for profiling */
	if (state->priv->enable_profile) {
		/* save the duration in the array */
		elapsed = g_timer_elapsed (state->priv->timer, NULL);
		if (state->priv->step_profile != NULL)
			state->priv->step_profile[state->priv->current] = elapsed;
		g_timer_start (state->priv->timer);
	}

	/* is already at 100%? */
	if (state->priv->current >= state->priv->steps) {
		g_set_error (error, CD_STATE_ERROR, CD_STATE_ERROR_INVALID,
			     "already at 100%% state [%s]", strloc);
		cd_state_print_parent_chain (state, 0);
		ret = FALSE;
		goto out;
	}

	/* is child not at 100%? */
	if (state->priv->child != NULL) {
		CdStatePrivate *child_priv = state->priv->child->priv;
		if (child_priv->current != child_priv->steps) {
			g_print ("child is at %i/%i steps and parent done [%s]\n",
				 child_priv->current, child_priv->steps, strloc);
			cd_state_print_parent_chain (state->priv->child, 0);
			ret = TRUE;
			/* do not abort, as we want to clean this up */
		}
	}

	/* another */
	state->priv->current++;

	/* find new percentage */
	if (state->priv->step_data == NULL) {
		percentage = cd_state_discrete_to_percent (state->priv->current,
							    state->priv->steps);
	} else {
		/* this is cumalative */
		percentage = state->priv->step_data[state->priv->current - 1];
	}
	cd_state_set_percentage (state, (guint) percentage);

	/* show any profiling stats */
	if (state->priv->enable_profile &&
	    state->priv->current == state->priv->steps &&
	    state->priv->step_profile != NULL) {
		cd_state_show_profile (state);
	}

	/* reset child if it exists */
	if (state->priv->child != NULL)
		cd_state_reset (state->priv->child);
out:
	return ret;
}

/**
 * cd_state_finished_real:
 **/
gboolean
cd_state_finished_real (CdState *state, GError **error, const gchar *strloc)
{
	gboolean ret = TRUE;

	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* is already at 100%? */
	if (state->priv->current == state->priv->steps)
		goto out;

	/* all done */
	state->priv->current = state->priv->steps;

	/* set new percentage */
	cd_state_set_percentage (state, 100);
out:
	return ret;
}

/**
 * cd_state_finalize:
 **/
static void
cd_state_finalize (GObject *object)
{
	CdState *state;

	g_return_if_fail (object != NULL);
	g_return_if_fail (CD_IS_STATE (object));
	state = CD_STATE (object);

	cd_state_reset (state);
	g_free (state->priv->id);
	g_free (state->priv->step_data);
	g_free (state->priv->step_profile);
	g_timer_destroy (state->priv->timer);

	G_OBJECT_CLASS (cd_state_parent_class)->finalize (object);
}

/**
 * cd_state_class_init:
 **/
static void
cd_state_class_init (CdStateClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_state_finalize;

	signals [SIGNAL_PERCENTAGE_CHANGED] =
		g_signal_new ("percentage-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdStateClass, percentage_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);

	signals [SIGNAL_SUBPERCENTAGE_CHANGED] =
		g_signal_new ("subpercentage-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdStateClass, subpercentage_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);

	g_type_class_add_private (klass, sizeof (CdStatePrivate));
}

/**
 * cd_state_init:
 **/
static void
cd_state_init (CdState *state)
{
	state->priv = CD_STATE_GET_PRIVATE (state);
	state->priv->global_share = 1.0f;
	state->priv->timer = g_timer_new ();
}

/**
 * cd_state_new:
 **/
CdState *
cd_state_new (void)
{
	CdState *state;
	state = g_object_new (CD_TYPE_STATE, NULL);
	return CD_STATE (state);
}
