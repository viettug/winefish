/* $Id: document.h,v 1.2 2005/07/21 08:59:47 kyanh Exp $ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * document.h - global function for document handling
 *
 * Copyright (C) 1998 Olivier Sessink and Chris Mazuc
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
#ifndef __DOCUMENT_H_
#define __DOCUMENT_H_

enum {
	DOCUMENT_BACKUP_ABORT_SAVE,
	DOCUMENT_BACKUP_ABORT_ABORT,
	DOCUMENT_BACKUP_ABORT_ASK
};
void autoclosing_init(void);
GList *return_allwindows_documentlist(void);
GList *return_filenamestringlist_from_doclist(GList *doclist);
gint documentlist_return_index_from_filename(GList *doclist, gchar *filename);
Tdocument *documentlist_return_document_from_filename(GList *doclist, gchar *filename);
Tdocument *documentlist_return_document_from_index(GList *doclist, gint index);

void doc_update_highlighting(Tbfwin *bfwin,guint callback_action, GtkWidget *widget);
void doc_set_wrap(Tdocument *doc);
gboolean doc_set_filetype(Tdocument *doc, Tfiletype *ft);
Tfiletype *get_filetype_by_name(gchar * name);
Tfiletype *get_filetype_by_filename_and_content(gchar *filename, gchar *buf);
void doc_reset_filetype(Tdocument * doc, gchar * newfilename, gchar *buf);
void doc_set_font(Tdocument *doc, gchar *fontstring);
void doc_set_tabsize(Tdocument *doc, gint tabsize);
void gui_change_tabsize(Tbfwin *bfwin,guint action,GtkWidget *widget);

gboolean doc_is_empty_non_modified_and_nameless(Tdocument *doc);
gboolean test_docs_modified(GList *doclist);
gboolean test_only_empty_doc_left(GList *doclist);

gboolean doc_has_selection(Tdocument *doc);
void doc_set_modified(Tdocument *doc, gint value);
void doc_scroll_to_cursor(Tdocument *doc);
gchar *doc_get_chars(Tdocument *doc, gint start, gint end);
gint doc_get_max_offset(Tdocument *doc);
void doc_select_region(Tdocument *doc, gint start, gint end, gboolean do_scroll);
void doc_select_line(Tdocument *doc, gint line, gboolean do_scroll);
gboolean doc_get_selection(Tdocument *doc, gint *start, gint *end);
gint doc_get_cursor_position(Tdocument *doc);
void doc_set_statusbar_insovr(Tdocument *doc);
void doc_set_statusbar_editmode_encoding(Tdocument *doc);

/* the prototype for these functions is changed!! */
void doc_replace_text_backend(Tdocument *doc, const gchar * newstring, gint start, gint end);
void doc_replace_text(Tdocument *doc, const gchar * newstring, gint start, gint end);

void doc_insert_two_strings(Tdocument *doc, const gchar *before_str, const gchar *after_str);

void doc_bind_signals(Tdocument *doc);
void doc_unbind_signals(Tdocument *doc);
gboolean buffer_to_file(Tbfwin *bfwin, gchar *buffer, gchar *filename);
void doc_destroy(Tdocument * doc, gboolean delay_activation);
void document_unset_filename(Tdocument *doc);
gchar *ask_new_filename(Tbfwin *bfwin,gchar *oldfilename, const gchar *gui_name, gboolean is_move);
gint doc_save(Tdocument * doc, gboolean do_save_as, gboolean do_move, gboolean window_closing);
void document_set_line_numbers(Tdocument *doc, gboolean value);
Tdocument *doc_new(Tbfwin* bfwin, gboolean delay_activate);
void doc_new_with_new_file(Tbfwin *bfwin, gchar * new_filename);
Tdocument *doc_new_with_file(Tbfwin *bfwin, gchar * filename, gboolean delay_activate, gboolean move_to_this_win);
void docs_new_from_files(Tbfwin *bfwin, GList * file_list, gboolean move_to_this_win, gint linenumber);
void doc_reload(Tdocument *doc);
void doc_activate(Tdocument *doc);
void doc_force_activate(Tdocument *doc);
/* callbacks for the menu and toolbars */
void file_open_from_selection(Tbfwin *bfwin);
void file_save_cb(GtkWidget * widget, Tbfwin *bfwin);
void file_save_as_cb(GtkWidget * widget, Tbfwin *bfwin);
void file_move_to_cb(GtkWidget * widget, Tbfwin *bfwin);
/* void file_open_url_cb(GtkWidget * widget, Tbfwin *bfwin); */
void file_open_cb(GtkWidget * widget, Tbfwin *bfwin);
/*
void file_open_advanced_cb(GtkWidget * widget, Tbfwin *bfwin);
void open_advanced_from_filebrowser(Tbfwin *bfwin, gchar *path);
*/
void file_insert_menucb(Tbfwin *bfwin,guint callback_action, GtkWidget *widget);
void file_new_cb(GtkWidget * widget, Tbfwin *bfwin);
void file_close_cb(GtkWidget * widget, Tbfwin *bfwin);
void bfwin_close_all_documents(Tbfwin *bfwin, gboolean window_closing);
void file_close_all_cb(GtkWidget * widget, Tbfwin *bfwin);
void file_save_all_cb(GtkWidget * widget, Tbfwin *bfwin);

void edit_cut_cb(GtkWidget * widget, Tbfwin *bfwin);
void edit_copy_cb(GtkWidget * widget, Tbfwin *bfwin);
void edit_paste_cb(GtkWidget * widget, Tbfwin *bfwin);
void edit_select_all_cb(GtkWidget * widget, Tbfwin *bfwin);

void doc_toggle_highlighting_cb(Tbfwin *bfwin,guint action,GtkWidget *widget);
void doc_toggle_wrap_cb(Tbfwin *bfwin,guint action,GtkWidget *widget);
void doc_toggle_linenumbers_cb(Tbfwin *bfwin,guint action,GtkWidget *widget);
void all_documents_apply_settings(void);

void doc_convert_asciichars_in_selection(Tbfwin *bfwin,guint callback_action,GtkWidget *widget);
void word_count_cb (Tbfwin *bfwin,guint callback_action,GtkWidget *widget);
void doc_indent_selection(Tdocument *doc, gboolean unindent);
void menu_indent_cb(Tbfwin *bfwin,guint callback_action, GtkWidget *widget);
GList * list_relative_document_filenames(Tdocument *curdoc);
void file_floatingview_menu_cb(Tbfwin *bfwin,guint callback_action, GtkWidget *widget);

/* kyanh */
void doc_comment_selection(Tdocument *doc, gboolean uncomment);
void menu_comment_cb(Tbfwin *bfwin,guint callback_action, GtkWidget *widget);
void doc_shift_selection(Tdocument *doc, gboolean uncomment);
void menu_shift_cb(Tbfwin *bfwin,guint callback_action, GtkWidget *widget);
void menu_del_line_cb(Tbfwin *bfwin,guint callback_action, GtkWidget *widget);
void doc_del_line(Tdocument *doc, gboolean vers);

#endif /* __DOCUMENT_H_ */
