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

typedef enum {
	CD_MLUC_DESCRIPTION,
	CD_MLUC_COPYRIGHT,
	CD_MLUC_MANUFACTURER,
	CD_MLUC_MODEL,
	CD_MLUC_LAST
} CdIccMluc;

/**
 * CdIccPrivate:
 *
 * Private #CdIcc data
 **/
struct _CdIccPrivate
{
	CdColorspace		 colorspace;
	CdProfileKind		 kind;
	cmsHPROFILE		 lcms_profile;
	gboolean		 can_delete;
	gchar			*filename;
	gdouble			 version;
	GHashTable		*mluc_data[CD_MLUC_LAST];
	GHashTable		*metadata;
	guint32			 size;
};

G_DEFINE_TYPE (CdIcc, cd_icc, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_SIZE,
	PROP_FILENAME,
	PROP_VERSION,
	PROP_KIND,
	PROP_COLORSPACE,
	PROP_CAN_DELETE,
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
	GDateTime *created;
	GString *str;
	guint32 i;
	guint32 number_tags;
	guint32 tmp;
	guint8 profile_id[4];

	g_return_val_if_fail (CD_IS_ICC (icc), NULL);

	/* print header */
	str = g_string_new ("icc:\nHeader:\n");

	/* print size */
	tmp = cd_icc_get_size (icc);
	if (tmp > 0)
		g_string_append_printf (str, "  Size\t\t= %i bytes\n", tmp);

	/* version */
	g_string_append_printf (str, "  Version\t= %.1f\n",
				cd_icc_get_version (icc));

	/* device class */
	g_string_append_printf (str, "  Profile Kind\t= %s\n",
				cd_profile_kind_to_string (cd_icc_get_kind (icc)));

	/* colorspace */
	g_string_append_printf (str, "  Colorspace\t= %s\n",
				cd_colorspace_to_string (cd_icc_get_colorspace (icc)));


	/* PCS */
	g_string_append (str, "  Conn. Space\t= ");
	switch (cmsGetPCS (priv->lcms_profile)) {
	case cmsSigXYZData:
		g_string_append (str, "xyz\n");
		break;
	case cmsSigLabData:
		g_string_append (str, "lab\n");
		break;
	default:
		g_string_append (str, "unknown\n");
		break;
	}

	/* date and time */
	created = cd_icc_get_created (icc);
	if (created != NULL) {
		gchar *created_str;
		created_str = g_date_time_format (created, "%F, %T");
		g_string_append_printf (str, "  Date, Time\t= %s\n", created_str);
		g_free (created_str);
		g_date_time_unref (created);
	}

	/* profile use flags */
	g_string_append (str, "  Flags\t\t= ");
	tmp = cmsGetHeaderFlags (priv->lcms_profile);
	g_string_append (str, (tmp & cmsEmbeddedProfileTrue) > 0 ?
				"Embedded profile" : "Not embedded profile");
	g_string_append (str, ", ");
	g_string_append (str, (tmp & cmsUseWithEmbeddedDataOnly) > 0 ?
				"Use with embedded data only" : "Use anywhere");
	g_string_append (str, "\n");

	/* rendering intent */
	g_string_append (str, "  Rndrng Intnt\t= ");
	switch (cmsGetHeaderRenderingIntent (priv->lcms_profile)) {
	case INTENT_PERCEPTUAL:
		g_string_append (str, "perceptual\n");
		break;
	case INTENT_RELATIVE_COLORIMETRIC:
		g_string_append (str, "relative-colorimetric\n");
		break;
	case INTENT_SATURATION:
		g_string_append (str, "saturation\n");
		break;
	case INTENT_ABSOLUTE_COLORIMETRIC:
		g_string_append (str, "absolute-colorimetric\n");
		break;
	default:
		g_string_append (str, "unknown\n");
		break;
	}

	/* profile ID */
	cmsGetHeaderProfileID (priv->lcms_profile, profile_id);
	g_string_append_printf (str, "  Profile ID\t= 0x%02x%02x%02x%02x\n",
				profile_id[0],
				profile_id[1],
				profile_id[2],
				profile_id[3]);

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
 * cd_icc_load:
 **/
static void
cd_icc_load (CdIcc *icc, CdIccLoadFlags flags)
{
	CdIccPrivate *priv = icc->priv;
	cmsHANDLE dict;

	/* get version */
	priv->version = cmsGetProfileVersion (priv->lcms_profile);

	/* get the profile kind */
	switch (cmsGetDeviceClass (priv->lcms_profile)) {
	case cmsSigInputClass:
		priv->kind = CD_PROFILE_KIND_INPUT_DEVICE;
		break;
	case cmsSigDisplayClass:
		priv->kind = CD_PROFILE_KIND_DISPLAY_DEVICE;
		break;
	case cmsSigOutputClass:
		priv->kind = CD_PROFILE_KIND_OUTPUT_DEVICE;
		break;
	case cmsSigLinkClass:
		priv->kind = CD_PROFILE_KIND_DEVICELINK;
		break;
	case cmsSigColorSpaceClass:
		priv->kind = CD_PROFILE_KIND_COLORSPACE_CONVERSION;
		break;
	case cmsSigAbstractClass:
		priv->kind = CD_PROFILE_KIND_ABSTRACT;
		break;
	case cmsSigNamedColorClass:
		priv->kind = CD_PROFILE_KIND_NAMED_COLOR;
		break;
	default:
		priv->kind = CD_PROFILE_KIND_UNKNOWN;
	}

	/* get colorspace */
	switch (cmsGetColorSpace (priv->lcms_profile)) {
	case cmsSigXYZData:
		priv->colorspace = CD_COLORSPACE_XYZ;
		break;
	case cmsSigLabData:
		priv->colorspace = CD_COLORSPACE_LAB;
		break;
	case cmsSigLuvData:
		priv->colorspace = CD_COLORSPACE_LUV;
		break;
	case cmsSigYCbCrData:
		priv->colorspace = CD_COLORSPACE_YCBCR;
		break;
	case cmsSigYxyData:
		priv->colorspace = CD_COLORSPACE_YXY;
		break;
	case cmsSigRgbData:
		priv->colorspace = CD_COLORSPACE_RGB;
		break;
	case cmsSigGrayData:
		priv->colorspace = CD_COLORSPACE_GRAY;
		break;
	case cmsSigHsvData:
		priv->colorspace = CD_COLORSPACE_HSV;
		break;
	case cmsSigCmykData:
		priv->colorspace = CD_COLORSPACE_CMYK;
		break;
	case cmsSigCmyData:
		priv->colorspace = CD_COLORSPACE_CMY;
		break;
	default:
		priv->colorspace = CD_COLORSPACE_UNKNOWN;
	}

	/* read optional metadata? */
	dict = cmsReadTag (priv->lcms_profile, cmsSigMetaTag);
	if (dict != NULL) {
		const cmsDICTentry *entry;
		gchar ascii_name[1024];
		gchar ascii_value[1024];
		for (entry = cmsDictGetEntryList (dict);
		     entry != NULL;
		     entry = cmsDictNextEntry (entry)) {
			wcstombs (ascii_name, entry->Name, sizeof (ascii_name));
			wcstombs (ascii_value, entry->Value, sizeof (ascii_value));
			g_hash_table_insert (priv->metadata,
					     g_strdup (ascii_name),
					     g_strdup (ascii_value));
		}
	}
}

/**
 * cd_icc_load_data:
 * @icc: a #CdIcc instance.
 * @data: binary data
 * @data_len: Length of @data
 * @flags: a set of #CdIccLoadFlags
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
		  CdIccLoadFlags flags,
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

	/* load cached data */
	cd_icc_load (icc, flags);
out:
	return ret;
}

/**
 * cd_icc_save_file:
 * @icc: a #CdIcc instance.
 * @file: a #GFile
 * @flags: a set of #CdIccSaveFlags
 * @cancellable: A #GCancellable or %NULL
 * @error: A #GError or %NULL
 *
 * Saves an ICC profile to a local or remote file.
 *
 * Return vale: %TRUE for success.
 *
 * Since: 0.1.32
 **/
gboolean
cd_icc_save_file (CdIcc *icc,
		  GFile *file,
		  CdIccSaveFlags flags,
		  GCancellable *cancellable,
		  GError **error)
{
	CdIccPrivate *priv = icc->priv;
	gboolean ret = FALSE;
	gchar *data = NULL;
	GError *error_local = NULL;
	gsize length;

	g_return_val_if_fail (CD_IS_ICC (icc), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	/* get size of profile */
	ret = cmsSaveProfileToMem (priv->lcms_profile,
				   NULL,
				   (guint32 *) &length);
	if (!ret) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_SAVE,
				     "failed to dump ICC file");
		goto out;
	}

	/* allocate and get profile data */
	data = g_new0 (gchar, length);
	ret = cmsSaveProfileToMem (priv->lcms_profile,
				   data,
				   (guint32 *) &length);
	if (!ret) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_SAVE,
				     "failed to dump ICC file to memory");
		goto out;
	}

	/* actually write file */
	ret = g_file_replace_contents (file,
				       data,
				       length,
				       NULL,
				       FALSE,
				       G_FILE_CREATE_NONE,
				       NULL,
				       cancellable,
				       &error_local);
	if (!ret) {
		g_set_error (error,
			     CD_ICC_ERROR,
			     CD_ICC_ERROR_FAILED_TO_SAVE,
			     "failed to dump ICC file: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	g_free (data);
	return ret;
}

/**
 * cd_icc_load_file:
 * @icc: a #CdIcc instance.
 * @file: a #GFile
 * @flags: a set of #CdIccLoadFlags
 * @cancellable: A #GCancellable or %NULL
 * @error: A #GError or %NULL
 *
 * Loads an ICC profile from a local or remote file.
 *
 * Since: 0.1.32
 **/
gboolean
cd_icc_load_file (CdIcc *icc,
		  GFile *file,
		  CdIccLoadFlags flags,
		  GCancellable *cancellable,
		  GError **error)
{
	CdIccPrivate *priv = icc->priv;
	gboolean ret = FALSE;
	gchar *data = NULL;
	GError *error_local = NULL;
	GFileInfo *info = NULL;
	gsize length;

	g_return_val_if_fail (CD_IS_ICC (icc), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	/* load files */
	ret = g_file_load_contents (file, cancellable, &data, &length,
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
	ret = cd_icc_load_data (icc,
				(const guint8 *) data,
				length,
				flags,
				error);
	if (!ret)
		goto out;

	/* find out if the user could delete this profile */
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE,
				  G_FILE_QUERY_INFO_NONE,
				  cancellable,
				  &error_local);
	if (info == NULL) {
		g_set_error (error,
			     CD_ICC_ERROR,
			     CD_ICC_ERROR_FAILED_TO_OPEN,
			     "failed to query file: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
	priv->can_delete = g_file_info_get_attribute_boolean (info,
							      G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE);

	/* save filename for later */
	priv->filename = g_file_get_path (file);
out:
	if (info != NULL)
		g_object_unref (info);
	g_free (data);
	return ret;
}

/**
 * cd_icc_load_fd:
 * @icc: a #CdIcc instance.
 * @fd: a file descriptor
 * @flags: a set of #CdIccLoadFlags
 * @error: A #GError or %NULL
 *
 * Loads an ICC profile from an open file descriptor.
 *
 * Since: 0.1.32
 **/
gboolean
cd_icc_load_fd (CdIcc *icc,
		gint fd,
		CdIccLoadFlags flags,
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

	/* load cached data */
	cd_icc_load (icc, flags);
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
 * cd_icc_get_filename:
 * @icc: A valid #CdIcc
 *
 * Gets the filename of the ICC data, if one exists.
 *
 * Return value: A filename, or %NULL
 *
 * Since: 0.1.32
 **/
const gchar *
cd_icc_get_filename (CdIcc *icc)
{
	CdIccPrivate *priv = icc->priv;
	g_return_val_if_fail (CD_IS_ICC (icc), NULL);
	return priv->filename;
}

/**
 * cd_icc_get_version:
 * @icc: a #CdIcc instance.
 *
 * Gets the ICC profile version, typically 2.1 or 4.2
 *
 * Return value: A floating point version number, or 0.0 for unknown
 *
 * Since: 0.1.32
 **/
gdouble
cd_icc_get_version (CdIcc *icc)
{
	g_return_val_if_fail (CD_IS_ICC (icc), 0.0f);
	return icc->priv->version;
}

/**
 * cd_icc_get_kind:
 * @icc: a #CdIcc instance.
 *
 * Gets the profile kind.
 *
 * Return value: The kind, e.g. CD_PROFILE_KIND_INPUT
 *
 * Since: 0.1.32
 **/
CdProfileKind
cd_icc_get_kind (CdIcc *icc)
{
	g_return_val_if_fail (CD_IS_ICC (icc), CD_PROFILE_KIND_UNKNOWN);
	return icc->priv->kind;
}

/**
 * cd_icc_get_colorspace:
 * @icc: a #CdIcc instance.
 *
 * Gets the profile colorspace
 *
 * Return value: The profile colorspace, e.g. %CD_COLORSPACE_RGB
 *
 * Since: 0.1.32
 **/
CdColorspace
cd_icc_get_colorspace (CdIcc *icc)
{
	g_return_val_if_fail (CD_IS_ICC (icc), CD_COLORSPACE_UNKNOWN);
	return icc->priv->colorspace;
}

/**
 * cd_icc_get_metadata:
 * @icc: A valid #CdIcc
 *
 * Gets all the metadata from the ICC profile.
 *
 * Return value: (transfer container): The profile metadata
 *
 * Since: 0.1.32
 **/
GHashTable *
cd_icc_get_metadata (CdIcc *icc)
{
	return g_hash_table_ref (icc->priv->metadata);
}

/**
 * cd_icc_get_metadata_item:
 * @icc: A valid #CdIcc
 * @key: the dictionary key
 *
 * Gets an item of data from the ICC metadata store.
 *
 * Return value: The dictionary data, or %NULL if the key does not exist.
 *
 * Since: 0.1.32
 **/
const gchar *
cd_icc_get_metadata_item (CdIcc *icc, const gchar *key)
{
	return (const gchar *) g_hash_table_lookup (icc->priv->metadata, key);
}

/**
 * cd_icc_add_metadata:
 * @icc: A valid #CdIcc
 * @key: the metadata key
 * @value: the metadata value
 *
 * Sets an item of data to the profile metadata, overwriting it if
 * it already exists.
 *
 * Since: 0.1.32
 **/
void
cd_icc_add_metadata (CdIcc *icc, const gchar *key, const gchar *value)
{
	g_hash_table_insert (icc->priv->metadata,
			     g_strdup (key),
			     g_strdup (value));
}

/**
 * cd_icc_get_named_colors:
 * @icc: a #CdIcc instance.
 *
 * Gets any named colors in the profile
 *
 * Return value: (transfer container): An array of #CdColorSwatch or %NULL if no colors exist
 *
 * Since: 0.1.32
 **/
GPtrArray *
cd_icc_get_named_colors (CdIcc *icc)
{
	CdColorLab lab;
	CdColorSwatch *swatch;
	CdIccPrivate *priv = icc->priv;
	cmsNAMEDCOLORLIST *nc2;
	cmsUInt16Number pcs[3];
	gboolean ret;
	gchar name[cmsMAX_PATH];
	gchar prefix[33];
	gchar suffix[33];
	GPtrArray *array = NULL;
	GString *string;
	guint j;
	guint size;

	/* do any named colors exist? */
	nc2 = cmsReadTag (priv->lcms_profile, cmsSigNamedColor2Type);
	if (nc2 == NULL)
		goto out;

	/* get each NC */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_color_swatch_free);
	size = cmsNamedColorCount (nc2);
	for (j = 0; j < size; j++) {

		/* parse title */
		ret = cmsNamedColorInfo (nc2, j,
					 name,
					 prefix,
					 suffix,
					 (cmsUInt16Number *) &pcs,
					 NULL);
		if (!ret)
			continue;
		string = g_string_new ("");
		if (prefix[0] != '\0')
			g_string_append_printf (string, "%s ", prefix);
		g_string_append (string, name);
		if (suffix[0] != '\0')
			g_string_append_printf (string, " %s", suffix);

		/* check is valid */
		ret = g_utf8_validate (string->str, string->len, NULL);
		if (!ret)
			ret = cd_icc_fix_utf8_string (string);

		/* save color if valid */
		if (ret) {
			cmsLabEncoded2Float ((cmsCIELab *) &lab, pcs);
			swatch = cd_color_swatch_new ();
			cd_color_swatch_set_name (swatch, string->str);
			cd_color_swatch_set_value (swatch, (const CdColorLab *) &lab);
			g_ptr_array_add (array, swatch);
		}
		g_string_free (string, TRUE);
	}
out:
	return array;
}

/**
 * cd_icc_get_can_delete:
 * @icc: a #CdIcc instance.
 *
 * Finds out if the profile could be deleted.
 * This is only applicable for profiles loaded with cd_icc_load_file() as
 * obviously data and fd's cannot be sanely unlinked.
 *
 * Return value: %TRUE if g_file_delete() would likely work
 *
 * Since: 0.1.32
 **/
gboolean
cd_icc_get_can_delete (CdIcc *icc)
{
	g_return_val_if_fail (CD_IS_ICC (icc), FALSE);
	return icc->priv->can_delete;
}

/**
 * cd_icc_get_created:
 * @icc: A valid #CdIcc
 *
 * Gets the ICC creation date and time.
 *
 * Return value: A #GDateTime object, or %NULL for not set
 *
 * Since: 0.1.32
 **/
GDateTime *
cd_icc_get_created (CdIcc *icc)
{
	CdIccPrivate *priv = icc->priv;
	gboolean ret;
	GDateTime *created = NULL;
	struct tm created_tm;
	time_t created_t;

	g_return_val_if_fail (CD_IS_ICC (icc), NULL);


	/* get the profile creation time and date */
	ret = cmsGetHeaderCreationDateTime (priv->lcms_profile, &created_tm);
	if (!ret)
		goto out;

	/* convert to UNIX time */
	created_t = mktime (&created_tm);
	if (created_t == (time_t) -1)
		goto out;

	/* instantiate object */
	created = g_date_time_new_from_unix_utc (created_t);
out:
	return created;
}

/**
 * cd_icc_get_mluc_data:
 **/
static const gchar *
cd_icc_get_mluc_data (CdIcc *icc,
		      const gchar *locale,
		      CdIccMluc mluc,
		      cmsTagSignature *sigs,
		      GError **error)
{
	CdIccPrivate *priv = icc->priv;
	cmsMLU *mlu = NULL;
	const gchar *country_code = "\0\0\0";
	const gchar *language_code = "\0\0\0";
	const gchar *value;
	gchar *key = NULL;
	gchar text_buffer[128];
	gchar *tmp;
	gsize rc;
	guint32 text_size;
	guint i;
	wchar_t wtext[128];

	g_return_val_if_fail (CD_IS_ICC (icc), NULL);

	/* en_US is signified by missing codes */
	if (locale != NULL && g_str_has_prefix (locale, "en_US"))
		locale = NULL;

	/* does cache entry exist already? */
	value = g_hash_table_lookup (priv->mluc_data[mluc],
				     locale != NULL ? locale : "");
	if (value != NULL)
		goto out;

	/* convert the locale into something we can use as a key, in this case
	 * 'en_GB.UTF-8' -> 'en_GB'
	 * 'fr'          -> 'fr' */
	if (locale != NULL) {
		key = g_strdup (locale);
		g_strdelimit (key, ".(", '\0');

		/* decompose it into language and country codes */
		tmp = g_strstr_len (key, -1, "_");
		language_code = key;
		if (tmp != NULL) {
			country_code = tmp + 1;
			*tmp = '\0';
		}

		/* check the format is correct */
		if (strlen (language_code) != 2) {
			g_set_error (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_INVALID_LOCALE,
				     "invalid locale: %s", locale);
			goto out;
		}
		if (country_code != NULL &&
		    country_code[0] != '\0' &&
		    strlen (country_code) != 2) {
			g_set_error (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_INVALID_LOCALE,
				     "invalid locale: %s", locale);
			goto out;
		}
	}

	/* read each MLU entry in order of preference */
	for (i = 0; sigs[i] != 0; i++) {
		mlu = cmsReadTag (priv->lcms_profile, sigs[i]);
		if (mlu != NULL)
			break;
	}
	if (mlu == NULL) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_NO_DATA,
				     "cmsSigProfile*Tag mising");
		goto out;
	}
	text_size = cmsMLUgetWide (mlu,
				   language_code,
				   country_code,
				   wtext,
				   sizeof (wtext));
	if (text_size == 0)
		goto out;
	rc = wcstombs (text_buffer,
		       wtext,
		       sizeof (text_buffer));
	if (rc == (gsize) -1) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_NO_DATA,
				     "invalid UTF-8");
		goto out;
	}

	/* insert into locale cache */
	tmp = g_strdup (text_buffer);
	g_hash_table_insert (priv->mluc_data[mluc],
			     g_strdup (locale != NULL ? locale : ""),
			     tmp);
	value = tmp;
out:
	g_free (key);
	return value;
}

/**
 * cd_icc_get_description:
 * @icc: A valid #CdIcc
 * @locale: A locale, e.g. "en_GB.UTF-8" or %NULL for the profile default
 * @error: A #GError or %NULL
 *
 * Gets the profile description.
 * If the translated text is not available in the selected locale then the
 * default untranslated (en_US) text is returned.
 *
 * Return value: The text as a UTF-8 string, or %NULL of the locale is invalid
 *               or the tag does not exist.
 *
 * Since: 0.1.32
 **/
const gchar *
cd_icc_get_description (CdIcc *icc, const gchar *locale, GError **error)
{
	cmsTagSignature sigs[] = { 0x6473636d, /* 'dscm' */
				   cmsSigProfileDescriptionTag,
				   0 };
	return cd_icc_get_mluc_data (icc,
				     locale,
				     CD_MLUC_DESCRIPTION,
				     sigs,
				     error);
}

/**
 * cd_icc_get_copyright:
 * @icc: A valid #CdIcc
 * @locale: A locale, e.g. "en_GB.UTF-8" or %NULL for the profile default
 * @error: A #GError or %NULL
 *
 * Gets the profile copyright.
 * If the translated text is not available in the selected locale then the
 * default untranslated (en_US) text is returned.
 *
 * Return value: The text as a UTF-8 string, or %NULL of the locale is invalid
 *               or the tag does not exist.
 *
 * Since: 0.1.32
 **/
const gchar *
cd_icc_get_copyright (CdIcc *icc, const gchar *locale, GError **error)
{
	cmsTagSignature sigs[] = { cmsSigCopyrightTag, 0 };
	return cd_icc_get_mluc_data (icc,
				     locale,
				     CD_MLUC_COPYRIGHT,
				     sigs,
				     error);
}

/**
 * cd_icc_get_manufacturer:
 * @icc: A valid #CdIcc
 * @locale: A locale, e.g. "en_GB.UTF-8" or %NULL for the profile default
 * @error: A #GError or %NULL
 *
 * Gets the profile manufacturer.
 * If the translated text is not available in the selected locale then the
 * default untranslated (en_US) text is returned.
 *
 * Return value: The text as a UTF-8 string, or %NULL of the locale is invalid
 *               or the tag does not exist.
 *
 * Since: 0.1.32
 **/
const gchar *
cd_icc_get_manufacturer (CdIcc *icc, const gchar *locale, GError **error)
{
	cmsTagSignature sigs[] = { cmsSigDeviceMfgDescTag, 0 };
	return cd_icc_get_mluc_data (icc,
				     locale,
				     CD_MLUC_MANUFACTURER,
				     sigs,
				     error);
}

/**
 * cd_icc_get_model:
 * @icc: A valid #CdIcc
 * @locale: A locale, e.g. "en_GB.UTF-8" or %NULL for the profile default
 * @error: A #GError or %NULL
 *
 * Gets the profile model.
 * If the translated text is not available in the selected locale then the
 * default untranslated (en_US) text is returned.
 *
 * Return value: The text as a UTF-8 string, or %NULL of the locale is invalid
 *               or the tag does not exist.
 *
 * Since: 0.1.32
 **/
const gchar *
cd_icc_get_model (CdIcc *icc, const gchar *locale, GError **error)
{
	cmsTagSignature sigs[] = { cmsSigDeviceModelDescTag, 0 };
	return cd_icc_get_mluc_data (icc,
				     locale,
				     CD_MLUC_MODEL,
				     sigs,
				     error);
}

/**
 * cd_icc_set_description:
 * @icc: A valid #CdIcc
 * @locale: A locale, e.g. "en_GB.UTF-8" or %NULL for the profile default
 * @value: New string value
 *
 * Sets the profile description for a specific locale.
 *
 * Since: 0.1.32
 **/
void
cd_icc_set_description (CdIcc *icc, const gchar *locale, const gchar *value)
{
	CdIccPrivate *priv = icc->priv;
	g_hash_table_insert (priv->mluc_data[CD_MLUC_DESCRIPTION],
			     g_strdup (locale != NULL ? locale : ""),
			     g_strdup (value));
}

/**
 * cd_icc_set_copyright:
 * @icc: A valid #CdIcc
 * @locale: A locale, e.g. "en_GB.UTF-8" or %NULL for the profile default
 * @value: New string value
 *
 * Sets the profile _copyright for a specific locale.
 *
 * Since: 0.1.32
 **/
void
cd_icc_set_copyright (CdIcc *icc, const gchar *locale, const gchar *value)
{
	CdIccPrivate *priv = icc->priv;
	g_hash_table_insert (priv->mluc_data[CD_MLUC_COPYRIGHT],
			     g_strdup (locale != NULL ? locale : ""),
			     g_strdup (value));
}

/**
 * cd_icc_set_manufacturer:
 * @icc: A valid #CdIcc
 * @locale: A locale, e.g. "en_GB.UTF-8" or %NULL for the profile default
 * @value: New string value
 *
 * Sets the profile manufacturer for a specific locale.
 *
 * Since: 0.1.32
 **/
void
cd_icc_set_manufacturer (CdIcc *icc, const gchar *locale, const gchar *value)
{
	CdIccPrivate *priv = icc->priv;
	g_hash_table_insert (priv->mluc_data[CD_MLUC_MANUFACTURER],
			     g_strdup (locale != NULL ? locale : ""),
			     g_strdup (value));
}

/**
 * cd_icc_set_model:
 * @icc: A valid #CdIcc
 * @locale: A locale, e.g. "en_GB.UTF-8" or %NULL for the profile default
 * @value: New string value
 *
 * Sets the profile model for a specific locale.
 *
 * Since: 0.1.32
 **/
void
cd_icc_set_model (CdIcc *icc, const gchar *locale, const gchar *value)
{
	CdIccPrivate *priv = icc->priv;
	g_hash_table_insert (priv->mluc_data[CD_MLUC_MODEL],
			     g_strdup (locale != NULL ? locale : ""),
			     g_strdup (value));
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
	case PROP_FILENAME:
		g_value_set_string (value, priv->filename);
		break;
	case PROP_VERSION:
		g_value_set_double (value, priv->version);
		break;
	case PROP_KIND:
		g_value_set_uint (value, priv->kind);
		break;
	case PROP_COLORSPACE:
		g_value_set_uint (value, priv->colorspace);
		break;
	case PROP_CAN_DELETE:
		g_value_set_boolean (value, priv->can_delete);
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

	/**
	 * CdIcc:filename:
	 */
	pspec = g_param_spec_string ("filename", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_FILENAME, pspec);

	/**
	 * CdIcc:version:
	 */
	pspec = g_param_spec_double ("version", NULL, NULL,
				     0, G_MAXFLOAT, 0,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_VERSION, pspec);

	/**
	 * CdIcc:kind:
	 */
	pspec = g_param_spec_uint ("kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_KIND, pspec);

	/**
	 * CdIcc:colorspace:
	 */
	pspec = g_param_spec_uint ("colorspace", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_COLORSPACE, pspec);

	/**
	 * CdIcc:can-delete:
	 */
	pspec = g_param_spec_boolean ("can-delete", NULL, NULL,
				      FALSE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_CAN_DELETE, pspec);

	g_type_class_add_private (klass, sizeof (CdIccPrivate));
}

/**
 * cd_icc_init:
 */
static void
cd_icc_init (CdIcc *icc)
{
	guint i;

	icc->priv = CD_ICC_GET_PRIVATE (icc);
	icc->priv->kind = CD_PROFILE_KIND_UNKNOWN;
	icc->priv->colorspace = CD_COLORSPACE_UNKNOWN;
	icc->priv->metadata = g_hash_table_new_full (g_str_hash,
						     g_str_equal,
						     g_free,
						     g_free);
	for (i = 0; i < CD_MLUC_LAST; i++) {
		icc->priv->mluc_data[i] = g_hash_table_new_full (g_str_hash,
								 g_str_equal,
								 g_free,
								 g_free);
	}
}

/**
 * cd_icc_finalize:
 */
static void
cd_icc_finalize (GObject *object)
{
	CdIcc *icc = CD_ICC (object);
	CdIccPrivate *priv = icc->priv;
	guint i;

	g_free (priv->filename);
	g_hash_table_destroy (priv->metadata);
	for (i = 0; i < CD_MLUC_LAST; i++)
		g_hash_table_destroy (priv->mluc_data[i]);
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
