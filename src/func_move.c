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

#define DEBUG

#include <gtk/gtk.h>

#include "bluefish.h"
#include "func_move.h"

gint func_move(GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt) {
	DEBUG_MSG("func_move: started with opt = %d\n", opt >> FUNC_VALUE_);

	Tdocument *doc = bfwin->current_document;

	if (!doc) return 0;

	GtkTextIter itend, iter;
	gboolean retval  = 1;
	gint c_offset, d_offset;
	
	if ( gtk_text_buffer_get_selection_bounds( doc->buffer, &iter, &itend ) ) return 0;
	opt = opt >> FUNC_VALUE_;
	switch(opt) {
	case FUNC_MOVE_END:
		gtk_text_iter_forward_to_end(&itend);
		break;
	case FUNC_MOVE_START:
		gtk_text_iter_set_line (&itend, 0);
		break;
	case FUNC_MOVE_LINE_START:
		gtk_text_iter_set_line_offset ( &itend, 0 );
		break;
	case FUNC_MOVE_LINE_END:
		if ( gtk_text_iter_forward_line( &itend ) || ( gtk_text_iter_get_line_index(&iter) != gtk_text_iter_get_line_index(&itend) ) )
			gtk_text_iter_backward_char(&itend);
		break;
	case FUNC_MOVE_LINE_UP:
		c_offset = gtk_text_iter_get_line_offset(&itend);
		if ( gtk_text_iter_backward_line(&itend) ) {
			d_offset = gtk_text_iter_get_chars_in_line(&itend);
			if (c_offset >= d_offset) c_offset = MAX(0,d_offset - 1);
			gtk_text_iter_set_line_offset(&itend, c_offset);
		}
		break;
	case FUNC_MOVE_LINE_DOWN:
		c_offset = gtk_text_iter_get_line_offset(&itend);
		if (gtk_text_iter_forward_line(&itend) ) {
			d_offset = gtk_text_iter_get_chars_in_line(&itend);
			if (c_offset >= d_offset) c_offset = MAX(0,d_offset - 1);
			gtk_text_iter_set_line_offset(&itend, c_offset);
		}
		break;
	default:
		retval = 0;
		break;
	}
	if (retval) {
		gtk_text_buffer_place_cursor(doc->buffer, &itend);
		gtk_text_view_scroll_to_iter( GTK_TEXT_VIEW( doc->view ), &itend, 0.25, FALSE, 0.5, 0.5);
	}
	return retval;
}
