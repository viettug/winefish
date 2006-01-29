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

/* #define DEBUG */

#include <gtk/gtk.h>

#include "bluefish.h"
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
gint brace_finder(GtkTextBuffer *buffer, gint opt) {
	DEBUG_MSG("\nbrace_finder: %%=%d,\\=%d,{=%d,}=%d,[=%d,]=%d, \\n=%d, \\r=%d \n", '%', '\\', '{','}','[', ']', '\n', '\r' );
	GtkTextIter iter_start, iter_end;
	GtkTextMark *insert, *select;
	gboolean retval;
	gunichar ch;

	insert = gtk_text_buffer_get_insert(buffer);
	select = gtk_text_buffer_get_selection_bound(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter_start, insert);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter_end, select);

	/* there's no selection */
	if (gtk_text_iter_equal(&iter_start, &iter_end)){
		GtkTextIter tmpiter, tmp2iter;
		gint level;

		/* A:: check if we are inside comment line */
		tmpiter = iter_start;
		gtk_text_iter_set_line_offset(&tmpiter,0); /* move to start of line */
		/* now forward to find next %. limit = iter_start */
		ch = gtk_text_iter_get_char(&tmpiter);
		if ( (ch == 37) || ( gtk_text_iter_forward_find_char(&tmpiter, char_predicate, GINT_TO_POINTER(37), &iter_start) && is_true_char(&tmpiter) ) ) {
		/* if a line is started by percent, the gtk_text_iter_forward_find_char() still forwards to the next char ;) */
			DEBUG_MSG("[found %%]");
			return BR_RET_IN_COMMENT;
		}
		if (!is_true_char(&iter_start)) {
			DEBUG_MSG("[not a true { nor }]");
			return BR_RET_IS_ESCAPED;
		}

		retval = FALSE;
		ch = gtk_text_iter_get_char(&iter_start);

		if (ch == 123 ) {/* { */
			DEBUG_MSG("\nbrace_finder: find forward...=============\n");
			tmpiter = iter_start; /* reset tmpiter */
			level = 1;
			/* we may meet } */
			while (gtk_text_iter_forward_char(&tmpiter)) {
				ch = gtk_text_iter_get_char(&tmpiter);
				DEBUG_MSG("%c", ch);
				/* DEBUG_MSG("brace_finder: enter new loop... with char = %c\n", ch); */
				if ((ch == 37) && is_true_char(&tmpiter)) {/* % */
#ifdef CANNOT_USE_THIS /* as we may skip the char followed the line end */
					gtk_text_iter_forward_line(&tmpiter);
					gunichar tmpch = gtk_text_iter_get_char(&tmpiter);
					DEBUG_MSG("brace_finder: find true %%. forward to next line\n");
					DEBUG_MSG("brace_finder: current char (after forwarding) %c\n", tmpch);
#endif /* CANNOT_USE_THIS */
					gtk_text_iter_forward_to_line_end(&tmpiter);
					DEBUG_MSG("[%%]");
				}else if( (ch == 123)  && is_true_char(&tmpiter)) {/* { */
					level ++;
					DEBUG_MSG("[+%d]", level);
				}else if((ch ==125) && is_true_char(&tmpiter)) {/* } */
					level --;
					DEBUG_MSG("[-%d]\n", level);
					if (level==0) {
						retval = TRUE;
						break; /* finish the loop */
					}
				}
			}
		}else if (ch == 125) {/* } */
			DEBUG_MSG("\nbrace_finder: find backward...=============\n");
			level=1;
			tmpiter = iter_start;
			while (gtk_text_iter_backward_char(&tmpiter) ){
				ch = gtk_text_iter_get_char(&tmpiter);
				DEBUG_MSG("%c", ch);
				if ((ch==123) && is_true_char(&tmpiter)) {/* { */
					level--;
					if (level==0) {
						retval = TRUE;
						break;
					}
					DEBUG_MSG("[-%d]", level);
				}else if((ch==125) && is_true_char(&tmpiter)) {/* } */
					level ++;
					DEBUG_MSG("[-%d]", level);
				}else if (gtk_text_iter_ends_line(&tmpiter)) {
					DEBUG_MSG("[line end]");
					tmp2iter = tmpiter;
					gtk_text_iter_set_line_offset(&tmp2iter,0);
					/* if we are in a comment line
					search from the begining of line to the end == tmpiter;
					*/
					ch = gtk_text_iter_get_char(&tmp2iter);
					if ((ch ==37) || ( gtk_text_iter_forward_find_char(&tmp2iter, char_predicate, GINT_TO_POINTER(37), &tmpiter) && is_true_char(&tmp2iter) ) ) {
						DEBUG_MSG("[found %%]");
						tmpiter = tmp2iter;
					}
				}
			}
		}
		/* finished */
		if (retval) {
			if (opt & MOVE_IF_FOUND) {
				gtk_text_buffer_place_cursor (buffer, &tmpiter);
			}
			return BR_RET_FOUND;
		}
		return BR_RET_NOT_FOUND;
	}
#ifdef DEBUG
	else{
		DEBUG_MSG("brace_finder: selection is not empy. does nothing\n");
	}
#endif
	return BR_RET_IN_SELECTION;
}
