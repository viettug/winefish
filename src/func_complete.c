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

#include <gtk/gtk.h>

#include <string.h> /* strstr(), strlen() */

#include "bluefish.h"
#include "snooper.h"

static gboolean find_char( gunichar ch, gchar *data ) {
	return ( strchr( data, ch ) != NULL );
}

static void completion_popup_menu_init() {
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

gboolean completion_popup_menu( GtkWidget *widget, GdkEventKey *kevent, Tdocument *doc ) {

	if ( !main_v->completion.window ) {
		completion_popup_menu_init();
	}

	/* store the window pointer */
	main_v->completion.bfwin = BFWIN(doc->bfwin)->current_document;
	
	/* reset the popup content */
	GtkTreeModel *model;
	{
		gchar *buf = NULL;/* gap_command(widget, kevent, doc); */
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
			return FALSE;
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
				return FALSE;
			}
		}
		/* shouldnot remove completion_list_i after using it */

		/* Adds the second GList onto the end of the first GList.
		Note that the elements of the second GList are not copied.
		They are used directly. */

		/* there is *ONLY* one word and the user reach end of this word */
		if ( (main_v->completion.show != COMPLETION_FIRST_CALL) &&  g_list_length(completion_list) ==1 && strlen (completion_list->data) == strlen(buf) ) {
			return FALSE;
		}

		main_v->completion.cache = buf;
		
		GtkListStore *store;
		GtkTreeIter iter;
		
		/* create list of completion words */
		DEBUG_MSG("completion_popup_menu: rebuild the word list\n");
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
	if (win) {
		/* get the position of text view window relative to its parents */
		gdk_window_get_geometry(win, &root_x, &root_y, NULL, NULL, NULL);
		x += root_x;
		y += root_y;
	} else {/* when does this case take place ?*/
		g_print("completion_popup_menu: not a window\n");
		return FALSE;
	}

	gtk_window_move (GTK_WINDOW(main_v->completion.window), x+16, y);
	{/* select the first item; need*not* for the first time */
		GtkTreePath *treepath = gtk_tree_path_new_from_string("0");
		if (treepath) {
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(main_v->completion.treeview), treepath, NULL, FALSE);
			gtk_tree_path_free(treepath);
		}
	}
	
	gtk_widget_show_all(main_v->completion.window);
	return TRUE;
}
