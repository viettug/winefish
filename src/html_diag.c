/* $Id$ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * html_diag.c - general functions to create HTML dialogs
 *
 * Copyright (C) 2000-2002 Olivier Sessink
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

#include <gtk/gtk.h>
#include <string.h>	/* strrchr */
#include <stdlib.h> /* strtod */
/* #define DEBUG */

#include "bluefish.h"
#include "html_diag.h" /* myself */
#include "gtk_easy.h" /* window_full() */
#include "bf_lib.h"
#include "stringlist.h" /* add_to_stringlist */
#include "document.h" /* doc_save_selection */

/* kyanh, removed, 20050225
Trecent_attribs recent_attribs;
*/

/*****************************************/
/********** DIALOG FUNCTIONS *************/
/*****************************************/

void html_diag_destroy_cb(GtkWidget * widget, Thtml_diag *dg) {
	/* kyanh, removed, 20050225,
	dg->tobedestroyed = TRUE; */
	if (dg->mark_ins) {
		gtk_text_buffer_delete_mark(dg->doc->buffer,dg->mark_ins);
	}
	if (dg->mark_sel) {
		gtk_text_buffer_delete_mark(dg->doc->buffer,dg->mark_sel);
	}
	window_destroy(dg->dialog); /* kyanh, from: gtk_easy.h */
	DEBUG_MSG("html_diag_destroy_cb, about to free dg=%p\n",dg);
	g_free(dg);
}

void html_diag_cancel_clicked_cb(GtkWidget *widget, gpointer data) {
	html_diag_destroy_cb(NULL, data);
}

Thtml_diag *html_diag_new(Tbfwin *bfwin, gchar *title) {
	Thtml_diag *dg;
	
	dg = g_malloc(sizeof(Thtml_diag));
	/* kyanh, removed, 20050225,
	dg->tobedestroyed = FALSE;
	*/
	DEBUG_MSG("html_diag_new, dg=%p\n",dg);
	dg->dialog = window_full2(title, GTK_WIN_POS_MOUSE, 12,G_CALLBACK(html_diag_destroy_cb), dg, TRUE/* escape key close the winow */,  bfwin ? bfwin->main_window : NULL);
	gtk_window_set_type_hint(GTK_WINDOW(dg->dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_role(GTK_WINDOW(dg->dialog), "html_dialog");
	dg->vbox = gtk_vbox_new(FALSE, 1);
	gtk_container_add(GTK_CONTAINER(dg->dialog), dg->vbox);
	{
		if (!gtk_text_buffer_get_mark(bfwin->current_document->buffer,"diag_ins")) {
			GtkTextIter iter;
			GtkTextMark* mark = gtk_text_buffer_get_mark(bfwin->current_document->buffer,"insert");
			gtk_text_buffer_get_iter_at_mark(bfwin->current_document->buffer,&iter,mark);
			dg->mark_ins = gtk_text_buffer_create_mark(bfwin->current_document->buffer,"diag_ins",&iter,TRUE);
			mark = gtk_text_buffer_get_mark(bfwin->current_document->buffer,"selection_bound");
			gtk_text_buffer_get_iter_at_mark(bfwin->current_document->buffer,&iter,mark);
			dg->mark_sel = gtk_text_buffer_create_mark(bfwin->current_document->buffer,"diag_sel",&iter,TRUE);
		} else {
			dg->mark_ins = dg->mark_sel = NULL;
		}
	}
/*	
	dg->range.pos = -1;
	dg->range.end = -1;
*/

	if (main_v->props.view_bars & MODE_MAKE_LATEX_TRANSIENT) {
		/* must be set before realizing */
		DEBUG_MSG("html_diag_finish, setting dg->dialog=%p transient!\n", dg->dialog);
		gtk_window_set_transient_for(GTK_WINDOW(dg->dialog), GTK_WINDOW(bfwin->main_window));
	}
	gtk_widget_realize(dg->dialog);
	dg->bfwin = bfwin;
	dg->doc = bfwin->current_document;
	return dg;
}

GtkWidget *html_diag_table_in_vbox(Thtml_diag *dg, gint rows, gint cols) {
	GtkWidget *returnwidget = gtk_table_new(rows, cols, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(returnwidget), 12);
	gtk_table_set_col_spacings(GTK_TABLE(returnwidget), 12);
	gtk_box_pack_start(GTK_BOX(dg->vbox), returnwidget, FALSE, FALSE, 0);
	return returnwidget;
}

void html_diag_finish(Thtml_diag *dg, GtkSignalFunc ok_func) {
	GtkWidget *hbox;

	gtk_box_pack_start(GTK_BOX(dg->vbox), gtk_hseparator_new(), FALSE, FALSE, 12);
	hbox = gtk_hbutton_box_new();
	gtk_hbutton_box_set_layout_default(GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 12);
	
	dg->obut = bf_stock_ok_button(ok_func, dg);
	dg->cbut = bf_stock_cancel_button(G_CALLBACK(html_diag_cancel_clicked_cb), dg);

	gtk_box_pack_start(GTK_BOX(hbox),dg->cbut , FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox),dg->obut , FALSE, FALSE, 0);
	gtk_window_set_default(GTK_WINDOW(dg->dialog), dg->obut);

	gtk_box_pack_start(GTK_BOX(dg->vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show_all(GTK_WIDGET(dg->dialog));
}

/*************************************************/
/********** HTML -> DIALOG FUNCTIONS *************/
/*************************************************/

/* TODO: removed this function */
void fill_dialogvalues(gchar *dialogitems[], gchar *dialogvalues[], gchar **custom, Ttagpopup *data, Thtml_diag *diag) {

	gint count=0;
	
	while (dialogitems[count]) {
		dialogvalues[count] = NULL;
		count++;
	}
}

GList *add_entry_to_stringlist(GList *which_list, GtkWidget *entry) {
	if (entry) {
		gchar *temp = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
		which_list = add_to_stringlist(which_list, temp);
		g_free(temp);
	}
	return which_list;
}

/* END OF FILE */
