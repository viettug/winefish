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

/* kyanh, 20060128 */
gint brace_finder(GtkTextBuffer *buffer, gpointer *brfinder, gint opt, gint limit) {
	GtkTextIter iter_start, iter_start_new, iter_end;
	GtkTextIter tmpiter/* LEFT */, tmp2iter /* RIGHT */, tmpiter_extra /* = `tmpiter' for dollar sign if opt & BR_HILIGHT_IF_FOUND */;
	GtkTextMark *insert, *select;
	gchar *tmpstr;
	gint retval;
	gunichar ch, Lch, Rch;

	if (brfinder) {
		if ( BRACEFINDER(*brfinder)->last_status & BR_RET_FOUND) {
			gtk_text_buffer_get_iter_at_mark(buffer, &tmpiter, BRACEFINDER(*brfinder)->mark_left);
			tmp2iter = tmpiter;
			gtk_text_iter_forward_char(&tmp2iter);
			gtk_text_buffer_remove_tag(buffer, BRACEFINDER(*brfinder)->tag, &tmpiter, &tmp2iter);
			DEBUG_MSG("\nbrace_finder: delete hilight marks for %s\n", gtk_text_iter_get_text(&tmpiter, &tmp2iter));

			gtk_text_buffer_get_iter_at_mark(buffer, &tmpiter, BRACEFINDER(*brfinder)->mark_mid);
			tmp2iter = tmpiter;
			gtk_text_iter_forward_char(&tmp2iter);
			DEBUG_MSG("brace_finder: delete hilight marks for %s\n", gtk_text_iter_get_text(&tmpiter, &tmp2iter));
			gtk_text_buffer_remove_tag(buffer, BRACEFINDER(*brfinder)->tag, &tmpiter, &tmp2iter);

			if (BRACEFINDER(*brfinder)->last_status & BR_RET_FOUND_DOLLAR_EXTRA) {
				gtk_text_buffer_get_iter_at_mark(buffer, &tmpiter, BRACEFINDER(*brfinder)->mark_right);
				tmp2iter = tmpiter;
				gtk_text_iter_forward_char(&tmp2iter);
				gtk_text_buffer_remove_tag(buffer, BRACEFINDER(*brfinder)->tag, &tmpiter, &tmp2iter);
			}
		}
		BRACEFINDER(*brfinder)->last_status = 0;
	}

	insert = gtk_text_buffer_get_insert(buffer);
	select = gtk_text_buffer_get_selection_bound(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter_start, insert);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter_end, select);

	tmpstr = gtk_text_iter_get_text(&iter_start, &iter_end);

	/* there's no selection */
	if (!tmpstr || (strlen(tmpstr) <= 1) ){
		gint level, limit_idx;

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
				
		/* A:: check if we are inside comment line */
		tmpiter = iter_start_new;
		gtk_text_iter_set_line_offset(&tmpiter,0); /* move to start of line */
		/* now forward to find next %. limit = iter_start_new */
		ch = gtk_text_iter_get_char(&tmpiter);
		if ( (ch == 37) || ( gtk_text_iter_forward_find_char(&tmpiter, percent_predicate, NULL, &iter_start_new) && is_true_char(&tmpiter) ) ) {
			/* if a line is started by percent, the gtk_text_iter_forward_find_char() still forwards to the next char ;) */
			DEBUG_MSG("[found %%]");
			return BR_RET_IN_COMMENT;
		}

		level = 1;
		limit_idx=1;
		retval = BR_RET_NOT_FOUND;
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
					limit_idx++;
					if (limit && (limit_idx > limit)) { return BR_RET_NOT_FOUND; }
					gtk_text_iter_forward_to_line_end(&tmpiter);
					DEBUG_MSG("[%%]");
				}else if( (ch == Lch)  && is_true_char(&tmpiter)) {/* { */
					level ++;
					DEBUG_MSG("[+%d]", level);
				}else if((ch == Rch) && is_true_char(&tmpiter)) {/* } */
					level --;
					DEBUG_MSG("[-%d]\n", level);
					if (level==0) {
						retval = BR_RET_FOUND | BR_RET_FOUND_RIGHT_BRACE;
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
						retval = BR_RET_FOUND | BR_RET_FOUND_LEFT_BRACE;
						break;
					}
					DEBUG_MSG("[-%d]", level);
				}else if((ch==Lch) && is_true_char(&tmpiter)) {/* } */
					level ++;
					DEBUG_MSG("[-%d]", level);
				}else if (gtk_text_iter_ends_line(&tmpiter)) {
					DEBUG_MSG("[line end]");
					limit_idx++;
					if (limit && (limit_idx > limit)) { return BR_RET_NOT_FOUND; }
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
			if ( (opt & BR_HILIGHT_IF_FOUND) || (opt & BR_FIND_FORWARD) ) {
				while (gtk_text_iter_forward_char(&tmpiter)) {
					ch = gtk_text_iter_get_char(&tmpiter);
					if ((ch == 37) && is_true_char(&tmpiter)) {/* % */
						limit_idx++;
						if (limit && (limit_idx > limit)) { return BR_RET_NOT_FOUND; }
						gtk_text_iter_forward_to_line_end(&tmpiter);
					}else if( (ch == 36)  && is_true_char(&tmpiter)) {/* { */
						retval = BR_RET_FOUND/* | BR_RET_FOUND_DOLLAR*/;
						break;
					}
				}
			}
			if ( (opt & BR_HILIGHT_IF_FOUND) || (opt & BR_FIND_BACKWARD) ) {/* try to find backward */
				tmpiter_extra = iter_start_new;
				while (gtk_text_iter_backward_char(&tmpiter_extra)) {
					ch = gtk_text_iter_get_char(&tmpiter_extra);
					if( (ch == 36)  && is_true_char(&tmpiter_extra)) {/* { */
						retval = BR_RET_FOUND | BR_RET_FOUND_DOLLAR_EXTRA;
						break;
					} else if (gtk_text_iter_ends_line(&tmpiter_extra)) {
						limit_idx++;
						if (limit && (limit_idx > limit)) { return BR_RET_NOT_FOUND; }
						tmp2iter = tmpiter_extra;
						gtk_text_iter_set_line_offset(&tmp2iter,0);
						ch = gtk_text_iter_get_char(&tmp2iter);
						if ((ch ==37) || ( gtk_text_iter_forward_find_char(&tmp2iter, percent_predicate, NULL, &tmpiter_extra) && is_true_char(&tmp2iter) ) ) {
							tmpiter_extra = tmp2iter;
						}
					}
				}
				if ( !(opt & BR_HILIGHT_IF_FOUND) ) {
					tmpiter = tmpiter_extra;
				}
			}
		}
		/* finished */
		if (retval & BR_RET_FOUND ) {
			tmp2iter = tmpiter;
			gtk_text_iter_forward_char(&tmp2iter);
			if (opt & BR_MOVE_IF_FOUND) {
				gtk_text_buffer_place_cursor (buffer, &tmpiter);
				gtk_text_buffer_select_range(buffer,&tmpiter,&tmp2iter);
			}
			if ((opt & BR_HILIGHT_IF_FOUND) && brfinder) {
				BRACEFINDER(*brfinder)->last_status = retval;

				if (BRACEFINDER(*brfinder)->mark_left) {
					gtk_text_buffer_move_mark(buffer,BRACEFINDER(*brfinder)->mark_left , &tmpiter);
					gtk_text_buffer_move_mark(buffer,BRACEFINDER(*brfinder)->mark_mid, &iter_start_new);
				}else{
					/* STORIES: i used TRUE (left_gravity), then when we put a slash before anymark,
					the backslash will be hilighted :D Now i now the reason why. Now ready to fix BUG#82 */
					BRACEFINDER(*brfinder)->mark_left = gtk_text_buffer_create_mark(buffer,NULL,&tmpiter,FALSE);
					BRACEFINDER(*brfinder)->mark_mid = gtk_text_buffer_create_mark(buffer,NULL,&iter_start_new,FALSE);
				}
				if (retval & BR_RET_FOUND_DOLLAR_EXTRA) {
					if (BRACEFINDER(*brfinder)->mark_right) {
						gtk_text_buffer_move_mark(buffer,BRACEFINDER(*brfinder)->mark_right,&tmpiter_extra);
					}else{
						BRACEFINDER(*brfinder)->mark_right = gtk_text_buffer_create_mark(buffer,NULL,&tmpiter_extra,FALSE);
					}
				}

				/* right matched */
				gtk_text_buffer_apply_tag(buffer,BRACEFINDER(*brfinder)->tag,&tmpiter, &tmp2iter);

				/* mid matched or current cursor */
				tmpiter = iter_start_new;
				tmp2iter = tmpiter;
				gtk_text_iter_forward_char(&tmp2iter);
				gtk_text_buffer_apply_tag(buffer,BRACEFINDER(*brfinder)->tag,&tmpiter, &tmp2iter);

				if (retval & BR_RET_FOUND_DOLLAR_EXTRA) {
					/* left matched or extra dollar */
					tmp2iter = tmpiter_extra;
					gtk_text_iter_forward_char(&tmp2iter);
					gtk_text_buffer_apply_tag(buffer,BRACEFINDER(*brfinder)->tag,&tmpiter_extra, &tmp2iter);
				}
			}
		}
		return retval;
	}
	g_free(tmpstr);
	return BR_RET_IN_SELECTION;
}
