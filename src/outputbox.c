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

#define DEBUG

#include "outputbox_cfg.h"

#include <gtk/gtk.h>
#include <string.h> /* strlen() */

#include "bluefish.h"
#include "bf_lib.h"
#include "document.h"
#include "gtk_easy.h"
#include "stringlist.h"
#include "pixmap.h"

#include "outputbox.h" /* myself */
#include "gui.h" /* statusbar_message */

#ifdef __KA_BACKEND__
#include <sys/types.h>
#include <signal.h> /* kill() */
#include <sys/stat.h> /* open() */
#include <fcntl.h> /* kyanh, open() */ 
/* kyanh, 20050301,
Thanks to M. Garoche <...@easyconnect.fr>
Move from <wait.h> to <sys/wait.h>
*/
#include <sys/wait.h> /* wait(), waitid() */
#include <regex.h>
#include <stdlib.h>
#include "outputbox_ka.h" /* backend by ka */
#endif /* __KA_BACKEND__ */

#ifdef __BF_BACKEND__
#include "outputbox_bf.h"
#endif /* __BF_BACKEND__ */

static void ob_lview_row_activated_lcb( GtkTreeView *tree, GtkTreePath *path, GtkTreeViewColumn *column, Toutputbox *ob )
{
	GtkTreeIter iter;
	gchar *file, *line;
	gint lineval;
	gtk_tree_model_get_iter( GTK_TREE_MODEL( ob->lstore ), &iter, path );
	gtk_tree_model_get( GTK_TREE_MODEL( ob->lstore ), &iter, 3, &file, 1, &line, -1 );
	DEBUG_MSG( "ob_lview_row_activated_lcb, file=%s, line=%s\n", file, line );
	if ( file && strlen( file ) ) {
		doc_new_with_file( ob->bfwin, file, FALSE, FALSE );
	}
	if ( line && strlen( line ) ) {
		lineval = atoi( line );
		flush_queue();
		doc_select_line( ob->bfwin->current_document, lineval, TRUE );
	}
	g_free( line );
	g_free( file );
}

/* TODO: hide only this page */
static void outputbox_close_lcb( GtkWidget *widget, Toutputbox *ob )
{
	setup_toggle_item_from_widget( ob->bfwin->menubar, N_( "/View/View Outputbox" ), FALSE );
	gtk_widget_hide( ob->bfwin->ob_hbox );
}

/* kyanh */
static void ob_lview_copy_line_lcb (GtkWidget *widget, Toutputbox *ob ) {
	GtkTreePath *treepath;
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(ob->lview), &treepath, NULL);
	if (treepath) {
		GtkTreeIter iter;
		gtk_tree_model_get_iter(GTK_TREE_MODEL( ob->lstore ), &iter, treepath);
		gchar *message;
		gtk_tree_model_get( GTK_TREE_MODEL( ob->lstore ), &iter, 2, &message, -1 );
		DEBUG_MSG("ob_lview_copy_line_lcb: message = '%s'\n",message);
		gtk_clipboard_set_text  (gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), message, -1);
		g_free(message);
		gtk_tree_path_free(treepath);
	}
}

/* kyanh */
static GtkWidget *ob_lview_create_popup_menu (Toutputbox *ob)
{
	DEBUG_MSG("ob_lview_create_popup_menu: fetching value = %d\n", ob->OB_FETCHING);
	if (ob->OB_FETCHING != OB_IS_READY) {
		DEBUG_MSG("ob_lview_create_popup_menu: return without creating any menu\n");
		return NULL;
	}

	GtkWidget *menu;
	GtkWidget *menu_item;
	menu = gtk_menu_new ();
	menu_item = gtk_menu_item_new_with_label(_("copy this line"));
	g_signal_connect( menu_item, "activate", G_CALLBACK( ob_lview_copy_line_lcb ), ob );
	gtk_widget_show (menu_item);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	
	menu_item = gtk_menu_item_new_with_label(_("hide box"));
	g_signal_connect( menu_item, "activate", G_CALLBACK( outputbox_close_lcb ), ob );
	gtk_widget_show (menu_item);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	return menu;
}

static gboolean ob_lview_button_press_lcb( GtkWidget *widget, GdkEventButton *bevent, Toutputbox *ob ) {
	DEBUG_MSG("ob_view_button_release_lcb: event = %d\n", bevent->button);
	if (bevent->button == 3) {
		GtkWidget * menu = ob_lview_create_popup_menu(ob);
		if (menu) {
			gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, bevent->button, bevent->time);
		}
	} else if(bevent->button == 1) {
		/*
		GtkTreePath *treepath;
		gtk_tree_view_get_cursor(GTK_TREE_VIEW(ob->lview), &treepath, NULL);
		if (treepath) {
			GtkTreeIter iter;
			gtk_tree_model_get_iter(GTK_TREE_MODEL( ob->lstore ), &iter, treepath);
			gchar *filepath;
			gtk_tree_model_get( GTK_TREE_MODEL( ob->lstore ), &iter, 3, &filepath, -1 );
			if(filepath) {
				statusbar_message(ob->bfwin, filepath, 1000);
			}
			gtk_tree_path_free(treepath);
		}*/
	}
	return FALSE;
}

/* kyanh, added, 20050227
 Send some specific message to outputbox.
 Please DONOT use for general purpose :!

 @markup: should be: b, i, u
*/
void outputbox_message( Toutputbox *ob, const char *string, const char *markup )
{
	DEBUG_MSG("outputbox_message: %s\n", string);
	GtkTreeIter iter;
	gchar *tmpstr = g_markup_escape_text(string,-1);
	if (markup && strlen(markup)) {
		tmpstr = g_strdup_printf("&gt; <%s>%s</%s>", markup, string, markup);
	}else{
		tmpstr = g_strdup_printf("&gt; %s", string);
	}
	gtk_list_store_append( GTK_LIST_STORE( ob->lstore ), &iter );
	gtk_list_store_set( GTK_LIST_STORE( ob->lstore ), &iter, 2, tmpstr, -1 );
	g_free(tmpstr);

	/* TODO: Scroll as an Optional */
	/* The Outputbox may *NOT* be shown before scrolling :) */
	/* kyanh, added, 20050301 */
	GtkTreePath *treepath = gtk_tree_model_get_path( GTK_TREE_MODEL( ob->lstore ), &iter );
	gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( ob->lview ), treepath, NULL, FALSE /* skip align */, 0, 0 );
	gtk_tree_path_free( treepath );
	flush_queue();
}

#ifdef __KA_BACKEND__
/*
when this function is call, the function continue_execute may be running :)
*/
static void clean_up_child_process (gint signal_number)
{
	/* Clean up the child process. */
	gint status;
	DEBUG_MSG("clean_up_child_process: starting...\n");
	/* waitpid(-1,&status,WNOHANG); */
	wait(&status);
	/* Store its exit status in a global variable. */
	child_exit_status = status;
	DEBUG_MSG("clean_up_child_process: finished.\n");
}
#endif /* __KA_BACKEND__ */

static void ob_notebook_switch_page_lcb(GtkNotebook *notebook, GtkNotebookPage *page, gint page_num, Tbfwin *bfwin)
{
	Toutputbox *ob;
	ob = outputbox_get_box(bfwin, page_num);
	DEBUG_MSG("ob_notebook_switch_page_lcb: select page %d, %p\n", page_num, ob);
	if (ob && (ob->OB_FETCHING < OB_IS_READY)) {
		DEBUG_MSG("ob_notebook_switch_page_lcb: Stop TRUE, ob_fetching = %d\n", ob->OB_FETCHING);
		outputbox_set_status(ob, TRUE, TRUE);
	}else{
#ifdef DEBUG
		if(ob) {
			DEBUG_MSG("ob_notebook_switch_page_lcb: Strop FALSE, ob_fetching = %d\n", ob->OB_FETCHING);
		}
#endif
		outputbox_set_status(ob, FALSE, TRUE);
	}
}

/* prepare frontent for outputbox */
static void outputbox_init_frontend(Tbfwin *bfwin) {
#ifdef __KA_BACKEND__
	/* Handle SIGCHLD by calling clean_up_child_process. */
	struct sigaction sigchld_action;
	memset (&sigchld_action, 0, sizeof (sigchld_action));
	sigchld_action.sa_handler = &clean_up_child_process;
	sigaction (SIGCHLD, &sigchld_action, NULL);
#endif /* __KA_BACKEND__ */

	DEBUG_MSG("outputbox_init_frontend: entering...\n");
	/* create the hbox for ob */
	bfwin->ob_hbox = gtk_hbox_new( FALSE, 0 );
	gtk_paned_add2( GTK_PANED( bfwin->vpane ), bfwin->ob_hbox );
	gtk_paned_set_position( GTK_PANED( bfwin->vpane ), ( gint ) ( bfwin->vpane->allocation.height * 0.7 ) );

	/* notebook for ob */
	bfwin->ob_notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(bfwin->ob_notebook), TRUE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(bfwin->ob_notebook), FALSE);
	gtk_notebook_set_tab_hborder(GTK_NOTEBOOK(bfwin->ob_notebook), 0); /*deprecated*/
	gtk_notebook_set_tab_vborder(GTK_NOTEBOOK(bfwin->ob_notebook), 0); /*deprecated*/
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(bfwin->ob_notebook),GTK_POS_LEFT);

	gtk_box_pack_start(GTK_BOX(bfwin->ob_hbox), bfwin->ob_notebook, TRUE, TRUE, 0);
	DEBUG_MSG("outputbox_init_frontend: added notebook %p\n", bfwin->ob_notebook);

	g_signal_connect(G_OBJECT(bfwin->ob_notebook),"switch-page",G_CALLBACK(ob_notebook_switch_page_lcb), bfwin);

	setup_toggle_item_from_widget( bfwin->menubar, N_( "/View/View Outputbox" ), TRUE );
}

/*
gboolean ob_lview_move_cursor_lcb(GtkTreeView *treeview,GtkMovementStep arg1,gint arg2,gpointer user_data)
{
	DEBUG_MSG("ob_lview_move_cursor_lcb: hello.......................................................\n");
	return TRUE;
}
*/

Toutputbox *outputbox_new_box( Tbfwin *bfwin, const gchar *title )
{
	if (!bfwin->ob_hbox) {
		outputbox_init_frontend(bfwin);
	}
	/* create the backend */
	GtkTreeViewColumn * column;
	GtkWidget *scrolwin;
	GtkCellRenderer *renderer;
#ifdef OB_WITH_IMAGE
	GtkWidget *vbox2, *but, *image;
#endif /* OB_WITH_IMAGE */
	Toutputbox *ob;

	ob = g_new0( Toutputbox, 1 );
	DEBUG_MSG( "init_output_box, created %p\n", ob );
	ob->OB_FETCHING = OB_IS_READY;
	ob->bfwin = bfwin;
	ob->def = NULL;
#ifdef __KA_BACKEND__
	ob->pid=0;
#endif

/* LVIEW settings */
	/* basename, linenumber, message, filename (hidden) */
	ob->lstore = gtk_list_store_new ( 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING );
	ob->lview = gtk_tree_view_new_with_model( GTK_TREE_MODEL( ob->lstore ) );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW (ob->lview) , FALSE );
	g_object_unref( ob->lstore );
	/* the view widget now holds the only reference,
	if the view is destroyed, the model will be destroyed */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ( NULL, renderer, "markup", 0, NULL );
	gtk_tree_view_append_column ( GTK_TREE_VIEW( ob->lview ), column );
	column = gtk_tree_view_column_new_with_attributes ( NULL, renderer, "text", 1, NULL );
	gtk_tree_view_append_column ( GTK_TREE_VIEW( ob->lview ), column );
	column = gtk_tree_view_column_new_with_attributes ( NULL, renderer, "markup", 2, NULL );
	gtk_tree_view_append_column ( GTK_TREE_VIEW( ob->lview ), column );

	g_signal_connect( G_OBJECT( ob->lview ), "row-activated", G_CALLBACK( ob_lview_row_activated_lcb ), ob );
	g_signal_connect( G_OBJECT( ob->lview ), "button-press-event", G_CALLBACK( ob_lview_button_press_lcb ), ob );
	/* g_signal_connect( G_OBJECT( ob->lview ), "move-cursor", G_CALLBACK( ob_lview_move_cursor_lcb ), ob ); */
/* end LVIEW settings */

#ifdef OB_WITH_IMAGE
	/* but button for closing ... */
	vbox2 = gtk_vbox_new( FALSE, 0 );
	but = gtk_button_new();
	image = new_pixmap( 4 );
	gtk_widget_show( image );
	gtk_container_add( GTK_CONTAINER( but ), image );
	gtk_container_set_border_width( GTK_CONTAINER( but ), 0 );
	gtk_widget_set_size_request( but, 16, 16 );
	g_signal_connect( G_OBJECT( but ), "clicked", G_CALLBACK( outputbox_close_clicked_lcb ), ob );
	gtk_box_pack_start( GTK_BOX( ob->hbox ), vbox2, FALSE, FALSE, 0 );
	gtk_box_pack_start( GTK_BOX( vbox2 ), but, FALSE, FALSE, 0 );
#endif /* OB_WITH_IMAGE */

	/* scrolling */
	scrolwin = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolwin ), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

	gtk_container_add( GTK_CONTAINER( scrolwin ), ob->lview );
	gtk_widget_set_size_request( scrolwin, 150, 150 ); /* TODO: autosize */
	
	GtkWidget *label = gtk_label_new(title);
	ob->page_number = gtk_notebook_append_page(GTK_NOTEBOOK(ob->bfwin->ob_notebook), scrolwin, label);

	/* need eventboxgtk_tooltips_set_tip( main_v->tooltips, label, "asdfasdfasfsaf", NULL ); */

	DEBUG_MSG("outputbox_new_box: added page %p\n", scrolwin);
	return ob;
}

void outputbox(Tbfwin *bfwin, gpointer *ob, const gchar *title, gchar *pattern, gint file_subpat, gint line_subpat, gint output_subpat, gchar *command, gint show_all_output )
{
	DEBUG_MSG("outputbox: starting...\n");
	if ( *ob ) {
		setup_toggle_item_from_widget(bfwin->menubar, N_("/View/View Outputbox"), TRUE); /* fix BUG[200503]#25 */
		/* GtkWidget *label = gtk_label_new(title);
		gtk_notebook_set_tab_label(label); */
	} else {
		*ob = outputbox_new_box(bfwin,title);
	}

	gtk_widget_show_all( bfwin->ob_hbox );
	gtk_notebook_set_current_page(GTK_NOTEBOOK(bfwin->ob_notebook), OUTPUTBOX(*ob)->page_number);

	gtk_list_store_clear( GTK_LIST_STORE( OUTPUTBOX(*ob)->lstore ) );
	flush_queue();

	if (!command) {
		/* TODO: check for valid command */
		outputbox_message(*ob, _("empty command"), "b");
		return;
	}
	{
		gchar *format_str;
		if ( bfwin->project && ( bfwin->project->view_bars & PROJECT_MODE ) ) {
			format_str = g_strdup_printf(_("%s # project mode: ON"), command );
		} else {
			format_str = g_strdup_printf(_("%s # project mode: OFF"), command);
		}
		outputbox_message( *ob, format_str, "i" );
		g_free( format_str );
		flush_queue();
	}
	if (show_all_output & OB_NEED_SAVE_FILE) {
		file_save_cb( NULL, bfwin );
		if ( !bfwin->current_document->filename ) {
			outputbox_message(*ob, _("files wasnot saved. tool canceled"), "b");
			flush_queue();
			return;
		}
	}

	if ( OUTPUTBOX(*ob)->OB_FETCHING < OB_IS_READY ) { /* stop older output box */
		DEBUG_MSG("outputbox: current OB_FETCHING=%d < %d\n", OUTPUTBOX(*ob)->OB_FETCHING,OB_IS_READY);
		outputbox_message( *ob, _("tool is running. press Escape to stop it first."), "b" );
		flush_queue();
		return;
	}

	OUTPUTBOX(*ob)->basepath_cached = NULL;
	OUTPUTBOX(*ob)->basepath_cached_color = FALSE;
	OUTPUTBOX(*ob)->OB_FETCHING = OB_GO_FETCHING;
	OUTPUTBOX(*ob)->def = g_new0( Toutput_def, 1 );
	OUTPUTBOX(*ob)->def->pattern = g_strdup( pattern );
	OUTPUTBOX(*ob)->def->file_subpat = file_subpat;
	OUTPUTBOX(*ob)->def->line_subpat = line_subpat;
	OUTPUTBOX(*ob)->def->output_subpat = output_subpat;
	OUTPUTBOX(*ob)->def->show_all_output = show_all_output;
	regcomp( &OUTPUTBOX(*ob)->def->preg, OUTPUTBOX(*ob)->def->pattern, REG_EXTENDED );
	/* TODO  : check for valid preg */
	OUTPUTBOX(*ob)->def->command = g_strdup( command );
	DEBUG_MSG("outputbox: starting command: %s\n", command);
#ifdef __KA_BACKEND__
	/* kyanh */
	OUTPUTBOX(*ob)->retfile = NULL;
	OUTPUTBOX(*ob)->io_channel = NULL;
	OUTPUTBOX(*ob)->pollID = 0;
	OUTPUTBOX(*ob)->pid = 0;
#endif /* __KA_BACKEND__ */
	run_command(*ob);
	if (OUTPUTBOX(*ob)->OB_FETCHING == OB_ERROR) {
		OUTPUTBOX(*ob)->OB_FETCHING = OB_IS_READY;
	}
	/* gtk_widget_show_all(ob->hbox); */
	DEBUG_MSG("outputbox: finished\n");
}

/* this function call from gui.c (when program exist) and
from menu (toggle). for that, the `ob' may be NULL;
*/
void outputbox_stop(Toutputbox *ob) {
	if (!ob) {
		return;
	}
	DEBUG_MSG("outputbox_stop: starting...\n");
	/* menuitem_set_sensitive(ob->bfwin->menubar, N_("/External/Stop..."), FALSE); */
	/* flush_queue(); */
	if (ob->OB_FETCHING == OB_GO_FETCHING || ob->OB_FETCHING == OB_IS_FETCHING) {
#ifdef __KA_BACKEND__
		if (ob->pid) {
#endif /* __KA_BACKEND__ */
#if __BF_BACKEND__
		if (ob->handle->child_pid) {/* TODO: why check for ob? */
#endif /* __BF_BACKEND__ */
			ob->OB_FETCHING = OB_STOP_REQUEST;
			outputbox_message(ob, _("stop request. stopping tool..."), "b");
#ifdef __KA_BACKEND__
			finish_execute(ob);
#endif /* __KA_BACKEND__ */
		}
	}
	DEBUG_MSG("outputbox_stop: fisnised.\n");
}

Toutputbox *outputbox_get_box (Tbfwin *bfwin, guint page)
{
	Toutputbox *ob;
	ob = OUTPUTBOX(bfwin->outputbox);
	if (ob && (ob->page_number == page)) {
		return ob;
	}else {
		ob = OUTPUTBOX(bfwin->grepbox);
		if (ob && (ob->page_number == page)) {
			return ob;
		}
	}
	return NULL;
}

void outputbox_set_status(Toutputbox *ob, gboolean status, gboolean force)
{
	if (!ob) { return ;}

	gint cur_page;
	cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ob->bfwin->ob_notebook));

	if (force) {
		menuitem_set_sensitive(ob->bfwin->menubar, N_("/External/Stop..."), status);
	}else{/* force FALSE ==> call by outputbox backend, for e.g., outputbox_bf.c::finish_excute() */
		if (ob->page_number == cur_page) {
			menuitem_set_sensitive(ob->bfwin->menubar, N_("/External/Stop..."), status);
		}
		GtkWidget *tab_label, *child_widget;
		child_widget = gtk_notebook_get_nth_page(GTK_NOTEBOOK(ob->bfwin->ob_notebook), ob->page_number);
		tab_label = gtk_notebook_get_tab_label(GTK_NOTEBOOK(ob->bfwin->ob_notebook), child_widget);

		GdkColor colorred = {0,0,0,65535};
		GdkColor colorblack = {0,0,0,0};

		if ( status ) {
			DEBUG_MSG("outputbox_set_status: red color\n");
			gtk_widget_modify_fg( tab_label, GTK_STATE_NORMAL, &colorred );
			gtk_widget_modify_fg( tab_label, GTK_STATE_PRELIGHT, &colorred );
			gtk_widget_modify_fg( tab_label, GTK_STATE_ACTIVE, &colorred );
		} else {
			DEBUG_MSG("outputbox_set_status: normal status\n");
			gtk_widget_modify_fg( tab_label, GTK_STATE_NORMAL, &colorblack );
			gtk_widget_modify_fg( tab_label, GTK_STATE_PRELIGHT, &colorblack );
			gtk_widget_modify_fg( tab_label, GTK_STATE_ACTIVE, &colorblack );
		}
	}
}
