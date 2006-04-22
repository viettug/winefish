/* $Id$ */

/* Winefish LaTeX Editor
 *
 * Autotext support
 *
 * Copyright (c) 2005 KyAnh <kyanh@o2.pl>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/*
 * NOTES
 * g_completion provides an completion only, *NOT* autotext
 *
 */
#include <gtk/gtk.h>
#include <stdlib.h> /* strtoul() */
#include <string.h> /* strcmp() */

#include "bluefish.h"
#include "autox.h"
#include "rcfile.h" /* rcfile_parse_completion() */
#include "stringlist.h" /* count_array() */
#include "snooper.h"

/* a share autotext array for all file types */
/*
static GHashTable *autotext_hashtable = NULL;
static GPtrArray *autotext_array= NULL;
*/

#ifdef DEBUG
static void autotext_test(gchar *key, gchar *value, void *data) {
	g_print("autotext_test: %s => %s\n", key, value);
}
#endif

static void autotext_remove_key_value(gpointer *key_or_value) {
	g_free(key_or_value);
}

void autotext_init(void) {
	GList *autotext_list = NULL;
	rcfile_parse_autotext(&autotext_list);

	main_v->props.autotext_hashtable = g_hash_table_new_full((GHashFunc)g_str_hash, (GEqualFunc)g_str_equal, (GDestroyNotify)autotext_remove_key_value, (GDestroyNotify)autotext_remove_key_value);
	main_v->props.autotext_array = g_ptr_array_new();
	GList *tmp2list = g_list_first(autotext_list);
	gint index=0;
	while (tmp2list) {
		gchar **strarr;
		strarr = (gchar **) tmp2list->data;
		if (count_array(strarr) == 3) {
			if (!g_hash_table_lookup(main_v->props.autotext_hashtable, strarr[0])) {
				if (strlen(strarr[1]) || strlen(strarr[2])) {
					gchar **tmp_array = g_malloc(3*sizeof(gchar *));
					tmp_array[0] = g_strdup(strarr[1]);
					tmp_array[1] = g_strdup(strarr[2]);
					tmp_array[2] = NULL;
					g_ptr_array_add(main_v->props.autotext_array,tmp_array);
					g_hash_table_insert(main_v->props.autotext_hashtable, g_strdup(strarr[0]), g_strdup_printf("%d", index));
					index++;
				}
			}/* else: replace the old by the new one */
		}
		tmp2list = g_list_next(tmp2list);
	}

	/* we donot need autotext_list anymore */
	/* this is taken from rcfile.c::free_configlist() -- this function is static */
	{
		GList *tmplist = g_list_first(autotext_list);
		while(tmplist) {
			void *cli = tmplist->data;
			g_free(cli);
			tmplist = g_list_next(tmplist);
		}
		g_list_free(autotext_list);
	}
	
#ifdef DEBUG
	g_hash_table_foreach(main_v->props.autotext_hashtable, (GHFunc)autotext_test, NULL);
	gchar **tmp = NULL; /* g_malloc(2*sizeof(gchar *)); */
	for (index=0; index < main_v->props.autotext_array->len; index++) {
		tmp = g_ptr_array_index(main_v->props.autotext_array, index);
		g_print("autotext: element %d/%d: [%s,%s]\n", index, main_v->props.autotext_array->len,  tmp[0], tmp[1]);
	}
	gchar *value;
	value = g_hash_table_lookup(main_v->props.autotext_hashtable, "\\abc");
	if (value) {
		index = strtoul(value, NULL, 10);
		tmp = g_ptr_array_index(main_v->props.autotext_array, index);
		g_print("autotext: lookup: index=%d, (%s,%s)\n", index, tmp[0], tmp[1]);
	}
	g_free(value);
#endif
}

gchar **autotext_done(gchar *lookup) {
	gchar *value = g_hash_table_lookup(main_v->props.autotext_hashtable, lookup);
	if (value) {
		gint index = strtoul(value, NULL, 10);
		gchar **tmp = g_ptr_array_index(main_v->props.autotext_array, index);
		return tmp;
	}else{
		return NULL;
	}
}

void completion_init(void) {
	main_v->completion.show = COMPLETION_WINDOW_INIT;
	main_v->props.completion = g_completion_new(NULL);
	main_v->props.completion_s = g_completion_new(NULL);
	main_v->completion.cache = NULL;

	GList *tmplist = NULL, *tmplist_s = NULL;
	GList *tmp2list = NULL, *tmp2list_s = NULL;
	GList *wordslist = NULL, *wordslist_s = NULL;
	/* GList *search = NULL; */

	rcfile_parse_completion(&wordslist, &wordslist_s);

	tmp2list = g_list_first(wordslist);
	GList *search = NULL;
	while (tmp2list) {
		gchar **strarr;
		strarr = (gchar **) tmp2list->data;
		/* TODO: remove this check for performance reason ?? */
		if (count_array(strarr) == 2) {
			/* searching for duplicate entries may slow down Winefish
			... but is there any way ? */
			search = g_list_find_custom(tmplist, strarr[1],(GCompareFunc)strcmp);
			if (search == NULL) {
				tmplist = g_list_insert_sorted(tmplist, strarr[1], (GCompareFunc)strcmp);
			}
#ifdef DEBUG
			else{
				g_print("completion_init: duplicate entry: %s\n", strarr[1]);
			}
#endif
		}
		tmp2list = g_list_next(tmp2list);
	}
	if (tmplist) {
		g_completion_add_items(main_v->props.completion, tmplist);
		g_list_free(tmplist);
		g_list_free(search);
	}

	tmp2list = g_list_first(wordslist);
	while(tmp2list) {
		void *cli = tmp2list->data;
		g_free(cli);
		tmp2list = g_list_next(tmp2list);
	}
	g_list_free(wordslist);

	/* session ****************************************************************************/

	tmp2list_s = g_list_first(wordslist_s);
	GList *search2 = NULL;
	while (tmp2list_s) {
		gchar **strarr;
		strarr = (gchar **) tmp2list_s->data;
		if (count_array(strarr) == 2) {
			search2 = g_list_find_custom(main_v->props.completion->items, strarr[1], (GCompareFunc)strcmp);
			if ( !search2 ) {
				search2 = g_list_find_custom(tmplist_s, strarr[1],(GCompareFunc)strcmp);
				if (!search2) {
					tmplist_s = g_list_insert_sorted(tmplist_s, strarr[1], (GCompareFunc)strcmp);
				}
			}
		}
		tmp2list_s = g_list_next(tmp2list_s);
	}
	if (tmplist_s) {
		g_completion_add_items(main_v->props.completion_s, tmplist_s);
		g_list_free(tmplist_s);
	}
	tmp2list_s = g_list_first(wordslist_s);
	while(tmp2list_s) {
		void *cli = tmp2list_s->data;
		g_free(cli);
		tmp2list_s = g_list_next(tmp2list_s);
	}
	g_list_free(wordslist_s);
}


