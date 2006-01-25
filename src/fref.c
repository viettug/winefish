/* $Id: fref.c,v 1.1.1.1 2005/06/29 11:03:23 kyanh Exp $ */
/* 
 * fref.c - function reference file
 * Modified for Winefish (C) 2005 Ky Anh <kyanh@o2.pl>
 * 
 *
 * indent --line-length 100 --k-and-r-style --tab-size 4 -bbo --ignore-newlines 
 */

#include <gtk/gtk.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>

/* #define DEBUG */
#include "bluefish.h"
#include "fref.h"
#include "rcfile.h"				/* array_from_arglist() */
#include "stringlist.h"
#include "document.h"
#include "bf_lib.h"
#include "gtk_easy.h"
#include "char_table.h"
#include "treetips.h"

enum {
	FREF_ACTION_INSERT,
	FREF_ACTION_DIALOG,
	FREF_ACTION_INFO
};

enum {
	FREF_IT_DESC,
	FREF_IT_ATTRS,
	FREF_IT_NOTES
};

enum {
	STR_COLUMN,
	PTR_COLUMN,
	FILE_COLUMN,
	N_COLUMNS
};

#define FR_TYPE_TAG				   1
#define FR_TYPE_FUNCTION			2
#define FR_TYPE_CLASS				3
#define FR_TYPE_GROUP				4

#define MAX_NEST_LEVEL			20

typedef struct {
	gpointer data;
	Tbfwin *bfwin;
} Tcallbackdata;

typedef struct {
	gchar *name;
	gchar *title;
	gchar *description;
	gchar *def_value;
	gboolean required;
	gboolean has_list;
	gchar *values;
	GtkWidget *dlg_item;
} FRAttrInfo;

typedef struct {
	gchar *name;
	gchar *title;
	gchar *description;
	gchar *def_value;
	gchar *type;
	gboolean required;
	gboolean has_list;
	gchar *values;
	GtkWidget *dlg_item;
} FRParamInfo;


typedef struct {
	gchar type;
	gchar *name;
	gchar *description;
	gchar *tip;
	gchar *return_type;			/* if function */
	gchar *return_description;	/* if function */
	GList *attributes;
	GList *params;
	GList *methods;
	gchar *info_text;
	gchar *info_title;
	gchar *dialog_text;
	gchar *dialog_title;
	gchar *insert_text;
} FRInfo;

typedef struct {
	FRInfo *act_info;
	FRInfo *act_grp;
	FRAttrInfo *act_attr;
	FRParamInfo *act_param;
	GtkWidget *tree;
	GtkTreeStore *store;
	GtkTreeIter grp_parent[MAX_NEST_LEVEL];
	gint nest_level;
	GtkTreeIter parent;
	gint state;
	gint pstate;
	gint vstate;
	GHashTable *dict;
} FRParseAux;

#define FR_LOADER_STATE_NONE            1
#define FR_LOADER_STATE_TAG             2
#define FR_LOADER_STATE_FUNC            3
#define FR_LOADER_STATE_CLASS           4
#define FR_LOADER_STATE_ATTR            5
#define FR_LOADER_STATE_PARAM           6
#define FR_LOADER_STATE_TIP             7
#define FR_LOADER_STATE_DESCR           8
#define FR_LOADER_STATE_INFO            9
#define FR_LOADER_STATE_DIALOG          10
#define FR_LOADER_STATE_INSERT          11
#define FR_LOADER_STATE_VALLIST         12
#define FR_LOADER_STATE_RETURN          13
#define FR_LOADER_STATE_GROUPDESC       14

#define FR_INFO_TITLE			1
#define FR_INFO_DESC			  2
#define FR_INFO_ATTRS			3
#define FR_INFO_NOTES			4

#define FR_COL_1 			"#4B6983"
#define FR_COL_2 			"#7590AE"
#define FR_COL_3 			"#666666"
#define FR_COL_4 			"#FFFFFF"

GtkWidget *fref_prepare_dialog(Tbfwin * bfwin, FRInfo * entry);

typedef struct {
	GtkWidget *tree;
	GtkTooltips *argtips;
	GtkWidget *infocheck;
	GtkWidget *infoview;
	GtkWidget *infoscroll;
	Tbfwin *bfwin;
	TreeTips *tips;
} Tfref_gui;

#define FREFGUI(var) ((Tfref_gui *)(var))

typedef struct {
	GtkTreeStore *store;
	GHashTable *refcount;		/* Opened reference count */
} Tfref_data;
#define FREFDATA(var) ((Tfref_data *)(var))

typedef struct {
	gchar *name;
	gchar *description;
} Tfref_name_data;

#ifdef DEBUG
static void free_with_print(gpointer ptr)
{
	DEBUG_MSG("free_wp: free'ing %p\n", ptr);
	g_free(ptr);
}
#endif
void fref_free_info(FRInfo * info)
{
	FRAttrInfo *tmpa;
	FRParamInfo *tmpp;
	GList *lst;
	DEBUG_MSG("fref_free_info, freeing info->name=%s\n", info->name);
	g_free(info->name);
	g_free(info->description);
	g_free(info->tip);
	g_free(info->return_type);
	g_free(info->return_description);
	g_free(info->info_text);
	g_free(info->info_title);
	g_free(info->dialog_text);
	g_free(info->dialog_title);
	g_free(info->insert_text);
	if (info->attributes) {
		lst = g_list_first(info->attributes);
		while (lst) {
			tmpa = (FRAttrInfo *) lst->data;
			g_free(tmpa->name);
			g_free(tmpa->title);
			g_free(tmpa->description);
			g_free(tmpa->def_value);
			g_free(tmpa->values);
			g_free(tmpa);
			lst = g_list_next(lst);
		}
		g_list_free(info->attributes);
	}							/* attributes */
	if (info->params) {
		lst = g_list_first(info->params);
		while (lst) {
			tmpp = (FRParamInfo *) lst->data;
			g_free(tmpp->name);
			g_free(tmpp->title);
			g_free(tmpp->description);
			g_free(tmpp->def_value);
			g_free(tmpp->type);
			g_free(tmpp->values);
			g_free(tmpp);
			lst = g_list_next(lst);
		}
		g_list_free(info->params);
	}
	/* params */
	/* methods */
	g_free(info);
}

void fref_name_loader_start_element(GMarkupParseContext * context, const gchar * element_name,
									const gchar ** attribute_names, const gchar ** attribute_values,
									gpointer user_data, GError ** error)
{
	GHashTable *attrs;
	int i;
	Tfref_name_data *data;
	gchar *tmps;

	if (user_data == NULL)
		return;
	data = (Tfref_name_data *) user_data;

	attrs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	i = 0;
	while (attribute_names[i] != NULL) {
		g_hash_table_insert(attrs, (gpointer) g_strdup(attribute_names[i]),
							(gpointer) g_strdup(attribute_values[i]));
		i++;
	}

	if (strcmp(element_name, "ref") == 0) {
		tmps = g_hash_table_lookup(attrs, "name");
		if (tmps != NULL)
			data->name = g_strdup(tmps);
		tmps = g_hash_table_lookup(attrs, "description");
		if (tmps != NULL)
			data->description = g_strdup(tmps);
	}
	g_hash_table_destroy(attrs);
}

void fref_loader_start_element(GMarkupParseContext * context, const gchar * element_name,
							   const gchar ** attribute_names, const gchar ** attribute_values,
							   gpointer user_data, GError ** error)
{
	FRParseAux *aux;
	FRInfo *info;
	FRAttrInfo *tmpa;
	FRParamInfo *tmpp;
	GHashTable *attrs;
	int i;
	gpointer pomstr;
	GtkTreeIter iter;
	GtkTreeRowReference *rref;
	GtkTreePath *path = NULL;
	gchar *string = NULL;

	if (user_data == NULL)
		return;
	aux = (FRParseAux *) user_data;
	attrs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	i = 0;
	while (attribute_names[i] != NULL) {
		g_hash_table_insert(attrs, (gpointer) g_strdup(attribute_names[i]),
							(gpointer) g_strdup(attribute_values[i]));
		i++;
	}

	switch (aux->state) {

	case FR_LOADER_STATE_NONE:
		if (strcmp(element_name, "group") == 0) {
			if (aux->nest_level < MAX_NEST_LEVEL) {
				info = g_new0(FRInfo, 1);
				info->type = FR_TYPE_GROUP;
				info->name = g_strdup(g_hash_table_lookup(attrs, "name"));
				gtk_tree_store_append(aux->store, &iter, &aux->parent);
				string = g_strdup(g_hash_table_lookup(attrs, "name"));
				gtk_tree_store_set(aux->store, &iter, STR_COLUMN,
								   string, FILE_COLUMN, NULL, PTR_COLUMN, info, -1);
				g_free (string);
				aux->grp_parent[aux->nest_level] = aux->parent;
				aux->parent = iter;
			} else
				g_warning("Maximum nesting level reached!");
			(aux->nest_level)++;
		} else if (strcmp(element_name, "description") == 0) {	/* group description */
			FRInfo *tmpinfo;
			gtk_tree_model_get(GTK_TREE_MODEL(aux->store), &aux->parent, PTR_COLUMN, &tmpinfo, -1);
			if (tmpinfo != NULL) {
				aux->pstate = FR_LOADER_STATE_NONE;
				aux->state = FR_LOADER_STATE_GROUPDESC;
			}
		} else if (strcmp(element_name, "tag") == 0) {
			aux->state = FR_LOADER_STATE_TAG;
			aux->pstate = FR_LOADER_STATE_NONE;
			info = g_new0(FRInfo, 1);
			info->type = FR_TYPE_TAG;
			info->name = g_strdup(g_hash_table_lookup(attrs, "name"));
			gtk_tree_store_append(aux->store, &iter, &aux->parent);
			gtk_tree_store_set(aux->store, &iter, STR_COLUMN, info->name, PTR_COLUMN, info,
							   FILE_COLUMN, NULL, -1);
			aux->act_info = info;
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(aux->store), &iter);
			rref = gtk_tree_row_reference_new(GTK_TREE_MODEL(aux->store), path);
			g_hash_table_insert(aux->dict, g_strdup(info->name), rref);
		} else if (strcmp(element_name, "function") == 0) {
			aux->state = FR_LOADER_STATE_FUNC;
			aux->pstate = FR_LOADER_STATE_NONE;
			info = g_new0(FRInfo, 1);
			info->type = FR_TYPE_FUNCTION;
			info->name = g_strdup(g_hash_table_lookup(attrs, "name"));
			gtk_tree_store_append(aux->store, &iter, &aux->parent);
			gtk_tree_store_set(aux->store, &iter, STR_COLUMN, info->name, PTR_COLUMN, info, -1);
			aux->act_info = info;
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(aux->store), &iter);
			rref = gtk_tree_row_reference_new(GTK_TREE_MODEL(aux->store), path);
			g_hash_table_insert(aux->dict, g_strdup(info->name), rref);
		} else if (strcmp(element_name, "class") == 0) {
			aux->state = FR_LOADER_STATE_CLASS;
			aux->pstate = FR_LOADER_STATE_NONE;
			info = g_new0(FRInfo, 1);
			info->type = FR_TYPE_CLASS;
			info->name = g_strdup(g_hash_table_lookup(attrs, "name"));
			gtk_tree_store_append(aux->store, &iter, &aux->parent);
			gtk_tree_store_set(aux->store, &iter, STR_COLUMN, info->name, PTR_COLUMN, info, -1);
			aux->act_info = info;
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(aux->store), &iter);
			rref = gtk_tree_row_reference_new(GTK_TREE_MODEL(aux->store), path);
			g_hash_table_insert(aux->dict, g_strdup(info->name), rref);
		} else if (strcmp(element_name, "ref") == 0) {
		} else
			g_warning("FREF Config Error: Unknown element %s", element_name);
		break;					/* state NONE */


	case FR_LOADER_STATE_TAG:
		if (strcmp(element_name, "description") == 0) {
			aux->state = FR_LOADER_STATE_DESCR;
			aux->pstate = FR_LOADER_STATE_TAG;
		} else if (strcmp(element_name, "tip") == 0) {
			aux->state = FR_LOADER_STATE_TIP;
			aux->pstate = FR_LOADER_STATE_TAG;
		} else if (strcmp(element_name, "attribute") == 0) {
			aux->state = FR_LOADER_STATE_ATTR;
			aux->pstate = FR_LOADER_STATE_TAG;
			tmpa = g_new0(FRAttrInfo, 1);
			tmpa->name = g_strdup(g_hash_table_lookup(attrs, "name"));
			tmpa->title = g_strdup(g_hash_table_lookup(attrs, "title"));
			pomstr = g_hash_table_lookup(attrs, "required");
			if (pomstr != NULL
				&& (g_strcasecmp(pomstr, "y") == 0 || g_strcasecmp(pomstr, "1") == 0))
				tmpa->required = TRUE;
			else
				tmpa->required = FALSE;
			pomstr = g_hash_table_lookup(attrs, "vallist");
			if (pomstr != NULL
				&& (g_strcasecmp(pomstr, "y") == 0 || g_strcasecmp(pomstr, "1") == 0))
				tmpa->has_list = TRUE;
			else
				tmpa->has_list = FALSE;
			tmpa->def_value = g_strdup(g_hash_table_lookup(attrs, "default"));
			aux->act_attr = tmpa;
			aux->act_info->attributes = g_list_append(aux->act_info->attributes, tmpa);
		} else if (strcmp(element_name, "dialog") == 0) {
			aux->state = FR_LOADER_STATE_DIALOG;
			aux->pstate = FR_LOADER_STATE_TAG;
			aux->act_info->dialog_title = g_strdup(g_hash_table_lookup(attrs, "title"));
		} else if (strcmp(element_name, "info") == 0) {
			aux->state = FR_LOADER_STATE_INFO;
			aux->pstate = FR_LOADER_STATE_TAG;
			aux->act_info->info_title = g_strdup(g_hash_table_lookup(attrs, "title"));
		} else if (strcmp(element_name, "insert") == 0) {
			aux->state = FR_LOADER_STATE_INSERT;
			aux->pstate = FR_LOADER_STATE_TAG;
		} else
			g_warning("FREF Config Error: Unknown tag (%s)", element_name);
		break;					/* state TAG */
	case FR_LOADER_STATE_FUNC:
		if (strcmp(element_name, "description") == 0) {
			aux->state = FR_LOADER_STATE_DESCR;
			aux->pstate = FR_LOADER_STATE_FUNC;
		} else if (strcmp(element_name, "tip") == 0) {
			aux->state = FR_LOADER_STATE_TIP;
			aux->pstate = FR_LOADER_STATE_FUNC;
		} else if (strcmp(element_name, "param") == 0) {
			aux->state = FR_LOADER_STATE_PARAM;
			aux->pstate = FR_LOADER_STATE_FUNC;
			tmpp = g_new0(FRParamInfo, 1);
			tmpp->name = g_strdup(g_hash_table_lookup(attrs, "name"));
			tmpp->title = g_strdup(g_hash_table_lookup(attrs, "title"));
			pomstr = g_hash_table_lookup(attrs, "required");
			if (pomstr != NULL
				&& (g_strcasecmp(pomstr, "y") == 0 || g_strcasecmp(pomstr, "1") == 0))
				tmpp->required = TRUE;
			else
				tmpp->required = FALSE;
			pomstr = g_hash_table_lookup(attrs, "vallist");
			if (pomstr != NULL
				&& (g_strcasecmp(pomstr, "y") == 0 || g_strcasecmp(pomstr, "1") == 0))
				tmpp->has_list = TRUE;
			else
				tmpp->has_list = FALSE;
			tmpp->def_value = g_strdup(g_hash_table_lookup(attrs, "default"));
			tmpp->type = g_strdup(g_hash_table_lookup(attrs, "type"));
			aux->act_param = tmpp;
			aux->act_info->params = g_list_append(aux->act_info->params, tmpp);
		} else if (strcmp(element_name, "return") == 0) {
			aux->state = FR_LOADER_STATE_RETURN;
			aux->pstate = FR_LOADER_STATE_FUNC;
			aux->act_info->return_type = g_strdup(g_hash_table_lookup(attrs, "type"));
		} else if (strcmp(element_name, "dialog") == 0) {
			aux->state = FR_LOADER_STATE_DIALOG;
			aux->pstate = FR_LOADER_STATE_FUNC;
			aux->act_info->dialog_title = g_strdup(g_hash_table_lookup(attrs, "title"));
		} else if (strcmp(element_name, "info") == 0) {
			aux->state = FR_LOADER_STATE_INFO;
			aux->pstate = FR_LOADER_STATE_FUNC;
			aux->act_info->info_title = g_strdup(g_hash_table_lookup(attrs, "title"));
		} else if (strcmp(element_name, "insert") == 0) {
			aux->state = FR_LOADER_STATE_INSERT;
			aux->pstate = FR_LOADER_STATE_FUNC;
		} else
			g_warning("FREF Config Error: Unknown tag (%s)", element_name);
		break;					/* state FUNC */
	case FR_LOADER_STATE_ATTR:
		if (strcmp(element_name, "vallist") == 0) {
			aux->state = FR_LOADER_STATE_VALLIST;
			aux->vstate = FR_LOADER_STATE_ATTR;
		} else
			g_warning("FREF Config Error: Unknown tag (%s)", element_name);
		break;					/* state ATTR */
	case FR_LOADER_STATE_PARAM:
		if (strcmp(element_name, "vallist") == 0) {
			aux->state = FR_LOADER_STATE_VALLIST;
			aux->vstate = FR_LOADER_STATE_PARAM;
		} else
			g_warning("FREF Config Error: Unknown tag (%s)", element_name);
		break;					/* state PARAM */
	}							/* switch */

	if (path) gtk_tree_path_free (path);
	g_hash_table_destroy(attrs);
}

void fref_loader_end_element(GMarkupParseContext * context, const gchar * element_name,
							 gpointer user_data, GError ** error)
{
	FRParseAux *aux;

	if (user_data == NULL)
		return;
	aux = (FRParseAux *) user_data;
	switch (aux->state) {
	case FR_LOADER_STATE_TAG:
		if (strcmp(element_name, "tag") == 0) {
			aux->act_info = NULL;
			aux->pstate = FR_LOADER_STATE_NONE;
		}
		break;					/* tag */
	case FR_LOADER_STATE_FUNC:
		if (strcmp(element_name, "function") == 0) {
			aux->act_info = NULL;
			aux->pstate = FR_LOADER_STATE_NONE;
		}
		break;					/* function */
	case FR_LOADER_STATE_ATTR:
		if (strcmp(element_name, "attribute") == 0) {
			aux->act_attr = NULL;
			aux->pstate = FR_LOADER_STATE_TAG;
		}
		break;					/* attribute */
	case FR_LOADER_STATE_PARAM:
		if (strcmp(element_name, "param") == 0) {
			aux->act_param = NULL;
			aux->pstate = FR_LOADER_STATE_FUNC;
		}
		break;					/* param */
	}							/* switch */

	if (aux->state != FR_LOADER_STATE_VALLIST)
		aux->state = aux->pstate;
	else
		aux->state = aux->vstate;

	if (strcmp(element_name, "group") == 0) {
		if (aux->nest_level > 0) {
			(aux->nest_level)--;
			if (aux->nest_level < MAX_NEST_LEVEL)
				aux->parent = aux->grp_parent[aux->nest_level];
		}
	}

}

void fref_loader_text(GMarkupParseContext * context, const gchar * _text, gsize _text_len,
					  gpointer user_data, GError ** error)
{
	FRParseAux *aux;
	gchar *text;
	gint text_len;
	FRInfo *tmpinfo;

	if (user_data == NULL && _text == NULL)
		return;
	/* remove white spaces from the begining and the end */
	text = g_strdup(_text);
	text = g_strstrip(text);
	text_len = strlen(text);

	aux = (FRParseAux *) user_data;
	switch (aux->state) {
	case FR_LOADER_STATE_DESCR:
		aux->act_info->description = g_strndup(text, text_len);
		break;
	case FR_LOADER_STATE_TIP:
		aux->act_info->tip = g_strndup(text, text_len);
		break;
	case FR_LOADER_STATE_ATTR:
		aux->act_attr->description = g_strndup(text, text_len);
		break;
	case FR_LOADER_STATE_PARAM:
		aux->act_param->description = g_strndup(text, text_len);
		break;
	case FR_LOADER_STATE_RETURN:
		aux->act_info->return_description = g_strndup(text, text_len);
		break;
	case FR_LOADER_STATE_VALLIST:
		if (aux->vstate == FR_LOADER_STATE_ATTR)
			aux->act_attr->values = g_strndup(text, text_len);
		else if (aux->vstate == FR_LOADER_STATE_PARAM)
			aux->act_param->values = g_strndup(text, text_len);
		else
			g_warning("FREF Config Error: cannot assign vallist");
		break;
	case FR_LOADER_STATE_DIALOG:
		aux->act_info->dialog_text = g_strndup(text, text_len);
		break;
	case FR_LOADER_STATE_INFO:
		aux->act_info->info_text = g_strndup(text, text_len);
		break;
	case FR_LOADER_STATE_INSERT:
		aux->act_info->insert_text = g_strndup(text, text_len);
		break;
	case FR_LOADER_STATE_GROUPDESC:
		gtk_tree_model_get(GTK_TREE_MODEL(aux->store), &aux->parent, PTR_COLUMN, &tmpinfo, -1);
		if (tmpinfo != NULL) {
			/* BUG?? this appears as if it is freed in fref_free_info()
			 * but a small memory leak is shown when opening the HTML ref
			 */
			tmpinfo->description = g_strndup(text, text_len);
		}
		break;
	}							/* switch */
	g_free(text);
}

void fref_loader_error(GMarkupParseContext * context, GError * error, gpointer user_data)
{
	if (error != NULL)
		g_warning("FREF Config Error: %s", error->message);
	else
		g_warning("FREF Config Error: Unknown Error");
}

/* CONFIG FILE PARSER */

static GMarkupParser FRParser = {
	fref_loader_start_element,
	fref_loader_end_element,
	fref_loader_text,
	NULL,
	fref_loader_error
};

/* AUXILIARY FILE PARSER */

static GMarkupParser FRNameParser = {
	fref_name_loader_start_element,
	NULL,
	NULL,
	NULL,
	fref_loader_error
};

gchar *fref_xml_get_refname(gchar * filename)
{
	GMarkupParseContext *ctx;
	gchar *config;
	gsize len;
	Tfref_name_data *aux = NULL;
	gchar *tmps;

	if (filename == NULL)
		return NULL;

	aux = g_new0(Tfref_name_data, 1);
	ctx = g_markup_parse_context_new(&FRNameParser, (GMarkupParseFlags) 0, (gpointer) aux, NULL);
	if (ctx == NULL) {
		g_free(aux);
		return NULL;
	}
	if (!g_file_get_contents(filename, &config, &len, NULL)) {
		g_markup_parse_context_free(ctx);
		g_free(aux);
		return NULL;
	}

	if (!g_markup_parse_context_parse(ctx, config, len, NULL)) {
		g_markup_parse_context_free(ctx);
		g_free(aux);
		g_free(config);
		return NULL;
	}
	g_markup_parse_context_free(ctx);
	tmps = aux->name;
	g_free(aux->description);
	g_free(aux);
	g_free(config);
	return tmps;
}



void fref_loader_load_ref_xml(gchar * filename, GtkWidget * tree, GtkTreeStore * store,
							  GtkTreeIter * parent, GHashTable * dict)
{
	GMarkupParseContext *ctx;
	gchar *config;
	gsize len;
	FRParseAux *aux;

	if (filename == NULL)
		return;
	aux = g_new0(FRParseAux, 1);
	aux->tree = tree;
	aux->store = store;
	aux->state = FR_LOADER_STATE_NONE;
	aux->parent = *parent;
	aux->nest_level = 0;
	aux->dict = dict;

	ctx = g_markup_parse_context_new(&FRParser, (GMarkupParseFlags) 0, (gpointer) aux, NULL);
	if (ctx == NULL)
		return;
	if (!g_file_get_contents(filename, &config, &len, NULL)) {
		g_markup_parse_context_free(ctx);
		g_free(aux);
		return;
	}
	if (!g_markup_parse_context_parse(ctx, config, len, NULL)) {
		g_markup_parse_context_free(ctx);
		g_free(aux);
		g_free(config);
		return;
	}
	g_markup_parse_context_free(ctx);
	if (aux->act_info)
		fref_free_info(aux->act_info);
	if (aux->act_attr) {
		g_free(aux->act_attr->name);
		g_free(aux->act_attr->title);
		g_free(aux->act_attr->description);
		g_free(aux->act_attr->def_value);
		g_free(aux->act_attr->values);
		g_free(aux->act_attr);
	}
	if (aux->act_param) {
		g_free(aux->act_param->name);
		g_free(aux->act_param->title);
		g_free(aux->act_param->description);
		g_free(aux->act_param->def_value);
		g_free(aux->act_param->type);
		g_free(aux->act_param->values);
		g_free(aux->act_param);
	}
	g_free(aux);
	g_free(config);
}

void fref_loader_unload_ref(GtkTreeStore * store, GtkTreeIter * position)
{
	GtkTreeIter iter;
	GtkTreePath *path;

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), position);
	DEBUG_MSG("fref_loader_unload_ref, called for path %s\n", gtk_tree_path_to_string(path));
	while (gtk_tree_model_iter_children(GTK_TREE_MODEL(store), &iter, position)) {
		FRInfo *entry;
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, PTR_COLUMN, &entry, -1);
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &iter)) {
			fref_loader_unload_ref(store, &iter);
			/* have not to free entry name, because this is a pointer to info field */
		}
		/* this is very crappy... on my OpenBSD box gtk_tree_store_remove() has a different
		   prototype, and it does *not* return a boolean, but just void, that means we can't 
		   use it to step trough the tree... */
		gtk_tree_store_remove(store, &iter);
		if (entry != NULL) {
			fref_free_info(entry);
		}
	}							/* while */

	iter = *position;
	if (gtk_tree_path_get_depth(path) > 1) {
		gchar *name;
		DEBUG_MSG("fref_loader_unload_ref, path depth > 0\n");
		/*gtk_tree_model_get_iter(GTK_TREE_MODEL(store),&iter,path); */
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, STR_COLUMN, &name, -1);
		if (name) {
			/* have to free name column for group */
			DEBUG_MSG("fref_loader_unload_ref, free'ing %s\n", name);
			g_free(name);		/* freeing item name */
		}
	} else {					/* freeing search dictionary */
		GHashTable *dict;
		DEBUG_MSG("fref_loader_unload_ref, path depth == 0, this is the top node\n");
		/*gtk_tree_model_get_iter(GTK_TREE_MODEL(store),&iter,path);  */
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, PTR_COLUMN, &dict, -1);
		if (dict) {
			DEBUG_MSG("fref_loader_unload_ref, destroying search hashtable %p\n", dict);
			g_hash_table_destroy(dict);
			gtk_tree_store_set(store, &iter, PTR_COLUMN, NULL, -1);
		}
	}
	gtk_tree_path_free(path);
	DEBUG_MSG("fref_loader_unload, done\n");
}

void fref_loader_unload_all(GtkWidget * tree, GtkTreeStore * store)
{

	GtkTreeIter iter;
	gint *cnt = NULL;
	gpointer *aux;
	gboolean do_unload = FALSE;

	DEBUG_MSG("fref_loader_unload_all, started for tree=%p, store=%p\n", tree, store);
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, 0)) {
		GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iter);
		if (gtk_tree_path_get_depth(path) == 1) {
			gchar *cat;
			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, STR_COLUMN, &cat, -1);
			DEBUG_MSG("fref_loader_unload_all, cat=%s\n", cat);
			aux = g_hash_table_lookup(FREFDATA(main_v->frefdata)->refcount, cat);
			if (cat) 
				g_free (cat);
				
			if (aux != NULL) {
				cnt = (gint *) aux;
				*cnt = (*cnt) - 1;
				if (*cnt <= 0)
					do_unload = TRUE;
				else
					do_unload = FALSE;
				DEBUG_MSG("fref_loader_unload_all, cnt=%d, do_unload=%d\n", *cnt, do_unload);
			} else
				do_unload = FALSE;				
		} else
			do_unload = FALSE;
		if (do_unload) {
			gchar *file, *name;
			DEBUG_MSG("fref_loader_unload_all, calling fref_loader_unload_ref\n");
			fref_loader_unload_ref(store, &iter);
			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, STR_COLUMN, &name, FILE_COLUMN, &file,
							   -1);
			if (file) {
				DEBUG_MSG("fref_loader_unload_all, freeing %s\n", file);
				g_free(file);
			}
			if (name) {
				DEBUG_MSG("fref_loader_unload_all, freeing %s\n", name);
				g_free(name);
			}
			gtk_tree_store_remove(store, &iter);
		}						/* do_unload */
		if (path)
			gtk_tree_path_free (path);
	}							/* while */
	/* gtk_tree_store_clear(store); */
}

/* END OF CONFIG PARSER */

/* GUI */
static void fill_toplevels(Tfref_data * fdata, gboolean empty_first)
{
	GList *reflist;
	gint *cnt;

	if (empty_first) {
		gtk_tree_store_clear(fdata->store);
	}
	/* prepare first nodes - read from configuration data */
	reflist = g_list_first(main_v->props.reference_files);
	while (reflist) {
		gchar **tmparray = reflist->data;
		if (count_array(tmparray) == 2) {
			if (file_exists_and_readable(tmparray[1])) {
				GtkTreeIter iter;
				GtkTreeIter iter2;
				gtk_tree_store_append(fdata->store, &iter, NULL);
				gtk_tree_store_set(fdata->store, &iter, STR_COLUMN, tmparray[0], PTR_COLUMN, NULL,
								   FILE_COLUMN, tmparray[1], -1);
				cnt = g_new0(gint, 1);
				*cnt = 0;
				g_hash_table_replace(fdata->refcount, g_strdup(tmparray[0]), cnt);

				/* dummy node for er display */
				gtk_tree_store_append(fdata->store, &iter2, &iter);
			}
		}
		reflist = g_list_next(reflist);
	}
}

void fref_cleanup(Tbfwin * bfwin)
{
	DEBUG_MSG("fref_cleanup, started for bfwin=%p, refcount at %p\n", bfwin,
			  FREFDATA(main_v->frefdata)->refcount);
	fref_loader_unload_all(FREFGUI(bfwin->fref)->tree, FREFDATA(main_v->frefdata)->store);
/*	g_hash_table_destroy(FREFDATA(main_v->frefdata)->refcount); */
/* Ok I'll not free search dictionary too ... O.S. */
	FREFGUI(bfwin->fref)->tree = NULL;
	FREFGUI(bfwin->fref)->argtips = NULL;
	tree_tips_destroy(FREFGUI(bfwin->fref)->tips);
	g_free (bfwin->fref);
	bfwin->fref = NULL;
}

/* fref_init is ONCE called by bluefish.c to init the fref_data structure */
void fref_init()
{
	Tfref_data *fdata = g_new(Tfref_data, 1);
	fdata->store = gtk_tree_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_STRING);
	fdata->refcount = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	fill_toplevels(fdata, FALSE);
	DEBUG_MSG("fref_init, refcount at %p\n", fdata->refcount);
	main_v->frefdata = fdata;
}

/*------------ PREPARE INFO -----------------------*/
gchar *fref_prepare_info(FRInfo * entry, gint infotype, gboolean use_colors)
{
	gchar *ret, *tofree;
	GList *lst;
	FRAttrInfo *tmpa;
	FRParamInfo *tmpp;

	ret = g_strdup("");
	switch (entry->type) {
	case FR_TYPE_GROUP:
		switch (infotype) {
		case FR_INFO_DESC:
			if (entry->description != NULL) {
				gchar *escaped = xml_escape(entry->description);
				tofree = ret;
				if (use_colors) {

					ret =
						g_strconcat(ret, "<span size=\"small\"  foreground=\"#FFFFFF\" >       ",
									escaped, "</span>", NULL);
				} else {
					ret =
						g_strconcat(ret, "<span size=\"small\">       ", escaped, "</span>", NULL);
				}
				g_free(tofree);
				g_free(escaped);
			}
			break;
		}
		break;
	case FR_TYPE_TAG:
		switch (infotype) {
		case FR_INFO_TITLE:
			tofree = ret;
			if (use_colors)
				ret =
					g_strconcat(ret, "<span size=\"medium\" foreground=\"#FFFFFF\" >  TAG: <b>",
								entry->name, "</b></span>", NULL);
			else
				ret =
					g_strconcat(ret, "<span size=\"medium\">  TAG: <b>", entry->name, "</b></span>",
								NULL);
			g_free(tofree);
			break;
		case FR_INFO_DESC:
			if (entry->description != NULL) {
				gchar *escaped = xml_escape(entry->description);
				tofree = ret;
				if (use_colors)
					ret =
						g_strconcat(ret, "<span size=\"small\"  foreground=\"#FFFFFF\" >       ",
									escaped, "</span>", NULL);
				else
					ret =
						g_strconcat(ret, "<span size=\"small\">       ", escaped, "</span>", NULL);
				g_free(tofree);
				g_free(escaped);
			}
			break;
		case FR_INFO_NOTES:
			if (entry->info_text != NULL) {
				gchar *escaped = xml_escape(entry->info_text);
				tofree = ret;

				if (use_colors)
					ret =
						g_strconcat(ret, "<span size=\"medium\" foreground=\"", FR_COL_4,
									"\" ><b>NOTES:</b> \n", escaped, "</span>", NULL);
				else
					ret =
						g_strconcat(ret, "<span size=\"medium\"><b>NOTES:</b> \n", escaped,
									"</span>", NULL);
				g_free(tofree);
				g_free(escaped);
			}
			break;
		case FR_INFO_ATTRS:
			lst = g_list_first(entry->attributes);
			while (lst) {
				gchar *escaped1, *escaped2;
				tmpa = (FRAttrInfo *) lst->data;
				tofree = ret;
				escaped1 = xml_escape(tmpa->name);
				escaped2 = xml_escape(tmpa->description);
				if (use_colors) 
					
					ret =
						g_strconcat(ret, "<span size=\"small\" >          <b><i>", escaped1,
									"</i></b></span> - <span size=\"small\" foreground=\"",
									FR_COL_3, "\" >", escaped2, "</span>\n", NULL);
				else
					ret =
						g_strconcat(ret, "<span size=\"small\" >          <b><i>", escaped1,
									"</i></b></span> - <span size=\"small\">", escaped2,
									"</span>\n", NULL);
				g_free(escaped1);
				g_free(escaped2);
				g_free(tofree);
				lst = g_list_next(lst);
			}
			break;
		}						/* switch infotype */
		break;					/* TAG */
	case FR_TYPE_FUNCTION:
		switch (infotype) {
		case FR_INFO_TITLE:
			tofree = ret;
			if (use_colors)
				ret =
					g_strconcat(ret,
								"<span size=\"medium\" foreground=\"#FFFFFF\" >  FUNCTION: <b>",
								entry->name, "</b></span>", NULL);
			else
				ret =
					g_strconcat(ret, "<span size=\"medium\">  FUNCTION: <b>", entry->name,
								"</b></span>", NULL);
			g_free(tofree);
			break;
		case FR_INFO_DESC:
			if (entry->description != NULL) {
				gchar *escaped = xml_escape(entry->description);
				tofree = ret;
				if (use_colors)
					ret =
						g_strconcat(ret, "<span size=\"small\" foreground=\"#FFFFFF\" >       <i>",
									escaped, "</i></span>\n", NULL);
				else
					ret =
						g_strconcat(ret, "<span size=\"small\">       <i>", escaped,
									"</i></span>\n", NULL);
				g_free(tofree);
				g_free(escaped);
			}
			if (entry->return_type != NULL) {
				tofree = ret;
				if (use_colors)
					ret =
						g_strconcat(ret, "<span size=\"medium\" foreground=\"", FR_COL_4,
									"\">       <b>RETURNS: ", entry->return_type, "</b></span>\n",
									NULL);
				else
					ret =
						g_strconcat(ret, "<span size=\"medium\">       <b>RETURNS: ",
									entry->return_type, "</b></span>\n", NULL);
				g_free(tofree);
			}
			if (entry->return_description != NULL) {
				tofree = ret;
				if (use_colors)
					ret =
						g_strconcat(ret, "<span size=\"small\" foreground=\"", FR_COL_4,
									"\">       ", entry->return_description, "</span>\n", NULL);
				else
					ret =
						g_strconcat(ret, "<span size=\"small\">       ", entry->return_description,
									"</span>\n", NULL);
				g_free(tofree);
			}
			break;
		case FR_INFO_NOTES:
			if (entry->info_text != NULL) {
				gchar *escaped = xml_escape(entry->info_text);
				tofree = ret;
				if (use_colors)
					ret =
						g_strconcat(ret, "<span size=\"medium\" foreground=\"", FR_COL_4,
									"\"><b>NOTES:</b> \n", escaped, "</span>", NULL);
				else
					ret =
						g_strconcat(ret, "<span size=\"medium\"><b>NOTES:</b> \n", escaped,
									"</span>", NULL);
				g_free(tofree);
				g_free(escaped);
			}
			break;
		case FR_INFO_ATTRS:
			lst = g_list_first(entry->params);
			while (lst) {
				tmpp = (FRParamInfo *) lst->data;
				tofree = ret;
				if (tmpp->description != NULL && tmpp->type != NULL) {
					gchar *escaped1, *escaped2, *escaped3;
					escaped1 = xml_escape(tmpp->name);
					escaped2 = xml_escape(tmpp->type);
					escaped3 = xml_escape(tmpp->description);
					if (use_colors)
						ret =
							g_strconcat(ret, "<span size=\"small\">          <b><i>", escaped1,
										"(", escaped2,
										")</i></b></span> - <span size=\"small\" foreground=\"",
										FR_COL_3, "\">", escaped3, "</span>\n", NULL);
					else
						ret =
							g_strconcat(ret, "<span size=\"small\">          <b><i>", escaped1,
										"(", escaped2, ")</i></b></span> - <span size=\"small\">",
										escaped3, "</span>\n", NULL);
					g_free(escaped1);
					g_free(escaped2);
					g_free(escaped3);
				} else if (tmpp->type != NULL) {
					gchar *escaped1, *escaped2;
					escaped1 = xml_escape(tmpp->name);
					escaped2 = xml_escape(tmpp->type);
					ret =
						g_strconcat(ret, "<span size=\"small\">          <b><i>", escaped1, "(",
									escaped2, ")</i></b></span>\n", NULL);
					g_free(escaped1);
					g_free(escaped2);
				} else {
					gchar *escaped1;
					escaped1 = xml_escape(tmpp->name);
					ret =
						g_strconcat(ret, "<span size=\"small\">          <b><i>", escaped1,
									"</i></b></span>\n", NULL);
					g_free(escaped1);
				}
				g_free(tofree);
				lst = g_list_next(lst);
			}
			break;
		}						/* switch infotype */
		break;
	}
	return ret;
}

/*------------ SHOW INFO -----------------------*/
static void info_window_close_lcb(GtkWidget * widget, gpointer data)
{
	gtk_widget_destroy(data);
}



gchar *fref_prepare_text(FRInfo * entry, GtkWidget * dialog)
{
	/* Here attribute/param names have to be replaced by values from dialog */
	gchar *p, *prev, *stringdup, *tofree;
	gchar *tmp, *dest, *tmp3 = NULL;
	GList *lst;
	gint d1, d2;
	FRAttrInfo *tmpa;
	FRParamInfo *tmpp;
	const gchar *converted = NULL, *tmp2 = NULL;

	dest = g_strdup("");
	stringdup = g_strdup(entry->dialog_text);
	prev = stringdup;
	p = strchr(prev, '%');
	while (p) {
		tmp = dest;
		*p = '\0';
		p++;
		if (*p == '%') {
			converted = "%";
		} else {

			switch (entry->type) {
			case FR_TYPE_TAG:
				d1 = g_ascii_digit_value(*p);
				if (d1 != -1) {
					d2 = g_ascii_digit_value(*(p + 1));
					if (d2 != -1)
						d1 = d1 * 10 + d2;
					lst = g_list_nth(entry->attributes, d1);
					if (lst != NULL) {
						tmpa = (FRAttrInfo *) lst->data;
						if (tmpa->dlg_item) {
							if (GTK_IS_COMBO(tmpa->dlg_item))
								converted =
									gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(tmpa->dlg_item)->entry));
							else
								converted = gtk_entry_get_text(GTK_ENTRY(tmpa->dlg_item));
						} else
							converted = "";
					} else
						converted = "";
				} else if (*p == '_') {	/* non empty attributes */
					lst = g_list_first(entry->attributes);
					tmp3 = g_strdup("");
					while (lst != NULL) {
						tmpa = (FRAttrInfo *) lst->data;
						if (tmpa->dlg_item) {
							if (GTK_IS_COMBO(tmpa->dlg_item))
								tmp2 =
									gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(tmpa->dlg_item)->entry));
							else
								tmp2 = gtk_entry_get_text(GTK_ENTRY(tmpa->dlg_item));
							if (strcmp(tmp2, "") != 0) {
								tofree = tmp3;
								tmp3 = g_strconcat(tmp3, " ", tmpa->name, "=\"", tmp2, "\"", NULL);
								g_free(tofree);
							}
						}
						lst = g_list_next(lst);
					}			/* while */
					converted = tmp3;
				} else if (*p == '~') {	/* non empty attributes and required  */
					lst = g_list_first(entry->attributes);
					tmp3 = g_strdup("");
					while (lst != NULL) {
						tmpa = (FRAttrInfo *) lst->data;
						if (tmpa->dlg_item) {
							if (GTK_IS_COMBO(tmpa->dlg_item))
								tmp2 =
									gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(tmpa->dlg_item)->entry));
							else
								tmp2 = gtk_entry_get_text(GTK_ENTRY(tmpa->dlg_item));
							if (strcmp(tmp2, "") != 0 || tmpa->required) {
								tofree = tmp3;
								tmp3 = g_strconcat(tmp3, " ", tmpa->name, "=\"", tmp2, "\"", NULL);
								g_free(tofree);
							}
						}
						lst = g_list_next(lst);
					}			/* while */
					converted = tmp3;
				}				/* required and non-empty */
				break;
			case FR_TYPE_FUNCTION:
				d1 = g_ascii_digit_value(*p);
				if (d1 != -1) {
					d2 = g_ascii_digit_value(*(p + 1));
					if (d2 != -1)
						d1 = d1 * 10 + d2;
					lst = g_list_nth(entry->params, d1);
					if (lst != NULL) {
						tmpp = (FRParamInfo *) lst->data;
						if (tmpp->dlg_item) {
							if (GTK_IS_COMBO(tmpp->dlg_item))
								converted =
									gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(tmpp->dlg_item)->entry));
							else
								converted = gtk_entry_get_text(GTK_ENTRY(tmpp->dlg_item));
						} else
							converted = "";
					} else
						converted = "";
				}
				break;

			}					/* switch */
		}
		dest = g_strconcat(dest, prev, converted, NULL);
		g_free(tmp);			/* here I free the dest (tmp=dest) */
		g_free(tmp3);
		prev = ++p;
		p = strchr(p, '%');
	}
	tmp = dest;
	dest = g_strconcat(dest, prev, NULL);
	g_free(tmp);
	g_free(stringdup);
	return dest;
}

GList *fref_string_to_list(gchar * string, gchar * delimiter)
{
	GList *lst;
	gchar **arr;
	gint i;

	lst = NULL;
	arr = g_strsplit(string, delimiter, 0);
	i = 0;
	while (arr[i] != NULL) {
		lst = g_list_append(lst, arr[i]);
		i++;
	}
	return lst;
}


/* CALLBACKS */

static GtkWidget *togglemenuitem(GSList * group, gchar * name, gboolean selected,
								 GCallback toggledfunc, gpointer toggleddata)
{
	GtkWidget *retval;
	retval = gtk_radio_menu_item_new_with_label(group, name);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(retval), selected);
	g_signal_connect(GTK_OBJECT(retval), "toggled", toggledfunc, toggleddata);
	DEBUG_MSG("togglemenuitem, created %p and set to %d\n", retval, selected);
	return retval;
}

static void fref_ldblclck_changed(GtkWidget * widget, gpointer data)
{
	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		DEBUG_MSG("fref_ldblclck_changed, set to %d\n", GPOINTER_TO_INT(data));
		main_v->globses.fref_ldoubleclick_action = GPOINTER_TO_INT(data);
	}
}

static void fref_infotype_changed(GtkWidget * widget, gpointer data)
{
	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		DEBUG_MSG("fref_infotype_changed, set to %d\n", GPOINTER_TO_INT(data));
		main_v->globses.fref_info_type = GPOINTER_TO_INT(data);
	}
}

static FRInfo *get_current_entry(Tbfwin * bfwin)
{
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(FREFGUI(bfwin->fref)->tree), &path, &col);
	if (path != NULL) {
		FRInfo *retval = NULL;
		GValue *val;
		GtkTreeIter iter;
		gtk_tree_model_get_iter(gtk_tree_view_get_model(GTK_TREE_VIEW(FREFGUI(bfwin->fref)->tree)),
								&iter, path);
		gtk_tree_path_free(path);
		val = g_new0(GValue, 1);
		gtk_tree_model_get_value(gtk_tree_view_get_model(GTK_TREE_VIEW(FREFGUI(bfwin->fref)->tree)),
								 &iter, 1, val);
		if (G_IS_VALUE(val) && g_value_fits_pointer(val)) {
			retval = (FRInfo *) g_value_peek_pointer(val);
		}
		g_value_unset(val);
		g_free(val);
		return retval;
	}
	return NULL;
}

gboolean frefcb_info_keypress(GtkWidget * widget, GdkEventKey * event, Tcallbackdata * cd)
{
	if (event->keyval == GDK_Escape && cd->data != NULL) {
		gtk_widget_destroy(GTK_WIDGET(cd->data));
		g_free(cd);
		return FALSE;
	}
	return TRUE;
}

static void frefcb_info_close(GtkButton * button, Tcallbackdata * cd)
{
	if (cd->data != NULL) {
		gtk_widget_destroy(GTK_WIDGET(cd->data));
	}
	g_free(cd);
}


static void frefcb_row_expanded(GtkTreeView * treeview, GtkTreeIter * arg1, GtkTreePath * arg2,
								gpointer user_data)
{
	GValue *val;
	GtkTreeIter iter;
	gchar *cat;
	gint *cnt = NULL;
	gpointer *aux;
	gboolean do_load = FALSE;
	GHashTable *dict;

	if (gtk_tree_path_get_depth(arg2) == 1) {
		val = g_new0(GValue, 1);
		gtk_tree_model_get_value(GTK_TREE_MODEL(user_data), arg1, 0, val);
		cat = (gchar *) (g_value_peek_pointer(val));
		aux = g_hash_table_lookup(FREFDATA(main_v->frefdata)->refcount, cat);
		if (aux != NULL) {
			cnt = (gint *) aux;
			*cnt = (*cnt) + 1;
			if (*cnt == 1)
				do_load = TRUE;
			else
				do_load = FALSE;
		} else
			do_load = FALSE;
		g_value_unset (val);
		g_free (val);
	} else
		do_load = FALSE;
	if (do_load) {
		val = g_new0(GValue, 1);
		gtk_tree_model_get_value(GTK_TREE_MODEL(user_data), arg1, 2, val);
		dict =
			g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
								  (GDestroyNotify) gtk_tree_row_reference_free);
		DEBUG_MSG("frefcb_row_expanded, search hash_table at %p\n", dict);
		gtk_tree_store_set(GTK_TREE_STORE(user_data), arg1, PTR_COLUMN, dict, -1);
		if (G_IS_VALUE(val) && g_value_peek_pointer(val) != NULL) {
			fref_loader_load_ref_xml((gchar *) g_value_peek_pointer(val), GTK_WIDGET(treeview),
									 GTK_TREE_STORE(user_data), arg1, dict);
		}

		/* remove dummy */

		if (g_value_peek_pointer(val) != NULL
			&& gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(user_data), &iter, arg1, 0)) {
			gtk_tree_store_remove(GTK_TREE_STORE(user_data), &iter);
		}
		g_value_unset (val);
		g_free (val);
	}
}

static void frefcb_info_dialog(GtkButton * button, Tcallbackdata * cd)
{
	FRInfo *entry;
	GtkWidget *dialog;
	gchar *pomstr;
	gint resp;
	Tbfwin *bfwin = cd->bfwin;

	if (cd->data != NULL) {
		gtk_widget_destroy(GTK_WIDGET(cd->data));
		g_free(cd);
	}
	entry = get_current_entry(bfwin);
	if (entry == NULL)
		return;

	dialog = fref_prepare_dialog(bfwin, entry);
	if (dialog) {
		resp = gtk_dialog_run(GTK_DIALOG(dialog));
		if (resp == GTK_RESPONSE_OK) {
			pomstr = fref_prepare_text(entry, dialog);
			gtk_widget_destroy(dialog);
			doc_insert_two_strings(bfwin->current_document, pomstr, NULL);
			g_free(pomstr);
		} else
			gtk_widget_destroy(dialog);
	}
}

static void frefcb_info_insert(GtkButton * button, Tcallbackdata * cd)
{
	FRInfo *entry;
	Tbfwin *bfwin = cd->bfwin;
	if (cd->data != NULL) {
		gtk_widget_destroy(GTK_WIDGET(cd->data));
		g_free(cd);
	}
	entry = get_current_entry(bfwin);
	if (entry == NULL || entry->insert_text == NULL)
		return;
	doc_insert_two_strings(bfwin->current_document, entry->insert_text, NULL);

}

void fref_show_info(Tbfwin * bfwin, FRInfo * entry, gboolean modal, GtkWidget * parent)
{
	GtkWidget *label_t, *frame, *scroll, *view, *btn1, *btn2, *btn3, *vbox, *hbox, *fourbox,
		*label_d, *label_a, *label_n, *ev_t, *ev_d, *ev_a, *ev_n;
	GtkWidget *info_window;
	Tcallbackdata *cd;
	GdkColor col1, col2, col4;

	cd = g_new(Tcallbackdata, 1);
	gdk_color_parse(FR_COL_1, &col1);
	gdk_color_parse(FR_COL_2, &col2);
	gdk_color_parse(FR_COL_4, &col4);

	label_t = gtk_label_new(fref_prepare_info(entry, FR_INFO_TITLE, TRUE));
	label_d = gtk_label_new(fref_prepare_info(entry, FR_INFO_DESC, TRUE));
	label_a = gtk_label_new(fref_prepare_info(entry, FR_INFO_ATTRS, TRUE));
	label_n = gtk_label_new(fref_prepare_info(entry, FR_INFO_NOTES, TRUE));
	gtk_label_set_use_markup(GTK_LABEL(label_t), TRUE);
	gtk_label_set_use_markup(GTK_LABEL(label_d), TRUE);
	gtk_label_set_use_markup(GTK_LABEL(label_a), TRUE);
	gtk_label_set_use_markup(GTK_LABEL(label_n), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label_t), 0.0, 0.0);
	gtk_misc_set_padding(GTK_MISC(label_t), 5, 5);
	gtk_label_set_line_wrap(GTK_LABEL(label_t), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label_d), 0.0, 0.0);
	gtk_misc_set_padding(GTK_MISC(label_d), 5, 5);
	gtk_label_set_line_wrap(GTK_LABEL(label_d), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label_a), 0.0, 0.0);
	gtk_misc_set_padding(GTK_MISC(label_a), 5, 5);
	gtk_label_set_line_wrap(GTK_LABEL(label_a), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label_n), 0.0, 0.0);
	gtk_misc_set_padding(GTK_MISC(label_n), 5, 5);
	gtk_label_set_line_wrap(GTK_LABEL(label_n), TRUE);
	ev_t = gtk_event_box_new();
	gtk_widget_modify_bg(GTK_WIDGET(ev_t), GTK_STATE_NORMAL, &col1);
	gtk_container_add(GTK_CONTAINER(ev_t), label_t);
	ev_d = gtk_event_box_new();
	gtk_widget_modify_bg(GTK_WIDGET(ev_d), GTK_STATE_NORMAL, &col2);
	gtk_container_add(GTK_CONTAINER(ev_d), label_d);
	ev_a = gtk_event_box_new();
	gtk_widget_modify_bg(GTK_WIDGET(ev_a), GTK_STATE_NORMAL, &col4);
	gtk_container_add(GTK_CONTAINER(ev_a), label_a);
	ev_n = gtk_event_box_new();
	gtk_widget_modify_bg(GTK_WIDGET(ev_n), GTK_STATE_NORMAL, &col1);
	gtk_container_add(GTK_CONTAINER(ev_n), label_n);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	btn2 = NULL;
	btn3 = NULL;
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER,
								   GTK_POLICY_AUTOMATIC);
	view =
		gtk_viewport_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 1024, 1, 100, 200)),
						 GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 768, 1, 100, 200)));
	info_window = window_full2(_("Info"), GTK_WIN_POS_NONE, 0, G_CALLBACK(info_window_close_lcb)
							   , NULL, TRUE, FALSE);

	if (modal) {
		gtk_window_set_modal(GTK_WINDOW(info_window), TRUE);
	}

	fourbox = gtk_vbox_new(FALSE, 0);

	frame = gtk_frame_new(NULL);

	hbox = gtk_hbox_new(TRUE, 5);

	gtk_container_add(GTK_CONTAINER(view), GTK_WIDGET(fourbox));
	gtk_box_pack_start(GTK_BOX(fourbox), ev_t, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(fourbox), ev_d, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(fourbox), ev_a, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(fourbox), ev_n, TRUE, TRUE, 0);

	btn1 = gtk_button_new_with_label(_("Close"));
	gtk_box_pack_start(GTK_BOX(hbox), btn1, TRUE, TRUE, 5);

	if (!modal) {
		btn2 = gtk_button_new_with_label(_("Dialog"));
		btn3 = gtk_button_new_with_label(_("Insert"));
		gtk_box_pack_start(GTK_BOX(hbox), btn2, TRUE, TRUE, 5);
		gtk_box_pack_start(GTK_BOX(hbox), btn3, TRUE, TRUE, 5);
	}

	gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(view));
	gtk_container_add(GTK_CONTAINER(info_window), GTK_WIDGET(frame));

	vbox = gtk_vbox_new(FALSE, 1);
	gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(vbox));
	gtk_container_set_border_width(GTK_CONTAINER(frame), 0);
	gtk_frame_set_label_widget(GTK_FRAME(frame), NULL);

	gtk_widget_set_size_request(info_window, 400, 300);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);

	cd->data = info_window;
	cd->bfwin = bfwin;

	g_signal_connect(G_OBJECT(info_window), "key-press-event", G_CALLBACK(frefcb_info_keypress),
					 cd);
	g_signal_connect(G_OBJECT(btn1), "clicked", G_CALLBACK(frefcb_info_close), cd);
	if (!modal) {
		g_signal_connect(G_OBJECT(btn2), "clicked", G_CALLBACK(frefcb_info_dialog), cd);
		g_signal_connect(G_OBJECT(btn3), "clicked", G_CALLBACK(frefcb_info_insert), cd);
	}
	gtk_widget_show_all(info_window);
}

void frefcb_info_show(GtkButton * button, Tbfwin * bfwin)
{
	FRInfo *entry;
	entry = get_current_entry(bfwin);
	if (entry == NULL)
		return;

	fref_show_info(bfwin, entry, TRUE, gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

GtkWidget *fref_prepare_dialog(Tbfwin * bfwin, FRInfo * entry)
{
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *input;
	GtkWidget *combo;
	GtkWidget *dialog_action_area;
	GtkWidget *cancelbutton;
	GtkWidget *okbutton;
	GtkWidget *infobutton;
	GtkWidget *scroll;
	GtkRequisition req, req2;
	FRAttrInfo *attr = NULL;
	FRParamInfo *par = NULL;
	GList *list = NULL;
	gint itnum, w, h;

	DEBUG_MSG("fref_prepare_dialog, started\n");
	dialog = gtk_dialog_new();
	if (entry->dialog_title != NULL)
		gtk_window_set_title(GTK_WINDOW(dialog), entry->dialog_title);
	vbox = GTK_DIALOG(dialog)->vbox;

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_AUTOMATIC);
	gtk_widget_show(scroll);

	gtk_widget_show(vbox);

	switch (entry->type) {
	case FR_TYPE_TAG:
		list = entry->attributes;
		break;
	case FR_TYPE_FUNCTION:
		list = entry->params;
		break;
	case FR_TYPE_CLASS:
		list = NULL;
		break;
	}
	if (list == NULL) {
		gtk_widget_destroy(dialog);
		DEBUG_MSG("fref_prepare_dialog, list==NULL, aborting..\n");
		return NULL;
	}

	table = gtk_table_new(g_list_length(list), 2, FALSE);


	gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll), table);

	switch (entry->type) {
	case FR_TYPE_TAG:
		{
			list = g_list_first(list);
			attr = (FRAttrInfo *) g_list_nth_data(list, 0);
			itnum = 0;
			while (itnum < g_list_length(list)) {
				if (attr->title != NULL) {
					label = gtk_label_new("");
					if (attr->required) {
						gchar *tofree =
							g_strconcat("<span color='#FF0000'>", attr->title, "</span>", NULL);
						gtk_label_set_markup(GTK_LABEL(label), tofree);
						g_free(tofree);
					} else {
						gtk_label_set_text(GTK_LABEL(label), attr->title);
					}
				} else {
					label = gtk_label_new("");
					if (attr->required) {
						gchar *tofree =
							g_strconcat("<span color='#FF0000'>", attr->name, "</span>", NULL);
						gtk_label_set_markup(GTK_LABEL(label), tofree);
						g_free(tofree);
					} else {
						gtk_label_set_text(GTK_LABEL(label), attr->name);
					}
				}
				gtk_widget_show(label);
				gtk_table_attach(GTK_TABLE(table), label, 0, 1, itnum, itnum + 1,
								 (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 5, 6);
				gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
				gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
				if (attr->has_list) {
					combo = gtk_combo_new();
					gtk_widget_show(combo);
					gtk_table_attach(GTK_TABLE(table), combo, 1, 2, itnum, itnum + 1,
									 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
									 (GtkAttachOptions) (0), 5, 5);
					/*  gtk_combo_set_value_in_list(GTK_COMBO(combo), TRUE,
					   TRUE); */
					gtk_combo_set_popdown_strings(GTK_COMBO(combo),
												  fref_string_to_list(attr->values, ","));
					if (attr->def_value != NULL)
						gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), attr->def_value);
					attr->dlg_item = combo;
					gtk_tooltips_set_tip(FREFGUI(bfwin->fref)->argtips, GTK_COMBO(combo)->entry,
										 attr->description, "");
				} else {
					input = gtk_entry_new();
					if (attr->def_value != NULL)
						gtk_entry_set_text(GTK_ENTRY(input), attr->def_value);
					gtk_widget_show(input);
					gtk_table_attach(GTK_TABLE(table), input, 1, 2, itnum, itnum + 1,
									 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
									 (GtkAttachOptions) (0), 5, 5);
					attr->dlg_item = input;
					gtk_tooltips_set_tip(FREFGUI(bfwin->fref)->argtips, input, attr->description,
										 "");
				}
				itnum++;
				attr = (FRAttrInfo *) g_list_nth_data(list, itnum);
			}
		};
		break;
	case FR_TYPE_FUNCTION:
		{
			list = g_list_first(list);
			par = (FRParamInfo *) g_list_nth_data(list, 0);
			itnum = 0;
			while (itnum < g_list_length(list)) {
				if (par->title != NULL) {
					label = gtk_label_new("");
					if (par->required) {
						gchar *tofree = NULL;
						if (par->type != NULL)
							tofree =
								g_strconcat("<span color='#FF0000'>", par->title, " (", par->type,
											") ", "</span>", NULL);
						else
							tofree =
								g_strconcat("<span color='#FF0000'>", par->title, " </span>", NULL);
						gtk_label_set_markup(GTK_LABEL(label), tofree);
						g_free(tofree);
					} else {
						gchar *tofree = NULL;
						if (par->type != NULL)
							tofree = g_strconcat(par->title, " (", par->type, ") ", NULL);
						else
							tofree = g_strconcat(par->title, " ", NULL);
						gtk_label_set_text(GTK_LABEL(label), tofree);
						g_free(tofree);
					}
				} else {
					label = gtk_label_new("");
					if (par->required) {
						gchar *tofree = NULL;
						if (par->type != NULL)
							tofree =
								g_strconcat("<span color='#FF0000'>", par->name, " (", par->type,
											") ", "</span>", NULL);
						else
							tofree =
								g_strconcat("<span color='#FF0000'>", par->name, " </span>", NULL);
						gtk_label_set_markup(GTK_LABEL(label), tofree);
						g_free(tofree);
					} else {
						gchar *tofree = NULL;
						if (par->type != NULL)
							tofree = g_strconcat(par->name, " (", par->type, ") ", NULL);
						else
							tofree = g_strconcat(par->name, " ", NULL);
						gtk_label_set_text(GTK_LABEL(label), tofree);
						g_free(tofree);
					}
				}


				gtk_widget_show(label);
				gtk_table_attach(GTK_TABLE(table), label, 0, 1, itnum, itnum + 1,
								 (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 5, 6);
				gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
				gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
				if (par->has_list) {
					combo = gtk_combo_new();
					gtk_widget_show(combo);
					gtk_table_attach(GTK_TABLE(table), combo, 1, 2, itnum, itnum + 1,
									 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
									 (GtkAttachOptions) (0), 5, 5);
					/*gtk_combo_set_value_in_list(GTK_COMBO(combo), TRUE,TRUE); */
					gtk_combo_set_popdown_strings(GTK_COMBO(combo),
												  fref_string_to_list(par->values, ","));
					if (par->def_value != NULL)
						gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), par->def_value);
					par->dlg_item = combo;
					gtk_tooltips_set_tip(FREFGUI(bfwin->fref)->argtips, GTK_COMBO(combo)->entry,
										 par->description, "");
				} else {
					input = gtk_entry_new();
					if (par->def_value != NULL)
						gtk_entry_set_text(GTK_ENTRY(input), par->def_value);
					gtk_widget_show(input);
					gtk_table_attach(GTK_TABLE(table), input, 1, 2, itnum, itnum + 1,
									 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
									 (GtkAttachOptions) (0), 5, 5);
					par->dlg_item = input;
					if (par->description != NULL)
						gtk_tooltips_set_tip(FREFGUI(bfwin->fref)->argtips, input, par->description,
											 "");
				}
				itnum++;
				par = (FRParamInfo *) g_list_nth_data(list, itnum);
			}
		};
		break;

	}							/* switch */

	dialog_action_area = GTK_DIALOG(dialog)->action_area;
	gtk_widget_show(dialog_action_area);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(dialog_action_area), GTK_BUTTONBOX_END);


	infobutton = gtk_button_new_with_label(_("Info"));
	gtk_widget_show(infobutton);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), infobutton, FALSE, FALSE, 2);
	g_signal_connect(G_OBJECT(infobutton), "clicked", G_CALLBACK(frefcb_info_show), bfwin);

	cancelbutton = gtk_button_new_from_stock("gtk-cancel");
	gtk_widget_show(cancelbutton);
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), cancelbutton, GTK_RESPONSE_CANCEL);

	okbutton = gtk_button_new_from_stock("gtk-ok");
	gtk_widget_show(okbutton);
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), okbutton, GTK_RESPONSE_OK);
	gtk_tooltips_enable(FREFGUI(bfwin->fref)->argtips);

	gtk_widget_show(table);
	gtk_window_get_size(GTK_WINDOW(dialog), &w, &h);
	gtk_widget_size_request(table, &req);
	gtk_widget_size_request(dialog_action_area, &req2);
	gtk_window_resize(GTK_WINDOW(dialog), req.width + 10, MIN(400, req.height + req2.height + 20));
	return dialog;
}

static void fref_popup_menu_dialog(GtkWidget * widget, Tcallbackdata * cd)
{
	GtkWidget *dialog;
	gint resp;
	gchar *pomstr;
	FRInfo *entry = cd->data;
	Tbfwin *bfwin = cd->bfwin;
	g_free(cd);
	DEBUG_MSG("starting dialog\n");
	dialog = fref_prepare_dialog(bfwin, entry);
	if (dialog) {
		resp = gtk_dialog_run(GTK_DIALOG(dialog));
		if (resp == GTK_RESPONSE_OK) {
			pomstr = fref_prepare_text(entry, dialog);
			gtk_widget_destroy(dialog);
			doc_insert_two_strings(bfwin->current_document, pomstr, NULL);
			g_free(pomstr);
		} else
			gtk_widget_destroy(dialog);
	}
}
static void fref_popup_menu_insert(GtkWidget * widget, Tcallbackdata * cd)
{
	FRInfo *entry = cd->data;
	Tbfwin *bfwin = cd->bfwin;
	g_free(cd);
	doc_insert_two_strings(bfwin->current_document, entry->insert_text, NULL);
}


static void fref_popup_menu_info(GtkWidget * widget, Tcallbackdata * cd)
{
	FRInfo *entry = cd->data;
	Tbfwin *bfwin = cd->bfwin;
	g_free(cd);
	fref_show_info(bfwin, entry, FALSE, NULL);
}


static void fref_popup_menu_rescan_lcb(GtkWidget * widget, gpointer user_data)
{
	gchar *userdir = g_strconcat(g_get_home_dir(), "/.bluefish/", NULL);
	DEBUG_MSG("fref_popup_menu_rescan_lcb, started\n");
	fref_rescan_dir(PKGDATADIR);
	fref_rescan_dir(userdir);
	g_free(userdir);
	DEBUG_MSG("about to refill toplevels\n");
	fill_toplevels(FREFDATA(main_v->frefdata), TRUE);
}

static GtkWidget *fref_popup_menu(Tbfwin * bfwin, FRInfo * entry)
{
	GtkWidget *menu, *menu_item;
	DEBUG_MSG("fref_popup_menu, started\n");
	menu = gtk_menu_new();
	if (entry) {
		Tcallbackdata *cd = g_new(Tcallbackdata, 1);
		cd->bfwin = bfwin;
		cd->data = entry;
		menu_item = gtk_menu_item_new_with_label(_("Dialog"));
		g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(fref_popup_menu_dialog), cd);
		gtk_menu_append(GTK_MENU(menu), menu_item);
		menu_item = gtk_menu_item_new_with_label(_("Insert"));
		g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(fref_popup_menu_insert), cd);
		gtk_menu_append(GTK_MENU(menu), menu_item);
		menu_item = gtk_menu_item_new_with_label(_("Info"));
		g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(fref_popup_menu_info), cd);
		gtk_menu_append(GTK_MENU(menu), menu_item);
		menu_item = gtk_menu_item_new();
		gtk_menu_append(GTK_MENU(menu), menu_item);
	}
	menu_item = gtk_menu_item_new_with_label(_("Options"));
	gtk_menu_append(GTK_MENU(menu), menu_item);
	{
		GtkWidget *optionsmenu, *ldblclckmenu, *infowinmenu;
		GSList *group = NULL;
		GSList *group2 = NULL;
		optionsmenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), optionsmenu);
		menu_item = gtk_menu_item_new_with_label(_("Rescan reference files"));
		g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(fref_popup_menu_rescan_lcb),
						 NULL);
		gtk_menu_append(GTK_MENU(optionsmenu), menu_item);
		menu_item = gtk_menu_item_new_with_label(_("Left doubleclick action"));
		gtk_menu_append(GTK_MENU(optionsmenu), menu_item);

		ldblclckmenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), ldblclckmenu);
		menu_item =
			togglemenuitem(NULL, _("Insert"),
						   (main_v->globses.fref_ldoubleclick_action == FREF_ACTION_INSERT),
						   G_CALLBACK(fref_ldblclck_changed), GINT_TO_POINTER(FREF_ACTION_INSERT));
		group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item));
		gtk_menu_append(GTK_MENU(ldblclckmenu), menu_item);
		menu_item =
			togglemenuitem(group, _("Dialog"),
						   (main_v->globses.fref_ldoubleclick_action == FREF_ACTION_DIALOG),
						   G_CALLBACK(fref_ldblclck_changed), GINT_TO_POINTER(FREF_ACTION_DIALOG));
		gtk_menu_append(GTK_MENU(ldblclckmenu), menu_item);
		menu_item =
			togglemenuitem(group, _("Info"),
						   (main_v->globses.fref_ldoubleclick_action == FREF_ACTION_INFO),
						   G_CALLBACK(fref_ldblclck_changed), GINT_TO_POINTER(FREF_ACTION_INFO));
		gtk_menu_append(GTK_MENU(ldblclckmenu), menu_item);

		menu_item = gtk_menu_item_new_with_label(_("Info type"));
		gtk_menu_append(GTK_MENU(optionsmenu), menu_item);
		infowinmenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), infowinmenu);
		menu_item =
			togglemenuitem(NULL, _("Description"), (main_v->globses.fref_info_type == FREF_IT_DESC),
						   G_CALLBACK(fref_infotype_changed), GINT_TO_POINTER(FREF_IT_DESC));
		group2 = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item));
		gtk_menu_append(GTK_MENU(infowinmenu), menu_item);
		menu_item =
			togglemenuitem(group2, _("Attributes/Parameters"),
						   (main_v->globses.fref_info_type == FREF_IT_ATTRS),
						   G_CALLBACK(fref_infotype_changed), GINT_TO_POINTER(FREF_IT_ATTRS));
		gtk_menu_append(GTK_MENU(infowinmenu), menu_item);
		menu_item =
			togglemenuitem(group2, _("Notes"), (main_v->globses.fref_info_type == FREF_IT_NOTES),
						   G_CALLBACK(fref_infotype_changed), GINT_TO_POINTER(FREF_IT_NOTES));
		gtk_menu_append(GTK_MENU(infowinmenu), menu_item);


	}
	gtk_widget_show_all(menu);
	g_signal_connect_after(G_OBJECT(menu), "destroy", G_CALLBACK(destroy_disposable_menu_cb), menu);
	return menu;
}

static gboolean frefcb_event_keypress(GtkWidget * widget, GdkEventKey * event, Tbfwin * bfwin)
{
	FRInfo *entry;
	entry = get_current_entry(bfwin);
	if (entry != NULL) {
		if (g_strcasecmp(gdk_keyval_name(event->keyval), "F1") == 0) {
			fref_show_info(bfwin, entry, FALSE, NULL);
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean reference_file_known(gchar * path)
{
	GList *tmplist = g_list_first(main_v->props.reference_files);
	while (tmplist) {
		gchar **arr = tmplist->data;
		if (count_array(arr) == 2 && strcmp(arr[1], path) == 0) {
			return TRUE;
		}
		tmplist = g_list_next(tmplist);
	}
	return FALSE;
}

void fref_rescan_dir(const gchar * dir)
{
	const gchar *filename;
	GError *error = NULL;
	gchar *tofree;
	GPatternSpec *ps = g_pattern_spec_new("funcref_*.xml");
	GDir *gd = g_dir_open(dir, 0, &error);
	filename = g_dir_read_name(gd);
	while (filename) {
		if (g_pattern_match(ps, strlen(filename), filename, NULL)) {
			gchar *path = g_strconcat(dir, filename, NULL);
			DEBUG_MSG("filename %s has a match!\n", filename);
			if (!reference_file_known(path)) {
				tofree = fref_xml_get_refname(path);
				main_v->props.reference_files =
					g_list_append(main_v->props.reference_files,
								  array_from_arglist(g_strdup(tofree), path, NULL));
				g_free(tofree);
			}
			g_free(path);
		}
		filename = g_dir_read_name(gd);
	}
	g_dir_close(gd);
	g_pattern_spec_free(ps);
}

typedef struct {
	Tbfwin *bfwin;
	gchar *cat;
} Tfref_cleanup;

static guint fref_idle_cleanup(Tfref_cleanup * data)
{
	GtkTreeIter iter;
	gboolean cont = TRUE;
	DEBUG_MSG("fref_idle_cleanup, started for data=%s\n", data->cat);
	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(FREFDATA(main_v->frefdata)->store), &iter);
	while (cont) {
		gchar *str = NULL;
		gtk_tree_model_get(GTK_TREE_MODEL(FREFDATA(main_v->frefdata)->store), &iter, STR_COLUMN,
						   &str, -1);
		DEBUG_MSG("fref_idle_cleanup, comparing %s,%s\n", str, data->cat);
		if (strcmp(str, data->cat) == 0) {
			DEBUG_MSG("fref_idle_cleanup, found!\n");
			fref_loader_unload_ref(GTK_TREE_STORE(FREFDATA(main_v->frefdata)->store), &iter);
			break;
		}
		g_free (str);
		cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(FREFDATA(main_v->frefdata)->store), &iter);
	}
	{
		GtkTreeIter newiter;
		gtk_tree_store_append(GTK_TREE_STORE(FREFDATA(main_v->frefdata)->store), &newiter, &iter);
	}
	g_free (data->cat);
	g_free (data);
	return FALSE;				/* do not call again */
}

static void frefcb_row_collapsed(GtkTreeView * treeview, GtkTreeIter * arg1, GtkTreePath * arg2,
								 Tbfwin * bfwin)
{
	gint *cnt = NULL;
	gpointer *aux;
	gboolean do_unload = FALSE;
	GtkTreeModel *treemodel = GTK_TREE_MODEL(FREFDATA(main_v->frefdata)->store);
	if (gtk_tree_path_get_depth(arg2) == 1) {
		gchar *cat;
		gtk_tree_model_get(treemodel, arg1, STR_COLUMN, &cat, -1);
		DEBUG_MSG("frefcb_row_collapsed, lookup for the refcount of %s\n", cat);
		aux = g_hash_table_lookup(FREFDATA(main_v->frefdata)->refcount, cat);
		if (aux != NULL) {
			cnt = (gint *) aux;
			DEBUG_MSG("frefcb_row_collapsed, refcount=%d, about to decrease\n", *cnt);
			*cnt = (*cnt) - 1;
			if (*cnt == 0) {
				Tfref_cleanup *data = g_new(Tfref_cleanup, 1);
				do_unload = TRUE;
				data->cat = cat;
				data->bfwin = bfwin;
				gtk_timeout_add(250, (GtkFunction) fref_idle_cleanup, data);
			}
		} else
			do_unload = FALSE;
	} else
		do_unload = FALSE;
/*	if (do_unload) {
		DEBUG_MSG("frefcb_row_collapsed, do_unload=%d, unloading!\n",do_unload);
		val = g_new0(GValue, 1);
		gtk_tree_model_get_value(GTK_TREE_MODEL(treemodel), arg1, 2, val);
		if (G_IS_VALUE(val) && g_value_peek_pointer(val)!=NULL) {
			DEBUG_MSG("frefcb_row_collapsed, calling fref_loader_unload_ref\n");
			fref_loader_unload_ref(GTK_TREE_STORE(treemodel),arg1);
			/ * dummy node for expander display * /
			DEBUG_MSG("frefcb_row_collapsed, appending dummy node\n");
			gtk_tree_store_append(GTK_TREE_STORE(treemodel), &iter, arg1);
		}
		g_free(val);
	}*/
}


static void frefcb_full_info(GtkButton * button, Tbfwin * bfwin)
{
	FRInfo *entry;
	entry = get_current_entry(bfwin);
	if (entry == NULL)
		return;
	fref_show_info(bfwin, entry, FALSE, NULL);
}

static void frefcb_search(GtkButton * button, Tbfwin * bfwin)
{
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	GtkWidget *dlg, *entry;
	GValue *val;
	GtkTreeIter iter;
	GHashTable *dict;
	gint result;
	gchar *stf = NULL;

	gtk_tree_view_get_cursor(GTK_TREE_VIEW(FREFGUI(bfwin->fref)->tree), &path, &col);
	if (path != NULL) {
		while (gtk_tree_path_get_depth(path) > 1 && gtk_tree_path_up(path));

		gtk_tree_model_get_iter(gtk_tree_view_get_model(GTK_TREE_VIEW(FREFGUI(bfwin->fref)->tree)),
								&iter, path);
		gtk_tree_path_free(path);
		val = g_new0(GValue, 1);
		/* first column of reference title holds dictionary */
		gtk_tree_model_get_value(gtk_tree_view_get_model(GTK_TREE_VIEW(FREFGUI(bfwin->fref)->tree)),
								 &iter, 1, val);
		if (G_IS_VALUE(val) && g_value_fits_pointer(val)) {
			dict = (GHashTable *) g_value_peek_pointer(val);
			if (dict != NULL) {
				gpointer ret = NULL;
				dlg =
					gtk_dialog_new_with_buttons(_("Reference search"), NULL, GTK_DIALOG_MODAL,
												GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, GTK_STOCK_OK,
												GTK_RESPONSE_ACCEPT, NULL);
				gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_ACCEPT);
				entry = gtk_entry_new();
				gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
				gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), entry, TRUE, TRUE, 0);
				gtk_widget_show(entry);
				result = gtk_dialog_run(GTK_DIALOG(dlg));
				if (result == GTK_RESPONSE_ACCEPT) {
					stf = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
					ret = g_hash_table_lookup(dict, stf);
					g_free(stf);
					if (!ret)
						error_dialog(bfwin->main_window, _("Reference search"),
									 _("Reference not found"));
				}
				gtk_widget_destroy(dlg);

				if (ret != NULL) {
					GtkTreePath *path2 = gtk_tree_row_reference_get_path(ret);
#ifndef HAVE_ATLEAST_GTK_2_2
					gtktreepath_expand_to_root(FREFGUI(bfwin->fref)->tree, path2);
#else
					gtk_tree_view_expand_to_path(GTK_TREE_VIEW(FREFGUI(bfwin->fref)->tree), path2);
#endif
					gtk_tree_view_set_cursor(GTK_TREE_VIEW(FREFGUI(bfwin->fref)->tree), path2,
											 gtk_tree_view_get_column(GTK_TREE_VIEW
																	  (FREFGUI(bfwin->fref)->tree),
																	  0), FALSE);
				}
			} else
				error_dialog(bfwin->main_window, _("Error"),
							 _
							 ("Perhaps you didn't load a reference, or you did not select a reference to search in."));
		}
		g_value_unset(val);
		g_free(val);
	}
}

static gboolean frefcb_event_mouseclick(GtkWidget * widget, GdkEventButton * event, Tbfwin * bfwin)
{
	FRInfo *entry;

	if (widget != FREFGUI(bfwin->fref)->tree)
		return FALSE;

	entry = get_current_entry(bfwin);
	if (entry == NULL) {
		if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
			gtk_menu_popup(GTK_MENU(fref_popup_menu(bfwin, NULL)), NULL, NULL, NULL, NULL,
						   event->button, event->time);
			return TRUE;
		} else
			return FALSE;
	}

	if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {	/* right mouse click */
		gtk_menu_popup(GTK_MENU(fref_popup_menu(bfwin, entry)), NULL, NULL, NULL, NULL,
					   event->button, event->time);
	} else if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) {	/* double click  */
		Tcallbackdata *cd = g_new(Tcallbackdata, 1);
		cd->data = entry;
		cd->bfwin = bfwin;
		switch (main_v->globses.fref_ldoubleclick_action) {
		case FREF_ACTION_INSERT:
			fref_popup_menu_insert(NULL, cd);
			break;
		case FREF_ACTION_DIALOG:
			fref_popup_menu_dialog(NULL, cd);
			break;
		case FREF_ACTION_INFO:
			fref_popup_menu_info(NULL, cd);
			break;
		default:
			g_print("weird, fref_doubleclick_action=%d\n",
					main_v->globses.fref_ldoubleclick_action);
			main_v->globses.fref_ldoubleclick_action = FREF_ACTION_DIALOG;
			break;
		}
	}
	return FALSE;				/* we have handled the event, but the treeview freezes if you return TRUE,    so we return FALSE */
}

static void frefcb_cursor_changed(GtkTreeView * treeview, Tbfwin * bfwin)
{
	FRInfo *entry;
	gchar *info = NULL, *tmpinfo = NULL; 
	GdkRectangle rect;

	entry = get_current_entry(bfwin);
	if (entry == NULL)
		return;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FREFGUI(bfwin->fref)->infocheck))) {
		if (entry->description != NULL) {
			switch (main_v->globses.fref_info_type) {
			case FREF_IT_DESC:
				tmpinfo = fref_prepare_info(entry, FR_INFO_DESC, FALSE);
				info = g_strconcat("<span size=\"small\"><b>Description:</b></span>\n", tmpinfo, NULL);
				break;
			case FREF_IT_ATTRS:
				tmpinfo = fref_prepare_info(entry, FR_INFO_ATTRS, FALSE);
				switch (entry->type) {
				case FR_TYPE_TAG:
					info = g_strconcat("<span size=\"small\"><b>Attributes:</b></span>\n", tmpinfo, NULL);
					break;
				case FR_TYPE_FUNCTION:
					info = g_strconcat("<span size=\"small\"><b>Parameters:</b></span>\n", tmpinfo, NULL);
					break;
				}
				break;
			case FREF_IT_NOTES:
				info = fref_prepare_info(entry, FR_INFO_NOTES, FALSE);
				break;
			default:
				info = g_strdup("Unknown fref_info_type");
			}					/* switch */
			gtk_tree_view_get_visible_rect(GTK_TREE_VIEW(FREFGUI(bfwin->fref)->tree), &rect);
			gtk_widget_set_size_request(FREFGUI(bfwin->fref)->infoview, rect.width, -1);
			gtk_label_set_markup(GTK_LABEL(FREFGUI(bfwin->fref)->infoview), info);
			if (tmpinfo) g_free (tmpinfo);
			g_free(info);
		} else
			gtk_label_set_text(GTK_LABEL(FREFGUI(bfwin->fref)->infoview), "");
	}
}

static void frefcb_infocheck_toggled(GtkToggleButton * togglebutton, Tbfwin * bfwin)
{
	if (gtk_toggle_button_get_active(togglebutton))
		gtk_widget_show(FREFGUI(bfwin->fref)->infoscroll);
	else
		gtk_widget_hide(FREFGUI(bfwin->fref)->infoscroll);
}

gchar *fref_tip(gconstpointer win, gconstpointer tree, gint x, gint y)
{
	gchar *str = NULL, *tofree = NULL;
	GList *lst;
	FRAttrInfo *tmpa;
	FRParamInfo *tmpp;
	FRInfo *info = NULL;
	GtkTreePath *path;


	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tree), x, y, &path, NULL, NULL, NULL))
		return NULL;

	if (path != NULL) {
		GValue *val;
		GtkTreeIter iter;
		gtk_tree_model_get_iter(gtk_tree_view_get_model(GTK_TREE_VIEW(tree)), &iter, path);
		gtk_tree_path_free(path);
		val = g_new0(GValue, 1);
		gtk_tree_model_get_value(gtk_tree_view_get_model(GTK_TREE_VIEW(tree)), &iter, 1, val);
		if (G_IS_VALUE(val) && g_value_fits_pointer(val)) {
			info = (FRInfo *) (g_value_peek_pointer(val));
		}
		g_value_unset(val);
		g_free(val);
	}

	if (!info)
		return NULL;

	switch (info->type) {
	case FR_TYPE_TAG:
		str = g_strdup_printf("<b>%s:</b> ", info->name);
		lst = g_list_first(info->attributes);
		while (lst) {
			tmpa = (FRAttrInfo *) lst->data;
			tofree = str;
			str = g_strconcat(str, "<span size=\"small\" ><i>", tmpa->name, "</i></span>  ", NULL);
			g_free(tofree);
			lst = g_list_next(lst);
		}
		break;
	case FR_TYPE_FUNCTION:
		if (info->return_type != NULL)
			str = g_strdup_printf("<i>%s</i>  %s( ", info->return_type, info->name);
		else
			str = g_strdup_printf("%s( ", info->name);
		lst = g_list_first(info->params);
		while (lst) {
			tmpp = (FRParamInfo *) lst->data;
			tofree = str;
			if (tmpp->type != NULL)
				str = g_strconcat(str, "<i>", tmpp->type, "</i>  <b>", tmpp->name, "</b>", NULL);
			else
				str = g_strconcat(str, "<b>", tmpp->name, "</b>", NULL);
			g_free(tofree);
			lst = g_list_next(lst);
			if (lst) {
				tofree = str;
				str = g_strconcat(str, ",", NULL);
				g_free(tofree);
			}
		}
		tofree = str;
		str = g_strconcat(str, " )", NULL);
		g_free(tofree);
		break;
	}
	return str;
}

GtkWidget *fref_gui(Tbfwin * bfwin)
{
	GtkWidget *scroll, *box, *pane, *box2, *btn1, *btn2, *btn3;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	Tfref_data *fdata = FREFDATA(main_v->frefdata);
	GtkTreeIter it;

	bfwin->fref = g_new0(Tfref_gui, 1);

	pane = gtk_vpaned_new();
	box = gtk_vbox_new(FALSE, 1);
	box2 = gtk_hbox_new(FALSE, 1);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_AUTOMATIC);
	FREFGUI(bfwin->fref)->infoscroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(FREFGUI(bfwin->fref)->infoscroll),
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	FREFGUI(bfwin->fref)->tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(fdata->store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(FREFGUI(bfwin->fref)->tree), FALSE);
	/*gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (FREFGUI(bfwin->fref)->tree), TRUE);*/
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("", cell, "text", STR_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(FREFGUI(bfwin->fref)->tree), column);

	gtk_container_add(GTK_CONTAINER(scroll), FREFGUI(bfwin->fref)->tree);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(FREFGUI(bfwin->fref)->tree), FALSE);

	g_signal_connect(G_OBJECT(FREFGUI(bfwin->fref)->tree), "row-collapsed",
					 G_CALLBACK(frefcb_row_collapsed), bfwin);
	g_signal_connect(G_OBJECT(FREFGUI(bfwin->fref)->tree), "row-expanded",
					 G_CALLBACK(frefcb_row_expanded), fdata->store);
	g_signal_connect(G_OBJECT(FREFGUI(bfwin->fref)->tree), "button-press-event",
					 G_CALLBACK(frefcb_event_mouseclick), bfwin);
	g_signal_connect(G_OBJECT(FREFGUI(bfwin->fref)->tree), "key-press-event",
					 G_CALLBACK(frefcb_event_keypress), bfwin);

	gtk_widget_show(FREFGUI(bfwin->fref)->tree);
	gtk_widget_show(scroll);

	/* BUG??: don't we have a global tooltips object somewhere ? 
	FREFGUI(bfwin->fref)->argtips = gtk_tooltips_new();*/
	FREFGUI(bfwin->fref)->argtips = main_v->tooltips;
	FREFGUI(bfwin->fref)->tips =
		tree_tips_new_full(bfwin, GTK_TREE_VIEW(FREFGUI(bfwin->fref)->tree), fref_tip);
	tree_tips_set_hide_interval(FREFGUI(bfwin->fref)->tips, 5000);

	FREFGUI(bfwin->fref)->infoview = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(FREFGUI(bfwin->fref)->infoview), TRUE);
	gtk_label_set_use_markup(GTK_LABEL(FREFGUI(bfwin->fref)->infoview), TRUE);
	gtk_misc_set_alignment(GTK_MISC(FREFGUI(bfwin->fref)->infoview), 0.0, 0.0);
	gtk_misc_set_padding(GTK_MISC(FREFGUI(bfwin->fref)->infoview), 5, 5);

	g_signal_connect(G_OBJECT(FREFGUI(bfwin->fref)->tree), "cursor-changed",
					 G_CALLBACK(frefcb_cursor_changed), bfwin);

	FREFGUI(bfwin->fref)->infocheck = gtk_check_button_new_with_label(_("info"));
	gtk_widget_show(FREFGUI(bfwin->fref)->infocheck);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FREFGUI(bfwin->fref)->infocheck), TRUE);
	g_signal_connect(G_OBJECT(FREFGUI(bfwin->fref)->infocheck), "toggled",
					 G_CALLBACK(frefcb_infocheck_toggled), bfwin);
	{
		/* BUG??: is this ever freed ??? */
		Tcallbackdata *cd = g_new(Tcallbackdata, 1);
		cd->data = NULL;
		cd->bfwin = bfwin;
		btn1 = bf_allbuttons_backend(_("d"), FALSE, 0, G_CALLBACK(frefcb_info_dialog), cd);
	}
	btn2 = bf_allbuttons_backend(_("i"), FALSE, 0, G_CALLBACK(frefcb_full_info), bfwin);
	btn3 = bf_allbuttons_backend(_("s"), FALSE, 0, G_CALLBACK(frefcb_search), bfwin);
	gtk_tooltips_set_tip(FREFGUI(bfwin->fref)->argtips, btn1, _("dialog"), "");
	gtk_tooltips_set_tip(FREFGUI(bfwin->fref)->argtips, btn2, _("info"), "");
	gtk_tooltips_set_tip(FREFGUI(bfwin->fref)->argtips, btn3, _("search"), "");

	gtk_box_pack_start(GTK_BOX(box2), FREFGUI(bfwin->fref)->infocheck, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box2), btn3, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box2), btn2, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box2), btn1, FALSE, TRUE, 0);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(FREFGUI(bfwin->fref)->infoscroll),
										  FREFGUI(bfwin->fref)->infoview);
	gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), box2, FALSE, TRUE, 0);
	gtk_paned_pack1(GTK_PANED(pane), box, TRUE, FALSE);
	gtk_paned_pack2(GTK_PANED(pane), FREFGUI(bfwin->fref)->infoscroll, TRUE, TRUE);
	gtk_widget_show_all(pane);

	if (!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(fdata->store), &it, NULL, 0))
		fill_toplevels(fdata, FALSE);
	return pane;
}
