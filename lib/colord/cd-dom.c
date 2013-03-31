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
 * SECTION:cd-dom
 * @short_description: A XML parser that exposes a DOM tree
 */

#include "config.h"

#include <glib.h>

#include "cd-dom.h"

static void	cd_dom_class_init	(CdDomClass	*klass);
static void	cd_dom_init		(CdDom		*dom);
static void	cd_dom_finalize		(GObject	*object);

#define CD_DOM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_DOM, CdDomPrivate))

/**
 * CdDomPrivate:
 *
 * Private #CdDom data
 **/
struct _CdDomPrivate
{
	GNode			*root;
	GNode			*current;
};

typedef struct
{
	gchar		*name;
	GString		*cdata;
	GHashTable	*attributes;
} CdDomNodeData;

G_DEFINE_TYPE (CdDom, cd_dom, G_TYPE_OBJECT)

/**
 * cd_dom_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.31
 **/
GQuark
cd_dom_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("cd_dom_error");
	return quark;
}

/**
 * cd_dom_print_node_cb:
 **/
static gboolean
cd_dom_print_node_cb (GNode *node, gpointer user_data)
{
	CdDomNodeData *data = node->data;
	GString *string = user_data;
	guint depth = g_node_depth (node);
	guint i;

	if (data == NULL)
		goto out;
	for (i = 0; i < depth; i++)
		g_string_append (string, " ");
	g_string_append_printf (string,
				"<%s> [%s]\n",
				data->name,
				data->cdata->str);
out:
	return FALSE;
}

/**
 * cd_dom_to_string:
 * @dom: a #CdDom instance.
 *
 * Returns a string representation of the DOM tree.
 *
 * Return value: an allocated string
 *
 * Since: 0.1.31
 **/
gchar *
cd_dom_to_string (CdDom *dom)
{
	GString *string;

	g_return_val_if_fail (CD_IS_DOM (dom), NULL);

	string = g_string_new (NULL);
	g_node_traverse (dom->priv->root,
			 G_PRE_ORDER,
			 G_TRAVERSE_ALL,
			 -1,
			 cd_dom_print_node_cb,
			 string);

	return g_string_free (string, FALSE);
}

/**
 * cd_dom_start_element_cb:
 **/
static void
cd_dom_start_element_cb (GMarkupParseContext *context,
			 const gchar         *element_name,
			 const gchar        **attribute_names,
			 const gchar        **attribute_values,
			 gpointer             user_data,
			 GError             **error)
{
	CdDom *dom = (CdDom *) user_data;
	CdDomNodeData *data;
	GNode *new;
	guint i;

	/* create the new node data */
	data = g_slice_new (CdDomNodeData);
	data->name = g_strdup (element_name);
	data->cdata = g_string_new (NULL);
	data->attributes = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free,
						  g_free);
	for (i = 0; attribute_names[i] != NULL; i++) {
		g_hash_table_insert (data->attributes,
				     g_strdup (attribute_names[i]),
				     g_strdup (attribute_values[i]));
	}

	/* add the node to the DOM */
	new = g_node_new (data);
	g_node_append (dom->priv->current, new);
	dom->priv->current = new;
}

/**
 * cd_dom_end_element_cb:
 **/
static void
cd_dom_end_element_cb (GMarkupParseContext *context,
		       const gchar         *element_name,
		       gpointer             user_data,
		       GError             **error)
{
	CdDom *dom = (CdDom *) user_data;
	dom->priv->current = dom->priv->current->parent;
}

/**
 * cd_dom_text_cb:
 **/
static void
cd_dom_text_cb (GMarkupParseContext *context,
		const gchar         *text,
		gsize                text_len,
		gpointer             user_data,
		GError             **error)
{
	CdDom *dom = (CdDom *) user_data;
	CdDomNodeData *data;
	guint i;

	/* no data */
	if (text_len == 0)
		return;

	/* all whitespace? */
	for (i = 0; i < text_len; i++) {
		if (text[i] != ' ' &&
		    text[i] != '\n' &&
		    text[i] != '\t')
			break;
	}
	if (i >= text_len)
		return;

	/* save cdata */
	data = dom->priv->current->data;
	g_string_append (data->cdata, text);
}

/**
 * cd_dom_parse_xml_data:
 * @dom: a #CdDom instance.
 * @data: XML data
 * @data_len: Length of @data, or -1 if NULL terminated
 * @error: A #GError or %NULL
 *
 * Parses data into a DOM tree.
 *
 * Since: 0.1.31
 **/
gboolean
cd_dom_parse_xml_data (CdDom *dom,
		       const gchar *data,
		       gssize data_len,
		       GError **error)
{
	gboolean ret;
	GMarkupParseContext *ctx;
	const GMarkupParser parser = {
		cd_dom_start_element_cb,
		cd_dom_end_element_cb,
		cd_dom_text_cb,
		NULL,
		NULL };

	g_return_val_if_fail (CD_IS_DOM (dom), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	ctx = g_markup_parse_context_new (&parser,
					  G_MARKUP_PREFIX_ERROR_POSITION,
					  dom,
					  NULL);
	ret = g_markup_parse_context_parse (ctx,
					    data,
					    data_len,
					    error);
	if (!ret)
		goto out;
out:
	g_markup_parse_context_free (ctx);
	return ret;
}

/**
 * cd_dom_get_child_node:
 **/
static GNode *
cd_dom_get_child_node (const GNode *root, const gchar *name)
{
	GNode *node;
	CdDomNodeData *data;

	/* find a node called name */
	for (node = root->children; node != NULL; node = node->next) {
		data = node->data;
		if (data == NULL)
			return NULL;
		if (g_strcmp0 (data->name, name) == 0)
			return node;
	}
	return NULL;
}

/**
 * cd_dom_get_node_name:
 * @node: a #GNode
 *
 * Gets the node name, e.g. "body"
 *
 * Return value: string value
 *
 * Since: 0.1.31
 **/
const gchar *
cd_dom_get_node_name (const GNode *node)
{
	g_return_val_if_fail (node != NULL, NULL);
	if (node->data == NULL)
		return NULL;
	return ((CdDomNodeData *) node->data)->name;
}

/**
 * cd_dom_get_node_data:
 * @node: a #GNode
 *
 * Gets the node data, e.g. "paragraph text"
 *
 * Return value: string value
 *
 * Since: 0.1.31
 **/
const gchar *
cd_dom_get_node_data (const GNode *node)
{
	g_return_val_if_fail (node != NULL, NULL);
	if (node->data == NULL)
		return NULL;
	return ((CdDomNodeData *) node->data)->cdata->str;
}

/**
 * cd_dom_get_node_data_as_double:
 * @node: a #GNode
 *
 * Gets the node data, e.g. 7.4
 *
 * Return value: floating point value, or %G_MAXDOUBLE for error
 *
 * Since: 0.1.32
 **/
gdouble
cd_dom_get_node_data_as_double (const GNode *node)
{
	const gchar *tmp;
	gchar *endptr = NULL;
	gdouble value = G_MAXDOUBLE;
	gdouble value_tmp;

	g_return_val_if_fail (node != NULL, G_MAXDOUBLE);

	/* get string */
	tmp = cd_dom_get_node_data (node);
	if (tmp == NULL)
		goto out;

	/* convert to string */
	value_tmp = g_ascii_strtod (tmp, &endptr);
	if (endptr != NULL && endptr[0] != '\0')
		goto out;

	/* success */
	value = value_tmp;
out:
	return value;
}

/**
 * cd_dom_get_node_data_as_int:
 * @node: a #GNode
 *
 * Gets the node data, e.g. 128
 *
 * Return value: signed integer value, or %G_MAXINT for error
 *
 * Since: 0.1.32
 **/
gint
cd_dom_get_node_data_as_int (const GNode *node)
{
	const gchar *tmp;
	gchar *endptr = NULL;
	gint64 value_tmp;
	gint value = G_MAXINT;

	g_return_val_if_fail (node != NULL, G_MAXINT);

	/* get string */
	tmp = cd_dom_get_node_data (node);
	if (tmp == NULL)
		goto out;

	/* convert to string */
	value_tmp = g_ascii_strtoll (tmp, &endptr, 10);
	if (endptr != NULL && endptr[0] != '\0')
		goto out;
	if (value_tmp > G_MAXINT || value_tmp < G_MININT)
		goto out;

	/* success */
	value = (gint) value_tmp;
out:
	return value;
}

/**
 * cd_dom_get_node_attribute:
 * @node: a #GNode
 *
 * Gets a node attribute, e.g. "false"
 *
 * Return value: string value
 *
 * Since: 0.1.31
 **/
const gchar *
cd_dom_get_node_attribute (const GNode *node, const gchar *key)
{
	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (key != NULL, NULL);
	if (node->data == NULL)
		return NULL;
	return g_hash_table_lookup (((CdDomNodeData *) node->data)->attributes, key);
}

/**
 * cd_dom_get_node:
 * @dom: a #CdDom instance.
 * @root: a root node, or %NULL
 * @path: a path in the DOM, e.g. "html/body"
 *
 * Gets a node from the DOM tree.
 *
 * Return value: A #GNode, or %NULL if not found
 *
 * Since: 0.1.31
 **/
const GNode *
cd_dom_get_node (CdDom *dom, const GNode *root, const gchar *path)
{
	gchar **split;
	const GNode *node;
	guint i;

	g_return_val_if_fail (CD_IS_DOM (dom), NULL);
	g_return_val_if_fail (path != NULL, NULL);

	/* default value */
	if (root == NULL)
		root = dom->priv->root;

	node = root;
	split = g_strsplit (path, "/", -1);
	for (i = 0; split[i] != NULL; i++) {
		node = cd_dom_get_child_node (node, split[i]);
		if (node == NULL)
			goto out;
	}
out:
	g_strfreev (split);
	return node;
}

/**
 * cd_dom_get_node_lab:
 * @node: a #GNode
 * @lab: a #CdColorLab
 *
 * Extracts a Lab color value from the DOM tree
 *
 * Return value: %TRUE if the color was parsed successfully
 *
 * Since: 0.1.31
 **/
gboolean
cd_dom_get_node_lab (const GNode *node, CdColorLab *lab)
{
	gboolean ret = FALSE;
	const GNode *values[3];
	gchar *endptr = NULL;

	/* find nodes */
	values[0] = cd_dom_get_child_node (node, "L");
	if (values[0] == NULL)
		goto out;
	values[1] = cd_dom_get_child_node (node, "a");
	if (values[1] == NULL)
		goto out;
	values[2] = cd_dom_get_child_node (node, "b");
	if (values[2] == NULL)
		goto out;

	/* parse values */
	lab->L = g_ascii_strtod (cd_dom_get_node_data (values[0]), &endptr);
	if (endptr != NULL && endptr[0] != '\0')
		goto out;
	lab->a = g_ascii_strtod (cd_dom_get_node_data (values[1]), &endptr);
	if (endptr != NULL && endptr[0] != '\0')
		goto out;
	lab->b = g_ascii_strtod (cd_dom_get_node_data (values[2]), &endptr);
	if (endptr != NULL && endptr[0] != '\0')
		goto out;

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cd_dom_get_node_rgb:
 * @node: a #GNode
 * @rgb: a #CdColorRGB
 *
 * Extracts a RGB color value from the DOM tree
 *
 * Return value: %TRUE if the color was parsed successfully
 *
 * Since: 0.1.31
 **/
gboolean
cd_dom_get_node_rgb (const GNode *node, CdColorRGB *rgb)
{
	gboolean ret = FALSE;
	const GNode *values[3];
	gchar *endptr = NULL;

	/* find nodes */
	values[0] = cd_dom_get_child_node (node, "R");
	if (values[0] == NULL)
		goto out;
	values[1] = cd_dom_get_child_node (node, "G");
	if (values[1] == NULL)
		goto out;
	values[2] = cd_dom_get_child_node (node, "B");
	if (values[2] == NULL)
		goto out;

	/* parse values */
	rgb->R = g_ascii_strtod (cd_dom_get_node_data (values[0]), &endptr);
	if (endptr != NULL && endptr[0] != '\0')
		goto out;
	rgb->G = g_ascii_strtod (cd_dom_get_node_data (values[1]), &endptr);
	if (endptr != NULL && endptr[0] != '\0')
		goto out;
	rgb->B = g_ascii_strtod (cd_dom_get_node_data (values[2]), &endptr);
	if (endptr != NULL && endptr[0] != '\0')
		goto out;

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cd_dom_get_node_yxy:
 * @node: a #GNode
 * @yxy: a #CdColorYxy
 *
 * Extracts a Yxy color value from the DOM tree
 *
 * Return value: %TRUE if the color was parsed successfully
 *
 * Since: 0.1.31
 **/
gboolean
cd_dom_get_node_yxy (const GNode *node, CdColorYxy *yxy)
{
	gboolean ret = FALSE;
	const GNode *values[3];
	gchar *endptr = NULL;

	/* find nodes */
	values[0] = cd_dom_get_child_node (node, "Y");
	if (values[0] == NULL)
		goto out;
	values[1] = cd_dom_get_child_node (node, "x");
	if (values[1] == NULL)
		goto out;
	values[2] = cd_dom_get_child_node (node, "y");
	if (values[2] == NULL)
		goto out;

	/* parse values */
	yxy->Y = g_ascii_strtod (cd_dom_get_node_data (values[0]), &endptr);
	if (endptr != NULL && endptr[0] != '\0')
		goto out;
	yxy->x = g_ascii_strtod (cd_dom_get_node_data (values[1]), &endptr);
	if (endptr != NULL && endptr[0] != '\0')
		goto out;
	yxy->y = g_ascii_strtod (cd_dom_get_node_data (values[2]), &endptr);
	if (endptr != NULL && endptr[0] != '\0')
		goto out;

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cd_dom_get_node_localized:
 * @node: a #GNode
 * @key: the key to use, e.g. "copyright"
 *
 * Extracts localized values from the DOM tree
 *
 * Return value: (transfer full): A hash table with the locale (e.g. en_GB) as the key
 *
 * Since: 0.1.31
 **/
GHashTable *
cd_dom_get_node_localized (const GNode *node, const gchar *key)
{
	CdDomNodeData *data;
	const gchar *xml_lang;
	const gchar *data_unlocalized;
	const gchar *data_localized;
	GHashTable *hash = NULL;
	GNode *tmp;

	/* does it exist? */
	tmp = cd_dom_get_child_node (node, key);
	if (tmp == NULL)
		goto out;
	data_unlocalized = cd_dom_get_node_data (tmp);

	/* find a node called name */
	hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	for (tmp = node->children; tmp != NULL; tmp = tmp->next) {
		data = tmp->data;
		if (data == NULL)
			continue;
		if (g_strcmp0 (data->name, key) != 0)
			continue;

		/* avoid storing identical strings */
		xml_lang = g_hash_table_lookup (data->attributes, "xml:lang");
		data_localized = data->cdata->str;
		if (xml_lang != NULL && g_strcmp0 (data_unlocalized, data_localized) == 0)
			continue;
		g_hash_table_insert (hash,
				     g_strdup (xml_lang != NULL ? xml_lang : ""),
				     g_strdup (data_localized));
	}
out:
	return hash;
}

/**
 * cd_dom_destroy_node_cb:
 **/
static gboolean
cd_dom_destroy_node_cb (GNode *node, gpointer user_data)
{
	CdDomNodeData *data = node->data;
	if (data == NULL)
		goto out;
	g_free (data->name);
	g_string_free (data->cdata, TRUE);
	g_hash_table_unref (data->attributes);
	g_slice_free (CdDomNodeData, data);
out:
	return FALSE;
}

/**
 * cd_dom_class_init:
 */
static void
cd_dom_class_init (CdDomClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_dom_finalize;
	g_type_class_add_private (klass, sizeof (CdDomPrivate));
}

/**
 * cd_dom_init:
 */
static void
cd_dom_init (CdDom *dom)
{
	dom->priv = CD_DOM_GET_PRIVATE (dom);
	dom->priv->root = g_node_new (NULL);
	dom->priv->current = dom->priv->root;
}

/**
 * cd_dom_finalize:
 */
static void
cd_dom_finalize (GObject *object)
{
	CdDom *dom = CD_DOM (object);

	g_node_traverse (dom->priv->root,
			 G_PRE_ORDER,
			 G_TRAVERSE_ALL,
			 -1,
			 cd_dom_destroy_node_cb,
			 NULL);
	g_node_destroy (dom->priv->root);

	G_OBJECT_CLASS (cd_dom_parent_class)->finalize (object);
}

/**
 * cd_dom_new:
 *
 * Creates a new #CdDom object.
 *
 * Return value: a new CdDom object.
 *
 * Since: 0.1.31
 **/
CdDom *
cd_dom_new (void)
{
	CdDom *dom;
	dom = g_object_new (CD_TYPE_DOM, NULL);
	return CD_DOM (dom);
}

