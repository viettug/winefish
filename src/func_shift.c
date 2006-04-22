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

#include <string.h> /* strstr */

#include "bluefish.h"
#include "document.h"
#include "undo_redo.h"

/* kyanh, added */
/* vers=0: right == NOOPS*/
static void doc_shift_selection( Tdocument *doc, gboolean vers ) {
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

void menu_shift_cb( Tbfwin *bfwin, guint callback_action, GtkWidget *widget ) {
	if ( bfwin->current_document ) {
		doc_shift_selection( bfwin->current_document, ( callback_action == 1 ) );
	}
}
