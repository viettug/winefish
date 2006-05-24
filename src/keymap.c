/* $Id$ */

/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * outputbox.c the output box
 *
 * Copyright (C) 2002 Olivier Sessink
 * Modified for Winefish (C) 2005 2006 kyanh <kyanh@o2.pl>
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
 */

#include "config.h"

#ifdef SNOOPER2

#include <gtk/gtk.h>
#include <string.h>

#include "bluefish.h"
#include "stringlist.h"
#include "rcfile.h"
#include "snooper2.h"
#include "bf_lib.h" /* return_first_existing_filename */
#include "bfspell.h" /* func_spell_check */

static gint func_any(GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt) {
	g_print("_any: hello\n");
	return 1;
}

/** TODO: passed some data here */
static void add_function(const char *human_name, FUNCTION computer_name, gint data) {
	Tfunc *func;
	func = g_new0(Tfunc, 1);
	func->exec = computer_name;
	func->data = data | FUNC_FROM_SNOOPER;
	g_hash_table_insert(main_v->func_hashtable, (gpointer)human_name, func);
}

void funclist_init() {
	main_v->func_hashtable = g_hash_table_new((GHashFunc)g_str_hash, (GEqualFunc)g_str_equal);
	add_function("_help", func_any, FUNC_ANY);
	add_function("_about", func_any, FUNC_ANY);
	add_function("_key_list", func_any, FUNC_ANY);

	add_function("_complete", func_complete_show, FUNC_ANY);
	add_function("_complete_eat", func_complete_eat, FUNC_ANY);

	add_function("_comment", func_comment, FUNC_ANY);
	add_function("_uncomment", func_comment, FUNC_VALUE_0 | FUNC_ANY);

	add_function("_indent", func_indent, FUNC_ANY);
	add_function("_unindent", func_indent, FUNC_VALUE_0 | FUNC_ANY);

	add_function("_delete_line", func_delete_line, FUNC_VALUE_0 | FUNC_ANY);
	add_function("_delete_line_right", func_delete_line, FUNC_VALUE_1 | FUNC_ANY);
	add_function("_delete_line_left", func_delete_line, FUNC_VALUE_2 | FUNC_ANY);

	add_function("_grep", func_grep, FUNC_ANY);
	add_function("_template_list", func_template_list, FUNC_ANY);

	add_function("_move_line_up", func_move, FUNC_ANY | (FUNC_MOVE_LINE_UP<<FUNC_VALUE_) );
	add_function("_move_line_down", func_move, FUNC_ANY | (FUNC_MOVE_LINE_DOWN<<FUNC_VALUE_) );
	add_function("_move_line_end", func_move, FUNC_ANY | (FUNC_MOVE_LINE_END<<FUNC_VALUE_) );
	add_function("_move_line_start", func_move, FUNC_ANY | (FUNC_MOVE_LINE_START<<FUNC_VALUE_) );
	add_function("_move_start", func_move, FUNC_ANY | (FUNC_MOVE_START<<FUNC_VALUE_) );
	add_function("_move_end", func_move, FUNC_ANY | (FUNC_MOVE_END<<FUNC_VALUE_) );
	add_function("_move_char_left", func_move, FUNC_ANY | (FUNC_MOVE_CHAR_LEFT<<FUNC_VALUE_) );
	add_function("_move_char_right", func_move, FUNC_ANY | (FUNC_MOVE_CHAR_RIGHT<<FUNC_VALUE_) );
	add_function("_move_sentence_start", func_move, FUNC_ANY | (FUNC_MOVE_SENTENCE_START<<FUNC_VALUE_) );
	add_function("_move_sentence_end", func_move, FUNC_ANY | (FUNC_MOVE_SENTENCE_END<<FUNC_VALUE_) );
	add_function("_move_word_start", func_move, FUNC_ANY | (FUNC_MOVE_WORD_START<<FUNC_VALUE_) );
	add_function("_move_word_end", func_move, FUNC_ANY | (FUNC_MOVE_WORD_END<<FUNC_VALUE_) );
#ifdef ENABLE_MOVE_DISPLAY_LINE
	add_function("_move_display_start", func_move, FUNC_ANY | (FUNC_MOVE_DISPLAY_START<<FUNC_VALUE_) );
	add_function("_move_display_end", func_move, FUNC_ANY | (FUNC_MOVE_DISPLAY_END<<FUNC_VALUE_) );
#endif /* ENABLE_MOVE_DISPLAY_LINE */
	add_function("_select_line_up", func_move, FUNC_ANY | FUNC_VALUE_0 | (FUNC_MOVE_LINE_UP<<FUNC_VALUE_) );
	add_function("_select_line_down", func_move, FUNC_ANY | FUNC_VALUE_0 | (FUNC_MOVE_LINE_DOWN<<FUNC_VALUE_) );
	add_function("_select_line_end", func_move, FUNC_ANY | FUNC_VALUE_0 | (FUNC_MOVE_LINE_END<<FUNC_VALUE_) );
#if 0
	/* use func_move() to delete... but not better as func_delete_line() */
	add_function("_delete_line_end2", func_move, FUNC_ANY | FUNC_VALUE_1 | (FUNC_MOVE_LINE_END<<FUNC_VALUE_) );
#endif
	add_function("_select_line_start", func_move, FUNC_ANY | FUNC_VALUE_0 | (FUNC_MOVE_LINE_START<<FUNC_VALUE_) );
	add_function("_select_start", func_move, FUNC_ANY | FUNC_VALUE_0 | (FUNC_MOVE_START<<FUNC_VALUE_) );
	add_function("_select_end", func_move, FUNC_ANY | FUNC_VALUE_0 | (FUNC_MOVE_END<<FUNC_VALUE_) );
	add_function("_select_char_left", func_move, FUNC_ANY | FUNC_VALUE_0 | (FUNC_MOVE_CHAR_LEFT<<FUNC_VALUE_) );
	add_function("_select_char_right", func_move, FUNC_ANY | FUNC_VALUE_0 | (FUNC_MOVE_CHAR_RIGHT<<FUNC_VALUE_) );
	add_function("_select_sentence_start", func_move, FUNC_ANY | FUNC_VALUE_0 | (FUNC_MOVE_SENTENCE_START<<FUNC_VALUE_) );
	add_function("_select_sentence_end", func_move, FUNC_ANY | FUNC_VALUE_0 | (FUNC_MOVE_SENTENCE_END<<FUNC_VALUE_) );
	add_function("_select_word_start", func_move, FUNC_ANY | FUNC_VALUE_0 | (FUNC_MOVE_WORD_START<<FUNC_VALUE_) );
	add_function("_select_word_end", func_move, FUNC_ANY | FUNC_VALUE_0 | (FUNC_MOVE_WORD_END<<FUNC_VALUE_) );
	add_function("_select_line", func_move, FUNC_ANY | FUNC_VALUE_0 | (FUNC_MOVE_LINE<<FUNC_VALUE_) );

	add_function("_zoom_in", func_zoom, FUNC_ANY);
	add_function("_zoom_out", func_zoom, FUNC_ANY | FUNC_VALUE_0);
#ifdef HAVE_LIBASPELL
	add_function("_spell_check", func_spell_check, FUNC_ANY);
	/* add_function("_spell_check_line", func_spell_check, FUNC_ANY | FUNC_VALUE_0 ); */
#endif /* HAVE_LIBASPELL */
}

static void rcfile_parse_keys(void *keys_list) {
	gchar *filename, *filename_, *filename__;
	GList *keys_configlist=NULL;
	init_prop_arraylist(&keys_configlist, keys_list, "map:",2, TRUE);
	filename = g_strconcat(g_get_current_dir(), "/keymap", NULL);
	filename_ = g_strconcat(g_get_home_dir(), "/.winefish/keymap", NULL);
	filename__ = return_first_existing_filename(filename, filename_, PKGDATADIR"keymap", NULL);
	if (!parse_config_file(keys_configlist, filename__))
		DEBUG_MSG("rcfile_parse_keys: failed; cannot locate 'keymap' file\n");
	free_configlist(keys_configlist);
	g_free(filename);
	g_free(filename_);
	g_free(filename__);
}

static void hash_table_key_value_free(gpointer data) {
	DEBUG_MSG("hash_table_key_value_free: for %s\n", (gchar *)data);
	g_free(data);
}

/* this function should be called afted the func_hashtable was initialized */
void keymap_init(void) {
	GList *keys_list = NULL;
	rcfile_parse_keys(&keys_list);
	main_v->key_hashtable = g_hash_table_new_full(g_str_hash, g_str_equal,hash_table_key_value_free,hash_table_key_value_free);
	GList *tmp2list = g_list_first(keys_list);
	while (tmp2list) {
		gchar **strarr;
		strarr = (gchar **) tmp2list->data;
		if (count_array(strarr) >= 2) {
			if ( strlen(strarr[1]) && strlen(strarr[0]) && g_hash_table_lookup(main_v->func_hashtable, strarr[0]) ) {
				gchar *keyseq, *func_name;
				keyseq = g_strdup(strarr[1]);
				func_name = g_strdup(strarr[0]);
				g_hash_table_insert(main_v->key_hashtable, keyseq, func_name);
				DEBUG_MSG("keys_init: map [func=%s] to [keys_seq=%s]\n", func_name, keyseq);
			}
		}
		tmp2list = g_list_next(tmp2list);
	}

	/* we donot need keys_list anymore */
	{
		GList *tmplist = g_list_first(keys_list);
		while(tmplist) {
			/* TODO: is this enought to free everythign? */
			void *cli = tmplist->data;
			g_free(cli);
			tmplist = g_list_next(tmplist);
		}
		g_list_free(keys_list);
	}
}

#endif /* SNOOPER2 */
