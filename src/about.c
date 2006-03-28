/* $Id$ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * about.c
 *
 * Copyright (C) 2004 Eugene Morenko(More) more@irpin.com
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

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* getopt() */

#include "bluefish.h"
#include "about.h"
#include "gtk_easy.h"

#ifndef FUNC_GREP_RECURSIVE_MAX_DEPTH
#define FUNC_GREP_RECURSIVE_MAX_DEPTH "none"
#endif

static GtkWidget *info;

static void add_page(GtkNotebook * notebook, const gchar * name, const gchar * buf,
					 gboolean hscrolling) {
	GtkWidget *textview, *label, *sw;
	label = gtk_label_new(name);
	sw = textview_buffer_in_scrolwin(&textview, -1, 150, buf, GTK_WRAP_WORD);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textview), FALSE);
	gtk_notebook_append_page(notebook, sw, label);
}

static void about_dialog_close_lcb(GtkObject *object,GtkWidget *win) {
	window_destroy(win);
}

void about_dialog_create(gpointer * data, guint * callback_action, GtkWidget * widget) {
	GtkWidget *vbox, *vbox2, *hbox;
	GtkWidget *notebook;
	GtkWidget *info_ok_button;
	/* GdkColor color; */
	
	gchar *INFO =g_strdup_printf( _(\
"Winefish LaTeX Editor (based on Bluefish)\n\
\n\
* home: http://winefish.berlios.de/\n\
* open source development project\n\
* released under the GPL license\n\
\n\
* version: %s\n\
* maximum length\n    latex command: %d\n    autotext command: %d\n\
* recursive grep: %s\n\
* delimiters:\n    %s\n\
* configured:\n    $%s\n"),
	VERSION,
	COMMAND_MAX_LENGTH, AUTOTEXT_MAX_LENGTH,
	FUNC_GREP_RECURSIVE_MAX_DEPTH,
	DELIMITERS,
	CONFIGURE_OPTIONS);
gchar *AUTHORS = _(\
"developers:\n\
* kyanh <kyanh@viettug.org>\n\
\n\
translators:\n\
* French: Michèle Garoche <michele.garoche@easyconnect.fr>\n\
* Italian: Daniele Medri <daniele@medri.org>\n\
* Vietnamese: kyanh <kyanh@viettug.org>\n\
\n\
THANKS to all who helped making this software available.\n\
");
	info = window_full2(_("About Winefish"), GTK_WIN_POS_CENTER, 6
			,G_CALLBACK(about_dialog_close_lcb),NULL, TRUE, NULL);
	gtk_window_set_resizable(GTK_WINDOW(info), FALSE);
	gtk_widget_set_size_request(GTK_WIDGET(info), 345, -1);
	/*
	color.red = 65535;
	color.blue = 65535;
	color.green = 65535;
	gtk_widget_modify_bg(info, GTK_STATE_NORMAL,&color);
	*/
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(info), vbox2);
	{
		GError *error=NULL;
		GtkWidget *image;
		GdkPixbuf* pixbuf= gdk_pixbuf_new_from_file(WINEFISH_SPLASH_FILENAME,&error);
		if (error) {
			g_print("ERROR while loading splash screen: %s\n", error->message);
			g_error_free(error);
		} else if (pixbuf) {
			image = gtk_image_new_from_pixbuf(pixbuf);
			gtk_box_pack_start(GTK_BOX(vbox2), image, FALSE, FALSE, 0);
			g_object_unref(pixbuf);
		}
	}

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), vbox, TRUE, TRUE, 0);

	/* the notebook */
	notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_BOTTOM);

	/* add pages */
	/* I1 = g_strconcat(INFO, CONFIGURE_OPTIONS, NULL); */
	add_page(GTK_NOTEBOOK(notebook), _("info"), INFO, TRUE);
	/* g_free(I1); */
	g_free(INFO);

	add_page(GTK_NOTEBOOK(notebook), _("authors"), AUTHORS, TRUE);
	
	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

	hbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start( GTK_BOX (vbox), hbox, FALSE, FALSE, 4);
	
	/*{
	info_ok_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	GTK_WIDGET_SET_FLAGS(info_ok_button, GTK_CAN_DEFAULT);
	}*/
	info_ok_button = bf_allbuttons_backend(_("oops..."), FALSE, 0, G_CALLBACK(about_dialog_close_lcb), info);
	gtk_box_pack_start(GTK_BOX(hbox), info_ok_button, FALSE, FALSE, 0);
	GTK_WIDGET_SET_FLAGS(info_ok_button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(info_ok_button);

	g_signal_connect(info_ok_button, "clicked", G_CALLBACK(about_dialog_close_lcb), info);

	gtk_widget_show_all(info);
}
