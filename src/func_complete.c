/* $Id$ */

/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 *
 * Copyright (C) 2006 kyanh <kyanh@o2.pl>
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

#define DEBUG

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <string.h> /* strstr(), strlen() */

#include "bluefish.h"
#include "snooper.h"

static gboolean find_char( gunichar ch, gchar *data ) {
	return ( strchr( data, ch ) != NULL );
}

static void func_complete_init() {
	DEBUG_MSG("func_complete_init: started\n");
	main_v->completion.window = gtk_window_new( GTK_WINDOW_POPUP );
	main_v->completion.treeview = gtk_tree_view_new();
	/* add column */
	{
		GtkCellRenderer *renderer;
		GtkTreeViewColumn *column;
		renderer = gtk_cell_renderer_text_new ();
		/* g_object_set(renderer, "background", "#ADD8E6", NULL); */
		column = gtk_tree_view_column_new_with_attributes ("", renderer, "text", 0, NULL);
		gtk_tree_view_column_set_min_width(column,100);
		gtk_tree_view_append_column (GTK_TREE_VIEW(main_v->completion.treeview), column);
	}

	/* add scroll */
	GtkWidget *scrolwin;
	scrolwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
	
	/* setting for this treeview */
	gtk_tree_view_set_enable_search( GTK_TREE_VIEW(main_v->completion.treeview), FALSE );
	gtk_tree_view_set_headers_visible ( GTK_TREE_VIEW(main_v->completion.treeview), FALSE );
	
	/* add the treeview to scroll window */
	gtk_container_add(GTK_CONTAINER(scrolwin), main_v->completion.treeview);
	/* TODO: better size handler */
	gtk_widget_set_size_request(scrolwin, -1, 100);

	GtkWidget *frame;
	frame = gtk_frame_new(NULL);
	gtk_container_add( GTK_CONTAINER( frame ), scrolwin );
	gtk_container_add( GTK_CONTAINER( main_v->completion.window ), frame );
}

gint func_complete_hide() {
	DEBUG_MSG("func_complete_hide: started\n");
	if ( main_v->completion.window ) {
		gtk_widget_hide_all(main_v->completion.window);
		main_v->completion.show = COMPLETION_WINDOW_HIDE;
		return 1;
	}
	return 0;
}

gint func_complete_show( GtkWidget *widget_, Tbfwin *bfwin ) {
	DEBUG_MSG("func_complete_show: started\n");

	Tdocument *doc = bfwin->current_document;
	GtkWidget *widget = doc->view;
	if ( ! GTK_WIDGET_HAS_FOCUS(widget) ) {
		return 0;
	}

	if ( !main_v->completion.window ) {
		func_complete_init();
	}
	/* store the window pointer */
	main_v->completion.bfwin = BFWIN(doc->bfwin)->current_document;

	/* reset the popup content */
	GtkTreeModel *model;
	{
		gchar *buf = NULL;/* func_complete_eat(widget, kevent, doc); */
		{/* get text at doc's iterator */
			GtkTextMark * imark;
			GtkTextIter itstart, iter, maxsearch;

			imark = gtk_text_buffer_get_insert( doc->buffer );
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, imark );
			itstart = iter;
			maxsearch = iter;
			gtk_text_iter_backward_chars( &maxsearch, 50 ); /* 50 chars... may it be too long? */
			
			if ( gtk_text_iter_backward_find_char( &itstart, ( GtkTextCharPredicate ) find_char, GINT_TO_POINTER( "\\" ), &maxsearch ) ) {
				maxsearch = iter; /* re-use maxsearch */
				buf = gtk_text_buffer_get_text( doc->buffer, &itstart, &maxsearch, FALSE );
			}
		}

		if (!buf || ( (main_v->completion.show != COMPLETION_FIRST_CALL) && (strlen(buf) < 3)) ) {
			func_complete_hide();
			return 0;
		}

		GList *completion_list_0 = NULL;
		GList *completion_list_1 = NULL;
		GList *completion_list = NULL;

		completion_list_0 = g_completion_complete(main_v->props.completion, buf, NULL);

		if (completion_list_0) {
			completion_list = g_list_copy(completion_list_0);
			completion_list_1 = g_completion_complete(main_v->props.completion_s, buf, NULL);
			if (completion_list_1) {
				completion_list = g_list_concat(completion_list, g_list_copy(completion_list_1));
				g_list_sort(completion_list, (GCompareFunc)strcmp);
			}
		}else {
			completion_list_1 = g_completion_complete(main_v->props.completion_s, buf, NULL);
			if (completion_list_1) {
				completion_list = g_list_copy(completion_list_1);
				g_list_sort(completion_list, (GCompareFunc)strcmp);
			}else{
				func_complete_hide();
				return 0;
			}
		}
		/* shouldnot remove completion_list_i after using it */

		/* Adds the second GList onto the end of the first GList.
		Note that the elements of the second GList are not copied.
		They are used directly. */

		/* there is *ONLY* one word and the user reach end of this word */
		if ( (main_v->completion.show != COMPLETION_FIRST_CALL) &&  g_list_length(completion_list) ==1 && strlen (completion_list->data) == strlen(buf) ) {
			func_complete_hide();
			return 0;
		}

		main_v->completion.cache = buf;
		
		GtkListStore *store;
		GtkTreeIter iter;

		/* create list of completion words */
		DEBUG_MSG("func_complete_show: rebuild the word list for treeview\n");
		store = gtk_list_store_new (1, G_TYPE_STRING);

		GList *item;
		item = g_list_first(completion_list);
		while (item) {
			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter, 0, (gchar *)item->data, -1);
			item = g_list_next(item);
		}
		g_list_free(completion_list);
		model = GTK_TREE_MODEL(store);
	}

	/* the old model will be ignored */
	gtk_tree_view_set_model(GTK_TREE_VIEW(main_v->completion.treeview), model);
	g_object_unref(model);

	/* Moving the popup window */
	gint root_x, x, root_y, y;
	GdkWindow *win;
	{
		GdkRectangle rect;
		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, gtk_text_buffer_get_insert( doc->buffer ) );	
		gtk_text_view_get_iter_location( GTK_TEXT_VIEW(widget), &iter, &rect);
		gtk_text_view_buffer_to_window_coords( GTK_TEXT_VIEW(widget), GTK_TEXT_WINDOW_WIDGET, rect.x, rect.y, &x, &y );
	}
	
	gtk_window_get_position(GTK_WINDOW(BFWIN(doc->bfwin)->main_window), &root_x, &root_y);
	x += root_x;
	y += root_y;
	win = gtk_text_view_get_window(GTK_TEXT_VIEW(widget), GTK_TEXT_WINDOW_WIDGET);

	/* get the position of text view window relative to its parents */
	gdk_window_get_geometry(win, &root_x, &root_y, NULL, NULL, NULL);
	x += root_x;
	y += root_y;

	gtk_window_move (GTK_WINDOW(main_v->completion.window), x+16, y);
	{/* select the first item; need*not* for the first time */
		GtkTreePath *treepath = gtk_tree_path_new_from_string("0");
		if (treepath) {
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(main_v->completion.treeview), treepath, NULL, FALSE);
			gtk_tree_path_free(treepath);
		}
	}
	
	gtk_widget_show_all(main_v->completion.window);

	main_v->completion.show = COMPLETION_AUTO_CALL;

	return 1;
}

gint func_complete_delete() {
	if (main_v->completion.show != COMPLETION_DELETE ) {
		return 0;
	}
	DEBUG_MSG("doc: delete item from popup\n");
	/* delete stuff:
	- get current position/command
	- hide the popup window
	- delete the selected item. this alters `main_v->props.completion(_s)->items' (GCompletion)
	- rebuild the popup
	- recaculate: func_complete_show(widget, kevent, doc)
	*/
	/* get current selection */
	GtkTreePath *treepath = NULL;
	GtkTreeModel *model;
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_v->completion.treeview));
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(main_v->completion.treeview), &treepath, NULL);
	gchar *user_selection = NULL;
	if (treepath) {
		GtkTreeIter iter;
		GValue *val = NULL;
		gtk_tree_model_get_iter(model, &iter, treepath);
		gtk_tree_path_free(treepath);
		val = g_new0(GValue, 1);
		gtk_tree_model_get_value(model, &iter, 0, val);
		user_selection = g_strdup((gchar *) (g_value_peek_pointer(val)));
		g_value_unset (val);
		g_free (val);
		DEBUG_MSG("completion: user selected '%s'\n", user_selection);
	}
	if (user_selection) {
		/* hide the windows */
		gtk_widget_hide_all( main_v->completion.window );
	
		/* now delete */
		DEBUG_MSG("completion: delete an item '%s'\n", user_selection);
		GList *tmp;
		GList *first_word = NULL;
		gboolean word_removed = FALSE;
		/* --- how to remove an item `foobar' ---
		we first have a list prefixed by `foobar'.
		get the first item of this list and remove the item from the main list by `g_list_remove_link'.
		finally, remove the item from the main list.
		*/
		if (!word_removed) {
			tmp = g_completion_complete(main_v->props.completion,user_selection,NULL);
			if (tmp) {
				first_word = g_list_first(tmp);
				if (strcmp((gchar*)first_word->data, user_selection) == 0 ) {
					g_list_remove_link(tmp,first_word);
					DEBUG_MSG("completion: first word : %s\n",(gchar*)first_word->data);
					g_completion_remove_items(main_v->props.completion, first_word);
					DEBUG_MSG("completion: ...item deleted from global list\n");
					word_removed = TRUE;
				}
			}
		}
		if (!word_removed) {
			tmp = g_completion_complete(main_v->props.completion_s,user_selection,NULL);
			/* note the session list wasnot sorted. if we remove an item, add it again to
			the session list (by `func_complete_eat', the list is unordered. so we have to
			search through the session_list. we donot use the g_list_find_custom as
			tmp is *GCompletion --- !!!! */
			/* TODO: optimized */
			first_word = g_list_last(tmp);
			gint count=0;
			while (first_word && (strcmp((gchar*)first_word->data, user_selection) != 0)) {
				first_word = first_word->prev;
				count ++;
			}
			DEBUG_MSG("completion: passed %d words\n",count);
			/* we must ensure the the first item == user_selection */
			if ( first_word ) {
				g_list_remove_link(tmp,first_word);
				DEBUG_MSG("completion: first word : %s\n",(gchar*)first_word->data);
				g_completion_remove_items(main_v->props.completion_s, first_word);
				DEBUG_MSG("completion: ...item deleted from session list\n");
				word_removed = TRUE;
			}
		}
		if(word_removed) {
			gchar *tmpstr = g_strdup_printf(_("deleted '%s'"), user_selection);
			/* statusbar_message(doc->bfwin, tmpstr, 2000); */
			g_free(tmpstr);
		}
		g_free(user_selection);
		main_v->completion.show = COMPLETION_AUTO_CALL;
	}else{
		main_v->completion.show = COMPLETION_WINDOW_HIDE;
	}
	return 1;
}

gint func_complete_move(GdkEventKey *kevent) {
	GtkTreeModel *model;
	gint maxnode, index;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_v->completion.treeview));
	maxnode = gtk_tree_model_iter_n_children(model, NULL);
	main_v->completion.show = SNOOPER_COMPLETION_MOVE_TYPE(kevent->keyval);
	
	if (maxnode ==1) {
		func_complete_hide();
		return 0;
	}
	GtkTreePath *treepath = NULL;
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(main_v->completion.treeview), &treepath, NULL);

	/* TODO: remove this check */
	if (treepath) {
		{
			gchar *path;
			path = gtk_tree_path_to_string(treepath);
			index = atoi(path);
			g_free(path);
		}
		if (main_v->completion.show == COMPLETION_WINDOW_UP) {
			index --;
		} else if (main_v->completion.show == COMPLETION_WINDOW_PAGE_UP) {
			index = index - 5;
		} else if (main_v->completion.show == COMPLETION_WINDOW_DOWN) {
			index ++;
		} else {
			index = index + 5;
		}

		if (index < 0) {
			index = 0;
		} else if (index >= maxnode) {
			index = maxnode -1;
		}
		{
			gchar *tmpstr = g_strdup_printf("%d", index);
			treepath = gtk_tree_path_new_from_string(tmpstr);
			g_free(tmpstr);
		}
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(main_v->completion.treeview), treepath, NULL, FALSE);
		gtk_tree_path_free(treepath);
	}
	main_v->completion.show = COMPLETION_WINDOW_SHOW;
	return 1;
}

gint func_complete_do(Tdocument *doc) {
	/* orignal: key-release event */
	func_complete_hide();
	if ( main_v->completion.bfwin != BFWIN(doc->bfwin)->current_document ) {
		return 0;
	}
	/* inserting staff */
	/* get user's selection */
	/* get path and iter */
	GtkTreePath *treepath = NULL;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_v->completion.treeview));
	/* lucky, model is*NOT* NULL -- we skip a check :) */

	gtk_tree_view_get_cursor(GTK_TREE_VIEW(main_v->completion.treeview), &treepath, NULL);
	if (treepath) {
		GtkTreeIter iter;
		gchar *user_selection = NULL;
		GValue *val = NULL;
		gint i, len, cache_len;

		gtk_tree_model_get_iter(model, &iter, treepath);
		gtk_tree_path_free(treepath);
#ifdef ENABLE_FIX_UNIKEY_GTK
		GtkTextIter cursor_iter, tmp_iter;
		GtkTextMark *imark;
		gchar *test_buf;
		imark = gtk_text_buffer_get_insert( doc->buffer );
		gtk_text_buffer_get_iter_at_mark( doc->buffer, &cursor_iter, imark );
		tmp_iter = cursor_iter;
		gtk_text_iter_backward_chars(&tmp_iter, 1);
		test_buf = gtk_text_buffer_get_text(doc->buffer, &tmp_iter, &cursor_iter, FALSE);
		if (test_buf[0] == ' ') {
			gtk_text_buffer_delete( doc->buffer, &tmp_iter, &cursor_iter);
		}
#endif /* ENABLE_FIX_UNIKEY_GTK */
		val = g_new0(GValue, 1);
		gtk_tree_model_get_value(model, &iter, 0, val);
		user_selection = g_strdup((gchar *) (g_value_peek_pointer(val)));
		g_value_unset (val);
		g_free (val);
		/* inserting */
		cache_len = strlen(main_v->completion.cache);
		len = strlen(user_selection);
		if ( len == cache_len ) {
			return TRUE;
		} else if (len > cache_len ){
			len = len - cache_len;
			gchar *retval = g_malloc((len+1) * sizeof(char));
			for (i=0; i< len; i++) {
				retval[i] = user_selection[cache_len + i];
			}
			retval[len] = '\0';
			GtkTextIter iter;
			GtkTextMark *imark;
			imark = gtk_text_buffer_get_insert( doc->buffer );
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, imark );
			gtk_text_buffer_insert( doc->buffer, &iter, retval, -1 );
			g_free(retval);
		}
		g_free(user_selection);
	}
	return 1;
}

/**
* kyanh <kyanh@o2.pl>
* add user command to autocompletion list.
* this causes the list be sorted again ==> slower performance*
*/

gint func_complete_eat( GtkWidget *widget, GdkEventKey *kevent, Tdocument *doc ) {
	guint32 character = gdk_keyval_to_unicode( kevent->keyval );
	if ( (kevent->keyval != GDK_Return) && (character==0 || !strstr(DELIMITERS, kevent->string)) ) {
		return 0;
	}
	gchar *buf=NULL;
	GtkTextMark * imark;
	GtkTextIter itstart, iter, maxsearch;
	
	imark = gtk_text_buffer_get_insert( doc->buffer );
	gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, imark );
	
	itstart = iter;
	gtk_text_iter_backward_chars(&itstart, 1);
	/* buf = gtk_text_buffer_get_text(doc->buffer, &itstart, &iter, FALSE); */
	/* if ( strlen(buf)==1 && strstr(DELIMITERS, buf) ) { */
	iter = itstart;
	maxsearch = itstart;
	gtk_text_iter_backward_chars( &maxsearch, COMMAND_MAX_LENGTH );
	if ( gtk_text_iter_backward_find_char( &itstart, ( GtkTextCharPredicate ) find_char, GINT_TO_POINTER( "\\" ), &maxsearch ) ) {
		int ovector[ 3 ], ret;
		maxsearch = iter; /* reuse maxsearch */
		buf = gtk_text_buffer_get_text( doc->buffer, &itstart, &maxsearch, FALSE );
		ret = pcre_exec( main_v->anycommand_regc, NULL, buf, strlen( buf ), 0, PCRE_ANCHORED, ovector, 3 );
		if ( ret > 0 ) {
			/* buf is now the user command */
			DEBUG_MSG("func_complete_eat: found command = [%s]\n", buf);
			GList *tmplist = NULL;
			tmplist = g_list_find_custom(main_v->props.completion->items, buf, (GCompareFunc)strcmp);
			/* g_completion_complete(main_v->props.completion, buf, NULL); */
			if ( !tmplist ) {
				tmplist = g_list_find_custom(main_v->props.completion_s->items, buf, (GCompareFunc)strcmp);
				/* g_completion_complete(main_v->props.completion_s, buf, NULL); */
				if (!tmplist) {
					DEBUG_MSG("func_complete_eat: new command = [%s]\n", buf);
					gchar *tmpstr = g_strdup(buf);
					tmplist = g_list_append(tmplist, tmpstr);
					/* Cannot use: g_list_append(tmplist, buf); as buf is freed. See below. */
					g_completion_add_items(main_v->props.completion_s, tmplist);
					DEBUG_MSG("func_complete_eat: completion_s data = %s\n", (gchar *)main_v->props.completion_s->items->data);
					g_list_free(tmplist);
				}
			}
		}
	}
	g_free(buf);
	return 1;
}
