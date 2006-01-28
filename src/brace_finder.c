/* $Id$ */

/* Winefish LaTeX Editor
 *
 * brace_finder.c
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

#define DEBUG

#include <gtk/gtk.h>

#include "bluefish.h"
#include "gui.h" /* statusbar_message() */
#include "brace_finder.h"
/* #include "document.h" */

/*
%=37,\=92,{=123,}=125,[=91,]=93
*/

/*
*/
static gboolean is_true_char(GtkTextIter *iter)
{
	gboolean retval = TRUE;
	GtkTextIter tmpiter;
	tmpiter = *iter;
	gunichar ch;
	while (gtk_text_iter_backward_char(&tmpiter)) {
		ch =gtk_text_iter_get_char(&tmpiter);
		if ( ch == 92 ) {
			retval = ! retval;
		}else{
			break;
		}
	}
	return retval;
}

static gboolean char_predicate(gunichar ch, gpointer data) {
	if (ch == GPOINTER_TO_INT(data)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/* kyanh, 20060128 */
void brace_finder(Tdocument *doc) {
	/* DEBUG_MSG("brace_finder: %%=%d,\\=%d,{=%d,}=%d,[=%d,]=%d\n", '%', '\\', '{','}','[', ']' ); */
	GtkTextIter iter_start, iter_end;
	GtkTextMark *insert, *select;
	gboolean retval;
	gunichar ch;

	insert = gtk_text_buffer_get_insert(doc->buffer);
	select = gtk_text_buffer_get_selection_bound(doc->buffer);
	gtk_text_buffer_get_iter_at_mark(doc->buffer, &iter_start, insert);
	gtk_text_buffer_get_iter_at_mark(doc->buffer, &iter_end, select);
	if (gtk_text_iter_equal(&iter_start, &iter_end)){
		ch = gtk_text_iter_get_char(&iter_start);
		if (ch == 123 ) {/* { */
			/*
			retval = is_true_char(&iter_start);
			DEBUG_MSG("brace_finder: first check: true_char ({) =  %d\n", retval);
			*/
		/* A:: check if we are inside comment line */
			/* A:: 1*/
			GtkTextIter tmpiter;
			tmpiter = iter_start;
			gtk_text_iter_set_line_offset(&tmpiter,0); /* move to start of line */
			/* now forward to find next %. limit = iter_start */
			retval = gtk_text_iter_forward_find_char(&tmpiter, char_predicate, GINT_TO_POINTER(37), &iter_start);
			if (retval && is_true_char(&tmpiter)) {
				DEBUG_MSG("brace_finder: inside a comment line\n");
				return;
			}
			if (!is_true_char(&iter_start)) {
				DEBUG_MSG("brace_finder: not a true {\n");
				return;
			}
		/* B:: now forward until '}' */
			retval = FALSE;
			tmpiter = iter_start; /* reset tmpiter */
			gint level = 1;
			/* we may meet } */
			while (gtk_text_iter_forward_char(&tmpiter)) {
				ch = gtk_text_iter_get_char(&tmpiter);
				/* DEBUG_MSG("brace_finder: enter new loop... with char = %c\n", ch); */
				if (ch == 37) {/* % */
					if (is_true_char(&tmpiter)) {
#ifdef CANNOT_USE_THIS /* as we may skip the char followed the line end */
						gtk_text_iter_forward_line(&tmpiter);
						gunichar tmpch = gtk_text_iter_get_char(&tmpiter);
						DEBUG_MSG("brace_finder: find true %%. forward to next line\n");
						DEBUG_MSG("brace_finder: current char (after forwarding) %c\n", tmpch);
#endif /* CANNOT_USE_THIS */
					gtk_text_iter_forward_to_line_end(&tmpiter);
					DEBUG_MSG("brace_finder: find true %%. forward to end of line\n");
					}
				}else if(ch == 123) {/* { */
					if (is_true_char(&tmpiter)) {
						level ++;
						DEBUG_MSG("brace_finder: find true {. level up to %d\n", level);
					}
				}else if(ch ==125) {/* } */
					if (is_true_char(&tmpiter)) {
						level --;
						DEBUG_MSG("brace_finder: find true }. level down to %d\n", level);
						if (level==0) {
							retval = TRUE;
							break; /* finish the loop */
						}
					}
				}
			}
			if (retval) {
				DEBUG_MSG("brace_finder: okay. find }\n");
			}else{
				DEBUG_MSG("brace_finder: oops. cannot find }\n");
			}
		}
	}
#ifdef DEBUG
	else{
		DEBUG_MSG("brace_finder: selection is not empy. does nothing\n");
	}
#endif
}

