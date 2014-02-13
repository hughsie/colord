/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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
 * SECTION:cd-context-lcms
 * @short_description: Functionality to save per-thread context data for LCMS2
 */

#include "config.h"

#include <lcms2.h>
#include <lcms2_plugin.h>

#include "cd-context-lcms.h"
#include "cd-icc.h" /* for the error codes */

/*< private >*/
#define LCMS_CURVE_PLUGIN_TYPE_REC709	1024

/**
 * cd_context_lcms_get_error:
 **/
static GError **
cd_context_lcms_get_error (gpointer ctx)
{
	GError **error_ctx;
#ifdef HAVE_LCMS_CREATE_CONTEXT
	error_ctx = cmsGetContextUserData (ctx);
#else
	error_ctx = (GError **) ctx;
#endif
	return error_ctx;
}

/**
 * cd_context_lcms2_error_cb:
 **/
static void
cd_context_lcms2_error_cb (cmsContext context_id,
			   cmsUInt32Number code,
			   const gchar *message)
{
	gint error_code;
	GError **error_ctx;

	/* nothing set, must be pre-2.6 */
	if (context_id == NULL) {
		g_warning ("Error handler called with no context: %s", message);
		return;
	}

	/* there's already one error pending */
	error_ctx = cd_context_lcms_get_error (context_id);
	if (*error_ctx != NULL) {
		g_prefix_error (error_ctx, "%s & ", message);
		return;
	}

	/* convert the first cmsERROR in into a CdIccError */
	switch (code) {
	case cmsERROR_CORRUPTION_DETECTED:
		error_code = CD_ICC_ERROR_CORRUPTION_DETECTED;
		break;
	case cmsERROR_FILE:
	case cmsERROR_READ:
	case cmsERROR_SEEK:
		error_code = CD_ICC_ERROR_FAILED_TO_OPEN;
		break;
	case cmsERROR_WRITE:
		error_code = CD_ICC_ERROR_FAILED_TO_SAVE;
		break;
	case cmsERROR_COLORSPACE_CHECK:
		error_code = CD_ICC_ERROR_INVALID_COLORSPACE;
		break;
	case cmsERROR_BAD_SIGNATURE:
		error_code = CD_ICC_ERROR_FAILED_TO_PARSE;
		break;
	case cmsERROR_ALREADY_DEFINED:
	case cmsERROR_INTERNAL:
	case cmsERROR_NOT_SUITABLE:
	case cmsERROR_NULL:
	case cmsERROR_RANGE:
	case cmsERROR_UNDEFINED:
	case cmsERROR_UNKNOWN_EXTENSION:
		error_code = CD_ICC_ERROR_INTERNAL;
		break;
	default:
		g_warning ("LCMS2 error code not recognised; please report");
		error_code = CD_ICC_ERROR_INTERNAL;
	}
	error_ctx = cd_context_lcms_get_error (context_id);
	g_set_error_literal (error_ctx, CD_ICC_ERROR, error_code, message);
}

/**
 * cd_context_lcms_plugins_cb:
 **/
static double
cd_context_lcms_plugins_cb (int type, const double params[], double x)
{
	gdouble val = 0.f;

	switch (type) {
	case -LCMS_CURVE_PLUGIN_TYPE_REC709:
		if (x < params[4])
			val = x * params[3];
		else
			val = params[1] * pow (x, (1.f / params[0])) + params[2];
		break;
	case LCMS_CURVE_PLUGIN_TYPE_REC709:
		if (x <= (params[3] * params[4]))
			val = x / params[3];
		else
			val = pow (((x + params[2]) / params[1]), params[0]);
		break;
	}
	return val;
}

cmsPluginParametricCurves cd_icc_lcms_plugins = {
	{ cmsPluginMagicNumber,			/* 'acpp' */
	  2000,					/* minimum version */
	  cmsPluginParametricCurveSig,		/* type */
	  NULL },				/* no more plugins */
	1,					/* number functions */
	{LCMS_CURVE_PLUGIN_TYPE_REC709},	/* function types */
	{5},					/* parameter count */
	cd_context_lcms_plugins_cb		/* evaluator */
};

/**
 * cd_context_lcms_new:
 *
 * Return value: (transfer full): A new LCMS context
 **/
gpointer
cd_context_lcms_new (void)
{
	cmsContext ctx;
	GError **error_ctx;
	error_ctx = g_new0 (GError *, 1);
#ifdef HAVE_LCMS_CREATE_CONTEXT
	ctx = cmsCreateContext (NULL, error_ctx);
	cmsSetLogErrorHandlerTHR (ctx, cd_context_lcms2_error_cb);
	cmsPluginTHR (ctx, &cd_icc_lcms_plugins);
#else
	ctx = (cmsContext) error_ctx;
	cmsSetLogErrorHandler (cd_context_lcms2_error_cb);
	/* we've disabled this as it's unreliable without a context */
	if(0) cmsPlugin (&cd_icc_lcms_plugins);
#endif
	return ctx;
}

/**
 * _cd_context_lcms_pre26_start:
 **/
void
_cd_context_lcms_pre26_start (void)
{
#ifndef HAVE_LCMS_CREATE_CONTEXT
	cmsSetLogErrorHandler (cd_context_lcms2_error_cb);
#endif
}

/**
 * _cd_context_lcms_pre26_stop:
 **/
void
_cd_context_lcms_pre26_stop (void)
{
#ifndef HAVE_LCMS_CREATE_CONTEXT
	cmsSetLogErrorHandler (NULL);
#endif
}

/**
 * cd_context_lcms_free:
 **/
void
cd_context_lcms_free (gpointer ctx)
{
	GError **error_ctx;

#ifdef HAVE_LCMS_CREATE_CONTEXT
	error_ctx = cmsGetContextUserData (ctx);
#else
	error_ctx = (GError **) ctx;
#endif
	g_clear_error (error_ctx);

#ifdef HAVE_LCMS_CREATE_CONTEXT
	cmsUnregisterPluginsTHR (ctx);
	cmsDeleteContext (ctx);
#endif
}

/**
 * cd_context_lcms_error_clear:
 **/
void
cd_context_lcms_error_clear (gpointer ctx)
{
	g_clear_error (cd_context_lcms_get_error (ctx));
}

/**
 * cd_context_lcms_error_check:
 **/
gboolean
cd_context_lcms_error_check (gpointer ctx, GError **error)
{
	GError **error_ctx;
	error_ctx = cd_context_lcms_get_error (ctx);
	if (*error_ctx == NULL)
		return TRUE;
	g_propagate_error (error, *error_ctx);
	*error_ctx = NULL;
	return FALSE;
}
