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

/**
 * SECTION:cd-icc
 * @short_description: A XML parser that exposes a ICC tree
 */

#include "config.h"

#include <glib.h>
#include <lcms2.h>
#include <string.h>
#include <stdlib.h>

#include "cd-icc.h"

static void	cd_icc_class_init	(CdIccClass	*klass);
static void	cd_icc_init		(CdIcc		*icc);
static void	cd_icc_finalize		(GObject	*object);

#define CD_ICC_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_ICC, CdIccPrivate))

/**
 * CdIccPrivate:
 *
 * Private #CdIcc data
 **/
struct _CdIccPrivate
{
	cmsHPROFILE		 lcms_profile;
	guint32			 size;
};

G_DEFINE_TYPE (CdIcc, cd_icc, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_SIZE,
	PROP_LAST
};

/**
 * cd_icc_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.32
 **/
GQuark
cd_icc_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("cd_icc_error");
	return quark;
}

/**
 * cd_icc_fix_utf8_string:
 *
 * NC entries are supposed to be 7-bit ASCII, although some profile vendors
 * try to be clever which breaks handling them as UTF-8.
 **/
static gboolean
cd_icc_fix_utf8_string (GString *string)
{
	guint i;
	guchar tmp;

	/* replace clever characters */
	for (i = 0; i < string->len; i++) {
		tmp = (guchar) string->str[i];

		/* (R) */
		if (tmp == 0xae) {
			string->str[i] = 0xc2;
			g_string_insert_c (string, i + 1, tmp);
			i += 1;
		}

		/* unknown */
		if (tmp == 0x86)
			g_string_erase (string, i, 1);
	}

	/* check if we repaired it okay */
	return g_utf8_validate (string->str, string->len, NULL);
}

/**
 * cd_icc_to_string:
 * @icc: a #CdIcc instance.
 *
 * Returns a string representation of the ICC profile.
 *
 * Return value: an allocated string
 *
 * Since: 0.1.32
 **/
gchar *
cd_icc_to_string (CdIcc *icc)
{
	CdIccPrivate *priv = icc->priv;
	cmsInt32Number tag_size;
	cmsTagSignature sig;
	cmsTagTypeSignature tag_type;
	gboolean ret;
	gchar tag_str[5] = "    ";
	GString *str;
	guint32 i;
	guint32 number_tags;
	guint32 tmp;

	g_return_val_if_fail (CD_IS_ICC (icc), NULL);

	/* print header */
	str = g_string_new ("icc:\nHeader:\n");

	/* print size */
	tmp = cd_icc_get_size (icc);
	if (tmp > 0)
		g_string_append_printf (str, "  Size\t\t= %i bytes\n", tmp);

	/* print tags */
	g_string_append (str, "\n");
	number_tags = cmsGetTagCount (priv->lcms_profile);
	for (i = 0; i < number_tags; i++) {
		sig = cmsGetTagSignature (priv->lcms_profile, i);

		/* convert to text */
		tmp = GUINT32_FROM_BE (sig);
		memcpy (tag_str, &tmp, 4);

		/* print header */
		g_string_append_printf (str, "tag %02i:\n", i);
		g_string_append_printf (str, "  sig\t'%s' [0x%x]\n", tag_str, sig);
		tag_size = cmsReadRawTag (priv->lcms_profile, sig, &tmp, 4);
		memcpy (tag_str, &tmp, 4);
		tag_type = GUINT32_FROM_BE (tmp);
		g_string_append_printf (str, "  type\t'%s' [0x%x]\n", tag_str, tag_type);
		g_string_append_printf (str, "  size\t%i\n", tag_size);

		/* print tag details */
		switch (tag_type) {
		case cmsSigTextType:
		case cmsSigTextDescriptionType:
		case cmsSigMultiLocalizedUnicodeType:
		{
			cmsMLU *mlu;
			gchar text_buffer[128];
			guint32 text_size;

			g_string_append_printf (str, "Text:\n");
			mlu = cmsReadTag (priv->lcms_profile, sig);
			if (mlu == NULL) {
				g_string_append_printf (str, "  Info:\t\tMLU invalid!\n");
				break;
			}
			text_size = cmsMLUgetASCII (mlu,
						    cmsNoLanguage,
						    cmsNoCountry,
						    text_buffer,
						    sizeof (text_buffer));
			if (text_size > 0) {
				g_string_append_printf (str, "  en_US:\t%s [%i bytes]\n",
							text_buffer, text_size);
			}
			break;
		}
		case cmsSigXYZType:
		{
			cmsCIEXYZ *xyz;
			xyz = cmsReadTag (priv->lcms_profile, sig);
			g_string_append_printf (str, "XYZ:\n");
			g_string_append_printf (str, "  X:%f Y:%f Z:%f\n",
						xyz->X, xyz->Y, xyz->Z);
			break;
		}
		case cmsSigCurveType:
		{
			cmsToneCurve *curve;
			gdouble estimated_gamma;
			g_string_append_printf (str, "Curve:\n");
			curve = cmsReadTag (priv->lcms_profile, sig);
			estimated_gamma = cmsEstimateGamma (curve, 0.01);
			if (estimated_gamma > 0) {
				g_string_append_printf (str,
							"  Curve is gamma of %f\n",
							estimated_gamma);
			}
			break;
		}
		case cmsSigDictType:
		{
			cmsHANDLE dict;
			const cmsDICTentry *entry;
			gchar ascii_name[1024];
			gchar ascii_value[1024];

			g_string_append_printf (str, "Dictionary:\n");
			dict = cmsReadTag (priv->lcms_profile, sig);
			for (entry = cmsDictGetEntryList (dict);
			     entry != NULL;
			     entry = cmsDictNextEntry (entry)) {

				/* convert from wchar_t to UTF-8 */
				wcstombs (ascii_name, entry->Name, sizeof (ascii_name));
				wcstombs (ascii_value, entry->Value, sizeof (ascii_value));
				g_string_append_printf (str, "  %s\t->\t%s\n",
							ascii_name, ascii_value);
			}
			break;
		}
		case cmsSigNamedColor2Type:
		{
			CdColorLab lab;
			cmsNAMEDCOLORLIST *nc2;
			cmsUInt16Number pcs[3];
			gchar name[cmsMAX_PATH];
			gchar prefix[33];
			gchar suffix[33];
			GString *string;
			guint j;

			g_string_append_printf (str, "Named colors:\n");
			nc2 = cmsReadTag (priv->lcms_profile, sig);
			if (nc2 == NULL) {
				g_string_append_printf (str, "  Info:\t\tNC invalid!\n");
				continue;
			}

			/* get the number of NCs */
			tmp = cmsNamedColorCount (nc2);
			if (tmp == 0) {
				g_string_append_printf (str, "  Info:\t\tNo NC's!\n");
				continue;
			}
			for (j = 0; j < tmp; j++) {

				/* parse title */
				string = g_string_new ("");
				ret = cmsNamedColorInfo (nc2, j,
							 name,
							 prefix,
							 suffix,
							 (cmsUInt16Number *)&pcs,
							 NULL);
				if (!ret) {
					g_string_append_printf (str, "  Info:\t\tFailed to get NC #%i", j);
					continue;
				}
				if (prefix[0] != '\0')
					g_string_append_printf (string, "%s ", prefix);
				g_string_append (string, name);
				if (suffix[0] != '\0')
					g_string_append_printf (string, " %s", suffix);

				/* check is valid */
				ret = g_utf8_validate (string->str, string->len, NULL);
				if (!ret) {
					g_string_append (str, "  Info:\t\tInvalid 7 bit ASCII / UTF8\n");
					ret = cd_icc_fix_utf8_string (string);
					if (!ret) {
						g_string_append (str, "  Info:\t\tIFailed to fix: skipping entry\n");
						continue;
					}
				}

				/* get color */
				cmsLabEncoded2Float ((cmsCIELab *) &lab, pcs);
				g_string_append_printf (str, "  %03i:\t %s\tL:%.2f a:%.3f b:%.3f\n",
							j,
							string->str,
							lab.L, lab.a, lab.b);
				g_string_free (string, TRUE);
			}
			break;
		}
		default:
			break;
		}

		/* done! */
		g_string_append_printf (str, "\n");
	}

	/* remove trailing newline */
	if (str->len > 0)
		g_string_truncate (str, str->len - 1);

	return g_string_free (str, FALSE);
}

/**
 * cd_icc_load_data:
 * @icc: a #CdIcc instance.
 * @data: binary data
 * @data_len: Length of @data
 * @error: A #GError or %NULL
 *
 * Loads an ICC profile from raw byte data.
 *
 * Since: 0.1.32
 **/
gboolean
cd_icc_load_data (CdIcc *icc,
		  const guint8 *data,
		  gsize data_len,
		  GError **error)
{
	CdIccPrivate *priv = icc->priv;
	gboolean ret = TRUE;

	g_return_val_if_fail (CD_IS_ICC (icc), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (data_len != 0, FALSE);
	g_return_val_if_fail (priv->lcms_profile == NULL, FALSE);

	/* ensure we have the header */
	if (data_len < 0x84) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_PARSE,
				     "icc was not valid (file size too small)");
		goto out;
	}

	/* load icc into lcms */
	priv->lcms_profile = cmsOpenProfileFromMem (data, data_len);
	if (priv->lcms_profile == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_PARSE,
				     "failed to load: not an ICC icc");
		goto out;
	}

	/* save length to avoid trusting the profile */
	priv->size = data_len;
out:
	return ret;
}

/**
 * cd_icc_load_file:
 * @icc: a #CdIcc instance.
 * @file: a #GFile
 * @error: A #GError or %NULL
 *
 * Loads an ICC profile from a local or remote file.
 *
 * Since: 0.1.32
 **/
gboolean
cd_icc_load_file (CdIcc *icc,
		  GFile *file,
		  GError **error)
{
	gboolean ret = FALSE;
	gchar *data = NULL;
	GError *error_local = NULL;
	gsize length;

	g_return_val_if_fail (CD_IS_ICC (icc), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	/* load files */
	ret = g_file_load_contents (file, NULL, &data, &length,
				    NULL, &error_local);
	if (!ret) {
		g_set_error (error,
			     CD_ICC_ERROR,
			     CD_ICC_ERROR_FAILED_TO_OPEN,
			     "failed to load file: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* parse the data */
	ret = cd_icc_load_data (icc, (const guint8 *) data, length, error);
	if (!ret)
		goto out;
out:
	g_free (data);
	return ret;
}

/**
 * cd_icc_load_fd:
 * @icc: a #CdIcc instance.
 * @fd: a file descriptor
 * @error: A #GError or %NULL
 *
 * Loads an ICC profile from an open file descriptor.
 *
 * Since: 0.1.32
 **/
gboolean
cd_icc_load_fd (CdIcc *icc,
		gint fd,
		GError **error)
{
	CdIccPrivate *priv = icc->priv;
	FILE *stream = NULL;
	gboolean ret = TRUE;

	g_return_val_if_fail (CD_IS_ICC (icc), FALSE);
	g_return_val_if_fail (fd > 0, FALSE);

	/* convert the file descriptor to a stream */
	stream = fdopen (fd, "r");
	if (stream == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_ICC_ERROR,
			     CD_ICC_ERROR_FAILED_TO_OPEN,
			     "failed to open stream from fd %i",
			     fd);
		goto out;
	}

	/* parse the ICC file */
	priv->lcms_profile = cmsOpenProfileFromStream (stream, "r");
	if (priv->lcms_profile == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_OPEN,
				     "failed to open stream");
		goto out;
	}
out:
	return ret;
}

/**
 * cd_icc_get_handle:
 * @icc: a #CdIcc instance.
 *
 * Return the cmsHPROFILE instance used locally. This may be required if you
 * are using the profile in a transform.
 *
 * Return value: (transfer none): Do not call cmsCloseProfile() on this value!
 **/
gpointer
cd_icc_get_handle (CdIcc *icc)
{
	g_return_val_if_fail (CD_IS_ICC (icc), NULL);
	return icc->priv->lcms_profile;
}

/**
 * cd_icc_get_size:
 *
 * Gets the ICC profile file size
 *
 * Return value: The size in bytes, or 0 for unknown.
 *
 * Since: 0.1.32
 **/
guint32
cd_icc_get_size (CdIcc *icc)
{
	g_return_val_if_fail (CD_IS_ICC (icc), 0);
	return icc->priv->size;
}

/**
 * cd_icc_get_property:
 **/
static void
cd_icc_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	CdIcc *icc = CD_ICC (object);
	CdIccPrivate *priv = icc->priv;

	switch (prop_id) {
	case PROP_SIZE:
		g_value_set_uint (value, priv->size);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * cd_icc_set_property:
 **/
static void
cd_icc_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * cd_icc_class_init:
 */
static void
cd_icc_class_init (CdIccClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = cd_icc_finalize;
	object_class->get_property = cd_icc_get_property;
	object_class->set_property = cd_icc_set_property;

	/**
	 * CdIcc:size:
	 */
	pspec = g_param_spec_uint ("size", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_SIZE, pspec);

	g_type_class_add_private (klass, sizeof (CdIccPrivate));
}

/**
 * cd_icc_init:
 */
static void
cd_icc_init (CdIcc *icc)
{
	icc->priv = CD_ICC_GET_PRIVATE (icc);
}

/**
 * cd_icc_finalize:
 */
static void
cd_icc_finalize (GObject *object)
{
	CdIcc *icc = CD_ICC (object);
	CdIccPrivate *priv = icc->priv;

	if (priv->lcms_profile != NULL)
		cmsCloseProfile (priv->lcms_profile);

	G_OBJECT_CLASS (cd_icc_parent_class)->finalize (object);
}

/**
 * cd_icc_new:
 *
 * Creates a new #CdIcc object.
 *
 * Return value: a new CdIcc object.
 *
 * Since: 0.1.32
 **/
CdIcc *
cd_icc_new (void)
{
	CdIcc *icc;
	icc = g_object_new (CD_TYPE_ICC, NULL);
	return CD_ICC (icc);
}
