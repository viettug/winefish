/* $Id: gtk_easy.h 80 2006-01-28 09:59:34Z kyanh $ */
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

#ifndef __GTK_EASY_H_
#define __GTK_EASY_H_

#define FILE_CHOOSER_USE_VFS(dialog) (g_object_set_data(G_OBJECT(dialog),"GnomeFileSelectorEnableVFS",GINT_TO_POINTER(1)))

/* GtkWindowPosition can be 
GTK_WIN_POS_NONE
GTK_WIN_POS_CENTER
GTK_WIN_POS_MOUSE */

typedef enum { none, file, font } Textra_but;
void flush_queue(void);
gint widget_get_string_size(GtkWidget *widget, gchar *string);
void widget_set_visible(GtkWidget *widget, gboolean visible);
/* Single-button dialogs*/
void single_button_dialog_backend(GtkWidget *win,gchar * primary, gchar * secondary, gchar * icon);
void error_dialog(GtkWidget *win,gchar * primary, gchar * secondary);
void warning_dialog(GtkWidget *win,gchar * primary, gchar * secondary);
void info_dialog(GtkWidget *win,gchar * primary, gchar * secondary);
/* Multi-button dialogs */
gint multi_error_dialog(GtkWidget *win,gchar *primary, gchar *secondary, gint defval, gint cancelval, gchar **buttons);
gint multi_warning_dialog(GtkWidget *win,gchar *primary, gchar *secondary, gint defval, gint cancelval, gchar **buttons);
gint multi_query_dialog(GtkWidget *win,gchar *primary, gchar *secondary, gint defval, gint cancelval, gchar **buttons);
/* Progress bar */
void progress_set(gpointer gp, guint value);
gpointer progress_popup(GtkWidget *win,gchar *title, guint maxvalue);
void progress_destroy(gpointer gp);

void setup_toggle_item(GtkItemFactory * ifactory, gchar * path, gint state);
#define setup_toggle_item_from_widget(var1, var2, var3) setup_toggle_item(gtk_item_factory_from_widget(var1), var2, var3)
#define menuitem_set_sensitive(menubar, path, state) gtk_widget_set_sensitive(gtk_item_factory_get_widget(gtk_item_factory_from_widget(menubar), path), state)
void string_apply(gchar ** config_var, GtkWidget * entry);
void integer_apply(gint *config_var, GtkWidget * widget, gboolean is_checkbox);
void bitwise_apply(guint32 *config_var, GtkWidget * widget, gboolean is_checkbox, guint32 BIT);
GtkWidget *combo_with_popdown(const gchar * setstring, GList * which_list, gint editable);
GtkWidget *boxed_combo_with_popdown(const gchar * setstring, GList * which_list, gint editable, GtkWidget *box);
GtkWidget *combo_with_popdown_sized(const gchar * setstring, GList * which_list, gint editable, gint width);
GtkWidget *entry_with_text(const gchar * setstring, gint max_lenght);
GtkWidget *boxed_entry_with_text(const gchar * setstring, gint max_lenght, GtkWidget *box);
GtkWidget *boxed_full_entry(const gchar * labeltext, gchar * setstring,gint max_lenght, GtkWidget * box);
GtkWidget *checkbut_with_value(gchar *labeltext, gint which_config_int);
GtkWidget *boxed_checkbut_with_value(gchar *labeltext, gint which_config_int, GtkWidget * box);
GtkWidget *radiobut_with_value(gchar *labeltext, gint enabled, GtkRadioButton *prevbut);
GtkWidget *boxed_radiobut_with_value(gchar *labeltext, gint enabled, GtkRadioButton *prevbut, GtkWidget *box);
GtkWidget *spinbut_with_value(gchar *value, gfloat lower, gfloat upper, gfloat step_increment, gfloat page_increment);
GtkWidget *optionmenu_with_value(gchar **options, gint curval);
GtkWidget *boxed_optionmenu_with_value(const gchar *labeltext, gint curval, GtkWidget *box, gchar **options);
GtkWidget *window_with_title(const gchar * title, GtkWindowPosition position, gint borderwidth);

#define window_full(title,position,borderwidth,close_func,close_data,delete_on_escape) window_full2(title,position,borderwidth,close_func,close_data,delete_on_escape,NULL)

GtkWidget *window_full2(const gchar * title, GtkWindowPosition position, gint borderwidth, GCallback close_func,
					   gpointer close_data, gboolean delete_on_escape, GtkWidget *transientforparent);

GtkWidget *textview_buffer_in_scrolwin(GtkWidget **textview, gint width, gint height, const gchar *contents, GtkWrapMode wrapmode);
void window_destroy(GtkWidget * windowname);
void window_close_by_widget_cb(GtkWidget * widget, gpointer data);
void window_close_by_data_cb(GtkWidget * widget, gpointer data);
GtkWidget *apply_font_style(GtkWidget * this_widget, gchar * fontstring);

GtkWidget *hbox_with_pix_and_text(const gchar *label, gint bf_pixmaptype, gboolean w_mnemonic);

GtkWidget *bf_allbuttons_backend(const gchar *label, gboolean w_mnemonic, gint bf_pixmaptype
		, GCallback func, gpointer func_data );
#define bf_generic_button_with_image(label,pixmap_type,func,func_data) bf_allbuttons_backend(label,FALSE,pixmap_type,func,func_data)
#define bf_generic_mnemonic_button(label,func,func_data) bf_allbuttons_backend(label,TRUE,-1,func,func_data)

GtkWidget *bf_gtkstock_button(const gchar * stock_id, GCallback func, gpointer func_data);
#define bf_stock_ok_button(func, data) bf_gtkstock_button(GTK_STOCK_OK, func, data)
#define bf_stock_cancel_button(func, data) bf_gtkstock_button(GTK_STOCK_CANCEL, func, data)

GtkWidget *bf_generic_frame_new(const gchar *label, GtkShadowType shadowtype, gint borderwidth);
void bf_mnemonic_label_tad_with_alignment(const gchar *labeltext, GtkWidget *m_widget, gfloat xalign, gfloat yalign, 
						GtkWidget *table, guint left_attach, guint right_attach, guint top_attach, guint bottom_attach);
GtkWidget *bf_label_with_markup(const gchar *labeltext);
void bf_label_tad_with_markup(const gchar *labeltext, gfloat xalign, gfloat yalign,
								GtkWidget *table, guint left_attach, guint right_attach, guint top_attach, guint bottom_attach);						
GtkWidget *file_but_new(GtkWidget * which_entry, gint full_pathname, Tbfwin *bfwin);
#ifdef HAVE_ATLEAST_GTK_2_4
GtkWidget * file_chooser_dialog(Tbfwin *bfwin, gchar *title, GtkFileChooserAction action, 
											gchar *set, gboolean localonly, gboolean multiple, const gchar *filter);
#else
void close_modal_window_lcb(GtkWidget * widget, gpointer window);
gchar *return_file_w_title(gchar * setfile, gchar *title);
gchar *return_file(gchar * setfile);
GList *return_files_w_title(gchar * setfile, gchar *title);
GList *return_files(gchar * setfile);
gchar *return_dir(gchar *setdir, gchar *title);
#endif
void destroy_disposable_menu_cb(GtkWidget *widget, GtkWidget *menu);

#ifndef HAVE_ATLEAST_GTK_2_2
void gtktreepath_expand_to_root(GtkWidget *tree, const GtkTreePath *this_path);
#endif
#endif
