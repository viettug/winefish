/* $Id$
 *
 * lykey, demo gtk-program with emacs-key style
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

#include "config.h"

#ifdef SNOOPER2

#define DEBUG

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "bluefish.h"
#include "snooper2.h"

static gboolean snooper_loopkup_keyseq(GtkWidget *widget, Tbfwin *bfwin, GdkEventKey *kevent1, GdkEventKey *kevent2) {
	gchar *tmpstr;
	const gchar *ctrl, *shift, *mod1;
	const gchar *ctrl2, *shift2, *mod12;
	guint32 ch, ch2;
	gchar *value;
	gboolean retval;

	ch = gdk_keyval_to_unicode( kevent1->keyval );
	ctrl = kevent1->state & GDK_CONTROL_MASK ? "<control>": "";
	shift = kevent1->state & GDK_SHIFT_MASK ? "<shift>": "";
	mod1 = kevent1->state & GDK_MOD1_MASK ? "<mod1>": "";

	if (kevent2) {
		ch2 = gdk_keyval_to_unicode( kevent2->keyval );
		ctrl2 = kevent2->state & GDK_CONTROL_MASK ? "<control>": "";
		shift2 = kevent2->state & GDK_SHIFT_MASK ? "<shift>": "";
		mod12 = kevent2->state & GDK_MOD1_MASK ? "<mod1>": "";
		/* TODO: use (gtk_accelerator_name) */
		tmpstr = g_strdup_printf("%s%s%s%c%s%s%s%c",ctrl,shift,mod1,ch,ctrl2,shift2,mod12,ch2);
	}else{
		tmpstr = g_strdup_printf("%s%s%s%c",ctrl,shift,mod1,ch);
	}

	retval = FALSE;
	value = g_hash_table_lookup(main_v->key_hashtable, tmpstr);
	if (value) {
		Tfunc *value_;
		value_ = g_hash_table_lookup(main_v->func_hashtable,value);
		if (value_) {
			if ( FUNC_VALID_TYPE( value_->data, widget ) ) {
				retval = TRUE;
				func_complete_hide(bfwin);
				FUNC(value_)->exec(widget, kevent1, bfwin, value_->data );
			}
		}
	}
	SNOOPER(bfwin->snooper)->stat = SNOOPER_HAS_EXCUTING_FUNC;
	DEBUG_MSG("snooper: lookup '%s', retval = %d\n", tmpstr, retval);
	g_free(tmpstr);
	return retval;
}

static gboolean snooper_accel_group_find_func(GtkAccelKey *key, GClosure *closure, gpointer data) {
	GdkEventKey *test = (GdkEventKey*)data;
#ifdef DEBUG_ALL
	DEBUG_MSG("snooper_accel_group_find_func: accel(%d,%d) compared with (%d,%d)\n", key->accel_key, key->accel_mods, test->keyval, test->state);
#endif /* DEBUG */
	return ( (key->accel_key == test->keyval) && (key->accel_mods & test->state ) );
}

static gboolean snooper_loopkup_keys_in_accel_map(GdkEventKey *kevent) {
	GtkAccelKey *accel_key;
	gboolean retval;

	retval = FALSE;
	accel_key = gtk_accel_group_find( main_v->accel_group, (GtkAccelGroupFindFunc) snooper_accel_group_find_func, kevent);
	if (accel_key) retval = TRUE;

	DEBUG_MSG("snooper: lookup in accel. group, returns %d\n", retval);
	return retval;
}

static gint main_snooper (GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin) {
	Tsnooper *snooper =  SNOOPER(bfwin->snooper);

	DEBUG_MSG("snooper:id(%d)press(%d)widget(%p,%s)\n", snooper->id, (kevent->type == GDK_KEY_PRESS), widget, gtk_widget_get_name(widget) );

	/** check for valid snooper here **/
	if (snooper->id != main_v->active_snooper ) return FALSE;

	if (  kevent->type == GDK_KEY_RELEASE && ( snooper->stat == SNOOPER_CANCEL_RELEASE_EVENT || snooper->stat == SNOOPER_HAS_EXCUTING_FUNC ) ) {
		DEBUG_MSG("snooper: has executed function OR cancel command requested; exit...\n");
		snooper->stat = 0;
		return TRUE;
	}

	/** special for completion **/
	if ( SNOOPER_COMPLETION_ON(bfwin) ) {
		DEBUG_MSG("snooper: completion on bfwin = %p\n", bfwin);
		if ( SNOOPER_COMPLETION_MOVE(kevent->keyval) ) {
			func_complete_move(kevent, bfwin);
			snooper->stat = SNOOPER_CANCEL_RELEASE_EVENT;
			return TRUE;
		}else if ( SNOOPER_COMPLETION_ACCEPT(kevent->keyval) ) {
			func_complete_do(bfwin);
			snooper->stat = SNOOPER_CANCEL_RELEASE_EVENT;
			return TRUE;
		}else if ( SNOOPER_COMPLETION_ESCAPE(kevent->keyval) ) {
			func_complete_hide(bfwin);
			snooper->stat = SNOOPER_CANCEL_RELEASE_EVENT;
			return TRUE;
		}else if ( SNOOPER_COMPLETION_DELETE(kevent->keyval) ) {
			func_complete_delete(widget, bfwin);
			snooper->stat = SNOOPER_CANCEL_RELEASE_EVENT;
			return TRUE;
		}else if ( snooper->stat ) {
			func_complete_hide(bfwin);
		}
	}

	/** if completion is hidden **/
	if (kevent->type == GDK_KEY_PRESS) {
		SNOOPER(bfwin->snooper)->last_event = (GdkEvent *)kevent;
		if ( snooper->stat && ( kevent->keyval == GDK_Escape ) ) {
			snooper->stat = SNOOPER_CANCEL_RELEASE_EVENT;
			/* hide the completion popup menu */
			return TRUE;
		}else if ( (snooper->stat == SNOOPER_HALF_SEQ) || SNOOPER_IS_KEYSEQ(kevent) ) {
			if ( snooper->stat == SNOOPER_HALF_SEQ ) {
				snooper_loopkup_keyseq(widget, bfwin, (GdkEventKey*) snooper -> last_seq, kevent);
				return TRUE;
			} else if (snooper->stat == 0) {
				*( (GdkEventKey*) snooper->last_seq )= *kevent;
				if (snooper_loopkup_keyseq(widget, bfwin, kevent, NULL) ) {
					return TRUE;
				}else{
					if (snooper_loopkup_keys_in_accel_map(kevent)) {
						snooper->stat = SNOOPER_HAS_EXCUTING_FUNC;
						return FALSE;
					}else{
						snooper->stat = SNOOPER_HALF_SEQ;
						return TRUE;
					}
				}
			}
		}else{
			snooper->stat = 0;
			return FALSE;
		}
	}else{/** key release **/
		if ( snooper->stat ==  SNOOPER_HALF_SEQ ) {
			DEBUG_MSG("snooper: in the middle of sequence; release event cancelled\n");
			return TRUE;
		}
	}
	return FALSE;
}

void snooper_install(Tbfwin *bfwin) {
	bfwin->snooper = g_new0(Tsnooper,1);
	SNOOPER(bfwin->snooper)->id = gtk_key_snooper_install( (GtkKeySnoopFunc) main_snooper, bfwin);
	SNOOPER(bfwin->snooper)->last_seq = gdk_event_new(GDK_KEY_PRESS);
	SNOOPER(bfwin->snooper)->last_event = gdk_event_new(GDK_KEY_PRESS);
}

#endif /* SNOOPER2 */
