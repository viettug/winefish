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

#include "bluefish.h"

/* kyanh, added */
/* vers=0: remove from cursor to the end*/
static void doc_del_line( Tdocument *doc, gboolean vers ) {
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

void menu_del_line_cb( Tbfwin *bfwin, guint callback_action, GtkWidget *widget ) {
	if ( bfwin->current_document ) {
		doc_del_line( bfwin->current_document, ( callback_action == 1 ) );
	}
}
