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

/*
 opt & FUNC_VALUE_0: delete the whole line
 opt & FUNC_VALUE_1: delete to right (end of line)
 opt & FUNC_VALUE_2: delete to left (begining of line)
*/
gint func_delete_line( GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt ) {
	Tdocument *doc = bfwin->current_document;
	if (!doc) return 0;

	GtkTextIter itstart, itend;
	if ( gtk_text_buffer_get_selection_bounds( doc->buffer, &itstart, &itend ) )
		return 0;
	/* there is no selection, work on the current line */
	GtkTextIter iter;
	gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, gtk_text_buffer_get_insert( doc->buffer ) );
	/* delete the whole line */
	if ( opt & FUNC_VALUE_0 ) {
		gtk_text_iter_set_line_offset( &iter, 0 );
		gtk_text_iter_forward_line( &itend );
	/* delete to end of line */
	} else if (opt & FUNC_VALUE_1 ) {
		if ( gtk_text_iter_forward_line( &itend ) )
			gtk_text_iter_backward_char(&itend);
	/* delete to begin of line */
	} else if (opt & FUNC_VALUE_2 ) {
		gtk_text_iter_set_line_offset( &iter, 0 );
	}
	if (gtk_text_iter_compare(&iter, &itend) == -1) {
		gtk_text_buffer_delete( doc->buffer, &iter, &itend );
		return 1;
	}
	return 0;
}

void menu_del_line_cb( Tbfwin *bfwin, guint callback_action, GtkWidget *widget ) {
	gint opt = 0;
	switch (callback_action) {
	case 1: opt = FUNC_VALUE_1; break;
	case 2: opt = FUNC_VALUE_2; break;
	default: opt = FUNC_VALUE_0; break;
	}
	func_delete_line( widget, NULL, bfwin, opt );
}
