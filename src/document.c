/* $Id$ */

/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
* document.c - the document
*
* Copyright (C) 1998-2004 Olivier Sessink
* Copyright (C) 1998 Chris Mazuc
* Some additions Copyright (C) 2004 Eugene Morenko(More)
*
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

/* this is needed for Solaris to comply with the latest POSIX standard
* regarding the ctime_r() function
* the problem is that it generates a compiler warning on Linux, lstat() undefined.. */
/*  Michèle Garoche, 20060208 */
#ifdef PLATFORM_SOLARIS
#define _POSIX_C_SOURCE 200312L
#endif /* PLATFORM_SOLARIS */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h> /* for the keyboard event codes */
#include <sys/types.h> /* stat() */
#include <sys/stat.h> /* stat() */
#include <unistd.h> /* stat() */
#include <stdio.h> /* fopen() */
#include <string.h> /* strchr() */
#include <regex.h> /* regcomp() */
#include <stdlib.h> /* system() */
#include <time.h> /* ctime_r() */
#include <pcre.h>

/* #define DEBUG */

#ifdef DEBUGPROFILING
#include <sys/times.h>
#endif

#include "bluefish.h"

#include "document.h"
#include "highlight.h" /* all highlight functions */
#include "gui.h" /* statusbar_message() */
#include "bf_lib.h"
#include "menu.h" /* add_to_recent_list */
#include "stringlist.h" /* free_stringlist() */
#include "gtk_easy.h" /* *_dialog() */
#include "undo_redo.h" /* doc_unre_init() */
#include "char_table.h" /* convert_utf8...() */
#include "pixmap.h"
#include "snr2.h" /* snr2_run_extern_replace */
#include "filebrowser.h"
#include "bookmark.h"
#include "autox.h" /* autotext_done() */
#include "snooper.h"
#include "brace_finder.h" /* VALID_BRACE */

#include "func_grep.h"

#ifndef DELIMITERS
#define DELIMITERS " `1234567890-=~!@#$%^&*()_+[]{};':\",./<>?\\|"
/* TODO: @ for latex package writer */
#endif /* DELIMITERS */

#ifndef COMMAND_MAX_LENGTH
#define COMMAND_MAX_LENGTH 15
#endif /* COMMAND_MAX_LENGTH */

#ifndef AUTOTEXT_MAX_LENGTH
#define AUTOTEXT_MAX_LENGTH 15
#endif /* AUTOTEXT_MAX_LENGTH */

typedef struct
{
	GtkWidget *textview;
	GtkWidget *window;
}
Tfloatingview;
#define FLOATINGVIEW(var) ((Tfloatingview *)(var))

typedef struct
{
	gint so;
	gint eo;
}
Tpasteoperation;
#define PASTEOPERATION(var) ((Tpasteoperation *)(var))

void autoclosing_init( void )
{
	const char * error;
	int erroffset;
	main_v->autoclosingtag_regc = pcre_compile( "\\\\begin{([a-zA-Z]+\\*{0,1})?}$", PCRE_DOLLAR_ENDONLY, &error, &erroffset, NULL );
#ifdef DEBUG
	if ( !main_v->autoclosingtag_regc ) {
		DEBUG_MSG( "autoclosing_init, ERROR, %s\n", error );
	}
#endif
#ifdef HAVE_CONTEXT
	main_v->autoclosingtag_be_regc = pcre_compile("\\\\(begin|start)([a-zA-Z]+)$", 0, &error, &erroffset, NULL );
#else
	main_v->autoclosingtag_be_regc = pcre_compile("\\\\(begin)([a-zA-Z]+)$", 0, &error, &erroffset, NULL );
#endif
	main_v->anycommand_regc = pcre_compile("\\\\[a-z]+$", PCRE_CASELESS|PCRE_DOLLAR_ENDONLY, &error, &erroffset, NULL );
}

/**
* return_allwindows_documentlist:
*
* returns a documentlist with all documents in all windows, the list should be freed, the Tdocuments obviously not
*
* Return value: #GList* with all documents
*/
GList *return_allwindows_documentlist()
	{
		GList * newdoclist = NULL, *bflist, *tmplist = NULL;
		bflist = g_list_first( main_v->bfwinlist );
		DEBUG_MSG( "return_allwindows_documentlist, bfwinlist length=%d\n", g_list_length( main_v->bfwinlist ) );
		while ( bflist ) {
			DEBUG_MSG( "return_allwindows_documentlist, current bfwin doclist length=%d\n", g_list_length( BFWIN( bflist->data ) ->documentlist ) );
			tmplist = g_list_first( BFWIN( bflist->data ) ->documentlist );
			while ( tmplist ) {
				newdoclist = g_list_append( newdoclist, tmplist->data );
				tmplist = g_list_next( tmplist );
			}
			bflist = g_list_next( bflist );
		}
		DEBUG_MSG( "return_allwindows_documentlist, returning list length %d\n", g_list_length( newdoclist ) );
		return newdoclist;
	}

/**
* return_filenamestringlist_from_doclist:
* @doclist: #GList*
*
* Returns a stringlist with filenames given a 
* list with documents (#Tdocument*)
*
* Return value: #GList* stringlist with filenames
*/
GList *return_filenamestringlist_from_doclist( GList *doclist )
{
	GList * newlist = NULL, *tmplist;
	DEBUG_MSG( "return_filenamestringlist_from_doclist, started for doclist %p, len=%d\n", doclist, g_list_length( doclist ) );
	tmplist = g_list_first( doclist );
	while ( tmplist ) {
		if ( DOCUMENT( tmplist->data ) ->filename ) {
			DEBUG_MSG( "return_filenamestringlist_from_doclist, adding filename %s\n", DOCUMENT( tmplist->data ) ->filename );
			newlist = g_list_append( newlist, g_strdup( DOCUMENT( tmplist->data ) ->filename ) );
		}
		tmplist = g_list_next( tmplist );
	}
	return newlist;
}

/*
* return_num_untitled_documents:
* @doclist: #GList* with documents
*
* returns the number of untitled documents 
* opened in Bluefish
*
* Return value: #gint with number
*
gint return_num_untitled_documents(GList *doclist) {
	gint retval = 0;
	GList *tmplist = g_list_first(doclist);
	while (tmplist) {
		if (DOCUMENT(tmplist->data)->filename == NULL) retval++;
		tmplist = g_list_next(tmplist);
	}
	return retval;
}*/

/**
* add_filename_to_history:
* @bfwin: #Tbfwin* 
* @filename: a #gchar
* 
* adds a filename to the recently opened files list
* will not add it to the menu, only to the list and the file
**/
void add_filename_to_history( Tbfwin *bfwin, gchar *filename )
{
	gchar * dirname;

	add_to_recent_list( bfwin, filename, 0, FALSE ); /* the recent menu */
	dirname = g_path_get_dirname( filename );
	DEBUG_MSG( "add_filename_to_history, adding %s\n", dirname );
	main_v->recent_directories = add_to_history_stringlist( main_v->recent_directories, dirname, FALSE, TRUE );
	g_free( dirname );
}

/**
* documentlist_return_index_from_filename:
* @doclist: #GList* with the documents to search in
* @filename: a #gchar
* 
* if the file is open, it returns the index in the documentlist
* which is also the index in the notebook
* if the file is not open it returns -1
*
* Return value: the index number on success, -1 if the file is not open
**/
gint documentlist_return_index_from_filename( GList *doclist, gchar *filename )
{
	GList * tmplist;
	gint count = 0;

	if ( !filename ) {
		return -1;
	}

	tmplist = g_list_first( doclist );
	while ( tmplist ) {
		if ( ( ( Tdocument * ) tmplist->data ) ->filename && ( strcmp( filename, ( ( Tdocument * ) tmplist->data ) ->filename ) == 0 ) ) {
			return count;
		}
		count++;
		tmplist = g_list_next( tmplist );
	}
	return -1;
}
/**
* documentlist_return_index_from_filename:
* @param doclist: #GList* with the documents to search in
* @param filename: a #gchar
* 
* if the file is open, it returns the Tdocument* in the documentlist
* if the file is not open it returns NULL
*
* Return value: #Tdocument* or NULL if not open
**/
Tdocument *documentlist_return_document_from_filename( GList *doclist, gchar *filename )
{
	/* BUG#10 */
	GList * tmplist;
	gchar *ondiskencoding;
	struct stat statbuf;
	gint inode;
	if (!filename) {
		DEBUG_MSG( "documentlist_return_document_from_filename, no filename. returning\n" );
		return NULL;
	}
	ondiskencoding = get_filename_on_disk_encoding(filename);
	if ( stat(ondiskencoding,&statbuf) != 0 ) {
		DEBUG_MSG( "documentlist_return_document_from_filename, error calling `stat'! returning\n" );
		return NULL;
	}
	inode = statbuf.st_ino;
	g_free(ondiskencoding);

	DEBUG_MSG( "documentlist_return_document_from_filename, filename=%s\n", filename );
	tmplist = g_list_first( doclist );
	while ( tmplist ) {
		DEBUG_MSG( "documentlist_return_document_from_filename, comparing with %s\n", filename );
		ondiskencoding = get_filename_on_disk_encoding(DOCUMENT( tmplist->data ) ->filename);
		if ( DOCUMENT( tmplist->data ) ->filename
				   && ( (stat( ondiskencoding , &statbuf) ==0) && (statbuf.st_ino == inode) )
				   /* && ( strcmp( filename, DOCUMENT( tmplist->data ) ->filename ) == 0 ) */
		   )
		{
			g_free(ondiskencoding);
			DEBUG_MSG( "documentlist_return_document_from_filename, found, returning %p\n", tmplist->data );
			return DOCUMENT( tmplist->data );
		}
		/* g_print("check %s, inode1=%d, inode2=%d\n",DOCUMENT( tmplist->data ) ->filename, inode, statbuf.st_ino); */
		tmplist = g_list_next( tmplist );
		g_free(ondiskencoding);
	}
	DEBUG_MSG( "documentlist_return_document_from_filename, not found, returning NULL\n" );
	return NULL;
}

/**
* documentlist_return_document_from_index:
* @doclist: #GList* with the documents to search in
* @index: a #gint, index in the documentlist.
*
* If the index is valid, it returns the appropriate Tdocument.
*
* Return value: Pointer to Tdocument on success, NULL on invalid index.
**/
Tdocument *documentlist_return_document_from_index( GList *doclist, gint index )
{
	return ( Tdocument * ) g_list_nth_data( doclist, index );
}

/**
* doc_update_highlighting:
* @bfwin: #Tbfwin* with the window
* @callback_action: #guint ignored
* @widget: a #GtkWidget* ignored
*
* this function works on the current document
* if highlighting is disabled, this enables the highlighting
* the highlighting is also refreshed for the full document
*
* Return value: void
**/
void doc_update_highlighting( Tbfwin *bfwin, guint callback_action, GtkWidget *widget )
{
	if ( !bfwin->current_document )
		return ;
	DEBUG_MSG( "doc_update_highlighting, curdoc=%p, highlightstate=%d\n", bfwin->current_document, bfwin->current_document->view_bars & VIEW_COLORIZED );
	if ( !(bfwin->current_document->view_bars & VIEW_COLORIZED) ) {
		setup_toggle_item( gtk_item_factory_from_widget( bfwin->menubar ),
					_( "/Document/Highlight Syntax" ), TRUE );
		DEBUG_MSG( "doc_update_highlighting, calling doc_toggle_highlighting_cb\n" );
		doc_toggle_highlighting_cb( bfwin, 0, NULL );
	} else {
		doc_highlight_full( bfwin->current_document );
	}
}

/**
* doc_set_wrap:
* @doc: a #Tdocument
*
* this function will synchronise doc->wrapstate with the textview widget
* if doc->wrapstate TRUE it will set the textview to GTK_WRAP_WORD
* else (FALSE) it will set the textview to GTK_WRAP_NONE
*
* Return value: void
**/
void doc_set_wrap( Tdocument * doc )
{
	if ( doc->view_bars & MODE_WRAP ) {
		gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( doc->view ), GTK_WRAP_WORD );
	} else {
		gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( doc->view ), GTK_WRAP_NONE );
	}
}
/**
* doc_set_filetype:
* @doc: a #Tdocument
* @ft: a #Tfiletype with the new filetype
*
* this function will compare the filetype from the document and the new filetype
* and if they are different it will remove the old highlighting, set the newfiletype
* and set the filetype widget, it will return TRUE if the type was changed
*
* Return value: #gboolean if the value was changed
**/
gboolean doc_set_filetype( Tdocument *doc, Tfiletype *ft )
{
	if ( ft != doc->hl ) {
		doc_remove_highlighting( doc );
		doc->hl = ft;
		doc->need_highlighting = TRUE;
		doc->view_bars  = SET_BIT( doc->view_bars, MODE_AUTO_COMPLETE, (main_v->props.view_bars & MODE_AUTO_COMPLETE) &&  ( ft->autoclosingtag > 0 ));
		DEBUG_MSG("doc_set_filetype: autoclosingtag = %d, view_bar bitwise = %d\n", ft->autoclosingtag , GET_BIT(doc->view_bars, MODE_AUTO_COMPLETE));
		gui_set_document_widgets( doc );
		return TRUE;
	}
	return FALSE;
}
/**
* get_filetype_by_name:
* @name: a #gchar* with the filetype name
*
* returns the Tfiletype* for corresponding to name
*
* Return value: Tfiletype* 
**/
Tfiletype *get_filetype_by_name( gchar * name )
{
	GList * tmplist;
	tmplist = g_list_first( main_v->filetypelist );
	while ( tmplist ) {
		if ( strcmp( ( ( Tfiletype * ) tmplist->data ) ->type, name ) == 0 ) {
			return ( Tfiletype * ) tmplist->data;
		}
		tmplist = g_list_next( tmplist );
	}
	return NULL;
}
/**
* get_filetype_by_filename_and_content:
* @filename: a #gchar* with the filename or NULL
* @buf: a #gchar* with the contents to search for with the Tfiletype->content_regex or NULL
*
* returns the Tfiletype* for corresponding to filename, using the file extension. If
* nothing is found using the file extension or filename==NULL it will start matching 
* the contents in buf with Tfiletype->content_regex
*
* if no filetype is found it will return NULL
*
* Return value: #Tfiletype* or NULL
**/
Tfiletype *get_filetype_by_filename_and_content( gchar *filename, gchar *buf )
{
	GList * tmplist;

	if ( buf ) {
		tmplist = g_list_first( main_v->filetypelist );
		while ( tmplist ) {
			Tfiletype * ft = ( Tfiletype * ) tmplist->data;
			if ( strlen( ft->content_regex ) ) {
				pcre * pcreg;
				const char *err = NULL;
				int erroffset = 0;
				DEBUG_MSG( "get_filetype_by_filename_and_content, compiling pattern %s\n", ft->content_regex );
				pcreg = pcre_compile( ft->content_regex, PCRE_DOTALL | PCRE_MULTILINE, &err, &erroffset, NULL );
				if ( err ) {
					DEBUG_MSG( "while testing for filetype '%s', pattern '%s' resulted in error '%s' at position %d\n", ft->type, ft->content_regex, err, erroffset );
				}
				if ( pcreg ) {
					int ovector[ 30 ];
					int retval = pcre_exec( pcreg, NULL, buf, strlen( buf ), 0, 0, ovector, 30 );
					DEBUG_MSG( "get_filetype_by_filename_and_content, buf='%s'\n", buf );
					DEBUG_MSG( "get_filetype_by_filename_and_content, pcre_exec retval=%d\n", retval );
					if ( retval > 0 ) {
						/* we have a match!! */
						pcre_free( pcreg );
						return ft;
					}
					pcre_free( pcreg );
				}
			} else {
				DEBUG_MSG( "get_filetype_by_filename_and_content, type %s does not have a pattern (%s)\n", ft->type, ft->content_regex );
			}
			tmplist = g_list_next( tmplist );
		}
	}
	if ( filename ) {
		tmplist = g_list_first( main_v->filetypelist );
		while ( tmplist ) {
			if ( filename_test_extensions( ( ( Tfiletype * ) tmplist->data ) ->extensions, filename ) ) {
				return ( Tfiletype * ) tmplist->data;
			}
			tmplist = g_list_next( tmplist );
		}
	}
	return NULL;
}
/**
* doc_reset_filetype:
* @doc: #Tdocument to reset
* @newfilename: a #gchar* with the new filename
* @buf: a #gchar* with the contents of the file, or NULL if the function should get that from the TextBuffer
*
* sets the new filetype based on newfilename and content, updates the widgets and highlighting
* (using doc_set_filetype())
*
* Return value: void
**/
void doc_reset_filetype( Tdocument * doc, gchar * newfilename, gchar *buf )
{
	DEBUG_MSG("doc_reset_filetype: entering...\n");
	Tfiletype * ft;
	if ( buf ) {
		ft = get_filetype_by_filename_and_content( newfilename, buf );
	} else {
		gchar *tmp = doc_get_chars( doc, 0, main_v->props.numcharsforfiletype );
		ft = get_filetype_by_filename_and_content( newfilename, tmp );
		g_free( tmp );
	}
	if ( !ft ) {
		GList * tmplist;
		/* if none found return first set (is default set) */
		tmplist = g_list_first( main_v->filetypelist );
		if ( !tmplist ) {
			DEBUG_MSG( "doc_reset_filetype, no default filetype? huh?\n" );
			return ;
		}
		ft = ( Tfiletype * ) tmplist->data;
	}
	doc_set_filetype( doc, ft );
}

/**
* doc_set_font:
* @doc: a #Tdocument
* @fontstring: a #gchar describing the font
*
* this function will set the textview from doc to use the font
* described by fontstring
*
* Return value: void
**/

#ifdef __GNUC__
__inline__
#endif
void doc_set_font( Tdocument *doc, gchar *fontstring )
{
	if ( fontstring ) {
		apply_font_style( doc->view, fontstring );
	} else {
		apply_font_style( doc->view, main_v->props.editor_font_string );
	}
}

/**
* This function is taken from gtksourceview
* Copyright (C) 2001
* Mikael Hermansson <tyan@linux.se>
* Chris Phelps <chicane@reninet.com>
*/
static gint textview_calculate_real_tab_width( GtkWidget *textview, gint tab_size )
{
	gchar * tab_string;
	gint counter = 0;
	gint tab_width = 0;

	if ( tab_size <= 0 )
		return 0;

	tab_string = g_malloc ( tab_size + 1 );
	while ( counter < tab_size ) {
		tab_string[ counter ] = ' ';
		counter++;
	}
	tab_string[ tab_size ] = '\0';
	tab_width = widget_get_string_size( textview, tab_string );
	g_free( tab_string );
	/*	if (tab_width < 0) tab_width = 0;*/
	return tab_width;
}

/**
* doc_set_tabsize:
* @doc: a #Tdocument
* @tabsize: a #gint with the tab size
*
* this function will set the textview from doc to use the tabsize
* described by tabsize
*
* Return value: void
**/
void doc_set_tabsize( Tdocument *doc, gint tabsize )
{
	PangoTabArray * tab_array;
	gint pixels = textview_calculate_real_tab_width( GTK_WIDGET( doc->view ), tabsize );
	DEBUG_MSG( "doc_set_tabsize, tabsize=%d, pixels=%d\n", tabsize, pixels );
	tab_array = pango_tab_array_new ( 1, TRUE );
	pango_tab_array_set_tab ( tab_array, 0, PANGO_TAB_LEFT, pixels );
	gtk_text_view_set_tabs ( GTK_TEXT_VIEW ( doc->view ), tab_array );
	pango_tab_array_free( tab_array );
}

/**
* gui_change_tabsize:
* @bfwin: #Tbfwin* with the window
* @action: a #guint, if 1 increase the tabsize, if 0 decrease
* @widget: a #GtkWidget, ignored
*
* this function is the callback for the menu, based on action
* it will increase or decrease the tabsize by one 
* for ALL DOCUMENTS (BUG: currently only all documents in the same window)
*
* Return value: void
**/
void gui_change_tabsize( Tbfwin *bfwin, guint action, GtkWidget *widget )
{
	GList * tmplist;
	PangoTabArray *tab_array;
	gint pixels;
	if ( action == 1 ) {
		main_v->props.editor_tab_width++;
	} else {
		main_v->props.editor_tab_width--;
	}
	{
		gchar *message = g_strdup_printf( "Setting tabsize to %d", main_v->props.editor_tab_width );
		statusbar_message( bfwin, message, 2000 );
		g_free( message );
	}
	/* this should eventually be the total documentlist, not only for this window */
	tmplist = g_list_first( bfwin->documentlist );
	pixels = textview_calculate_real_tab_width( GTK_WIDGET( ( ( Tdocument * ) tmplist->data ) ->view ), main_v->props.editor_tab_width );
	tab_array = pango_tab_array_new ( 1, TRUE );
	pango_tab_array_set_tab ( tab_array, 0, PANGO_TAB_LEFT, pixels );
	while ( tmplist ) {
		gtk_text_view_set_tabs ( GTK_TEXT_VIEW( ( ( Tdocument * ) tmplist->data ) ->view ), tab_array );
		tmplist = g_list_next( tmplist );
	}
	pango_tab_array_free( tab_array );
}
/**
* doc_is_empty_non_modified_and_nameless:
* @doc: a #Tdocument
*
* this function returns TRUE if the document pointer to by doc
* is an empty, nameless and non-modified document
*
* Return value: gboolean, TRUE if doc is empty, non-modified and nameless
**/
gboolean doc_is_empty_non_modified_and_nameless( Tdocument *doc )
{
	if ( !doc ) {
		return FALSE;
	}
	if ( doc->modified || doc->filename ) {
		return FALSE;
	}
	if ( gtk_text_buffer_get_char_count( doc->buffer ) > 0 ) {
		return FALSE;
	}
	return TRUE;
}


/* gboolean test_docs_modified(GList *doclist)
* if doclist is NULL it will use main_v->documentlist as doclist
* returns TRUE if there are any modified documents in doclist
* returns FALSE if there are no modified documents in doclist
*/

/**
* test_docs_modified:
* @doclist: a #GList with documents
*
* this function will test if any documents in doclist are modified
*
* Return value: gboolean
**/

gboolean test_docs_modified( GList *doclist )
{

	GList * tmplist;
	Tdocument *tmpdoc;

	if ( doclist ) {
		tmplist = g_list_first( doclist );
	} else {
		g_print( "test_docs_modified, calling without a doclist is deprecated, aborting\n" );
		exit( 144 );
	}

	while ( tmplist ) {
		tmpdoc = ( Tdocument * ) tmplist->data;
#ifdef DEBUG
g_assert( tmpdoc );
#endif

		if ( tmpdoc->modified ) {
			return TRUE;
		}
		tmplist = g_list_next( tmplist );
	}
	return FALSE;
}
/**
* test_only_empty_doc_left:
* @doclist: #GList* with all documents to test in
*
* returns TRUE if there is only 1 document open, and that document
* is not modified and 0 bytes long and without filename
* returns FALSE if there are multiple documents open, or 
* a modified document is open, or a > 0 bytes document is open
* or a document with filename is open
*
* Return value: void
**/
gboolean test_only_empty_doc_left( GList *doclist )
{
	if ( g_list_length( doclist ) > 1 ) {
		return FALSE;
	} else {
		Tdocument *tmpdoc;
		GList *tmplist = g_list_first( doclist );
		if ( tmplist ) {
#ifdef DEBUG
g_assert( tmplist->data );
#endif

			tmpdoc = tmplist->data;
			if ( !doc_is_empty_non_modified_and_nameless( tmpdoc ) ) {
				return FALSE;
			}
		}
	}
	return TRUE;
}
/**
* doc_move_to_window:
* @doc: #Tdocument*
* @newwin: #Tbfwin*
*
* detaches the document from it's old window (doc->bfwin) and attaches
* it to the window newwin
*
* Return value: void, ignored
*/
void doc_move_to_window( Tdocument *doc, Tbfwin *newwin )
{
	Tbfwin * oldwin = BFWIN( doc->bfwin );
	GtkWidget *tab_widget, *scroll, *tab_menu;

	DEBUG_MSG( "doc_move_to_window, oldwin=%p, newwin=%p, doc=%p\n", oldwin, newwin, doc );

	/* FIXED: Major BUG#54 */
	/* (doc_new):
		tab_label -> tab_eventbox -> hbox
	So we need parent->parent;
	*/
	tab_widget = doc->tab_label->parent->parent;
	scroll = doc->view->parent;
	tab_menu = doc->tab_menu;

	gtk_widget_ref( scroll );
	gtk_widget_ref( tab_widget );
	gtk_widget_ref( tab_menu );

	/* DEBUG_MSG( "doc_move_to_window, first:\n\tdoc->tab_label=%p, tab_label=%p\n", doc->tab_label, tab_label ); */
	gtk_notebook_remove_page( GTK_NOTEBOOK( oldwin->notebook ), g_list_index( oldwin->documentlist, doc ) );
	oldwin->documentlist = g_list_remove( oldwin->documentlist, doc );
	
	DEBUG_MSG( "doc_move_to_window, removed doc=%p from oldwin %p\n", doc, oldwin );
	doc->bfwin = newwin;
	newwin->documentlist = g_list_append( newwin->documentlist, doc );
	gtk_notebook_append_page_menu( GTK_NOTEBOOK( newwin->notebook ), scroll, tab_widget, tab_menu );
	DEBUG_MSG( "doc_move_to_window, appended doc=%p to newwin %p\n", doc, newwin );

	gtk_widget_unref( scroll );
	gtk_widget_unref( tab_widget );
	gtk_widget_unref( tab_menu );

	gtk_widget_show_all( scroll );
	gtk_widget_show_all( tab_widget );
	gtk_widget_show ( tab_menu );
	
	if ( NULL == oldwin->documentlist ) {
		file_new_cb( NULL, oldwin );
	}
}
/**
* doc_has_selection:
* @doc: a #Tdocument
*
* returns TRUE if the document has a selection
* returns FALSE if it does not
*
* Return value: gboolean
**/
gboolean doc_has_selection( Tdocument *doc )
{
	return gtk_text_buffer_get_selection_bounds( doc->buffer, NULL, NULL );
}

/**
* doc_set_tooltip:
* @doc: #Tdocument*
*
* will set the tooltip on the notebook tab eventbox
*
* Return value: void
*/
static void doc_set_tooltip( Tdocument *doc )
{
	gchar * text, *tmp;
	gchar mtimestr[ 128 ], *modestr = NULL, *sizestr = NULL;
	mtimestr[ 0 ] = '\0';
	if ( doc->statbuf.st_mode != 0 || doc->statbuf.st_size != 0 ) {
		modestr = filemode_to_string( doc->statbuf.st_mode );
		ctime_r( &doc->statbuf.st_mtime, mtimestr );
		/* sizestr = g_strdup_printf( "%ld", doc->statbuf.st_size ); */
		/*  Michèle Garoche, 20060208 */
		if (sizeof(off_t) == sizeof(unsigned long long int)) {
			sizestr = g_strdup_printf("%llu", (unsigned long long int )doc->statbuf.st_size);
		} else {
			sizestr = g_strdup_printf("%lu", doc->statbuf.st_size);
		 }
	}
	tmp = text = g_strconcat( _( "name: " ), gtk_label_get_text( GTK_LABEL( doc->tab_menu ) )
					, _( "\ntype: " ), doc->hl->type
					, _( "\nencoding: " ), ( doc->encoding != NULL ) ? doc->encoding : main_v->props.newfile_default_encoding
					, NULL );
	if ( sizestr ) {
		text = g_strconcat( text, _( "\nsize (on disk): " ), sizestr, _( " bytes" ), NULL );
		g_free( tmp );
		g_free( sizestr );
		tmp = text;
	}
	if ( modestr ) {
		text = g_strconcat( text, _( "\npermissions: " ), modestr, NULL );
		g_free( tmp );
		g_free( modestr );
		tmp = text;
	}
	if ( mtimestr[ 0 ] != '\0' ) {
		trunc_on_char( mtimestr, '\n' );
		text = g_strconcat( text, _( "\nlast modified: " ), mtimestr, NULL );
		g_free( tmp );
		tmp = text;
	}

	gtk_tooltips_set_tip( main_v->tooltips, doc->tab_eventbox, text, "" );
	g_free( text );
}
/**
* doc_set_title:
* @doc: #Tdocument*
*
* will set the notebook tab label and the notebook tab menu label
* and if this document->bfwin == document->bfwin->current_document
* it will update the bfwin title
* it will also call doc_set_tooltip() to reflect the changes in the tooltip
*
* Return value: void
*/
static void doc_set_title( Tdocument *doc )
{
	gchar * label_string, *tabmenu_string;
	if ( doc->filename ) {
		label_string = g_path_get_basename( doc->filename );
		tabmenu_string = g_strdup( doc->filename );
	} else {
		label_string = g_strdup_printf( _( "Untitled %d" ), main_v->num_untitled_documents );
		tabmenu_string = g_strdup( label_string );
		main_v->num_untitled_documents++;
	}
	gtk_label_set( GTK_LABEL( doc->tab_menu ), tabmenu_string );
	gtk_label_set( GTK_LABEL( doc->tab_label ), label_string );
	doc_set_tooltip( doc );
	g_free( label_string );
	g_free( tabmenu_string );
	if ( doc->bfwin == BFWIN( doc->bfwin ) ->current_document ) {
		gui_set_title( doc->bfwin, doc );
	}
}
/**
* doc_set_modified:
* @doc: a #Tdocument
* @value: a gint TRUE or FALSE
*
* sets the doc->modified to value
* if it already has this value, do nothing
* if it does not have this value, it will do some action
*
* if the document pointed to by doc == the current document
* it will update the toolbar and menu undo/redo items
*
* if value is TRUE, it will make the notebook and notebook-menu
* label red, if value is FALSE it will set them to black
*
* Return value: void
**/
void doc_set_modified( Tdocument *doc, gint value )
{
	DEBUG_MSG( "doc_set_modified, started, doc=%p, value=%d\n", doc, value );
	if ( doc->modified != value ) {
		GdkColor colorred = {0, 0,0, 65535};
		GdkColor colorblack = {0, 0, 0, 0};

		doc->modified = value;
		if ( doc->modified ) {
			gtk_widget_modify_fg( doc->tab_menu, GTK_STATE_NORMAL, &colorred );
			gtk_widget_modify_fg( doc->tab_menu, GTK_STATE_PRELIGHT, &colorred );
			gtk_widget_modify_fg( doc->tab_label, GTK_STATE_NORMAL, &colorred );
			gtk_widget_modify_fg( doc->tab_label, GTK_STATE_PRELIGHT, &colorred );
			gtk_widget_modify_fg( doc->tab_label, GTK_STATE_ACTIVE, &colorred );
		} else {
			gtk_widget_modify_fg( doc->tab_menu, GTK_STATE_NORMAL, &colorblack );
			gtk_widget_modify_fg( doc->tab_menu, GTK_STATE_PRELIGHT, &colorblack );
			gtk_widget_modify_fg( doc->tab_label, GTK_STATE_NORMAL, &colorblack );
			gtk_widget_modify_fg( doc->tab_label, GTK_STATE_PRELIGHT, &colorblack );
			gtk_widget_modify_fg( doc->tab_label, GTK_STATE_ACTIVE, &colorblack );
		}
	}
#ifdef DEBUG
	else {
		DEBUG_MSG( "doc_set_modified, doc %p did have value %d already\n", doc, value );
	}
#endif
	/* only when this is the current document we have to change these */
	DEBUG_MSG( "doc=%p, doc->bfwin=%p\n", doc, doc->bfwin );
	if ( doc == BFWIN( doc->bfwin ) ->current_document ) {
		gui_set_undo_redo_widgets( BFWIN( doc->bfwin ), doc_has_undo_list( doc ), doc_has_redo_list( doc ) );
	}
}
	
/* returns 1 if the file is modified on disk, returns 0
if the file is modified by another process, returns
0 if there was no previous mtime information available
if newstatbuf is not NULL, it will be filled with the new statbuf from the file IF IT WAS CHANGED!!!
leave NULL if you do not need this information, if the file is not changed, this field will not be set!!
*/
static gboolean doc_check_modified_on_disk( Tdocument *doc, struct stat *newstatbuf )
{
	if ( main_v->props.modified_check_type == 0 || !doc->filename || doc->statbuf.st_mtime == 0 || doc->statbuf.st_size == 0 )
	{
		return FALSE;
	} else if ( main_v->props.modified_check_type < 4 )
	{
		struct stat statbuf;
		gchar *ondiskencoding = get_filename_on_disk_encoding( doc->filename );
		if ( stat( ondiskencoding, &statbuf ) == 0 ) {
			g_free( ondiskencoding );
			*newstatbuf = statbuf;
			if ( main_v->props.modified_check_type == 1 || main_v->props.modified_check_type == 2 ) {
				if ( doc->statbuf.st_mtime < statbuf.st_mtime ) {
					return TRUE;
				}
			}
			if ( main_v->props.modified_check_type == 1 || main_v->props.modified_check_type == 3 ) {
				if ( doc->statbuf.st_size != statbuf.st_size ) {
					return TRUE;
				}
			}
		} else
			g_free( ondiskencoding );
	} else
	{
		DEBUG_MSG( "doc_check_mtime, type %d checking not yet implemented\n", main_v->props.modified_check_type );
	}
	return FALSE;
}

/* doc_set_stat_info() includes setting the mtime field, so there is no need
to call doc_update_mtime() as well */
static void doc_set_stat_info( Tdocument *doc )
{
	if ( doc->filename ) {
		gchar * ondiskencoding = get_filename_on_disk_encoding( doc->filename );

	struct stat statbuf;
	if ( lstat( ondiskencoding, &statbuf ) == 0 ) {
		if ( S_ISLNK( statbuf.st_mode ) ) {
			doc->is_symlink = 1;
			stat( ondiskencoding, &statbuf );
		} else {
			doc->is_symlink = 0;
		}
		doc->statbuf = statbuf;
	}
		g_free( ondiskencoding );
		doc_set_tooltip( doc );
	}
}

/**
* doc_scroll_to_cursor:
* @doc: a #Tdocument
* 
* scolls the document pointer to by doc to its cursor position, 
* making the cursor visible
* 
* Return value: void
**/
void doc_scroll_to_cursor( Tdocument *doc )
{
	GtkTextMark * mark = gtk_text_buffer_get_insert( doc->buffer );
	gtk_text_view_scroll_to_mark( GTK_TEXT_VIEW( doc->view ), mark, 0.25, FALSE, 0.5, 0.5 );
}

/**
* doc_get_chars:
* @doc: a #Tdocument
* @start: a #gint, the start position
* @end: a #gint, the end position
* 
* returns all characters (NOT BYTES!!) from start to end from the document 
* pointer to by doc. end may be -1 to point to the end of the document
* 
* Return value: gchar * with the requested characters
**/
gchar *doc_get_chars( Tdocument *doc, gint start, gint end )
{
	GtkTextIter itstart, itend;
	gchar *string;

	gtk_text_buffer_get_iter_at_offset( doc->buffer, &itstart, start );
	if ( end >= 0 ) {
		gtk_text_buffer_get_iter_at_offset( doc->buffer, &itend, end );
	} else if ( end == -1 ) {
		gtk_text_buffer_get_end_iter( doc->buffer, &itend );
	} else {
		DEBUG_MSG( "doc_get_chars, end < -1, returning NULL\n" );
		return NULL;
	}
	DEBUG_MSG( "doc_get_chars, retrieving string, start=%d, end=%d\n", start, end );
	string = gtk_text_buffer_get_text( doc->buffer, &itstart, &itend, FALSE );
	DEBUG_MSG( "doc_get_chars, retrieved string (%p)\n", string );
	return string;
}
/**
* doc_get_max_offset:
* @doc: a #Tdocument
* 
* returns the number of characters (NOT BYTES!!) in this document
* 
* Return value: gint with the number of characters
**/
gint doc_get_max_offset( Tdocument *doc )
{
	return gtk_text_buffer_get_char_count( doc->buffer );
}

/**
* doc_select_region:
* @doc: a #Tdocument
* @start: a #gint with the start of selection
* @end: a #gint with the end of the selection
* @do_scroll: a #gboolean, if we should scroll to the selection
* 
* selects from start to end in the doc, and if do_scroll is set it will make
* sure the selection is visible to the user
*
* Return value: void
**/
void doc_select_region( Tdocument *doc, gint start, gint end, gboolean do_scroll )
{
	GtkTextIter itstart, itend;
	gtk_text_buffer_get_iter_at_offset( doc->buffer, &itstart, start );
	gtk_text_buffer_get_iter_at_offset( doc->buffer, &itend, end );
	gtk_text_buffer_move_mark_by_name( doc->buffer, "insert", &itstart );
	gtk_text_buffer_move_mark_by_name( doc->buffer, "selection_bound", &itend );
	if ( do_scroll ) {
		gtk_text_view_scroll_to_iter( GTK_TEXT_VIEW( doc->view ), &itstart, 0.25, FALSE, 0.5, 0.5 );
	}
}

/**
* doc_select_line:
* @doc: a #Tdocument
* @line: a #gint with the line number to select
* @do_scroll: a #gboolean, if we should scroll to the selection
* 
* selects the line in doc, and if do_scroll is set it will make
* sure the selection is visible to the user
* the line number starts at line 1, not at line 0!!
*
* Return value: void
**/
void doc_select_line( Tdocument *doc, gint line, gboolean do_scroll )
{
	GtkTextIter itstart;
	gtk_text_buffer_get_iter_at_line( doc->buffer, &itstart, line - 1 );
#ifdef SELECT_LINE
	GtkTextIter itend = itstart;
	/* do the section */
	gtk_text_iter_forward_to_line_end( &itend );
	gtk_text_buffer_move_mark_by_name( doc->buffer, "insert", &itstart );
	gtk_text_buffer_move_mark_by_name( doc->buffer, "selection_bound", &itend );
	if ( do_scroll ) {
		gtk_text_view_scroll_to_iter( GTK_TEXT_VIEW( doc->view ), &itstart, 0.25, FALSE, 0.5, 0.5 );
		/*
		GtkTextMark *tmpmark = gtk_text_buffer_get_mark( doc->buffer, "insert" );
		gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW( doc->view ), tmpmark, 0.25, FALSE, 0.5, 0.5 );
		*/
	}
#else
	gtk_text_buffer_move_mark_by_name( doc->buffer, "insert", &itstart );
	gtk_text_buffer_move_mark_by_name( doc->buffer, "selection_bound", &itstart );
	if ( do_scroll ) {
		gtk_text_view_scroll_to_iter( GTK_TEXT_VIEW( doc->view ), &itstart, 0.25, FALSE, 0.5, 0.5 );
	}
#endif /* SELECT_LINE */
}

/**
* doc_get_selection:
* @doc: a #Tdocument
* @start: a #gint * to store the start
* @end: a #gint * to store the end
* 
*  returns FALSE if there is no selection
*  returns TRUE if there is a selection, and start and end will be set
*  to the current selection
*
* Return value: gboolean if there is a selection
**/
gboolean doc_get_selection( Tdocument *doc, gint *start, gint *end )
{
	GtkTextIter itstart, itend;
	GtkTextMark *mark = gtk_text_buffer_get_insert( doc->buffer );
	gtk_text_buffer_get_iter_at_mark( doc->buffer, &itstart, mark );
	mark = gtk_text_buffer_get_selection_bound( doc->buffer );
	gtk_text_buffer_get_iter_at_mark( doc->buffer, &itend, mark );
	*start = gtk_text_iter_get_offset( &itstart );
	*end = gtk_text_iter_get_offset( &itend );
	DEBUG_MSG( "doc_get_selection, start=%d, end=%d\n", *start, *end );
	if ( *start == *end ) {
		return FALSE;
	}
	if ( *start > *end ) {
		gint tmp = *start;
		*start = *end;
		*end = tmp;
	}
	return TRUE;
}
/**
* doc_get_cursor_position:
* @doc: a #Tdocument
* 
* returns the cursor position in doc as character offset
*
* Return value: gint with the character offset of the cursor
**/
gint doc_get_cursor_position( Tdocument *doc )
{
	GtkTextIter iter;
	GtkTextMark *mark = gtk_text_buffer_get_insert( doc->buffer );
	gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, mark );
	return gtk_text_iter_get_offset( &iter );
}

/**
* doc_set_statusbar_lncol:
* @doc: a #Tdocument
* 
* Return value: void
**/
static void doc_set_statusbar_lncol( Tdocument *doc )
{
	gchar * msg;
	gint line;
	gint col = 0;
	GtkTextIter iter, start;

	gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, gtk_text_buffer_get_insert( doc->buffer ) );

	line = gtk_text_iter_get_line( &iter );

	start = iter;
	gtk_text_iter_set_line_offset( &start, 0 );

	while ( !gtk_text_iter_equal( &start, &iter ) ) {
		if ( gtk_text_iter_get_char( &start ) == '\t' ) {
			col += ( main_v->props.editor_tab_width - ( col % main_v->props.editor_tab_width ) );
		} else
			++col;
		gtk_text_iter_forward_char( &start );
	}

	msg = g_strdup_printf( "%d,%d", line + 1, col + 1 );

	gtk_statusbar_pop( GTK_STATUSBAR( BFWIN( doc->bfwin ) ->statusbar_lncol ), 0 );
	gtk_statusbar_push( GTK_STATUSBAR( BFWIN( doc->bfwin ) ->statusbar_lncol ), 0, msg );

	g_free( msg );
}

/**
* doc_set_statusbar_insovr:
* @doc: a #Tdocument
* 
* 
*
* Return value: void
**/
void doc_set_statusbar_insovr( Tdocument *doc )
{
	gtk_statusbar_pop( GTK_STATUSBAR( BFWIN( doc->bfwin ) ->statusbar_insovr ), 0 );
	gtk_statusbar_push( GTK_STATUSBAR( BFWIN( doc->bfwin ) ->statusbar_insovr ), 0, ( doc->view_bars & MODE_OVERWRITE ? "OVR" : "INS" ) );
}
/**
* doc_set_statusbar_editmode_encoding:
* @doc: a #Tdocument
* 
* 
* 
*
* Return value: void
**/
void doc_set_statusbar_editmode_encoding( Tdocument *doc )
{
	gchar * msg;
	if ( doc->hl == NULL )
		msg = g_strdup_printf( "%s, %s", "text", doc->encoding );
	else
		msg = g_strdup_printf( "%s, %s", doc->hl->type, doc->encoding );
	gtk_statusbar_pop( GTK_STATUSBAR( BFWIN( doc->bfwin ) ->statusbar_editmode ), 0 );
	gtk_statusbar_push( GTK_STATUSBAR( BFWIN( doc->bfwin ) ->statusbar_editmode ), 0, msg );
	g_free( msg );
}

/**
* doc_replace_text_backend:
* @doc: a #Tdocument
* @newstring: a #const char * with the new string
* @start: a gint with the start character position
* @end: a gint with the end character position
* 
* unbinds all signals so there will be no call to a highlighting 
* update or anything else
* deletes the text in the region between start and end
* registers that text to the undo/redo functionality
* inserts newstring at that same position
* registers this to the undo/redo functionality
* marks the document as modified and marks it as needing highlighting
* binds the signals again to their callbacks
*
* multiple calls to doc_replace_text_backend will all be in the same undo/redo group
*
* Return value: void
**/
void doc_replace_text_backend( Tdocument *doc, const gchar * newstring, gint start, gint end )
{
	doc_unbind_signals( doc );
	/* delete region, and add that to undo/redo list */
	{
		gchar *buf;
		GtkTextIter itstart, itend;
		DEBUG_MSG( "doc_replace_text_backend, get iters at start %d and end %d\n", start, end );
		gtk_text_buffer_get_iter_at_offset( doc->buffer, &itstart, start );
		gtk_text_buffer_get_iter_at_offset( doc->buffer, &itend, end );
		buf = gtk_text_buffer_get_text( doc->buffer, &itstart, &itend, FALSE );
		gtk_text_buffer_delete( doc->buffer, &itstart, &itend );
		DEBUG_MSG( "doc_replace_text_backend, calling doc_unre_add for buf=%s, start=%d and end=%d\n", buf, start, end );
		doc_unre_add( doc, buf, start, end, UndoDelete );
		g_free( buf );
		DEBUG_MSG( "doc_replace_text_backend, text deleted from %d to %d\n", start, end );
	}

	/* add new text to this region, the buffer is changed so re-calculate itstart */
	{
		GtkTextIter itstart;
		gint insert = ( end > start ) ? start : end;
		DEBUG_MSG( "doc_replace_text_backend, set insert pos to %d\n", insert );
		gtk_text_buffer_get_iter_at_offset( doc->buffer, &itstart, insert );
		gtk_text_buffer_insert( doc->buffer, &itstart, newstring, -1 );
		doc_unre_add( doc, newstring, insert, insert + g_utf8_strlen( newstring, -1 ), UndoInsert );
	}
	doc_bind_signals( doc );
	doc_set_modified( doc, 1 );
	doc->need_highlighting = TRUE;
}
/**
* doc_replace_text:
* @doc: a #Tdocument
* @newstring: a #const char * with the new string
* @start: a gint with the start character position
* @end: a gint with the end character position
* 
* identical to doc_replace_text_backend, with one difference, multiple calls to
* doc_replace_text will be all be in a different undo/redo group
*
* Return value: void
**/
void doc_replace_text( Tdocument * doc, const gchar * newstring, gint start, gint end )
{
	doc_unre_new_group( doc );
	doc_replace_text_backend( doc, newstring, start, end );
	doc_unre_new_group( doc );
}

/* kyanh, removed, 20050303 */
static void doc_convert_chars_to_entities(Tdocument *doc, gint start, gint end, gboolean ascii, gboolean iso) {
	gchar *string;
	DEBUG_MSG("doc_convert_chars_to_entities, start=%d, end=%d\n", start,end);
	string = doc_get_chars(doc, start, end);
	if (string) {
		gchar *newstring = convert_string_utf8_to_html(string, ascii, iso);
		g_free(string);
		if (newstring) {
			doc_replace_text(doc, newstring, start, end);
			g_free(newstring);
		}
#ifdef DEBUG
		else {
			DEBUG_MSG("doc_convert_chars_to_entities, newstring=NULL\n");
		}
#endif
	}
#ifdef DEBUG
		else {
			DEBUG_MSG("doc_convert_chars_to_entities, string=NULL\n");
		}
#endif		 
}

/* kyanh, removed, 20050303 */
static void doc_convert_chars_to_entities_in_selection(Tdocument *doc, gboolean ascii, gboolean iso) {
	gint start, end;
	if (doc_get_selection(doc, &start, &end)) {
		DEBUG_MSG("doc_convert_chars_to_entities_in_selection, start=%d, end=%d\n", start, end);
		doc_convert_chars_to_entities(doc, start, end, ascii, iso);
	}
}

static void doc_convert_case_in_selection( Tdocument *doc, gboolean toUpper )
{
	gint start, end;
	if ( doc_get_selection( doc, &start, &end ) ) {
		gchar * string = doc_get_chars( doc, start, end );
		if ( string ) {
			gchar * newstring = ( toUpper & 1/* menu.c */ ) ? g_utf8_strup( string, -1 ) : g_utf8_strdown( string, -1 );
			g_free( string );
			if ( newstring ) {
				doc_replace_text( doc, newstring, start, end );
				g_free( newstring );
			}
		}
	}
}


/**
* doc_insert_two_strings:
* @doc: a #Tdocument
* @before_str: a #const char * with the first string
* @after_str: a #const char * with the second string
* 
* if the marks 'diag_ins' and 'diag_sel' exist, they will be used
* as pos1 and pos2
* if a selection exists, the selection start and end will be pos1 and pos2
* if both not exist the cursor position will be both pos1 and pos2
*
* inserts the first string at pos1 and the second at pos2 in doc
* it does not unbind any signal, so the insert callback will have to do 
* do the undo/redo, modified and highlighting stuff
*
* multiple calls to this function will be in separate undo/redo groups
*
* Return value: void
**/ 
/*
*/
void doc_insert_two_strings( Tdocument *doc, const gchar *before_str, const gchar *after_str )
{
	/*
		indent the content -- if the content starts by
		* \begin{}
		* \start{}
	*/
	if (main_v->props.view_bars & MODE_AUTO_INDENT/* && g_str_has_prefix(before_str, "\\begin{") */)
	{
		gchar *indent=NULL;
		GtkTextMark* imark;
		GtkTextIter itstart, itend;
		imark = gtk_text_buffer_get_insert( doc->buffer );
		gtk_text_buffer_get_iter_at_mark( doc->buffer, &itend, imark );
		itstart = itend;
		/* set to the beginning of current line */
		gtk_text_iter_set_line_index( &itstart, 0 );
		indent = gtk_text_buffer_get_text( doc->buffer, &itstart, &itend, FALSE );
		if ( indent ) {
			/* now count the number of spaces in this line */
			gchar *indenting = indent;
			while ( *indenting == '\t' || *indenting == ' ' ) {
				indenting++;
			}
			/* ending search, non-whitespace found, so terminate at this position */
			*indenting = '\0';
			if ( strlen( indent ) ) {
				/* add indent to before_str and after_str */
				gchar **output = NULL;
				gchar **tmpchar;
				gint count;
				if (before_str) {
					output = g_strsplit(before_str, "\n", 0);
					count=0;
					tmpchar=output;
					if (tmpchar) {
						while (*tmpchar != NULL) {
							if (count==0) {
								before_str = g_strdup(output[0]);
							}else{
								/* add indent for the lines with the index  >= 2 */
								before_str = g_strconcat(before_str, "\n", indent, output[count], NULL);
							}
							count++;
							tmpchar++;
						}
					}
				}
				if (after_str) {
					/* for after_str */
					output = g_strsplit(after_str, "\n", 0);
					count=0;
					tmpchar=output;
					if (tmpchar) {
						while (*tmpchar != NULL) {
							if (count) {
								after_str = g_strconcat(after_str, "\n", indent, output[count], NULL);
							} else {
								/* hi hi i donnot know why... but i have to count this case. */
								after_str = g_strdup(output[0]);
							}
							count++;
							tmpchar++;
						}
					}
					/* alternative: use split then join; but this is*NOT* always good; */
				}
				g_free(indent);
				g_strfreev(output);
			}
		}
	}

	GtkTextIter itinsert, itselect;
	GtkTextMark *insert, *select;
	gboolean have_diag_marks = FALSE;
	/* kyanh, removed, 20050312 */
	insert = gtk_text_buffer_get_mark( doc->buffer, "diag_ins" );
	if ( insert ) {
		select = gtk_text_buffer_get_mark( doc->buffer, "diag_sel" );
		have_diag_marks = TRUE;
	} else {
		insert = gtk_text_buffer_get_insert( doc->buffer );
		select = gtk_text_buffer_get_selection_bound( doc->buffer );
	}

	gtk_text_buffer_get_iter_at_mark( doc->buffer, &itinsert, insert );
	gtk_text_buffer_get_iter_at_mark( doc->buffer, &itselect, select );
#ifdef DEBUG
	DEBUG_MSG( "doc_insert_two_strings, current marks: itinsert=%d, itselect=%d\n", gtk_text_iter_get_offset( &itinsert ), gtk_text_iter_get_offset( &itselect ) );
#endif

	if ( gtk_text_iter_equal( &itinsert, &itselect ) ) {
		/* no selection */
		gchar * double_str = g_strconcat( before_str, after_str, NULL );
		gtk_text_buffer_insert( doc->buffer, &itinsert, double_str, -1 );
		g_free( double_str );
		if ( after_str && strlen( after_str ) ) {
			/* the buffer has changed, but gtk_text_buffer_insert makes sure */
			/* that itinsert points to the end of the inserted text. */
			/* thus, no need to get a new one. */
			gtk_text_iter_backward_chars( &itinsert, g_utf8_strlen( after_str, -1 ) );
			gtk_text_buffer_place_cursor( doc->buffer, &itinsert );
			gtk_widget_grab_focus( doc->view );
		}
	} else { /* there is a selection */
		GtkTextMark *marktoresetto;
		GtkTextIter firstiter;
		if ( gtk_text_iter_compare( &itinsert, &itselect ) < 0 ) { /* select backward */
			firstiter = itinsert;
			marktoresetto = ( have_diag_marks ) ? gtk_text_buffer_get_selection_bound( doc->buffer ) : select;
			/* marktoresetto = select; */
		} else { /* select forward */
			firstiter = itselect;
			marktoresetto = ( have_diag_marks ) ? gtk_text_buffer_get_insert( doc->buffer ) : insert;
		}
		/* there is a selection */
		gtk_text_buffer_insert( doc->buffer, &firstiter, before_str, -1 );
		if ( after_str && strlen( after_str ) ) {
			/* the buffer is changed, reset the select iterator */
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &itselect, marktoresetto );
			gtk_text_buffer_insert( doc->buffer, &itselect, after_str, -1 );
			/* now the only thing left is to move the selection and insert mark back to their correct places to preserve the users selection */
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &itselect, marktoresetto );
			gtk_text_iter_backward_chars( &itselect, g_utf8_strlen( after_str, -1 ) );
			gtk_text_buffer_move_mark( doc->buffer, marktoresetto, &itselect );
		}

	}
	doc_unre_new_group( doc );
	DEBUG_MSG( "doc_insert_two_strings, finished\n" );
}


static void add_encoding_to_list( gchar *encoding )
{
	gchar **enc = g_new0( gchar *, 3 );
	enc[ 0 ] = g_strdup( encoding );
	if ( !arraylist_value_exists( main_v->props.encodings, enc, 1, FALSE ) ) {
		GList * tmplist;
		enc[ 1 ] = g_strdup( encoding );
		main_v->props.encodings = g_list_insert( main_v->props.encodings, enc, 1 );
		tmplist = g_list_first( main_v->bfwinlist );
		while ( tmplist ) {
			encoding_menu_rebuild( BFWIN( tmplist->data ) );
			tmplist = g_list_next( tmplist );
		}
	} else {
		g_free( enc[ 0 ] );
		g_free( enc );
	}
}

static gchar *get_buffer_from_filename( Tbfwin *bfwin, gchar *filename, int *returnsize )
{
	gboolean result;
	gchar *buffer;
	GError *error = NULL;
	gsize length;
	gchar *ondiskencoding = get_filename_on_disk_encoding( filename );
	result = g_file_get_contents( ondiskencoding, &buffer, &length, &error );
	g_free( ondiskencoding );
	if ( result == FALSE ) {
		gchar * errmessage = g_strconcat( _( "Could not read file:\n" ), filename, NULL );
		warning_dialog( bfwin->main_window, errmessage, NULL );
		g_free( errmessage );
		return NULL;
	}
	*returnsize = length;
	return buffer;
}

/**
* doc_file_to_textbox:
* @doc: The #Tdocument target.
* @filename: Filename to read in.
* @enable_undo: #gboolean
* @delay: Whether to delay GUI-calls.
*
* Open and read in a file to the doc buffer.
* The data is inserted starting at the current cursor position.
* Charset is detected, and highlighting performed (if applicable).
*
* Return value: A #gboolean, TRUE if successful, FALSE on error.
**/
gboolean doc_file_to_textbox( Tdocument * doc, gchar * filename, gboolean enable_undo, gboolean delay )
{
	gchar * message;
	gint cursor_offset;
	int document_size = 0;

	if ( !enable_undo ) {
		doc_unbind_signals( doc );
	}
	message = g_strconcat( _( "Opening file " ), filename, NULL );
	statusbar_message( BFWIN( doc->bfwin ), message, 1000 );
	g_free( message );

	/* now get the current cursor position */
	{
		GtkTextMark* insert;
		GtkTextIter iter;
		insert = gtk_text_buffer_get_insert( doc->buffer );
		gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, insert );
		cursor_offset = gtk_text_iter_get_offset( &iter );
	}

	/* This opens the contents of a file to a textbox */
	{
		gchar *encoding = NULL;
		gchar *newbuf = NULL;
		gsize wsize;
		gchar *buffer = get_buffer_from_filename( BFWIN( doc->bfwin ), filename, &document_size );
		if ( !buffer )
		{
			DEBUG_MSG( "doc_file_to_textbox, buffer==NULL, returning\n" );
			return FALSE;
		}
		if ( !newbuf )
		{
			DEBUG_MSG( "doc_file_to_textbox, trying newfile default encoding %s\n", main_v->props.newfile_default_encoding );
			newbuf = g_convert( buffer, -1, "UTF-8", main_v->props.newfile_default_encoding, NULL, &wsize, NULL );
			if ( newbuf ) {
				DEBUG_MSG( "doc_file_to_textbox, file is in default encoding: %s\n", main_v->props.newfile_default_encoding );
				encoding = g_strdup( main_v->props.newfile_default_encoding );
			}
		}
		if ( !newbuf )
		{
			DEBUG_MSG( "doc_file_to_textbox, file is not in UTF-8, trying encoding from locale\n" );
			newbuf = g_locale_to_utf8( buffer, -1, NULL, &wsize, NULL );
			if ( newbuf ) {
				const gchar * tmpencoding = NULL;
				g_get_charset( &tmpencoding );
				DEBUG_MSG( "doc_file_to_textbox, file is in locale encoding: %s\n", tmpencoding );
				encoding = g_strdup( tmpencoding );
			}
		}
		if ( !newbuf )
		{
			DEBUG_MSG( "doc_file_to_textbox, file NOT is converted yet, trying UTF-8 encoding\n" );
			if ( g_utf8_validate( buffer, -1, NULL ) ) {
				encoding = g_strdup( "UTF-8" );
			}
		}
		if ( !newbuf )
		{
			GList * tmplist;
			DEBUG_MSG( "doc_file_to_textbox, tried the most obvious encodings, nothing found.. go trough list\n" );
			tmplist = g_list_first( main_v->props.encodings );
			while ( tmplist ) {
				gchar **enc = tmplist->data;
				DEBUG_MSG( "doc_file_to_textbox, trying encoding %s\n", enc[ 1 ] );
				newbuf = g_convert( buffer, -1, "UTF-8", enc[ 1 ], NULL, &wsize, NULL );
				if ( newbuf ) {
					encoding = g_strdup( enc[ 1 ] );
					tmplist = NULL;
				} else {
					DEBUG_MSG( "doc_file_to_textbox, no newbuf, next in list\n" );
					tmplist = g_list_next( tmplist );
				}
			}
		}
		if ( !newbuf )
		{
			error_dialog( BFWIN( doc->bfwin ) ->main_window, _( "Cannot display file, unknown characters found." ), NULL );
		} else
		{
			g_free( buffer );
			buffer = newbuf;
			if ( doc->encoding )
				g_free( doc->encoding );
			doc->encoding = encoding;
			add_encoding_to_list( encoding );
		}
		if ( buffer )
		{
			gtk_text_buffer_insert_at_cursor( doc->buffer, buffer, -1 );
			g_free( buffer );
		}
	}
	if ( doc->view_bars & VIEW_COLORIZED ) {
		doc->need_highlighting = TRUE;
		DEBUG_MSG( "doc_file_to_textbox, highlightstate=%d, need_highlighting=%d, delay=%d\n", doc->view_bars & VIEW_COLORIZED, doc->need_highlighting, delay );
		if ( !delay ) {
#ifdef DEBUG
		g_print( "doc_file_to_textbox, doc->hlset=%p\n", doc->hl );
		if ( doc->hl ) {
			g_print( "doc_file_to_textbox, doc->hlset->highlightlist=%p\n", doc->hl->highlightlist );
		}
#endif
			doc_highlight_full( doc );
		}
	}
	if ( !enable_undo ) {
		
		doc_bind_signals( doc );
	}

	{
		/* set the cursor position back */
		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_offset( doc->buffer, &iter, cursor_offset );
		gtk_text_buffer_place_cursor( doc->buffer, &iter );
		if ( !delay ) {
			gtk_text_view_place_cursor_onscreen( GTK_TEXT_VIEW( doc->view ) );
		}
		/*		gtk_notebook_set_page(GTK_NOTEBOOK(main_v->notebook),g_list_length(main_v->documentlist) - 1);
				notebook_changed(-1);*/
	}
	return TRUE;
}

/**
* doc_check_backup:
* @doc: #Tdocument*
*
* creates a backup, depending on the configuration
* returns 1 on success, 0 on failure
* if no backup is required, or no filename known, 1 is returned
*
* Return value: #gint 1 on success or 0 on failure
*/
static gint doc_check_backup( Tdocument *doc )
{
	gint res = 1;

	if ( (main_v->props.view_bars & MODE_CREATE_BACKUP_ON_SAVE) && strlen(main_v->props.backup_filestring) && doc->filename && file_exists_and_readable( doc->filename ) ) {
		gchar * backupfilename, *ondiskencoding;
		backupfilename = g_strconcat( doc->filename, main_v->props.backup_filestring, NULL );
		ondiskencoding = get_filename_on_disk_encoding( backupfilename );
		res = file_copy( doc->filename, backupfilename );
		if ( doc->statbuf.st_uid != -1 && !doc->is_symlink ) {
			chmod( ondiskencoding, doc->statbuf.st_mode );
			chown( ondiskencoding, doc->statbuf.st_uid, doc->statbuf.st_gid );
		}
		g_free( ondiskencoding );
		g_free( backupfilename );
	}
	return res;
}

static void doc_buffer_insert_text_lcb( GtkTextBuffer *textbuffer, GtkTextIter * iter, gchar * string, gint len, Tdocument * doc )
{
	gint pos = gtk_text_iter_get_offset( iter );
	gint clen = g_utf8_strlen( string, len );
	DEBUG_MSG( "doc_buffer_insert_text_lcb, started, string='%s', len=%d, clen=%d\n", string, len, clen );
	/* the 'len' seems to be the number of bytes and not the number of characters.. */

	if ( doc->paste_operation ) {
		if ( ( pos + clen ) > PASTEOPERATION( doc->paste_operation ) ->eo )
			PASTEOPERATION( doc->paste_operation ) ->eo = pos + clen;
		if ( pos < PASTEOPERATION( doc->paste_operation ) ->so || PASTEOPERATION( doc->paste_operation ) ->so == -1 )
			PASTEOPERATION( doc->paste_operation ) ->so = pos;
	} else if ( len == 1 ) {
		/* undo_redo stuff */
		if ( !doc_unre_test_last_entry( doc, UndoInsert, -1, pos )
			|| string[ 0 ] == ' '
			|| string[ 0 ] == '\n'
			|| string[ 0 ] == '\t'
			|| string[ 0 ] == '\r' ) {
			DEBUG_MSG( "doc_buffer_insert_text_lcb, need a new undogroup\n" );
			doc_unre_new_group( doc );
		}
	}
	/* we do not call doc_unre_new_group() for multi character inserts, these are from paste, and edit_paste_cb groups them already */
	/*  else if (clen != 1) {
		doc_unre_new_group(doc);
	} */

	doc_unre_add( doc, string, pos, pos + clen, UndoInsert );
	doc_set_modified( doc, 1 );
	DEBUG_MSG( "doc_buffer_insert_text_lcb, done\n" );
}
static gboolean find_char( gunichar ch, gchar *data )
{
#ifdef DEBUG
if ( ch < 127 ) {
	DEBUG_MSG( "find_char, looking at character %c, searching for '%s', returning %d\n", ch, data, ( strchr( data, ch ) != NULL ) );
} else {
	DEBUG_MSG( "find_char, looking at character code %d, searching for '%s', returning %d\n", ch, data, ( strchr( data, ch ) != NULL ) );
}
#endif
	return ( strchr( data, ch ) != NULL );
}

static void doc_buffer_insert_text_after_lcb( GtkTextBuffer *textbuffer, GtkTextIter * iter, gchar * string, gint len, Tdocument * doc )
{
	DEBUG_MSG( "doc_buffer_insert_text_after_lcb, started for string '%s'\n", string );
	if ( !doc->paste_operation ) {
		/* highlighting stuff */
		if ( (doc->view_bars & VIEW_COLORIZED) && string && doc->hl ) {
			gboolean do_highlighting = FALSE;
			if ( doc->hl->update_chars[ 0 ] == '\0' ) {
				do_highlighting = TRUE;
			} else {
				gint i = 0;
				while ( string[ i ] != '\0' ) {
					if ( strchr( doc->hl->update_chars, string[ i ] ) ) {
						do_highlighting = TRUE;
						break;
					}
					i++;
				}
			}
			if ( do_highlighting ) {
				doc_highlight_line( doc );
			}
		}
	}
#ifdef DEBUG
else {
	DEBUG_MSG( "doc_buffer_insert_text_after_lcb, paste_operation, NOT DOING ANYTHING\n" );
}
#endif
}

static void completion_popup_menu_init() {
#ifdef SHOW_SNOOPER
	g_print("completion_popup_menu: call init function\n");
#endif
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

/**
 * kyanh <kyanh@o2.pl>
 * add user command to autocompletion list.
 * this causes the list be sorted again ==> slower performance
 */

static void gap_command( GtkWidget *widget, GdkEventKey *kevent, Tdocument *doc ) {

	guint32 character = gdk_keyval_to_unicode( kevent->keyval );
	DEBUG_MSG( "gap_command: keyval=%d (or %X), character=%d, string=%s, state=%d, hw_keycode=%d\n", kevent->keyval, kevent->keyval, character, kevent->string, kevent->state, kevent->hardware_keycode );

	/* SHIFT + DELIMITER ==> 2 keys release event and we move remove one */
	if ( (kevent->keyval != GDK_Return) && (character==0 || !strstr(DELIMITERS, kevent->string)) ) {
		return;
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
			/*if (ret <=0) {
				ret = pcre_exec( main_v->autoclosingtag_regc, NULL, buf, strlen( buf ), 0, PCRE_ANCHORED, ovector, 12 );
			}*/
			if ( ret > 0 ) {
				/* buf is now the user command */
				DEBUG_MSG("gap_command: found command = [%s]\n", buf);
				GList *tmplist = NULL;
				tmplist = g_list_find_custom(main_v->props.completion->items, buf, (GCompareFunc)strcmp);
				/* g_completion_complete(main_v->props.completion, buf, NULL); */
				if ( !tmplist ) {
					tmplist = g_list_find_custom(main_v->props.completion_s->items, buf, (GCompareFunc)strcmp);
					/* g_completion_complete(main_v->props.completion_s, buf, NULL); */
					if (!tmplist) {
						DEBUG_MSG("gap_command: new command = [%s]\n", buf);
						gchar *tmpstr = g_strdup(buf);
						tmplist = g_list_append(tmplist, tmpstr);
						/* Cannot use: g_list_append(tmplist, buf); as buf is freed. See below. */
						g_completion_add_items(main_v->props.completion_s, tmplist);
						DEBUG_MSG("gap_command: completion_s data = %s\n", (gchar *)main_v->props.completion_s->items->data);
						g_list_free(tmplist);
					}
				}
			}
		}
	/*}*/
	g_free(buf);
}

static gboolean completion_popup_menu( GtkWidget *widget, GdkEventKey *kevent, Tdocument *doc ) {

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
#ifdef SHOW_SNOOPER
		g_print("completion: found command '%s'\n", buf);
#endif /* SHOW_SNOOPER */
	
		if (!buf || ( (main_v->completion.show != COMPLETION_FIRST_CALL) && (strlen(buf) < 3)) ) { /* we require \xy */
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
	/*
	gdouble tx, ty;
	if (gdk_event_get_coords((GdkEvent *)kevent, &tx, &ty)) {
		g_print("old algorithm: (%d, %d); gdk_get_coords: (%f, %f)\n", x, y, tx, ty);
	}
	*/

	gtk_window_move (GTK_WINDOW(main_v->completion.window), x+16, y);
	{/* select the first item; need*not* for the first time */
		GtkTreePath *treepath = gtk_tree_path_new_from_string("0");
		if (treepath) {
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(main_v->completion.treeview), treepath, NULL, FALSE);
			gtk_tree_path_free(treepath);
		}
	}
	
	gtk_widget_show_all(main_v->completion.window);
#ifdef SHOW_SNOOPER
	g_print("completion_popup_menu: window has been created\n");
#endif
	return TRUE;
}

static gboolean doc_view_key_press_lcb( GtkWidget *widget, GdkEventKey *kevent, Tdocument *doc ) {

#ifdef SHOW_SNOOPER
	g_print("doc: got key pressed\n");
#endif
	if ( ! ( (kevent->state & GDK_CONTROL_MASK) && ( (kevent->keyval == GDK_bracketleft) || (kevent->keyval == GDK_bracketright) ) ) ) {
		brace_finder(doc->buffer, doc->brace_finder, 0, -1);
	}

	if (!(doc->view_bars & MODE_AUTO_COMPLETE)) {
		return FALSE;
	}

	if (main_v->completion.show == COMPLETION_DELETE ) {
		DEBUG_MSG("doc: delete item from popup\n");
		/* delete stuff:
		- get current position/command
		- hide the popup window
		- delete the selected item. this alters `main_v->props.completion(_s)->items' (GCompletion)
		- rebuild the popup
		- recaculate: completion_popup_menu(widget, kevent, doc)
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
				the session list (by `gap_command', the list is unordered. so we have to
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
				statusbar_message(doc->bfwin, tmpstr, 2000);
				g_free(tmpstr);
			}
			g_free(user_selection);
			
			main_v->completion.show = COMPLETION_AUTO_CALL;
		}else{
			main_v->completion.show = COMPLETION_WINDOW_HIDE;
		}
		return TRUE;
	}
	if (main_v->completion.show == COMPLETION_WINDOW_UP || main_v->completion.show == COMPLETION_WINDOW_DOWN || main_v->completion.show == COMPLETION_WINDOW_PAGE_DOWN || main_v->completion.show == COMPLETION_WINDOW_PAGE_UP){
		GtkTreeModel *model;
		gint maxnode, index;

		model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_v->completion.treeview));
		maxnode = gtk_tree_model_iter_n_children(model, NULL);

		if (maxnode ==1) {
			gtk_widget_hide( GTK_WIDGET( main_v->completion.window ));
			main_v->completion.show = COMPLETION_WINDOW_HIDE;
			return TRUE;
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
		return TRUE;
	} else if ( main_v->completion.show == COMPLETION_FIRST_CALL ) {
		if (!completion_popup_menu(widget, kevent, doc)) {
#ifdef SHOW_SNOOPER
			g_print("doc: completion returns NULL. popup willnot be shown\n");
#endif
			main_v->completion.show = COMPLETION_WINDOW_HIDE;
		}else{
			main_v->completion.show = COMPLETION_WINDOW_SHOW;
		}
	} else if (main_v->completion.show == COMPLETION_WINDOW_HIDE) {
#ifdef SHOW_SNOOPER
		g_print("...hide popup window\n");
#endif
		gtk_widget_hide_all( main_v->completion.window );
	}
	/* moved to snooper.c :: completion_snooper()
	main_v->lastkp_keyval = kevent->keyval;
	main_v->lastkp_hardware_keycode = kevent->hardware_keycode;
	*/
	return FALSE; /* we didn't handle all of the event */
}

static gboolean doc_view_key_release_lcb( GtkWidget *widget, GdkEventKey *kevent, Tdocument *doc )
{
#ifdef SHOW_SNOOPER
	g_print("doc: got key released\n");
#endif
/* never reach: if ( (kevent->keyval == GDK_space) && (kevent->state & GDK_CONTROL_MASK ))*/
	/* complete the word */
	if (doc->view_bars & MODE_AUTO_COMPLETE) {
		if (main_v->completion.show == COMPLETION_WINDOW_ACCEPT) {
#ifdef SHOW_SNOOPER
			g_print("...autotcompletion accepted\n");
#endif
			gtk_widget_hide_all( main_v->completion.window );
			main_v->completion.show = COMPLETION_WINDOW_HIDE;
	
			if ( main_v->completion.bfwin != BFWIN(doc->bfwin)->current_document ) {
#ifdef SHOW_SNOOPER
				/* TODO: hide the popup when window changed */
				g_print("completion: accepted, but window changed. Ingore it.\n");
#endif
				return TRUE;
			}
	
			/* inserting staff */
#ifdef SHOW_SNOOPER
			g_print("completion: cached ='%s'\n", main_v->completion.cache);
#endif
			{
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
#ifdef HAVE_UNIKEY_GTK
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
#endif
					val = g_new0(GValue, 1);
					gtk_tree_model_get_value(model, &iter, 0, val);
					user_selection = g_strdup((gchar *) (g_value_peek_pointer(val)));
					g_value_unset (val);
					g_free (val);
#ifdef SHOW_SNOOPER
					g_print("completion: user selected '%s'\n", user_selection);
#endif
					/*
					inserting
					*/
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
#ifdef SHOW_SNOOPER
						g_print("completion: will add '%s' (%d chars)\n", retval, len);
#endif
						GtkTextIter iter;
						GtkTextMark *imark;
						imark = gtk_text_buffer_get_insert( doc->buffer );
						gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, imark );
						gtk_text_buffer_insert( doc->buffer, &iter, retval, -1 );
						g_free(retval);
					}
					g_free(user_selection);
				}
			}
			return TRUE;
		} else if ( main_v->completion.show == COMPLETION_WINDOW_SHOW || main_v->completion.show == COMPLETION_AUTO_CALL ) {
			if (!completion_popup_menu(widget, kevent, doc)) {
#ifdef SHOW_SNOOPER
				g_print("doc: completion returns NULL. popup willnot be shown\n");
#endif
				gtk_widget_hide_all( main_v->completion.window );
				main_v->completion.show = COMPLETION_WINDOW_HIDE;
			} else {
				main_v->completion.show = COMPLETION_WINDOW_SHOW;
			}
		}
	}
	gap_command( widget, kevent, doc);
	/* shift> = ]*/
	/* if the shift key is released before the '>' key, we get a key release not for '>' but for '.'. We, therefore have set that in the key_press event, and check if the same hardware keycode was released */
	/* complete environment */
	if ( ! ( (kevent->state & GDK_CONTROL_MASK) && ( (kevent->keyval == GDK_bracketleft) || (kevent->keyval == GDK_bracketright) ) ) ) {
		brace_finder(doc->buffer,doc->brace_finder,BR_AUTO_FIND, BRACE_FINDER_MAX_LINES);
	}

	if ( ( kevent->keyval == GDK_braceright ) || (main_v->last_kevent && ( kevent->hardware_keycode == ((GdkEventKey *)main_v->last_kevent)->hardware_keycode && ((GdkEventKey *)main_v->last_kevent)->keyval == GDK_braceright )) ) {
		/* autoclose environment for LaTeX */
		if ( doc->view_bars & MODE_AUTO_COMPLETE ) {
			GtkTextMark * imark;
			GtkTextIter itstart, iter, maxsearch;

			imark = gtk_text_buffer_get_insert( doc->buffer );
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, imark );
			itstart = iter;
			maxsearch = iter;
			DEBUG_MSG( "doc_view_key_release_lcb, autocomplete, started at %d\n", gtk_text_iter_get_offset( &itstart ) );
			gtk_text_iter_backward_chars( &maxsearch, 30 ); /* \begin{flushleft} */
			if ( gtk_text_iter_backward_find_char( &itstart, ( GtkTextCharPredicate ) find_char, GINT_TO_POINTER( "\\" ), &maxsearch ) ) {
				/* we use a regular expression to check if the tag is valid, AND to parse the tagname from the string */
				gchar * buf;
				int ovector[ 12 ], ret;
				DEBUG_MSG( "doc_view_key_release_lcb, we found a '<'\n" );
				maxsearch = iter; /* re-use maxsearch */
				buf = gtk_text_buffer_get_text( doc->buffer, &itstart, &maxsearch, FALSE );
				DEBUG_MSG( "doc_view_key_release_lcb, buf='%s'\n", buf );
				ret = pcre_exec( main_v->autoclosingtag_regc, NULL, buf, strlen( buf ), 0, PCRE_ANCHORED, ovector, 12 );
				if ( ret > 0 ) {
					gchar * tagname, *toinsert, *indent = NULL;
					DEBUG_MSG( "doc_view_key_release_lcb, autoclosing, we have a tag, ret=%d, starts at ovector[2]=%d, ovector[3]=%d\n", ret, ovector[ 2 ], ovector[ 3 ] );
					tagname = g_strndup( &buf[ ovector[ 2 ] ], ovector[ 3 ] - ovector[ 2 ] );
					DEBUG_MSG( "doc_view_key_release_lcb, autoclosing, tagname='%s'\n", tagname );
					/* add to the completion lisst */
					/* TODO: move this to gap_command */
					{
						gchar *tmpstr = g_strconcat("\\begin{",tagname,NULL);
						GList *search = NULL;
						
						search = g_list_find_custom(main_v->props.completion->items,tmpstr, (GCompareFunc)strcmp);
						if (!search) {
							search = g_list_find_custom(main_v->props.completion_s->items,tmpstr, (GCompareFunc)strcmp);
							if (!search) {
								DEBUG_MSG("doc: new environment captured: %s\n", tmpstr);
								GList *tmplist = NULL;
								tmplist = g_list_append(tmplist, tmpstr);
								g_completion_add_items(main_v->props.completion_s,tmplist);
								g_list_free(tmplist);
							}
						}
						/* search cannot be freed */
					}
					/* count the autoindent */
					if (main_v->props.view_bars & MODE_AUTO_INDENT) {
						itstart = iter;
						gtk_text_iter_set_line_index(&itstart, 0);
						indent = gtk_text_buffer_get_text(doc->buffer, &itstart, &iter, FALSE);
						if (indent) {
							gchar *indenting = indent;
							while (*indenting == '\t' || *indenting == ' ' ) {
								indenting++;
							}
							*indenting = '\0';
							if (strlen( indent )) {
								toinsert = g_strconcat( "\n", indent, "\n", indent, "\\end{", tagname, "}", NULL);
							}else{
								toinsert = g_strconcat( "\n\n\\end{", tagname, "}", NULL );
							}
						}else{
							toinsert = g_strconcat( "\n\n\\end{", tagname, "}", NULL );
						}
					}else{
						toinsert = g_strconcat( "\n\n\\end{", tagname, "}", NULL );
					}
					if ( toinsert ) {
						/* we re-use the maxsearch iter now */
						gtk_text_buffer_insert( doc->buffer, &maxsearch, toinsert, -1 );
						/* now we set the cursor back to its previous location, re-using itstart */
						gtk_text_buffer_get_iter_at_mark( doc->buffer, &itstart, gtk_text_buffer_get_insert( doc->buffer ) );
						gint tmpint = strlen( tagname ) + 7; /* NOTE: utf8 tagname ==> use g_utf8_strlen */
						if (indent) {
							gtk_text_iter_backward_chars( &itstart, tmpint + strlen(indent));
							g_free(indent);
						}else{
							gtk_text_iter_backward_chars( &itstart, tmpint);
						}
						/* gint startoffset = gtk_text_iter_get_offset(&itstart); */
						gtk_text_buffer_place_cursor( doc->buffer, &itstart );
						/* doc_unre_add(doc, toinsert, startoffset, startoffset+tmpint, UndoInsert); */
						g_free( toinsert );
					}
#ifdef DEBUG
					else {
						DEBUG_MSG( "doc_view_key_release_lcb, no match!! '%s' is not a valid tag\n", buf );
					}
#endif
					g_free( tagname );
				}
#ifdef DEBUG
				else {
					DEBUG_MSG( "doc_view_key_release_lcb, ret=%d\n", ret );
					DEBUG_MSG( "document: key_release_lcb: [%s]\n", buf );
				}
#endif
				/* cleanup and return */
				g_free( buf );
			}
#ifdef DEBUG
			else {
				DEBUG_MSG( "doc_view_key_release_lcb, did not find a '\\' character\n" );
			}
#endif
		}
	/* autoindent */
	} else if ( ( kevent->keyval == GDK_Return || kevent->keyval == GDK_KP_Enter ) && !( kevent->state & GDK_SHIFT_MASK || kevent->state & GDK_CONTROL_MASK || kevent->state & GDK_MOD1_MASK ) ) {
		/* find the \startFOO. TODO: \bFOO */
		gchar *tagname = NULL;
		gchar *endtag = NULL;
		if ( doc->view_bars & MODE_AUTO_COMPLETE ) {
			GtkTextMark * imark;
			GtkTextIter itstart, iter, maxsearch;

			imark = gtk_text_buffer_get_insert( doc->buffer );
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, imark );
			itstart = iter;
			maxsearch = iter;
			DEBUG_MSG( "doc_view_key_release_lcb, autocomplete, started at %d\n", gtk_text_iter_get_offset( &itstart ) );
			gtk_text_iter_backward_chars( &maxsearch, 30 ); /* \begin{flushleft} */
			if ( gtk_text_iter_backward_find_char( &itstart, ( GtkTextCharPredicate ) find_char, GINT_TO_POINTER( "\\" ), &maxsearch ) ) {
				/* we use a regular expression to check if the tag is valid, AND to parse the tagname from the string */
				gchar * buf;
				int ovector[ 9 ], ret;
				DEBUG_MSG( "doc_view_key_release_lcb, we found a '<'\n" );
				maxsearch = iter; /* re-use maxsearch */
				buf = gtk_text_buffer_get_text( doc->buffer, &itstart, &maxsearch, FALSE );
				DEBUG_MSG( "doc_view_key_release_lcb, buf='%s'\n", buf );
				ret = pcre_exec( main_v->autoclosingtag_be_regc, NULL, buf, strlen( buf ), 0, PCRE_ANCHORED, ovector, 9 );
				if ( ret > 0 ) {
					tagname = g_strndup( &buf[ ovector[ 3 ] ], ovector[ 1 ] - ovector[ 3 ] );
					endtag = g_strndup( &buf[ ovector[ 2 ] ], ovector[ 3 ] - ovector[ 2 ] );
				}
				g_free( buf );
			}
		}
		GtkTextIter itend;
		gchar *string = NULL; /* badname */
		if ( main_v->props.view_bars & MODE_AUTO_INDENT ) {
			gchar *indenting;
			GtkTextMark* imark;
			GtkTextIter itstart;
			imark = gtk_text_buffer_get_insert( doc->buffer );
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &itend, imark );
			itstart = itend;
			/* set to the beginning of the previous line */
			gtk_text_iter_backward_line( &itstart );
			gtk_text_iter_set_line_index( &itstart, 0 );
			string = gtk_text_buffer_get_text( doc->buffer, &itstart, &itend, FALSE );
			if ( string ) {
				/* now count the indenting in this string */
				indenting = string;
				while ( *indenting == '\t' || *indenting == ' ' ) {
					indenting++;
				}
				/* ending search, non-whitespace found, so terminate at this position */
				*indenting = '\0';
				if ( strlen( string ) ) {
					DEBUG_MSG( "doc_buffer_insert_text_lcb, inserting indenting\n" );
					gtk_text_buffer_insert( doc->buffer, &itend, string, -1 );
				}
			}
		}
		/* insert \endFOO */
		if ( tagname ) { /* so endtag != NULL ; */
			gint tmpint=0;
			gchar *toinsert;
#ifdef HAVE_CONTEXT
			if (strncmp(endtag,"begin",5) ==0 ) {
				endtag = g_strdup("\\end");
				tmpint += 5; /* 5 = strlen (endtag) +1 */
			}else{
				endtag = g_strdup("\\stop");
				tmpint += 6;
			}
#else
			endtag = g_strdup("\\end");
			tmpint += 5;
#endif
			if ( string && (tmpint += strlen(string)) ) {
				toinsert = g_strconcat("\n", string, endtag, tagname, NULL);
			}else{
				toinsert = g_strconcat("\n", endtag, tagname, NULL);
			}
			g_free(endtag);
			g_free( string );
			gtk_text_buffer_insert( doc->buffer, &itend, toinsert, -1 );
			g_free(toinsert);
			tmpint += strlen(tagname);
			gtk_text_iter_backward_chars( &itend, tmpint);
			gtk_text_buffer_place_cursor( doc->buffer, &itend );
			g_free(tagname);
		}
	/* autobrace. TODO: confused with \begin{asdf} */
/*
	}else if ( ( kevent->keyval == GDK_braceleft ) || ( kevent->hardware_keycode == main_v->lastkp_hardware_keycode && main_v->lastkp_keyval == GDK_braceleft ) ) {
		GtkTextMark * imark;
		GtkTextIter iter;
		imark = gtk_text_buffer_get_insert( doc->buffer );
		gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, imark );
		gtk_text_buffer_insert( doc->buffer, &iter, "}", -1 );
		gtk_text_iter_backward_chars( &iter, 1);
		gtk_text_buffer_place_cursor( doc->buffer, &iter );
*/
	/* autotext */
	} else if ( ( kevent->state & GDK_SHIFT_MASK ) && !( kevent->state & GDK_CONTROL_MASK ) && ( kevent->keyval == GDK_space ) ) {
		/* kyanh, added, 20050302 */
		/* User must press SHIFT + SPACE to start autotext */
		GtkTextMark * imark;
		GtkTextIter itstart, iter, maxsearch;

		imark = gtk_text_buffer_get_insert( doc->buffer );
		gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, imark );
		itstart = iter;
		maxsearch = iter;
		gtk_text_iter_backward_chars( &maxsearch, AUTOTEXT_MAX_LENGTH );
		if ( gtk_text_iter_backward_find_char( &itstart, ( GtkTextCharPredicate ) find_char, GINT_TO_POINTER( "/" ), &maxsearch ) ) {
			gchar * buf;
			maxsearch = iter; /* re-use maxsearch */
			gtk_text_iter_backward_chars( &maxsearch, 1 ); /* skip the space */
			buf = gtk_text_buffer_get_text( doc->buffer, &itstart, &maxsearch, FALSE );
			if ( strlen( buf ) > 2 ) { /* NOTE: why >2 ? i forgot the reason :) */
				gchar **tmp = autotext_done( buf );
				if ( tmp ) {
					gtk_text_iter_forward_chars( &maxsearch, 1 ); /* kill the space */
					gtk_text_buffer_delete( doc->buffer, &maxsearch, &itstart );
					gtk_text_iter_set_line_index(&maxsearch, 0);
					gchar *indent=NULL;
					if (main_v->props.view_bars & MODE_AUTO_INDENT ) {
						indent = gtk_text_buffer_get_text( doc->buffer, &maxsearch, &itstart, FALSE);
						if (indent) {
							gchar *indenting = indent;
							while ( *indenting == '\t' || *indenting == ' ' ) {
								indenting++;
							}
							*indenting = '\0';
						}
					}
					gchar **output=NULL, **tmpchar;
					gchar *result=NULL;
					gint count;
					if ( strlen( tmp[ 0 ] ) ) {
						if (indent) {
							output = g_strsplit(tmp[0],"\n",0);
							count=0;
							tmpchar = output;
							while (*tmpchar != NULL ) {
								if (count) {
									result = g_strconcat(result, "\n", indent, output[count], NULL);
								}else{
									result = g_strdup(output[0]);
								}
								count++;
								tmpchar++;
							}
							gtk_text_buffer_insert( doc->buffer, &itstart, result, -1 );
						}else{
							gtk_text_buffer_insert( doc->buffer, &itstart, tmp[ 0 ], -1 );
						}
					}
					if ( strlen( tmp[ 1 ] ) ) {
						if (indent) {
							output = g_strsplit(tmp[1], "\n",0);
							count = 0;
							tmpchar = output;
							while (*tmpchar != NULL) {
								if (count) {
									result = g_strconcat(result, "\n", indent, output[count], NULL);
								}else{
									result = g_strdup(output[0]);
								}
								count++;
								tmpchar++;
							}
						}else{
							result = g_strdup(tmp[1]);
						}
						gtk_text_buffer_insert( doc->buffer, &itstart, result, -1 );
						gtk_text_iter_backward_chars( &itstart, g_utf8_strlen( result, -1 ) );
						gtk_text_buffer_place_cursor( doc->buffer, &itstart );
						gtk_widget_grab_focus( doc->view );
					}
					g_free(indent);
					g_free(result);
					g_strfreev(output);
					/* donot free tmp :) */
				}
			}
			g_free( buf );
		}
	}
	/* TODO: */
	/*
	{
		GtkTextIter cursor_iter, tmp_iter;
		GtkTextMark *imark;
		gchar *buf;
		imark = gtk_text_buffer_get_insert( doc->buffer );
		gtk_text_buffer_get_iter_at_mark( doc->buffer, &cursor_iter, imark );
		tmp_iter = cursor_iter;
		gtk_text_iter_backward_chars(&tmp_iter, 1);
		buf = gtk_text_buffer_get_text(doc->buffer, &tmp_iter, &cursor_iter, FALSE);
		if (buf && (strcmp(buf,"}")==0)) {
			/ * searching backward for { * /
			g_print("find }\n");
		}
	}
	*/
	return FALSE; /* we didn't handle all of the event */
}

static void doc_buffer_delete_range_lcb( GtkTextBuffer *textbuffer, GtkTextIter * itstart, GtkTextIter * itend, Tdocument * doc )
{
	gchar * string;
	gboolean do_highlighting = FALSE;
	string = gtk_text_buffer_get_text( doc->buffer, itstart, itend, FALSE );
	/*DEBUG_MSG( "doc_buffer_delete_range_lcb, string='%s'\n", string );*/
	if ( string ) {
		/* highlighting stuff */
		if ( (doc->view_bars & VIEW_COLORIZED) && string && doc->hl ) {
			if ( strlen( doc->hl->update_chars ) == 0 ) {
				do_highlighting = TRUE;
			} else {
				gint i = 0;
				while ( string[ i ] != '\0' ) {
					if ( strchr( doc->hl->update_chars, string[ i ] ) ) {
						do_highlighting = TRUE;
						break;
					}
					i++;
				}
			}
			if ( do_highlighting ) {
				doc_highlight_line( doc );
			}
		}
		/* undo_redo stuff */
		{
			gint start, end, len;
			start = gtk_text_iter_get_offset( itstart );
			end = gtk_text_iter_get_offset( itend );
			len = end - start;
			/* DEBUG_MSG( "doc_buffer_delete_range_lcb, start=%d, end=%d, len=%d, string='%s'\n", start, end, len, string  ); */
			if ( len == 1 )
			{
				if ( ( !doc_unre_test_last_entry( doc, UndoDelete, start, -1 )  /* delete */
					&& !doc_unre_test_last_entry( doc, UndoDelete, end, -1 ) )  /* backspace */
					|| string[ 0 ] == ' '
					|| string[ 0 ] == '\n'
					|| string[ 0 ] == '\t'
					|| string[ 0 ] == '\r' ) {
					/* DEBUG_MSG( "doc_buffer_delete_range_lcb, need a new undogroup\n" ); */
					doc_unre_new_group( doc );
				}
			} else
			{
				doc_unre_new_group( doc );
			}
			doc_unre_add( doc, string, start, end, UndoDelete );
		}
		g_free( string );
	}
	doc_set_modified( doc, 1 );
}

static gboolean doc_view_button_release_lcb( GtkWidget *widget, GdkEventButton *bevent, Tdocument *doc )
{
	DEBUG_MSG( "doc_view_button_release_lcb, button %d\n", bevent->button );
	if (bevent->button == 1) {
		brace_finder(doc->buffer, doc->brace_finder, BR_AUTO_FIND, BRACE_FINDER_MAX_LINES);
	}
	if ( bevent->button == 2 ) {
		/* end of paste */
		if ( doc->paste_operation ) {
			if ( PASTEOPERATION( doc->paste_operation ) ->eo > PASTEOPERATION( doc->paste_operation ) ->so ) {
				DEBUG_MSG( "doc_view_button_release_lcb, start doc-highlight_region for so=%d, eo=%d\n", PASTEOPERATION( doc->paste_operation ) ->so, PASTEOPERATION( doc->paste_operation ) ->eo );
				doc_highlight_region( doc, PASTEOPERATION( doc->paste_operation ) ->so, PASTEOPERATION( doc->paste_operation ) ->eo );
			}
			g_free( doc->paste_operation );
			doc->paste_operation = NULL;
		}
		/* now we should update the highlighting for the pasted text, but how long is the pasted text ?? */
	}
	/*	if (bevent->button == 3) {
			GtkWidget *menuitem;
			GtkWidget *submenu;
			GtkWidget *menu = gtk_menu_new ();
			gboolean tag_found;
			tag_found = doc_bevent_in_html_tag(doc, bevent);
			menuitem = gtk_menu_item_new_with_label(_("Edit tag"));
			g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(rpopup_edit_tag_cb), doc);
			if (!tag_found) {
				gtk_widget_set_sensitive(menuitem, FALSE);
			}
	gtk_widget_show (menuitem);
			gtk_menu_append(GTK_MENU(menu), menuitem);
			menuitem = gtk_menu_item_new();
	gtk_widget_show (menuitem);
			gtk_menu_append(GTK_MENU(menu), menuitem);
	
			menuitem = gtk_menu_item_new_with_mnemonic (_("Input _Methods"));
	gtk_widget_show (menuitem);
			submenu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	gtk_im_multicontext_append_menuitems (GTK_IM_MULTICONTEXT (GTK_TEXT_VIEW(doc->view)->im_context),
						GTK_MENU_SHELL (submenu));
			gtk_widget_show_all (menu);
			gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
				NULL, doc->view, 0, gtk_get_current_event_time ());
		} */ 
	return FALSE;
}

static void doc_get_iter_at_bevent( Tdocument *doc, GdkEventButton *bevent, GtkTextIter *iter )
{
	gint xpos, ypos;
	GtkTextWindowType wintype;

	wintype = gtk_text_view_get_window_type( GTK_TEXT_VIEW( doc->view ), doc->view->window );
	gtk_text_view_window_to_buffer_coords( GTK_TEXT_VIEW( doc->view ), wintype, bevent->x, bevent->y,
					&xpos, &ypos );
	xpos += gtk_text_view_get_border_window_size( GTK_TEXT_VIEW( doc->view ), GTK_TEXT_WINDOW_LEFT );
	gtk_text_view_get_iter_at_location( GTK_TEXT_VIEW( doc->view ), iter, xpos, ypos );
}

static gboolean doc_view_button_press_lcb( GtkWidget *widget, GdkEventButton *bevent, Tdocument *doc )
{
	DEBUG_MSG( "doc_view_button_press_lcb, button %d\n", bevent->button );
	if (bevent->button==1) {
		brace_finder(doc->buffer, doc->brace_finder, 0, -1);
	}
	if ( bevent->button == 2 && !doc->paste_operation ) {
		doc->paste_operation = g_new( Tpasteoperation, 1 );
		PASTEOPERATION( doc->paste_operation ) ->so = -1;
		PASTEOPERATION( doc->paste_operation ) ->eo = -1;
	}
	if ( bevent->button == 3 ) {
		GtkTextIter iter;
		doc_get_iter_at_bevent( doc, bevent, &iter );
		/* kyanh, removed, 20050219, rpopup_bevent_in_html_code(doc, &iter); */
		bmark_store_bevent_location( doc, gtk_text_iter_get_offset( &iter ) );
	}
	return FALSE;
}

static void rpopup_add_bookmark_lcb( GtkWidget *widget, Tdocument *doc )
{
	bmark_add_at_bevent( doc );
}
static void rpopup_del_bookmark_lcb( GtkWidget *widget, Tdocument *doc )
{
	bmark_del_at_bevent( doc );
}


static void doc_view_populate_popup_lcb( GtkTextView *textview, GtkMenu *menu, Tdocument *doc )
{
	GtkWidget * menuitem;
	/* I found no way to connect an item-factory to this menu widget, so we have to do it in the manual way... */
	gtk_menu_shell_prepend( GTK_MENU_SHELL( menu ), GTK_WIDGET( gtk_menu_item_new() ) );

	menuitem = gtk_menu_item_new_with_label( _( "Replace" ) );
	g_signal_connect( menuitem, "activate", G_CALLBACK( replace_cb ), doc->bfwin );
	/* gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( menuitem ), gtk_image_new_from_stock( GTK_STOCK_FIND_AND_REPLACE, GTK_ICON_SIZE_MENU ) ); */
	gtk_menu_shell_prepend( GTK_MENU_SHELL( menu ), GTK_WIDGET( menuitem ) );

	menuitem = gtk_menu_item_new_with_label(_("Find"));/* gtk_image_menu_item_new_with_label( _( "Find" ) ); */
	g_signal_connect( menuitem, "activate", G_CALLBACK( search_cb ), doc->bfwin );
	
	/* gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( menuitem ), gtk_image_new_from_stock( GTK_STOCK_FIND, GTK_ICON_SIZE_MENU ) ); */
	gtk_menu_shell_prepend( GTK_MENU_SHELL( menu ), GTK_WIDGET( menuitem ) );

	gtk_menu_shell_prepend( GTK_MENU_SHELL( menu ), GTK_WIDGET( gtk_menu_item_new() ) );

	if ( bmark_have_bookmark_at_stored_bevent( doc ) ) {
		menuitem = gtk_menu_item_new_with_label( _( "Delete bookmark" ) );
		g_signal_connect( menuitem, "activate", G_CALLBACK( rpopup_del_bookmark_lcb ), doc );
		gtk_menu_shell_prepend( GTK_MENU_SHELL( menu ), GTK_WIDGET( menuitem ) );
	} else {
		menuitem = gtk_menu_item_new_with_label( _( "Add bookmark" ) );
		g_signal_connect( menuitem, "activate", G_CALLBACK( rpopup_add_bookmark_lcb ), doc );
		gtk_menu_shell_prepend( GTK_MENU_SHELL( menu ), GTK_WIDGET( menuitem ) );
	}

	/*
		menuitem = gtk_menu_item_new_with_label(_("Add permanent bookmark"));
		g_signal_connect(menuitem, "activate", G_CALLBACK(rpopup_permanent_bookmark_lcb), doc->bfwin);
		gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), GTK_WIDGET(menuitem));
		
		menuitem = gtk_menu_item_new_with_label(_("Add temporary bookmark"));
		g_signal_connect(menuitem, "activate", G_CALLBACK(rpopup_temporary_bookmark_lcb), doc->bfwin);
		gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), GTK_WIDGET(menuitem)); */

	/* kyanh, removed, 20050219
		gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), GTK_WIDGET(gtk_menu_item_new()));
	
		menuitem = gtk_image_menu_item_new_with_label(_("Edit color"));
		gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), GTK_WIDGET(menuitem));
		if (rpopup_doc_located_color(doc)) {
			g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(rpopup_edit_color_cb), doc);
		} else {
			gtk_widget_set_sensitive(menuitem, FALSE);
		}
	
		menuitem = gtk_image_menu_item_new_with_label(_("Edit tag"));
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),new_pixmap(113));
		gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), GTK_WIDGET(menuitem));
		if (rpopup_doc_located_tag(doc)) {
			g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(rpopup_edit_tag_cb), doc);
		} else {
			gtk_widget_set_sensitive(menuitem, FALSE);
		}
	*/
	gtk_widget_show_all( GTK_WIDGET( menu ) );
}

static void doc_buffer_mark_set_lcb( GtkTextBuffer *buffer, GtkTextIter *iter, GtkTextMark *set_mark, Tdocument *doc )
{
	doc_set_statusbar_lncol( doc );
}
static void doc_buffer_changed_lcb( GtkTextBuffer *textbuffer, Tdocument*doc )
{
	doc_set_statusbar_lncol( doc );
}

static void doc_view_toggle_overwrite_lcb( GtkTextView *view, Tdocument *doc )
{
	doc->view_bars = SET_BIT(doc->view_bars, MODE_OVERWRITE,!GET_BIT(doc->view_bars,MODE_OVERWRITE));
	doc_set_statusbar_insovr( doc );
}

/**
* doc_bind_signals:
* @doc: a #Tdocument
*
* Bind signals related to the doc's buffer:
* "insert-text", "delete-range" and "insert-text" for autoindent
*
* Return value: void
**/
void doc_bind_signals( Tdocument *doc )
{
	doc->ins_txt_id = g_signal_connect( G_OBJECT( doc->buffer ),
					"insert-text",
					G_CALLBACK( doc_buffer_insert_text_lcb ), doc );
	doc->del_txt_id = g_signal_connect( G_OBJECT( doc->buffer ),
					"delete-range",
					G_CALLBACK( doc_buffer_delete_range_lcb ), doc );
	doc->ins_aft_txt_id = g_signal_connect_after( G_OBJECT( doc->buffer ),
			"insert-text",
			G_CALLBACK( doc_buffer_insert_text_after_lcb ), doc );
}

/**
* doc_unbind_signals:
* @doc: a #Tdocument
*
* Unbind signals related to the doc's buffer:
* "insert-text", "delete-range" and "insert-text" for autoindent.
* This function checks if each individual signal has been bound before unbinding.
*
* Return value: void
**/
void doc_unbind_signals( Tdocument *doc )
{
	/*	g_print("doc_unbind_signals, before unbind ins=%lu, del=%lu\n", doc->ins_txt_id, doc->del_txt_id);*/
	if ( doc->ins_txt_id != 0 ) {
		g_signal_handler_disconnect( G_OBJECT( doc->buffer ), doc->ins_txt_id );
		doc->ins_txt_id = 0;
	}
	if ( doc->del_txt_id != 0 ) {
		g_signal_handler_disconnect( G_OBJECT( doc->buffer ), doc->del_txt_id );
		doc->del_txt_id = 0;
	}
	if ( doc->ins_aft_txt_id != 0 ) {
		g_signal_handler_disconnect( G_OBJECT( doc->buffer ), doc->ins_aft_txt_id );
		doc->ins_txt_id = 0;
	}
}
gboolean buffer_to_file( Tbfwin *bfwin, gchar *buffer, gchar *filename )
{
	FILE * fd;
	gchar *ondiskencoding = get_filename_on_disk_encoding( filename );
	fd = fopen( ondiskencoding, "w" );
	g_free( ondiskencoding );
	if ( fd == NULL ) {
		DEBUG_MSG( "buffer_to_file, cannot open file %s\n", filename );
		return FALSE;
	}
	fputs( buffer, fd );
	fclose( fd );
	return TRUE;
}

/**
* gint doc_textbox_to_file
* @doc: a #Tdocument*
* @filename: a #gchar*
* @window_closing: a #gboolean if the window is closing, we should supress any statusbar messages then
*
* If applicable, backup existing file,
* possibly update meta-tags (HTML),
* and finally write the document to the specified file.
*
* Return value: #gint set to
* 1: on success
* 2: on success but the backup failed
* -1: if the backup failed and save was aborted
* -2: if the file could not be opened or written
* -3: if the backup failed and save was aborted by the user
* -4: if the charset encoding conversion failed and the save was aborted by the user
**/
gint doc_textbox_to_file( Tdocument * doc, gchar * filename, gboolean window_closing )
{
	gint backup_retval;
	gint write_retval;
	gchar *buffer;
	GtkTextIter itstart, itend;

	if ( !window_closing )
		statusbar_message( BFWIN( doc->bfwin ), _( "saving file" ), 1000 );
	/* kyanh, removed, 20020220
		if (main_v->props.auto_update_meta) {
			const gchar *realname = g_get_real_name();
			if (realname && strlen(realname) > 0)  {
				gchar *tmp;
				Tsearch_result res = doc_search_run_extern(doc,"<meta[ \t\n]name[ \t\n]*=[ \t\n]*\"generator\"[ \t\n]+content[ \t\n]*=[ \t\n]*\"[^\"]*bluefish[^\"]*\"[ \t\n]*>",1,0);
				if (res.end > 0) {
					snr2_run_extern_replace(doc,"<meta[ \t\n]name[ \t\n]*=[ \t\n]*\"generator\"[ \t\n]+content[ \t\n]*=[ \t\n]*\"[^\"]*\"[ \t\n]*>",0,1,0,"<meta name=\"generator\" content=\"Bluefish, see http://bluefish.openoffice.nl/\">", FALSE);
				}
				tmp = g_strconcat("<meta name=\"author\" content=\"",realname,"\">",NULL);
				snr2_run_extern_replace(doc,"<meta[ \t\n]name[ \t\n]*=[ \t\n]*\"author\"[ \t\n]+content[ \t\n]*=[ \t\n]*\"[^\"]*\"[ \t\n]*>",0,1,0,tmp,FALSE);
				g_free(tmp);
			}
		}
	*/
	/* This writes the contents of a textbox to a file */
	backup_retval = doc_check_backup( doc );

	if ( !backup_retval ) {
		if ( main_v->props.backup_abort_action == DOCUMENT_BACKUP_ABORT_ABORT ) {
			DEBUG_MSG( "doc_textbox_to_file, backup failure, abort!\n" );
			return -1;
		} else if ( main_v->props.backup_abort_action == DOCUMENT_BACKUP_ABORT_ASK ) {
			gchar * options[] = {_( "_Abort save" ), _( "_Continue save" ), NULL};
			gint retval;
			gchar *tmpstr = g_strdup_printf( _( "A backupfile for %s could not be created. If you continue, this file will be overwritten." ), filename );
			retval = multi_warning_dialog( BFWIN( doc->bfwin ) ->main_window, _( "File backup failure" ), tmpstr, 1, 0, options );
			g_free( tmpstr );
			if ( retval == 0 ) {
				DEBUG_MSG( "doc_textbox_to_file, backup failure, user aborted!\n" );
				return -3;
			}
		}
	}

	gtk_text_buffer_get_bounds( doc->buffer, &itstart, &itend );
	buffer = gtk_text_buffer_get_text( doc->buffer, &itstart, &itend, FALSE );

	if ( doc->encoding ) {
		gchar * newbuf;
		gsize bytes_written = 0, bytes_read = 0;
		DEBUG_MSG( "doc_textbox_to_file, converting from UTF-8 to %s\n", doc->encoding );
		newbuf = g_convert( buffer, -1, doc->encoding, "UTF-8", &bytes_read, &bytes_written, NULL );
		if ( newbuf ) {
			g_free( buffer );
			buffer = newbuf;
		} else {
			gchar *options[] = {_( "_Abort save" ), _( "_Continue save in UTF-8" ), NULL};
			gint retval, line, column;
			glong position;
			gchar *tmpstr, failed[ 6 ];
			GtkTextIter iter;
			position = g_utf8_pointer_to_offset( buffer, buffer + bytes_read );
			gtk_text_buffer_get_iter_at_offset( doc->buffer, &iter, position );
			line = gtk_text_iter_get_line( &iter );
			column = gtk_text_iter_get_line_offset( &iter );
			failed[ 0 ] = '\0';
			g_utf8_strncpy( failed, buffer + bytes_read, 1 );
			tmpstr = g_strdup_printf( _( "Failed to convert %s to character encoding %s. Encoding failed on character '%s' at line %d column %d\n\nContinue saving in UTF-8 encoding?" ), filename, doc->encoding, failed, line + 1, column + 1 );
			retval = multi_warning_dialog( BFWIN( doc->bfwin ) ->main_window, _( "File encoding conversion failure" ), tmpstr, 1, 0, options );
			g_free( tmpstr );
			if ( retval == 0 ) {
				DEBUG_MSG( "doc_textbox_to_file, character set conversion failed, user aborted!\n" );
				return -4;
			} else {
				/* continue in UTF-8 */
				/* update_encoding_meta_in_file(doc, "UTF-8");
				*/
				g_free( buffer );
				gtk_text_buffer_get_bounds( doc->buffer, &itstart, &itend );
				buffer = gtk_text_buffer_get_text( doc->buffer, &itstart, &itend, FALSE );
			}
		}
	}

	write_retval = buffer_to_file( BFWIN( doc->bfwin ), buffer, filename );
	DEBUG_MSG( "doc_textbox_to_file, write_retval=%d\n", write_retval );
	g_free( buffer );
	if ( !write_retval ) {
		return -2;
	}

	if ( main_v->props.view_bars & MODE_CLEAR_UNDO_HISTORY_ON_SAVE ) {
		doc_unre_clear_all( doc );
	}
	DEBUG_MSG( "doc_textbox_to_file, calling doc_set_modified(doc, 0)\n" );
	doc_set_modified( doc, 0 );
	if ( !backup_retval ) {
		return 2;
	} else {
		return 1;
	}

}

/**
* doc_destroy:
* @doc: a #Tdocument
* @delay_activation: #gboolean whether to delay gui-updates.
*
* Performs all actions neccessary to remove an open document from the fish:
* Adds filename to recent-list,
* removes the document from the documentlist and notebook,
* change notebook-focus (if !delay_activation),
* delete backupfile if required by pref,
* free all related memory.
*
* Return value: void
**/
void doc_destroy( Tdocument * doc, gboolean delay_activation )
{
	Tbfwin * bfwin = BFWIN( doc->bfwin );

	DEBUG_MSG( "wf: doc_destroy, calling bmark_clean_for_doc(%p)\n", doc );
	bmark_clean_for_doc( doc );
	/*        bmark_adjust_visible(bfwin);   */

	if ( doc->filename ) {
		add_to_recent_list( doc->bfwin, doc->filename, 1, FALSE );
	}
	gui_notebook_unbind_signals( BFWIN( doc->bfwin ) );
	/* to make this go really quick, we first only destroy the notebook page and run flush_queue(),
	after the document is gone from the GUI we complete the destroy, to destroy only the notebook
	page we ref+ the scrolthingie, remove the page, and unref it again */

	/* related to BUG#63 . Lines added by kyanh <kyanh@o2.pl> */
	GtkTextIter start, end;
	DEBUG_MSG("doc_destroy: finding start and end iter\n");
	gtk_text_buffer_get_bounds(doc->buffer, &start, &end);
	DEBUG_MSG("doc_destroy: found start and end iter\n");
	/* kyanh, 20050723, related to BUG#63
	for long document, we need remove all tags before deleting the buffer.
	Why? Keeping the tags will take a very long time to delete the buffer. */
	gtk_text_buffer_remove_all_tags(doc->buffer, &start, &end);
	gtk_text_buffer_delete(doc->buffer, &start, &end);
	DEBUG_MSG("doc_destroy: destroyed text buffer\n");
	/* >> */

	g_free(doc->brace_finder); /* free the brace finder */

	g_object_ref( doc->view->parent );
	if ( doc->floatingview ) {
		gtk_widget_destroy( FLOATINGVIEW( doc->floatingview ) ->window );
		doc->floatingview = NULL;
	}

	/* now we remove the document from the document list */
	bfwin->documentlist = g_list_remove( bfwin->documentlist, doc );
	DEBUG_MSG( "removed %p from documentlist, list %p length=%d\n", doc
		, bfwin->documentlist
		, g_list_length( bfwin->documentlist ) );
	if ( bfwin->current_document == doc ) {
		bfwin->current_document = NULL;
	}

	/* then we remove the page from the notebook */
	DEBUG_MSG( "about to remove widget from notebook (doc=%p, current_document=%p)\n", doc, bfwin->current_document );
	/* gtk_widget_destroy(doc->tab_label->parent); */
	gtk_notebook_remove_page( GTK_NOTEBOOK( bfwin->notebook ), gtk_notebook_page_num( GTK_NOTEBOOK( bfwin->notebook ), doc->view->parent ) );
	DEBUG_MSG( "doc_destroy, removed widget from notebook (doc=%p), delay_activation=%d\n", doc, delay_activation );
	DEBUG_MSG( "doc_destroy, (doc=%p) about to bind notebook signals...\n", doc );
	gui_notebook_bind_signals( BFWIN( doc->bfwin ) );
	if ( !delay_activation ) {
		notebook_changed( BFWIN( doc->bfwin ), -1 );
	}
	DEBUG_MSG( "doc_destroy, (doc=%p) after calling notebook_changed()\n", doc );
	/* now we really start to destroy the document */
	/* kyanh, 20050226, the program terminates with SEGEMENT FAULT here */
	gtk_object_sink( GTK_OBJECT( doc->view->parent ) );
	/* g_object_unref(doc->view->parent); */
	/* kyanh, 20050225, 15:30: WOH,
	doc->view is a GtkTextView, it is a GtkObject.
	That's mean that it can be 'sink'.
	I try to remove the using g_object_unref by gtk_object_sink()
	and thing went well, at least at this point :)) CHEERS
	BUG[200502]#7
	*/
	if ( doc->filename ) {
		if ( (main_v->props.view_bars & MODE_REMOVE_BACKUP_ON_CLOSE) && strlen(main_v->props.backup_filestring) ) {
			gchar * backupfile = g_strconcat( doc->filename, main_v->props.backup_filestring, NULL );
			DEBUG_MSG( "unlinking %s, doc->filename=%s\n", backupfile, doc->filename );
			unlink( backupfile );
			g_free( backupfile );
		}
		g_free( doc->filename );
	}

	if ( doc->encoding )
		g_free( doc->encoding );

	g_object_unref( doc->buffer );
	doc_unre_destroy( doc );
	DEBUG_MSG( "doc_destroy, finished for %p\n", doc );
	g_free( doc );
}

/**
* document_unset_filename:
* @document: #Tdocument*
*
* this function is called if some other document is saved with a filename
* equal to this files filename, or when this file is deleted in the filebrowser
*
* return value: void, ignored
*/
void document_unset_filename( Tdocument *doc )
{
	if ( doc->filename ) {
		gchar * tmpstr2, *tmpstr3;
		gchar *tmpstr, *oldfilename = doc->filename;
		doc->filename = NULL;
		doc_set_title( doc );
		tmpstr2 = g_path_get_basename( oldfilename );
		tmpstr3 = get_utf8filename_from_on_disk_encoding( tmpstr2 );
		tmpstr = g_strconcat( _( "Previously: " ), tmpstr3, NULL );
		g_free( tmpstr2 );
		g_free( tmpstr3 );
		gtk_label_set( GTK_LABEL( doc->tab_label ), tmpstr );
		g_free( tmpstr );
		g_free( oldfilename );
	}
}

/**
* ask_new_filename:
* @bfwin: #Tbfwin* mainly used to set the dialog transient
* @oldfilename: #gchar* with the old filename
* @gui_name: #const gchar* with the name of the file used in the GUI
* @is_move: #gboolean if the title should be move or save as
*
* returns a newly allocated string with a new filename
*
* if a file with the selected name name was
* open already it will ask the user what to do, return NULL if
* the user wants to abort, or will remove the name of the other file if the user wants
* to continue
*
* Return value: gchar* with newly allocated string, or NULL on failure or abort
**/
gchar *ask_new_filename( Tbfwin *bfwin, gchar *oldfilename, const gchar *gui_name, gboolean is_move )
{
	Tdocument * exdoc;
	GList *alldocs;
	gchar *ondisk = get_filename_on_disk_encoding( oldfilename );
	gchar *newfilename = NULL;
	gchar *dialogtext;

	dialogtext = g_strdup_printf( ( is_move ) ? _( "Move/rename %s to" ) : _( "Save %s as" ), gui_name );
#ifdef HAVE_ATLEAST_GTK_2_4

	{
		GtkWidget *dialog;
		dialog = file_chooser_dialog( bfwin, dialogtext, GTK_FILE_CHOOSER_ACTION_SAVE, oldfilename, TRUE, FALSE, NULL );
		if ( gtk_dialog_run( GTK_DIALOG( dialog ) ) == GTK_RESPONSE_ACCEPT )
		{
			newfilename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( dialog ) );
		}
		gtk_widget_destroy( dialog );
	}
#else
	newfilename = return_file_w_title( ondisk, dialogtext );
#endif

	g_free( ondisk );
	g_free( dialogtext );

	if ( !newfilename ) {
		return NULL;
	}else{
		/* ondisk = get_filename_on_disk_encoding(newfilename); */
		if (g_file_test(newfilename, G_FILE_TEST_EXISTS)) {
			struct stat statbuf;
			gint inode;
			gint l_retval =1;
			ondisk = get_filename_on_disk_encoding(newfilename);
			if ( stat(ondisk,&statbuf) != 0 ) {
				DEBUG_MSG("ask_new_filename: stat newfile [%s] failed\n", newfilename);
				l_retval = 0;
			}else if (oldfilename) {
				inode = statbuf.st_ino;
				g_free(ondisk);
				ondisk = get_filename_on_disk_encoding(oldfilename);
				if ( stat(ondisk, &statbuf) !=0 ) {
					DEBUG_MSG("ask_new_filename: stat oldfile [%s] failed\n", oldfilename);
					l_retval = 0;
				}else if (l_retval && (statbuf.st_ino == inode) ) {
					l_retval = 0;
				}
			}
			g_free(ondisk);
			if (l_retval ==0) {
				DEBUG_MSG("ask_new_filename: stat failed or new file == old file. return NULL\n");
				g_free( newfilename );
				return NULL;
			}
		}
	}

	/* make a full path, re-use the ondisk variable */
	ondisk = newfilename;
	newfilename = create_full_path( ondisk, NULL );
	g_free( ondisk );


	alldocs = return_allwindows_documentlist();
	exdoc = documentlist_return_document_from_filename( alldocs, newfilename );
	g_list_free( alldocs );
	DEBUG_MSG( "ask_new_filename, exdoc=%p, newfilename=%s\n", exdoc, newfilename );
	if ( exdoc ) {
		gchar * tmpstr;
		gint retval;
		gchar *options[] = {_( "_Cancel" ), _( "_Overwrite" ), NULL};
		tmpstr = g_strdup_printf( _( "File %s exists and is opened, overwrite?" ), newfilename );
		retval = multi_warning_dialog( bfwin->main_window, tmpstr, _( "The file you have selected is being edited in Winefish." ), 1, 0, options );
		g_free( tmpstr );
		if ( retval == 0 ) {
			g_free( newfilename );
			return NULL;
		} else {
			document_unset_filename( exdoc );
		}
	} else {
		/* gchar *ondiskencoding = get_filename_on_disk_encoding( newfilename ); */
		if ( g_file_test( newfilename, G_FILE_TEST_EXISTS ) ) {
			gchar * tmpstr;
			gint retval;
			gchar *options[] = {_( "_Cancel" ), _( "_Overwrite" ), NULL};
			tmpstr = g_strdup_printf( _( "A file named \"%s\" already exists." ), newfilename );
			retval = multi_warning_dialog( bfwin->main_window, tmpstr,
						_( "Do you want to replace the existing file?" ), 1, 0, options );
			g_free( tmpstr );
			if ( retval == 0 ) {
				g_free( newfilename );
				/* g_free( ondiskencoding ); */
				return NULL;
			}
		}
		/* g_free( ondiskencoding ); */
	}
	return newfilename;
}


/**
* doc_save:
* @doc: the #Tdocument to save
* @do_save_as: #gboolean set to 1 if "save as"
* @do_move: #gboolean set to 1 if moving the file.
* @window_closing: #gboolean if the window is closing, should suppress statusbar messages then
*
* Performs all neccessary actions to save an open document.
* Warns the user of problems, and asks for a filename if neccessary.
* 
* Return value: #gint set to
* 1: on success
* 2: on success but the backup failed
* 3: on user abort
* -1: if the backup failed and save was aborted
* -2: if the file pointer could not be opened 
* -3: if the backup failed and save was aborted by the user
* -4: if there is no filename, after asking one from the user
* -5: if another process modified the file, and the user chose cancel
**/
enum {
	DOC_SAVE_RET_OK = 1,
	DOC_SAVE_RET_OK_BUT_BACKUP = 2,
	DOC_SAVE_RET_USER_ABORT = 3,
	DOC_SAVE_RET_BACKUP_FAILED_SAVE_ABORT =-1,
	DOC_SAVE_RET_COULD_NOT_OPEN_FILE =-2,
	DOC_SAVE_RET_BACKUP_FAILED_SAVE_USER_ABORT =-3,
	DOC_SAVE_RET_NO_FILENAME =-4,
	DOC_SAVE_RET_FILE_MODIFIED_USER_ABORT =-5
};

gint doc_save( Tdocument * doc, gboolean do_save_as, gboolean do_move, gboolean window_closing )
{
	gint retval = 0;
	gchar *old_name = NULL;
#ifdef DEBUG
	g_assert( doc );
#endif

	DEBUG_MSG( "doc_save, doc=%p, save_as=%d, do_move=%d\n", doc, do_save_as, do_move );
	if ( doc->filename == NULL ) {
		do_save_as = 1;
	}
	if ( do_move ) {
		do_save_as = 1;
		if ( doc->filename == NULL ) {
			do_move = FALSE;
		}
	}

	if ( do_save_as ) {
		DEBUG_MSG("doc_save: do save as...\n");
		gchar * newfilename = NULL;
		if ( !window_closing )
			statusbar_message( BFWIN( doc->bfwin ), _( "save as..." ), 1 );
		newfilename = ask_new_filename( BFWIN( doc->bfwin ), doc->filename, gtk_label_get_text( GTK_LABEL( doc->tab_label ) ), do_move );
		if ( !newfilename ) {
			DEBUG_MSG("doc_save: user abort\n");
			return DOC_SAVE_RET_USER_ABORT;
		}
#ifdef UNFIX_BUG_92
		/*if ( doc->filename ) { */
		if ( do_move ) {
			gchar * ondiskencoding = get_filename_on_disk_encoding( doc->filename );
			unlink( ondiskencoding );
			g_free( ondiskencoding );
		}
#endif /* UNFIX_BUG_92 */
		/*}*/
		if (do_move) {
			old_name = g_strdup(doc->filename);
		}
		g_free( doc->filename );
		doc->filename = newfilename;
		/* TODO: should feed the contents to the function too !! */
		doc_reset_filetype( doc, doc->filename, NULL );
		doc_set_title( doc );
		if ( doc == BFWIN( doc->bfwin ) ->current_document ) {
			gui_set_title( BFWIN( doc->bfwin ), doc );
		}
	} else /* (!do_save_as) */
	{
		gboolean modified;
		time_t oldmtime, newmtime;
		struct stat statbuf;
		modified = doc_check_modified_on_disk( doc, &statbuf );
		newmtime = statbuf.st_mtime;
		oldmtime = doc->statbuf.st_mtime;
		if ( modified ) {
			gchar * tmpstr, oldtimestr[ 128 ], newtimestr[ 128 ]; /* according to 'man ctime_r' this should be at least 26, so 128 should do ;-)*/
			gint retval;
			gchar *options[] = {_( "_Cancel" ), _( "_Overwrite" ), NULL};

			ctime_r( &newmtime, newtimestr );
			ctime_r( &oldmtime, oldtimestr );
			tmpstr = g_strdup_printf( _( "File: %s\n\nNew modification time: %s\nOld modification time: %s" ), doc->filename, newtimestr, oldtimestr );
			retval = multi_warning_dialog( BFWIN( doc->bfwin ) ->main_window, _( "File has been modified by another process." ), tmpstr, 1, 0, options );
			g_free( tmpstr );
			if ( retval == 0 ) {
				return DOC_SAVE_RET_FILE_MODIFIED_USER_ABORT;
			}
		}
	}
	
	DEBUG_MSG( "doc_save, returned file %s\n", doc->filename );
	/*	if (do_save_as && oldfilename && main_v->props.link_management) {
			update_filenames_in_file(doc, oldfilename, doc->filename, 1);
		}*/
	{
		gchar *tmp = g_strdup_printf( _( "Saving %s" ), doc->filename );
		if ( !window_closing )
			statusbar_message( BFWIN( doc->bfwin ), tmp, 1 );
		g_free( tmp );
		/* re-use tmp */
		tmp = g_path_get_dirname( doc->filename );
		if ( BFWIN( doc->bfwin ) ->session->savedir )
			g_free( BFWIN( doc->bfwin ) ->session->savedir );
		BFWIN( doc->bfwin ) ->session->savedir = tmp;
	}
	retval = doc_textbox_to_file( doc, doc->filename, window_closing );

	switch ( retval ) {
		gchar * errmessage;
	case DOC_SAVE_RET_BACKUP_FAILED_SAVE_ABORT:
		/* backup failed and aborted */
		errmessage = g_strconcat( _( "Could not backup file:\n\"" ), doc->filename, "\"", NULL );
		error_dialog( BFWIN( doc->bfwin ) ->main_window, _( "File save aborted.\n" ), errmessage );
		g_free( errmessage );
		break;
	case DOC_SAVE_RET_COULD_NOT_OPEN_FILE:
		/* could not open the file pointer */
		errmessage = g_strconcat( _( "Could not write file:\n\"" ), doc->filename, "\"", NULL );
		error_dialog( BFWIN( doc->bfwin ) ->main_window, _( "File save error" ), errmessage );
		g_free( errmessage );
		break;
	case DOC_SAVE_RET_BACKUP_FAILED_SAVE_USER_ABORT:
	case DOC_SAVE_RET_NO_FILENAME:
		/* do nothing, the save is aborted by the user */
		break;
	default:
		doc_set_stat_info( doc );
		{
			gchar *tmp = path_get_dirname_with_ending_slash( doc->filename );
			bfwin_filebrowser_refresh_dir( BFWIN( doc->bfwin ), tmp );
			g_free( tmp );
			/* BUG#92. only move file after saving is okay. */
			if ( do_move ) {
				DEBUG_MSG("remove the old file =%s\n", old_name);
				gchar * ondiskencoding = get_filename_on_disk_encoding( old_name );
				unlink( ondiskencoding );
				g_free( ondiskencoding );
				g_free(old_name);
			}
		}

		DEBUG_MSG( "doc_save, received return value %d from doc_textbox_to_file\n", retval );
		break;
	}
	return retval;
}

/**
* doc_close:
* @doc: The #Tdocument to clase.
* @warn_only: a #gint set to 1 if the document shouldn't actually be destroyed.
*
* Get confirmation when closing an unsaved file, save it if neccessary,
* and destroy the file unless aborted by user.
*
* Return value: #gint set to 0 (when cancelled/aborted) or 1 (when closed or saved&closed)
**/
gint doc_close( Tdocument * doc, gint warn_only )
{
	gchar * text;
	gint retval;
#ifdef DEBUG

	if ( !doc ) {
		DEBUG_MSG( "doc_close, returning because doc=NULL\n" );
		return 0;
	}
#endif

	if ( doc_is_empty_non_modified_and_nameless( doc ) && g_list_length( BFWIN( doc->bfwin ) ->documentlist ) == 1 ) {
		/* no need to close this doc, it's an Untitled empty document */
		DEBUG_MSG( "doc_close, 1 untitled empty non-modified document, returning\n" );
		return 0;
	}

	if ( doc->modified ) {
		/*if (doc->tab_label) {*/
		text = g_strdup_printf( _( "Save changes to \"%s\" before closing?." ),
					gtk_label_get_text ( GTK_LABEL ( doc->tab_label ) ) );
		/*} else {
			text = g_strdup(_("Save changes to this untitled file before closing?"));
		}*/

		{
			gchar *buttons[] = {_( "Do_n't save" ), GTK_STOCK_CANCEL, GTK_STOCK_SAVE, NULL};
			retval = multi_query_dialog( BFWIN( doc->bfwin ) ->main_window, text,
						_( "If you don't save your changes they will be lost." ), 2, 1, buttons );
		}
		g_free( text );

		switch ( retval ) {
		case 1:
			DEBUG_MSG( "doc_close, retval=2 (cancel) , returning\n" );
			return 2;
			break;
		case 2:
			doc_save( doc, FALSE, FALSE, FALSE );
			if ( doc->modified == 1 ) {
				/* something went wrong it's still not saved */
				return 0;
			}
			if ( !warn_only ) {
				doc_destroy( doc, FALSE );
			}
			break;
		case 0:
			if ( !warn_only ) {
				doc_destroy( doc, FALSE );
			}
			break;
		default:
			return 0;			/* something went wrong */
			break;
		}
	} else {
		if ( !warn_only ) {
			DEBUG_MSG( "doc_close, starting doc_destroy for doc=%p\n", doc );
			doc_destroy( doc, FALSE );
		}
	}
	DEBUG_MSG( "doc_close, finished\n" );
	/*	notebook_changed();*/
	return 1;
}

static void doc_close_but_clicked_lcb( GtkWidget *wid, gpointer data )
{

	doc_close( data, 0 );
}

/* contributed by Oskar Swida <swida@aragorn.pb.bialystok.pl>, with help from the gedit source */
static gboolean doc_textview_expose_event_lcb( GtkWidget * widget, GdkEventExpose * event, gpointer doc )
{
	GtkTextView * view = ( GtkTextView* ) widget;
	GdkRectangle rect;
	GdkWindow *win;
	GtkTextIter l_start, l_end, it;
	gint l_top1, l_top2;
	PangoLayout *l;
	gchar *pomstr;
	gint numlines, w, i;
	GHashTable *temp_tab;
	gint text_width;

	win = gtk_text_view_get_window( view, GTK_TEXT_WINDOW_LEFT );
	if ( win != event->window ) {
#ifndef STUPID	
		if ( event->window == gtk_text_view_get_window(view, GTK_TEXT_WINDOW_TEXT) )
		{
			{/* current line hilighting */
#ifdef STUPID_A_
				gint w2;
				GtkTextBuffer *buf = gtk_text_view_get_buffer(view);
				gtk_text_buffer_get_iter_at_mark(buf, &it, gtk_text_buffer_get_insert(buf));
				gtk_text_view_get_visible_rect(view, &rect);
				gtk_text_view_get_line_yrange(view, &it, &w, &w2);
				gtk_text_view_buffer_to_window_coords(view, GTK_TEXT_WINDOW_TEXT, rect.x, rect.y, &rect.x, &rect.y);
				gtk_text_view_buffer_to_window_coords(view, GTK_TEXT_WINDOW_TEXT, 0, w, NULL, &w);
				gdk_draw_rectangle(event->window, widget->style->bg_gc[GTK_WIDGET_STATE(widget)], TRUE,rect.x, w, rect.width, w2);
#else	
				GdkRectangle iter_rect;

				GtkTextBuffer *buf = gtk_text_view_get_buffer(view);
				gtk_text_buffer_get_iter_at_mark(buf, &it, gtk_text_buffer_get_insert(buf));

				gtk_text_view_get_visible_rect(view, &rect);
				gtk_text_view_get_iter_location(view, &it, &iter_rect);

				gtk_text_view_buffer_to_window_coords(view, GTK_TEXT_WINDOW_TEXT, rect.x, rect.y, &rect.x, &rect.y);
				gtk_text_view_buffer_to_window_coords(view, GTK_TEXT_WINDOW_TEXT, 0, iter_rect.y, NULL, &iter_rect.y);

				gdk_draw_rectangle(event->window, widget->style->bg_gc[GTK_WIDGET_STATE(widget)], TRUE,rect.x, iter_rect.y, rect.width, iter_rect.height);
#endif /* STUPID_A_ */
			}
			if (main_v->props.marker_ii || main_v->props.marker_iii || main_v->props.marker_i) {/* column marker */
				GdkRectangle visible_rect;
				GdkRectangle redraw_rect;
				gint tab_width_i=0, tab_width_ii=0, tab_width_iii=0;

				gchar *tab_string;
				if (main_v->props.marker_i) {
					tab_string = g_strnfill (main_v->props.marker_i, '_');
					tab_width_i = widget_get_string_size(widget, tab_string);
					g_free(tab_string);
				}
				
				if (main_v->props.marker_ii && (main_v->props.marker_ii != main_v->props.marker_i)) {
					tab_string = g_strnfill (main_v->props.marker_ii, '_');
					tab_width_ii = widget_get_string_size(widget, tab_string);
					g_free(tab_string);
				}
				
				if (main_v->props.marker_iii && (main_v->props.marker_iii != main_v->props.marker_i) && (main_v->props.marker_iii != main_v->props.marker_ii)) {
					tab_string = g_strnfill (main_v->props.marker_iii, '_');
					tab_width_iii = widget_get_string_size(widget, tab_string);
					g_free(tab_string);
				}
			
				gtk_text_view_get_visible_rect (view, &visible_rect);
				gtk_text_view_buffer_to_window_coords (view,GTK_TEXT_WINDOW_TEXT, visible_rect.x,visible_rect.y,	&redraw_rect.x,&redraw_rect.y);
				redraw_rect.width = visible_rect.width;
				redraw_rect.height = visible_rect.height;
				
				if (tab_width_i) {
					gtk_paint_vline(widget->style,event->window,GTK_WIDGET_STATE (widget), &redraw_rect,widget,"marker", redraw_rect.y, redraw_rect.y + redraw_rect.height, tab_width_i - visible_rect.x + redraw_rect.x + gtk_text_view_get_left_margin (view));
				}
				if (tab_width_ii) {
					gtk_paint_vline(widget->style,event->window,GTK_WIDGET_STATE (widget), &redraw_rect,widget,"marker", redraw_rect.y, redraw_rect.y + redraw_rect.height, tab_width_ii - visible_rect.x + redraw_rect.x + gtk_text_view_get_left_margin (view));
				}
				if (tab_width_iii) {
					gtk_paint_vline(widget->style,event->window,GTK_WIDGET_STATE (widget), &redraw_rect,widget,"marker", redraw_rect.y, redraw_rect.y + redraw_rect.height, tab_width_iii - visible_rect.x + redraw_rect.x + gtk_text_view_get_left_margin (view));
				}
				
			}
		}
#endif		
		return FALSE;
	}

	gtk_text_view_get_visible_rect( view, &rect );
	gtk_text_view_get_line_at_y( view, &l_start, rect.y, &l_top1 );
	gtk_text_view_get_line_at_y( view, &l_end, rect.y + rect.height, &l_top2 );

	numlines = gtk_text_buffer_get_line_count( gtk_text_view_get_buffer( view ) );
	pomstr = g_strdup_printf( "%d", MAX( 99, numlines ) );
	l = gtk_widget_create_pango_layout( widget, pomstr ); 
	g_free( pomstr );

	pango_layout_get_pixel_size( l, &text_width, NULL );
	pango_layout_set_width (l,text_width);
	pango_layout_set_alignment (l, PANGO_ALIGN_RIGHT);

	gtk_text_view_set_border_window_size( view, GTK_TEXT_WINDOW_LEFT, text_width + 4 );

	it = l_start;
	temp_tab = bmark_get_bookmarked_lines( DOCUMENT( doc ), &l_start, &l_end );
	for ( i = gtk_text_iter_get_line( &l_start );i <= gtk_text_iter_get_line( &l_end );i++ ) {
		gchar* val;
		gtk_text_iter_set_line( &it, i );
		gtk_text_view_get_line_yrange( view, &it, &w, NULL );
		gtk_text_view_buffer_to_window_coords( view, GTK_TEXT_WINDOW_LEFT, 0, w, NULL, &w );
		pomstr = NULL;
		if ( temp_tab ) {
			val = g_hash_table_lookup( temp_tab, &i );
			if ( val ) {
				pomstr = g_strdup_printf( "<span background=\"%s\" >%d</span>", val[ 0 ] == '0' ? "#768BEA" : "#62CB7F", i + 1 );
			}
		}
		if ( pomstr == NULL ) {
			pomstr = g_strdup_printf( "%d", i + 1 );
		}
		pango_layout_set_markup( l, pomstr, -1 );
		gtk_paint_layout( widget->style, win, GTK_WIDGET_STATE( widget ), FALSE, NULL, widget, NULL, text_width + 2, w, l );
		g_free( pomstr );
	}
	g_object_unref( G_OBJECT( l ) );
	if ( temp_tab )
		g_hash_table_destroy( temp_tab );
	return TRUE;
}

/**
* document_set_line_numbers:
* @doc: a #Tdocument*
* @value: a #gboolean
*
* Show or hide linenumbers (at the left of the main GtkTextView).
*
* Return value: void
**/
void document_set_line_numbers( Tdocument *doc, gboolean value )
{
	if ( value ) {
		gtk_text_view_set_left_margin( GTK_TEXT_VIEW( doc->view ), 2 );
		gtk_text_view_set_border_window_size( GTK_TEXT_VIEW( doc->view ), GTK_TEXT_WINDOW_LEFT, 20 );
		g_signal_connect( G_OBJECT( doc->view ), "expose-event", G_CALLBACK( doc_textview_expose_event_lcb ), doc );
	} else {
		gtk_text_view_set_left_margin( GTK_TEXT_VIEW( doc->view ), 0 );
		gtk_text_view_set_border_window_size( GTK_TEXT_VIEW( doc->view ), GTK_TEXT_WINDOW_LEFT, 0 );
	}
}

static void doc_view_drag_end_lcb( GtkWidget *widget, GdkDragContext *drag_context, Tdocument *doc )
{
	if ( doc->paste_operation ) {
		if ( PASTEOPERATION( doc->paste_operation ) ->eo > PASTEOPERATION( doc->paste_operation ) ->so ) {
			doc_highlight_region( doc, PASTEOPERATION( doc->paste_operation ) ->so, PASTEOPERATION( doc->paste_operation ) ->eo );
		}
		g_free( doc->paste_operation );
		doc->paste_operation = NULL;
	}
}
static void doc_view_drag_begin_lcb( GtkWidget *widget, GdkDragContext *drag_context, Tdocument *doc )
{
	if ( !doc->paste_operation ) {
		doc->paste_operation = g_new( Tpasteoperation, 1 );
		PASTEOPERATION( doc->paste_operation ) ->so = -1;
		PASTEOPERATION( doc->paste_operation ) ->eo = -1;
	}
}

/**
* doc_new:
* @bfwin: #Tbfwin* with the window to open the document in
* @delay_activate: Whether to perform GUI-calls and flush_queue(). Set to TRUE when loading several documents at once.
*
* Create a new document, related structures and a nice little textview to display the document in.
* Finally, add a new tab to the notebook.
* The GtkTextView is not actually gtk_widget_shown() if delay_activate == TRUE. This is done by doc_activate() instead.
*
* Return value: a #Tdocument* pointer to the just created document.
**/
Tdocument *doc_new( Tbfwin* bfwin, gboolean delay_activate )
{
	GtkWidget * scroll;
	Tdocument *newdoc = g_new0( Tdocument, 1 );
	DEBUG_MSG( "doc_new, main_v is at %p, bfwin at %p, newdoc at %p\n", main_v, bfwin, newdoc );
	
	newdoc->bfwin = ( gpointer ) bfwin;
	newdoc->hl = ( Tfiletype * ) ( ( GList * ) g_list_first( main_v->filetypelist ) ) ->data;

	/* * VIEW_BARS settings * */
	/* so stupid ;) ~~~> BUG#81
	newdoc->view_bars = SET_BIT(newdoc->view_bars, VIEW_COLORIZED, (main_v->props.view_bars & VIEW_COLORIZED ) && ( newdoc->hl->autoclosingtag > 0 ));
	*/
	if ( (main_v->props.view_bars & MODE_AUTO_COMPLETE) && (newdoc->hl->autoclosingtag>0) ) {
		newdoc->view_bars = SET_BIT(newdoc->view_bars, MODE_AUTO_COMPLETE, 1);
	}
	DEBUG_MSG("doc_new, autocompletion: main=%d, filetype=%d, current=%d\n", main_v->props.view_bars & MODE_AUTO_COMPLETE, (newdoc->hl->autoclosingtag > 0), newdoc->view_bars & MODE_AUTO_COMPLETE);

	/* use project's settings if there's; otherwise use global properties */
	newdoc->view_bars = SET_BIT(newdoc->view_bars, MODE_WRAP, ( bfwin->project ) ? GET_BIT(bfwin->project->view_bars,MODE_WRAP) : GET_BIT(main_v->props.view_bars,MODE_WRAP) );
	newdoc->view_bars = SET_BIT(newdoc->view_bars, MODE_OVERWRITE, FALSE);

	newdoc->view_bars = SET_BIT(newdoc->view_bars, VIEW_COLORIZED, GET_BIT(main_v->props.view_bars,VIEW_COLORIZED));

	/* use session value */
	newdoc->view_bars = SET_BIT(newdoc->view_bars, VIEW_LINE_NUMBER, GET_BIT(main_v->session->view_bars,VIEW_LINE_NUMBER));

	/* * End VIEW_BARS settings * */

	newdoc->buffer = gtk_text_buffer_new( highlight_return_tagtable() );
	
	newdoc->brace_finder =g_new0( Tbracefinder, 1);
	BRACEFINDER(newdoc->brace_finder)->tag = gtk_text_buffer_create_tag (newdoc->buffer, NULL,"background", "yellow", "foreground", "black", NULL);
	GtkTextIter iter;
	gtk_text_buffer_get_start_iter(newdoc->buffer,&iter);
	BRACEFINDER(newdoc->brace_finder)->mark_left = gtk_text_buffer_create_mark(newdoc->buffer,NULL,&iter,FALSE);
	BRACEFINDER(newdoc->brace_finder)->mark_mid = gtk_text_buffer_create_mark(newdoc->buffer,NULL,&iter,FALSE);
	BRACEFINDER(newdoc->brace_finder)->mark_right = gtk_text_buffer_create_mark(newdoc->buffer,NULL,&iter,FALSE);

	BRACEFINDER(newdoc->brace_finder)->last_status = 0;
	
	newdoc->view = gtk_text_view_new_with_buffer( newdoc->buffer );
	scroll = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scroll ),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC );
	gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW
					( scroll ), GTK_SHADOW_IN );
	gtk_container_add( GTK_CONTAINER( scroll ), newdoc->view );

	newdoc->tab_label = gtk_label_new( NULL );
	GTK_WIDGET_UNSET_FLAGS( newdoc->tab_label, GTK_CAN_FOCUS );
	if ( strlen( main_v->props.tab_font_string ) ) {
		apply_font_style( newdoc->tab_label, main_v->props.tab_font_string );
	}
	newdoc->tab_menu = gtk_label_new( NULL );
	newdoc->tab_eventbox = gtk_event_box_new();
	gtk_misc_set_alignment( GTK_MISC( newdoc->tab_menu ), 0, 0 );

	doc_unre_init( newdoc );
	doc_set_font( newdoc, NULL );
	
	doc_set_wrap( newdoc );
	/* newdoc->modified = 0; */
	doc_set_title( newdoc );
	/*newdoc->filename = NULL;*/
	newdoc->need_highlighting = 0;
	newdoc->statbuf.st_mtime = 0;
	newdoc->statbuf.st_size = 0;
	newdoc->statbuf.st_uid = -1;
	newdoc->statbuf.st_gid = -1;
	newdoc->is_symlink = 0;
	newdoc->encoding = g_strdup( main_v->props.newfile_default_encoding );

	doc_bind_signals( newdoc );

	g_signal_connect( G_OBJECT( newdoc->view ), "button-release-event",
			G_CALLBACK( doc_view_button_release_lcb ), newdoc );
	g_signal_connect( G_OBJECT( newdoc->view ), "button-press-event",
			G_CALLBACK( doc_view_button_press_lcb ), newdoc );
	g_signal_connect( G_OBJECT( newdoc->buffer ), "changed",
			G_CALLBACK( doc_buffer_changed_lcb ), newdoc );
	g_signal_connect( G_OBJECT( newdoc->buffer ), "mark-set",
			G_CALLBACK( doc_buffer_mark_set_lcb ), newdoc );
	g_signal_connect( G_OBJECT( newdoc->view ), "toggle-overwrite",
			G_CALLBACK( doc_view_toggle_overwrite_lcb ), newdoc );
	/*	g_signal_connect(G_OBJECT(newdoc->view), "paste-clipboard",
			G_CALLBACK(doc_paste_clipboard_lcb), newdoc);
		g_signal_connect_after(G_OBJECT(newdoc->view), "button-release-event", 
			G_CALLBACK(doc_view_button_release_after_lcb), newdoc);*/
	g_signal_connect_after( G_OBJECT( newdoc->view ), "drag-end",
				G_CALLBACK( doc_view_drag_end_lcb ), newdoc );
	g_signal_connect_after( G_OBJECT( newdoc->view ), "drag-begin",
				G_CALLBACK( doc_view_drag_begin_lcb ), newdoc );
	g_signal_connect_after( G_OBJECT( newdoc->view ), "key-release-event",
				G_CALLBACK( doc_view_key_release_lcb ), newdoc );
	g_signal_connect( G_OBJECT( newdoc->view ), "key-press-event",
			G_CALLBACK( doc_view_key_press_lcb ), newdoc );
	g_signal_connect_after( G_OBJECT( newdoc->view ), "populate-popup",
				G_CALLBACK( doc_view_populate_popup_lcb ), newdoc );

	bfwin->documentlist = g_list_append( bfwin->documentlist, newdoc );

	if ( !delay_activate )
		gtk_widget_show( newdoc->view ); /* Delay _show() if neccessary */

	gtk_widget_show( newdoc->tab_label );
	gtk_widget_show( scroll );

	DEBUG_MSG( "doc_new, appending doc to notebook\n" );
	{
		GtkWidget *hbox, *but, *image;
		hbox = gtk_hbox_new( FALSE, 0 );
		but = gtk_button_new();
		image = new_pixmap( 101 );
		gtk_container_add( GTK_CONTAINER( but ), image );
		gtk_container_set_border_width( GTK_CONTAINER( but ), 0 );
		gtk_widget_set_size_request( but, 12, 12 );
		gtk_button_set_relief( GTK_BUTTON( but ), GTK_RELIEF_NONE );
		g_signal_connect( G_OBJECT( but ), "clicked", G_CALLBACK( doc_close_but_clicked_lcb ), newdoc );
		gtk_container_add( GTK_CONTAINER( newdoc->tab_eventbox ), newdoc->tab_label );
		gtk_box_pack_start( GTK_BOX( hbox ), newdoc->tab_eventbox, FALSE, FALSE, 0 );
		gtk_box_pack_start( GTK_BOX( hbox ), but, FALSE, FALSE, 0 );
		gtk_widget_show_all( hbox );
		gtk_notebook_append_page_menu( GTK_NOTEBOOK( bfwin->notebook ), scroll , hbox, newdoc->tab_menu );
	}

	/* why don't we move document_set_line_numbers() to gui_set_document_widgets() ? */
	document_set_line_numbers( newdoc, GET_BIT(newdoc->view_bars, VIEW_LINE_NUMBER ) );

	/* for some reason it only works after the document is appended to the notebook */
	doc_set_tabsize( newdoc, main_v->props.editor_tab_width );

	/* newdoc->view_bars = SET_BIT(newdoc->view_bars, VIEW_COLORIZED, GET_BIT(main_v->props.view_bars,VIEW_COLORIZED)); */
#ifdef DONE_IN_GUI_SET_DOCUMENT_WIDGETS
	/* BUG#77 */
	if (newdoc->view_bars & VIEW_COLORIZED) {
		setup_toggle_item( gtk_item_factory_from_widget( bfwin->menubar ), _( "/Document/Highlight Syntax" ), TRUE );
	}
	if (newdoc->view_bars & VIEW_LINE_NUMBER) {
		setup_toggle_item( gtk_item_factory_from_widget( bfwin->menubar ), _( "/Document/Line Numbers" ), TRUE );
	}
	if (newdoc->view_bars & MODE_AUTO_COMPLETE) {
		setup_toggle_item( gtk_item_factory_from_widget( bfwin->menubar ), _( "/Document/AutoCompletion" ), TRUE );
	}
	g_print( "doc_new, autocomplete =%d \n", newdoc->view_bars & MODE_AUTO_COMPLETE),
	DEBUG_MSG( "doc_new, need_highlighting=%d, highlightstate=%d, global higlight = %d\n", newdoc->need_highlighting, GET_BIT(newdoc->view_bars,VIEW_COLORIZED), GET_BIT(main_v->props.view_bars,VIEW_COLORIZED) );
#endif /* DONE_IN_GUI_SET_DOCUMENT_WIDGETS */
	/*
		these lines should not be here since notebook_changed() calls flush_queue()
		that means that this document can be closed during notebook_changed(), and functions like open_file 
		rely on the fact that this function returns an existing document (and not a closed one!!)
	if (!delay_activate) {
			DEBUG_MSG("doc_new, notebook current page=%d, newdoc is on page %d\n",gtk_notebook_get_current_page(GTK_NOTEBOOK(main_v->notebook)),gtk_notebook_page_num(GTK_NOTEBOOK(main_v->notebook),scroll));
			DEBUG_MSG("doc_new, setting notebook page to %d\n", g_list_length(main_v->documentlist) - 1);
			gtk_notebook_set_current_page(GTK_NOTEBOOK(main_v->notebook),g_list_length(main_v->documentlist) - 1);
			if (bfwin->current_document != newdoc) {
				notebook_changed(-1);
			}*/ 
	/*		doc_activate() will be called by notebook_changed() and it will grab the focus
			gtk_widget_grab_focus(newdoc->view);	*/ 
	/*	}*/
	return newdoc;
}

/**
* doc_new_with_new_file:
* @bfwin: #Tbfwin*
* @new_filename: #gchar* filename to give document.
*
* Create a new document, name it by new_filename, and create the file.
*
* Return value: void
**/
void doc_new_with_new_file( Tbfwin *bfwin, gchar * new_filename )
{
	Tdocument * doc;
	Tfiletype *ft;
	if ( new_filename == NULL ) {
		statusbar_message( bfwin, _( "No filename" ), 2 );
		return ;
	}
	if ( !(main_v->props.view_bars & MODE_ALLOW_MULTIPLE_INSTANCE)) {
		gboolean res;
		res = switch_to_document_by_filename( bfwin, new_filename );
		if ( res ) {
			return ;
		}
	}
	DEBUG_MSG( "doc_new_with_new_file, new_filename=%s\n", new_filename );
	add_filename_to_history( bfwin, new_filename );
	doc = doc_new( bfwin, FALSE );
	doc->filename = g_strdup( new_filename );
	if ( bfwin->project && bfwin->project->template && strlen( bfwin->project->template ) >
		2 ) {
		doc_file_to_textbox( doc, bfwin->project->template , FALSE, FALSE )
		;
	}
	/* may be related to BUG#93
	(doc_file_to_textbox) does the hilight if found ft->hilight;
	why the have to check this after?
	*/
	ft = get_filetype_by_filename_and_content( doc->filename, NULL );
	if ( ft ) doc->hl = ft;
	/*	doc->modified = 1;*/
	doc_set_title( doc );
	doc_save( doc, FALSE, FALSE, FALSE );
	doc_set_stat_info( doc ); /* also sets mtime field */
	switch_to_document_by_pointer( bfwin, doc );
	doc_activate( doc );
}

/**
* doc_new_with_file:
* @bfwin: #Tbfwin* with the window to open the document in
* @filename: #gchar* with filename to load.
* @delay_activate: #gboolean if GUI calls are wanted.
* @move_to_this_win: #gboolean if the file should be moved to this window if already open
*
* Create a new document and read in a file.
* Errors are not propagated to user in any other way than returning a pointer or NULL
*
* Return value: #Tdocument*, or NULL on error
**/
Tdocument * doc_new_with_file( Tbfwin *bfwin, gchar * filename, gboolean delay_activate, gboolean move_to_this_win )
{
	Tdocument * doc;
	gboolean opening_in_existing_doc = FALSE;
	gchar *fullfilename;
	DEBUG_MSG( "doc_new_with_file, called for %s\n", filename );
	if ( ( filename == NULL ) || ( !file_exists_and_readable( filename ) ) ) {
		DEBUG_MSG( "doc_new_with_file, file %s !file_exists or readable\n", filename );
		return NULL;
	}
	fullfilename = create_full_path( filename, NULL );
	if ( bfwin ) {
		gchar * tmpstring = g_path_get_dirname( fullfilename );
		if ( bfwin->session->opendir )
			g_free( bfwin->session->opendir );
		bfwin->session->opendir = tmpstring;
	}
	/* BUG#78 */
	if ( !(main_v->props.view_bars & MODE_ALLOW_MULTIPLE_INSTANCE) ) {
		GList * alldocs = return_allwindows_documentlist();
		Tdocument *tmpdoc = documentlist_return_document_from_filename( alldocs, fullfilename );
		DEBUG_MSG( "doc_new_with_file, fullfilename=%s, tmpdoc=%p\n", fullfilename, tmpdoc );
		g_list_free( alldocs );
		if ( tmpdoc ) {
			DEBUG_MSG( "doc_new_with_file, %s is already open %p\n", filename, tmpdoc );
			if ( move_to_this_win && documentlist_return_document_from_filename( bfwin->documentlist, fullfilename ) == NULL ) {
				doc_move_to_window( tmpdoc, bfwin );
			} else {
				if ( !delay_activate ) {
					switch_to_document_by_pointer( BFWIN( tmpdoc->bfwin ), tmpdoc );
					/* related to BUG#44 */
					/* if ( bfwin != tmpdoc->bfwin ) { */
						gtk_window_present( GTK_WINDOW( BFWIN( tmpdoc->bfwin ) ->main_window ) );
					/*}*/
				}
			}
			g_free( fullfilename );
			return tmpdoc;
		}
	}
	DEBUG_MSG( "doc_new_with_file, fullfilename=%s, filename=%s\n", fullfilename, filename );
	add_filename_to_history( bfwin, fullfilename );

	if ( /*g_list_length( bfwin->documentlist ) == 1 && */doc_is_empty_non_modified_and_nameless( bfwin->current_document ) ) {
		doc = bfwin->current_document;
		opening_in_existing_doc = TRUE;
		bfwin->last_activated_doc = NULL;
	} else {
		doc = doc_new( bfwin, delay_activate );
	}
	/* we do not need to free fullfilename anymore now */
	doc->filename = fullfilename;
	DEBUG_MSG( "doc_new_with_file, hl is resetted to filename, about to load file\n" );
	doc_file_to_textbox( doc, doc->filename, FALSE, delay_activate );
	/* after the textbuffer is filled the filetype can be found */
	doc_reset_filetype( doc, doc->filename, NULL );

	/* hey, this should be done by doc_activate
	menu_current_document_set_toggle_wo_activate(NULL, doc->encoding);*/
	doc_set_stat_info( doc ); /* also sets mtime field */
	doc_set_title( doc ); /* sets the tooltip as well, so it should be called *after* doc_set_stat_info() */
	if ( !delay_activate ) {
		if ( opening_in_existing_doc ) {
			doc_activate( doc );
		}
		switch_to_document_by_pointer( bfwin, doc );
		doc_activate( doc );
		/*filebrowser_open_dir(BFWIN(doc->bfwin),fullfilename); is already called by doc_activate() */
	}
	bmark_set_for_doc( doc );
	bmark_check_length( bfwin, doc );
	/*	bmark_adjust_visible(bfwin);   */

	return doc;
}

/**
* docs_new_from_files:
* @bfwin: #Tbfwin* with the window to open the document in
* @file_list: #GList with filenames to open.
* @move_to_this_win: #gboolean if the file needs to be moved to this window if it is open already
*
* Open a number of new documents from files in stringlist file_list.
* If a file is open already in another window, it might be moved to this window, else
* nothing is done for this file
* Report files with problems to user.
* If more than 8 files are opened at once, a modal progressbar is shown while loading.
*
* Return value: void
**/
void docs_new_from_files( Tbfwin *bfwin, GList * file_list, gboolean move_to_this_win, gint linenumber )
{
	GList * tmplist, *errorlist = NULL;
	gboolean delay = ( g_list_length( file_list ) > 1 );
	gpointer pbar = NULL;
	gint i = 0;
	gint num_files_opened=0;
	DEBUG_MSG( "docs_new_from_files, lenght=%d\n", g_list_length( file_list ) );

	/* Hide the notebook and show a progressbar while
	* adding several files. */
	if ( g_list_length( file_list ) > 8 ) {
		notebook_hide( bfwin );
		pbar = progress_popup( bfwin->main_window, _( "Loading files..." ), g_list_length( file_list ) );
	}

	tmplist = g_list_first( file_list );
	Tdocument *tmpdoc = NULL;
	while ( tmplist ) {
		DEBUG_MSG( "docs_new_from_files, about to open %s, delay=%d\n", ( gchar * ) tmplist->data, delay );
		tmpdoc = doc_new_with_file( bfwin, ( gchar * ) tmplist->data, delay, move_to_this_win );
		if ( !tmpdoc ) {
			errorlist = g_list_append( errorlist, g_strdup( ( gchar * ) tmplist->data ) );
		} else {
			num_files_opened ++;
			if (linenumber >=0) {
				doc_activate( tmpdoc );
				doc_select_line( tmpdoc, linenumber, TRUE );
				gtk_widget_grab_focus( GTK_WIDGET( tmpdoc->view ) );
			}
		}
		if ( pbar ) {
			progress_set( pbar, ++i );
			flush_queue();
		}
		tmplist = g_list_next( tmplist );
	}
	if ( errorlist ) {
		gchar * message, *tmp;
		tmp = stringlist_to_string( errorlist, "\n" );
		message = g_strconcat( _( "These files could not opened:\n\n" ), tmp, NULL );
		g_free( tmp );
		warning_dialog( bfwin->main_window, _( "Unable to open file(s)\n" ), message );
		g_free( message );
	}
	free_stringlist( errorlist );
	
	if ( num_files_opened >= 1 /*delay */) {
		DEBUG_MSG( "since we delayed the highlighting, we set the notebook and filebrowser page now\n" );

		/* Destroy the progressbar and show the notebook when finished. */
		progress_destroy( pbar );
		notebook_show( bfwin );

		gtk_notebook_set_page( GTK_NOTEBOOK( bfwin->notebook ), g_list_length( bfwin->documentlist ) - 1 );
		notebook_changed( bfwin, -1 );
		/* num_files_opened so we not not to check...
		if ( bfwin->current_document && bfwin->current_document->filename ) {
		*/
			/*filebrowser_open_dir(bfwin,bfwin->current_document->filename); is called by doc_activate() */
		doc_activate( bfwin->current_document );
		/*}*/
		gui_set_title( bfwin, bfwin->current_document );
		if (num_files_opened ==1) {
			DEBUG_MSG("docs_new_from_files: num_files_opened =%d\n", num_files_opened);
			if (DOCUMENT(bfwin->current_document)->need_highlighting) {
				doc_highlight_full(bfwin->current_document);
			}
		}
	}
	/* related to BUG#93 */
#ifdef CODE_MOVED_UP
	if (bfwin->current_document) {
		gui_set_title( bfwin, bfwin->current_document );
	}
#endif /* CODE_MOVED_UP */
}

/**
* doc_reload:
* @doc: a #Tdocument
*
* Revert to file on disk.
*
* Return value: void
**/
void doc_reload( Tdocument *doc )
{
	if ( ( doc->filename == NULL ) || ( !file_exists_and_readable( doc->filename ) ) ) {
		statusbar_message( BFWIN( doc->bfwin ), _( "unable to open file" ), 2000 );
		return ;
	}
	gint lineindex;
	{
		GtkTextIter itstart, itend, itercur;
		gtk_text_buffer_get_selection_bounds(doc->buffer, &itercur, NULL);
		lineindex = gtk_text_iter_get_line(&itercur);
		DEBUG_MSG("doc_reload: current lineindex %d\n", lineindex);
		gtk_text_buffer_get_bounds( doc->buffer, &itstart, &itend );
		gtk_text_buffer_delete( doc->buffer, &itstart, &itend );
	}

	doc_file_to_textbox( doc, doc->filename, FALSE, FALSE );
	doc_unre_clear_all( doc );
	doc_set_modified( doc, 0 );
	doc_set_stat_info( doc ); /* also sets mtime field */
	doc_select_line(doc, lineindex, TRUE);
}

/**
* doc_activate:
* @doc: a #Tdocument
*
* Perform actions neccessary when a document is focused. I.e. called from the notebook.
*
* Show textview, warn if the file on disk has been changed,
* update line-numbers etc and highlighting.
*
* Return value: void
**/
void doc_activate( Tdocument *doc )
{
	gboolean modified;
	time_t oldmtime, newmtime;
#ifdef DEBUG

	if ( !doc ) {
		DEBUG_MSG( "doc_activate, doc=NULL!!! ABORTING!!\n" );
		exit( 44 );
	}
#endif
	if ( doc == NULL || doc == BFWIN( doc->bfwin ) ->last_activated_doc ) {
		return ;
	}
	BFWIN( doc->bfwin ) ->last_activated_doc = doc;
	gtk_widget_show( doc->view ); /* This might be the first time this document is activated. */
	{
		struct stat statbuf;
		modified = doc_check_modified_on_disk( doc, &statbuf );
		newmtime = statbuf.st_mtime;
		oldmtime = doc->statbuf.st_mtime;
	}
	if ( modified ) {
		gchar * tmpstr, oldtimestr[ 128 ], newtimestr[ 128 ]; /* according to 'man ctime_r' this should be at least 26, so 128 should do ;-)*/
		gint retval;
		gchar *options[] = {_( "_Reload" ), _( "_Ignore" ), NULL};

		ctime_r( &newmtime, newtimestr );
		ctime_r( &oldmtime, oldtimestr );
		tmpstr = g_strdup_printf( _( "Filename: %s\n\nNew modification time is: %s\nOld modification time is: %s" ), doc->filename, newtimestr, oldtimestr );
		retval = multi_warning_dialog( BFWIN( doc->bfwin ) ->main_window, _( "File has been modified by another process\n" ), tmpstr, 0, 1, options );
		g_free( tmpstr );
		if ( retval == 1 ) {
			doc_set_stat_info( doc );
		} else {
			doc_reload( doc );
		}
	}
	DEBUG_MSG( "doc_activate, calling gui_set_document_widgets()\n" );
	gui_set_document_widgets( doc );
	gui_set_title( BFWIN( doc->bfwin ), doc );
	doc_set_statusbar_lncol( doc );
	doc_set_statusbar_insovr( doc );
	doc_set_statusbar_editmode_encoding( doc );

	/* if highlighting is needed for this document do this now !! */
	if ( doc->need_highlighting && (doc->view_bars & VIEW_COLORIZED) ) {
		doc_highlight_full( doc );
		DEBUG_MSG( "doc_activate, doc=%p, after doc_highlight_full, need_highlighting=%d\n", doc, doc->need_highlighting );
	}

	/*	doc_scroll_to_cursor(doc);*/
	if ( doc->filename ) {
		gchar * dir1 = g_path_get_dirname( doc->filename );
		gchar *dir2 = ending_slash( dir1 );
		if ( dir2[ 0 ] == '/' ) {
			chdir( dir2 );
		}
		if ( main_v->props.filebrowser_focus_follow ) {
			DEBUG_MSG( "doc_activate, call filebrowser_open_dir() for %s\n", dir2 );
			filebrowser_open_dir( BFWIN( doc->bfwin ), dir2 );
		}
		g_free( dir1 );
		g_free( dir2 );
	}
	DEBUG_MSG( "doc_activate, doc=%p, about to grab focus\n", doc );
	gtk_widget_grab_focus( GTK_WIDGET( doc->view ) );

	DEBUG_MSG( "doc_activate, doc=%p, finished\n", doc );
}

void doc_force_activate( Tdocument *doc )
{
	BFWIN( doc->bfwin ) ->last_activated_doc = NULL;
	doc_activate( doc );
}



void file_open_from_selection( Tbfwin *bfwin )
{
	gchar * string;
	GtkClipboard* cb;

	cb = gtk_clipboard_get( GDK_SELECTION_PRIMARY );
	string = gtk_clipboard_wait_for_text( cb );
	if ( string ) {
		DEBUG_MSG( "file_open_from_selection, opening %s\n", string );
		if ( NULL == strchr( string, '/' ) && bfwin->current_document->filename ) {
			/* now we should look in the directory of the current file */
			gchar * dir, *tmp;
			dir = g_path_get_dirname( bfwin->current_document->filename );
			tmp = g_strconcat( dir, "/", string, NULL );
			DEBUG_MSG( "file_open_from_selection, trying %s\n", tmp );
			doc_new_with_file( bfwin, tmp, FALSE, FALSE );
			g_free( dir );
			g_free( tmp );
		} else {
			doc_new_with_file( bfwin, string, FALSE, FALSE );
		}
		g_free( string );
	}
}

/**
* file_save_cb:
* @widget: unused #GtkWidget
* @bfwin: #Tbfwin* with the current window
*
* Save the current document.
*
* Return value: void
**/
void file_save_cb( GtkWidget * widget, Tbfwin *bfwin )
{
	doc_save( bfwin->current_document, FALSE, FALSE, FALSE );
}

/**
* file_save_as_cb:
* @widget: unused #GtkWidget
* @bfwin: #Tbfwin* with the current window
*
* Save current document, let user choose filename.
*
* Return value: void
**/
void file_save_as_cb( GtkWidget * widget, Tbfwin *bfwin )
{
	doc_save( bfwin->current_document, TRUE, FALSE, FALSE );
}

/**
* file_move_to_cb:
* @widget: unused #GtkWidget
* @bfwin: #Tbfwin* with the current window
*
* Move current document, let user choose filename.
*
* Return value: void
**/
void file_move_to_cb( GtkWidget * widget, Tbfwin *bfwin )
{
	doc_save( bfwin->current_document, TRUE, TRUE, FALSE );
}

/**
* file_open_cb:
* @widget: unused #GtkWidget
* @bfwin: #Tbfwin* with the current window
*
* Prompt user for files to open.
*
* Return value: void
**/
void file_open_cb( GtkWidget * widget, Tbfwin *bfwin )
{
	GList * tmplist = NULL;
	DEBUG_MSG( "file_open_cb, started, calling return_files()\n" );
#ifdef HAVE_ATLEAST_GTK_2_4

	{
		GtkWidget *dialog;
		GSList *slist;
		dialog = file_chooser_dialog( bfwin, _( "Select files" ), GTK_FILE_CHOOSER_ACTION_OPEN, NULL, TRUE, TRUE, NULL );
		if ( gtk_dialog_run ( GTK_DIALOG ( dialog ) ) == GTK_RESPONSE_ACCEPT )
		{
			slist = gtk_file_chooser_get_filenames( GTK_FILE_CHOOSER( dialog ) );
			tmplist = glist_from_gslist( slist );
			g_slist_free( slist );
		}
		gtk_widget_destroy( dialog );
	}
#else
	tmplist = return_files( NULL );
#endif

	if ( !tmplist ) {
		return ;
	}
	{
		gint len = g_list_length( tmplist );
		gchar *message = g_strdup_printf( _( "Loading %d file(s)..." ), len );
		statusbar_message( bfwin, message, 2000 + len * 50 );
		g_free( message );
		flush_queue();
	}
	DEBUG_MSG( "file_open_cb, calling docs_new_from_files()\n" );
	docs_new_from_files( bfwin, tmplist, FALSE, -1 );
	free_stringlist( tmplist );
}

/**
* file_insert_menucb:
* @bfwin: Tbfwin* which window
* @callback_action: unused #guint
* @widget: #GtkWidget* unused
*
* Prompt user for a file, and insert the contents into the current document.
*
* Return value: void
**/
void file_insert_menucb( Tbfwin *bfwin, guint callback_action, GtkWidget *widget )
{
	gchar * tmpfilename = NULL;
#ifdef HAVE_ATLEAST_GTK_2_4

	{
		GtkWidget *dialog;
		dialog = file_chooser_dialog( bfwin, _( "Select file to insert" ), GTK_FILE_CHOOSER_ACTION_OPEN, NULL, TRUE, FALSE, NULL );
		if ( gtk_dialog_run( GTK_DIALOG( dialog ) ) == GTK_RESPONSE_ACCEPT )
		{
			tmpfilename = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER( dialog ) );
		}
		gtk_widget_destroy( dialog );
	}
#else
	tmpfilename = return_file_w_title( NULL, _( "Select file to insert" ) );
#endif

	if ( tmpfilename == NULL ) {
		statusbar_message( bfwin, _( "no file to insert" ), 2000 );
		return ;
	} else {
		/* do we need to set the insert point in some way ?? */
		doc_file_to_textbox( bfwin->current_document, tmpfilename, TRUE, FALSE );
		g_free( tmpfilename );
		doc_set_modified( bfwin->current_document, 1 );
	}
}

/**
* file_new_cb:
* @windget: #GtkWidget* ignored
* @bfwin: Tbfwin* where to open the new document
*
* Create a new, empty file in window bfwin
*
* Return value: void
**/
void file_new_cb( GtkWidget *widget, Tbfwin *bfwin )
{
	DEBUG_MSG("file_new_cb: hello world\n");
	Tdocument * doc;
	doc = doc_new( bfwin, FALSE );
	switch_to_document_by_pointer( bfwin, doc );
	if ( bfwin->project && bfwin->project->template && strlen( bfwin->project->template ) >
		2 ) {
		doc_file_to_textbox( doc, bfwin->project->template , FALSE, FALSE )
		;
		doc_activate( doc );
	}
}

/**
* file_close_cb:
* @widget: unused #GtkWidget
* @data: unused #gpointer
*
* Close the current document.
*
* Return value: void
**/
void file_close_cb( GtkWidget * widget, Tbfwin *bfwin )
{
	doc_close( bfwin->current_document, 0 );
}

void bfwin_close_all_documents( Tbfwin *bfwin, gboolean window_closing )
{
	GList * tmplist;
	Tdocument *tmpdoc;
	gint retval = -1;

	DEBUG_MSG( "file_close_all_cb, started\n" );

	/* first a warning loop */
	if ( test_docs_modified( bfwin->documentlist ) ) {
		if ( g_list_length ( bfwin->documentlist ) > 1 ) {
			gchar * options[] = {_( "_Save All" ), _( "Close _All" ), _( "Choose per _File" ), _( "_Cancel" ), NULL};
			retval = multi_query_dialog( bfwin->main_window, _( "Multiple open files have been changed." ),
						_( "If you don't save your changes they will be lost." ), 3, 3, options );
			if ( retval == 3 ) {
				DEBUG_MSG( "file_close_all_cb, cancel clicked, returning 0\n" );
				return ;
			}
		} else {
			retval = 2;
		}
	} else {
		retval = 1;
	}
	DEBUG_MSG( "file_close_all_cb, after the warnings, retval=%d, now close all the windows\n", retval );

	tmplist = g_list_first( bfwin->documentlist );
	while ( tmplist ) {
		tmpdoc = ( Tdocument * ) tmplist->data;
		if ( test_only_empty_doc_left( bfwin->documentlist ) ) {
			return ;
		}

		switch ( retval ) {
		case 0:
			doc_save( tmpdoc, FALSE, FALSE, window_closing );
			if ( !tmpdoc->modified ) {
				doc_destroy( tmpdoc, TRUE );
			} else {
				return ;
			}
			tmplist = g_list_first( bfwin->documentlist );
			break;
		case 1:
			doc_destroy( tmpdoc, TRUE );
			tmplist = g_list_first( bfwin->documentlist );
			break;
		case 2:
			if ( doc_close( tmpdoc, 0 ) != 2 ) {
				tmplist = g_list_first( bfwin->documentlist );
			} else {
				/* notebook_changed();*/
				return ;
			}
			break;
		default:
			/* notebook_changed();*/
			return ;
			break;
		}
	}
	notebook_changed( bfwin, -1 );
	DEBUG_MSG( "file_close_all_cb, finished\n" );
}

/**
* file_close_all_cb:
* @widget: unused #GtkWidget
* @bfwin: #Tbfwin* 
*
* Close all open files. Prompt user when neccessary.
*
* Return value: void
**/
void file_close_all_cb( GtkWidget * widget, Tbfwin *bfwin )
{
	bfwin_close_all_documents( bfwin, FALSE );
}


/**
* file_save_all_cb:
* @widget: unused #GtkWidget
* @data: unused #gpointer
*
* 	Save all editor notebooks
*
* Return value: void
**/
void file_save_all_cb( GtkWidget * widget, Tbfwin *bfwin )
{

	GList * tmplist;
	Tdocument *tmpdoc;

	tmplist = g_list_first( bfwin->documentlist );
	while ( tmplist ) {
		tmpdoc = ( Tdocument * ) tmplist->data;
		if ( tmpdoc->modified ) {
			doc_save( tmpdoc, FALSE, FALSE, FALSE );
		}
		tmplist = g_list_next( tmplist );
	}
}

/**
* edit_cut_cb:
* @widget: unused #GtkWidget
* @data: unused #gpointer
*
* 	Cut selection from current buffer, to clipboard.
*
* Return value: void
**/
void edit_cut_cb( GtkWidget * widget, Tbfwin *bfwin )
{
	doc_unre_new_group( bfwin->current_document );
	gtk_text_buffer_cut_clipboard( bfwin->current_document->buffer, gtk_clipboard_get( GDK_SELECTION_CLIPBOARD ), TRUE );
	doc_unre_new_group( bfwin->current_document );
}

/**
* edit_copy_cb:
* @widget: unused #GtkWidget
* @data: unused #gpointer
*
* 	Copy selection from current buffer, to clipboard.
*
* Return value: void
**/
void edit_copy_cb( GtkWidget * widget, Tbfwin *bfwin )
{
	gtk_text_buffer_copy_clipboard( bfwin->current_document->buffer, gtk_clipboard_get( GDK_SELECTION_CLIPBOARD ) );
}

/**
* edit_paste_cb:
* @widget: unused #GtkWidget
* @data: unused #gpointer
*
* 	Paste contents of clipboard. Disable highlighting while pasting, for speed.
*
* Return value: void
**/
void edit_paste_cb( GtkWidget * widget, Tbfwin *bfwin )
{
	GtkTextMark * mark;
	GtkTextIter itstart, itend;
	gint eo_so_diff;

	Tdocument *doc = bfwin->current_document;
	DEBUG_MSG( "edit_paste_cb, started\n" );
	if ( !doc->paste_operation ) {
		doc->paste_operation = g_new( Tpasteoperation, 1 );
		PASTEOPERATION( doc->paste_operation ) ->so = -1;
		PASTEOPERATION( doc->paste_operation ) ->eo = -1;
	}
	doc_unre_new_group( doc );

	DEBUG_MSG( "edit_paste_cb, pasting clipboard\n" );
	gtk_text_buffer_paste_clipboard ( doc->buffer, gtk_clipboard_get( GDK_SELECTION_CLIPBOARD ), NULL, TRUE );

	doc_unre_new_group( doc );
	
	eo_so_diff = PASTEOPERATION( doc->paste_operation ) ->eo - PASTEOPERATION( doc->paste_operation ) ->so;
	if ( eo_so_diff >0 ) {
		/* BUG#80 */
		if (doc->view_bars & VIEW_COLORIZED) {
			DEBUG_MSG( "edit_paste_cb, start doc_highlight_region for so=%d, eo=%d\n", PASTEOPERATION( doc->paste_operation ) ->so, PASTEOPERATION( doc->paste_operation ) ->eo );
			doc_highlight_region( doc, PASTEOPERATION( doc->paste_operation ) ->so, PASTEOPERATION( doc->paste_operation ) ->eo );
		}else{
			/* removed all tags ;) */
			gtk_text_buffer_get_bounds(doc->buffer, &itstart, &itend);
			gtk_text_buffer_remove_all_tags(doc->buffer, &itstart, &itend);
		}
	}
	g_free( doc->paste_operation );
	doc->paste_operation = NULL;

	mark = gtk_text_buffer_get_insert( doc->buffer );
	gtk_text_view_scroll_mark_onscreen( GTK_TEXT_VIEW( bfwin->current_document->view ), mark );
	/* BUGS#88 */
	if ( eo_so_diff ==1 ) {
		gtk_text_buffer_get_iter_at_mark(doc->buffer, &itstart, mark);
		itend = itstart;
		gtk_text_iter_backward_char(&itend);
		gtk_text_buffer_remove_tag(doc->buffer, BRACEFINDER(doc->brace_finder)->tag , &itstart, &itend);
	}
	DEBUG_MSG( "edit_paste_cb, finished\n" );
}

/**
* edit_select_all_cb:
* @widget: unused #GtkWidget
* @data: unused #gpointer
*
* Mark entire current document as selected.
*
* Return value: void
**/
void edit_select_all_cb( GtkWidget * widget, Tbfwin *bfwin )
{
	GtkTextIter itstart, itend;
	gtk_text_buffer_get_bounds( bfwin->current_document->buffer, &itstart, &itend );
	gtk_text_buffer_move_mark_by_name( bfwin->current_document->buffer, "insert", &itstart );
	gtk_text_buffer_move_mark_by_name( bfwin->current_document->buffer, "selection_bound", &itend );
}

/**
* doc_toggle_highlighting_cb:
* @callback_data: unused #gpointer
* @action: unused #guint
* @widget: unused #GtkWidget*
*
* Toggle highlighting on/off for current document.
*
* Return value: void
**/
void doc_toggle_highlighting_cb( Tbfwin *bfwin, guint action, GtkWidget *widget )
{
	bfwin->current_document->view_bars = SET_BIT(bfwin->current_document->view_bars, VIEW_COLORIZED, !(GET_BIT(bfwin->current_document->view_bars,VIEW_COLORIZED)));
	DEBUG_MSG( "doc_toggle_highlighting_cb, started, highlightstate now is %d\n", bfwin->current_document->view_bars &VIEW_COLORIZED );
	if ( !(bfwin->current_document->view_bars &VIEW_COLORIZED) ) {
		doc_remove_highlighting( bfwin->current_document );
	} else {
		doc_highlight_full( bfwin->current_document );
	}
}

/**
* all_documents_apply_settings:
*
* applies changes from the preferences to all documents
*
* Return value: void
*/
void all_documents_apply_settings()
{
	GList * tmplist = g_list_first( return_allwindows_documentlist() );
	while ( tmplist ) {
		Tdocument * doc = tmplist->data;
		doc_set_tabsize( doc, main_v->props.editor_tab_width );
		doc_set_font( doc, main_v->props.editor_font_string );
		tmplist = g_list_next( tmplist );
	}

}

/**
* doc_convert_asciichars_in_selection:
* @callback_data: unused #gpointer
* @callback_action: #guint type of chars to change
* @widget: unused #GtkWidget*
*
* Convert characters in current document to entities.
* callback_action set to 1 (only ascii), 2 (only iso) or 3 (both).
* or 4 (ToUppercase) or 5 (ToLowercase)
*
* Return value: void
**/
void doc_convert_asciichars_in_selection( Tbfwin *bfwin, guint callback_action, GtkWidget *widget )
{
	if (callback_action < 4) {
		doc_convert_case_in_selection( bfwin->current_document, callback_action );
	}
	/* kyanh, removed, 20050303, */
	else {
		doc_convert_chars_to_entities_in_selection(bfwin->current_document, (callback_action &4), (callback_action &8 ));
	}
}

/**
* doc_toggle_highlighting_cb:
* @callback_data: unused #gpointer
* @action: unused #guint
* @widget: unused #GtkWidget*
*
* Show word-, line- and charcount for current document in the statusbar.
* Note: The wordcount() call returns number of actual utf8-chars, not bytes.
*
* Return value: void
**/
void word_count_cb ( Tbfwin *bfwin, guint callback_action, GtkWidget *widget )
{
	guint chars = 0, lines = 0, words = 0;
	gchar *allchars, *wc_message;

	allchars = doc_get_chars( bfwin->current_document, 0, -1 );
	wordcount( allchars, &chars, &lines, &words );
	g_free( allchars );

	wc_message = g_strdup_printf( _( "Statistics: %d lines, %d words, %d characters" ), lines, words, chars );
	statusbar_message ( bfwin, wc_message, 5000 );
	g_free ( wc_message );
}

/**
* doc_toggle_highlighting_cb:
* @doc: a #Tdocument*
* @unindent: #gboolean
*
* Indent the selected block in current document.
* Set unindent to TRUE to unindent.
*
* Return value: void
**/
void doc_indent_selection( Tdocument *doc, gboolean unindent )
{
	GtkTextIter itstart, itend;
	if ( gtk_text_buffer_get_selection_bounds( doc->buffer, &itstart, &itend ) ) {
		GtkTextMark * end;
		/*		gboolean firstrun=TRUE;*/

		doc_unbind_signals( doc );
		doc_unre_new_group( doc );
		/* we have a selection, now we loop trough the characters, and for every newline
		we add or remove a tab, we set the end with a mark */
		end = gtk_text_buffer_create_mark( doc->buffer, NULL, &itend, TRUE );
		if ( gtk_text_iter_get_line_offset( &itstart ) > 0 ) {
			gtk_text_iter_set_line_index( &itstart, 0 );
		}
		while ( gtk_text_iter_compare( &itstart, &itend ) < 0 ) {
			GtkTextMark * cur;
			/*			if (firstrun && !gtk_text_iter_starts_line(&itstart)) {
							gtk_text_iter_forward_line(&itstart);
						}
						firstrun = FALSE;*/
			cur = gtk_text_buffer_create_mark( doc->buffer, NULL, &itstart, TRUE );
			if ( unindent ) {
				/* when unindenting we try to set itend to the end of the indenting step
				which might be a tab or 'tabsize' spaces, then we delete that part */
				gboolean cont = TRUE;
				gchar *buf = NULL;
				gunichar cchar = gtk_text_iter_get_char( &itstart );
				if ( cchar == 9 ) { /* 9 is ascii for tab */
					itend = itstart;
					cont = gtk_text_iter_forward_char( &itend );
					buf = g_strdup( "\t" );
				} else if ( cchar == 32 ) { /* 32 is ascii for space */
					gint i = 0;
					itend = itstart;
					gtk_text_iter_forward_chars( &itend, main_v->props.editor_tab_width );
					buf = gtk_text_buffer_get_text( doc->buffer, &itstart, &itend, FALSE );
					DEBUG_MSG( "tab_width=%d, strlen(buf)=%d, buf='%s'\n", main_v->props.editor_tab_width, strlen( buf ), buf );
					while ( cont && buf[ i ] != '\0' ) {
						cont = ( buf[ i ] == ' ' );
						DEBUG_MSG( "doc_indent_selection, buf[%d]='%c'\n", i, buf[ i ] );
						i++;
					}
					if ( !cont ) {
						g_free ( buf );
					}
				} else {
					cont = FALSE;
				}
				if ( cont ) {
					gint offsetstart, offsetend;
					offsetstart = gtk_text_iter_get_offset( &itstart );
					offsetend = gtk_text_iter_get_offset( &itend );
					gtk_text_buffer_delete( doc->buffer, &itstart, &itend );
					doc_unre_add( doc, buf, offsetstart, offsetend, UndoDelete );
					g_free ( buf );
				}
#ifdef DEBUG
				else {
					DEBUG_MSG( "doc_indent_selection, NOT continue!!\n" );
				}
#endif

			} else { /* indent */
				gint offsetstart = gtk_text_iter_get_offset( &itstart );
				gchar *indentstring;
				gint indentlen;
				if ( main_v->props.view_bars & MODE_INDENT_WITH_SPACES ) {
					indentstring = bf_str_repeat( " ", main_v->props.editor_tab_width );
					indentlen = main_v->props.editor_tab_width;
				} else {
					indentstring = g_strdup( "\t" );
					indentlen = 1;
				}
				gtk_text_buffer_insert( doc->buffer, &itstart, indentstring, indentlen );
				doc_unre_add( doc, indentstring, offsetstart, offsetstart + indentlen, UndoInsert );
				g_free( indentstring );
			}
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &itstart, cur );
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &itend, end );
			gtk_text_buffer_delete_mark( doc->buffer, cur );
			gtk_text_iter_forward_line( &itstart );
			DEBUG_MSG( "doc_indent_selection, itstart at %d, itend at %d\n", gtk_text_iter_get_offset( &itstart ), gtk_text_iter_get_offset( &itend ) );
		}
		gtk_text_buffer_delete_mark( doc->buffer, end );
		doc_bind_signals( doc );
		doc_set_modified( doc, 1 );
	} else {
		/* there is no selection, work on the current line */
		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, gtk_text_buffer_get_insert( doc->buffer ) );
		gtk_text_iter_set_line_offset( &iter, 0 );
		if ( unindent ) {
			gint deletelen = 0;
			gchar *tmpstr, *tmp2str;
			GtkTextIter itend = iter;
			gtk_text_iter_forward_chars( &itend, main_v->props.editor_tab_width );
			tmpstr = gtk_text_buffer_get_text( doc->buffer, &iter, &itend, FALSE );
			tmp2str = bf_str_repeat( " ", main_v->props.editor_tab_width );
			if ( tmpstr[ 0 ] == '\t' ) {
				deletelen = 1;
			} else if ( tmpstr && strncmp( tmpstr, tmp2str, main_v->props.editor_tab_width ) == 0 ) {
				deletelen = main_v->props.editor_tab_width;
			}
			g_free( tmpstr );
			g_free( tmp2str );
			if ( deletelen ) {
				itend = iter;
				gtk_text_iter_forward_chars( &itend, deletelen );
				gtk_text_buffer_delete( doc->buffer, &iter, &itend );
			}
		} else { /* indent */
			gchar *indentstring;
			gint indentlen;
			if ( main_v->props.view_bars & MODE_INDENT_WITH_SPACES ) {
				indentstring = bf_str_repeat( " ", main_v->props.editor_tab_width );
				indentlen = main_v->props.editor_tab_width;
			} else {
				indentstring = g_strdup( "\t" );
				indentlen = 1;
			}
			gtk_text_buffer_insert( doc->buffer, &iter, indentstring, indentlen );
			g_free( indentstring );
		}
	}
}

void menu_indent_cb( Tbfwin *bfwin, guint callback_action, GtkWidget *widget )
{
	if ( bfwin->current_document ) {
		doc_indent_selection( bfwin->current_document, ( callback_action == 1 ) );
	}
}

/**
* list_relative_document_filenames:
* @curdoc: #Tdocument: the current document
*
* this function will generate a stringlist with a relative links to 
* all other open documents. This list should be freed using free_stringlist()
*
* Return value: #GList with strings
*/
GList *list_relative_document_filenames( Tdocument *curdoc )
{
	GList * tmplist, *retlist = NULL;
	if ( curdoc->filename == NULL ) {
		return NULL;
	}
	tmplist = g_list_first( BFWIN( curdoc->bfwin ) ->documentlist );
	while ( tmplist ) {
		Tdocument * tmpdoc = tmplist->data;
		if ( tmpdoc != curdoc && tmpdoc->filename != NULL ) {
			retlist = g_list_append( retlist, create_relative_link_to( curdoc->filename, tmpdoc->filename ) );
		}
		tmplist = g_list_next( tmplist );
	}
	return retlist;
}

static void floatingview_destroy_lcb( GtkWidget *widget, Tdocument *doc )
{
	DEBUG_MSG( "floatingview_destroy_lcb, called for doc=%p, doc->floatingview=%p\n", doc, doc->floatingview );
	if ( doc->floatingview ) {
		gtk_widget_destroy( FLOATINGVIEW( doc->floatingview ) ->window );
		g_free( doc->floatingview );
		doc->floatingview = NULL;
	}
}

static void new_floatingview( Tdocument *doc )
{
	Tfloatingview * fv;
	gchar *title;
	GtkWidget *scrolwin;
	if ( doc->floatingview ) {
		fv = FLOATINGVIEW( doc->floatingview );
		gtk_window_present( GTK_WINDOW( fv->window ) );
		return ;
	}
	fv = g_new( Tfloatingview, 1 );
	doc->floatingview = fv;
	DEBUG_MSG( "new_floatingview for doc=%p is at %p\n", doc, doc->floatingview );
	title = ( doc->filename ) ? doc->filename : "Untitled";
	fv->window = window_full2( title, GTK_WIN_POS_NONE, 5, G_CALLBACK( floatingview_destroy_lcb ), doc, TRUE, NULL );
	gtk_window_set_role( GTK_WINDOW( fv->window ), "floatingview" );
	fv->textview = gtk_text_view_new_with_buffer( doc->buffer );
	gtk_text_view_set_editable( GTK_TEXT_VIEW( fv->textview ), FALSE );
	gtk_text_view_set_cursor_visible( GTK_TEXT_VIEW( fv->textview ), FALSE );
	apply_font_style( fv->textview, main_v->props.editor_font_string );
	gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( fv->textview ), GTK_WRAP_WORD );
	scrolwin = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolwin ), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW( scrolwin ), fv->textview );
	gtk_container_add( GTK_CONTAINER( fv->window ), scrolwin );
	gtk_window_set_default_size( GTK_WINDOW( fv->window ), 600, 600 );
	gtk_widget_show_all( fv->window );
}

void file_floatingview_menu_cb( Tbfwin *bfwin, guint callback_action, GtkWidget *widget )
{
	new_floatingview( bfwin->current_document );
}

/* kyanh, added */
void menu_comment_cb( Tbfwin *bfwin, guint callback_action, GtkWidget *widget )
{
	if ( bfwin->current_document ) {
		doc_comment_selection( bfwin->current_document, ( callback_action == 1 ) );
	}
}

/* kyanh, added */
void doc_comment_selection( Tdocument *doc, gboolean uncomment )
{
	GtkTextIter itstart, itend;
	if ( gtk_text_buffer_get_selection_bounds( doc->buffer, &itstart, &itend ) ) {
		GtkTextMark * end;

		doc_unbind_signals( doc );
		doc_unre_new_group( doc );
		/* we have a selection, now we loop trough the characters, and for every newline
		we add or remove a tab, we set the end with a mark */
		end = gtk_text_buffer_create_mark( doc->buffer, NULL, &itend, TRUE );
		/* set to: the fist char of the fist line */
		if ( gtk_text_iter_get_line_offset( &itstart ) > 0 ) {
			gtk_text_iter_set_line_index( &itstart, 0 );
		}
		gint maxpre = 0; /* maximum number of stripped string from previous line*/
		if ( uncomment ) {
			gchar * tmpstr = gtk_text_buffer_get_text( doc->buffer, &itstart, &itend, FALSE );
			/* g_print("===========\nCurrent selection:[\n%s]\n",tmpstr); */
			/* TODO: donot use split; use gtk_text_iter_forward_line(&itstart); */
			gchar **tmpsplit = g_strsplit( tmpstr, "\n", 0 );
			gint nline = count_array( tmpsplit ); /* number of line */
			DEBUG_MSG( "Number of lines: %d\n", nline );
			gint i = 0, j = 0;
			/* TODO: for (i=0; tmpsplit[i]!=NULL; i++ ) */
			for ( i = 0; i < nline; i++ ) {
				/* g_print("Line %d: %s\n", i,tmpsplit[i]); */
				j = 0;
				while ( tmpsplit[ i ][ j ] != '\0' && ( tmpsplit[ i ][ j ] == '%' || tmpsplit[ i ][ j ] == ' ' || tmpsplit[ i ][ j ] == '\t' ) ) {
					j++;
				}
				/*g_print("May be remove %d chars\n", j); */
				if ( i ) { /* isNOT first line? */
					/* we check for the last char of line */
					if ( tmpsplit[ i ][ j ] != '\0' && tmpsplit[ i ][ j ] != '%' && tmpsplit[ i ][ j ] != ' ' && tmpsplit[ i ][ j ] != '\t' ) {
						if ( maxpre > j ) {
							maxpre = j;
						}
					} /*else: a line contains only space, tab or percent*/
				} else {
					maxpre = j;
				}
			}
			DEBUG_MSG( "Will remove %d chars from each line\n", maxpre );
			/* ASK SOME ONE, what version of FREE to be used here */
			g_strfreev( tmpsplit );
		}
		/* remove one line from current selection for each step*/
		while ( gtk_text_iter_compare( &itstart, &itend ) < 0 ) {
			GtkTextMark * cur;
			cur = gtk_text_buffer_create_mark( doc->buffer, NULL, &itstart, TRUE );
			if ( uncomment ) {
				/* TODO: use insert-replace buffer for faster code */
				if ( maxpre ) {
					itend = itstart;
					gtk_text_iter_forward_chars( &itend, maxpre );
					gchar *buf = gtk_text_buffer_get_text( doc->buffer, &itstart, &itend, FALSE );
					gint offsetstart, offsetend;
					offsetstart = gtk_text_iter_get_offset( &itstart );
					offsetend = gtk_text_iter_get_offset( &itend );
					DEBUG_MSG( "Remove: Range [%d,%d] String [%s]\n", offsetstart, offsetend, buf );
					gtk_text_buffer_delete( doc->buffer, &itstart, &itend );
					doc_unre_add( doc, buf, offsetstart, offsetend, UndoDelete );
				}
			} else { /* comment */
				gint offsetstart = gtk_text_iter_get_offset( &itstart );
				gchar *indentstring;
				gint indentlen;
				indentstring = g_strdup( "%% " );
				indentlen = 3;
				gtk_text_buffer_insert( doc->buffer, &itstart, indentstring, indentlen );
				doc_unre_add( doc, indentstring, offsetstart, offsetstart + indentlen, UndoInsert );
				g_free( indentstring );
			}
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &itstart, cur );
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &itend, end );
			gtk_text_buffer_delete_mark( doc->buffer, cur );
			/* forward one more line */
			gtk_text_iter_forward_line( &itstart );
			DEBUG_MSG( "doc_comment_selection, itstart at %d, itend at %d\n", gtk_text_iter_get_offset( &itstart ), gtk_text_iter_get_offset( &itend ) );
		}
		gtk_text_buffer_delete_mark( doc->buffer, end );
		doc_bind_signals( doc );
		doc_set_modified( doc, 1 );
	} else {
		/* there is no selection, work on the current line */
		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, gtk_text_buffer_get_insert( doc->buffer ) );
		gtk_text_iter_set_line_offset( &iter, 0 );
		if ( uncomment ) {
			GtkTextIter itend;

			gtk_text_buffer_get_end_iter( doc->buffer, &itend );
			gchar *tmpstr = gtk_text_buffer_get_text( doc->buffer, &iter, &itend, FALSE );

			/* TODO: COULD WE USE PCRE HERE ?*/
			gint i = 0;
			while ( tmpstr[ i ] != '\0' && ( tmpstr[ i ] == '%' || tmpstr[ i ] == ' ' || tmpstr[ i ] == '\t' ) ) {
				i++;
			}
			g_free( tmpstr );
			if ( i ) {
				itend = iter;
				gtk_text_iter_forward_chars( &itend, i );
				gtk_text_buffer_delete( doc->buffer, &iter, &itend );
			}
		} else { /* comment */
			gchar *indentstring;
			gint indentlen;
			indentstring = g_strdup( "%% " );
			indentlen = 3;
			gtk_text_buffer_insert( doc->buffer, &iter, indentstring, indentlen );
			g_free( indentstring );
		}
	}
}

/* kyanh, added */
void menu_shift_cb( Tbfwin *bfwin, guint callback_action, GtkWidget *widget )
{
	if ( bfwin->current_document ) {
		doc_shift_selection( bfwin->current_document, ( callback_action == 1 ) );
	}
}

/* kyanh, added */
/* vers=0: right == NOOPS*/
void doc_shift_selection( Tdocument *doc, gboolean vers )
{
	GtkTextIter itstart, itend;
	if ( gtk_text_buffer_get_selection_bounds( doc->buffer, &itstart, &itend ) ) {
		GtkTextMark * end;

		doc_unbind_signals( doc );
		doc_unre_new_group( doc );
		/* we have a selection, now we loop trough the characters, and for every newline
		we add or remove a tab, we set the end with a mark */
		end = gtk_text_buffer_create_mark( doc->buffer, NULL, &itend, TRUE );
		/* set to: the fist char of the fist line */
		if ( gtk_text_iter_get_line_offset( &itstart ) > 0 ) {
			gtk_text_iter_set_line_index( &itstart, 0 );
		}
		/* remove one line from current selection for each step*/
		while ( gtk_text_iter_compare( &itstart, &itend ) < 0 ) {
			GtkTextMark * cur;
			cur = gtk_text_buffer_create_mark( doc->buffer, NULL, &itstart, TRUE );
			if ( vers ) {
				itend = itstart;
				gtk_text_iter_forward_chars( &itend, 1 );
				gchar *buf = gtk_text_buffer_get_text( doc->buffer, &itstart, &itend, FALSE );
				if ( !strstr( buf, "\n" ) ) {
					gint offsetstart, offsetend;
					offsetstart = gtk_text_iter_get_offset( &itstart );
					offsetend = gtk_text_iter_get_offset( &itend );
					gtk_text_buffer_delete( doc->buffer, &itstart, &itend );
					doc_unre_add( doc, buf, offsetstart, offsetend, UndoDelete );
				}
				g_free( buf );
			}
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &itstart, cur );
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &itend, end );
			gtk_text_buffer_delete_mark( doc->buffer, cur );
			/* forward one more line */
			gtk_text_iter_forward_line( &itstart );
		}
		gtk_text_buffer_delete_mark( doc->buffer, end );
		doc_bind_signals( doc );
		doc_set_modified( doc, 1 );
	} else {
		/* there is no selection, work on the current line */
		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, gtk_text_buffer_get_insert( doc->buffer ) );
		if ( vers ) {
			GtkTextIter itend;
			gtk_text_iter_set_line_offset( &iter, 0 );
			itend = iter;
			gtk_text_iter_forward_chars( &itend, 1 );
			gtk_text_buffer_delete( doc->buffer, &iter, &itend );
		}
	}
}

/* kyanh, added */
void menu_del_line_cb( Tbfwin *bfwin, guint callback_action, GtkWidget *widget )
{
	if ( bfwin->current_document ) {
		doc_del_line( bfwin->current_document, ( callback_action == 1 ) );
	}
}

/* kyanh, added */
/* vers=0: remove from cursor to the end*/
void doc_del_line( Tdocument *doc, gboolean vers )
{
	GtkTextIter itstart, itend;
	if ( !gtk_text_buffer_get_selection_bounds( doc->buffer, &itstart, &itend ) ) {
		/* there is no selection, work on the current line */
		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, gtk_text_buffer_get_insert( doc->buffer ) );
		if ( vers ) {
			gtk_text_iter_set_line_offset( &iter, 0 );
			gtk_text_iter_forward_line( &itend );
			gtk_text_buffer_delete( doc->buffer, &iter, &itend );
		}
	}
}
