/* $Id: brace_finder.c 480 2006-04-28 04:55:44Z kyanh $ */

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

/*
static gboolean percent_predicate(gunichar ch, gpointer data) {
	if (ch == 37) {
		return TRUE;
	} else {
		return FALSE;
	}
}

static gboolean dollar_predicate(gunichar ch, gpointer data) {
	if (ch == 36) {
		return TRUE;
	} else {
		return FALSE;
	}
}
*/

/* kyanh, 20060128 */
/**
@param opt: BR_MOVE_IF_FOUND | BR_FIND_BACKWARD | BR_FIND_FORWARD | BR_AUTO_FIND
@param limit: with limit or not
*/
guint16 brace_finder(GtkTextBuffer *buffer, Tbracefinder *brfinder, gint opt, gint limit) {
	GtkTextIter iter_start, iter_start_new, iter_end;
	GtkTextIter tmpiter/* LEFT */, tmp2iter /* RIGHT */, tmpiter_extra /* = `tmpiter' for dollar sign if opt & BR_AUTO_FIND */;
	guint16 retval;
	
	retval = brfinder->last_status;
	
	DEBUG_MSG("\nbrace_finder: opt=%d, limit=%d, last_status=%hd, moved_left=%hd, moved_right=%hd\n", opt, limit,retval, retval &BR_RET_MOVED_LEFT, retval & BR_RET_MOVED_RIGHT );
	if ( retval & BR_RET_FOUND ) {
		if (!(retval & BR_RET_MISS_MID_BRACE)) {
#ifdef STUPID_USE_CACHE
			gboolean use_cached= FALSE;
			if (retval & (BR_RET_MOVED_LEFT | BR_RET_MOVED_RIGHT) ) {
				DEBUG_MSG("brace_finder: previous status: jump left/right from a brace\n");
				if (opt & BR_MOVE_IF_FOUND) {
					use_cached = TRUE;
				}
			}
			if (use_cached) {
				gtk_text_buffer_get_iter_at_mark(buffer, &tmpiter, brfinder->mark_left);
				gtk_text_buffer_get_iter_at_mark(buffer, &tmp2iter, brfinder->mark_mid);
				if (retval & BR_RET_MOVED_LEFT) {
					gtk_text_buffer_place_cursor(buffer, &tmpiter);
					retval = SET_BIT(retval, BR_RET_MOVED_LEFT,0);
					retval = SET_BIT(retval, BR_RET_MOVED_RIGHT,1);
				}else{
					retval = SET_BIT(retval, BR_RET_MOVED_LEFT,1);
					retval = SET_BIT(retval, BR_RET_MOVED_RIGHT,0);
					gtk_text_buffer_place_cursor(buffer, &tmp2iter);
				}
				brfinder->last_status = retval;
				return retval;
			}else{
#endif /* STUPID_USE_CACHE */
				gtk_text_buffer_get_iter_at_mark(buffer, &tmp2iter, brfinder->mark_mid);
				tmpiter_extra = tmp2iter;
				gtk_text_iter_forward_char(&tmpiter_extra);
				gtk_text_buffer_remove_tag(buffer, brfinder->tag, &tmp2iter, &tmpiter_extra);
#ifdef STUPID_USE_CACHE
			}
#endif /* STUPID_USE_CACHE */
		}

		if (retval & (BR_RET_FOUND_RIGHT_BRACE | BR_RET_FOUND_RIGHT_DOLLAR) ) {
			gtk_text_buffer_get_iter_at_mark(buffer, &tmpiter, brfinder->mark_left);
			tmpiter_extra = tmpiter;
			gtk_text_iter_forward_char(&tmpiter_extra);
			gtk_text_buffer_remove_tag(buffer, brfinder->tag, &tmpiter, &tmpiter_extra);
		}

		if (  retval & ( BR_RET_FOUND_LEFT_DOLLAR | BR_RET_FOUND_LEFT_BRACE )  ) {
			gtk_text_buffer_get_iter_at_mark(buffer, &tmpiter, brfinder->mark_right);
			tmpiter_extra = tmpiter;
			gtk_text_iter_forward_char(&tmpiter_extra);
			gtk_text_buffer_remove_tag(buffer, brfinder->tag, &tmpiter, &tmpiter_extra);
		}
	}
	if (limit <0 ) {
		DEBUG_MSG("brace_finder: limit<0, received 'flash' signal. done and return now...\n");
		return BR_RET_NOOP;
	}

	brfinder->last_status = 0;

	GtkTextMark *insert, *select;
	insert = gtk_text_buffer_get_insert(buffer);
	select = gtk_text_buffer_get_selection_bound(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter_start, insert);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter_end, select);

	gchar *tmpstr;
	tmpstr = gtk_text_iter_get_text(&iter_start, &iter_end);
	/* if there's selection, its length should be 1 */
	if (tmpstr && (strlen(tmpstr) > 1) ){
		g_free(tmpstr);
		brfinder->last_status  = BR_RET_IN_SELECTION;
		return BR_RET_IN_SELECTION;
	}else{
		g_free(tmpstr);
	}

	gunichar ch, Lch, Rch;
	gint level, limit_idx, char_idx;	
	retval = BR_RET_NOT_FOUND;
	gint found_comment_sign;

	/* A:: check if we are inside comment line */
	DEBUG_MSG("brace_finder: check for comment status:\n");
	tmpiter = iter_start;
	gtk_text_iter_set_line_offset(&tmpiter,0); /* move to start of line */
	/* now forward to find next %. limit = iter_start_new */
	found_comment_sign = FALSE;
	while ( (gtk_text_iter_compare(&tmpiter, &iter_start) < 0) ) {
		if ( ( gtk_text_iter_get_char(&tmpiter) == 37) && is_true_char(&tmpiter) ) {
			found_comment_sign = TRUE;
			break;
		}else{
			gtk_text_iter_forward_char(&tmpiter);
		}
	}
	if ( found_comment_sign ) {/* in comment */
		DEBUG_MSG("brace_finder: in comment line\n");
		iter_start = tmpiter;
	}

	/* recaculate the iter_start */
	tmpiter = iter_start;
	gtk_text_iter_backward_char(&tmpiter);
	Lch = gtk_text_iter_get_char(&tmpiter);
	Rch = gtk_text_iter_get_char(&iter_start);
	if ( (Rch == 36) && is_true_char(&iter_start) ) {
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
		iter_start_new = iter_start;
		retval = retval | BR_RET_MISS_MID_BRACE;
		DEBUG_MSG("brace_finder: missed mid brace\n");
	}
	if (gtk_text_iter_get_char(&iter_start_new) ==37) {
		gtk_text_iter_backward_char(&iter_start_new);
	}
#ifdef DEBUG
	Lch = gtk_text_iter_get_char(&iter_start_new);
	DEBUG_MSG("brace_finder: new start = %c\n", Lch);
#endif

	limit_idx=1; level=1; char_idx=1;
	tmpiter = iter_start_new;
	if (retval & BR_RET_MISS_MID_BRACE) {
		Lch = 123;
	}else{
		Lch = gtk_text_iter_get_char(&tmpiter);
	}
	DEBUG_MSG("brace_finder: Lch=%c\n", Lch);
	if ( VALID_LEFT_BRACE(Lch) ) {/* { */
		DEBUG_MSG("brace_finder: find forward...\n");
		Rch = (Lch==40)?41:((Lch==91)?93:125);
		/* we may meet } */
		while (gtk_text_iter_forward_char(&tmpiter)) {
			if (limit) { char_idx++; if (char_idx > BRACE_FINDER_MAX_CHARS) { break; } }
			ch = gtk_text_iter_get_char(&tmpiter);
			DEBUG_MSG("%c",ch);
			if ((ch == 37) && is_true_char(&tmpiter)) {/* % */
				gtk_text_iter_forward_to_line_end(&tmpiter);
			}else if( (ch == Lch)  && is_true_char(&tmpiter)) {/* { */
				level ++;
			}else if((ch == Rch) && is_true_char(&tmpiter)) {/* } */
				level --;
				if (level==0) {
					retval = retval | BR_RET_FOUND | BR_RET_FOUND_RIGHT_BRACE;
					break; /* finish the loop */
				}else if(level<0){
					break;
				}
			}else if (limit && gtk_text_iter_ends_line(&tmpiter)) {
				limit_idx++;
				if (limit_idx > limit) { break; }
			}
		}
	}

	limit_idx=1; level=1; char_idx=1;
	tmpiter_extra = iter_start_new;
	if (retval & BR_RET_MISS_MID_BRACE) {
		Lch = 125;
	}else{
		Lch = gtk_text_iter_get_char(&tmpiter_extra);
	}
	if ( VALID_RIGHT_BRACE(Lch) ) {/* } */
		DEBUG_MSG("brace_finder: find backward...\n");
		Rch = (Lch==41)?40:((Lch==93)?91:123);
		while (gtk_text_iter_backward_char(&tmpiter_extra) ){
			if (limit) { char_idx++; if (char_idx > BRACE_FINDER_MAX_CHARS) { break; } }
			ch = gtk_text_iter_get_char(&tmpiter_extra);
			if ((ch==Rch) && is_true_char(&tmpiter_extra)) {/* { */
				level--;
				if (level==0) {
					retval = retval | BR_RET_FOUND | BR_RET_FOUND_LEFT_BRACE;
					break;
				}else if (level<0) {
					break;
				}
			}else if((ch==Lch) && is_true_char(&tmpiter_extra)) {/* } */
				level ++;
			}else if (gtk_text_iter_ends_line(&tmpiter_extra)) {
				DEBUG_MSG("brace_finder: end of line. check for comment status\n");
				if (limit) {
					limit_idx++;
					if (limit_idx > limit) { break; }
				}
				tmp2iter = tmpiter_extra;
				gtk_text_iter_set_line_offset(&tmp2iter,0);
				/* if we are in a comment line
				search from the begining of line to the end == tmpiter;
				*/
				found_comment_sign = FALSE;
				while (gtk_text_iter_compare(&tmp2iter, &tmpiter_extra) < 0 ) {
					if ( (gtk_text_iter_get_char(&tmp2iter) ==37) && is_true_char(&tmp2iter) ) {
						found_comment_sign = TRUE;
						break;
					}else{
						gtk_text_iter_forward_char(&tmp2iter);
					}
				}
				if (found_comment_sign) {
					tmpiter_extra = tmp2iter;
				}
			}
		}
	}
	if ( Lch == 36 ) {
		if ( (opt & BR_AUTO_FIND) || (opt & BR_FIND_FORWARD) ) {
			limit_idx=1; char_idx=1;
			tmpiter = iter_start_new;
			while (gtk_text_iter_forward_char(&tmpiter)) {
				if (limit) { char_idx++; if (char_idx > BRACE_FINDER_MAX_CHARS) { break; } }
				ch = gtk_text_iter_get_char(&tmpiter);
				if ((ch == 37) && is_true_char(&tmpiter)) {/* % */
					gtk_text_iter_forward_to_line_end(&tmpiter);
				}else if( (ch == 36)  && is_true_char(&tmpiter)) {/* { */
					retval = retval | BR_RET_FOUND | BR_RET_FOUND_RIGHT_DOLLAR;
					break;
				}else if (limit && gtk_text_iter_ends_line(&tmpiter)) {
					limit_idx++;
					if (limit_idx > limit) { break; }
				}
			}
		}
		if ( (opt & BR_AUTO_FIND) || (opt & BR_FIND_BACKWARD) ) {/* try to find backward */
			tmpiter_extra = iter_start_new;
			limit_idx=1; char_idx=1;
			while (gtk_text_iter_backward_char(&tmpiter_extra)) {
				if (limit) { char_idx++; if (char_idx > BRACE_FINDER_MAX_CHARS) { break; } }
				ch = gtk_text_iter_get_char(&tmpiter_extra);
				if( (ch == 36)  && is_true_char(&tmpiter_extra)) {/* { */
					retval = retval | BR_RET_FOUND | BR_RET_FOUND_LEFT_DOLLAR;
					break;
				} else if (gtk_text_iter_ends_line(&tmpiter_extra)) {
					if (limit) {
						limit_idx++;
						if (limit_idx > limit) { break; }
					}
					tmp2iter = tmpiter_extra;
					gtk_text_iter_set_line_offset(&tmp2iter,0);
					found_comment_sign = FALSE;
					while (gtk_text_iter_compare(&tmp2iter, &tmpiter_extra) < 0 ) {
						if ( (gtk_text_iter_get_char(&tmp2iter) ==37) && is_true_char(&tmp2iter) ) {
							found_comment_sign = TRUE;
							break;
						}else{
							gtk_text_iter_forward_char(&tmp2iter);
						}
					}
					if (found_comment_sign) {
						tmpiter_extra = tmp2iter;
					}
				}
			}
		}
	}
	/*
	if (limit==0) {
		retval = retval | BR_RET_FOUND_WITH_LIMIT_O;
	}
	*/
	/* finished */
	if (retval & BR_RET_FOUND ) {
		/* for $:  result is (extra-mid-left)
		for others: result is (left-mid) or (mid-left)
		*/
		/* gtk_text_iter_order(&tmpiter,&iter_start_new); */
		if ( opt & BR_MOVE_IF_FOUND ) {
			if (retval & BR_RET_MISS_MID_BRACE) {
				if ( (opt & BR_FIND_BACKWARD) && (retval & BR_RET_FOUND_LEFT_BRACE) ) {
					retval = retval | BR_RET_MOVED_LEFT;
					gtk_text_buffer_place_cursor (buffer, &tmpiter_extra);
				} else if ( (opt & BR_FIND_FORWARD) && (retval & BR_RET_FOUND_RIGHT_BRACE) ) {
					retval = retval | BR_RET_MOVED_RIGHT;
					gtk_text_buffer_place_cursor (buffer, &tmpiter);
				}
			}else{
				if ( (opt & BR_FIND_BACKWARD) && (retval & BR_RET_FOUND_LEFT_DOLLAR) ) {
					retval = retval | BR_RET_MOVED_LEFT;
					gtk_text_buffer_place_cursor (buffer, &tmpiter_extra);
				}else if ( (opt & BR_FIND_FORWARD) && (retval & BR_RET_FOUND_RIGHT_DOLLAR) ) {
					retval = retval | BR_RET_MOVED_RIGHT;
					gtk_text_buffer_place_cursor (buffer, &tmpiter);
				}else if ( (retval & BR_RET_FOUND_RIGHT_BRACE) ){
					retval = retval | BR_RET_MOVED_RIGHT;
					gtk_text_buffer_place_cursor (buffer, &tmpiter);
				}else if ( (retval & BR_RET_FOUND_LEFT_BRACE) ) {
					retval = retval | BR_RET_MOVED_LEFT;
					gtk_text_buffer_place_cursor (buffer, &tmpiter_extra);
				}
			}
		}
		if (retval & (BR_RET_FOUND_RIGHT_BRACE | BR_RET_FOUND_RIGHT_DOLLAR) ) {
			gtk_text_buffer_move_mark(buffer,brfinder->mark_left , &tmpiter);
			/* right matched */
			tmp2iter = tmpiter;
			gtk_text_iter_forward_char(&tmp2iter);
			gtk_text_buffer_apply_tag(buffer,brfinder->tag,&tmpiter, &tmp2iter);
		}
		if (!(retval & BR_RET_MISS_MID_BRACE)) {
			gtk_text_buffer_move_mark(buffer,brfinder->mark_mid, &iter_start_new);
			/* mid matched or current cursor */
			tmpiter = iter_start_new;
			tmp2iter = tmpiter;
			gtk_text_iter_forward_char(&tmp2iter);
			gtk_text_buffer_apply_tag(buffer,brfinder->tag,&tmpiter, &tmp2iter);
		}
		if ( retval & ( BR_RET_FOUND_LEFT_DOLLAR | BR_RET_FOUND_LEFT_BRACE ) ) {
			gtk_text_buffer_move_mark(buffer,brfinder->mark_right,&tmpiter_extra);
			/* left matched or extra dollar */
			tmp2iter = tmpiter_extra;
			gtk_text_iter_forward_char(&tmp2iter);
			gtk_text_buffer_apply_tag(buffer,brfinder->tag,&tmpiter_extra, &tmp2iter);
		}
	}
	brfinder->last_status = retval;
	DEBUG_MSG("brace_finder: now_status=%d, moved_left=%d, moved_right=%d\n", retval, retval &BR_RET_MOVED_LEFT, retval & BR_RET_MOVED_RIGHT );
	return retval;
}
