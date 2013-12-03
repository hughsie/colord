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
 * @short_description: An object to read and write a binary ICC profile
 */

#include "config.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <lcms2.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "cd-icc.h"

static void	cd_icc_class_init	(CdIccClass	*klass);
static void	cd_icc_init		(CdIcc		*icc);
static void	cd_icc_load_named_colors (CdIcc		*icc);
static void	cd_icc_finalize		(GObject	*object);

#define CD_ICC_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_ICC, CdIccPrivate))

typedef enum {
	CD_MLUC_DESCRIPTION,
	CD_MLUC_COPYRIGHT,
	CD_MLUC_MANUFACTURER,
	CD_MLUC_MODEL,
	CD_MLUC_LAST
} CdIccMluc;

#ifndef HAVE_LCMS_GET_HEADER_CREATOR
#define cmsSigProfileDescriptionMLTag	0x6473636d
#endif

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
	gchar			*checksum;
	gchar			*filename;
	gdouble			 version;
	GHashTable		*mluc_data[CD_MLUC_LAST]; /* key is 'en_GB' or '' for default */
	GHashTable		*metadata;
	guint32			 size;
	GPtrArray		*named_colors;
	guint			 temperature;
	CdColorXYZ		 white;
	CdColorXYZ		 red;
	CdColorXYZ		 green;
	CdColorXYZ		 blue;
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
	PROP_CHECKSUM,
	PROP_RED,
	PROP_GREEN,
	PROP_BLUE,
	PROP_WHITE,
	PROP_TEMPERATURE,
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
 * cd_icc_uint32_to_str:
 **/
static void
cd_icc_uint32_to_str (guint32 id, gchar *str)
{
	/* this is a hack */
	memcpy (str, &id, 4);
	str[4] = '\0';
}

/**
 * cd_icc_lcms2_error_cb:
 **/
static void
cd_icc_lcms2_error_cb (cmsContext context_id,
		       cmsUInt32Number code,
		       const gchar *text)
{
	g_warning ("lcms2(profile): Failed with error: %s [%i]", text, code);
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
	cmsTagSignature sig_link;
	cmsTagTypeSignature tag_type;
	gboolean ret;
	gchar *tag_wrfix;
	gchar tag_str[5] = "    ";
	GDateTime *created;
	GString *str;
	guint32 i;
	guint32 number_tags;
	guint32 tmp;
	guint64 header_flags;
	guint8 profile_id[4];

	g_return_val_if_fail (CD_IS_ICC (icc), NULL);

	/* setup error handler */
	cmsSetLogErrorHandler (cd_icc_lcms2_error_cb);

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

	/* header attributes */
	cmsGetHeaderAttributes (priv->lcms_profile, &header_flags);
	g_string_append (str, "  Dev. Attrbts\t= ");
	g_string_append (str, (header_flags & cmsTransparency) > 0 ?
				"transparency" : "reflective");
	g_string_append (str, ", ");
	g_string_append (str, (header_flags & cmsMatte) > 0 ?
				"matte" : "glossy");
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

#ifdef HAVE_LCMS_GET_HEADER_CREATOR
	/* creator */
	tmp = cmsGetHeaderCreator (priv->lcms_profile);
	cd_icc_uint32_to_str (GUINT32_FROM_BE (tmp), tag_str);
	g_string_append_printf (str, "  Creator\t= %s\n", tag_str);
#endif

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
		cd_icc_uint32_to_str (GUINT32_FROM_BE (sig), tag_str);

		/* print header */
		g_string_append_printf (str, "tag %02i:\n", i);
		g_string_append_printf (str, "  sig\t'%s' [0x%x]\n", tag_str, sig);

		/* is this linked to another data area? */
		sig_link = cmsTagLinkedTo (priv->lcms_profile, sig);
		if (sig_link != 0) {
			cd_icc_uint32_to_str (GUINT32_FROM_BE (sig_link), tag_str);
			g_string_append_printf (str, "  link\t'%s' [0x%x]\n", tag_str, sig_link);
			continue;
		}

		/* get the tag type
		 *
		 * LCMS2 has an interesting bug where calling:
		 *  cmsReadRawTag(hProfile, sig, buffer, sizeof(buffer))
		 * actually writes the wrole tag to buffer, which overflows if
		 * tag_size > buffer. To work around this allocate a hugely
		 * wasteful buffer of the whole tag size just to get the first
		 * four bytes.
		 *
		 * But hey, at least we don't crash anymore...
		 */
		tag_size = cmsReadRawTag (priv->lcms_profile, sig, NULL, 0);
		if (tag_size == 0 || tag_size > 16 * 1024 * 1024) {
			g_string_append_printf (str, "WARNING: Tag size impossible %i", tag_size);
			continue;
		}
		tag_wrfix = g_new0 (gchar, tag_size);
		tag_size = cmsReadRawTag (priv->lcms_profile, sig, tag_wrfix, 4);
		memcpy (&tmp, tag_wrfix, 4);
		g_free (tag_wrfix);

		cd_icc_uint32_to_str (tmp, tag_str);
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
#ifdef HAVE_LCMS_MLU_TRANSLATIONS_COUNT
			gchar country_code[3] = "\0\0\0";
			gchar language_code[3] = "\0\0\0";
			gsize rc;
			guint32 j;
			guint32 mlu_size;
			wchar_t wtext[128];
#endif

			g_string_append_printf (str, "Text:\n");
			mlu = cmsReadTag (priv->lcms_profile, sig);
			if (mlu == NULL) {
				g_string_append_printf (str, "  Info:\t\tMLU invalid!\n");
				break;
			}
#ifdef HAVE_LCMS_MLU_TRANSLATIONS_COUNT
			mlu_size = cmsMLUtranslationsCount (mlu);
			if (mlu_size == 0)
				g_string_append_printf (str, "  Info:\t\tMLU empty!\n");
			for (j = 0; j < mlu_size; j++) {
				ret = cmsMLUtranslationsCodes (mlu,
							       j,
							       language_code,
							       country_code);
				if (!ret)
					continue;
				text_size = cmsMLUgetWide (mlu,
							   language_code,
							   country_code,
							   wtext,
							   sizeof (wtext));
				if (text_size == 0)
					continue;
				rc = wcstombs (text_buffer,
					       wtext,
					       sizeof (text_buffer));
				if (rc == (gsize) -1) {
					g_string_append_printf (str, "  %s_%s:\tInvalid!\n",
								language_code[0] != '\0' ? language_code : "en",
								country_code[0] != '\0' ? country_code : "US");
					continue;
				}
				g_string_append_printf (str, "  %s_%s:\t%s [%i bytes]\n",
							language_code[0] != '\0' ? language_code : "**",
							country_code[0] != '\0' ? country_code : "**",
							text_buffer,
							text_size);
			}
#else
			text_size = cmsMLUgetASCII (mlu,
						    cmsNoLanguage,
						    cmsNoCountry,
						    text_buffer,
						    sizeof (text_buffer));
			if (text_size > 0) {
				g_string_append_printf (str, "  en_US:\t%s [%i bytes]\n",
							text_buffer, text_size);
			}
#endif
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
		case cmsSigVcgtType:
		{
			const cmsToneCurve **vcgt;
			g_string_append (str, "VideoCardGammaTable:\n");
			vcgt = cmsReadTag (priv->lcms_profile, sig);
			if (vcgt == NULL || vcgt[0] == NULL) {
				g_debug ("icc does not have any VCGT data");
				continue;
			}
			g_string_append_printf (str, "  channels\t = %i\n", 3);
#ifdef HAVE_LCMS_GET_TABLE_ENTRIES
			g_string_append_printf (str, "  entries\t = %i\n",
						cmsGetToneCurveEstimatedTableEntries (vcgt[0]));
#endif
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

/* map lcms profile class to colord type */
const struct {
	cmsProfileClassSignature	lcms;
	CdProfileKind			colord;
} map_profile_kind[] = {
	{ cmsSigInputClass,		CD_PROFILE_KIND_INPUT_DEVICE },
	{ cmsSigDisplayClass,		CD_PROFILE_KIND_DISPLAY_DEVICE },
	{ cmsSigOutputClass,		CD_PROFILE_KIND_OUTPUT_DEVICE },
	{ cmsSigLinkClass,		CD_PROFILE_KIND_DEVICELINK },
	{ cmsSigColorSpaceClass,	CD_PROFILE_KIND_COLORSPACE_CONVERSION },
	{ cmsSigAbstractClass,		CD_PROFILE_KIND_ABSTRACT },
	{ cmsSigNamedColorClass,	CD_PROFILE_KIND_NAMED_COLOR },
	{ 0,				CD_PROFILE_KIND_LAST }
};

/* map lcms colorspace to colord type */
const struct {
	cmsColorSpaceSignature		lcms;
	CdColorspace			colord;
} map_colorspace[] = {
	{ cmsSigXYZData,		CD_COLORSPACE_XYZ },
	{ cmsSigLabData,		CD_COLORSPACE_LAB },
	{ cmsSigLuvData,		CD_COLORSPACE_LUV },
	{ cmsSigYCbCrData,		CD_COLORSPACE_YCBCR },
	{ cmsSigYxyData,		CD_COLORSPACE_YXY },
	{ cmsSigRgbData,		CD_COLORSPACE_RGB },
	{ cmsSigGrayData,		CD_COLORSPACE_GRAY },
	{ cmsSigHsvData,		CD_COLORSPACE_HSV },
	{ cmsSigCmykData,		CD_COLORSPACE_CMYK },
	{ cmsSigCmyData,		CD_COLORSPACE_CMY },
	{ 0,				CD_COLORSPACE_LAST }
};

/**
 * cd_icc_get_precooked_md5:
 **/
static gchar *
cd_icc_get_precooked_md5 (cmsHPROFILE lcms_profile)
{
	cmsUInt8Number icc_id[16];
	gboolean md5_precooked = FALSE;
	gchar *md5 = NULL;
	guint i;

	/* check to see if we have a pre-cooked MD5 */
	cmsGetHeaderProfileID (lcms_profile, icc_id);
	for (i = 0; i < 16; i++) {
		if (icc_id[i] != 0) {
			md5_precooked = TRUE;
			break;
		}
	}
	if (md5_precooked == FALSE)
		goto out;

	/* convert to a hex string */
	md5 = g_new0 (gchar, 32 + 1);
	for (i = 0; i < 16; i++)
		g_snprintf (md5 + i * 2, 3, "%02x", icc_id[i]);
out:
	return md5;
}

/**
 * cd_icc_calc_whitepoint:
 **/
static gboolean
cd_icc_calc_whitepoint (CdIcc *icc, GError **error)
{
	CdIccPrivate *priv = icc->priv;
	cmsBool bpc[2] = { FALSE, FALSE };
	cmsCIExyY tmp;
	cmsCIEXYZ whitepoint;
	cmsFloat64Number adaption[2] = { 0, 0 };
	cmsHPROFILE profiles[2];
	cmsHTRANSFORM transform;
	cmsUInt32Number intents[2] = { INTENT_ABSOLUTE_COLORIMETRIC,
				       INTENT_ABSOLUTE_COLORIMETRIC };
	gboolean got_temperature;
	gboolean ret = TRUE;
	gdouble temp_float;
	guint8 data[3] = { 255, 255, 255 };

	/* do Lab to RGB transform to get primaries */
	profiles[0] = priv->lcms_profile;
	profiles[1] = cmsCreateXYZProfile ();
	transform = cmsCreateExtendedTransform (NULL,
						2,
						profiles,
						bpc,
						intents,
						adaption,
						NULL, 0, /* gamut ICC */
						TYPE_RGB_8,
						TYPE_XYZ_DBL,
						cmsFLAGS_NOOPTIMIZE);
	if (transform == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_PARSE,
				     "failed to setup RGB -> XYZ transform");
		goto out;
	}

	/* run white through the transform */
	cmsDoTransform (transform, data, &whitepoint, 1);
	cd_color_xyz_set (&priv->white,
			  whitepoint.X,
			  whitepoint.Y,
			  whitepoint.Z);

	/* get temperature rounded to nearest 100K */
	cmsXYZ2xyY (&tmp, &whitepoint);
	got_temperature = cmsTempFromWhitePoint (&temp_float, &tmp);
	if (got_temperature)
		priv->temperature = (((guint) temp_float) / 100) * 100;
out:
	if (profiles[1] != NULL)
		cmsCloseProfile (profiles[1]);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	return ret;
}

/**
 * cd_icc_load_primaries:
 **/
static gboolean
cd_icc_load_primaries (CdIcc *icc, GError **error)
{
	CdIccPrivate *priv = icc->priv;
	cmsCIEXYZ *cie_xyz;
	cmsHPROFILE xyz_profile = NULL;
	cmsHTRANSFORM transform = NULL;
	gboolean ret = TRUE;
	gdouble rgb_values[3];

	/* get white point */
	ret = cd_icc_calc_whitepoint (icc, error);
	if (!ret)
		goto out;

	/* get the illuminants from the primaries */
	cie_xyz = cmsReadTag (priv->lcms_profile, cmsSigRedMatrixColumnTag);
	if (cie_xyz != NULL) {
		cd_color_xyz_copy ((CdColorXYZ *) cie_xyz, &priv->red);
		cie_xyz = cmsReadTag (priv->lcms_profile, cmsSigGreenMatrixColumnTag);
		cd_color_xyz_copy ((CdColorXYZ *) cie_xyz, &priv->green);
		cie_xyz = cmsReadTag (priv->lcms_profile, cmsSigBlueMatrixColumnTag);
		cd_color_xyz_copy ((CdColorXYZ *) cie_xyz, &priv->blue);
		goto out;
	}

	/* get the illuminants by running it through the profile */
	xyz_profile = cmsCreateXYZProfile ();
	transform = cmsCreateTransform (priv->lcms_profile, TYPE_RGB_DBL,
					xyz_profile, TYPE_XYZ_DBL,
					INTENT_PERCEPTUAL, 0);
	if (transform == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_PARSE,
				     "failed to setup RGB -> XYZ transform");
		goto out;
	}

	/* red */
	rgb_values[0] = 1.0;
	rgb_values[1] = 0.0;
	rgb_values[2] = 0.0;
	cmsDoTransform (transform, rgb_values, &priv->red, 1);

	/* green */
	rgb_values[0] = 0.0;
	rgb_values[1] = 1.0;
	rgb_values[2] = 0.0;
	cmsDoTransform (transform, rgb_values, &priv->green, 1);

	/* blue */
	rgb_values[0] = 0.0;
	rgb_values[1] = 0.0;
	rgb_values[2] = 1.0;
	cmsDoTransform (transform, rgb_values, &priv->blue, 1);
out:
	if (transform != NULL)
		cmsDeleteTransform (transform);
	if (xyz_profile != NULL)
		cmsCloseProfile (xyz_profile);
	return ret;
}

/**
 * cd_icc_load:
 **/
static gboolean
cd_icc_load (CdIcc *icc, CdIccLoadFlags flags, GError **error)
{
	CdIccPrivate *priv = icc->priv;
	cmsColorSpaceSignature colorspace;
	cmsHANDLE dict;
	cmsProfileClassSignature profile_class;
	gboolean ret = TRUE;
	guint i;

	/* setup error handler */
	cmsSetLogErrorHandler (cd_icc_lcms2_error_cb);

	/* get version */
	priv->version = cmsGetProfileVersion (priv->lcms_profile);

	/* convert profile kind */
	profile_class = cmsGetDeviceClass (priv->lcms_profile);
	for (i = 0; map_profile_kind[i].colord != CD_PROFILE_KIND_LAST; i++) {
		if (map_profile_kind[i].lcms == profile_class) {
			priv->kind = map_profile_kind[i].colord;
			break;
		}
	}

	/* convert colorspace */
	colorspace = cmsGetColorSpace (priv->lcms_profile);
	for (i = 0; map_colorspace[i].colord != CD_COLORSPACE_LAST; i++) {
		if (map_colorspace[i].lcms == colorspace) {
			priv->colorspace = map_colorspace[i].colord;
			break;
		}
	}

	/* read optional metadata? */
	if ((flags & CD_ICC_LOAD_FLAGS_METADATA) > 0) {
		dict = cmsReadTag (priv->lcms_profile, cmsSigMetaTag);
		if (dict != NULL) {
			const cmsDICTentry *entry;
			gchar ascii_name[1024];
			gchar ascii_value[1024];
			for (entry = cmsDictGetEntryList (dict);
			     entry != NULL;
			     entry = cmsDictNextEntry (entry)) {
				wcstombs (ascii_name,
					  entry->Name,
					  sizeof (ascii_name));
				wcstombs (ascii_value,
					  entry->Value,
					  sizeof (ascii_value));
				g_hash_table_insert (priv->metadata,
						     g_strdup (ascii_name),
						     g_strdup (ascii_value));
			}
		}
	}

	/* get precooked profile ID if one exists */
	priv->checksum = cd_icc_get_precooked_md5 (priv->lcms_profile);

	/* read default translations */
	cd_icc_get_description (icc, NULL, NULL);
	cd_icc_get_copyright (icc, NULL, NULL);
	cd_icc_get_manufacturer (icc, NULL, NULL);
	cd_icc_get_model (icc, NULL, NULL);
	if ((flags & CD_ICC_LOAD_FLAGS_TRANSLATIONS) > 0) {
		/* FIXME: get the locale list from LCMS */
	}

	/* read named colors if the client cares */
	if ((flags & CD_ICC_LOAD_FLAGS_NAMED_COLORS) > 0)
		cd_icc_load_named_colors (icc);

	/* read primaries if the client cares */
	if ((flags & CD_ICC_LOAD_FLAGS_PRIMARIES) > 0 &&
	    priv->colorspace == CD_COLORSPACE_RGB) {
		ret = cd_icc_load_primaries (icc, error);
		if (!ret)
			goto out;
	}
out:
	return ret;
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
	ret = cd_icc_load (icc, flags, error);
	if (!ret)
		goto out;

	/* calculate the data MD5 if there was no embedded profile */
	if (priv->checksum == NULL &&
	    (flags & CD_ICC_LOAD_FLAGS_FALLBACK_MD5) > 0) {
		priv->checksum = g_compute_checksum_for_data (G_CHECKSUM_MD5,
							      (const guchar *) data,
							      data_len);
	}
out:
	return ret;
}

/**
 * utf8_to_wchar_t:
 **/
static wchar_t *
utf8_to_wchar_t (const char *src)
{
	const gchar *orig_locale;
	gssize len;
	gssize converted;
	wchar_t *buf = NULL;

	/* switch the locale to a known UTF-8 LC_CTYPE */
	orig_locale = setlocale (LC_CTYPE, NULL);
	setlocale (LC_CTYPE, "en_US.UTF-8");
	len = mbstowcs (NULL, src, 0);
	if (len < 0) {
		g_warning ("Invalid UTF-8 in string %s", src);
		goto out;
	}
	len += 1;
	buf = g_malloc (sizeof (wchar_t) * len);
	converted = mbstowcs (buf, src, len - 1);
	g_assert (converted != -1);
	buf[converted] = '\0';
out:
	setlocale (LC_CTYPE, orig_locale);
	return buf;
}

/**
 * cd_util_write_dict_entry:
 **/
static gboolean
cd_util_write_dict_entry (cmsHANDLE dict,
			  const gchar *key,
			  const gchar *value,
			  GError **error)
{
	gboolean ret = FALSE;
	wchar_t *mb_key = NULL;
	wchar_t *mb_value = NULL;

	mb_key = utf8_to_wchar_t (key);
	if (mb_key == NULL) {
		g_set_error (error,
			     CD_ICC_ERROR,
			     CD_ICC_ERROR_FAILED_TO_SAVE,
			     "Failed to write invalid ASCII key: '%s'",
			     key);
		goto out;
	}
	mb_value = utf8_to_wchar_t (value);
	if (mb_value == NULL) {
		g_set_error (error,
			     CD_ICC_ERROR,
			     CD_ICC_ERROR_FAILED_TO_SAVE,
			     "Failed to write invalid ASCII value: '%s'",
			     value);
		goto out;
	}
	ret = cmsDictAddEntry (dict, mb_key, mb_value, NULL, NULL);
	if (!ret) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_SAVE,
				     "Failed to write dict entry");
		goto out;
	}
out:
	g_free (mb_key);
	g_free (mb_value);
	return ret;
}

typedef struct {
	gchar		*language_code;	/* will always be xx\0 */
	gchar		*country_code;	/* will always be xx\0 */
	wchar_t		*wtext;
} CdMluObject;

/**
 * cd_util_mlu_object_free:
 **/
static void
cd_util_mlu_object_free (gpointer data)
{
	CdMluObject *obj = (CdMluObject *) data;
	g_free (obj->language_code);
	g_free (obj->country_code);
	g_free (obj->wtext);
	g_free (obj);
}

/**
 * cd_util_mlu_object_parse:
 **/
static CdMluObject *
cd_util_mlu_object_parse (const gchar *locale, const gchar *utf8_text)
{
	CdMluObject *obj = NULL;
	gchar *key = NULL;
	gchar **split = NULL;
	guint type;
	wchar_t *wtext;

	/* untranslated version */
	if (locale == NULL || locale[0] == '\0') {
		wtext = utf8_to_wchar_t (utf8_text);
		if (wtext == NULL)
			goto out;
		obj = g_new0 (CdMluObject, 1);
		obj->wtext = wtext;
		goto out;
	}

	/* ignore ##@latin */
	if (g_strstr_len (locale, -1, "@") != NULL)
		goto out;

	key = g_strdup (locale);
	g_strdelimit (key, ".", '\0');
	split = g_strsplit (key, "_", -1);
	if (strlen (split[0]) != 2)
		goto out;
	type = g_strv_length (split);
	if (type > 2)
		goto out;

	/* convert to wchars */
	wtext = utf8_to_wchar_t (utf8_text);
	if (wtext == NULL)
		goto out;

	/* lv */
	if (type == 1) {
		obj = g_new0 (CdMluObject, 1);
		obj->language_code = g_strdup (split[0]);
		obj->wtext = wtext;
		goto out;
	}

	/* en_GB */
	if (strlen (split[1]) != 2)
		goto out;
	obj = g_new0 (CdMluObject, 1);
	obj->language_code = g_strdup (split[0]);
	obj->country_code = g_strdup (split[1]);
	obj->wtext = wtext;
out:
	g_free (key);
	g_strfreev (split);
	return obj;
}

/**
 * cd_util_write_tag_ascii:
 **/
static gboolean
cd_util_write_tag_ascii (CdIcc *icc,
			 cmsTagSignature sig,
			 GHashTable *hash,
			 GError **error)
{
	CdIccPrivate *priv = icc->priv;
	cmsMLU *mlu = NULL;
	const gchar *value;
	gboolean ret = TRUE;

	/* get default value */
	value = g_hash_table_lookup (hash, "");
	if (value == NULL) {
		cmsWriteTag (priv->lcms_profile, sig, NULL);
		goto out;
	}

	/* set value */
	mlu = cmsMLUalloc (NULL, 1);
	ret = cmsMLUsetASCII (mlu, "en", "US", value);
	if (!ret) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_SAVE,
				     "cannot write MLU text");
		goto out;
	}

	/* write tag */
	ret = cmsWriteTag (priv->lcms_profile, sig, mlu);
	if (!ret) {
		g_set_error (error,
			     CD_ICC_ERROR,
			     CD_ICC_ERROR_FAILED_TO_SAVE,
			     "cannot write tag: 0x%x",
			     sig);
		goto out;
	}
out:
	if (mlu != NULL)
		cmsMLUfree (mlu);
	return ret;
}

/**
 * cd_util_sort_mlu_array_cb:
 **/
static gint
cd_util_sort_mlu_array_cb (gconstpointer a, gconstpointer b)
{
	CdMluObject *sa = *((CdMluObject **) a);
	CdMluObject *sb = *((CdMluObject **) b);
	return g_strcmp0 (sa->language_code, sb->language_code);
}

/**
 * cd_util_write_tag_localized:
 **/
static gboolean
cd_util_write_tag_localized (CdIcc *icc,
			     cmsTagSignature sig,
			     GHashTable *hash,
			     GError **error)
{
	CdIccPrivate *priv = icc->priv;
	CdMluObject *obj;
	cmsMLU *mlu = NULL;
	const gchar *locale;
	const gchar *value;
	gboolean ret = TRUE;
	GList *keys;
	GList *l;
	GPtrArray *array;
	guint i;

	/* convert all the hash entries into CdMluObject's */
	keys = g_hash_table_get_keys (hash);
	array = g_ptr_array_new_with_free_func (cd_util_mlu_object_free);
	for (l = keys; l != NULL; l = l->next) {
		locale = l->data;
		value = g_hash_table_lookup (hash, locale);
		if (value == NULL)
			continue;
		obj = cd_util_mlu_object_parse (locale, value);
		if (obj == NULL) {
			g_warning ("failed to parse localized text: %s[%s]",
				   value, locale);
			continue;
		}
		g_ptr_array_add (array, obj);
	}

	/* delete tag if there is no data */
	if (array->len == 0) {
		cmsWriteTag (priv->lcms_profile, sig, NULL);
		goto out;
	}

	/* sort the data so we always write the default first */
	g_ptr_array_sort (array, cd_util_sort_mlu_array_cb);

	/* create MLU object to hold all the translations */
	mlu = cmsMLUalloc (NULL, array->len);
	for (i = 0; i < array->len; i++) {
		obj = g_ptr_array_index (array, i);
		if (obj->language_code == NULL &&
		    obj->country_code == NULL) {
			/* the default translation is encoded as en_US rather
			 * than NoLanguage_NoCountry as the latter means
			 * 'the first entry' when reading */
			ret = cmsMLUsetWide (mlu, "en", "US", obj->wtext);
		} else {
			ret = cmsMLUsetWide (mlu,
					     obj->language_code != NULL ? obj->language_code : cmsNoLanguage,
					     obj->country_code != NULL ? obj->country_code : cmsNoCountry,
					     obj->wtext);
		}
		if (!ret) {
			g_set_error_literal (error,
					     CD_ICC_ERROR,
					     CD_ICC_ERROR_FAILED_TO_SAVE,
					     "cannot write MLU text");
			goto out;
		}
	}

	/* write tag */
	ret = cmsWriteTag (priv->lcms_profile, sig, mlu);
	if (!ret) {
		g_set_error (error,
			     CD_ICC_ERROR,
			     CD_ICC_ERROR_FAILED_TO_SAVE,
			     "cannot write tag: 0x%x",
			     sig);
		goto out;
	}
out:
	g_list_free (keys);
	if (mlu != NULL)
		cmsMLUfree (mlu);
	return ret;
}

/**
 * cd_icc_save_file_mkdir_parents:
 **/
static gboolean
cd_icc_save_file_mkdir_parents (GFile *file, GError **error)
{
	gboolean ret = FALSE;
	GFile *parent_dir = NULL;

	/* get parent directory */
	parent_dir = g_file_get_parent (file);
	if (parent_dir == NULL) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_CREATE,
				     "could not get parent dir");
		goto out;
	}

	/* ensure desination does not already exist */
	ret = g_file_query_exists (parent_dir, NULL);
	if (ret)
		goto out;
	ret = g_file_make_directory_with_parents (parent_dir, NULL, error);
	if (!ret)
		goto out;
out:
	if (parent_dir != NULL)
		g_object_unref (parent_dir);
	return ret;
}

/**
 * cd_icc_check_error_cb:
 **/
static void
cd_icc_check_error_cb (cmsContext context_id,
		       cmsUInt32Number code,
		       const char *message)
{
	gboolean *ret = (gboolean *) context_id;
	*ret = FALSE;
}

/**
 * cd_icc_check_lcms2_MemoryWrite:
 *
 * - Create a sRGB profile with some metadata
 * - Serialize it to a memory blob
 * - Load the resulting memory blob
 *
 * If there are any errors thrown, we're being compiled against a LCMS2 with
 * the bad MemoryWrite() implementation.
 **/
static gboolean
cd_icc_check_lcms2_MemoryWrite (void)
{
	cmsHANDLE dict;
	cmsHPROFILE p;
	cmsUInt32Number size;
	gboolean ret = TRUE;
	gchar *data;

	/* setup temporary log handler */
	cmsSetLogErrorHandler (cd_icc_check_error_cb);

	/* create test data */
	p = cmsCreate_sRGBProfileTHR (&ret);
	dict = cmsDictAlloc (NULL);
	cmsDictAddEntry (dict, L"1", L"2", NULL, NULL);
	cmsWriteTag (p, cmsSigMetaTag, dict);
	cmsSaveProfileToMem (p, NULL, &size);
	data = g_malloc (size);
	cmsSaveProfileToMem (p, data, &size);
	cmsCloseProfile (p);
	cmsDictFree (dict);

	/* open file */
	p = cmsOpenProfileFromMemTHR (&ret, data, size);
	dict = cmsReadTag (p, cmsSigMetaTag);
	g_assert (dict != (gpointer) 0x01); /* appease GCC */
	cmsCloseProfile (p);
	g_free (data);
	cmsSetLogErrorHandler (NULL);
	return ret;
}

/**
 * cd_icc_serialize_profile_fallback:
 **/
static GBytes *
cd_icc_serialize_profile_fallback (CdIcc *icc, GError **error)
{
	CdIccPrivate *priv = icc->priv;
	gboolean ret;
	GBytes *data = NULL;
	gchar *data_tmp = NULL;
	gchar *temp_file = NULL;
	GError *error_local = NULL;
	gint fd;
	gsize length = 0;

	/* get unique temp file */
	fd = g_file_open_tmp ("colord-XXXXXX.icc", &temp_file, &error_local);
	if (fd < 0) {
		g_set_error (error,
			     CD_ICC_ERROR,
			     CD_ICC_ERROR_FAILED_TO_SAVE,
			     "failed to open temp file: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* dump to a file, avoiding the problematic call to MemoryWrite() */
	ret = cmsSaveProfileToFile (priv->lcms_profile, temp_file);
	if (!ret) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_SAVE,
				     "failed to dump ICC file to temp file");
		goto out;
	}
	ret = g_file_get_contents (temp_file, &data_tmp, &length, NULL);
	if (!ret) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_SAVE,
				     "failed to load temp file");
		goto out;
	}
	/* success */
	data = g_bytes_new (data_tmp, length);
out:
	if (fd >= 0)
		close (fd);
	if (temp_file != NULL)
		g_unlink (temp_file);
	g_free (temp_file);
	g_free (data_tmp);
	return data;
}

/**
 * cd_icc_serialize_profile:
 **/
static GBytes *
cd_icc_serialize_profile (CdIcc *icc, GError **error)
{
	CdIccPrivate *priv = icc->priv;
	cmsUInt32Number length = 0;
	gboolean ret;
	GBytes *data = NULL;
	gchar *data_tmp = NULL;

	/* get size of profile */
	ret = cmsSaveProfileToMem (priv->lcms_profile,
				   NULL, &length);
	if (!ret) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_SAVE,
				     "failed to dump ICC file");
		goto out;
	}

	/* sanity check to 16Mb */
	if (length == 0 || length > 16 * 1024 * 1024) {
		g_set_error (error,
			     CD_ICC_ERROR,
			     CD_ICC_ERROR_FAILED_TO_SAVE,
			     "failed to save ICC file, requested %u "
			     "bytes and limit is 16Mb",
			     length);
		goto out;
	}

	/* allocate and get profile data */
	data_tmp = g_new0 (gchar, length);
	ret = cmsSaveProfileToMem (priv->lcms_profile,
				   data_tmp, &length);
	if (!ret) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_SAVE,
				     "failed to dump ICC file to memory");
		goto out;
	}

	/* success */
	data = g_bytes_new (data_tmp, length);
out:
	g_free (data_tmp);
	return data;
}

/**
 * cd_icc_save_data:
 * @icc: a #CdIcc instance.
 * @flags: a set of #CdIccSaveFlags
 * @error: A #GError or %NULL
 *
 * Saves an ICC profile to an allocated memory location.
 *
 * Return vale: A #GBytes structure, or %NULL for error
 *
 * Since: 1.0.2
 **/
GBytes *
cd_icc_save_data (CdIcc *icc,
		  CdIccSaveFlags flags,
		  GError **error)
{
	CdIccPrivate *priv = icc->priv;
	cmsHANDLE dict = NULL;
	const gchar *key;
	const gchar *value;
	gboolean ret = FALSE;
	GBytes *data = NULL;
	GList *l;
	GList *md_keys = NULL;
	guint i;

	g_return_val_if_fail (CD_IS_ICC (icc), FALSE);

	/* setup error handler */
	cmsSetLogErrorHandler (cd_icc_lcms2_error_cb);

	/* convert profile kind */
	for (i = 0; map_profile_kind[i].colord != CD_PROFILE_KIND_LAST; i++) {
		if (map_profile_kind[i].colord == priv->kind) {
			cmsSetDeviceClass (priv->lcms_profile,
					   map_profile_kind[i].lcms);
			break;
		}
	}

	/* convert colorspace */
	for (i = 0; map_colorspace[i].colord != CD_COLORSPACE_LAST; i++) {
		if (map_colorspace[i].colord == priv->colorspace) {
			cmsSetColorSpace (priv->lcms_profile,
					  map_colorspace[i].lcms);
			break;
		}
	}

	/* set version */
	if (priv->version > 0.0)
		cmsSetProfileVersion (priv->lcms_profile, priv->version);

	/* save metadata */
	if (g_hash_table_size (priv->metadata) != 0) {
		dict = cmsDictAlloc (NULL);
		md_keys = g_hash_table_get_keys (priv->metadata);
		if (md_keys != NULL) {
			for (l = md_keys; l != NULL; l = l->next) {
				key = l->data;
				value = g_hash_table_lookup (priv->metadata, key);
				ret = cd_util_write_dict_entry (dict, key,
								value, error);
				if (!ret)
					goto out;
			}
		}
		ret = cmsWriteTag (priv->lcms_profile, cmsSigMetaTag, dict);
		if (!ret) {
			g_set_error_literal (error,
					     CD_ICC_ERROR,
					     CD_ICC_ERROR_FAILED_TO_SAVE,
					     "cannot write metadata");
			goto out;
		}
	} else {
		cmsWriteTag (priv->lcms_profile, cmsSigMetaTag, NULL);
	}

	/* save translations */
	if (priv->version < 4.0) {
		/* v2 profiles cannot have a mluc type for cmsSigProfileDescriptionTag
		 * so use the non-standard Apple extension cmsSigProfileDescriptionTagML
		 * and only write a en_US version for the description */
		ret = cd_util_write_tag_ascii (icc,
					       cmsSigProfileDescriptionTag,
					       priv->mluc_data[CD_MLUC_DESCRIPTION],
					       error);
		if (!ret)
			goto out;
#ifdef HAVE_LCMS_GET_HEADER_CREATOR
		ret = cd_util_write_tag_localized (icc,
						   cmsSigProfileDescriptionMLTag,
						   priv->mluc_data[CD_MLUC_DESCRIPTION],
						   error);
		if (!ret)
			goto out;
#endif
		ret = cd_util_write_tag_ascii (icc,
					       cmsSigCopyrightTag,
					       priv->mluc_data[CD_MLUC_COPYRIGHT],
					       error);
		if (!ret)
			goto out;
		ret = cd_util_write_tag_ascii (icc,
					       cmsSigDeviceMfgDescTag,
					       priv->mluc_data[CD_MLUC_MANUFACTURER],
					       error);
		if (!ret)
			goto out;
		ret = cd_util_write_tag_ascii (icc,
						   cmsSigDeviceModelDescTag,
						   priv->mluc_data[CD_MLUC_MODEL],
						   error);
		if (!ret)
			goto out;
	} else {
		/* v4 profiles can use mluc types for all fields */
		ret = cd_util_write_tag_localized (icc,
						   cmsSigProfileDescriptionTag,
						   priv->mluc_data[CD_MLUC_DESCRIPTION],
						   error);
		if (!ret)
			goto out;
		ret = cd_util_write_tag_localized (icc,
						   cmsSigCopyrightTag,
						   priv->mluc_data[CD_MLUC_COPYRIGHT],
						   error);
		if (!ret)
			goto out;
		ret = cd_util_write_tag_localized (icc,
						   cmsSigDeviceMfgDescTag,
						   priv->mluc_data[CD_MLUC_MANUFACTURER],
						   error);
		if (!ret)
			goto out;
		ret = cd_util_write_tag_localized (icc,
						   cmsSigDeviceModelDescTag,
						   priv->mluc_data[CD_MLUC_MODEL],
						   error);
		if (!ret)
			goto out;
	}

	/* write profile id */
	ret = cmsMD5computeID (priv->lcms_profile);
	if (!ret) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_SAVE,
				     "failed to compute profile id");
		goto out;
	}

	/* LCMS2 doesn't serialize some tags properly when using the function
	 * cmsSaveProfileToMem() twice, so until lcms 2.6 is released we have to
	 * use a fallback version which uses a temporary file.
	 * See https://github.com/mm2/Little-CMS/commit/1d2643cb8153c48dcfdee3d5cda43a38f7e719e2
	 * for the fix. */
	if (cd_icc_check_lcms2_MemoryWrite ()) {
		data = cd_icc_serialize_profile (icc, error);
	} else {
		g_debug ("Using file serialization due to bad MemoryWrite.");
		data = cd_icc_serialize_profile_fallback (icc, error);
	}
out:
	g_list_free (md_keys);
	if (dict != NULL)
		cmsDictFree (dict);
	return data;
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
	gboolean ret;
	GBytes *data = NULL;
	GError *error_local = NULL;

	g_return_val_if_fail (CD_IS_ICC (icc), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	/* get data */
	data = cd_icc_save_data (icc, flags, error);
	if (data == NULL) {
		ret = FALSE;
		goto out;
	}

	/* ensure parent directories exist */
	ret = cd_icc_save_file_mkdir_parents (file, error);
	if (!ret)
		goto out;

	/* actually write file */
	ret = g_file_replace_contents (file,
				       g_bytes_get_data (data, NULL),
				       g_bytes_get_size (data),
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
			     "failed to save ICC file: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	g_bytes_unref (data);
	return ret;
}

/**
 * cd_icc_set_filename:
 * @icc: a #CdIcc instance.
 * @filename: a filename, or %NULL
 *
 * Sets the filename, which may be required if the ICC profile has been loaded
 * using cd_icc_load_fd() from a disk cache.
 *
 * Since: 1.1.1
 **/
void
cd_icc_set_filename (CdIcc *icc, const gchar *filename)
{
	g_free (icc->priv->filename);
	icc->priv->filename = g_strdup (filename);
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
	ret = cd_icc_load (icc, flags, error);
	if (!ret)
		goto out;
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
 * cd_icc_load_handle:
 * @icc: a #CdIcc instance.
 * @handle: a cmsHPROFILE instance
 * @flags: a set of #CdIccLoadFlags
 * @error: A #GError or %NULL
 *
 * Set the internal cmsHPROFILE instance. This may be required if you create
 * the profile using cmsCreateRGBProfile() and then want to use the
 * functionality in #CdIcc.
 *
 * Do not call cmsCloseProfile() on @handle in the caller, this will be done
 * when the @icc object is finalized. Treat the profile like it's been adopted
 * by this module.
 *
 * Additionally, this function cannot be called more than once, and also can't
 * be called if cd_icc_load_file() has previously been used on the @icc object.
 *
 * Since: 0.1.33
 **/
gboolean
cd_icc_load_handle (CdIcc *icc,
		    gpointer handle,
		    CdIccLoadFlags flags,
		    GError **error)
{
	CdIccPrivate *priv = icc->priv;
	gboolean ret;

	g_return_val_if_fail (CD_IS_ICC (icc), FALSE);
	g_return_val_if_fail (handle != NULL, FALSE);
	g_return_val_if_fail (priv->lcms_profile == NULL, FALSE);

	/* load profile */
	priv->lcms_profile = handle;
	ret = cd_icc_load (icc, flags, error);
	if (!ret)
		goto out;
out:
	return ret;
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
 * cd_icc_set_version:
 * @icc: a #CdIcc instance.
 * @version: the profile version, e.g. 2.1 or 4.0
 *
 * Sets the profile version.
 *
 * Since: 0.1.32
 **/
void
cd_icc_set_version (CdIcc *icc, gdouble version)
{
	g_return_if_fail (CD_IS_ICC (icc));
	icc->priv->version = version;
	g_object_notify (G_OBJECT (icc), "version");
}

/**
 * cd_icc_get_kind:
 * @icc: a #CdIcc instance.
 *
 * Gets the profile kind.
 *
 * Return value: The kind, e.g. %CD_PROFILE_KIND_INPUT
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
 * cd_icc_set_kind:
 * @icc: a #CdIcc instance.
 * @kind: the profile kind, e.g. %CD_PROFILE_KIND_DISPLAY_DEVICE
 *
 * Sets the profile kind.
 *
 * Since: 0.1.32
 **/
void
cd_icc_set_kind (CdIcc *icc, CdProfileKind kind)
{
	g_return_if_fail (CD_IS_ICC (icc));
	icc->priv->kind = kind;
	g_object_notify (G_OBJECT (icc), "kind");
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
 * cd_icc_set_colorspace:
 * @icc: a #CdIcc instance.
 * @colorspace: the profile colorspace, e.g. %CD_COLORSPACE_RGB
 *
 * Sets the colorspace kind.
 *
 * Since: 0.1.32
 **/
void
cd_icc_set_colorspace (CdIcc *icc, CdColorspace colorspace)
{
	g_return_if_fail (CD_IS_ICC (icc));
	icc->priv->colorspace = colorspace;
	g_object_notify (G_OBJECT (icc), "colorspace");
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
	g_return_val_if_fail (CD_IS_ICC (icc), NULL);
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
	g_return_val_if_fail (CD_IS_ICC (icc), NULL);
	g_return_val_if_fail (key != NULL, NULL);
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
	g_return_if_fail (CD_IS_ICC (icc));
	g_return_if_fail (key != NULL);
	g_return_if_fail (value != NULL);
	g_hash_table_insert (icc->priv->metadata,
			     g_strdup (key),
			     g_strdup (value));
}

/**
 * cd_icc_remove_metadata:
 * @icc: A valid #CdIcc
 * @key: the metadata key
 *
 * Removes an item of metadata.
 *
 * Since: 0.1.32
 **/
void
cd_icc_remove_metadata (CdIcc *icc, const gchar *key)
{
	g_return_if_fail (CD_IS_ICC (icc));
	g_return_if_fail (key != NULL);
	g_hash_table_remove (icc->priv->metadata, key);
}

/**
 * cd_icc_load_named_colors:
 **/
static void
cd_icc_load_named_colors (CdIcc *icc)
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
	GString *string;
	guint j;
	guint size;

	/* do any named colors exist? */
	nc2 = cmsReadTag (priv->lcms_profile, cmsSigNamedColor2Type);
	if (nc2 == NULL)
		goto out;

	/* get each NC */
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
			g_ptr_array_add (icc->priv->named_colors, swatch);
		}
		g_string_free (string, TRUE);
	}
out:
	return;
}

/**
 * cd_icc_get_named_colors:
 * @icc: a #CdIcc instance.
 *
 * Gets any named colors in the profile.
 * This function will only return results if the profile was loaded with the
 * %CD_ICC_LOAD_FLAGS_NAMED_COLORS flag.
 *
 * Return value: (transfer container) (element-type CdColorSwatch): An array of color swatches
 *
 * Since: 0.1.32
 **/
GPtrArray *
cd_icc_get_named_colors (CdIcc *icc)
{
	g_return_val_if_fail (CD_IS_ICC (icc), NULL);
	return g_ptr_array_ref (icc->priv->named_colors);
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

	created_tm.tm_isdst = -1;

	/* convert to UNIX time */
	created_t = mktime (&created_tm);
	if (created_t == (time_t) -1)
		goto out;

	/* instantiate object */
	created = g_date_time_new_from_unix_local (created_t);
out:
	return created;
}

/**
 * cd_icc_get_checksum:
 * @icc: A valid #CdIcc
 *
 * Gets the profile checksum if one exists.
 * This will either be the embedded profile ID, or the file checksum if
 * the #CdIcc object was loaded using cd_icc_load_data() or cd_icc_load_file()
 * and the %CD_ICC_LOAD_FLAGS_FALLBACK_MD5 flag is used.
 *
 * Return value: An embedded MD5 checksum, or %NULL for not set
 *
 * Since: 0.1.32
 **/
const gchar *
cd_icc_get_checksum (CdIcc *icc)
{
	g_return_val_if_fail (CD_IS_ICC (icc), NULL);
	return icc->priv->checksum;
}

/**
 * cd_icc_get_locale_key:
 **/
static gchar *
cd_icc_get_locale_key (const gchar *locale)
{
	gchar *locale_key;

	/* en_US is the default locale in an ICC profile */
	if (locale == NULL || g_str_has_prefix (locale, "en_US")) {
		locale_key = g_strdup ("");
		goto out;
	}
	locale_key = g_strdup (locale);
	g_strdelimit (locale_key, ".(", '\0');
out:
	return locale_key;
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
	gchar *locale_key = NULL;
	gchar *text_buffer = NULL;
	gchar *tmp;
	gsize rc;
	guint32 text_size;
	guint i;
	wchar_t *wtext = NULL;

	g_return_val_if_fail (CD_IS_ICC (icc), NULL);

	/* setup error handler */
	cmsSetLogErrorHandler (cd_icc_lcms2_error_cb);

	/* does cache entry exist already? */
	locale_key = cd_icc_get_locale_key (locale);
	value = g_hash_table_lookup (priv->mluc_data[mluc], locale_key);
	if (value != NULL)
		goto out;

	/* convert the locale into something we can use as a key, in this case
	 * 'en_GB.UTF-8' -> 'en_GB'
	 * 'fr'          -> 'fr' */
	if (locale_key[0] != '\0') {

		/* decompose it into language and country codes */
		tmp = g_strstr_len (locale_key, -1, "_");
		language_code = locale_key;
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
	} else {
		/* lcms maps this to 'default' */
		language_code = "en";
		country_code = "US";
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

	/* get required size for wide chars */
	text_size = cmsMLUgetWide (mlu,
				   language_code,
				   country_code,
				   NULL,
				   0);
	if (text_size == 0)
		goto out;

	/* load wide chars */
	wtext = g_new (wchar_t, text_size);
	text_size = cmsMLUgetWide (mlu,
				   language_code,
				   country_code,
				   wtext,
				   text_size);
	if (text_size == 0)
		goto out;

	/* get required size for UTF-8 */
	rc = wcstombs (NULL, wtext, 0);
	if (rc == (gsize) -1) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_NO_DATA,
				     "invalid UTF-8");
		goto out;
	}

	/* load UTF-8 */
	text_buffer = g_new0 (gchar, rc + 1);
	wcstombs (text_buffer, wtext, rc);

	/* insert into locale cache */
	tmp = g_strdup (text_buffer);
	g_hash_table_insert (priv->mluc_data[mluc],
			     g_strdup (locale_key),
			     tmp);
	value = tmp;
out:
	g_free (locale_key);
	g_free (text_buffer);
	g_free (wtext);
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
	cmsTagSignature sigs[] = { cmsSigProfileDescriptionMLTag,
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
			     cd_icc_get_locale_key (locale),
			     g_strdup (value));
}

/**
 * cd_icc_set_description_items:
 * @icc: A valid #CdIcc
 * @values: New translated values, with the key being the locale.
 *
 * Sets the profile descriptions for specific locales.
 *
 * Since: 0.1.32
 **/
void
cd_icc_set_description_items (CdIcc *icc, GHashTable *values)
{
	const gchar *key;
	const gchar *value;
	GList *keys;
	GList *l;

	g_return_if_fail (CD_IS_ICC (icc));

	/* add each translation */
	keys = g_hash_table_get_keys (values);
	for (l = keys; l != NULL; l = l->next) {
		key = l->data;
		value = g_hash_table_lookup (values, key);
		cd_icc_set_description (icc, key, value);
	}
	g_list_free (keys);
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
			     cd_icc_get_locale_key (locale),
			     g_strdup (value));
}

/**
 * cd_icc_set_copyright_items:
 * @icc: A valid #CdIcc
 * @values: New translated values, with the key being the locale.
 *
 * Sets the profile copyrights for specific locales.
 *
 * Since: 0.1.32
 **/
void
cd_icc_set_copyright_items (CdIcc *icc, GHashTable *values)
{
	const gchar *key;
	const gchar *value;
	GList *keys;
	GList *l;

	g_return_if_fail (CD_IS_ICC (icc));

	/* add each translation */
	keys = g_hash_table_get_keys (values);
	for (l = keys; l != NULL; l = l->next) {
		key = l->data;
		value = g_hash_table_lookup (values, key);
		cd_icc_set_copyright (icc, key, value);
	}
	g_list_free (keys);
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
			     cd_icc_get_locale_key (locale),
			     g_strdup (value));
}

/**
 * cd_icc_set_manufacturer_items:
 * @icc: A valid #CdIcc
 * @values: New translated values, with the key being the locale.
 *
 * Sets the profile manufacturers for specific locales.
 *
 * Since: 0.1.32
 **/
void
cd_icc_set_manufacturer_items (CdIcc *icc, GHashTable *values)
{
	const gchar *key;
	const gchar *value;
	GList *keys;
	GList *l;

	g_return_if_fail (CD_IS_ICC (icc));

	/* add each translation */
	keys = g_hash_table_get_keys (values);
	for (l = keys; l != NULL; l = l->next) {
		key = l->data;
		value = g_hash_table_lookup (values, key);
		cd_icc_set_manufacturer (icc, key, value);
	}
	g_list_free (keys);
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
			     cd_icc_get_locale_key (locale),
			     g_strdup (value));
}

/**
 * cd_icc_set_model_items:
 * @icc: A valid #CdIcc
 * @values: New translated values, with the key being the locale.
 *
 * Sets the profile models for specific locales.
 *
 * Since: 0.1.32
 **/
void
cd_icc_set_model_items (CdIcc *icc, GHashTable *values)
{
	const gchar *key;
	const gchar *value;
	GList *keys;
	GList *l;

	g_return_if_fail (CD_IS_ICC (icc));

	/* add each translation */
	keys = g_hash_table_get_keys (values);
	for (l = keys; l != NULL; l = l->next) {
		key = l->data;
		value = g_hash_table_lookup (values, key);
		cd_icc_set_model (icc, key, value);
	}
	g_list_free (keys);
}

/**
 * cd_icc_get_temperature:
 * @icc: A valid #CdIcc
 *
 * Gets the ICC color temperature, rounded to the nearest 100K.
 * This function will only return results if the profile was loaded with the
 * %CD_ICC_LOAD_FLAGS_PRIMARIES flag.
 *
 * Return value: The color temperature in Kelvin, or 0 for error.
 *
 * Since: 0.1.32
 **/
guint
cd_icc_get_temperature (CdIcc *icc)
{
	g_return_val_if_fail (CD_IS_ICC (icc), 0);
	return icc->priv->temperature;
}

/**
 * cd_icc_get_red:
 * @icc: a valid #CdIcc instance
 *
 * Gets the profile red chromaticity value.
 * This function will only return results if the profile was loaded with the
 * %CD_ICC_LOAD_FLAGS_PRIMARIES flag.
 *
 * Return value: the #CdColorXYZ value
 *
 * Since: 0.1.32
 **/
const CdColorXYZ *
cd_icc_get_red (CdIcc *icc)
{
	g_return_val_if_fail (CD_IS_ICC (icc), NULL);
	return &icc->priv->red;
}

/**
 * cd_icc_get_green:
 * @icc: a valid #CdIcc instance
 *
 * Gets the profile green chromaticity value.
 * This function will only return results if the profile was loaded with the
 * %CD_ICC_LOAD_FLAGS_PRIMARIES flag.
 *
 * Return value: the #CdColorXYZ value
 *
 * Since: 0.1.32
 **/
const CdColorXYZ *
cd_icc_get_green (CdIcc *icc)
{
	g_return_val_if_fail (CD_IS_ICC (icc), NULL);
	return &icc->priv->green;
}

/**
 * cd_icc_get_blue:
 * @icc: a valid #CdIcc instance
 *
 * Gets the profile red chromaticity value.
 * This function will only return results if the profile was loaded with the
 * %CD_ICC_LOAD_FLAGS_PRIMARIES flag.
 *
 * Return value: the #CdColorXYZ value
 *
 * Since: 0.1.32
 **/
const CdColorXYZ *
cd_icc_get_blue (CdIcc *icc)
{
	g_return_val_if_fail (CD_IS_ICC (icc), NULL);
	return &icc->priv->blue;
}

/**
 * cd_icc_get_white:
 * @icc: a valid #CdIcc instance
 *
 * Gets the profile white point.
 * This function will only return results if the profile was loaded with the
 * %CD_ICC_LOAD_FLAGS_PRIMARIES flag.
 *
 * Return value: the #CdColorXYZ value
 *
 * Since: 0.1.32
 **/
const CdColorXYZ *
cd_icc_get_white (CdIcc *icc)
{
	g_return_val_if_fail (CD_IS_ICC (icc), NULL);
	return &icc->priv->white;
}

/**
 * cd_icc_create_from_edid:
 * @icc: A valid #CdIcc
 * @gamma_value: approximate device gamma
 * @red: primary color value
 * @green: primary color value
 * @blue: primary color value
 * @white: whitepoint value
 * @error: A #GError, or %NULL
 *
 * Creates an ICC profile from EDID data.
 *
 * Return value: %TRUE for success
 *
 * Since: 0.1.32
 **/
gboolean
cd_icc_create_from_edid (CdIcc *icc,
			 gdouble gamma_value,
			 const CdColorYxy *red,
			 const CdColorYxy *green,
			 const CdColorYxy *blue,
			 const CdColorYxy *white,
			 GError **error)
{
	CdIccPrivate *priv = icc->priv;
	cmsCIExyYTRIPLE chroma;
	cmsCIExyY white_point;
	cmsToneCurve *transfer_curve[3] = { NULL, NULL, NULL };
	gboolean ret = FALSE;

	/* setup error handler */
	cmsSetLogErrorHandler (cd_icc_lcms2_error_cb);

	/* not loaded */
	if (priv->lcms_profile != NULL) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_FAILED_TO_CREATE,
				     "already loaded or generated");
		goto out;
	}

	/* copy data from our structures (which are the wrong packing
	 * size for lcms2) */
	chroma.Red.x = red->x;
	chroma.Red.y = red->y;
	chroma.Green.x = green->x;
	chroma.Green.y = green->y;
	chroma.Blue.x = blue->x;
	chroma.Blue.y = blue->y;
	white_point.x = white->x;
	white_point.y = white->y;
	white_point.Y = 1.0;

	/* estimate the transfer function for the gamma */
	transfer_curve[0] = cmsBuildGamma (NULL, gamma_value);
	transfer_curve[1] = transfer_curve[0];
	transfer_curve[2] = transfer_curve[0];

	/* create our generated ICC */
	priv->lcms_profile = cmsCreateRGBProfile (&white_point,
						  &chroma,
						  transfer_curve);
	if (priv->lcms_profile == NULL) {
		g_set_error (error,
			     CD_ICC_ERROR,
			     CD_ICC_ERROR_FAILED_TO_CREATE,
			     "failed to create profile with chroma and gamma");
		goto out;
	}

	/* set header options */
	cmsSetHeaderRenderingIntent (priv->lcms_profile, INTENT_PERCEPTUAL);
	cmsSetDeviceClass (priv->lcms_profile, cmsSigDisplayClass);

	/* copy any important parts out of the lcms-generated profile */
	ret = cd_icc_load (icc, CD_ICC_LOAD_FLAGS_NONE, error);
	if (!ret)
		goto out;

	/* set the data source so we don't ever prompt the user to
	* recalibrate (as the EDID data won't have changed) */
	cd_icc_add_metadata (icc,
			     CD_PROFILE_METADATA_DATA_SOURCE,
			     CD_PROFILE_METADATA_DATA_SOURCE_EDID);

	/* success */
	ret = TRUE;
out:
	if (transfer_curve[0] != NULL)
		cmsFreeToneCurve (transfer_curve[0]);
	return ret;
}

/**
 * cd_icc_get_vcgt:
 * @icc: A valid #CdIcc
 * @size: the desired size of the table data
 * @error: A #GError or %NULL
 *
 * Gets the video card calibration data from the profile.
 *
 * Return value: (transfer container) (element-type CdColorRGB): VCGT data, or %NULL for error
 *
 * Since: 0.1.34
 **/
GPtrArray *
cd_icc_get_vcgt (CdIcc *icc, guint size, GError **error)
{
	CdColorRGB *tmp;
	cmsFloat32Number in;
	const cmsToneCurve **vcgt;
	GPtrArray *array = NULL;
	guint i;

	g_return_val_if_fail (CD_IS_ICC (icc), NULL);
	g_return_val_if_fail (icc->priv->lcms_profile != NULL, NULL);

	/* setup error handler */
	cmsSetLogErrorHandler (cd_icc_lcms2_error_cb);

	/* get tone curves from icc */
	vcgt = cmsReadTag (icc->priv->lcms_profile, cmsSigVcgtType);
	if (vcgt == NULL || vcgt[0] == NULL) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_NO_DATA,
				     "icc does not have any VCGT data");
		goto out;
	}

	/* create array */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_color_rgb_free);
	for (i = 0; i < size; i++) {
		in = (gdouble) i / (gdouble) (size - 1);
		tmp = cd_color_rgb_new ();
		cd_color_rgb_set (tmp,
				  cmsEvalToneCurveFloat(vcgt[0], in),
				  cmsEvalToneCurveFloat(vcgt[1], in),
				  cmsEvalToneCurveFloat(vcgt[2], in));
		g_ptr_array_add (array, tmp);
	}
out:
	return array;

}

/**
 * cd_icc_get_response:
 * @icc: A valid #CdIcc
 * @size: the size of the curve to generate
 * @error: a valid #GError, or %NULL
 *
 * Generates a response curve of a specified size.
 *
 * Return value: (transfer container) (element-type CdColorRGB): response data, or %NULL for error
 *
 * Since: 0.1.34
 **/
GPtrArray *
cd_icc_get_response (CdIcc *icc, guint size, GError **error)
{
	CdColorRGB *data;
	CdColorspace colorspace;
	cmsHPROFILE srgb_profile = NULL;
	cmsHTRANSFORM transform = NULL;
	const guint component_width = 3;
	gdouble tmp;
	gdouble *values_in = NULL;
	gdouble *values_out = NULL;
	gfloat divadd;
	gfloat divamount;
	GPtrArray *array = NULL;
	guint i;

	/* setup error handler */
	cmsSetLogErrorHandler (cd_icc_lcms2_error_cb);

	/* run through the icc */
	colorspace = cd_icc_get_colorspace (icc);
	if (colorspace != CD_COLORSPACE_RGB) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_INVALID_COLORSPACE,
				     "Only RGB colorspaces are supported");
		goto out;
	}

	/* create input array */
	values_in = g_new0 (gdouble, size * 3 * component_width);
	divamount = 1.0f / (gfloat) (size - 1);
	for (i = 0; i < size; i++) {
		divadd = divamount * (gfloat) i;

		/* red */
		values_in[(i * 3 * component_width) + 0] = divadd;
		values_in[(i * 3 * component_width) + 1] = 0.0f;
		values_in[(i * 3 * component_width) + 2] = 0.0f;

		/* green */
		values_in[(i * 3 * component_width) + 3] = 0.0f;
		values_in[(i * 3 * component_width) + 4] = divadd;
		values_in[(i * 3 * component_width) + 5] = 0.0f;

		/* blue */
		values_in[(i * 3 * component_width) + 6] = 0.0f;
		values_in[(i * 3 * component_width) + 7] = 0.0f;
		values_in[(i * 3 * component_width) + 8] = divadd;
	}

	/* create a transform from icc to sRGB */
	values_out = g_new0 (gdouble, size * 3 * component_width);
	srgb_profile = cmsCreate_sRGBProfile ();
	transform = cmsCreateTransform (icc->priv->lcms_profile, TYPE_RGB_DBL,
					srgb_profile, TYPE_RGB_DBL,
					INTENT_PERCEPTUAL, 0);
	if (transform == NULL) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_NO_DATA,
				     "Failed to setup transform");
		goto out;
	}
	cmsDoTransform (transform, values_in, values_out, size * 3);

	/* create output array */
	array = cd_color_rgb_array_new ();
	for (i = 0; i < size; i++) {
		data = cd_color_rgb_new ();
		cd_color_rgb_set (data, 0.0f, 0.0f, 0.0f);

		/* only save curve data if it is positive */
		tmp = values_out[(i * 3 * component_width) + 0];
		if (tmp > 0.0f)
			data->R = tmp;
		tmp = values_out[(i * 3 * component_width) + 4];
		if (tmp > 0.0f)
			data->G = tmp;
		tmp = values_out[(i * 3 * component_width) + 8];
		if (tmp > 0.0f)
			data->B = tmp;
		g_ptr_array_add (array, data);
	}
out:
	g_free (values_in);
	g_free (values_out);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	if (srgb_profile != NULL)
		cmsCloseProfile (srgb_profile);
	return array;
}

/**
 * cd_icc_set_vcgt:
 * @icc: A valid #CdIcc
 * @vcgt: (element-type CdColorRGB): video card calibration data
 * @error: A #GError or %NULL
 *
 * Sets the Video Card Gamma Table from the profile.
 *
 * Return vale: %TRUE for success.
 *
 * Since: 0.1.34
 **/
gboolean
cd_icc_set_vcgt (CdIcc *icc, GPtrArray *vcgt, GError **error)
{
	CdColorRGB *tmp;
	cmsToneCurve *curve[3];
	gboolean ret;
	guint16 *blue;
	guint16 *green;
	guint16 *red;
	guint i;

	g_return_val_if_fail (CD_IS_ICC (icc), FALSE);
	g_return_val_if_fail (icc->priv->lcms_profile != NULL, FALSE);

	/* setup error handler */
	cmsSetLogErrorHandler (cd_icc_lcms2_error_cb);

	/* unwrap data */
	red = g_new0 (guint16, vcgt->len);
	green = g_new0 (guint16, vcgt->len);
	blue = g_new0 (guint16, vcgt->len);
	for (i = 0; i < vcgt->len; i++) {
		tmp = g_ptr_array_index (vcgt, i);
		red[i]   = tmp->R * (gdouble) 0xffff;
		green[i] = tmp->G * (gdouble) 0xffff;
		blue[i]  = tmp->B * (gdouble) 0xffff;
	}

	/* build tone curve */
	curve[0] = cmsBuildTabulatedToneCurve16 (NULL, vcgt->len, red);
	curve[1] = cmsBuildTabulatedToneCurve16 (NULL, vcgt->len, green);
	curve[2] = cmsBuildTabulatedToneCurve16 (NULL, vcgt->len, blue);

	/* smooth it */
	for (i = 0; i < 3; i++)
		cmsSmoothToneCurve (curve[i], 5);

	/* write the tag */
	ret = cmsWriteTag (icc->priv->lcms_profile, cmsSigVcgtType, curve);
	if (!ret) {
		g_set_error_literal (error,
				     CD_ICC_ERROR,
				     CD_ICC_ERROR_NO_DATA,
				     "failed to write VCGT data");
		goto out;
	}
out:
	for (i = 0; i < 3; i++)
		cmsFreeToneCurve (curve[i]);
	g_free (red);
	g_free (green);
	g_free (blue);
	return ret;
}

/**
 * cd_icc_check_whitepoint:
 **/
static CdProfileWarning
cd_icc_check_whitepoint (CdIcc *icc)
{
	CdProfileWarning warning = CD_PROFILE_WARNING_NONE;
	guint temp = icc->priv->temperature;

	/* not set */
	if (temp == 0)
		goto out;

	/* hardcoded sanity check */
	if (temp < 3000 || temp > 10000)
		warning = CD_PROFILE_WARNING_WHITEPOINT_UNLIKELY;
out:
	return warning;
}

/**
 * cd_icc_check_vcgt:
 **/
static CdProfileWarning
cd_icc_check_vcgt (cmsHPROFILE profile)
{
	CdProfileWarning warning = CD_PROFILE_WARNING_NONE;
	cmsFloat32Number in;
	cmsFloat32Number now[3];
	cmsFloat32Number previous[3] = { -1, -1, -1};
	const cmsToneCurve **vcgt;
	const guint size = 32;
	guint i;

	/* does profile have monotonic VCGT */
	vcgt = cmsReadTag (profile, cmsSigVcgtTag);
	if (vcgt == NULL)
		goto out;
	for (i = 0; i < size; i++) {
		in = (gdouble) i / (gdouble) (size - 1);
		now[0] = cmsEvalToneCurveFloat(vcgt[0], in);
		now[1] = cmsEvalToneCurveFloat(vcgt[1], in);
		now[2] = cmsEvalToneCurveFloat(vcgt[2], in);

		/* check VCGT is increasing */
		if (i > 0) {
			if (now[0] < previous[0] ||
			    now[1] < previous[1] ||
			    now[2] < previous[2]) {
				warning = CD_PROFILE_WARNING_VCGT_NON_MONOTONIC;
				goto out;
			}
		}
		previous[0] = now[0];
		previous[1] = now[1];
		previous[2] = now[2];
	}
out:
	return warning;
}

/**
 * cd_profile_check_scum_dot:
 **/
static CdProfileWarning
cd_profile_check_scum_dot (cmsHPROFILE profile)
{
	CdProfileWarning warning = CD_PROFILE_WARNING_NONE;
	cmsCIELab white;
	cmsHPROFILE profile_lab;
	cmsHTRANSFORM transform;
	guint8 rgb[3] = { 0, 0, 0 };

	/* do Lab to RGB transform of 100,0,0 */
	profile_lab = cmsCreateLab2Profile (cmsD50_xyY ());
	transform = cmsCreateTransform (profile_lab, TYPE_Lab_DBL,
					profile, TYPE_RGB_8,
					INTENT_RELATIVE_COLORIMETRIC,
					cmsFLAGS_NOOPTIMIZE);
	if (transform == NULL) {
		g_warning ("failed to setup Lab -> RGB transform");
		goto out;
	}
	white.L = 100.0;
	white.a = 0.0;
	white.b = 0.0;
	cmsDoTransform (transform, &white, rgb, 1);
	if (rgb[0] != 255 || rgb[1] != 255 || rgb[2] != 255) {
		warning = CD_PROFILE_WARNING_SCUM_DOT;
		goto out;
	}
out:
	if (profile_lab != NULL)
		cmsCloseProfile (profile_lab);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	return warning;
}

/**
 * cd_icc_check_primaries:
 **/
static CdProfileWarning
cd_icc_check_primaries (cmsHPROFILE profile)
{
	CdProfileWarning warning = CD_PROFILE_WARNING_NONE;
	cmsCIEXYZ *tmp;

	/* The values used to check are based on the following ultra-wide
	 * gamut profile XYZ values:
	 *
	 * CIERGB:
	 * Red:		0.486893 0.174667 -0.001251
	 * Green:	0.306320 0.824768 0.016998
	 * Blue:	0.170990 0.000565 0.809158

	 * ProPhoto RGB:
	 * Red:		0.797546 0.288025 0.000000
	 * Green:	0.135315 0.711899 -0.000015
	 * Blue:	0.031342 0.000076 0.824921
	 */

	/* check red */
	tmp = cmsReadTag (profile, cmsSigRedColorantTag);
	if (tmp == NULL)
		goto out;
	if (tmp->X > 0.85f || tmp->Y < 0.15f || tmp->Z < -0.01) {
		warning = CD_PROFILE_WARNING_PRIMARIES_INVALID;
		goto out;
	}

	/* check green */
	tmp = cmsReadTag (profile, cmsSigGreenColorantTag);
	if (tmp == NULL)
		goto out;
	if (tmp->X < 0.10f || tmp->Y > 0.85f || tmp->Z < -0.01f) {
		warning = CD_PROFILE_WARNING_PRIMARIES_INVALID;
		goto out;
	}

	/* check blue */
	tmp = cmsReadTag (profile, cmsSigBlueColorantTag);
	if (tmp == NULL)
		goto out;
	if (tmp->X < 0.10f || tmp->Y < 0.01f || tmp->Z > 0.87f) {
		warning = CD_PROFILE_WARNING_PRIMARIES_INVALID;
		goto out;
	}
out:
	return warning;
}

/**
 * cd_icc_check_gray_axis:
 **/
static CdProfileWarning
cd_icc_check_gray_axis (cmsHPROFILE profile)
{
	CdProfileWarning warning = CD_PROFILE_WARNING_NONE;
	cmsCIELab gray[16];
	cmsHPROFILE profile_lab = NULL;
	cmsHTRANSFORM transform = NULL;
	const gdouble gray_error = 5.0f;
	gdouble last_l = -1;
	guint8 rgb[3*16];
	guint8 tmp;
	guint i;

	/* only do this for display profiles */
	if (cmsGetDeviceClass (profile) != cmsSigDisplayClass)
		goto out;

	/* do Lab to RGB transform of 100,0,0 */
	profile_lab = cmsCreateLab2Profile (cmsD50_xyY ());
	transform = cmsCreateTransform (profile, TYPE_RGB_8,
					profile_lab, TYPE_Lab_DBL,
					INTENT_RELATIVE_COLORIMETRIC,
					cmsFLAGS_NOOPTIMIZE);
	if (transform == NULL) {
		g_warning ("failed to setup RGB -> Lab transform");
		goto out;
	}

	/* run a 16 item gray ramp through the transform */
	for (i = 0; i < 16; i++) {
		tmp = (255.0f / (16.0f - 1)) * i;
		rgb[(i * 3) + 0] = tmp;
		rgb[(i * 3) + 1] = tmp;
		rgb[(i * 3) + 2] = tmp;
	}
	cmsDoTransform (transform, rgb, gray, 16);

	/* check a/b is small */
	for (i = 0; i < 16; i++) {
		if (gray[i].a > gray_error ||
		    gray[i].b > gray_error) {
			warning = CD_PROFILE_WARNING_GRAY_AXIS_INVALID;
			goto out;
		}
	}

	/* check it's monotonic */
	for (i = 0; i < 16; i++) {
		if (last_l > 0 && gray[i].L < last_l) {
			warning = CD_PROFILE_WARNING_GRAY_AXIS_NON_MONOTONIC;
			goto out;
		}
		last_l = gray[i].L;
	}
out:
	if (profile_lab != NULL)
		cmsCloseProfile (profile_lab);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	return warning;
}

/**
 * cd_icc_check_d50_whitepoint:
 **/
static CdProfileWarning
cd_icc_check_d50_whitepoint (cmsHPROFILE profile)
{
	CdProfileWarning warning = CD_PROFILE_WARNING_NONE;
	cmsCIExyY tmp;
	cmsCIEXYZ additive;
	cmsCIEXYZ primaries[4];
	cmsHPROFILE profile_lab;
	cmsHTRANSFORM transform;
	const cmsCIEXYZ *d50;
	const gdouble rgb_error = 0.05;
	const gdouble additive_error = 0.1f;
	const gdouble white_error = 0.05;
	guint8 rgb[3*4];
	guint i;

	/* do Lab to RGB transform to get primaries */
	profile_lab = cmsCreateXYZProfile ();
	transform = cmsCreateTransform (profile, TYPE_RGB_8,
					profile_lab, TYPE_XYZ_DBL,
					INTENT_RELATIVE_COLORIMETRIC,
					cmsFLAGS_NOOPTIMIZE);
	if (transform == NULL) {
		g_warning ("failed to setup RGB -> XYZ transform");
		goto out;
	}

	/* Run RGBW through the transform */
	rgb[0 + 0] = 255;
	rgb[0 + 1] = 0;
	rgb[0 + 2] = 0;
	rgb[3 + 0] = 0;
	rgb[3 + 1] = 255;
	rgb[3 + 2] = 0;
	rgb[6 + 0] = 0;
	rgb[6 + 1] = 0;
	rgb[6 + 2] = 255;
	rgb[9 + 0] = 255;
	rgb[9 + 1] = 255;
	rgb[9 + 2] = 255;
	cmsDoTransform (transform, rgb, primaries, 4);

	/* check red is in gamut */
	cmsXYZ2xyY (&tmp, &primaries[0]);
	if (tmp.x - 0.735 > rgb_error || 0.265 - tmp.y > rgb_error) {
		warning = CD_PROFILE_WARNING_PRIMARIES_UNLIKELY;
		goto out;
	}

	/* check green is in gamut */
	cmsXYZ2xyY (&tmp, &primaries[1]);
	if (0.160 - tmp.x > rgb_error || tmp.y - 0.840 > rgb_error) {
		warning = CD_PROFILE_WARNING_PRIMARIES_UNLIKELY;
		goto out;
	}

	/* check blue is in gamut */
	cmsXYZ2xyY (&tmp, &primaries[2]);
	if (0.037 - tmp.x > rgb_error || tmp.y - 0.358 > rgb_error) {
		warning = CD_PROFILE_WARNING_PRIMARIES_UNLIKELY;
		goto out;
	}

	/* only do the rest for display profiles */
	if (cmsGetDeviceClass (profile) != cmsSigDisplayClass)
		goto out;

	/* check white is D50 */
	d50 = cmsD50_XYZ();
	if (fabs (primaries[3].X - d50->X) > white_error ||
	    fabs (primaries[3].Y - d50->Y) > white_error ||
	    fabs (primaries[3].Z - d50->Z) > white_error) {
		warning = CD_PROFILE_WARNING_WHITEPOINT_INVALID;
		goto out;
	}

	/* check primaries add up to D50 */
	additive.X = 0;
	additive.Y = 0;
	additive.Z = 0;
	for (i = 0; i < 3; i++) {
		additive.X += primaries[i].X;
		additive.Y += primaries[i].Y;
		additive.Z += primaries[i].Z;
	}
	if (fabs (additive.X - d50->X) > additive_error ||
	    fabs (additive.Y - d50->Y) > additive_error ||
	    fabs (additive.Z - d50->Z) > additive_error) {
		warning = CD_PROFILE_WARNING_PRIMARIES_NON_ADDITIVE;
		goto out;
	}
out:
	if (profile_lab != NULL)
		cmsCloseProfile (profile_lab);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	return warning;
}

/**
 * cd_icc_get_warnings:
 * @icc: a #CdIcc instance.
 *
 * Returns any warnings with profiles
 *
 * Return value: (transfer container) (element-type CdProfileWarning): An array of warning values
 *
 * Since: 0.1.34
 **/
GArray *
cd_icc_get_warnings (CdIcc *icc)
{
	GArray *flags;
	gboolean ret;
	gchar ascii_name[1024];
	CdProfileWarning warning;

	g_return_val_if_fail (CD_IS_ICC (icc), NULL);
	g_return_val_if_fail (icc->priv->lcms_profile != NULL, NULL);

	/* setup error handler */
	cmsSetLogErrorHandler (cd_icc_lcms2_error_cb);

	flags = g_array_new (FALSE, FALSE, sizeof (CdProfileWarning));

	/* check that the profile has a description and a copyright */
	ret = cmsGetProfileInfoASCII (icc->priv->lcms_profile,
				      cmsInfoDescription, "en", "US",
				      ascii_name, 1024);
	if (!ret || ascii_name[0] == '\0') {
		warning = CD_PROFILE_WARNING_DESCRIPTION_MISSING;
		g_array_append_val (flags, warning);
	}
	ret = cmsGetProfileInfoASCII (icc->priv->lcms_profile,
				      cmsInfoCopyright, "en", "US",
				      ascii_name, 1024);
	if (!ret || ascii_name[0] == '\0') {
		warning = CD_PROFILE_WARNING_COPYRIGHT_MISSING;
		g_array_append_val (flags, warning);
	}

	/* not a RGB space */
	if (cmsGetColorSpace (icc->priv->lcms_profile) != cmsSigRgbData)
		goto out;

	/* does profile have an unlikely whitepoint */
	warning = cd_icc_check_whitepoint (icc);
	if (warning != CD_PROFILE_WARNING_NONE)
		g_array_append_val (flags, warning);

	/* does profile have monotonic VCGT */
	warning = cd_icc_check_vcgt (icc->priv->lcms_profile);
	if (warning != CD_PROFILE_WARNING_NONE)
		g_array_append_val (flags, warning);

	/* if Lab 100,0,0 does not map to RGB 255,255,255 for relative
	 * colorimetric then white it will not work on printers */
	warning = cd_profile_check_scum_dot (icc->priv->lcms_profile);
	if (warning != CD_PROFILE_WARNING_NONE)
		g_array_append_val (flags, warning);

	/* gray should give low a/b and should be monotonic */
	warning = cd_icc_check_gray_axis (icc->priv->lcms_profile);
	if (warning != CD_PROFILE_WARNING_NONE)
		g_array_append_val (flags, warning);

	/* tristimulus values cannot be negative */
	warning = cd_icc_check_primaries (icc->priv->lcms_profile);
	if (warning != CD_PROFILE_WARNING_NONE)
		g_array_append_val (flags, warning);

	/* check whitepoint works out to D50 */
	warning = cd_icc_check_d50_whitepoint (icc->priv->lcms_profile);
	if (warning != CD_PROFILE_WARNING_NONE)
		g_array_append_val (flags, warning);
out:
	return flags;
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
	case PROP_CHECKSUM:
		g_value_set_string (value, priv->checksum);
		break;
	case PROP_WHITE:
		g_value_set_boxed (value, g_boxed_copy (CD_TYPE_COLOR_XYZ, &priv->white));
		break;
	case PROP_RED:
		g_value_set_boxed (value, g_boxed_copy (CD_TYPE_COLOR_XYZ, &priv->red));
		break;
	case PROP_GREEN:
		g_value_set_boxed (value, g_boxed_copy (CD_TYPE_COLOR_XYZ, &priv->green));
		break;
	case PROP_BLUE:
		g_value_set_boxed (value, g_boxed_copy (CD_TYPE_COLOR_XYZ, &priv->blue));
		break;
	case PROP_TEMPERATURE:
		g_value_set_uint (value, priv->temperature);
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
	CdIcc *icc = CD_ICC (object);
	switch (prop_id) {
	case PROP_KIND:
		cd_icc_set_kind (icc, g_value_get_uint (value));
		break;
	case PROP_COLORSPACE:
		cd_icc_set_colorspace (icc, g_value_get_uint (value));
		break;
	case PROP_VERSION:
		cd_icc_set_version (icc, g_value_get_double (value));
		break;
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
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_VERSION, pspec);

	/**
	 * CdIcc:kind:
	 */
	pspec = g_param_spec_uint ("kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_KIND, pspec);

	/**
	 * CdIcc:colorspace:
	 */
	pspec = g_param_spec_uint ("colorspace", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_COLORSPACE, pspec);

	/**
	 * CdIcc:can-delete:
	 */
	pspec = g_param_spec_boolean ("can-delete", NULL, NULL,
				      FALSE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_CAN_DELETE, pspec);

	/**
	 * CdIcc:checksum:
	 */
	pspec = g_param_spec_string ("checksum", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_CHECKSUM, pspec);

	/**
	 * CdIcc:white:
	 */
	pspec = g_param_spec_boxed ("white", NULL, NULL,
				    CD_TYPE_COLOR_XYZ,
				    G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_WHITE, pspec);

	/**
	 * CdIcc:red:
	 */
	pspec = g_param_spec_boxed ("red", NULL, NULL,
				    CD_TYPE_COLOR_XYZ,
				    G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_RED, pspec);

	/**
	 * CdIcc:green:
	 */
	pspec = g_param_spec_boxed ("green", NULL, NULL,
				    CD_TYPE_COLOR_XYZ,
				    G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_GREEN, pspec);

	/**
	 * CdIcc:blue:
	 */
	pspec = g_param_spec_boxed ("blue", NULL, NULL,
				    CD_TYPE_COLOR_XYZ,
				    G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_BLUE, pspec);

	/**
	 * CdIcc:temperature:
	 */
	pspec = g_param_spec_uint ("temperature", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_TEMPERATURE, pspec);

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
	icc->priv->named_colors = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_color_swatch_free);
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
	cd_color_xyz_clear (&icc->priv->white);
	cd_color_xyz_clear (&icc->priv->red);
	cd_color_xyz_clear (&icc->priv->green);
	cd_color_xyz_clear (&icc->priv->blue);
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
	g_free (priv->checksum);
	g_ptr_array_unref (priv->named_colors);
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
