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

#define ALL_ACCELS_MASK (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK)
#define KEVENT(var) ( (GdkEventKey*)var )

static gchar * snooper_parse_key(GdkEventKey *kevent) {
	gchar *tmpstr = NULL;
	const gchar *ctrl, *shift, *mod1;
	guint keyval;
	GdkModifierType consumed;
	gdk_keymap_translate_keyboard_state (NULL, kevent->hardware_keycode, kevent->state, kevent->group, &keyval, NULL, NULL, &consumed);
	ctrl = kevent->state & ~consumed & GDK_CONTROL_MASK ? "<control>": "";
	shift = kevent->state & ~consumed & GDK_SHIFT_MASK ? "<shift>": "";
	mod1 = kevent->state & ~consumed & GDK_MOD1_MASK ? "<mod1>": "";
	tmpstr = g_strdup_printf("%s%s%s%c", ctrl, shift, mod1, gdk_keyval_to_unicode(keyval));
	DEBUG_MSG("snooper: key parsed = %s\n", tmpstr);
	return tmpstr;
}

static gboolean snooper_loopkup_keyseq(GtkWidget *widget, Tbfwin *bfwin, GdkEventKey *kevent1, GdkEventKey *kevent2) {
	gchar *tmpstr, *r1, *r2;
	gchar *value;
	gboolean retval;

	r1 = snooper_parse_key(kevent1);

	if (kevent2) {
		r2 = snooper_parse_key(kevent2);
		tmpstr = g_strdup_printf("%s%s",r1,r2);
		g_free(r1);
		g_free(r2);
	}else{
		tmpstr = r1;
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
	SNOOPER(bfwin->snooper)->stat |= SNOOPER_HAS_EXCUTING_FUNC;
	DEBUG_MSG("snooper: lookup '%s' (full=%d), retval = %d\n", tmpstr, kevent2 != NULL, retval);
	g_free(tmpstr);
	return retval;
}

static gboolean snooper_accel_group_find_func(GtkAccelKey *key, GClosure *closure, gpointer data) {
	guint keyval;
	GdkModifierType consumed;
	gdk_keymap_translate_keyboard_state (NULL, KEVENT(data)->hardware_keycode, KEVENT(data)->state, KEVENT(data)->group, &keyval, NULL, NULL, &consumed);
#ifdef DEBUG_ALL
	DEBUG_MSG("snooper: _accel_group_find_func: compared (%d,%d) with (%d,%d)\n", key->accel_key, key->accel_mods, keyval, KEVENT(data)->state & ~consumed );
#endif /* DEBUG_ALL */
	return ( (key->accel_key == keyval) && (key->accel_mods == ( KEVENT(data)->state & ~consumed) ) );
}

#ifdef DONOT_WORK
static gboolean snooper_loopkup_keys_in_accel_map(GtkWidget *widget, GdkEventKey *kevent) {
	return gtk_accel_groups_activate(G_OBJECT(widget), kevent->keyval, kevent->state);
}

#else /* DONOT_WORK */

static gboolean snooper_loopkup_keys_in_accel_map(GdkEventKey *kevent, Tbfwin *bfwin) {
	gpointer retval;
	retval = gtk_accel_group_find( bfwin->accel_group, snooper_accel_group_find_func, kevent );
	DEBUG_MSG("snooper: lookup in accel.map (1)\n" );
	if (!retval) {
		DEBUG_MSG("snooper: lookup in accel.map (2)\n" );
		retval = gtk_accel_group_find( bfwin->accel_group2, snooper_accel_group_find_func, kevent );
	}
#ifdef DEBUG
	gchar *tmpstr = snooper_parse_key(kevent);
	DEBUG_MSG("snooper: lookup '%s' in accel.map, return %p\n", tmpstr, retval);
	g_free(tmpstr);
#endif /* DEBUG */
	return retval != NULL;
}

#endif /* DONOT_WORK */

static gint main_snooper (GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin) {
	Tsnooper *snooper =  SNOOPER(bfwin->snooper);

	/** check for valid snooper here **/
	if ( ! (snooper->stat & SNOOPER_ACTIVE) ) {
		DEBUG_MSG("snooper: snooper on bfwin(%p) was disabled...\n", bfwin);
		return FALSE;
	}

#ifdef DEBUG
	DEBUG_MSG("snooper: stat(%s)press(%d,%d)length(%d)widget(%s)\n", STAT_NAME(snooper->stat),(kevent->type == GDK_KEY_PRESS), kevent->keyval, kevent->length, gtk_widget_get_name(widget) );
#endif

	if (kevent->type == GDK_KEY_PRESS)
		SNOOPER(bfwin->snooper)->last_event = (GdkEvent *)kevent;

	if (  kevent->type == GDK_KEY_RELEASE
		     && ( snooper->stat & (SNOOPER_CANCEL_RELEASE_EVENT | SNOOPER_HAS_EXCUTING_FUNC ) ) ) {
		DEBUG_MSG("snooper: exit as stat = %s\n", STAT_NAME(snooper->stat));
		snooper->stat &= ~(SNOOPER_CANCEL_RELEASE_EVENT | SNOOPER_HAS_EXCUTING_FUNC);
		return TRUE;
	}

	/** special for completion **/
	if ( SNOOPER_COMPLETION_ON(bfwin) ) {
		DEBUG_MSG("snooper: completion on bfwin = %p\n", bfwin);
		if ( SNOOPER_COMPLETION_MOVE(kevent->keyval) ) {
			func_complete_move(kevent, bfwin);
			snooper->stat |= SNOOPER_CANCEL_RELEASE_EVENT;
			return TRUE;
		}else if ( SNOOPER_COMPLETION_ACCEPT(kevent->keyval) ) {
			func_complete_do(bfwin);
			snooper->stat |= SNOOPER_CANCEL_RELEASE_EVENT;
			return TRUE;
		}else if ( SNOOPER_COMPLETION_ESCAPE(kevent->keyval) ) {
			func_complete_hide(bfwin);
			snooper->stat |= SNOOPER_CANCEL_RELEASE_EVENT;
			return TRUE;
		}else if ( SNOOPER_COMPLETION_DELETE(kevent->keyval) ) {
			func_complete_delete(widget, bfwin);
			snooper->stat |= SNOOPER_CANCEL_RELEASE_EVENT;
			return TRUE;
		}else if ( snooper->stat & ~SNOOPER_ACTIVE ) {
			DEBUG_MSG("snooper: completion shown and snooper->stat >0. hide stuff...\n");
			func_complete_hide(bfwin);
		}
	}

	/** if completion is hidden **/
	if (kevent->type == GDK_KEY_PRESS) {
		if ( ( snooper->stat & ~SNOOPER_ACTIVE )&& ( kevent->keyval == GDK_Escape ) ) {
			snooper->stat |= SNOOPER_CANCEL_RELEASE_EVENT;
			return TRUE;
		}else if ( kevent->length && ( ( snooper->stat & SNOOPER_HALF_SEQ ) || SNOOPER_IS_KEYSEQ(kevent) ) ) {
			if (snooper->stat & SNOOPER_HALF_SEQ ) {
				snooper->stat &=  ~SNOOPER_HALF_SEQ;
				snooper_loopkup_keyseq(widget, bfwin, (GdkEventKey*) snooper -> last_seq, kevent);
				return TRUE;
			} else {
				*( (GdkEventKey*) snooper->last_seq )= *kevent;
				if (snooper_loopkup_keyseq(widget, bfwin, kevent, NULL) ) {
					return TRUE;
				}else{
					if (snooper_loopkup_keys_in_accel_map(kevent,bfwin)) {
						snooper->stat |= SNOOPER_HAS_EXCUTING_FUNC;
						return FALSE;
					}else{
						snooper->stat |= SNOOPER_HALF_SEQ;
						return TRUE;
					}
				}
			}
		}else if (kevent->length) {
			DEBUG_MSG("snooper: not seq; reset stat = 0\n");
			snooper->stat = SNOOPER_ACTIVE;
			return FALSE;
		}
	}else{/** key release **/
		if ( snooper->stat & SNOOPER_HALF_SEQ ) {
			DEBUG_MSG("snooper: in the middle of sequence; release event cancelled\n");
			return TRUE;
		}
	}
	return FALSE;
}

void snooper_install(Tbfwin *bfwin) {
	bfwin->snooper = g_new0(Tsnooper,1);
	gtk_key_snooper_install( (GtkKeySnoopFunc) main_snooper, bfwin);
	SNOOPER(bfwin->snooper)->last_seq = gdk_event_new(GDK_KEY_PRESS);
	SNOOPER(bfwin->snooper)->last_event = gdk_event_new(GDK_KEY_PRESS);
	/* SNOOPER(bfwin->snooper)->stat = SNOOPER_ACTIVE; */
}

#endif /* SNOOPER2 */
