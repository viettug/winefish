/* $Id: gtk_easy.c 629 2007-03-30 17:10:58Z kyanh $ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 *
 * Copyright (C) 1999-2004 Olivier Sessink
 * Modified for Winefish (C) 2005 Ky Anh <kyanh@o2.pl> 
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
#include <stdlib.h> /* strtod() */
#include <string.h> /* strlen() */
#include <gdk/gdkkeysyms.h> /* GDK_Return */

#include "bluefish.h"
#include "gtk_easy.h"
#include "bf_lib.h"
#include "pixmap.h"
#include "gui.h" /* statusbar_message() */
#include "char_table.h" /* xml_escape() */

#ifdef WIN32
#define DIRSTR "\\"
#define DIRCHR 92
#else
#define DIRSTR "/"
#define DIRCHR '/'
#endif

/**
 * flush_queue:
 * 
 * handles the gtk and gdk queue
 * 
 * Return value: void
 **/
void flush_queue(void) {
	/*
	gint i=0;
	g_print("\n***[");
	*/
	while(g_main_context_pending(NULL)) {
		g_main_context_iteration (NULL, TRUE);
		/*
		i++;
		g_print(" %d",i);
		*/
	}
	/*g_print("]***");*/
}
/**
 * widget_get_string_size:
 * @widget: #GtkWidget* to put the string on
 * @string: #ghcar* with the string
 *
 * This function will calculate the width in pixels from the
 * string passed to it in string, using the font from widget
 *
 * Return value: #gint pixels
 */
gint widget_get_string_size(GtkWidget *widget, gchar *string) {
	PangoLayout *layout;
	gint retval = -1;
	layout = gtk_widget_create_pango_layout(widget,string);
	if (layout != NULL) {
		pango_layout_get_pixel_size(layout, &retval, NULL);
		g_object_unref(G_OBJECT (layout));
	}
	return retval;
}

void widget_set_visible(GtkWidget *widget, gboolean visible) {
	if (visible) {
		gtk_widget_show(widget);
	} else {
		gtk_widget_hide(widget);	
	}
}

/**
 * window_destroy:
 * @windowname: a #GtkWidget pointer to the window
 * 
 * Remove grab and signals and then destroy window
 * 
 * Return value: void
 **/
void window_destroy(GtkWidget * windowname) {
	DEBUG_MSG("window_destroy, windowname=%p, first the signal handlers\n", windowname);
	g_signal_handlers_destroy(G_OBJECT(windowname));
	DEBUG_MSG("window_destroy, then remove the grab\n");
	gtk_grab_remove(windowname);
	DEBUG_MSG("window_destroy, then destroy the widget\n");
	gtk_widget_destroy(windowname);
	DEBUG_MSG("window_destroy, done\n");
}

/**
 * window_close_by_widget_cb:
 * 	@widget: #GtkWidget the parent window of this widget will be destroyed
 * 	@data: gpointer, ignored
 *
 * 	destroy the toplevel window for widget
 *
 * Return value: void
 */
void window_close_by_widget_cb(GtkWidget * widget, gpointer data) {
	DEBUG_MSG("window_close_by_widget_cb, widget=%p\n", widget);
	window_destroy(gtk_widget_get_toplevel(widget));
}

/**
 * window_close_by_data_cb:
 * 	@widget: #GtkWidget, ignored
 * 	@data: #gpointer to a gtk-window which will be detroyed
 *
 * 	destroy a window using the gpointer data
 *
 * Return value: void
 */
void window_close_by_data_cb(GtkWidget * widget, gpointer data) {
	DEBUG_MSG("window_close_by_data_cb, data=%p\n", data);
	window_destroy(GTK_WIDGET(data));
}

/**
 * setup_toggle_item:
 * 	@ifactory: #GtkItemFactory * the itemfactory where the toggle item is in defined
 * 	@path: #gchar * the path in the itemfactory to find the toggle item
 * 	@state: #gint the state to put it in (0 or 1)
 * 
 * 	change the setting of a toggle item in a menu to state
 *
 * Return value: void
 */
void setup_toggle_item(GtkItemFactory * ifactory, gchar * path, gint state) {
	GtkWidget *toggle;
	toggle = gtk_item_factory_get_widget(ifactory, path);
	if (!toggle) {
		g_print("warning, menuitem \"%s\" does NOT exist! check the translation!\n", path);
		return;
	}
	DEBUG_MSG("setting path %s to %d\n",path,state);
	if ((GTK_CHECK_MENU_ITEM(toggle)->active) && state == 0) {
		GTK_CHECK_MENU_ITEM(toggle)->active = state;
		return;
	}
	if ((!GTK_CHECK_MENU_ITEM(toggle)->active) && state) {
		GTK_CHECK_MENU_ITEM(toggle)->active = 1;
		return;
	}
}

/**
 * string_apply:
 * @config_var: # gchar **, a pointer to a gchar pointer with where the content should be placed
 * 	@entry: the entry #GtkWidget * to read
 *
 * 	free the old string in **config_var, and replaces it with the new string from entry
 *
 * Return value:	void
 */
void string_apply(gchar ** config_var, GtkWidget * entry)
{
	gchar *tmpstring;
	DEBUG_MSG("string_apply, start\n");

	tmpstring = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	DEBUG_MSG("string_apply, tmpstring(%p)=%s\n", tmpstring, tmpstring);
	if (tmpstring) {
		if (config_var != NULL) {
			g_free(*config_var);
		}
		*config_var = tmpstring;
	}
#ifdef DEBUG
	else {
		DEBUG_MSG("string_apply, ***** tmpstring == NULL *****\n");
	}
#endif
}
/**
 * string_apply:
 * @config_var: #gint*, a pointer to a gint with where the content should be placed
 * 	@entry: the spinbutton or checkbox #GtkWidget * to read
 * @is_checkbox: a gboolean, TRUE if entry is a checkbox
 *
 * 	sets the content from the widget to the variable pointed to by the config_var pointer
 *
 * Return value: void
 */
void integer_apply(gint *config_var, GtkWidget * widget, gboolean is_checkbox) {
	if (is_checkbox) {
		*config_var = (GTK_TOGGLE_BUTTON(widget)->active);
	} else {
		*config_var = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
	}
	DEBUG_MSG("integer_apply, config_var(%p)=%i\n", config_var, *config_var);
}

/* added kyanh 20060127 */
void bitwise_apply(guint32 *config_var, GtkWidget * widget, gboolean is_checkbox, guint32 BIT) {
	guint32 tmp_val;
	if (is_checkbox) {
		tmp_val = (GTK_TOGGLE_BUTTON(widget)->active);
	} else {
		tmp_val = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
	}
	*config_var = SET_BIT(*config_var, BIT, tmp_val);
	DEBUG_MSG("bitwise_apply, config_var(%p)=%d\n", config_var, *config_var &  BIT);
}
		
/**
 * combo_with_popdown:
 * 	@setstring: #gchar* to set in textbox, if NULL it will be set ""
 * 	@which_list: #GList* to set in popdown widget
 * 	@editable: #gint if the combo should be editable (1 or 0)
 *
 * 	Create new combo and preset some values
 *
 * Return value: #GtkWidget* pointer to created combo
 */
GtkWidget *combo_with_popdown(const gchar * setstring, GList * which_list, gint editable) {
	GtkWidget *returnwidget;

	returnwidget = gtk_combo_new();
	gtk_combo_set_case_sensitive(GTK_COMBO(returnwidget),TRUE);
	if (which_list != NULL) {
		gtk_combo_set_popdown_strings(GTK_COMBO(returnwidget), which_list);
	}
	if (setstring != NULL) {
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(returnwidget)->entry), setstring);
	} else {
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(returnwidget)->entry), "");
	}
	if (editable == 0) {
		gtk_editable_set_editable(GTK_EDITABLE(GTK_ENTRY(GTK_COMBO(returnwidget)->entry)), FALSE);
	}
	gtk_combo_disable_activate(GTK_COMBO(returnwidget));
	gtk_entry_set_activates_default (GTK_ENTRY (GTK_COMBO (returnwidget)->entry), TRUE);
	return returnwidget;
}
/**
 * boxed_combo_with_popdown:
 * 	@setstring: #gchar* to set in textbox, if NULL it will be set ""
 * 	@which_list: #GList* to set in popdown widget
 * 	@editable: #gint if the combo should be editable (1 or 0)
 * @box: the #GtkWidget* box widget to add the combo to
 *
 * 	create a new combo with presets like in combo_with_popdown()
 * and add it to the box
 *
 * Return value: #GtkWidget* pointer to created combo
 */
GtkWidget *boxed_combo_with_popdown(const gchar * setstring, GList * which_list, gint editable, GtkWidget *box) {
	GtkWidget *returnwidget;
	
	returnwidget = combo_with_popdown(setstring, which_list, editable);
	gtk_box_pack_start(GTK_BOX(box), returnwidget, TRUE, TRUE, 3);
	return returnwidget;
}
/**
 * combo_with_popdown_sized:
 * 	@setstring: #gchar* to set in textbox, if NULL it will be set ""
 * 	@which_list: #GList* to set in popdown widget
 * 	@editable: #gint if the combo should be editable (1 or 0)
 * @width: #gint with the width in pixels the widget should be
 *
 * 	Create new combo and preset some values, with a horizontal size
 *
 * Return value: #GtkWidget* pointer to created combo
 */
GtkWidget *combo_with_popdown_sized(const gchar * setstring, GList * which_list, gint editable, gint width) {
	GtkWidget *returnwidget;
	
	returnwidget = combo_with_popdown(setstring, which_list, editable);
	gtk_widget_set_size_request(returnwidget, width , -1);
	return returnwidget;
}

/**
 * entry_with_text:
 * 	@setstring: #const gchar* if not NULL set this text
 * 	@max_lenght: #gint max. characters in the entry
 *
 * 	Create new entry with some preset values
 *
 * Return value: #GtkWidget* pointer to the new entry widget
 */
GtkWidget *entry_with_text(const gchar * setstring, gint max_lenght) {
	GtkWidget *returnwidget;

	if (max_lenght) {
		returnwidget = gtk_entry_new_with_max_length(max_lenght);
	} else {
		returnwidget = gtk_entry_new();
	}
	if (setstring) {
		gtk_entry_set_text(GTK_ENTRY(returnwidget), setstring);
	}
	gtk_entry_set_activates_default(GTK_ENTRY(returnwidget), TRUE);
	return returnwidget;
}
/**
 * boxed_entry_with_text:
 * 	@setstring: #const gchar* if not NULL set this text
 * 	@max_lenght: #gint max. characters in the entry
 * @box: the #GtkWidget* box widget to add the entry to
 *
 * 	Create new entry with some preset values, and add it to a box
 *
 * Return value: #GtkWidget* pointer to the new entry widget
 */
GtkWidget *boxed_entry_with_text(const gchar * setstring, gint max_lenght, GtkWidget *box) {
	GtkWidget *returnwidget;
	
	returnwidget = entry_with_text(setstring, max_lenght);
	gtk_box_pack_start(GTK_BOX(box), returnwidget, TRUE, TRUE, 0);
	return returnwidget;

}
/**
 * boxed_full_entry:
 * @labeltest: #const gchar * with the text for the label
 * 	@setstring: #const gchar* if not NULL set this text
 * 	@max_lenght: #gint max. characters in the entry
 * @box: the #GtkWidget* box widget to add the entry to
 *
 * 	Create new entry with some preset values, and add together 
 * with a label to a hbox, and add that hbox to
 * the box pointer
 *
 * Return value: #GtkWidget* pointer to the new entry widget
 */
GtkWidget *boxed_full_entry(const gchar * labeltext, gchar * setstring,gint max_lenght, GtkWidget * box) {
	GtkWidget *hbox, *return_widget, *label;

	hbox = gtk_hbox_new(FALSE,3);
	label = gtk_label_new_with_mnemonic(labeltext);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
	return_widget = boxed_entry_with_text(setstring, max_lenght, hbox);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), return_widget);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 3);

	return return_widget;
}
/**
 * checkbut_with_value:
 * @labeltest: #const gchar * with the text for the label
 * 	@which_config_int: #gint whether or not to set the checkbutton active
 *
 * 	Create new checkbutton with some value
 *
 * Return value: #GtkWidget* pointer to the new checkbutton widget
 */
GtkWidget *checkbut_with_value(gchar *labeltext, gint which_config_int) {
	GtkWidget *returnwidget;

	returnwidget = gtk_check_button_new_with_mnemonic(labeltext);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(returnwidget), which_config_int);
	return returnwidget;
}
/**
 * boxed_checkbut_with_value:
 * @labeltest: #const gchar * with the text for the label
 * 	@which_config_int: #gint whether or not to set the checkbutton active
 * @box: the #GtkWidget* box widget to add the entry to
 *
 * 	Create new checkbutton with some value, and add it to box
 *
 * Return value: #GtkWidget* pointer to the new checkbutton widget
 */
GtkWidget *boxed_checkbut_with_value(gchar *labeltext, gint which_config_int, GtkWidget * box) {
	GtkWidget *returnwidget;

	returnwidget = checkbut_with_value(labeltext, which_config_int);
	gtk_box_pack_start(GTK_BOX(box), returnwidget, FALSE, FALSE, 3);
	return returnwidget;
}
/**
 * radiobut_with_value:
 * @labeltest: #const gchar * with the text for the label
 * 	@enabled: #gint if the radiobutton is enabled or not
 * @prevbut: #GtkRadioButton* pointer to the first button or NULL if none
 *
 * 	Create new radiobutton with some value, and if there is a first button 
 * for this group, add this button to that group
 *
 * Return value: #GtkWidget* pointer to the new radiobutton widget
 */
GtkWidget *radiobut_with_value(gchar *labeltext, gint enabled, GtkRadioButton *prevbut) {
	GtkWidget *returnwidget;
	GSList *group=NULL;

	if (prevbut) {
		group = gtk_radio_button_group(prevbut);
	}	
	returnwidget = gtk_radio_button_new_with_mnemonic(group, labeltext);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(returnwidget), enabled);
	return returnwidget;
}
/**
 * boxed_radiobut_with_value:
 * @labeltest: #const gchar * with the text for the label
 * 	@enabled: #gint if the radiobutton is enabled or not
 * @prevbut: #GtkRadioButton* pointer to the first button or NULL if none
 * @box: the #GtkWidget* box widget to add the entry to
 *
 * 	Create new radiobutton with some value, and if there is a first button 
 * for this group, add this button to that group, and this button to box
 *
 * Return value: #GtkWidget* pointer to the new radiobutton widget
 */
GtkWidget *boxed_radiobut_with_value(gchar *labeltext, gint enabled, GtkRadioButton *prevbut, GtkWidget *box) {
	GtkWidget *returnwidget;

	returnwidget = radiobut_with_value(labeltext, enabled, prevbut);
	gtk_box_pack_start(GTK_BOX(box), returnwidget, FALSE, FALSE, 0);
	return returnwidget;
}

static gint is_int(gfloat testval) {
	DEBUG_MSG("is_int, (int)testval=%d\n", (int)testval);
	if ((gfloat)(((int) testval) - ((gfloat) testval )) == 0) {
		DEBUG_MSG("is_int, %f int!\n", testval);
		return 1;
	} else {
		DEBUG_MSG("is_int, %f float!\n", testval);
		return 0;
	}
}

/**
 * spinbut_with_value:
 * @value: #const gchar * with the value as string for the spinbut
 * 	@lower: a #gfloat with the lower value for the range
 * @upper: a #gfloat with the upper value for the range
 * @step_increment: a #gfloat with the stepsize for the spinbutton
 * @page_increment: a #gfloat with the pagesize (pgup/pgdn) for the spinbutton
 *
 * 	Create new spinbutton with the specified value and range
 *
 * Return value: #GtkWidget* pointer to the new spinbutton widget
 */
GtkWidget *spinbut_with_value(gchar *value, gfloat lower, gfloat upper, gfloat step_increment, gfloat page_increment) {
	GtkAdjustment *adj;
	GtkWidget *returnwidget;
	guint digits;
	double fvalue=0;

	if (value) {
		fvalue = strtod(value, NULL);
	}
	adj = (GtkAdjustment *) gtk_adjustment_new((gfloat)fvalue, (gfloat)lower, (gfloat)upper, step_increment, page_increment, 0.0);
	digits = (is_int(lower) ? 0 : 2);
	returnwidget = gtk_spin_button_new(adj, step_increment, digits);
/*	g_object_set(G_OBJECT(returnwidget), "numeric", TRUE, NULL);*/
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(returnwidget), FALSE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(returnwidget), FALSE);
/*	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(returnwidget), GTK_UPDATE_IF_VALID);*/
	if (!value) {
		gtk_entry_set_text(GTK_ENTRY(GTK_SPIN_BUTTON(returnwidget)), "");
	}
	
	return returnwidget;
}
/* THIS ONE IS NEVERUSED
GtkWidget *boxed_spinbut_with_value(gchar *value, gfloat lower, gfloat upper, gfloat step_increment, gfloat page_increment, GtkWidget *box) {
	GtkWidget *returnwidget;
	returnwidget = spinbut_with_value(value, lower, upper, step_increment, page_increment);
	gtk_box_pack_start(GTK_BOX(box), returnwidget, FALSE, FALSE, 0);
	return returnwidget;
}
*/
/**
 * optionmenu_with_value:
 * @options: a #gchar** NULL terminated array with option strings
 * @curval: a #gint with the current selected item
 *
 * Create new popupmenu from options, and set the selected index from curval
 *
 * Return value: #GtkWidget* pointer to the new menu widget
 * Created by: Rubén Dorta
 */

GtkWidget *optionmenu_with_value(gchar **options, gint curval) {
	GtkWidget *returnwidget;
	GtkWidget *menu, *menuitem;
	gchar **str;

	returnwidget = gtk_option_menu_new();
	menu = gtk_menu_new();
	str = options;

	while (*str) {
		DEBUG_MSG("prefs_optionmenu, adding %s to optionmenu\n", *str);
		menuitem = gtk_menu_item_new_with_label(_(*str));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		str++;
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(returnwidget), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(returnwidget), curval);
	return returnwidget;
}

/**
 * boxed_optionmenu_with_value:
 * @labeltext: a #const gchar* with the text for the label
 * @curval: a #gint with the current selected item
 * @box: a #GtkWidget* with the box to add this to
 * @options: a #gchar** NULL terminated array with option strings
 *
 * Create a new horizontal box with a labeltext and a new popupmenu using the options array
 * setting the selected index from curval
 *
 * Return value: #GtkWidget* pointer to the new menu widget
 * Modified by: Rubén Dorta
 */

GtkWidget *boxed_optionmenu_with_value(const gchar *labeltext, gint curval, GtkWidget *box, gchar **options) {
	GtkWidget *returnwidget;
	GtkWidget *hbox;

	hbox = gtk_hbox_new(FALSE,3);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(labeltext), FALSE, FALSE, 3);
	returnwidget = optionmenu_with_value(options, curval);
	gtk_box_pack_start(GTK_BOX(hbox), returnwidget, FALSE, FALSE, 3);
	return returnwidget;
}

/**
 * window_with_title:
 * 	@title: #gchar* the title string
 * 	@position: the gtk window position
 * 	@type: the gtk window type
 *
 * 	Create new window with title and some more settings
 *
 * Return value: #GtkWidget* pointer to created window
 */
GtkWidget *window_with_title(const gchar * title, GtkWindowPosition position, gint borderwidth) {
	GtkWidget *returnwidget;

	returnwidget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	if (title) {
		gtk_window_set_title(GTK_WINDOW(returnwidget), title);
	}
	if (borderwidth) {
		gtk_container_set_border_width(GTK_CONTAINER(returnwidget), borderwidth);
	}
	if (position) {
		gtk_window_set_position(GTK_WINDOW(returnwidget), position);
	}
#ifdef DEBUG
	 else {
		g_print("window_with_title, **NOT** setting position!!\n");
	}
#endif
	return returnwidget;
}
/* GtkWindowPosition can be 
GTK_WIN_POS_NONE
GTK_WIN_POS_CENTER
GTK_WIN_POS_MOUSE */

static gboolean window_full_key_press_event_lcb(GtkWidget *widget,GdkEventKey *event,GtkWidget *win) {
	DEBUG_MSG("window_full_key_press_event_lcb, started\n");
	if (event->keyval == GDK_Escape) {
		DEBUG_MSG("window_full_key_press_event_lcb, emit delete_event on %p\n", win);
/*		g_signal_emit_by_name(G_OBJECT(win), "delete_event");*/
		gtk_widget_destroy(win);
		DEBUG_MSG("window_full_key_press_event_lcb, DESTROYED %p\n", win);
		return TRUE;
	}
	return FALSE;
}

/**
 * window_full2:
 * 	@title: #gchar* the title string
 * 	@position: #gint the gtk window position GTK_WIN_POS_NONE, GTK_WIN_POS_CENTER, GTK_WIN_POS_MOUSE
 * 	@borderwidth: #gint border width
 * 	@close_func: #GCallback the callback function when closing the window
 * 	@close_data: #gpointer data passed to the closefunc, or NULL to pass the widget pointer as close data
 * @delete_on_escape: #gboolean if the escape key should close the dialog
 * @transientforparent: #GtkWidget* with the window to set this transient for, or NULL
 *
 * Create new window with title, callback functions, some more settings
 * and if needed set a callback so the window will be closed on escape press
 * and set it if needed transient for another window
 *
 * the close_func should have the form 
 * void close_func(GtkObject *object,gpointer user_data);
 *
 * Return value: #GtkWidget* pointer to created window
 */
GtkWidget *window_full2(const gchar * title, GtkWindowPosition position
			, gint borderwidth, GCallback close_func
			, gpointer close_data
			, gboolean delete_on_escape,
			GtkWidget *transientforparent)
{

	GtkWidget *returnwidget;

	returnwidget = window_with_title(title, position, borderwidth);
/*	g_signal_connect(G_OBJECT(returnwidget), "delete_event", close_func, close_data);*/
	/* use "destroy" and not "destroy_event", 'cause that doesn't work */
	if (close_data == NULL) close_data = returnwidget;
	g_signal_connect(G_OBJECT(returnwidget), "destroy", close_func, close_data);
	DEBUG_MSG("window_full, close_data=%p\n",close_data);
	if (transientforparent) {
		gtk_window_set_transient_for(GTK_WINDOW(returnwidget), GTK_WINDOW(transientforparent));
	}
	if (delete_on_escape) {
		g_signal_connect(G_OBJECT(returnwidget), "key_press_event", G_CALLBACK(window_full_key_press_event_lcb), returnwidget);
		/* for these windows it is also convenient if they destroy when their parent is destroyed */
		gtk_window_set_destroy_with_parent(GTK_WINDOW(returnwidget), TRUE);
	}
	DEBUG_MSG("window_full, return %p\n", returnwidget);
	return returnwidget;
}
/**
 * textview_buffer_in_scrolwin:
 * 	@textview: #GtkWidget** will be filled with the textview widget pointer
 * 	@width: #gint the width of the scrolwin, -1 if default required
 * 	@height: #gint the height of the scrolwin, -1 if default required
 * 	@contents: #gchar* the initial contents of the textbox
 * 	@wrapmode: #GtkWrapMode the wrapmode for the TextView GTK_WRAP_NONE,GTK_WRAP_CHAR,GTK_WRAP_WORD
 *
 * creates a textbuffer, a textview widget (stored in the textview pointer), 
 * puts that inside a scrolwin, adds the content and sets the size and returns
 * the scrolwin
 *
 * Return value: #GtkWidget* to the scolwin
 */
GtkWidget *textview_buffer_in_scrolwin(GtkWidget **textview, gint width, gint height, const gchar *contents, GtkWrapMode wrapmode) {
	GtkWidget *scrolwin;
	GtkTextBuffer *textbuf;
	
	textbuf = gtk_text_buffer_new(NULL);
	*textview = gtk_text_view_new_with_buffer(textbuf);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(*textview), wrapmode);
	scrolwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolwin), *textview);
	gtk_widget_set_size_request(scrolwin, width, height);
	if (contents) {
		gtk_text_buffer_set_text(textbuf, contents, -1);
	}
	return scrolwin;
}

/**
 * apply_font_style:
 * 	@this_widget: #GtkWidget* the widget to apply the new fontstyle
 * 	@fontstring: #gchar* the fontset which will be loaded
 *
 * 	Change the font of a widget with a certain font from a string
 *
 * Return value: #GtkWidget* to this_widget
 */
GtkWidget *apply_font_style(GtkWidget * this_widget, gchar * fontstring) {
	if (fontstring) {
		PangoFontDescription *font_desc;
		font_desc = pango_font_description_from_string(fontstring);
		gtk_widget_modify_font(this_widget, font_desc);
		pango_font_description_free(font_desc);
	}
	return this_widget;
}

/**
 * hbox_with_pix_and_text:
 * 	@label: #const gchar* with the text
 * 	@pixmap_type: #gint with a pixmap type known by new_pixmap() from pixmap.h
 *
 * constructs a hbox with a pixmap and text. The pixmap type should be known
 * to the new_pixmap() function from pixmap.c
 * This function is very useful to create a button with text and a pixmap
 *
 * Return value: #GtkWidget* to the hbox
 */
GtkWidget *hbox_with_pix_and_text(const gchar *label, gint bf_pixmaptype, gboolean w_mnemonic) {
	GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
	/* gtk_box_pack_start(GTK_BOX(hbox), new_pixmap(bf_pixmaptype), FALSE, FALSE, 1); */
	gtk_box_pack_start(GTK_BOX(hbox), ((w_mnemonic) ? gtk_label_new_with_mnemonic(label) : gtk_label_new(label)), TRUE, TRUE, 1);
	gtk_widget_show_all(hbox);
	return hbox;
}

GtkWidget *bf_allbuttons_backend(const gchar *label, gboolean w_mnemonic, gint bf_pixmaptype, GCallback func, gpointer func_data) {
	GtkWidget *button;
#ifdef USE_IMAGE_BUTTON
	if (bf_pixmaptype == -1) {
#endif
		/* there is no image needed, only text */
		if (w_mnemonic) {
			button = gtk_button_new_with_mnemonic(label);
		} else {
			button = gtk_button_new_with_label(label);
		}
#if USE_IMAGE_BUTTON
	} else {
		/* there is an image needed */
		button = gtk_button_new();
		if (label) {
			/* both a pixmap and text */
			gtk_container_set_border_width(GTK_CONTAINER(button), 0);
			gtk_container_add(GTK_CONTAINER(button), hbox_with_pix_and_text(label, bf_pixmaptype, w_mnemonic));
		} else {
			/* only pixmap */
			gtk_container_add(GTK_CONTAINER(button), new_pixmap(bf_pixmaptype));
		}
	}
#endif
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	g_signal_connect(G_OBJECT(button), "clicked", func, func_data);
	return button;
}

/**
 * bf_generic_button_with_image:
 * @label: #const gchar* button string with '_' for the mnemonic, or NULL
 * @pixmap_type: #gint image to display on button know to new_pixmap() from pixmap.c
 * @func: #GCallback pointer to signal handler
 * @func_data: #gpointer data for signal handler
 *
 * Create new button with an image and connect the "clicked" signal to func
 * if the label is NULL there will be only an image in the button
 *
 * Return value: #GtkWidget* pointer to created button
 */
/* GtkWidget *bf_generic_button_with_image(const gchar *label, gint pixmap_type, GCallback func, gpointer func_data) {
        GtkWidget *button;

	button = gtk_button_new();
	gtk_container_set_border_width(GTK_CONTAINER(button), 0);
	if (label) {
		gtk_container_add(GTK_CONTAINER(button), hbox_with_pix_and_text(label, pixmap_type));
	} else {
		gtk_container_add(GTK_CONTAINER(button), new_pixmap(pixmap_type));
	}
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	g_return_val_if_fail(button, NULL);
	g_signal_connect(G_OBJECT(button), "clicked", func, func_data);
	return button;
} */

/**
 * bf_generic_mnemonic_button:
 * @Text: #const gchar* button string, using '_' for the mnemonic
 * @func: #GCallback pointer to signal handler
 * @func_data: #gpointer data for signal handler
 *
 * 	Create new button with mnemonic and connect the "clicked" signal to func
 *
 * Return value: pointer to created button
 */
/* GtkWidget *bf_generic_mnemonic_button(const gchar * Text, GCallback func, gpointer func_data) {
	GtkWidget *button;

	button = gtk_button_new_with_mnemonic(Text);
	g_return_val_if_fail(button, NULL);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	g_signal_connect(G_OBJECT(button), "clicked", func, func_data);
	DEBUG_MSG("bf_generic_mnemonic_button, func_data=%p\n", func_data);
	return button;
} */
/**
 * bf_gtkstock_button:
 * @stock_id: #const gchar* wioth the GTK stock icon ID
 * @func: #GCallback pointer to signal handler
 * @func_data: #gpointer data for signal handler
 *
 * Create new button from the GTK stock icons
 *
 * Return value: pointer to created button
 */
GtkWidget *bf_gtkstock_button(const gchar * stock_id, GCallback func, gpointer func_data) {
	GtkWidget *button = gtk_button_new_from_stock(stock_id);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	g_signal_connect(G_OBJECT(button), "clicked", func, func_data);
	return button;
}

/**
 * bf_generic_frame_new:
 * @label: #const gchar* label string. Set to NULL for no label
 * @shadowtype: #GtkShadowType  border type
 * @borderwidth: #gint outside border width
 *
 * create a generic frame with shadow type and border
 *
 * Return value: #GtkWidget* pointer to created frame
 */
GtkWidget *bf_generic_frame_new(const gchar *label, GtkShadowType shadowtype, gint borderwidth) {
  GtkWidget *frame;

  frame = gtk_frame_new(label);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), shadowtype);
  gtk_container_set_border_width(GTK_CONTAINER(frame), borderwidth);
  
  return frame;
}

/**
 * bf_mnemonic_label_tad_with_alignment:
 * @labeltext: #const gchar* label string
 * @m_widget: #GtkWidget* widget accessed by the label mnemonic
 * @xalign: #gfloat label horizontal alignment
 * @yalign: #gfloat label vertical alignment
 * @table: #GtkWidget table label is packed into
 * @left_attach: #gint column number to attach the left side of the label to
 * @right_atach: #gint: column number to attach the right side of a label to
 * @top_attach: #gint: row number to attach the top of a label to
 * @bottom_attach: #gint: row number to attach the bottom of a label to 	
 *
 * 	create a label with a mnemonic, align it, and attach it to a table
 *
 * Return value: void
 */
void bf_mnemonic_label_tad_with_alignment(const gchar *labeltext, GtkWidget *m_widget,
						float xalign, gfloat yalign, GtkWidget *table, guint left_attach, 
						guint right_attach, guint top_attach, guint bottom_attach) {
	GtkWidget *label;

	label = gtk_label_new_with_mnemonic(labeltext);
	gtk_misc_set_alignment(GTK_MISC(label), xalign, yalign);
	gtk_table_attach(GTK_TABLE(table), label, left_attach, right_attach, top_attach, bottom_attach, GTK_FILL, GTK_FILL, 0, 0);
  
	if (m_widget != NULL) {
		if (GTK_IS_COMBO(m_widget)) {
			gtk_label_set_mnemonic_widget(GTK_LABEL(label), (GTK_COMBO(m_widget)->entry));
			gtk_entry_set_activates_default(GTK_ENTRY(GTK_COMBO(m_widget)->entry), TRUE);
		} else gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_widget);
	}
}

/**
 * bf_label_with_markup:
 * @labeltext: #const gchar* labeltext string
 */
GtkWidget *bf_label_with_markup(const gchar *labeltext) {
	GtkWidget *label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), labeltext);
	return label;
}
/**
 * bf_label_tad_with_markup:
 * @labeltext: #const gchar* label string
 * @xalign: #gfloat label horizontal alignment
 * @yalign: #gfloat label vertical alignment
 * @table: #GtkWidget* table label is packed into
 * @left_attach: #guint column number to attach the left side of the label to
 * @right_atach: #guint column number to attach the right side of a label to
 * @top_attach: #guint row number to attach the top of a label to
 * @bottom_attach: #guint row number to attach the bottom of a label to 	
 *
 *	create a label with markup, align it, and attach it to a table
 * using the table defaults
 *
 * Return value: void
 */
void bf_label_tad_with_markup(const gchar *labeltext, gfloat xalign, gfloat yalign,
								GtkWidget *table, guint left_attach, guint right_attach, 
								guint top_attach, guint bottom_attach) {
	GtkWidget *label;
	label = bf_label_with_markup(labeltext);
	gtk_misc_set_alignment(GTK_MISC(label), xalign, yalign);
	gtk_table_attach_defaults(GTK_TABLE(table), label, left_attach, right_attach, top_attach, bottom_attach);	
}
#ifdef NOTYETUSED
typedef struct {
	GtkWidget *win;
	GtkWidget *entry[10];
	gpointer data;
} Tmultientrywidget;

static void multi_entry_destroy(GtkObject *object, Tmultientrywidget *mew) {
	g_free(mew);
}

static void multi_entry_cancel_clicked(GtkWidget *widget, Tmultientrywidget *mew) {
	gtk_widget_destroy(mew->win);
}

Tmultientrywidget *build_multi_entry_window(gchar *title,GCallback ok_func
			,gpointer data, const gchar **labelarr) {
	gint arrlen,i;
	GtkWidget *table,*hbox,*but;

	arrlen = count_array(labelarr);
	if (arrlen >10) arrlen = 10;
	Tmultientrywidget *mew = g_new0(Tmultientrywidget,1);
	mew->data = data;
	mew->win = window_full2(title, GTK_WIN_POS_MOUSE, 10, multi_entry_destroy,mew, TRUE, NULL);
	table = gtk_table_new(arrlen+2,2,FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table),6);
	gtk_table_set_col_spacings(GTK_TABLE(table),6);
	gtk_container_add(GTK_CONTAINER(mew->win),table);
	for (i=0;i<arrlen;i++) {
		GtkWidget *label = gtk_label_new(labelarr[i]);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, i, i+1);
		mew->entry[i] = entry_with_text(NULL, 255);
		gtk_table_attach_defaults(GTK_TABLE(table), mew->entry[i], 1, 2, i, i+1);
	}
	gtk_table_attach_defaults(GTK_TABLE(table), gtk_hseparator_new(), 0, 2, i, i+1);
	hbox = gtk_hbutton_box_new();
	gtk_hbutton_box_set_layout_default(GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 12);
	but = bf_stock_cancel_button(G_CALLBACK(multi_entry_cancel_clicked), NULL);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 0);
	but = bf_stock_ok_button(G_CALLBACK(ok_func), mew);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 2, i+1, i+2);
	gtk_widget_show_all(mew->win);
}
#endif
/**************************************************************************/
/***********************  BUTTON DIALOG FUNCTIONS  ************************/
/**************************************************************************/
/*    If this code doesn't work, blame Christian <chris@tellefsen.net>    */

/* Emit a delete-event to the appropriate dialog, to detect a user bailing through escape. */
static gboolean dialog_key_press_event_lcb(GtkWidget *widget,GdkEventKey *event,GtkWidget *dialog) {
	DEBUG_MSG("dialog_key_press_event_lcb, started, widget is %p and dialog is %p\n", widget, dialog);
	if (event->keyval == GDK_Escape) {
		DEBUG_MSG("dialog_key_press_event_lcb, emit delete_event on %p\n", dialog);
		g_signal_emit_by_name(G_OBJECT(dialog), "delete_event");
		/* gtk_widget_destroy(dialog); */
		return TRUE;
	}
	return FALSE;
}

/* This function is called from single_button_dialog_backend() and multiple_button_dialog_backend()
 * The dialog passed is made HIG-compliant.
 *
 * A HiG-dialog is consists of a GtkDialog, with a 2-cell vbox. The top
 * vbox contains an hbox, with an image and a label. The label has a primary
 * (strong && larger) message and an additional, more descriptive message.
 * It is not resizeable, has no title, placement is selected by the wm.
 */
static void hig_dialog_backend (GtkDialog *dialog, gchar *primary, gchar *secondary, gchar *icon)
{
	GtkWidget *vbox, *hbox, *image, *label;
	gchar *message;
	gchar *spanstart = "<span weight=\"bold\" size=\"larger\">";
	gchar *spanend = "</span>\n\n";
	gchar *msgend = "\n";
	
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	vbox = GTK_DIALOG (dialog)->vbox;
	
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);

	if(icon) {
		image = gtk_image_new_from_stock (icon , GTK_ICON_SIZE_DIALOG); /* icon unknown ==> "broken image" displayed. */
		gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
		gtk_misc_set_alignment (GTK_MISC (image), 0, 0);
	}
	
	if(secondary) { /* Creates label-content. */
		gchar *str1, *str2;
		str1 = xml_escape(primary);
		str2 = xml_escape(secondary);
		message = g_strconcat(spanstart, str1, spanend, str2, msgend, NULL);
	} else {
		gchar *str1;
		str1 = xml_escape(primary);
		message = g_strconcat(spanstart, str1, spanend, NULL);
	}
			
	label = gtk_label_new (message);
	g_free(message);

	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
}

/**
 * multi_button_dialog_backend:
 * @win: #GtkWidget* with the parent window for this dialog
 * @primary: #gchar* containing primary dialog message.
 * @secondary: #gchar* containing more explanatory message. Set to NULL if unused.
 * @icon: #gchar* Your stock GTK image, i.e. GTK_STOCK_DIALOG_WARNING.
 * @defval: #gint default index when user press ENTER.
 * @cancelval: #gint: default index when user press ESCAPE. Should be something non-fatal. If this value is -1, the escape-button will be disabled.
 * @buttons: #gchar** array of buttons. NULL-terminated!! Must contain at least one button.
 *
 * create a modal dialog with several buttons, returning the index of the clicked button
 *
 * Return value: #gint the index num of the button label which was clicked	so 0 for the first, 1 for the 2nd etc.
 */
gint multi_button_dialog_backend(GtkWidget *win, gchar *primary, gchar *secondary, gchar *icon, gint defval, gint cancelval, gchar **buttons) {
	GtkWidget *dialog;
	int i, retval;

	dialog = gtk_dialog_new_with_buttons (NULL, /* Note that no title is being set. */
		GTK_WINDOW(win), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, *buttons, 0, NULL);
	DEBUG_MSG ("multi_button_dialog_backend: dialog=%p, primary=%s\n", dialog, primary);
	/* Add buttons. The index must be correct! */
	for (i = 1; *++buttons; i++) { /* Walk through the button-list until one reaches NULL. */
		DEBUG_MSG ("multi_button_dialog_backend: Adding button '%s', ID=%d\n", *buttons, i);
		gtk_dialog_add_button (GTK_DIALOG (dialog), *buttons, i);
	}

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), defval); /* enter-default*/
	if(cancelval != -1)/* enable escape ? */
		g_signal_connect(G_OBJECT(dialog), "key_press_event",
				G_CALLBACK(dialog_key_press_event_lcb), dialog); /* kill when escape is hit */

	hig_dialog_backend (GTK_DIALOG (dialog), primary, secondary, icon);
	gtk_widget_show_all (dialog);
	retval = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	DEBUG_MSG("multi_button_dialog_backend: got %d\n", retval);	
	return (retval == GTK_RESPONSE_DELETE_EVENT) ? cancelval : retval; /* When user hits escape -- give cancelval. */
} 

/**
 * single_button_dialog_backend:
 * @primary #gchar* containing primary dialog message.
 * @secondary #gchar* containing more explanatory message. Set to NULL if unused.
 * @icon #gchar* Your stock GTK image, i.e. GTK_STOCK_DIALOG_WARNING.
 * 
 * Create and display a single-button message-dialog - HiG-style.
 **/
void single_button_dialog_backend(GtkWidget *win,gchar * primary, gchar * secondary, gchar * icon) {
	gchar *buttons[] = {GTK_STOCK_OK, NULL};
	multi_button_dialog_backend(win,primary, secondary, icon, 0, 0, buttons);
}

/**
 * error_dialog:
 * @primary: #gchar* the primary error-description
 * @secondary: #gchar* the secondary description of the error or NULL.
 *
 * 	Display an error dialog, Gnome HiG style
 *
 * Return value: void
 */
void error_dialog(GtkWidget *win,gchar * primary, gchar * secondary) {
	single_button_dialog_backend(win,primary, secondary, GTK_STOCK_DIALOG_ERROR);
}

/**
 * info_dialog:
 * @primary: #gchar* the primary error-description
 * @secondary: #gchar* the secondary description of the error or NULL.
 *
 * 	Display an informational dialog, Gnome HiG style
 *
 * Return value: void
 */
void info_dialog(GtkWidget *win,gchar * primary, gchar * secondary) {
	single_button_dialog_backend(win,primary, secondary, GTK_STOCK_DIALOG_INFO);
}

/**
 * warning_dialog:
 * @primary: #gchar* the primary error-description
 * @secondary: #gchar* the secondary description of the error or NULL.
 *
 * 	Display a warning dialog, Gnome HiG style
 *
 * Return value: void
 */
void warning_dialog(GtkWidget *win,gchar * primary, gchar * secondary) {
	single_button_dialog_backend(win,primary, secondary, GTK_STOCK_DIALOG_WARNING);
}

/* multi_button_dialog and multi_stockbutton_dialog was here */

/**
 * multi_error_dialog:
 * @primary: #gchar* the primary error-description
 * @secondary: #gchar* the secondary description of the error or NULL.
 * @defval: #gint default index when user press ENTER.
 * @cancelval: #gint: default index when user press ESCAPE. Should be something non-fatal. If this value is -1, the escape-button will be disabled.
 * @buttons: #gchar** NULL terminated array of strings with button labels
 *
 * Create a modal error-dialog with several buttons, returning the index of the clicked button
 *
 * Return value: #gint the index num of the button label which was clicked	so 0 for the first, 1 for the 2nd etc.
 */
gint multi_error_dialog(GtkWidget *win,gchar *primary, gchar *secondary, gint defval, gint cancelval, gchar **buttons) {
	return multi_button_dialog_backend(win,primary, secondary, GTK_STOCK_DIALOG_ERROR, defval, cancelval, buttons);
}

/**
 * multi_warning_dialog:
 * @primary: #gchar* the primary error-description
 * @secondary: #gchar* the secondary description of the error or NULL.
 * @defval: #gint default index when user press ENTER.
 * @cancelval: #gint: default index when user press ESCAPE. Should be something non-fatal. If this value is -1, the escape-button will be disabled.
 * @buttons: #gchar** NULL terminated array of strings with button labels
 *
 * Create a modal warning-dialog with several buttons, returning the index of the clicked button
 *
 * Return value: #gint the index num of the button label which was clicked	so 0 for the first, 1 for the 2nd etc.
 */
gint multi_warning_dialog(GtkWidget *win,gchar *primary, gchar *secondary, gint defval, gint cancelval, gchar **buttons) {
	return multi_button_dialog_backend(win,primary, secondary, GTK_STOCK_DIALOG_WARNING, defval, cancelval, buttons);
}
 
/**
 * multi_error_dialog:
 * @primary: #gchar* the primary error-description
 * @secondary: #gchar* the secondary description of the error or NULL.
 * @defval: #gint default index when user press ENTER.
 * @cancelval: #gint: default index when user press ESCAPE. Should be something non-fatal. If this value is -1, the escape-button will be disabled.
 * @buttons: #gchar** NULL terminated array of strings with button labels
 *
 * Create a modal question-dialog with several buttons, returning the index of the clicked button
 *
 * Return value: #gint the index num of the button label which was clicked	so 0 for the first, 1 for the 2nd etc.
 */
gint multi_query_dialog(GtkWidget *win,gchar *primary, gchar *secondary, gint defval, gint cancelval, gchar **buttons) {
	return multi_button_dialog_backend(win,primary, secondary, GTK_STOCK_DIALOG_QUESTION, defval, cancelval, buttons);
}

/************************************************************************/
/*********************** PROGRESS-BAR FUNCTIONS *************************/
/************************************************************************/
/* Progress-bar-inspiration from the gtk-tutorial.. */

typedef struct _Tprogress {
	GtkWidget *bar;
	gboolean active; /* if FALSE, the bar is set to "confused-mode". */
	gboolean show_text; /* TRUE: String added to bar: text " " value " of  " maxvalue'*/
	gchar *text; /* Optional text to be displayed. */
	guint value; /* The current value. */
	guint maxvalue; /* The maximum value. Used to calculate a fraction between 0 and 1. */
	GtkWidget *owner; /* The widget to be destroyed when the bar is finished. */
	guint timer; /* keep track of the timer */

} Tprogress;

void progress_destroy(gpointer gp)
{
	if(gp) {
		Tprogress *p = (Tprogress *) gp;
		/* bar is destroyed when owner is destroyed. */
		if(p->owner)
			gtk_widget_destroy (p->owner);
		else
			gtk_widget_destroy(p->bar);
			
		gtk_timeout_remove(p->timer);
		p->timer = 0;
		
		g_free(p);
	}
}

static gboolean progress_update(gpointer data) {
	gchar *msg;
	Tprogress *p = (Tprogress *) data;
	gdouble frac = (gdouble) p->value / (gdouble) p->maxvalue;
	DEBUG_MSG("Entering: progress_update()\n");

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (p->bar), frac);
	msg = g_strdup_printf (_("%d of %d"), p->value, p->maxvalue);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (p->bar), msg);
	g_free (msg);
	
	if (p->value < p->maxvalue) {
/*		flush_queue ();*/
		return TRUE; /* Yes! Call me again! GIMMI! */
	} else {
		/* We're done! Signal that we're maxed now. */
		return FALSE;
	}
}

/**
 * progress_set:
 * @p The Tprogress struct assigned by a progress-creator. (i.e. progress_popup).
 * @value a #guint between 0 and p->maxvalue.
 *
 * Set a new value for the progressbar-value. Does not actually update the progress-bar.
 **/
void progress_set(gpointer gp, guint value)
{
	Tprogress *p = (Tprogress *) gp;
	p->value = value;
}

/**
 * progress_popup:
 *
 * Creates a popup containing a progress-bar. The dialog disappears when value = maxvalue.
 * 'text' must be freeed after use!
 *
 * Returns: #gpointer camouflaged pointer to struct Tprogress, a data-structure passed to progress_update.
 **/
gpointer progress_popup(GtkWidget *win,gchar *title, guint maxvalue) {
	Tprogress *p;
	
	p = g_malloc (sizeof (Tprogress));

	p->bar = gtk_progress_bar_new ();
	p->value = 0;
	p->maxvalue = maxvalue;
	
	
	/* other parameters */
	p->active = TRUE;
	p->show_text = TRUE;
	p->value = 0;
	p->maxvalue = maxvalue;

	p->owner = gtk_dialog_new();

	gtk_window_set_modal (GTK_WINDOW (p->owner), TRUE);
	gtk_window_set_transient_for (GTK_WINDOW (p->owner), GTK_WINDOW(win));


	/* Label, if applicable. Append pretty icon! */
	hig_dialog_backend(GTK_DIALOG (p->owner), title, NULL, GTK_STOCK_DIALOG_INFO);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (p->owner)->vbox),
                        p->bar, TRUE, TRUE, 12);

	p->timer = gtk_timeout_add (500, progress_update, p);
	gtk_widget_show_all (p->owner);
	
	return (gpointer) p;
}


/************************************************************************/
/************************ file_but_* FUNCTIONS **************************/
/************************************************************************/

typedef struct {
	GtkWidget *entry;
	Tbfwin *bfwin;
	gboolean fullpath;
} Tfilebut;

static void file_but_clicked_lcb(GtkWidget * widget, Tfilebut *fb) {
	gchar *tmpstring=NULL, /* *tmp2string, */ *setfile;
	DEBUG_MSG("file_but_clicked_lcb, started, which_entry=%p\n",fb->entry);
	setfile = gtk_editable_get_chars(GTK_EDITABLE(GTK_ENTRY(fb->entry)),0,-1);
	/* if setfile is empty we should probably use the current document basedir ? right? */
	if (!setfile || strlen(setfile)==0) {
		if (fb->bfwin->current_document->filename && strlen(fb->bfwin->current_document->filename)) {
			if (setfile) g_free(setfile);
			setfile = path_get_dirname_with_ending_slash(fb->bfwin->current_document->filename);
		}
	} else if (setfile && strchr(setfile, '/') == NULL && fb->bfwin->current_document->filename) {
		/* if setfile is a relative name, we should try to make it a full path. relative names have no slashes in the name */
		gchar *basedir, *oldsetfile;
		oldsetfile = setfile;
		basedir = path_get_dirname_with_ending_slash(fb->bfwin->current_document->filename);
		setfile = create_full_path(oldsetfile, basedir);
		g_free(oldsetfile);
		g_free(basedir);
	}
#ifdef HAVE_ATLEAST_GTK_2_4
	{
		GtkWidget *dialog;
		gchar *tmp2string;
		dialog = file_chooser_dialog(NULL, _("Select File"), GTK_FILE_CHOOSER_ACTION_OPEN, setfile, TRUE /* local only */, FALSE, NULL);
		gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(gtk_widget_get_toplevel(fb->entry)));
		if ( g_path_is_absolute(setfile) ) {/* valid setfile, BUG#37 */
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog),setfile);
		}
		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
			tmp2string = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
			tmpstring = create_full_path(tmp2string, NULL);
			g_free(tmp2string);
		}
		gtk_widget_destroy(dialog);
	}
#else
	tmpstring = return_file(setfile);
#endif

	g_free(setfile);
	DEBUG_MSG("file_but_clicked_lcb, return_file returned %s\n",tmpstring);
	if (tmpstring) {
		if (!fb->fullpath/*&& fb->bfwin*/) {
			tmpstring = g_path_get_basename(tmpstring);
		}
		gtk_entry_set_text(GTK_ENTRY(fb->entry), tmpstring);
/*	perhaps I break something by commenting-out this call, but otherwise the dialog is sometimes started
	again after the signal is emmitted
		gtk_signal_emit_by_name(GTK_OBJECT(which_entry), "activate");
		g_free(tmp2string); */
	}
}

static void file_but_destroy(GtkObject *object, Tfilebut *fb) {
	g_free(fb);
}
/**
 * file_but_new:
 * @which_entry: #GtkWidget* GTK_ENTRY where to put the filename
 * @win: #GtkWidget* the GTK_WINDOW where the button is placed
 * @full_pathname: #gint 1 or 0, if you want the full path or a relative path
 * #bfwin: #Tbfwin, only required if you want a relative pathname
 *
 *	Create new "open file" button, that will put the string into an entry
 *
 * Return value: #GtkWidget* pointer to file button
 */
GtkWidget *file_but_new(GtkWidget * which_entry, gint full_pathname, Tbfwin *bfwin) {
	GtkWidget *file_but;
	Tfilebut *fb;
	fb = g_new(Tfilebut,1);
	fb->entry = which_entry;
	fb->bfwin = bfwin;
	fb->fullpath = full_pathname;
	file_but = gtk_button_new();
	g_signal_connect(G_OBJECT(file_but), "destroy", G_CALLBACK(file_but_destroy), fb);
	DEBUG_MSG("file_but_new, entry=%p, button=%p\n",which_entry,file_but);
	gtk_container_add(GTK_CONTAINER(file_but), hbox_with_pix_and_text(_("_Browse..."), 112,TRUE));
	g_signal_connect(G_OBJECT(file_but), "clicked", G_CALLBACK(file_but_clicked_lcb), fb);
	gtk_widget_show(file_but);
	return file_but;
}

/************************************************************************/
/************    FILE SELECTION FUNCTIONS  ******************************/
/************************************************************************/

#ifdef HAVE_ATLEAST_GTK_2_4

static void viewlocal_toggled_lcb(GtkToggleButton *togglebutton,GtkWidget *dialog) {
	g_object_set(G_OBJECT(dialog), "show-hidden", togglebutton->active, NULL);
}

static void file_chooser_set_current_dir(GtkWidget *dialog, gchar *dir) {
	if (dir) {
		if (dir[0] == '/') gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),dir);
		else gtk_file_chooser_set_current_folder_uri(GTK_FILE_CHOOSER(dialog),dir);
	}
}

GtkWidget * file_chooser_dialog(Tbfwin *bfwin, gchar *title, GtkFileChooserAction action, gchar *set, gboolean localonly, gboolean multiple, const gchar *filter) {
	GtkWidget *vbox, *hbox, *dialog, *viewlocal;
	/* kyanh, removed, 20050619
	dialog = gtk_file_chooser_dialog_new_with_backend(title,bfwin ? GTK_WINDOW(bfwin->main_window) : NULL, action,"gnome-vfs", GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			(action == GTK_FILE_CHOOSER_ACTION_SAVE) ? GTK_STOCK_SAVE : GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);
	*/
	dialog = gtk_file_chooser_dialog_new(title,bfwin ? GTK_WINDOW(bfwin->main_window) : NULL, action, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			(action == GTK_FILE_CHOOSER_ACTION_SAVE) ? GTK_STOCK_SAVE : GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);
	gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	if (set && strlen(set)) {
		DEBUG_MSG("file_chooser_dialog, set=%s,localonly=%d\n",set,localonly);
		if (localonly || strchr(set,':')==NULL) {
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog),set);
		} else {
			gtk_file_chooser_set_uri(GTK_FILE_CHOOSER(dialog),set);
		}
	} else if (!localonly && bfwin) { /* localonly is used for the project files */
		if (action == GTK_FILE_CHOOSER_ACTION_SAVE) {
			DEBUG_MSG("file_chooser_dialog, opendir=%s, savedir=%s\n",bfwin->session->opendir,bfwin->session->savedir);
			if (bfwin->session->savedir) file_chooser_set_current_dir(dialog,bfwin->session->savedir);
			else if (bfwin->session->opendir) file_chooser_set_current_dir(dialog,bfwin->session->opendir);
		} else {
			DEBUG_MSG("file_chooser_dialog, opendir=%s\n",bfwin->session->opendir);
			file_chooser_set_current_dir(dialog,bfwin->session->opendir);
		}
	}
#ifdef DEBUG
	DEBUG_MSG("file_chooser_dialog, current_folder_uri=%s\n", gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER(dialog)));
#endif
	gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog),localonly);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), multiple);
	vbox = gtk_vbox_new(FALSE, 5);
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
	viewlocal = boxed_checkbut_with_value(_("Show hidden"), 0, hbox);
	g_signal_connect(G_OBJECT(viewlocal), "toggled", G_CALLBACK(viewlocal_toggled_lcb), dialog);
	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dialog),vbox);
	if (action == GTK_FILE_CHOOSER_ACTION_OPEN || action == GTK_FILE_CHOOSER_ACTION_SAVE){
		GList *tmplist;
		GtkFileFilter* ff;
		ff = gtk_file_filter_new();
		gtk_file_filter_set_name(ff,_("All files"));
		gtk_file_filter_add_pattern(ff, "*");
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ff);
		if (filter == NULL) {
			gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), ff);
		}
		tmplist = g_list_first(main_v->filetypelist);
		while (tmplist) {
			gchar **tmp;
			Tfiletype *ft = (Tfiletype *)tmplist->data;
			if ((strcmp(ft->type, "objectfile") != 0) && ft->extensions && *ft->extensions) {
				ff = gtk_file_filter_new();
				gtk_file_filter_set_name(ff,ft->type);
				DEBUG_MSG("file_chooser_dialog, adding filter '%s'\n", ft->type);
				tmp = ft->extensions;
				while (*tmp) {
					gchar *pattern;
					pattern = g_strconcat("*", *tmp, NULL);
					gtk_file_filter_add_pattern(ff, pattern);
					DEBUG_MSG("file_chooser_dialog, adding pattern '%s' to '%s'\n", pattern, ft->type);
					g_free(pattern);
					tmp++;
				}
				gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ff);
				if (filter && strcmp(filter, ft->type) == 0)	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), ff);
			}
			tmplist = g_list_next(tmplist);
		}
	}
	gtk_widget_show_all(vbox);
	return dialog;
}

#else
typedef struct {
	gboolean select_dir;
	gint multipleselect;
	gchar *filename_to_return;
	GList *filenames_to_return;
	GtkWidget *fs;
} Tfileselect;

static void fs_ok_clicked_lcb(GtkWidget * widget, Tfileselect *fileselect)
{
	gchar *dirname, *selected_file;

	selected_file = get_utf8filename_from_on_disk_encoding(gtk_file_selection_get_filename(GTK_FILE_SELECTION(fileselect->fs)));

	if (g_file_test(selected_file, G_FILE_TEST_IS_DIR)) {
		DEBUG_MSG("fs_ok_clicked_lcb,file_is_dir said %s is a dir!!!!\n", selected_file);
		if (fileselect->select_dir) {
			fileselect->filename_to_return = g_path_get_dirname(selected_file);
			g_free(selected_file);
			gtk_main_quit();
			window_destroy(GTK_WIDGET(fileselect->fs));
			return;
		} else {
			dirname = ending_slash(selected_file);
			gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileselect->fs), dirname);
			gtk_entry_set_text(GTK_ENTRY(GTK_FILE_SELECTION(fileselect->fs)->selection_entry), "");
			g_free(dirname);
			g_free(selected_file);
			return;
		}
	} else {
		DEBUG_MSG("fs_ok_clicked_lcb,file_is_dir said %s is NOT a dir.\n", selected_file);
	}

	if (fileselect->multipleselect) {
		gchar **filenames, **orig;
		/* multiple files allowed --> scan trough the list for selections */
		orig = filenames = gtk_file_selection_get_selections(GTK_FILE_SELECTION(fileselect->fs));
		if (filenames) {
			while (*filenames) {
				fileselect->filenames_to_return = g_list_append(fileselect->filenames_to_return, get_utf8filename_from_on_disk_encoding(*filenames));
				filenames++;
			}
			g_strfreev(orig);
		}
	} else {
		/* NO multiple files allowed --> return just one file */
		if (fileselect->select_dir) {
			fileselect->filename_to_return = g_path_get_dirname(selected_file);
		} else {
				fileselect->filename_to_return = g_strdup(selected_file);
		}
	}
	if ((fileselect->multipleselect 
			&& (g_list_length(fileselect->filenames_to_return) < 1)) 
		|| (!fileselect->multipleselect 
			&& strlen(fileselect->filename_to_return) < 1)){
/*		statusbar_message(_("No file to insert."), 2000);*/
	}
	
	g_free(selected_file);	
	gtk_main_quit();
	window_destroy(GTK_WIDGET(fileselect->fs));
}

void close_modal_window_lcb(GtkWidget * widget, gpointer window)
{
	DEBUG_MSG("close_modal_window_lcb, widget=%p, window=%p\n", widget, window);
	gtk_main_quit();
	window_destroy(window);
}

static void fs_history_pulldown_activate_lcb(GtkWidget *menuitem,Tfileselect *fileselect) {
	const gchar *filename = gtk_entry_get_text(GTK_ENTRY(GTK_FILE_SELECTION(fileselect->fs)->selection_entry));
	gchar *dirname = ending_slash(gtk_label_get_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(menuitem)))));
	gchar *fullpath = g_strconcat(dirname, filename, NULL);
	g_free(dirname);
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileselect->fs), fullpath);
	g_free(fullpath);
}

static void fs_history_pulldown_changed(GtkOptionMenu *optionmenu,Tfileselect *fileselect) {
	GtkWidget *menuitem, *menu;
	GList *tmplist;
	DEBUG_MSG("fs_history_pulldown_changed\n");
	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optionmenu));
	tmplist = g_list_first(main_v->recent_directories);
	while (tmplist) {
		menuitem = gtk_menu_item_new_with_label((gchar *)tmplist->data);
		g_signal_connect(G_OBJECT(menuitem),"activate",G_CALLBACK(fs_history_pulldown_activate_lcb),fileselect);
		gtk_widget_show(menuitem);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		tmplist = g_list_next(tmplist);
	}
}

static void fileselectwin(gchar *setfile, Tfileselect *fileselect, gchar *title) {

	fileselect->fs = gtk_file_selection_new(title);
	DEBUG_MSG("fileselectwin, started, fileselect->fs=%p\n", fileselect->fs);		
	gtk_file_selection_show_fileop_buttons(GTK_FILE_SELECTION(fileselect->fs));
	/*gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(fileselect->fs));*/
	g_signal_connect(G_OBJECT(fileselect->fs), "destroy", G_CALLBACK(close_modal_window_lcb), fileselect->fs);
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(fileselect->fs)->cancel_button),
					   "clicked", G_CALLBACK(close_modal_window_lcb), fileselect->fs);
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(fileselect->fs)->ok_button), "clicked", G_CALLBACK(fs_ok_clicked_lcb), fileselect);
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(fileselect->fs)->history_pulldown), "changed", G_CALLBACK(fs_history_pulldown_changed), fileselect);

	if (fileselect->multipleselect) {
		gtk_file_selection_set_select_multiple(GTK_FILE_SELECTION(fileselect->fs), TRUE);
	}
	if (setfile) {
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileselect->fs), setfile);
	}
	gtk_window_set_role(GTK_WINDOW(fileselect->fs), "fileselect");

	gtk_widget_show(fileselect->fs);
/*	gtk_grab_add(GTK_WIDGET(fileselect->fs));*/
	gtk_widget_realize(GTK_WIDGET(fileselect->fs));
	if (setfile) {
		/* this only has effect after the widget is shown */
		gtk_editable_select_region(GTK_EDITABLE(GTK_FILE_SELECTION(fileselect->fs)->selection_entry),0,-1);
	}
	
	/* When you closed the the file selector dialog focus always returned to the bluefish main window
	   changed to use gtk_widget_get_parent so focus returns to the dialog that opened the file selector
	*/
	DEBUG_MSG("gtk_widget_fet_parent returns %p\n",gtk_widget_get_parent(fileselect->fs));
	{
		GtkWidget *parent = gtk_widget_get_parent(fileselect->fs);
		if (parent) {
			gtk_window_set_transient_for(GTK_WINDOW(fileselect->fs), GTK_WINDOW(parent));
		}
	}
}
/**
 * return_file_w_title:
 * @setfile: #gchar* the file to intitially set the dialog with
 * @title: #gchar* with the dialog title
 *
 * makes a modal dialog with title that will return a file
 *
 * Return value: #gchar* with the selected filename
 */
gchar *return_file_w_title(gchar * setfile, gchar *title) {
	Tfileselect fileselect={FALSE, 0, NULL, NULL, NULL};

	fileselectwin(setfile, &fileselect, title);
	gtk_main();
	return fileselect.filename_to_return;
}
/**
 * return_file:
 * @setfile: #gchar* the file to intitially set the dialog with
 *
 * makes a modal dialog that will return a file
 *
 * Return value: #gchar* with the selected filename
 */
gchar *return_file(gchar * setfile) {
	return return_file_w_title(setfile, _("Select file"));
}
/**
 * return_files_w_title:
 * @setfile: #gchar* the file to intitially set the dialog with
 * @title: #gchar* with the dialog title
 *
 * makes a modal dialog that will return multiple files
 *
 * Return value: #GList* containing a stringlist with all selected files
 */
GList *return_files_w_title(gchar * setfile, gchar *title) {
	Tfileselect fileselect={FALSE, 1, NULL, NULL, NULL};
	
	fileselectwin(setfile, &fileselect, title);
	gtk_main();
	return fileselect.filenames_to_return;
}
/**
 * return_files:
 * @setfile: #gchar* the file to intitially set the dialog with
 *
 * makes a modal dialog that will return multiple files
 *
 * Return value: #GList* containing a stringlist with all selected files
 */
GList *return_files(gchar * setfile) {
	return return_files_w_title(setfile, _("Select files"));
}
/**
 * return_dir:
 * @setdir: #gchar* the dir to intitially set the dialog with
 * @title: #gchar* with the dialog title
 *
 * makes a modal dialog that will return a directory
 *
 * Return value: #gchar* with the selected directory
 */
gchar *return_dir(gchar *setdir, gchar *title) {
	Tfileselect fileselect={TRUE, 0, NULL, NULL, NULL};

	fileselectwin(setdir, &fileselect, title);
	gtk_main();
	return fileselect.filename_to_return;
}
#endif /* HAVE_ATLEAST_GTK_2_4 */

/************************************************************************/

static void ungroupradoiitems(GtkWidget *menu) {
	GList *tmplist = g_list_first(GTK_MENU_SHELL(menu)->children);
	while (tmplist) {
		GtkWidget *sub;
		DEBUG_MSG("ungroupradiomenuitems, another item\n");
		if (GTK_IS_RADIO_MENU_ITEM(tmplist->data)) {
			DEBUG_MSG("%p is a radiomenu item, ungrouped!\n",tmplist->data);
			gtk_radio_menu_item_set_group(tmplist->data,NULL);
		}
		sub = gtk_menu_item_get_submenu(tmplist->data);
		if (sub) ungroupradoiitems(sub);
		tmplist = g_list_next(tmplist);
	}
}

void destroy_disposable_menu_cb(GtkWidget *widget, GtkWidget *menu) {
	ungroupradoiitems(menu);
	gtk_widget_destroy(GTK_WIDGET(menu));
}


/***********************************************************************
 * workarounds for gtk-2.0  
 * these functions are not needed with gtk-2.2
 */
#ifndef HAVE_ATLEAST_GTK_2_2
static GtkTreePath *gtktreepath_up_multi(GtkTreePath *path, gint num) {
	while (num > 0) {
		gtk_tree_path_up(path);
		num--;
	}
	return path;
}

void gtktreepath_expand_to_root(GtkWidget *tree, const GtkTreePath *this_path) {
	gint num = gtk_tree_path_get_depth((GtkTreePath *)this_path);
	while (num >= 0) {
		GtkTreePath *path = gtk_tree_path_copy(this_path);
		path = gtktreepath_up_multi(path, num);
/*		g_signal_handlers_block_matched(G_OBJECT(tree), G_SIGNAL_MATCH_FUNC,
					0, 0, NULL, row_expanded_lcb, NULL);*/
		gtk_tree_view_expand_row(GTK_TREE_VIEW(tree), path, FALSE);
/*		g_signal_handlers_unblock_matched(G_OBJECT(tree), G_SIGNAL_MATCH_FUNC,
					0, 0, NULL, row_expanded_lcb, NULL);*/
		gtk_tree_path_free(path);
		num--;
	}
}
#endif /* ifndef HAVE_ATLEAST_GTK_2_2 */
