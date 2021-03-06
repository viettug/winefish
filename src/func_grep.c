/* $Id: func_grep.c 544 2006-04-29 14:12:50Z kyanh $ */

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

/* #define DEBUG */

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

#ifdef EXTERNAL_XARGS
#ifdef EXTERNAL_SED
#define HAVE_SED_XARGS
#endif /* EXTERNAL_SED */
#endif /* EXTERNAL_XARGS */

/**************************************************************************/
/* the start of the callback functions for the menu, acting on a document */
/**************************************************************************/

typedef struct
{
	GList *filenames_to_return;
	GtkWidget *win;
	GtkWidget *basedir;
	GtkWidget *skipdir;
	GtkWidget *find_pattern;
	GtkWidget *recursive;
	GtkWidget *open_files;
	GtkWidget *grep_pattern;
	GtkWidget *is_regex;
	GtkWidget *case_sensitive;
	Tbfwin *bfwin;
	gint retval;
}
Tfiles_advanced;

static int open_files;

enum {
	FIND_WITHOUT_PATTERN = 1<<0,
	FIND_IN_CURRENT_FILE = 1<<1,
	FIND_IN_ALL_OPENED_FILES = 1<<2,
	FIND_IN_DIRECTORY = 1<<3
};

/*
	FIND_IN_CURRENT_FILE
	FIND_IN_ALL_OPENED_FILES
	=> FIND_WITHOUT_PATTERN == 0
*/

enum {
	FIND_RET_FAILED=1<<0,
	FIND_RET_OK=1<<1,
	FIND_RET_FIND_IN_OPENED_FILE_HAS_NOT_FILENAME_YET=1<<2,
	FIND_RET_FIND_IN_OPENED_FILE_BUT_NOT_PATTERN_SPECIFIED=1<<3,
	FIND_RET_CANNOT_CREATE_SECURE_TEMP_FILE=1<<4,
	FIND_RET_EMPTY_FILE_LIST=1<<5,
	FIND_RET_NO_DIRECTORY_SPECIFIED=1<<6
};

static Tconvert_table func_grep_escape_table [] = {
	{'n', "\n"},
	{'t', "\t"},
	{'\\', "\\"},
	{'r', "\r"},
	{'a', "\a"},
	{'b', "\b"},
	{'v', "\v"},
	{0, NULL}
};

static gchar *func_grep_escape_string(const gchar *original) {
	gchar *string;
	DEBUG_MSG("func_grep: escape_string, started with %s\n", original);
	string = unexpand_string(original,'\\',func_grep_escape_table);
	DEBUG_MSG("func_grep: escape_string, finished with %s\n", string);
	return string;
}

static void files_advanced_win_destroy( GtkWidget * widget, Tfiles_advanced *tfs )
{
	DEBUG_MSG( "func_grep: destroy the dialog: start\n" );
	gtk_main_quit();
	DEBUG_MSG( "func_grep: call gtk_main_quit called\n" );
	window_destroy( tfs->win );
	open_files = 0;
}

static void files_advanced_win_ok_clicked( GtkWidget * widget, Tfiles_advanced *tfs )
{
	gchar * command =NULL,
		*temp_file=NULL,
		*c_basedir =NULL,
		*c_find_pattern =NULL,
		*c_recursive =NULL,
		*c_grep_pattern =NULL,
		*c_is_regex =NULL,
		*c_skipdir =NULL,
		*c_grep_pattern_escaped=NULL
		;

	gint type = FIND_IN_DIRECTORY;
	tfs->retval = FIND_RET_OK;

	/* get type, get c_find_pattern ***********************************************/

	c_find_pattern = gtk_editable_get_chars( GTK_EDITABLE( GTK_COMBO( tfs->find_pattern ) ->entry ), 0, -1 );

	if (!c_find_pattern || strlen(c_find_pattern) == 0) {
		c_find_pattern = g_strdup("*");
	}else if (strcmp(c_find_pattern, _("current file")) ==0 ) {
		type =  FIND_IN_CURRENT_FILE;
	}else if ( strcmp(c_find_pattern, _("all opened files")) ==0 ) {
		type = FIND_IN_ALL_OPENED_FILES;
	}

	if (type & FIND_IN_DIRECTORY ) {
		open_files = 100 + gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( tfs->open_files ) );
		if (open_files-100) {
			temp_file = create_secure_dir_return_filename();
			if ( !temp_file ) {
				tfs->retval = tfs->retval | FIND_RET_CANNOT_CREATE_SECURE_TEMP_FILE | FIND_RET_FAILED;
				DEBUG_MSG( "func_grep: can't get a secure temp filename ?????\n" );
			}
			DEBUG_MSG( "func_grep: temp_file=%s\n", temp_file );
		}

		if (! (tfs->retval & FIND_RET_FAILED) ) {
			c_basedir = gtk_editable_get_chars( GTK_EDITABLE( GTK_COMBO(tfs->basedir)->entry ), 0, -1 );
			if (!c_basedir || strlen(c_basedir)==0) {
				tfs->retval = tfs->retval | FIND_RET_NO_DIRECTORY_SPECIFIED | FIND_RET_FAILED ;
				DEBUG_MSG("func_grep: basedir is not specified\n");
			}
		}

		/* create c_find_pattern ***********************************************/
		if ( !( tfs->retval & FIND_RET_FAILED ) ){
			gchar ** tmparray;
			/* TODO: escape the string: replace any ' by \' */
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
			c_find_pattern = g_strconcat("\\( ",c_find_pattern, " \\)", NULL);
			g_strfreev(tmparray);
		}
	}else{
		/* find in current file or all opened files */
		if (type & FIND_IN_CURRENT_FILE) {
			if (! tfs->bfwin->current_document->filename ) {
				tfs->retval = tfs->retval | FIND_RET_FIND_IN_OPENED_FILE_HAS_NOT_FILENAME_YET | FIND_RET_FAILED;
				DEBUG_MSG("func_grep: find in current file but it hasnot filename yet\n");
			} else {
				c_find_pattern = g_strdup_printf("'%s'", tfs->bfwin->current_document->filename);
			}
		}else{
			GList *tmplist;
			tmplist = g_list_first(tfs->bfwin->documentlist);
			c_find_pattern = g_strdup("");
			while (tmplist) {
				if (DOCUMENT(tmplist->data)->filename) {
					c_find_pattern = g_strdup_printf("%s '%s'", c_find_pattern, DOCUMENT(tmplist->data)->filename);
				}
				tmplist = g_list_next(tmplist);
			}
			if (!strlen(c_find_pattern)) {
				tfs->retval = tfs->retval | FIND_RET_FIND_IN_OPENED_FILE_HAS_NOT_FILENAME_YET | FIND_RET_FAILED;
				DEBUG_MSG("func_grep: find in current files but they havenot filename yet\n");
			}
		}
	}

	if ( !(tfs->retval & FIND_RET_FAILED) ) {
		c_grep_pattern = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(tfs->grep_pattern)->entry),0,-1);
		type = SET_BIT(type, FIND_WITHOUT_PATTERN, (!c_grep_pattern || strlen(c_grep_pattern)==0) ); /* without patterns */
	
		if ( type & FIND_IN_DIRECTORY ) {
			c_is_regex = g_strdup_printf( "-%s%s%s", (open_files-100)?"l":( type & FIND_WITHOUT_PATTERN ?"l":"nH"), gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( tfs->is_regex ) ) ? "E": "",  gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( tfs->case_sensitive ) ) ? "" : "i" );
	
			if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( tfs->recursive ) ) ) {
				c_recursive = FUNC_GREP_RECURSIVE_MAX_DEPTH;
			} else {
				c_recursive = "-maxdepth 1";
			}
	
			if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( tfs->skipdir ) ) ) {
				c_skipdir = g_strdup("| grep -v SCCS/ | grep -v CVS/ | grep -v .svn/");
			}else{
				c_skipdir = g_strdup("");
			}
	
			/*
			command = `find c_basedir -name c_find_pattern c_recursive`
			command = `grep -E 'c_grep_pattern' `find c_basedir -name c_find_pattern c_recursive``
			*/
			if ( type & FIND_WITHOUT_PATTERN ) {
				command = g_strdup_printf("%s '%s' -type f %s %s %s", EXTERNAL_FIND, c_basedir, c_find_pattern, c_recursive, c_skipdir);
			} else {
				/* c_grep_pattern_escaped = g_strescape(c_grep_pattern,"\""); *//* TODO: escape \" */
				c_grep_pattern_escaped = func_grep_escape_string(c_grep_pattern);
				DEBUG_MSG("func_grep: original: %s; escaped : %s\n", c_grep_pattern, c_grep_pattern_escaped);
#ifdef HAVE_SED_XARGS
				DEBUG_MSG("func_grep: use xargs and sed\n");
				command = g_strdup_printf("%s '%s' -type f %s %s %s | %s -e 's/ /\\\\\\ /g' | %s %s %s '%s'", EXTERNAL_FIND, c_basedir, c_find_pattern, c_recursive, c_skipdir, EXTERNAL_SED, EXTERNAL_XARGS, EXTERNAL_GREP, c_is_regex, c_grep_pattern_escaped);
#else
				/* TODO: have find, have grep, but donot have sed/xargs. why???? */
				command = g_strdup_printf("%s %s '%s' `%s '%s' -type f %s %s %s`",EXTERNAL_GREP, c_is_regex, c_grep_pattern_escaped, EXTERNAL_FIND, c_basedir, c_find_pattern, c_recursive, c_skipdir);
#endif /* SED_XARGS */
			}
			DEBUG_MSG( "func_grep: command=%s\n", command );
			statusbar_message( tfs->bfwin, _( "searching files..." ), 1000 );
			flush_queue();
			if (open_files-100) {
				DEBUG_MSG("func_grep: redirect to %s\n", temp_file);
				command = g_strconcat(command, " > ", temp_file,NULL);
				system( command ); /* may halt !!! system call !!! */
				tfs->filenames_to_return = get_stringlist( temp_file, tfs->filenames_to_return );
				remove_secure_dir_and_filename( temp_file ); /* BUG#64, g_free( temp_file ); */
				if (!tfs->filenames_to_return) {
					tfs->retval = tfs->retval | FIND_RET_EMPTY_FILE_LIST;
				}
			}else{
				if (type & FIND_WITHOUT_PATTERN) {
					outputbox(tfs->bfwin,&tfs->bfwin->grepbox,_("grep"),  ".*", 0, -1, 0, command, 0);
				}else{
					/* the output maynot be captured completely. why? a line may be cut...*/
					outputbox(tfs->bfwin,&tfs->bfwin->grepbox,_("grep"),  "^([^:]+):([0-9]+):(.*)", 1, 2, 3, command, 0);
				}
			}
			/* add directory to history */
			{
				gchar *tmpstr = gtk_editable_get_chars(GTK_EDITABLE( GTK_COMBO( tfs->basedir ) ->entry),0,-1);
				if (tfs->bfwin->project) {
					DEBUG_MSG("func_grep: add directory %s to history of project\n",tmpstr);
					tfs->bfwin->project->session->recent_dirs = add_to_history_stringlist(tfs->bfwin->project->session->recent_dirs,  tmpstr, TRUE, TRUE);
				}else{
					DEBUG_MSG("func_grep: add directory %s to history of global session\n",tmpstr);
					tfs->bfwin->session->recent_dirs = add_to_history_stringlist(tfs->bfwin->session->recent_dirs,  tmpstr, TRUE, TRUE);
				}
				g_free(tmpstr);
				
			}
		}else{
			DEBUG_MSG("func_grep: start finding in opened file(s)...\n");
			if ( type & FIND_WITHOUT_PATTERN ) {
				tfs->retval = tfs->retval |FIND_RET_FIND_IN_OPENED_FILE_BUT_NOT_PATTERN_SPECIFIED | FIND_RET_FAILED;
				DEBUG_MSG("func_grep: find in open files but no pattern was specified\n");
			}else{
				c_is_regex = g_strdup_printf( "-nH%s%s", gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( tfs->is_regex ) ) ? "E": "",  gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( tfs->case_sensitive ) ) ? "" : "i" );
				/* c_grep_pattern_escaped = g_strescape(c_grep_pattern,"\""); *//* TODO: escape \" */
				c_grep_pattern_escaped = func_grep_escape_string(c_grep_pattern);
				DEBUG_MSG("func_grep: original: %s; escaped : %s\n", c_grep_pattern, c_grep_pattern_escaped);
				command = g_strdup_printf("%s %s '%s' %s", EXTERNAL_GREP, c_is_regex, c_grep_pattern_escaped,c_find_pattern);
				outputbox(tfs->bfwin,&tfs->bfwin->grepbox,_("grep"),  "^([^:]+):([0-9]+):(.*)", 1, 2, 3, command, 0);
			}
		}
		/* add pattern to history. only if found success ? */
		if (! ( type & FIND_WITHOUT_PATTERN )  && ! (tfs->retval & FIND_RET_FAILED) ) {
			if (tfs->bfwin->project) {
				DEBUG_MSG("func_grep: add grep_pattern %s to history of project\n",c_grep_pattern);
				tfs->bfwin->project->session->searchlist = add_to_history_stringlist(tfs->bfwin->project->session->searchlist,c_grep_pattern,TRUE,TRUE);
			}else{
				DEBUG_MSG("func_grep: add grep_pattern %s to history of global session\n",c_grep_pattern);
				tfs->bfwin->session->searchlist = add_to_history_stringlist(tfs->bfwin->session->searchlist,c_grep_pattern,TRUE/*top*/,TRUE/*move if exists */);
			}
		}
	}

	g_free( c_basedir );
	g_free( c_find_pattern );
	g_free( c_grep_pattern );
	g_free( c_grep_pattern_escaped );
	g_free( c_is_regex );
	g_free( c_skipdir);
	g_free( command );

	gboolean destroy_win = TRUE;
	if (tfs->retval &FIND_RET_FIND_IN_OPENED_FILE_HAS_NOT_FILENAME_YET) {
		statusbar_message(tfs->bfwin, _("func_grep: file(s) without filename"), 2000);
	}else if (tfs->retval &FIND_RET_FIND_IN_OPENED_FILE_BUT_NOT_PATTERN_SPECIFIED) {
		warning_dialog(tfs->win, _("please specify the pattern!"), NULL);
		/* gtk_widget_grab_focus(tfs->find_pattern); */
		destroy_win = FALSE;
	} else if (tfs->retval &FIND_RET_CANNOT_CREATE_SECURE_TEMP_FILE) {
		statusbar_message(tfs->bfwin, _("func_grep: cannot create secure file"), 2000);
	} else if (tfs->retval &FIND_RET_EMPTY_FILE_LIST) {
		statusbar_message(tfs->bfwin, _( "no matching files found" ), 2000);
	} else if (tfs->retval & FIND_RET_NO_DIRECTORY_SPECIFIED) {
		statusbar_message(tfs->bfwin, _( "no directory specified" ), 2000);
	}
	if (destroy_win) {
		files_advanced_win_destroy(widget, tfs);
	}
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
	GtkWidget *dialog;
	dialog = file_chooser_dialog( tfs->bfwin, _( "Select basedir" ), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, NULL, TRUE, FALSE, NULL );
	if ( gtk_dialog_run ( GTK_DIALOG ( dialog ) ) == GTK_RESPONSE_ACCEPT )
	{
		newdir = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( dialog ) );
	}
	gtk_widget_destroy( dialog );
	g_free( tmpdir );
	if ( newdir ) {
		/* gtk_entry_set_text( GTK_ENTRY( tfs->basedir ), newdir ); */
		gint position =0;
		gtk_editable_delete_text(GTK_EDITABLE( GTK_COMBO( tfs->basedir ) ->entry ),0,-1);
		gtk_editable_insert_text( GTK_EDITABLE( GTK_COMBO( tfs->basedir ) ->entry ), newdir, strlen(newdir), &position);
		gtk_editable_set_position (GTK_EDITABLE( GTK_COMBO( tfs->basedir ) ->entry), -1);
		g_free( newdir );
	}
	g_free( olddir );
}

static void files_advanced_win_set_sensitive (Tfiles_advanced *tfs) {
	gint type;
	gchar * c_find_pattern;
	c_find_pattern = gtk_editable_get_chars( GTK_EDITABLE( GTK_COMBO( tfs->find_pattern ) ->entry ), 0, -1 );
	if (strcmp(c_find_pattern, _("current file")) ==0 ) {
		type =  FIND_IN_CURRENT_FILE;
	}else if ( strcmp(c_find_pattern, _("all opened files"))==0 ) {
		type = FIND_IN_ALL_OPENED_FILES;
	}else{
		type = FIND_IN_DIRECTORY;
	}
	if ( !(type & FIND_IN_DIRECTORY) ) {
		gtk_widget_set_sensitive(tfs->basedir, FALSE);
		gtk_widget_set_sensitive(tfs->open_files, FALSE);
		gtk_widget_set_sensitive(tfs->recursive, FALSE);
		gtk_widget_set_sensitive(tfs->skipdir, FALSE);
	}else{
		gtk_widget_set_sensitive(tfs->basedir, TRUE);
		gtk_widget_set_sensitive(tfs->open_files, TRUE);
		gtk_widget_set_sensitive(tfs->recursive, TRUE);
		gtk_widget_set_sensitive(tfs->skipdir, TRUE);
	}
	g_free(c_find_pattern);
}

static void find_pattern_changed_lcb(GtkWidget *widget, Tfiles_advanced *tfs) {
	files_advanced_win_set_sensitive(tfs);
}

static void files_advanced_win( Tfiles_advanced *tfs)
{
	
	GtkWidget * vbox, *hbox, *but, *table;
	GList *list;
	if ( !tfs->basedir ) {
		gchar * curdir = g_get_current_dir();
		/* tfs->basedir = entry_with_text( curdir, 255 ); */
		tfs->basedir = combo_with_popdown(curdir,tfs->bfwin->project ?  tfs->bfwin->project->session->recent_dirs : tfs->bfwin->session->recent_dirs, TRUE/*editable*/);
		g_free ( curdir );
	}
	
	gtk_widget_set_size_request(tfs->basedir, 270,-1);

	tfs->win = window_full2( _( "Grep Function" ), GTK_WIN_POS_MOUSE, 12, G_CALLBACK( files_advanced_win_destroy ), tfs, TRUE, tfs->bfwin->main_window );
	DEBUG_MSG( "func_grep: tfs->win=%p\n", tfs->win );
	tfs->filenames_to_return = NULL;
	vbox = gtk_vbox_new( FALSE, 0 );
	gtk_container_add( GTK_CONTAINER( tfs->win ), vbox );

#define MAX_COLUMN 7

	table = gtk_table_new( 13, MAX_COLUMN, FALSE );
	gtk_table_set_row_spacings( GTK_TABLE( table ), 0 );
	gtk_table_set_col_spacings( GTK_TABLE( table ), 12 );
	gtk_box_pack_start( GTK_BOX( vbox ), table, FALSE, FALSE, 0 );

	/* gtk_table_attach_defaults( GTK_TABLE( table ), gtk_label_new( _( "grep {contains} `find {basedir} -name '{file type}'`" ) ), 0, 7, 0, 1 ); */
	/* gtk_table_attach_defaults( GTK_TABLE( table ), gtk_label_new( _( "left pattern blank == file listing" ) ), 0, MAX_COLUMN, 0, 1 );
	gtk_table_attach_defaults( GTK_TABLE( table ), gtk_hseparator_new(), 0, MAX_COLUMN, 1, 2 );
	*/

	/* filename part */
	/* curdir should get a value */
	bf_label_tad_with_markup( _( "<b>General</b>" ), 0, 0.5, table, 0, 3, 2, 3 );

	bf_mnemonic_label_tad_with_alignment( _( "Base_dir:" ), tfs->basedir, 0, 0.5, table, 1, 2, 3, 4 );
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->basedir, 2, MAX_COLUMN-1, 3, 4 );
	gtk_table_attach( GTK_TABLE( table ), bf_allbuttons_backend( _( "_Browse..." ), TRUE, 112, G_CALLBACK( files_advanced_win_select_basedir_lcb ), tfs ), MAX_COLUMN-1, MAX_COLUMN, 3, 4, GTK_SHRINK, GTK_SHRINK, 0, 0 );

	list = NULL;
	list = g_list_append( list, _("current file") );
	list = g_list_append( list, _("all opened files") );
	list = g_list_append( list, "*.tex" );
	list = g_list_append( list, "*.cls,*.dtx,*.sty,*.ins" );
	list = g_list_append( list, "*.ind,*.bbl" );
	list = g_list_append( list, "*.log,*.aux,*.idx,*.ilg,*.blg" );
	list = g_list_append( list, "*.toc,*.lof,*.lot,*.thm" );
	list = g_list_append( list, "*" );
	if (open_files - 100) {
		tfs->find_pattern = combo_with_popdown("*.tex", list, 1 );
	}else{
		tfs->find_pattern = combo_with_popdown( _("all opened files"), list, 1 );
	}
	bf_mnemonic_label_tad_with_alignment( _( "_File:" ), tfs->find_pattern, 0, 0.5, table, 1, 2, 4, 5 );
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->find_pattern, 2, MAX_COLUMN-1, 4, 5 );
	g_signal_connect(G_OBJECT( GTK_ENTRY( GTK_COMBO( tfs->find_pattern )->entry) ), "changed", G_CALLBACK(find_pattern_changed_lcb), tfs);

	g_list_free( list );

	tfs->recursive = checkbut_with_value( NULL, 1 );
	bf_mnemonic_label_tad_with_alignment( _( "_Recursive:" ), tfs->recursive, 0, 0.5, table, 1, 2, 5, 6 );
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->recursive, 2, MAX_COLUMN-1, 5, 6 );

	tfs->open_files = checkbut_with_value( NULL , (open_files-100) );
	bf_mnemonic_label_tad_with_alignment( _( "_Open files:" ), tfs->open_files, 0, 0.5, table, 1, 2, 6, 7);
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->open_files, 2, MAX_COLUMN-1, 6, 7 );

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
		tfs->grep_pattern = combo_with_popdown(buffer?buffer:"", tfs->bfwin->project? tfs->bfwin->project->session->searchlist : tfs->bfwin->session->searchlist, TRUE/*editable*/);
		if (buffer) g_free(buffer);
	}
	
	bf_mnemonic_label_tad_with_alignment( _( "_Pattern:" ), tfs->grep_pattern, 0, 0.5, table, 1, 2, 8, 9 );
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->grep_pattern, 2, MAX_COLUMN-1, 8, 9 );
	/* failed: gtk_tooltips_set_tip(main_v->tooltips, tfs->grep_pattern, _("left pattern blank == file listing..."),NULL); */

	tfs->case_sensitive = checkbut_with_value( NULL, 1 );
	bf_mnemonic_label_tad_with_alignment( _( "_Case sensitive:" ), tfs->case_sensitive, 0, 0.5, table, 1, 2, 9, 10 );
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->case_sensitive, 2, MAX_COLUMN-1, 9, 10 );

	tfs->is_regex = checkbut_with_value( NULL, 0 );
	bf_mnemonic_label_tad_with_alignment( _( "Is rege_x:" ), tfs->is_regex, 0, 0.5, table, 1, 2, 10, 11 );
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->is_regex, 2, MAX_COLUMN-1, 10, 11 );

	gtk_table_set_row_spacing( GTK_TABLE( table ), 10, 10 );
	bf_label_tad_with_markup( _( "<b>Advanced</b>" ), 0, 0.5, table, 0, 3, 11, 12 );

	tfs->skipdir = checkbut_with_value( NULL, 1);
	bf_mnemonic_label_tad_with_alignment( _( "Skip _VCS dirs:" ), tfs->skipdir, 0, 0.5, table, 1, 2, 12, 13 );
	gtk_table_attach_defaults( GTK_TABLE( table ), tfs->skipdir, 2, MAX_COLUMN-1, 12, 13 );

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
	
	files_advanced_win_set_sensitive ( tfs );
	gtk_widget_show_all( GTK_WIDGET( tfs->win ) );

	/*	gtk_grab_add(GTK_WIDGET(tfs->win));
	gtk_widget_realize(GTK_WIDGET(tfs->win));*/
	
	gtk_window_set_transient_for( GTK_WINDOW( tfs->win ), GTK_WINDOW( tfs->bfwin->main_window ) );
}

static GList *return_files_advanced( Tbfwin *bfwin, gchar *tmppath)
{
	Tfiles_advanced tfs = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, bfwin, FIND_RET_OK};

	/* Tfiles_advanced *tfs;
	tfs = g_new0(Tfiles_advanced, 1);

	tfs->bfwin = bfwin;
	tfs->retval = FIND_RET_OK;
	*/

	if ( tmppath ) {
		GtkWidget * curdir = entry_with_text( tmppath, 255 );
		tfs.basedir = curdir;
	}
	/* this is probably called different!! */
	files_advanced_win( &tfs );
	DEBUG_MSG( "func_grep: calling gtk_main()\n" );
	gtk_main();
	return tfs.filenames_to_return;
}

/***************************************************************/

/* void file_open_advanced_cb( Tbfwin *bfwin, gboolean v_open_files ) */
gint func_grep( GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt ) {
	if (open_files > 99 ) return 0;

	GList * tmplist;
	open_files = 100 + ( opt >> FUNC_VALUE_ );

	tmplist = return_files_advanced( bfwin , NULL);

	if (open_files - 100) {
		if ( !tmplist ) return 0;
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
	return 1;
}

void open_advanced_from_filebrowser( Tbfwin *bfwin, gchar *path )
{
	GList * tmplist;
	tmplist = return_files_advanced ( bfwin, path);
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

/* void template_rescan_cb(Tbfwin *bfwin) { */
gint func_template_list( GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt ) {
	gchar *command;
	gchar *c_basedir;
	if (main_v->props.templates_dir && strlen(main_v->props.templates_dir) ){
		c_basedir = g_strdup(main_v->props.templates_dir);
	}else{
		c_basedir = g_strconcat(g_get_home_dir(),"/tex/templates/",NULL);
	}
	/* c_basedir = g_strdup("/home/users/kyanh/tex/templates/"); */
#ifdef HAVE_SED_XARGS
	command = g_strdup_printf("%s '%s' -type f \\( -name '*.tex' -o -name '*.sty' \\) %s | grep -v SCCS/ | grep -v CVS/ | grep -v .svn/ | %s -e 's/ /\\\\\\ /g' | %s %s -EnH  '%%%%%%%%wf='", EXTERNAL_FIND, c_basedir, FUNC_GREP_RECURSIVE_MAX_DEPTH, EXTERNAL_SED, EXTERNAL_XARGS, EXTERNAL_GREP);
#else
	command = g_strdup_printf("%s -EnH '%%%%%%%%wf=' `%s '%s' -type f \\( -name '*.tex' -o -name '*.sty' \\) %s | grep -v SCCS/ | grep -v CVS/ | grep -v .svn/`",EXTERNAL_GREP, EXTERNAL_FIND, c_basedir, FUNC_GREP_RECURSIVE_MAX_DEPTH);
#endif /* SED_XARGS */	
	outputbox(bfwin, &bfwin->templatebox,_("template"),  "^([^:]+):([0-9]+):(.*)", 1, 2, 3, command, 0);
	g_free(command);
	g_free(c_basedir);
	return 1;
}
#endif /* EXTERNAL_FIND */
#endif /* EXTERNAL_GREP */

