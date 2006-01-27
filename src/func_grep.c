/* $Id$ */

/* Winefish LaTeX Editor (based on Bluefish)
 *
 * grep function: frontend and backend
 *
 * Copyright (c) 2006 kyanh <kyanh@o2.pl>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
 
/*
TODO: list all files contains a pattern. current: show details..
TODO: pass options to find and grep.
*/

#define DEBUG

#include "config.h"

#ifdef EXTERNAL_GREP
#ifdef EXTERNAL_FIND

#include <gtk/gtk.h>
#include <string.h>

#include "bluefish.h"
#include "outputbox.h"
#include "gtk_easy.h"
#include "gui.h" /* statusbar_message */
#include "bf_lib.h" /* create_secure_dir_return_filename, remove_secure_dir_and_filename */
#include "stringlist.h" /* get_stringlist */
#include "document.h"/* docs_new_from_files */
#include "func_grep.h"

#ifndef FUNC_GREP_RECURSIVE_MAX_DEPTH
#define FUNC_GREP_RECURSIVE_MAX_DEPTH "-maxdepth 50 "
#endif

/**************************************************************************/
/* the start of the callback functions for the menu, acting on a document */
/**************************************************************************/

typedef struct
{
	GList *filenames_to_return;
	GtkWidget *win;
	GtkWidget *basedir;
	GtkWidget *find_pattern;
	GtkWidget *recursive;
	GtkWidget *open_files;
	GtkWidget *grep_pattern;
	GtkWidget *is_regex;
	GtkWidget *case_sensitive;
	Tbfwin *bfwin;
}
Tfiles_advanced;

static int open_files;

static void files_advanced_win_destroy( GtkWidget * widget, Tfiles_advanced *tfs )
{
	DEBUG_MSG( "files_advanced_win_destroy, started\n" );
	gtk_main_quit();
	DEBUG_MSG( "files_advanced_win_destroy, gtk_main_quit called\n" );
	window_destroy( tfs->win );
	open_files = 0;
}

static void files_advanced_win_ok_clicked( GtkWidget * widget, Tfiles_advanced *tfs )
{
	/* create list here */
	gchar * command, *temp_file=NULL;
	gchar *c_basedir, *c_find_pattern, *c_recursive, *c_grep_pattern, *c_is_regex;
	gchar *c_grep_pattern_escaped=NULL;
	gint type = 0;
	
	c_basedir = gtk_editable_get_chars( GTK_EDITABLE( GTK_COMBO(tfs->basedir)->entry ), 0, -1 );
	
	if (!c_basedir || strlen(c_basedir)==0) {
		tfs->filenames_to_return =  NULL;
		g_free(c_basedir);
		files_advanced_win_destroy( widget, tfs );
		return;
	}
	
	open_files = 100 + gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( tfs->open_files ) );
	if (open_files-100) {
		temp_file = create_secure_dir_return_filename();
		if ( !temp_file ) {
			files_advanced_win_destroy( widget, tfs );
			DEBUG_MSG( "files_advanced_win_ok_clicked, can't get a secure temp filename ?????\n" );
			return ;
		}
		DEBUG_MSG( "files_advanced_win_ok_clicked, temp_file=%s\n", temp_file );
	}

	{
		gchar ** tmparray;
		c_find_pattern = gtk_editable_get_chars( GTK_EDITABLE( GTK_COMBO( tfs->find_pattern ) ->entry ), 0, -1 );
		if (!c_find_pattern || strlen(c_find_pattern) == 0) {
			c_find_pattern = g_strdup("*");
		}
		/* TODO: escape the string:
		replace any ' by \'
		DEBUG_MSG("func_grep: escape_string = %s\n", g_strescape(c_find_pattern, "\""));
		*/
		tmparray = g_strsplit(c_find_pattern,",",0);
		gchar **tmp2array = tmparray;
		gint newsize = 1; /* include terminator */
		c_find_pattern = g_realloc(c_find_pattern,newsize); /* for new string: g_malloc0 */
		c_find_pattern[0] = '\0';
		/* TODO: use g_strconcat is easier (for coding?) */
		while (*tmp2array) {
			newsize += strlen(*tmp2array) + 12; /* __'x' -name -o __ */
			c_find_pattern = g_realloc(c_find_pattern,newsize);
			strcat(c_find_pattern,"-name '");
			strcat(c_find_pattern,*tmp2array);
			strcat(c_find_pattern,"' -o ");
			tmp2array++;
		}
		DEBUG_MSG("func_grep: finalstring = %s\n", c_find_pattern);
		/* finalstring is of the form __X -o __ */
		newsize -= 4;
		c_find_pattern = g_realloc(c_find_pattern, newsize);
		c_find_pattern[newsize-1] = '\0';
		DEBUG_MSG("func_grep: truncated finalstring = %s\n", c_find_pattern);
		c_find_pattern = g_strconcat(" \\( ",c_find_pattern, " \\) ", NULL);
		g_strfreev(tmparray);
	}
	
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( tfs->recursive ) ) ) {
		c_recursive = FUNC_GREP_RECURSIVE_MAX_DEPTH;
	} else {
		c_recursive = "-maxdepth 1 ";
	}

	c_grep_pattern =/* gtk_editable_get_chars( GTK_EDITABLE( tfs->grep_pattern ), 0, -1 ); */
			gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(tfs->grep_pattern)->entry),0,-1);
	type = (!c_grep_pattern || strlen(c_grep_pattern)==0);
	if ( open_files-100) {
		c_is_regex = " -l ";
	}else{
		if (type) {
			c_is_regex = " -l ";
		}else{
			c_is_regex = " -nH ";
		}
	}
	
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( tfs->is_regex ) ) ) {
		c_is_regex = g_strconcat(" -E", c_is_regex, NULL);
	}
	if ( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( tfs->case_sensitive ) ) ) {
		c_is_regex = g_strconcat(" -i", c_is_regex, NULL);
	}
	
	/*
	command = `find c_basedir -name c_find_pattern c_recursive`
	command = `grep -E 'c_grep_pattern' `find c_basedir -name c_find_pattern c_recursive``
	*/
	if ( type ) {
		command = g_strconcat( EXTERNAL_FIND, " '", c_basedir, "' -type f",c_find_pattern, c_recursive, NULL );
	} else {
#ifdef EXTERNAL_XARGS
#ifdef EXTERNAL_SED
#define SED_XARGS
#endif /* EXTERNAL_SED */
#endif /* EXTERNAL_XARGS */
#ifdef SED_XARGS
		c_grep_pattern_escaped = g_strescape(c_grep_pattern,"\"");
		/* TODO: escape \" */

		DEBUG_MSG("func_grep: use xargs and sed\n");
		command = g_strconcat(EXTERNAL_FIND, " '",c_basedir, "' -type f", c_find_pattern, c_recursive, "| ", EXTERNAL_SED, " -e 's/ /\\\\\\ /g' | ", EXTERNAL_XARGS, " ", EXTERNAL_GREP, c_is_regex, " '", c_grep_pattern_escaped,"'", NULL);
#else
		command = g_strconcat( EXTERNAL_GREP, c_is_regex, " '", c_grep_pattern_escaped, "' `", EXTERNAL_FIND, " '", c_basedir, "' -type f", c_find_pattern, c_recursive, "`", NULL );
#endif /* SED_XARGS */
	}
	DEBUG_MSG( "files_advanced_win_ok_clicked, command=%s\n", command );
	statusbar_message( tfs->bfwin, _( "searching files..." ), 1000 );
	flush_queue();
	if (open_files-100) {
		DEBUG_MSG("files_advanced_win_ok_clicked, redirect to %s\n", temp_file);
		command = g_strconcat(command, " > ", temp_file,NULL);
		system( command ); /* may halt !!! system call !!! */
		tfs->filenames_to_return = get_stringlist( temp_file, tfs->filenames_to_return );
		remove_secure_dir_and_filename( temp_file ); /* BUG#64, g_free( temp_file ); */
	}else{
		tfs->filenames_to_return =  NULL;
		if (type ) {
			outputbox(tfs->bfwin,&tfs->bfwin->grepbox,_("grep"), ".*", 0, -1, 0, command, 0);
		}else{
			/* the output maynot be captured completely. why? a line may be cut...*/
			outputbox(tfs->bfwin,&tfs->bfwin->grepbox,_("grep"), "^([^:]+):([0-9]+):(.*)", 1, 2, 3, command, 0);
		}
	}
	/* history dealing */
	if (!type) {
		tfs->bfwin->session->searchlist = add_to_history_stringlist(tfs->bfwin->session->searchlist,c_grep_pattern,TRUE/*top*/,TRUE/*move if exists */);
	}
	{
		gchar *tmpstr = gtk_editable_get_chars(GTK_EDITABLE( GTK_COMBO( tfs->basedir ) ->entry),0,-1);
		tfs->bfwin->session->recent_dirs = add_to_history_stringlist(tfs->bfwin->session->recent_dirs,  tmpstr, TRUE, TRUE);
		g_free(tmpstr);
	}
	g_free( c_basedir );
	g_free( c_find_pattern );
	g_free( c_grep_pattern );
	g_free( c_grep_pattern_escaped );
	g_free( command );
	files_advanced_win_destroy( widget, tfs );
}
static void files_advanced_win_cancel_clicked( GtkWidget * widget, Tfiles_advanced *tfs )
{
	files_advanced_win_destroy( widget, tfs );
}

static void files_advanced_win_select_basedir_lcb( GtkWidget * widget, Tfiles_advanced *tfs )
{
	gchar * olddir;
	/* oldir = gtk_editable_get_chars( GTK_EDITABLE( tfs->basedir ), 0, -1 ); */
	
	olddir = gtk_editable_get_chars( GTK_EDITABLE( GTK_COMBO(tfs->basedir)->entry ), 0, -1 );
	
	/* concat a "/" to the end of the current directory. This fixes a bug where your
	current working directory was being parsed as /directory/file when you opened 
	the dialog to browse for a directory
	*/
	gchar *tmpdir = g_strconcat( olddir, "/", NULL );
	gchar *newdir = NULL;
#ifdef HAVE_ATLEAST_GTK_2_4

{
	GtkWidget *dialog;
		/*dialog = gtk_file_chooser_dialog_new (_("Select basedir"),NULL,
	GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
	GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
	NULL);
	gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog),TRUE);
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog),tmpdir);*/
	dialog = file_chooser_dialog( tfs->bfwin, _( "Select basedir" ), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, NULL, TRUE, FALSE, NULL );
	if ( gtk_dialog_run ( GTK_DIALOG ( dialog ) ) == GTK_RESPONSE_ACCEPT )
	{
		newdir = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( dialog ) );
	}
	gtk_widget_destroy( dialog );
}
#else
	newdir = return_dir( tmpdir, _( "Select basedir" ) );
#endif

	g_free( tmpdir );
	if ( newdir ) {
		/* gtk_entry_set_text( GTK_ENTRY( tfs->basedir ), newdir ); */
		gint position =0;
		gtk_editable_delete_text(GTK_EDITABLE( GTK_COMBO( tfs->basedir ) ->entry ),0,-1);
		gtk_editable_insert_text( GTK_EDITABLE( GTK_COMBO( tfs->basedir ) ->entry ), newdir, strlen(newdir), &position);
		g_free( newdir );
	}
	g_free( olddir );
}

static void files_advanced_win( Tfiles_advanced *tfs)
{
	GtkWidget * vbox, *hbox, *but, *table;
	GList *list;
	if ( !tfs->basedir ) {
		gchar * curdir = g_get_current_dir();
		/* tfs->basedir = entry_with_text( curdir, 255 ); */
		tfs->basedir = combo_with_popdown(curdir, tfs->bfwin->session->recent_dirs, TRUE/*editable*/);
		g_free ( curdir );
	}

	tfs->win = window_full2( _( "Grep Function" ), GTK_WIN_POS_MOUSE, 12, G_CALLBACK( files_advanced_win_destroy ), tfs, TRUE, tfs->bfwin->main_window );
	DEBUG_MSG( "files_advanced_win, tfs->win=%p\n", tfs->win );
	tfs->filenames_to_return = NULL;
	vbox = gtk_vbox_new( FALSE, 0 );
	gtk_container_add( GTK_CONTAINER( tfs->win ), vbox );

	table = gtk_table_new( 11, 7, FALSE );
	gtk_table_set_row_spacings( GTK_TABLE( table ), 5 );
	gtk_table_set_col_spacings( GTK_TABLE( table ), 12 );
	gtk_box_pack_start( GTK_BOX( vbox ), table, FALSE, FALSE, 0 );

	/* gtk_table_attach_defaults( GTK_TABLE( table ), gtk_label_new( _( "grep {contains} `find {basedir} -name '{file type}'`" ) ), 0, 7, 0, 1 ); */
	gtk_table_attach_defaults( GTK_TABLE( table ), gtk_label_new( _( "left pattern blank == file listing" ) ), 0, 7, 0, 1 );
	gtk_table_attach_defaults( GTK_TABLE( table ), gtk_hseparator_new(), 0, 7, 1, 2 );

	/* filename part */
	/* curdir should get a value */
	bf_label_tad_with_markup( _( "<b>General</b>" ), 0, 0.5, table, 0, 3, 2, 3 );

	bf_mnemonic_label_tad_with_alignment( _( "Base_dir:" ), tfs->basedir, 0, 0.5, table, 1, 2, 3, 4 );
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->basedir, 2, 6, 3, 4 );
	gtk_table_attach( GTK_TABLE( table ), bf_allbuttons_backend( _( "_Browse..." ), TRUE, 112, G_CALLBACK( files_advanced_win_select_basedir_lcb ), tfs ), 6, 7, 3, 4, GTK_SHRINK, GTK_SHRINK, 0, 0 );

	list = g_list_append( NULL, "*.tex" );
	list = g_list_append( list, "*.cls,*.dtx,*.sty,*.ins" );
	list = g_list_append( list, "*.ind,*.bbl" );
	list = g_list_append( list, "*.log,*.aux,*.idx,*.ilg,*.blg" );
	list = g_list_append( list, "*.toc,*.lof,*.lot,*.thm" );
	list = g_list_append( list, "*" );

	tfs->find_pattern = combo_with_popdown( "*.tex", list, 1 );
	bf_mnemonic_label_tad_with_alignment( _( "_File Type:" ), tfs->find_pattern, 0, 0.5, table, 1, 2, 4, 5 );
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->find_pattern, 2, 6, 4, 5 );

	g_list_free( list );

	tfs->recursive = checkbut_with_value( NULL, 1 );
	bf_mnemonic_label_tad_with_alignment( _( "_Recursive:" ), tfs->recursive, 0, 0.5, table, 1, 2, 5, 6 );
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->recursive, 2, 6, 5, 6 );

	tfs->open_files = checkbut_with_value( NULL , (open_files-100) );
	bf_mnemonic_label_tad_with_alignment( _( "_Open files:" ), tfs->open_files, 0, 0.5, table, 1, 2, 6, 7);
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->open_files, 2, 6, 6, 7 );

	/* content */
	gtk_table_set_row_spacing( GTK_TABLE( table ), 6, 10 );
	bf_label_tad_with_markup( _( "<b>Contains</b>" ), 0, 0.5, table, 0, 3, 7, 8 );

	{
		/* tfs->grep_pattern =  entry_with_text( NULL, 255 ); */
		gchar *buffer;
		GtkTextIter start, end;
		gtk_text_buffer_get_selection_bounds(tfs->bfwin->current_document->buffer, &start, &end);
		buffer = gtk_text_buffer_get_text(tfs->bfwin->current_document->buffer, &start, &end, FALSE);
		if (strchr(buffer,'\n')!=NULL) {
			/* a newline in the selection, we probably don't want this string as search string */
			g_free(buffer);
			buffer = NULL;
		}
		/* put the selected text in the pattern textfield */
		tfs->grep_pattern = combo_with_popdown(buffer?buffer:"", tfs->bfwin->session->searchlist, TRUE/*editable*/);
		if (buffer) g_free(buffer);
	}
	
	bf_mnemonic_label_tad_with_alignment( _( "Pa_ttern:" ), tfs->grep_pattern, 0, 0.5, table, 1, 2, 8, 9 );
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->grep_pattern, 2, 6, 8, 9 );

	tfs->case_sensitive = checkbut_with_value( NULL, 1 );
	bf_mnemonic_label_tad_with_alignment( _( "_Case sensitive:" ), tfs->case_sensitive, 0, 0.5, table, 1, 2, 9, 10 );
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->case_sensitive, 2, 6, 9, 10 );
	
	tfs->is_regex = checkbut_with_value( NULL, 0 );
	bf_mnemonic_label_tad_with_alignment( _( "Is rege_x:" ), tfs->is_regex, 0, 0.5, table, 1, 2, 10, 11 );
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->is_regex, 2, 6, 10, 11 );

	/* buttons */
	hbox = gtk_hbox_new( FALSE, 0 );
	gtk_box_pack_start( GTK_BOX( hbox ), gtk_hseparator_new(), TRUE, TRUE, 0 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 12 );
	hbox = gtk_hbutton_box_new();
	gtk_hbutton_box_set_layout_default( GTK_BUTTONBOX_END );
	gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbox ), 12 );
	but = bf_stock_cancel_button( G_CALLBACK( files_advanced_win_cancel_clicked ), tfs );
	gtk_box_pack_start( GTK_BOX( hbox ), but , FALSE, FALSE, 0 );
	but = bf_stock_ok_button( G_CALLBACK( files_advanced_win_ok_clicked ), tfs );
	gtk_box_pack_start( GTK_BOX( hbox ), but , FALSE, FALSE, 0 );
	gtk_window_set_default( GTK_WINDOW( tfs->win ), but );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show_all( GTK_WIDGET( tfs->win ) );
	/*	gtk_grab_add(GTK_WIDGET(tfs->win));
	gtk_widget_realize(GTK_WIDGET(tfs->win));*/
	gtk_window_set_transient_for( GTK_WINDOW( tfs->win ), GTK_WINDOW( tfs->bfwin->main_window ) );
}

static GList *return_files_advanced( Tbfwin *bfwin, gchar *tmppath)
{
	Tfiles_advanced tfs = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, bfwin};
	if ( tmppath ) {
		GtkWidget * curdir = entry_with_text( tmppath, 255 );
		tfs.basedir = curdir;
	}
	/* this is probably called different!! */
	files_advanced_win( &tfs );
	DEBUG_MSG( "return_files_advanced, calling gtk_main()\n" );
	gtk_main();
	return tfs.filenames_to_return;
}

/***************************************************************/

void file_open_advanced_cb( Tbfwin *bfwin, gboolean v_open_files )
{
	if (open_files > 99 )
	{
		/* prevent user from calling twice */
		return;
	}

	GList * tmplist;
	open_files = 100 + v_open_files;

	tmplist = return_files_advanced( bfwin , NULL);
	if (open_files - 100) {
		if ( !tmplist ) {
			/* info_dialog( bfwin->main_window, _( "No matching files found" ), NULL ); */
			statusbar_message(bfwin, _( "no matching files found" ), 1000);
			return ;
		}
		{
			gint len = g_list_length( tmplist );
			gchar *message = g_strdup_printf( _( "loading %d file(s)..." ), len );
			statusbar_message( bfwin, message, 2000 + len * 50 );
			g_free( message );
			flush_queue();
		}
		docs_new_from_files( bfwin, tmplist, FALSE, -1 );
		free_stringlist( tmplist );
	}
	open_files = 0;
}

void open_advanced_from_filebrowser( Tbfwin *bfwin, gchar *path )
{
	GList * tmplist;
	tmplist = return_files_advanced ( bfwin, path );
	if ( !tmplist ) {
		return ;
	}
	{
		gint len = g_list_length( tmplist );
		gchar *message = g_strdup_printf( _( "loading %d file(s)..." ), len );
		statusbar_message( bfwin, message, 2000 + len * 50 );
		g_free( message );
		flush_queue();
	}
	docs_new_from_files( bfwin, tmplist, FALSE, -1);
	free_stringlist( tmplist );
}

#endif /* EXTERNAL_FIND */
#endif /* EXTERNAL_GREP */

