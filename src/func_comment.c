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

/* #define DEBUG */

#include <gtk/gtk.h>

#include "bluefish.h"
#include "undo_redo.h"
#include "document.h"
#include "stringlist.h" /* count_array */
#include "snooper2.h"

static void doc_comment_selection( Tdocument *doc, gboolean uncomment ) {
	if (!doc) return;

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

void menu_comment_cb( Tbfwin *bfwin, guint callback_action, GtkWidget *widget ) {
	doc_comment_selection( bfwin->current_document, ( callback_action == 1 ) );
}

gint func_comment(GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt) {
	DEBUG_MSG("doc_comment_selection: started with opt = %d\n", opt);
	doc_comment_selection( bfwin->current_document, opt & FUNC_VALUE_0 );
	return 1;
}

/*
gint func_uncomment(GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt) {
	doc_comment_selection( bfwin->current_document, 1 );
	return 1;
}
*/

