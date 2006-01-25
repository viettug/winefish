/* $Id: snooper.c,v 1.4 2005/07/21 08:59:47 kyanh Exp $ */

/* Winefish LaTeX Editor
 * 
 * Completion support
 *
 * Copyright (c) 2005 KyAnh <kyanh@o2.pl>
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
 *
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "bluefish.h"
#include "snooper.h"

#undef SHOW_SNOOPER

/* UniKey stuff:
 this function will be call twice, once when key is pressed, once when key is released;
 snooper works well without turning on InputMethod (UniKey). It's hard to imagine. THIS IS
 A BUG of UniKey: I tried with other input methods and got not problems. *Note* that UniKey
 runs after snooper :) -- we can see this by #define SHOW_SNOOPER 1
*/

#define ALL_CONTROL_MASK ( GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD2_MASK | GDK_MOD3_MASK | GDK_MOD4_MASK | GDK_MOD5_MASK)

static gint completion_snooper (GtkWidget *widget, GdkEventKey *kevent, gpointer data) {
	if (kevent->type == GDK_KEY_PRESS) {
		main_v->lastkp_keyval = kevent->keyval;
		main_v->lastkp_hardware_keycode = kevent->hardware_keycode;
	}
#ifdef SHOW_SNOOPER
	if ( kevent->type == GDK_KEY_PRESS ) {
		g_print("\nsnooper: press\n");
	}else{
		g_print("snooper: release\n");
	}
	g_print("snooper: entering popup.show = %d\n", main_v->completion.show);
	guint32 character = gdk_keyval_to_unicode( kevent->keyval );
	g_print( "snooper: keyval=%d (or %X), character=%d, string=%s, state=%d, hw_keycode=%d\n", kevent->keyval, kevent->keyval, character, kevent->string, kevent->state, kevent->hardware_keycode );
	character = gdk_keyval_to_unicode( main_v->lastkp_keyval );
	g_print( "snooper: previous keyval=%d (or %X), character=%d, hw_keycode=%d\n", main_v->lastkp_keyval, main_v->lastkp_keyval, character, main_v->lastkp_hardware_keycode );
#endif
	if (main_v->completion.show == COMPLETION_AUTO_CALL) {
		return FALSE;
	} else if ( (kevent->keyval == GDK_Escape) && (main_v->completion.show == COMPLETION_WINDOW_SHOW) ) {
#ifdef SHOW_SNOOPER
		g_print("snooper: Escape key captured.\n");
#endif
		gtk_widget_hide( GTK_WIDGET( main_v->completion.window ));
		main_v->completion.show = COMPLETION_WINDOW_HIDE;
		return TRUE;
	} else if ( (kevent->keyval == GDK_Delete) && (main_v->completion.show == COMPLETION_WINDOW_SHOW) ) {
		if (kevent->type == GDK_KEY_PRESS ) {
			main_v->completion.show = COMPLETION_DELETE;
			return FALSE;
		}
		return TRUE;
		/* release-event will be ignored */
	} else if ( ( kevent->state & GDK_CONTROL_MASK ) && ( kevent->keyval == GDK_space ) ) {
		/* the popup is shown only key is press */
		if ( (main_v->completion.show < COMPLETION_WINDOW_SHOW) && (kevent->type == GDK_KEY_PRESS ) ) {
#ifdef SHOW_SNOOPER
			g_print("snooper: popup to be shown\n");
#endif
			main_v->completion.show = COMPLETION_FIRST_CALL; /* to be shown */
			if ( kevent->type == GDK_KEY_RELEASE ) {
				return TRUE; /* ignore the release key */
			}else{
#ifdef SHOW_SNOOPER
				g_print("snooper: CTRL + Space will be passed\n");
#endif
				return FALSE;
			}
		} else { /* press | release */
#ifdef SHOW_SNOOPER
			g_print("snooper: popup is active\n");
#endif
			return TRUE;
		}
	} else if ( (main_v->completion.show > COMPLETION_WINDOW_HIDE) && ((kevent->keyval == GDK_Return || kevent->keyval == GDK_Up || kevent->keyval == GDK_Down || kevent->keyval == GDK_Page_Up || kevent->keyval == GDK_Page_Down))) {
		if ( kevent->keyval == GDK_Return ) {
			main_v->completion.show = COMPLETION_WINDOW_ACCEPT;
			if ( kevent->type == GDK_KEY_PRESS ) {
				return TRUE;
			}else{
				return FALSE;
			}
		}
		if ( kevent->type == GDK_KEY_RELEASE ) {
			return TRUE;
		}
		if ( kevent->keyval == GDK_Up) {
			main_v->completion.show = COMPLETION_WINDOW_UP;
		} else if ( kevent->keyval == GDK_Down ) {
			main_v->completion.show = COMPLETION_WINDOW_DOWN;
		} else if ( kevent->keyval == GDK_Page_Down ) {
			main_v->completion.show = COMPLETION_WINDOW_PAGE_DOWN;
		} else if ( kevent->keyval == GDK_Page_Up ) {
			main_v->completion.show = COMPLETION_WINDOW_PAGE_UP;
		}
		return FALSE;
	} else {
#ifdef SHOW_SNOOPER
		g_print("snooper: -- auto call -- or nothing \n");
#endif
		if ( (main_v->completion.show == COMPLETION_WINDOW_SHOW ) && ( kevent->state & ALL_CONTROL_MASK)) {
		/* only SHIFT mask available here; this happens when user press SHIFT to get a Uppercase letters, for example. */
#ifdef SHOW_SNOOPER
			g_print("snooper: popup is showing. 'non-null-state' event will be canceled\n");
#endif
			return TRUE;
		} else /* if (kevent->keyval == GDK_BackSpace) */
			if (kevent->type == GDK_KEY_RELEASE)
		{
			/* auto show the form. TODO: Should be a main's properties */
			if ((kevent->state == 0) && ( (main_v->lastkp_keyval == GDK_braceleft) || ( (GDK_a <= main_v->lastkp_keyval) && (GDK_z >= main_v->lastkp_keyval)) || ((GDK_A <= main_v->lastkp_keyval) && (GDK_Z >= main_v->lastkp_keyval))) )
			{
#ifdef SHOW_SNOOPER
				g_print("snooper: previous: normal key. auto start completion...\n");
#endif
				main_v->completion.show = COMPLETION_AUTO_CALL;
			}
		}
	}
	return FALSE;
}

void snooper_install() {
	/* install a snooper for doc->view */
	main_v->snooper = gtk_key_snooper_install( (GtkKeySnoopFunc) completion_snooper, NULL);
}
