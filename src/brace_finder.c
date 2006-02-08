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
#include <string.h> /* strlen */

#include "bluefish.h"
#include "brace_finder.h"
/* #include "document.h" */

/*
(=40,)=41,
$=36,
%=37,
\=92,
{=123,}=125,
[=91,]=93,
\n=10, \r=13
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

static gboolean percent_predicate(gunichar ch, gpointer data) {
	if (ch == 37) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/*
static gboolean dollar_predicate(gunichar ch, gpointer data) {
	if (ch == 36) {
		return TRUE;
	} else {
		return FALSE;
	}
}
*/

#define VALID_LEFT_BRACE(Lch) ( (Lch == 123) || (Lch == 91) || (Lch==40) )
#define VALID_RIGHT_BRACE(Lch) ( (Lch == 125) || (Lch==93) || (Lch==41) )
#define VALID_LEFT_RIGHT_BRACE(Lch) ( VALID_LEFT_BRACE(Lch) || VALID_RIGHT_BRACE(Lch) )
#define VALID_BRACE(Lch) ( (Lch == 36) || VALID_LEFT_RIGHT_BRACE(Lch) )

/* kyanh, 20060128 */
gint brace_finder(GtkTextBuffer *buffer, gint opt) {
	GtkTextIter iter_start, iter_start_new, iter_end;
	GtkTextMark *insert, *select;
	gchar *tmpstr;
	gboolean retval;
	gunichar ch, Lch, Rch;

	insert = gtk_text_buffer_get_insert(buffer);
	select = gtk_text_buffer_get_selection_bound(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter_start, insert);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter_end, select);

	tmpstr = gtk_text_iter_get_text(&iter_start, &iter_end);

	/* there's no selection */
	if (!tmpstr || (strlen(tmpstr) <= 1) ){
		gint level;
		GtkTextIter tmpiter/* LEFT */, tmp2iter /* RIGHT */;

		/* A:: check if we are inside comment line */
		tmpiter = iter_start;
		gtk_text_iter_set_line_offset(&tmpiter,0); /* move to start of line */
		/* now forward to find next %. limit = iter_start_new */
		ch = gtk_text_iter_get_char(&tmpiter);
		if ( (ch == 37) || ( gtk_text_iter_forward_find_char(&tmpiter, percent_predicate, NULL, &iter_start) && is_true_char(&tmpiter) ) ) {
			/* if a line is started by percent, the gtk_text_iter_forward_find_char() still forwards to the next char ;) */
			DEBUG_MSG("[found %%]");
			return BR_RET_IN_COMMENT;
		}

		tmpiter = iter_start;
		/* tmp2iter = iter_start; */
		gtk_text_iter_backward_char(&tmpiter);
		Lch = gtk_text_iter_get_char(&tmpiter);
		Rch = gtk_text_iter_get_char(&iter_start);
		/* gtk_text_iter_forward_char(&tmpiter);
		Rch = gtk_text_iter_get_char(&tmpiter); */
		
		if ( (Rch == 36 ) && is_true_char(&iter_start)) {
			iter_start_new = iter_start;
		} else if ( VALID_BRACE(Rch) && is_true_char(&iter_start) ) {
			if ( opt & BR_FIND_BACKWARD ) {
				if ( VALID_BRACE(Lch) && is_true_char(&tmpiter) ) {
					iter_start_new = tmpiter;
				}else{
					iter_start_new = iter_start;
				}
			} else {
				iter_start_new = iter_start;
			}
		} else if ( VALID_BRACE(Lch) && is_true_char(&tmpiter) ) {
			iter_start_new = tmpiter;
		} else {
			return BR_RET_WRONG_OPERATION;
		}

		retval = FALSE;
		level = 1;
		Lch = gtk_text_iter_get_char(&iter_start_new);
		tmpiter = iter_start_new; /* reset tmpiter */

		if ( VALID_LEFT_BRACE(Lch) ) {/* { */
			DEBUG_MSG("\nbrace_finder: find forward...=============\n");
			Rch = (Lch==40)?41:((Lch==91)?93:125);
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
				}else if( (ch == Lch)  && is_true_char(&tmpiter)) {/* { */
					level ++;
					DEBUG_MSG("[+%d]", level);
				}else if((ch == Rch) && is_true_char(&tmpiter)) {/* } */
					level --;
					DEBUG_MSG("[-%d]\n", level);
					if (level==0) {
						retval = TRUE;
						break; /* finish the loop */
					}
				}
			}
		}else if ( VALID_RIGHT_BRACE(Lch) ) {/* } */
			DEBUG_MSG("\nbrace_finder: find backward...=============\n");
			Rch = (Lch==41)?40:((Lch==93)?91:123);
			while (gtk_text_iter_backward_char(&tmpiter) ){
				ch = gtk_text_iter_get_char(&tmpiter);
				DEBUG_MSG("%c", ch);
				if ((ch==Rch) && is_true_char(&tmpiter)) {/* { */
					level--;
					if (level==0) {
						retval = TRUE;
						break;
					}
					DEBUG_MSG("[-%d]", level);
				}else if((ch==Lch) && is_true_char(&tmpiter)) {/* } */
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
					if ((ch ==37) || ( gtk_text_iter_forward_find_char(&tmp2iter, percent_predicate, NULL, &tmpiter) && is_true_char(&tmp2iter) ) ) {
						DEBUG_MSG("[found %%]");
						tmpiter = tmp2iter;
					}
				}
			}
		}else if ( Lch == 36 ) {
			if (opt & BR_FIND_FORWARD) {
				while (gtk_text_iter_forward_char(&tmpiter)) {
					ch = gtk_text_iter_get_char(&tmpiter);
					if ((ch == 37) && is_true_char(&tmpiter)) {/* % */
						gtk_text_iter_forward_to_line_end(&tmpiter);
					}else if( (ch == 36)  && is_true_char(&tmpiter)) {/* { */
						retval = TRUE;
						break;
					}
				}
			} else if (opt & BR_FIND_BACKWARD) {/* try to find backward */
				while (gtk_text_iter_backward_char(&tmpiter)) {
					ch = gtk_text_iter_get_char(&tmpiter);
					if( (ch == 36)  && is_true_char(&tmpiter)) {/* { */
						retval = TRUE;
						break;
					} else if (gtk_text_iter_ends_line(&tmpiter)) {
						tmp2iter = tmpiter;
						gtk_text_iter_set_line_offset(&tmp2iter,0);
						ch = gtk_text_iter_get_char(&tmp2iter);
						if ((ch ==37) || ( gtk_text_iter_forward_find_char(&tmp2iter, percent_predicate, NULL, &tmpiter) && is_true_char(&tmp2iter) ) ) {
							tmpiter = tmp2iter;
						}
					}
				}
			}
		}else{/*never reach here */
			return BR_RET_WRONG_OPERATION;
		}
		/* finished */
		if (retval) {
			if (opt & BR_MOVE_IF_FOUND) {
				gtk_text_buffer_place_cursor (buffer, &tmpiter);
				tmp2iter = tmpiter;
				gtk_text_iter_forward_char(&tmp2iter);
				gtk_text_buffer_select_range(buffer,&tmpiter,&tmp2iter);
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
	g_free(tmpstr);
	return BR_RET_IN_SELECTION;
}
