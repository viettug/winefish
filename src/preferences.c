/* $Id$ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * preferences.c the preferences code
 *
 * Copyright (C) 2002-2003 Olivier Sessink
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
#include <stdlib.h> /* strtoul() */
#include <string.h> /* strcmp() */

#include "bluefish.h"
#include "stringlist.h" /* duplicate_arraylist*/
#include "bf_lib.h" /* list_switch_order() */
#include "gtk_easy.h"
#include "document.h"
#include "pixmap.h"
#include "highlight.h"
#include "filebrowser.h"
#include "menu.h"
#include "gui.h"

#include "rcfile.h" /* kyanh, TODO: remove this */

enum {
	filebrowser_two_pane_view,
	filebrowser_unknown_icon,
	filebrowser_dir_icon,
	editor_font_string,		/* editor font */
	editor_tab_width,	/* editor tabwidth */
	editor_indent_wspaces,
	tab_font_string,		/* notebook tabs font */
	highlight_num_lines_count, /* number of lines to highlight in continous highlighting */	
	defaulthighlight,		/* highlight documents by default */
	transient_htdialogs,  /* set html dialogs transient ro the main window */
	restore_dimensions,
	left_panel_width,
	left_panel_left,
	main_window_h,			/* main window height */
	main_window_w,			/* main window width */
	max_recent_files,	/* length of Open Recent list */
	max_dir_history,	/* length of directory history */
	backup_file, 			/* wheather to use a backup file */
	backup_filestring,  /* the string to append to the backup file */
	backup_abort_action, /* if the backup fails, continue 'save', 'abort' save, or 'ask' user */
	backup_cleanuponclose, /* remove the backupfile after close ? */	
	allow_multi_instances, /* allow multiple instances of the same file */
	modified_check_type, /* 0=no check, 1=by mtime and size, 2=by mtime, 3=by size, 4,5,...not implemented (md5sum?) */
	num_undo_levels, 	/* number of undo levels per document */
	clear_undo_on_save, 	/* clear all undo information on file save */
	newfile_default_encoding,/* if you open a new file, what encoding will it use */
	bookmarks_default_store,
	bookmarks_filename_mode,
	document_tabposition,
	leftpanel_tabposition,
	default_basedir,
	word_wrap,				/* use wordwrap */
	autoindent,			/* autoindent code */
	/* drop_at_drop_pos, 	*//* drop at drop position instead of cursor position */
	cust_menu, 		/* entries in the custom menu */
#ifdef WITH_SPC
	/* spell checker options */
	cfg_spc_cline      ,  /* spell checker command line */
	cfg_spc_lang       ,  /* language */
	spc_accept_compound ,  /* accept compound words ? */
	spc_use_esc_chars   ,  /* specify aditional characters that
                                     may be part of a word ? */
	spc_esc_chars      ,  /* which ones ? */
	spc_use_pers_dict  ,  /* use a personal dictionary */
	spc_pers_dict      ,  /* which one ? */
	spc_use_input_encoding ,  /* use input encoding */
	spc_input_encoding     ,  /* wich one ? */
	spc_output_html_chars  , /* TODO: removed; output html chars ? (like &aacute,)*/
#endif
	/* key conversion */
	conv_ctrl_enter,		/* convert control-enter key press */
	ctrl_enter_text,		/* inserted text */
	conv_shift_enter,		/* convert shift-enter key press */
	shift_enter_text,	/* inserted text */
	conv_special_char,		/* convert ctrl-'<','>','&' */
#ifdef WITH_MSG_QUEUE
	open_in_running_bluefish, /* open commandline documents in already running session*/
#endif /* WITH_MSG_QUEUE */
#ifdef EXTERNAL_GREP
#ifdef EXTERNAL_FIND
	templates_dir, /* directory for templates */
#endif /* EXTERNAL_FIND */
#endif /* EXTERNAL_GREP */
	marker_i, /* column marker, the first one */
	marker_ii,
	marker_iii,
	property_num_max
};

/* listts */
enum {
	browsers,
	external_commands,
	filetypes,
	filefilters,
	highlight_patterns,
	outputbox,
	autotext, /* kyanh, added,d 20050315 */
	completion,
	lists_num_max
};

typedef struct {
	GtkListStore *lstore;
	GtkWidget *lview;
	int insertloc;
	GList **thelist;
} Tlistpref;

typedef struct {
	GtkListStore *lstore;
	GtkWidget *lview;
	GtkWidget *entry[6];/* number of entries */
	GtkWidget *popmenu;
	GtkWidget *check;
	GtkWidget *radio[9]; /* number of radio box */
	gchar **curstrarr;
	const gchar *selected_filetype;
} Thighlightpatterndialog;

typedef struct {
	Tlistpref atd;
	gboolean refreshed;
} Tautox;

typedef struct {
	GtkWidget *prefs[property_num_max];
	GList *lists[lists_num_max];
	GtkWidget *win;
	GtkWidget *noteb;
	Tlistpref ftd;
	Tlistpref ffd;
	Thighlightpatterndialog hpd;
	Tlistpref bd; /* browsers */
	Tlistpref ed; /* external */
	Tlistpref od; /* output */
	Tautox att; /* autotext */
	Tautox atc; /* completion */
} Tprefdialog;

typedef enum {
	string_none,
	string_file,
	string_font,
	string_color
} Tprefstringtype;

/* bluefish#20060120 */
void pref_click_column  (GtkTreeViewColumn *treeviewcolumn, gpointer user_data) {
	GtkToggleButton *but = GTK_TOGGLE_BUTTON(user_data);
	GList *lst = gtk_tree_view_column_get_cell_renderers(treeviewcolumn);
	if ( gtk_toggle_button_get_active(but))
	{
		gtk_toggle_button_set_active(but,FALSE);
		gtk_button_set_label(GTK_BUTTON(but),"");
		gtk_tree_view_column_set_sizing(treeviewcolumn,GTK_TREE_VIEW_COLUMN_FIXED);
		gtk_tree_view_column_set_fixed_width(treeviewcolumn,30);
		if (lst) {
			g_object_set(G_OBJECT(lst->data),"sensitive",FALSE,"mode",GTK_CELL_RENDERER_MODE_INERT,NULL);
			g_list_free(lst);
		}	
	}	
	else
	{
		gtk_toggle_button_set_active(but,TRUE);
		gtk_button_set_label(GTK_BUTTON(but),(gchar*)g_object_get_data(G_OBJECT(but),"_title_"));
		gtk_tree_view_column_set_sizing(treeviewcolumn,GTK_TREE_VIEW_COLUMN_AUTOSIZE);
		if (lst) {
			g_object_set(G_OBJECT(lst->data),"sensitive",TRUE,"mode",GTK_CELL_RENDERER_MODE_EDITABLE,NULL);
			g_list_free(lst);
		}			
	}
}

/* type 0/1=text, 2=toggle */
/* sortable = TRUE: for autotext, completion */
/* or: use get_column to set 'sortable' manually */
/*
static void pref_create_column(GtkTreeView *treeview, gint type, GCallback func, gpointer data, const gchar *title, gint num, gboolean sortable) {
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	if (type == 1 || type == 0) {
		renderer = gtk_cell_renderer_text_new();
		g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
		g_signal_connect(G_OBJECT(renderer), "edited", func, data);
	} else {
		renderer = gtk_cell_renderer_toggle_new();
		g_object_set(G_OBJECT(renderer), "activatable", TRUE, NULL);
		g_signal_connect(G_OBJECT(renderer), "toggled", func, data);
	}
	column = gtk_tree_view_column_new_with_attributes(title, renderer,(type ==1) ? "text" : "active" ,num,NULL);
	if (sortable) {
		gtk_tree_view_column_set_sort_column_id(column, num);
	}
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
}
*/
/* type 0/1=text, 2=toggle,3=radio, 4=non-editable combo */
static void pref_create_column(GtkTreeView *treeview, gint type, GCallback func, gpointer data, const gchar *title, gint num, gboolean sortable) {
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *but;
	if (type == 1 || type == 0) {
		renderer = gtk_cell_renderer_text_new();
		if (func) {
			g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
			g_signal_connect(G_OBJECT(renderer), "edited", func, data);
		}
	} else if (type == 2 || type == 3) {
		renderer = gtk_cell_renderer_toggle_new();
		if (type == 3) {
			gtk_cell_renderer_toggle_set_radio(GTK_CELL_RENDERER_TOGGLE(renderer),TRUE);
		}
		if (func) {
			g_object_set(G_OBJECT(renderer), "activatable", TRUE, NULL);
			g_signal_connect(G_OBJECT(renderer), "toggled", func, data);
		}
	} else /* if (type ==4) */ {
		renderer = gtk_cell_renderer_combo_new();
		g_object_set(G_OBJECT(renderer), "has-entry", FALSE, NULL);
		/* should be done by the calling function: g_object_set(G_OBJECT(renderer), "model", model, NULL);*/
		g_object_set(G_OBJECT(renderer), "text-column", 0, NULL);
		if (func) {
			g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
			g_signal_connect(G_OBJECT(renderer), "edited", func, data);
		}
	}
	column = gtk_tree_view_column_new_with_attributes(title, renderer,(type ==1) ? "text" : "active" ,num,NULL);

	if (sortable) {
		gtk_tree_view_column_set_sort_column_id(column, num);
	}else{
		gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column),TRUE);
		but = gtk_check_button_new_with_label(title);
		g_object_set_data(G_OBJECT(but),"_title_",g_strdup(title));
		gtk_widget_show(but);
		gtk_tree_view_column_set_widget(GTK_TREE_VIEW_COLUMN(column),but);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(but),TRUE);
		g_signal_connect(G_OBJECT(column), "clicked", G_CALLBACK(pref_click_column), but);
	}
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
}

/* 3 entries must have len 3, but in reality it will be 4, because there is a NULL pointer appended */
static gchar **pref_create_empty_strarr(gint len) {
	gchar **strarr = g_malloc0((len+1)*sizeof(gchar *));
	gint i;
	strarr[0] = g_strdup(_("Untitled"));
	for (i=1;i<len;i++) {
		strarr[i] = g_strdup("");
	}
	strarr[len] = NULL;
	return strarr;
}
/* type 0=escapedtext, 1=text, 2=toggle */
static void pref_apply_change(GtkListStore *lstore, gint pointerindex, gint type, gchar *path, gchar *newval, gint index) {
	gchar **strarr;
	GtkTreeIter iter;
	GtkTreePath* tpath = gtk_tree_path_new_from_string(path);
	if (tpath && gtk_tree_model_get_iter(GTK_TREE_MODEL(lstore),&iter,tpath)) {
		gtk_tree_model_get(GTK_TREE_MODEL(lstore), &iter, pointerindex, &strarr, -1);
		DEBUG_MSG("pref_apply_change, lstore=%p, index=%d, type=%d, got strarr=%p\n",lstore,index,type,strarr);
		if (type ==1) {
			gtk_list_store_set(GTK_LIST_STORE(lstore),&iter,index,newval,-1);
		} else {
			gtk_list_store_set(GTK_LIST_STORE(lstore),&iter,index,(newval[0] == '1'),-1);
		}
		if (strarr[index]) {
			DEBUG_MSG("pref_apply_change, old value for strarr[%d] was %s\n",index,strarr[index]);
			g_free(strarr[index]);
		}
		if (type == 0) {
			strarr[index] = unescape_string(newval, FALSE);
		} else {
			strarr[index] = g_strdup(newval);
		}
		DEBUG_MSG("pref_apply_change, strarr[%d] now is %s\n",index,strarr[index]);
	} else {
		DEBUG_MSG("ERROR: path %s was not converted to tpath(%p) or iter (lstore=%p)\n",path,tpath,lstore);
	}
	gtk_tree_path_free(tpath);
}
static void pref_delete_strarr(Tprefdialog *pd, Tlistpref *lp, gint pointercolumn) {
	GtkTreeIter iter;
	GtkTreeSelection *select;
	lp->insertloc = -1;
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(lp->lview));
	if (gtk_tree_selection_get_selected (select,NULL,&iter)) {
		gchar **strarr;
		gtk_tree_model_get(GTK_TREE_MODEL(lp->lstore), &iter, pointercolumn, &strarr, -1);
		gtk_list_store_remove(GTK_LIST_STORE(lp->lstore),&iter);
		*lp->thelist = g_list_remove(*lp->thelist, strarr);
		g_strfreev(strarr);
	}
}

static void listpref_row_inserted(GtkTreeModel *treemodel,GtkTreePath *arg1,GtkTreeIter *arg2,Tlistpref *lp) {
	gint *indices = gtk_tree_path_get_indices(arg1);
	if (indices) {
		lp->insertloc = indices[0];
		DEBUG_MSG("reorderable_row_inserted, insertloc=%d\n",lp->insertloc);
	}
}
static void listpref_row_deleted(GtkTreeModel *treemodel,GtkTreePath *arg1,Tlistpref *lp) {
	if (lp->insertloc > -1) {
		gint *indices = gtk_tree_path_get_indices(arg1);
		if (indices) {
			GList *lprepend, *ldelete;
			gint deleteloc = indices[0];
			if (deleteloc > lp->insertloc) deleteloc--;
			DEBUG_MSG("reorderable_row_deleted, deleteloc=%d, insertloc=%d, listlen=%d\n",deleteloc,lp->insertloc,g_list_length(*lp->thelist));
			*lp->thelist = g_list_first(*lp->thelist);
			lprepend = g_list_nth(*lp->thelist,lp->insertloc);
			ldelete = g_list_nth(*lp->thelist,deleteloc);
			if (ldelete && (ldelete != lprepend)) {
				gpointer data = ldelete->data;
				*lp->thelist = g_list_remove(*lp->thelist, data);
				if (lprepend == NULL) {
					DEBUG_MSG("lprepend=NULL, appending %s to the list\n",((gchar **)data)[0]);
					*lp->thelist = g_list_append(g_list_last(*lp->thelist), data);
				} else {
					DEBUG_MSG("lprepend %s, ldelete %s\n",((gchar **)lprepend->data)[0], ((gchar **)data)[0]);
					*lp->thelist = g_list_prepend(lprepend, data);
				}
				*lp->thelist = g_list_first(*lp->thelist);
			} else {
				DEBUG_MSG("listpref_row_deleted, ERROR: ldelete %p, lprepend %p\n", ldelete, lprepend);
			}
		}
		lp->insertloc = -1;
	}
}

static void font_dialog_response_lcb(GtkDialog *fsd,gint response,GtkWidget *entry) {
	DEBUG_MSG("font_dialog_response_lcb, response=%d\n", response);
	if (response == GTK_RESPONSE_OK) {
		gchar *fontname;
		fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fsd));
		gtk_entry_set_text(GTK_ENTRY(entry), fontname);
		g_free(fontname);
	}
	gtk_widget_destroy(GTK_WIDGET(fsd));
}

static void font_button_lcb(GtkWidget *wid, GtkWidget *entry) {
	GtkWidget *fsd;
	const gchar *fontname;
	fsd = gtk_font_selection_dialog_new(_("Select font"));
	fontname = gtk_entry_get_text(GTK_ENTRY(entry)); /* do NOT free, this is an internal pointer */
	if (strlen(fontname)) {
		gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(fsd), fontname);
	}
	g_signal_connect(GTK_OBJECT(fsd),"response",G_CALLBACK(font_dialog_response_lcb),entry);
	gtk_window_set_transient_for(GTK_WINDOW(GTK_DIALOG(fsd)), GTK_WINDOW(gtk_widget_get_toplevel(entry)));
	gtk_window_set_modal(GTK_WINDOW(GTK_DIALOG(fsd)), TRUE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(GTK_DIALOG(fsd)), TRUE);
	gtk_widget_show(fsd);
}

static GtkWidget *prefs_string(const gchar *title, const gchar *curval, GtkWidget *box, Tprefdialog *pd, Tprefstringtype prefstringtype) {
	GtkWidget *hbox, *return_widget;

	hbox = gtk_hbox_new(FALSE,3);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(title), FALSE, FALSE, 3);
	return_widget = boxed_entry_with_text(curval, 1023, hbox);
	if (prefstringtype == file) {
		gtk_box_pack_start(GTK_BOX(hbox), file_but_new(return_widget, 1, NULL), FALSE, FALSE, 3);
	} else if (prefstringtype == font) {
		GtkWidget *but = bf_gtkstock_button(GTK_STOCK_SELECT_FONT, G_CALLBACK(font_button_lcb), return_widget);
		gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 3);
	}
	return return_widget;
}

static GtkWidget *prefs_combo(const gchar *title, const gchar *curval, GtkWidget *box, Tprefdialog *pd, GList *poplist, gboolean editable) {
	GtkWidget *return_widget;
	GtkWidget *hbox;

	hbox = gtk_hbox_new(FALSE,3);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(title), FALSE, FALSE, 3);
	return_widget = boxed_combo_with_popdown(curval, poplist, editable, hbox);
	return return_widget;
}

static GtkWidget *prefs_integer(const gchar *title, const gint curval, GtkWidget *box, Tprefdialog *pd, gfloat lower, gfloat upper) {
	GtkWidget *return_widget;
	GtkWidget *hbox;
	GtkObject *adjust;
	gfloat step_increment, page_increment;

	step_increment = (upper - lower)/1000;
	if (step_increment < 1) {
		step_increment = 1;
	}
	page_increment = (upper - lower)/20;
	if (page_increment < 10) {
		page_increment = 10;
	}
	hbox = gtk_hbox_new(FALSE,3);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 3);
	adjust = gtk_adjustment_new((1.0 * curval), lower, upper, step_increment ,page_increment , 0);
	return_widget = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 0.1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(title), FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox), return_widget, TRUE, TRUE, 3);
	return return_widget;
}


/*************************/
/* GENERAL FUNCTIONS     */
/*************************/

static void set_filetype_strarr_in_list(GtkTreeIter *iter, gchar **strarr, Tprefdialog *pd) {
	gint arrcount;
	arrcount = count_array(strarr);
	if (arrcount == 7) {
		gchar *escaped;
		escaped = escape_string(strarr[2],FALSE);
		gtk_list_store_set(GTK_LIST_STORE(pd->ftd.lstore), iter
				,0,strarr[0],1,strarr[1],2,escaped,3,strarr[3]
						,4,(strarr[4][0] != '0'),5,strarr[5],6,strarr[6]
				,7,strarr,-1);
		g_free(escaped);
	}
}
static void filetype_apply_change(Tprefdialog *pd, gint type, gchar *path, gchar *newval, gint index) {
	pref_apply_change(pd->ftd.lstore,7,type,path,newval,index);
}
static void filetype_0_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	filetype_apply_change(pd, 1, path, newtext, 0);
}
static void filetype_1_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	filetype_apply_change(pd, 1, path, newtext, 1);
}
static void filetype_2_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	filetype_apply_change(pd, 1, path, newtext, 2);
}
static void filetype_3_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	filetype_apply_change(pd, 1, path, newtext, 3);
}
static void filetype_4_toggled_lcb(GtkCellRendererToggle *cellrenderertoggle,gchar *path,Tprefdialog *pd) {
	gchar *val = g_strdup(cellrenderertoggle->active ? "0" : "1");
	filetype_apply_change(pd, 2, path, val, 4);
	g_free(val);
}
static void filetype_5_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	filetype_apply_change(pd, 1, path, newtext, 5);
}
static void filetype_6_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	if (strlen(newtext)==1 && newtext[0] >= '0' && newtext[0] <= '2') {
		filetype_apply_change(pd, 1, path, newtext, 6);
	}
}
static void add_new_filetype_lcb(GtkWidget *wid, Tprefdialog *pd) {
	gchar **strarr;
	GtkTreeIter iter;
	strarr = pref_create_empty_strarr(7);
	gtk_list_store_append(GTK_LIST_STORE(pd->ftd.lstore), &iter);
	set_filetype_strarr_in_list(&iter, strarr,pd);
	pd->lists[filetypes] = g_list_append(pd->lists[filetypes], strarr);
	pd->ftd.insertloc = -1;
}
static void delete_filetype_lcb(GtkWidget *wid, Tprefdialog *pd) {
	pref_delete_strarr(pd, &pd->ftd,7);
}

static void create_filetype_gui(Tprefdialog *pd, GtkWidget *vbox1) {
	GtkWidget *hbox, *but, *scrolwin;
	pd->lists[filetypes] = duplicate_arraylist(main_v->props.filetypes);
	pd->ftd.lstore = gtk_list_store_new (8,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_BOOLEAN,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_POINTER);
	pd->ftd.lview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pd->ftd.lstore));
	pref_create_column(GTK_TREE_VIEW(pd->ftd.lview), 1, G_CALLBACK(filetype_0_edited_lcb), pd, _("Label"), 0, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->ftd.lview), 1, G_CALLBACK(filetype_1_edited_lcb), pd, _("Extensions"), 1, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->ftd.lview), 1, G_CALLBACK(filetype_2_edited_lcb), pd, _("Update chars"), 2, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->ftd.lview), 1, G_CALLBACK(filetype_3_edited_lcb), pd, _("Icon"), 3, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->ftd.lview), 2, G_CALLBACK(filetype_4_toggled_lcb), pd, _("Editable"), 4, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->ftd.lview), 1, G_CALLBACK(filetype_5_edited_lcb), pd, _("Content regex"), 5, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->ftd.lview), 1, G_CALLBACK(filetype_6_edited_lcb), pd, _("AutoCompletion"), 6, FALSE);

	scrolwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolwin), pd->ftd.lview);
	gtk_widget_set_size_request(scrolwin, 150, 190);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolwin, TRUE, TRUE, 2);
	
/*	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(pd->ftd.lview));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed",G_CALLBACK(filetype_selection_changed_cb),pd);*/
	{
		GList *tmplist = g_list_first(pd->lists[filetypes]);
		while (tmplist) {
			gint arrcount;
			gchar **strarr = (gchar **)tmplist->data;
			arrcount = count_array(strarr);
			if (arrcount ==7) {
				GtkTreeIter iter;
				gtk_list_store_append(GTK_LIST_STORE(pd->ftd.lstore), &iter);
				set_filetype_strarr_in_list(&iter, strarr,pd);
			}
			tmplist = g_list_next(tmplist);
		}
	}
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(pd->ftd.lview), TRUE);
	pd->ftd.thelist = &pd->lists[filetypes];
	pd->ftd.insertloc = -1;
	g_signal_connect(G_OBJECT(pd->ftd.lstore), "row-inserted", G_CALLBACK(listpref_row_inserted), &pd->ftd);
	g_signal_connect(G_OBJECT(pd->ftd.lstore), "row-deleted", G_CALLBACK(listpref_row_deleted), &pd->ftd);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1),hbox, TRUE, TRUE, 2);
	but = bf_gtkstock_button(GTK_STOCK_ADD, G_CALLBACK(add_new_filetype_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);
	but = bf_gtkstock_button(GTK_STOCK_DELETE, G_CALLBACK(delete_filetype_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);
}

static void set_filefilter_strarr_in_list(GtkTreeIter *iter, gchar **strarr, Tprefdialog *pd) {
	gint arrcount;
	arrcount = count_array(strarr);
	if (arrcount==3) {
		gtk_list_store_set(GTK_LIST_STORE(pd->ffd.lstore), iter
				,0,strarr[0],1,(strarr[1][0] != '0'),2,strarr[2],3,strarr,-1);
	} else {
		DEBUG_MSG("ERROR: set_filefilter_strarr_in_list, arraycount != 3 !!!!!!\n");
	}
}
static void filefilter_apply_change(Tprefdialog *pd, gint type, gchar *path, gchar *newval, gint index) {
	DEBUG_MSG("filefilter_apply_change,lstore=%p,path=%s,newval=%s,index=%d\n",pd->ffd.lstore,path,newval,index);
	pref_apply_change(pd->ffd.lstore,3,type,path,newval,index);
}
static void filefilter_0_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	filefilter_apply_change(pd, 1, path, newtext, 0);
}
static void filefilter_1_toggled_lcb(GtkCellRendererToggle *cellrenderertoggle,gchar *path,Tprefdialog *pd) {
	gchar *val = g_strdup(cellrenderertoggle->active ? "0" : "1");
	filefilter_apply_change(pd, 2, path, val, 1);
	g_free(val);
}
static void filefilter_2_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	filefilter_apply_change(pd, 1, path, newtext, 2);
}
static void add_new_filefilter_lcb(GtkWidget *wid, Tprefdialog *pd) {
	gchar **strarr;
	GtkTreeIter iter;
	strarr = pref_create_empty_strarr(3);
	gtk_list_store_append(GTK_LIST_STORE(pd->ffd.lstore), &iter);
	set_filefilter_strarr_in_list(&iter, strarr,pd);
	pd->lists[filefilters] = g_list_append(pd->lists[filefilters], strarr);
	pd->ffd.insertloc = -1;
}

static void delete_filefilter_lcb(GtkWidget *wid, Tprefdialog *pd) {
	pref_delete_strarr(pd, &pd->ffd, 3);
}
static void create_filefilter_gui(Tprefdialog *pd, GtkWidget *vbox1) {
	GtkWidget *hbox, *but, *scrolwin;
	pd->lists[filefilters] = duplicate_arraylist(main_v->props.filefilters);
	pd->ffd.lstore = gtk_list_store_new (4,G_TYPE_STRING,G_TYPE_BOOLEAN,G_TYPE_STRING,G_TYPE_POINTER);
	pd->ffd.lview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pd->ffd.lstore));
	pref_create_column(GTK_TREE_VIEW(pd->ffd.lview), 1, G_CALLBACK(filefilter_0_edited_lcb), pd, _("Label"), 0, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->ffd.lview), 2, G_CALLBACK(filefilter_1_toggled_lcb), pd, _("Inverse filter"), 1, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->ffd.lview), 1, G_CALLBACK(filefilter_2_edited_lcb), pd, _("Filetypes in filter"), 2, FALSE);
	scrolwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolwin), pd->ffd.lview);
	gtk_widget_set_size_request(scrolwin, 150, 190);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolwin, TRUE, TRUE, 2);
	{
		GList *tmplist = g_list_first(pd->lists[filefilters]);
		while (tmplist) {
			gchar **strarr = (gchar **)tmplist->data;
			if (count_array(strarr)==3) {
				GtkTreeIter iter;
				gtk_list_store_append(GTK_LIST_STORE(pd->ffd.lstore), &iter);
				set_filefilter_strarr_in_list(&iter, strarr,pd);
			}
			tmplist = g_list_next(tmplist);
		}
	}
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(pd->ffd.lview), TRUE);
	pd->ffd.thelist = &pd->lists[filefilters];
	pd->ffd.insertloc = -1;
	g_signal_connect(G_OBJECT(pd->ffd.lstore), "row-inserted", G_CALLBACK(listpref_row_inserted), &pd->ffd);
	g_signal_connect(G_OBJECT(pd->ffd.lstore), "row-deleted", G_CALLBACK(listpref_row_deleted), &pd->ffd);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1),hbox, TRUE, TRUE, 2);
	but = bf_gtkstock_button(GTK_STOCK_ADD, G_CALLBACK(add_new_filefilter_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);
	but = bf_gtkstock_button(GTK_STOCK_DELETE, G_CALLBACK(delete_filefilter_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);	
}

static gchar **highlightpattern_create_strarr(Tprefdialog *pd) {
	gchar **strarr;
	strarr = g_malloc(12*sizeof(gchar *));
	strarr[0] = g_strdup(pd->hpd.selected_filetype);
	strarr[1] = gtk_editable_get_chars(GTK_EDITABLE(pd->hpd.entry[0]),0,-1);
	DEBUG_MSG("highlightpattern_create_strarr for %s-%s at %p\n",strarr[0],strarr[1],strarr);
	if (GTK_TOGGLE_BUTTON(pd->hpd.check)->active){
		strarr[2] = g_strdup("0");
	} else {
		strarr[2] = g_strdup("1");
	}
	strarr[3] = gtk_editable_get_chars(GTK_EDITABLE(pd->hpd.entry[1]),0,-1);
	strarr[4] = gtk_editable_get_chars(GTK_EDITABLE(pd->hpd.entry[2]),0,-1);
	if (GTK_TOGGLE_BUTTON(pd->hpd.radio[0])->active){
		strarr[5] = g_strdup("1");
	} else if (GTK_TOGGLE_BUTTON(pd->hpd.radio[1])->active) {
		strarr[5] = g_strdup("2");
	} else {
		strarr[5] = g_strdup("3");
	}
	strarr[6] = gtk_editable_get_chars(GTK_EDITABLE(pd->hpd.entry[3]),0,-1);
	strarr[7] = gtk_editable_get_chars(GTK_EDITABLE(pd->hpd.entry[4]),0,-1);
	strarr[8] = gtk_editable_get_chars(GTK_EDITABLE(pd->hpd.entry[5]),0,-1);
	if (GTK_TOGGLE_BUTTON(pd->hpd.radio[3])->active){
		strarr[9] = g_strdup("0");
	} else if (GTK_TOGGLE_BUTTON(pd->hpd.radio[4])->active) {
		strarr[9] = g_strdup("1");
	} else {
		strarr[9] = g_strdup("2");
	}
	if (GTK_TOGGLE_BUTTON(pd->hpd.radio[6])->active){
		strarr[10] = g_strdup("0");
	} else if (GTK_TOGGLE_BUTTON(pd->hpd.radio[7])->active) {
		strarr[10] = g_strdup("1");
	} else {
		strarr[10] = g_strdup("2");
	}
	strarr[11] = NULL;
	DEBUG_MSG("highlightpattern_create_strarr, strarr at %p with count %d\n", strarr, count_array(strarr));
	return strarr;
}

static void highlightpattern_apply_changes(Tprefdialog *pd) {
	DEBUG_MSG("highlightpattern_apply_changes, started\n");
	if (pd->hpd.curstrarr) {
		GList *tmplist;
		tmplist = g_list_first(pd->lists[highlight_patterns]);
		while (tmplist) {
			if (tmplist->data == pd->hpd.curstrarr) {
				DEBUG_MSG("highlightpattern_apply_changes, curstrarr==tmplist->data==%p\n", tmplist->data);
				g_strfreev(tmplist->data);
				tmplist->data = highlightpattern_create_strarr(pd);
				pd->hpd.curstrarr = tmplist->data;
				DEBUG_MSG("highlightpattern_apply_changes, new strarr for %s-%s\n",pd->hpd.curstrarr[0],pd->hpd.curstrarr[1]);
				return;
			}
			tmplist = g_list_next(tmplist);
		}
		DEBUG_MSG("highlightpattern_apply_changes, nothing found for curstrarr %p?!?\n", pd->hpd.curstrarr);
	}
	DEBUG_MSG("highlightpattern_apply_changes, no curstrarr, nothing to apply\n");
}

static void highlightpattern_fill_from_selected_filetype(Tprefdialog *pd) {
	DEBUG_MSG("highlightpattern_popmenu_activate, applied changes, about to clear liststore\n");
	gtk_list_store_clear(GTK_LIST_STORE(pd->hpd.lstore));
	if (pd->hpd.selected_filetype) {
		GList *tmplist;
		tmplist = g_list_first(pd->lists[highlight_patterns]);
		DEBUG_MSG("highlightpattern_popmenu_activate, about to fill for filetype %s (tmplist=%p)\n",pd->hpd.selected_filetype,tmplist);
		/* fill list model here */
		while (tmplist) {
			gchar **strarr =(gchar **)tmplist->data;
			if (count_array(strarr) ==11 && strarr[0]) {
				DEBUG_MSG("found entry with filetype %s\n",strarr[0]);
				if (strcmp(strarr[0], pd->hpd.selected_filetype)==0) {
					GtkTreeIter iter;
					DEBUG_MSG("highlightpattern_popmenu_activate, appending pattern %s with filetype %s\n",strarr[1],strarr[0]);
					gtk_list_store_append(GTK_LIST_STORE(pd->hpd.lstore), &iter);
					gtk_list_store_set(GTK_LIST_STORE(pd->hpd.lstore), &iter, 0, strarr[1], -1);
				}
			}
			tmplist = g_list_next(tmplist);
		}
	}
	pd->hpd.curstrarr = NULL;
	gtk_entry_set_text(GTK_ENTRY(pd->hpd.entry[0]), "");
	gtk_entry_set_text(GTK_ENTRY(pd->hpd.entry[1]), "");
	gtk_entry_set_text(GTK_ENTRY(pd->hpd.entry[2]), "");
	gtk_entry_set_text(GTK_ENTRY(pd->hpd.entry[3]), "");
	gtk_entry_set_text(GTK_ENTRY(pd->hpd.entry[4]), "");
	gtk_entry_set_text(GTK_ENTRY(pd->hpd.entry[5]), "");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pd->hpd.check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pd->hpd.radio[0]),TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pd->hpd.radio[3]),TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pd->hpd.radio[6]),TRUE);
}

static void highlightpattern_popmenu_activate(GtkMenuItem *menuitem,Tprefdialog *pd) {
	DEBUG_MSG("highlightpattern_popmenu_activate, pd=%p, menuitem=%p\n", pd, menuitem);
	highlightpattern_apply_changes(pd);
	pd->hpd.curstrarr = NULL;
	if (menuitem) {
		pd->hpd.selected_filetype = gtk_label_get_text(GTK_LABEL(GTK_BIN(menuitem)->child));
	}
	highlightpattern_fill_from_selected_filetype(pd);
}

static void add_new_highlightpattern_lcb(GtkWidget *wid, Tprefdialog *pd) {
	gchar *pattern = gtk_editable_get_chars(GTK_EDITABLE(pd->hpd.entry[0]),0,-1);
	if (pattern && pd->hpd.selected_filetype && strlen(pattern) && strlen(pd->hpd.selected_filetype)) {
		gchar **strarr = highlightpattern_create_strarr(pd);
		DEBUG_MSG("add_new_highlightpattern_lcb, appending strarr %p to list\n", strarr);
		pd->lists[highlight_patterns] = g_list_append(pd->lists[highlight_patterns], strarr);
		pd->hpd.curstrarr = NULL;
		{
			GtkTreeIter iter;
			GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pd->hpd.lview));
			DEBUG_MSG("add_new_highlightpattern_lcb, appending to lview\n");
			gtk_list_store_append(GTK_LIST_STORE(pd->hpd.lstore), &iter);
			gtk_list_store_set(GTK_LIST_STORE(pd->hpd.lstore), &iter, 0, strarr[1], -1);
			gtk_tree_selection_select_iter(selection,&iter);
		}
/*		gtk_entry_set_text(GTK_ENTRY(pd->hpd.entry[0]), "");*/
	} else {
		g_free(pattern);
	}
}

static void highlightpattern_selection_changed_cb(GtkTreeSelection *selection, Tprefdialog *pd) {
	GtkTreeIter iter;
	GtkTreeModel *model;
	DEBUG_MSG("highlightpattern_selection_changed_cb, started\n");
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gchar *pattern;
		GList *tmplist = g_list_first(pd->lists[highlight_patterns]);
/*		GtkWidget *menuitem = gtk_menu_get_active(GTK_MENU( gtk_option_menu_get_menu(GTK_OPTION_MENU(pd->hpd.popmenu)) ));*/
		gtk_tree_model_get(model, &iter, 0, &pattern, -1);
		DEBUG_MSG("highlightpattern_selection_changed_cb, selected=%s\n",pattern);
		highlightpattern_apply_changes(pd);
		pd->hpd.curstrarr = NULL;
		DEBUG_MSG("changed applied, searching for the data of the new selection\n");
		while (tmplist) {
			gchar **strarr =(gchar **)tmplist->data;
#ifdef DEBUG
			if (strarr == NULL){
				DEBUG_MSG("strarr== NULL !!!!!!!!!!!!!!!\n");
			}
#endif
			if (strcmp(strarr[1], pattern)==0 && strcmp(strarr[0], pd->hpd.selected_filetype)==0) {
				DEBUG_MSG("highlightpattern_selection_changed_cb, found strarr=%p\n", strarr);
				DEBUG_MSG("0=%s, 1=%s, 2=%s, 3=%s, 4=%s\n",strarr[0],strarr[1],strarr[2],strarr[3],strarr[4]);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pd->hpd.check), (strarr[2][0] == '0'));
				gtk_entry_set_text(GTK_ENTRY(pd->hpd.entry[0]), strarr[1]);
				gtk_entry_set_text(GTK_ENTRY(pd->hpd.entry[1]), strarr[3]);
				gtk_entry_set_text(GTK_ENTRY(pd->hpd.entry[2]), strarr[4]);
				if (strarr[5][0] == '3') {
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pd->hpd.radio[2]),TRUE);
				} else if (strarr[5][0] == '2') {
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pd->hpd.radio[1]),TRUE);
				} else {
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pd->hpd.radio[0]),TRUE);
				}
				gtk_entry_set_text(GTK_ENTRY(pd->hpd.entry[3]), strarr[6]);
				gtk_entry_set_text(GTK_ENTRY(pd->hpd.entry[4]), strarr[7]);
				gtk_entry_set_text(GTK_ENTRY(pd->hpd.entry[5]), strarr[8]);
				if (strarr[9][0] == '2') {
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pd->hpd.radio[5]),TRUE);
				} else if (strarr[9][0] == '1') {
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pd->hpd.radio[4]),TRUE);
				} else {
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pd->hpd.radio[3]),TRUE);
				}
				DEBUG_MSG("strarr[10]=%s, \n",strarr[10]);
				if (strarr[10][0] == '2') {
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pd->hpd.radio[8]),TRUE);
				} else if (strarr[10][0] == '1') {
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pd->hpd.radio[7]),TRUE);
				} else {
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pd->hpd.radio[6]),TRUE);
				}
				pd->hpd.curstrarr = strarr;
				break;
			}
			tmplist = g_list_next(tmplist);
		}
		g_free(pattern);
	} else {
		DEBUG_MSG("no selection, returning..\n");
	}
}
static void highlightpattern_type_toggled(GtkToggleButton *togglebutton,Tprefdialog *pd){
	DEBUG_MSG("highlightpattern_type_toggled, started\n");
	if (GTK_TOGGLE_BUTTON(pd->hpd.radio[0])->active) {
		gtk_widget_set_sensitive(pd->hpd.entry[2], TRUE);
	} else {
		gtk_widget_set_sensitive(pd->hpd.entry[2], FALSE);
	}
	DEBUG_MSG("highlightpattern_type_toggled, done\n");
}
static void highlightpattern_up_clicked_lcb(GtkWidget *wid, Tprefdialog *pd) {
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *pattern;

	GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pd->hpd.lview));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GList *previous=NULL, *tmplist = g_list_first(pd->lists[highlight_patterns]);
		gtk_tree_model_get(model, &iter, 0, &pattern, -1);
		DEBUG_MSG("highlightpattern_up_clicked_lcb, selected=%s\n",pattern);
		while (tmplist) {
			gchar **strarr =(gchar **)tmplist->data;
			if (strcmp(strarr[0], pd->hpd.selected_filetype)==0) {
				DEBUG_MSG("highlightpattern_up_clicked_lcb, comparing %s+%s for filetype %s\n",strarr[1], pattern,pd->hpd.selected_filetype);
				if (strcmp(strarr[1], pattern)==0) {
					DEBUG_MSG("highlightpattern_up_clicked_lcb, found %s, previous=%p, tmplist=%p\n",strarr[1],previous,tmplist);
					if (previous) {
						DEBUG_MSG("highlightpattern_up_clicked_lcb, switch list order %s <-> %s\n",((gchar **)tmplist->data)[1], ((gchar **)previous->data)[1]);
						list_switch_order(tmplist, previous);
						highlightpattern_popmenu_activate(NULL, pd);
					}
					return;
				}
				previous = tmplist;
			}
			tmplist = g_list_next(tmplist);
		}
	}
}
static void highlightpattern_down_clicked_lcb(GtkWidget *wid, Tprefdialog *pd) {
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *pattern;
	GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pd->hpd.lview));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GList *tmplist = g_list_first(pd->lists[highlight_patterns]);
		gtk_tree_model_get(model, &iter, 0, &pattern, -1);
		while (tmplist) {
			gchar **strarr =(gchar **)tmplist->data;
			if (strcmp(strarr[1], pattern)==0 && strcmp(strarr[0], pd->hpd.selected_filetype)==0) {
				if (tmplist->next) {
					list_switch_order(tmplist, tmplist->next);
					highlightpattern_popmenu_activate(NULL, pd);
					return;
				}
			}
			tmplist = g_list_next(tmplist);
		}
	}
}
static void highlightpattern_delete_clicked_lcb(GtkWidget *wid, Tprefdialog *pd) {
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *pattern;
	GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pd->hpd.lview));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GList *tmplist = g_list_first(pd->lists[highlight_patterns]);
		gtk_tree_model_get(model, &iter, 0, &pattern, -1);
		while (tmplist) {
			gchar **strarr =(gchar **)tmplist->data;
			if (strcmp(strarr[1], pattern)==0 && strcmp(strarr[0], pd->hpd.selected_filetype)==0) {
				pd->hpd.curstrarr = NULL;
				pd->lists[highlight_patterns] = g_list_remove(pd->lists[highlight_patterns], strarr);
				g_strfreev(strarr);
				highlightpattern_popmenu_activate(NULL, pd);
				return;
			}
			tmplist = g_list_next(tmplist);
		}
	}
}

static void highlightpattern_gui_rebuild_filetype_popup(Tprefdialog *pd) {
	GList *tmplist;
	GtkWidget *menu, *menuitem;
	gtk_option_menu_remove_menu(GTK_OPTION_MENU(pd->hpd.popmenu));
	menu = gtk_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(pd->hpd.popmenu), menu);
	gtk_widget_show(menu);
	tmplist = g_list_first(pd->lists[filetypes]);
	while (tmplist) {
		gchar **arr = (gchar **)tmplist->data;
		if (count_array(arr)>=3) {
			menuitem = gtk_menu_item_new_with_label(arr[0]);
			DEBUG_MSG("highlightpattern_gui_rebuild_filetype_popup, menuitem=%p for %s\n", menuitem, arr[0]);
			g_signal_connect(GTK_OBJECT(menuitem), "activate",G_CALLBACK(highlightpattern_popmenu_activate),pd);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
			gtk_widget_show(menuitem);
		}
		tmplist = g_list_next(tmplist);
	}
}

static void highlightpattern_reset_clicked_lcb(GtkWidget *button, Tprefdialog *pd) {
	gchar *defaultfile = return_first_existing_filename(PKGDATADIR"highlighting.default",
									"data/highlighting.default",
									"../data/highlighting.default",NULL);
	if (defaultfile) {
	DEBUG_MSG("hightlighting file: %s\n", defaultfile);
		/* get current selected filetype && create array to compare to*/
		gchar **compare = array_from_arglist(pd->hpd.selected_filetype, NULL);
		DEBUG_MSG("highlightpattern_reset_clicked_lcb, defaultfile=%s\n",defaultfile);
		/* delete filetype from arraylist */
		pd->lists[highlight_patterns] = arraylist_delete_identical(pd->lists[highlight_patterns], compare, 1, TRUE);
		/* load filetype from default file */
		pd->lists[highlight_patterns] = arraylist_append_identical_from_file(pd->lists[highlight_patterns], defaultfile, compare, 1, TRUE);
		g_strfreev(compare);
		/* re-load selected filetype in preferences gui */
		DEBUG_MSG("highlightpattern_reset_clicked_lcb, about to rebuild gui\n");
		highlightpattern_fill_from_selected_filetype(pd);
		g_free (defaultfile);
	}
}

static void create_highlightpattern_gui(Tprefdialog *pd, GtkWidget *vbox1) {
	GtkWidget *hbox, *but, *vbox3;
	pd->lists[highlight_patterns] = duplicate_arraylist(main_v->props.highlight_patterns);
	
	DEBUG_MSG("create_highlightpattern_gui, pd=%p, pd->lists[highlight_patterns]=%p\n", pd, pd->lists[highlight_patterns]);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 3);

	gtk_box_pack_start(GTK_BOX(hbox),gtk_label_new(_("filetype")),FALSE, FALSE, 3);
	pd->hpd.popmenu = gtk_option_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(pd->hpd.popmenu), gtk_menu_new());
	highlightpattern_gui_rebuild_filetype_popup(pd);
	gtk_box_pack_start(GTK_BOX(hbox),pd->hpd.popmenu,TRUE, TRUE, 3);
	but = gtk_button_new_with_label(_("Reset"));
	g_signal_connect(G_OBJECT(but), "clicked", G_CALLBACK(highlightpattern_reset_clicked_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but,FALSE, FALSE, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, TRUE, TRUE, 3);
	pd->hpd.entry[0] = boxed_full_entry(_("Pattern name"), NULL, 500, hbox);

	but = bf_gtkstock_button(GTK_STOCK_ADD, G_CALLBACK(add_new_highlightpattern_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, TRUE, 3);
	but = bf_gtkstock_button(GTK_STOCK_DELETE, G_CALLBACK(highlightpattern_delete_clicked_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 1);
	
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, TRUE, TRUE, 0);

	pd->hpd.lstore = gtk_list_store_new (1, G_TYPE_STRING);
	pd->hpd.lview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pd->hpd.lstore));
	{
		GtkTreeViewColumn *column;
		GtkWidget *scrolwin;
		GtkTreeSelection *select;
	   GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();

		column = gtk_tree_view_column_new_with_attributes (_("Pattern"), renderer,"text", 0,NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW(pd->hpd.lview), column);
		scrolwin = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_container_add(GTK_CONTAINER(scrolwin), pd->hpd.lview);
		gtk_box_pack_start(GTK_BOX(hbox), scrolwin, FALSE, TRUE, 2);
		
		select = gtk_tree_view_get_selection(GTK_TREE_VIEW(pd->hpd.lview));
		g_signal_connect(G_OBJECT(select), "changed",G_CALLBACK(highlightpattern_selection_changed_cb),pd);
	}

	vbox3 = gtk_vbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), vbox3, FALSE, FALSE, 2);
	/* pack up and down buttons here */

	but = bf_gtkstock_button(GTK_STOCK_GO_UP, G_CALLBACK(highlightpattern_up_clicked_lcb), pd);
	gtk_box_pack_start(GTK_BOX(vbox3), but, FALSE, FALSE, 1);
	but = bf_gtkstock_button(GTK_STOCK_GO_DOWN, G_CALLBACK(highlightpattern_down_clicked_lcb), pd);
	gtk_box_pack_start(GTK_BOX(vbox3), but, FALSE, FALSE, 1);
	
	vbox3 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox3, TRUE, TRUE, 2);

	pd->hpd.radio[0] = gtk_radio_button_new_with_label(NULL, _("Start pattern and end pattern"));
	gtk_box_pack_start(GTK_BOX(vbox3),pd->hpd.radio[0], TRUE, TRUE, 0);
	pd->hpd.radio[1] = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(pd->hpd.radio[0]), _("Only start pattern"));
	gtk_box_pack_start(GTK_BOX(vbox3),pd->hpd.radio[1], TRUE, TRUE, 0);
	pd->hpd.radio[2] = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(pd->hpd.radio[0]), _("Subpattern from parent"));
	gtk_box_pack_start(GTK_BOX(vbox3),pd->hpd.radio[2], TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(pd->hpd.radio[0]), "toggled", G_CALLBACK(highlightpattern_type_toggled), pd);
	g_signal_connect(G_OBJECT(pd->hpd.radio[1]), "toggled", G_CALLBACK(highlightpattern_type_toggled), pd);
	g_signal_connect(G_OBJECT(pd->hpd.radio[2]), "toggled", G_CALLBACK(highlightpattern_type_toggled), pd);

	pd->hpd.entry[1] = boxed_full_entry(_("Start pattern"), NULL, 4000, vbox3);
	pd->hpd.entry[2] = boxed_full_entry(_("End pattern"), NULL, 4000, vbox3);
	pd->hpd.check = boxed_checkbut_with_value(_("Case sensitive matching"), FALSE, vbox3);
	pd->hpd.entry[3] = boxed_full_entry(_("Parentmatch"), NULL, 300, vbox3);
	pd->hpd.entry[4] = prefs_string(_("Foreground color"), "", vbox3, pd, string_color);
	pd->hpd.entry[5] = prefs_string(_("Background color"), "", vbox3, pd, string_color);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, TRUE, TRUE, 0);
	
	vbox3 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox3, TRUE, TRUE, 0);
	
	pd->hpd.radio[3] = gtk_radio_button_new_with_label(NULL, _("don't change weight"));
	gtk_box_pack_start(GTK_BOX(vbox3),pd->hpd.radio[3], TRUE, TRUE, 0);
	pd->hpd.radio[4] = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(pd->hpd.radio[3]), _("force non-bold weight"));
	gtk_box_pack_start(GTK_BOX(vbox3),pd->hpd.radio[4], TRUE, TRUE, 0);
	pd->hpd.radio[5] = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(pd->hpd.radio[3]), _("force bold weight"));
	gtk_box_pack_start(GTK_BOX(vbox3),pd->hpd.radio[5], TRUE, TRUE, 0);

	vbox3 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox3, TRUE, TRUE, 0);

	pd->hpd.radio[6] = gtk_radio_button_new_with_label(NULL, _("don't change style"));
	gtk_box_pack_start(GTK_BOX(vbox3),pd->hpd.radio[6], TRUE, TRUE, 0);
	pd->hpd.radio[7] = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(pd->hpd.radio[6]), _("force non-italic style"));
	gtk_box_pack_start(GTK_BOX(vbox3),pd->hpd.radio[7], TRUE, TRUE, 0);
	pd->hpd.radio[8] = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(pd->hpd.radio[6]), _("force italic style"));
	gtk_box_pack_start(GTK_BOX(vbox3),pd->hpd.radio[8], TRUE, TRUE, 0);
}

static void set_browser_strarr_in_list(GtkTreeIter *iter, gchar **strarr, Tprefdialog *pd) {
	gint arrcount = count_array(strarr);
	if (arrcount==2) {
		gtk_list_store_set(GTK_LIST_STORE(pd->bd.lstore), iter
				,0,strarr[0],1,strarr[1],2,strarr,-1);
	} else {
		DEBUG_MSG("ERROR: set_browser_strarr_in_list, arraycount != 2 !!!!!!\n");
	}
}

static void browser_apply_change(Tprefdialog *pd, gint type, gchar *path, gchar *newval, gint index) {
	pref_apply_change(pd->bd.lstore,2,type,path,newval,index);
}
static void browser_0_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	browser_apply_change(pd, 1, path, newtext, 0);
}
static void browser_1_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	browser_apply_change(pd, 1, path, newtext, 1);
}
static void add_new_browser_lcb(GtkWidget *wid, Tprefdialog *pd) {
	gchar **strarr;
	GtkTreeIter iter;
	strarr = pref_create_empty_strarr(2);
	gtk_list_store_append(GTK_LIST_STORE(pd->bd.lstore), &iter);
	set_browser_strarr_in_list(&iter, strarr,pd);
	pd->lists[browsers] = g_list_append(pd->lists[browsers], strarr);
	pd->bd.insertloc = -1;
}
static void delete_browser_lcb(GtkWidget *wid, Tprefdialog *pd) {
	pref_delete_strarr(pd, &pd->bd, 2);
}
static void create_browsers_gui(Tprefdialog *pd, GtkWidget *vbox1) {
	GtkWidget *hbox, *but, *scrolwin;
	pd->lists[browsers] = duplicate_arraylist(main_v->props.browsers);
	pd->bd.lstore = gtk_list_store_new (3,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_POINTER);
	pd->bd.lview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pd->bd.lstore));
	pref_create_column(GTK_TREE_VIEW(pd->bd.lview), 1, G_CALLBACK(browser_0_edited_lcb), pd, _("Label"), 0, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->bd.lview), 1, G_CALLBACK(browser_1_edited_lcb), pd, _("Command"), 1, FALSE);
	scrolwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolwin), pd->bd.lview);
	gtk_widget_set_size_request(scrolwin, 150, 190);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolwin, TRUE, TRUE, 2);
	{
		GList *tmplist = g_list_first(pd->lists[browsers]);
		while (tmplist) {
			gchar **strarr = (gchar **)tmplist->data;
			GtkTreeIter iter;
			gtk_list_store_append(GTK_LIST_STORE(pd->bd.lstore), &iter);
			set_browser_strarr_in_list(&iter, strarr,pd);
			tmplist = g_list_next(tmplist);
		}
	}
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(pd->bd.lview), TRUE);
	pd->bd.thelist = &pd->lists[browsers];
	pd->bd.insertloc = -1;
	g_signal_connect(G_OBJECT(pd->bd.lstore), "row-inserted", G_CALLBACK(listpref_row_inserted), &pd->bd);
	g_signal_connect(G_OBJECT(pd->bd.lstore), "row-deleted", G_CALLBACK(listpref_row_deleted), &pd->bd);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1),hbox, TRUE, TRUE, 2);
	but = bf_gtkstock_button(GTK_STOCK_ADD, G_CALLBACK(add_new_browser_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);
	but = bf_gtkstock_button(GTK_STOCK_DELETE, G_CALLBACK(delete_browser_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);	
}

static void autotext_apply_change(Tprefdialog *pd, gint type, gchar *path, gchar *newval, gint index) {
	pref_apply_change(pd->att.atd.lstore,3/*pointer index*/,type,path,newval,index);
}

static void completion_apply_change(Tprefdialog *pd, gint type, gchar *path, gchar *newval, gint index) {
	pref_apply_change(pd->atc.atd.lstore,1/*pointer index*/,type,path,newval,index);
}

static void autotext_0_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	autotext_apply_change(pd, 1, path, newtext, 0);
#ifdef SCROLL_TO_CELL_DOESNOT_WORK
	GtkTreePath* treepath = gtk_tree_path_new_from_string(path);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(pd->att.atd.lview), treepath, NULL, FALSE, 0, 0);
	gtk_tree_path_free(treepath);
#else
	GtkTreeViewColumn *column;
	GtkTreePath *treepath;
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(pd->att.atd.lview), &treepath, &column);
	if (treepath) {
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(pd->att.atd.lview), treepath, NULL, FALSE, 0, 0);
		/* i tried to jump to next cell (first string), but failed */
	}
	/* we must free treepath, see GtkTreeView::GetCursor */
	gtk_tree_path_free(treepath);
#endif
}

static void completion_0_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	completion_apply_change(pd, 1, path, newtext, 0);
	GtkTreeViewColumn *column;
	GtkTreePath *treepath;
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(pd->atc.atd.lview), &treepath, &column);
	if (treepath) {
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(pd->atc.atd.lview), treepath, NULL, FALSE, 0, 0);
	}
	gtk_tree_path_free(treepath);
}

static void autotext_1_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	autotext_apply_change(pd, 1, path, newtext, 1);
}
static void autotext_2_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	autotext_apply_change(pd, 1, path, newtext, 2);
}

static gboolean autotext_remove_key_value(gchar *key, gchar *value, gpointer *data) {
	/* return TRUE: all key will be deleted */
	return TRUE;
}

static void add_new_autotext_lcb(GtkWidget *wid, Tprefdialog *pd) {
	if (pd->att.refreshed) {
		gchar **strarr;
		GtkTreeIter iter;

		strarr = pref_create_empty_strarr(3);
		gtk_list_store_append(GTK_LIST_STORE(pd->att.atd.lstore), &iter);
		gtk_list_store_set(GTK_LIST_STORE(pd->att.atd.lstore), &iter, 0, strarr[0], 1 , strarr[1], 2, strarr[2], 3, strarr, -1);
		pd->lists[autotext] = g_list_append(pd->lists[autotext], strarr);
		pd->att.atd.insertloc = -1;

		GtkTreeViewColumn *column = gtk_tree_view_get_column(GTK_TREE_VIEW(pd->att.atd.lview),0);
		/* check whether the first column is clicked */
		gboolean first_col_is_sorted = gtk_tree_view_column_get_sort_indicator(column);
		if (!first_col_is_sorted) {
			gtk_tree_view_column_clicked(column);
		}
		GtkTreePath *treepath = gtk_tree_model_get_path(GTK_TREE_MODEL(pd->att.atd.lstore), &iter);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(pd->att.atd.lview), treepath, NULL, FALSE, 0, 0);
		gtk_widget_grab_focus(GTK_WIDGET(pd->att.atd.lview));
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(pd->att.atd.lview), treepath, column, TRUE);

		/* can*NOT*: gtk_tree_path_free(treepath); */
	}else{
		warning_dialog(pd->win, _("Warning:"), _("Please refresh the list before adding any item!\n\nBecause of peformance reason, Winefish doesnot load your autotext file automatically. You may do it manually by choosing 'Refresh' now. *Note* that this may take a quite long time."));
	}
}

static void add_new_completion_lcb(GtkWidget *wid, Tprefdialog *pd) {
	if (pd->atc.refreshed) {
		gchar **strarr;
		GtkTreeIter iter;

		strarr = pref_create_empty_strarr(1);
		gtk_list_store_append(GTK_LIST_STORE(pd->atc.atd.lstore), &iter);
		gtk_list_store_set(GTK_LIST_STORE(pd->atc.atd.lstore), &iter, 0, strarr[0], 1, strarr, -1);
		pd->lists[completion] = g_list_append(pd->lists[completion], strarr);
		pd->atc.atd.insertloc = -1;

		GtkTreeViewColumn *column = gtk_tree_view_get_column(GTK_TREE_VIEW(pd->atc.atd.lview),0);
		/* check whether the first column is clicked */
		/*
		gboolean first_col_is_sorted = gtk_tree_view_column_get_sort_indicator(column);
		if (!first_col_is_sorted) {
			gtk_tree_view_column_clicked(column);
		}
		*/
		GtkTreePath *treepath = gtk_tree_model_get_path(GTK_TREE_MODEL(pd->atc.atd.lstore), &iter);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(pd->atc.atd.lview), treepath, NULL, FALSE, 0, 0);
		gtk_widget_grab_focus(GTK_WIDGET(pd->atc.atd.lview));
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(pd->atc.atd.lview), treepath, column, TRUE);

		/* can*NOT*: gtk_tree_path_free(treepath); */
	}else{
		warning_dialog(pd->win, _("Warning:"), _("Please refresh the list before adding any item!\n\nBecause of peformance reason, Winefish doesnot load your 'word-list' file automatically. You may do it manually by choosing 'Refresh' now. *Note* that this may take a quite long time."));
	}
}

static void delete_autotext_lcb(GtkWidget *wid, Tprefdialog *pd) {
	pref_delete_strarr(pd, &pd->att.atd, 3);
}

static void delete_completion_lcb(GtkWidget *wid, Tprefdialog *pd) {
	pref_delete_strarr(pd, &pd->atc.atd, 1);
}

static void autotext_insert_def_key_value(gchar *key, gchar *value, Tprefdialog *pd) {
	GtkTreeIter iter;
	gint index = strtoul(value, NULL, 10);
	gchar **tmp;
	tmp = g_ptr_array_index(main_v->props.autotext_array, index);
	{
		gchar **strarr = g_malloc(4*sizeof(gchar *));
		strarr[0] = g_strdup(key);
		strarr[1] = escape_string(tmp[0], TRUE);
		strarr[2] = escape_string(tmp[1], TRUE);
		strarr[3] = NULL;

		pd->lists[autotext] = g_list_append(pd->lists[autotext], strarr);
		gtk_list_store_append(GTK_LIST_STORE(pd->att.atd.lstore), &iter);
		gtk_list_store_set(GTK_LIST_STORE(pd->att.atd.lstore), &iter, 0, strarr[0], 1 , strarr[1], 2, strarr[2], 3, strarr, -1);
	}
}

static void completion_insert_global_words(Tprefdialog *pd) {
	GList *tmplist = g_list_first(main_v->props.completion->items);
	GtkTreeIter iter;
	while (tmplist) {
		gchar **strarr = g_malloc(2*sizeof(gchar *));
		strarr[0] = escape_string((gchar*)tmplist->data, TRUE);
		strarr[1] = NULL;
		pd->lists[completion] = g_list_append(pd->lists[completion], strarr);
		gtk_list_store_append(GTK_LIST_STORE(pd->atc.atd.lstore), &iter);
		gtk_list_store_set(GTK_LIST_STORE(pd->atc.atd.lstore), &iter, 0, strarr[0], 1, strarr, -1);
		tmplist = g_list_next(tmplist);
	}
}

static void refresh_autotext_lcb(GtkWidget *wid, Tprefdialog *pd) {
	pd->att.refreshed = TRUE;
	gtk_widget_set_sensitive(wid, FALSE);
	g_hash_table_foreach(main_v->props.autotext_hashtable,(GHFunc)autotext_insert_def_key_value, pd);
	GtkTreeViewColumn *column = gtk_tree_view_get_column(GTK_TREE_VIEW(pd->att.atd.lview),0);
	gtk_tree_view_column_clicked(column);
}

static void refresh_completion_lcb(GtkWidget *wid, Tprefdialog *pd) {
	pd->atc.refreshed = TRUE;
	gtk_widget_set_sensitive(wid, FALSE);
	completion_insert_global_words(pd);
	GtkTreeViewColumn *column = gtk_tree_view_get_column(GTK_TREE_VIEW(pd->atc.atd.lview),0);
	gtk_tree_view_column_clicked(column);
}


/* kyanh <kyanh@o2.pl> */
static void create_autotext_gui(Tprefdialog *pd, GtkWidget *vbox1) {
	pd->att.refreshed = FALSE;

	GtkWidget *hbox, *but, *scrolwin;
	pd->att.atd.lstore = gtk_list_store_new (4,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_POINTER);
	pd->att.atd.lview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pd->att.atd.lstore));

	pref_create_column(GTK_TREE_VIEW(pd->att.atd.lview), 1, G_CALLBACK(autotext_0_edited_lcb), pd, _("Definition"), 0, TRUE);
	pref_create_column(GTK_TREE_VIEW(pd->att.atd.lview), 1, G_CALLBACK(autotext_1_edited_lcb), pd, _("Before"), 1, TRUE);
	pref_create_column(GTK_TREE_VIEW(pd->att.atd.lview), 1, G_CALLBACK(autotext_2_edited_lcb), pd, _("After"), 2, TRUE);

	/* setting for new tree view */
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(pd->att.atd.lview), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(pd->att.atd.lview), 0);

	scrolwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolwin), pd->att.atd.lview);
	gtk_widget_set_size_request(scrolwin, 150, 190);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolwin, TRUE, TRUE, 2);
	
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(pd->att.atd.lview), TRUE);
	pd->att.atd.thelist = &pd->lists[autotext];
	pd->att.atd.insertloc = -1;

	g_signal_connect(G_OBJECT(pd->att.atd.lstore), "row-inserted", G_CALLBACK(listpref_row_inserted), &pd->att.atd);
	g_signal_connect(G_OBJECT(pd->att.atd.lstore), "row-deleted", G_CALLBACK(listpref_row_deleted), &pd->att.atd);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1),hbox, TRUE, TRUE, 2);

	if (main_v->props.autotext_array->len >= 100) {
	/* too much definition make make winefish slow down */
		but = bf_gtkstock_button(GTK_STOCK_REFRESH, G_CALLBACK(refresh_autotext_lcb), pd);
		gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);
	}else{
		pd->att.refreshed = TRUE;
		g_hash_table_foreach(main_v->props.autotext_hashtable,(GHFunc)autotext_insert_def_key_value, pd);
		GtkTreeViewColumn *column = gtk_tree_view_get_column(GTK_TREE_VIEW(pd->att.atd.lview),0);
		gtk_tree_view_column_clicked(column);
	}

	but = bf_gtkstock_button(GTK_STOCK_ADD, G_CALLBACK(add_new_autotext_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);
	{
		/*GtkTooltips *add_tips;
		add_tips = gtk_tooltips_new();*/
		gtk_tooltips_set_tip(main_v->tooltips, but, _("Add new autotext definition.\nStart finding the old one by pressing CTRL+F."), NULL);
	}
	but = bf_gtkstock_button(GTK_STOCK_DELETE, G_CALLBACK(delete_autotext_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);
}

static void create_completion_gui(Tprefdialog *pd, GtkWidget *vbox1) {
	pd->atc.refreshed = FALSE;

	GtkWidget *hbox, *but, *scrolwin;
	pd->atc.atd.lstore = gtk_list_store_new (2,G_TYPE_STRING,G_TYPE_POINTER);
	pd->atc.atd.lview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pd->atc.atd.lstore));

	pref_create_column(GTK_TREE_VIEW(pd->atc.atd.lview), 1, G_CALLBACK(completion_0_edited_lcb), pd, _("Command"), 0, TRUE);

	/* setting for new tree view */
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(pd->atc.atd.lview), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(pd->atc.atd.lview), 0);

	scrolwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolwin), pd->atc.atd.lview);
	gtk_widget_set_size_request(scrolwin, 150, 190);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolwin, TRUE, TRUE, 2);
	
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(pd->atc.atd.lview), TRUE);
	pd->atc.atd.thelist = &pd->lists[completion];
	pd->atc.atd.insertloc = -1;

	g_signal_connect(G_OBJECT(pd->atc.atd.lstore), "row-inserted", G_CALLBACK(listpref_row_inserted), &pd->atc.atd);
	g_signal_connect(G_OBJECT(pd->atc.atd.lstore), "row-deleted", G_CALLBACK(listpref_row_deleted), &pd->atc.atd);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1),hbox, TRUE, TRUE, 2);

	if (g_list_length(main_v->props.completion->items) >= 100) {
	/* too much definition make make winefish slow down */
		but = bf_gtkstock_button(GTK_STOCK_REFRESH, G_CALLBACK(refresh_completion_lcb), pd);
		gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);
	}else{
		pd->atc.refreshed = TRUE;
		/* add */
		completion_insert_global_words(pd);
		GtkTreeViewColumn *column = gtk_tree_view_get_column(GTK_TREE_VIEW(pd->atc.atd.lview),0);
		gtk_tree_view_column_clicked(column);
	}

	but = bf_gtkstock_button(GTK_STOCK_ADD, G_CALLBACK(add_new_completion_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);
	{
		/* GtkTooltips *add_tips;
		add_tips = gtk_tooltips_new(); */
		gtk_tooltips_set_tip(main_v->tooltips, but, _("Add new word which should be started by '\\'.\nStart finding the old one by pressing CTRL+F."), NULL);
	}
	but = bf_gtkstock_button(GTK_STOCK_DELETE, G_CALLBACK(delete_completion_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);
}

static void set_external_commands_strarr_in_list(GtkTreeIter *iter, gchar **strarr, Tprefdialog *pd) {
	gint arrcount = count_array(strarr);
	if (arrcount==2) {
		gtk_list_store_set(GTK_LIST_STORE(pd->ed.lstore), iter
				,0,strarr[0],1,strarr[1],2,strarr,-1);
	} else {
		DEBUG_MSG("ERROR: set_external_command_strarr_in_list, arraycount != 2 !!!!!!\n");
	}
}
static void external_commands_apply_change(Tprefdialog *pd, gint type, gchar *path, gchar *newval, gint index) {
	pref_apply_change(pd->ed.lstore,2,type,path,newval,index);
}
static void external_commands_0_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	external_commands_apply_change(pd, 1, path, newtext, 0);
}
static void external_commands_1_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	external_commands_apply_change(pd, 1, path, newtext, 1);
}
static void add_new_external_commands_lcb(GtkWidget *wid, Tprefdialog *pd) {
	gchar **strarr;
	GtkTreeIter iter;
	strarr = pref_create_empty_strarr(2);
	gtk_list_store_append(GTK_LIST_STORE(pd->ed.lstore), &iter);
	set_external_commands_strarr_in_list(&iter, strarr,pd);
	pd->lists[external_commands] = g_list_append(pd->lists[external_commands], strarr);
	pd->ed.insertloc = -1;
}
static void delete_external_commands_lcb(GtkWidget *wid, Tprefdialog *pd) {
	pref_delete_strarr(pd, &pd->ed, 2);
}
static void create_externals_gui(Tprefdialog *pd, GtkWidget *vbox1) {
	GtkWidget *hbox, *but, *scrolwin;
	pd->lists[external_commands] = duplicate_arraylist(main_v->props.external_commands);
	pd->ed.lstore = gtk_list_store_new (3,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_POINTER);
	pd->ed.lview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pd->ed.lstore));
	pref_create_column(GTK_TREE_VIEW(pd->ed.lview), 1, G_CALLBACK(external_commands_0_edited_lcb), pd, _("Label"), 0, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->ed.lview), 1, G_CALLBACK(external_commands_1_edited_lcb), pd, _("Command"), 1, FALSE);
	scrolwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolwin), pd->ed.lview);
	gtk_widget_set_size_request(scrolwin, 120, 190);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolwin, TRUE, TRUE, 2);
	{
		GList *tmplist = g_list_first(pd->lists[external_commands]);
		while (tmplist) {
			gchar **strarr = (gchar **)tmplist->data;
			GtkTreeIter iter;
			gtk_list_store_append(GTK_LIST_STORE(pd->ed.lstore), &iter);
			set_external_commands_strarr_in_list(&iter, strarr,pd);
			tmplist = g_list_next(tmplist);
		}
	}
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(pd->ed.lview), TRUE);
	pd->ed.thelist = &pd->lists[external_commands];
	pd->ed.insertloc = -1;
	g_signal_connect(G_OBJECT(pd->ed.lstore), "row-inserted", G_CALLBACK(listpref_row_inserted), &pd->ed);
	g_signal_connect(G_OBJECT(pd->ed.lstore), "row-deleted", G_CALLBACK(listpref_row_deleted), &pd->ed);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1),hbox, TRUE, TRUE, 2);
	but = bf_gtkstock_button(GTK_STOCK_ADD, G_CALLBACK(add_new_external_commands_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);
	but = bf_gtkstock_button(GTK_STOCK_DELETE, G_CALLBACK(delete_external_commands_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);	
}

static void set_outputbox_strarr_in_list(GtkTreeIter *iter, gchar **strarr, Tprefdialog *pd) {
	gint arrcount;
	arrcount = count_array(strarr);
	if (arrcount==7) {
		gtk_list_store_set(GTK_LIST_STORE(pd->od.lstore), iter
				,0,strarr[0],1,strarr[1],2,strarr[2],3,strarr[3]
						,4,strarr[4],5,strarr[5],6,strarr[6]/*(strarr[6][0] != '0')*/
				,7,strarr,-1);
	}
}
static void outputbox_apply_change(Tprefdialog *pd, gint type, gchar *path, gchar *newval, gint index) {
	pref_apply_change(pd->od.lstore,7,type,path,newval,index);
}
static void outputbox_0_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	outputbox_apply_change(pd, 1, path, newtext, 0);
}
static void outputbox_1_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	outputbox_apply_change(pd, 1, path, newtext, 1);
}
static void outputbox_2_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	outputbox_apply_change(pd, 1, path, newtext, 2);
}
static void outputbox_3_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	outputbox_apply_change(pd, 1, path, newtext, 3);
}
static void outputbox_4_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	outputbox_apply_change(pd, 1, path, newtext, 4);
}
static void outputbox_5_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	outputbox_apply_change(pd, 1, path, newtext, 5);
}
/*
static void outputbox_6_toggled_lcb(GtkCellRendererToggle *cellrenderertoggle,gchar *path,Tprefdialog *pd) {
	gchar *val = g_strdup(cellrenderertoggle->active ? "0" : "1");
	outputbox_apply_change(pd, 2, path, val, 6);
	g_free(val);
}
*/
static void outputbox_6_edited_lcb(GtkCellRendererText *cellrenderertext,gchar *path,gchar *newtext,Tprefdialog *pd) {
	outputbox_apply_change(pd, 1, path, newtext, 6);
}
static void add_new_outputbox_lcb(GtkWidget *wid, Tprefdialog *pd) {
	gchar **strarr;
	GtkTreeIter iter;
	strarr = pref_create_empty_strarr(7);
	gtk_list_store_append(GTK_LIST_STORE(pd->od.lstore), &iter);
	set_outputbox_strarr_in_list(&iter, strarr,pd);
	pd->lists[outputbox] = g_list_append(pd->lists[outputbox], strarr);
	pd->od.insertloc = -1;
}
static void delete_outputbox_lcb(GtkWidget *wid, Tprefdialog *pd) {
	pref_delete_strarr(pd, &pd->od, 7);
}

static void create_outputbox_gui(Tprefdialog *pd, GtkWidget *vbox1) {
	GtkWidget *hbox, *but, *scrolwin;
	pd->lists[outputbox] = duplicate_arraylist(main_v->props.outputbox);
	pd->od.lstore = gtk_list_store_new (8,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING/*G_TYPE_BOOLEAN*/,G_TYPE_POINTER);
	pd->od.lview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pd->od.lstore));
	pref_create_column(GTK_TREE_VIEW(pd->od.lview), 1, G_CALLBACK(outputbox_0_edited_lcb), pd, _("Name"), 0, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->od.lview), 1, G_CALLBACK(outputbox_1_edited_lcb), pd, _("Pattern"), 1, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->od.lview), 1, G_CALLBACK(outputbox_2_edited_lcb), pd, _("File #"), 2, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->od.lview), 1, G_CALLBACK(outputbox_3_edited_lcb), pd, _("Line #"), 3, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->od.lview), 1, G_CALLBACK(outputbox_4_edited_lcb), pd, _("Output #"), 4, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->od.lview), 1, G_CALLBACK(outputbox_5_edited_lcb), pd, _("Command"), 5, FALSE);
	pref_create_column(GTK_TREE_VIEW(pd->od.lview), 1, G_CALLBACK(outputbox_6_edited_lcb), pd, _("Save,Show"), 6, FALSE);
	scrolwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolwin), pd->od.lview);
	gtk_widget_set_size_request(scrolwin, 150, 190);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolwin, TRUE, TRUE, 2);
	{
		GList *tmplist = g_list_first(pd->lists[outputbox]);
		while (tmplist) {
			gint arrcount;
			gchar **strarr = (gchar **)tmplist->data;
			arrcount = count_array(strarr);
			if (arrcount==7) {
				GtkTreeIter iter;
				gtk_list_store_append(GTK_LIST_STORE(pd->od.lstore), &iter);
				set_outputbox_strarr_in_list(&iter, strarr,pd);
			}
			tmplist = g_list_next(tmplist);
		}
	}
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(pd->od.lview), TRUE);
	pd->od.thelist = &pd->lists[outputbox];
	pd->od.insertloc = -1;
	g_signal_connect(G_OBJECT(pd->od.lstore), "row-inserted", G_CALLBACK(listpref_row_inserted), &pd->od);
	g_signal_connect(G_OBJECT(pd->od.lstore), "row-deleted", G_CALLBACK(listpref_row_deleted), &pd->od);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1),hbox, TRUE, TRUE, 2);
	but = bf_gtkstock_button(GTK_STOCK_ADD, G_CALLBACK(add_new_outputbox_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);
	but = bf_gtkstock_button(GTK_STOCK_DELETE, G_CALLBACK(delete_outputbox_lcb), pd);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 2);

}

/* kyanh, added, 20050221 */
static void create_outputbox_info_gui(Tprefdialog *pd, GtkWidget *vbox1) {
	gtk_box_pack_start(GTK_BOX(vbox1),gtk_label_new(
_(
"%D: basedir of project\n\
%B: basefile (without extension) of project\n\
%d: current directory\n\
%b: basename (without extension) of current file\n\
%f: current file (full path)\n\
%l: current line\n\
%%: percent sign\n\
\n\
If there isn't any project, or project mode is off, we have\n\
\t%D=%d, %B=%b\n\
\n\
Save,Show:\n\
\tneed save file: 1\n\
\tshow all output: 2\n\
\tboth of them: 1+2 =3\n\
\tnone of them: 0\
")), TRUE, TRUE, 2);
}


/**************************************/
/* MAIN DIALOG FUNCTIONS              */
/**************************************/

static void preferences_destroy_lcb(GtkWidget * widget, Tprefdialog *pd) {
	GtkTreeSelection *select;
	DEBUG_MSG("preferences_destroy_lcb, started\n");

	/* FIX BUG#57, clear the list */
	gtk_list_store_clear (GTK_LIST_STORE(pd->att.atd.lstore));
	gtk_list_store_clear (GTK_LIST_STORE(pd->atc.atd.lstore));

	free_arraylist(pd->lists[filetypes]);
	free_arraylist(pd->lists[filefilters]);
	free_arraylist(pd->lists[highlight_patterns]);
	free_arraylist(pd->lists[browsers]);
	free_arraylist(pd->lists[external_commands]);
	free_arraylist(pd->lists[autotext]);
	free_arraylist(pd->lists[completion]);

	pd->lists[filetypes] = NULL;
	pd->lists[filefilters] = NULL;
	pd->lists[highlight_patterns] = NULL;
	pd->lists[browsers] = NULL;
	pd->lists[autotext] = NULL;
	pd->lists[completion] = NULL;	
	pd->lists[external_commands] = NULL;

/*	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(pd->ftd.lview));
	g_signal_handlers_destroy(G_OBJECT(select));*/
	DEBUG_MSG("preferences_destroy_lcb, destroying handlers for lstore %p\n",pd->ftd.lstore);
	g_signal_handlers_destroy(G_OBJECT(pd->ftd.lstore));

	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(pd->ffd.lview));
	g_signal_handlers_destroy(G_OBJECT(select));

	g_signal_handlers_destroy(G_OBJECT(pd->hpd.popmenu));
/*	g_signal_handlers_destroy(G_OBJECT(GTK_COMBO(pd->bd.combo)->list));*/
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(pd->bd.lview));
	g_signal_handlers_destroy(G_OBJECT(select));
/*	g_signal_handlers_destroy(G_OBJECT(GTK_COMBO(pd->ed.combo)->list));*/
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(pd->ed.lview));
	g_signal_handlers_destroy(G_OBJECT(select));

	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(pd->att.atd.lview));
	g_signal_handlers_destroy(G_OBJECT(select));

	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(pd->atc.atd.lview));
	g_signal_handlers_destroy(G_OBJECT(select));
	
	DEBUG_MSG("preferences_destroy_lcb, about to destroy the window\n");
	window_destroy(pd->win);
	g_free(pd);
}

static void preferences_apply(Tprefdialog *pd) {
	string_apply(&main_v->props.editor_font_string, pd->prefs[editor_font_string]);
	integer_apply(&main_v->props.editor_tab_width, pd->prefs[editor_tab_width], FALSE);

	bitwise_apply(&main_v->props.view_bars, pd->prefs[editor_indent_wspaces], TRUE, MODE_INDENT_WITH_SPACES);
	bitwise_apply(&main_v->props.view_bars, pd->prefs[word_wrap], TRUE, MODE_WRAP);
	bitwise_apply(&main_v->props.view_bars, pd->prefs[defaulthighlight], TRUE, VIEW_COLORIZED);
	bitwise_apply(&main_v->props.view_bars, pd->prefs[allow_multi_instances], TRUE, MODE_ALLOW_MULTIPLE_INSTANCE);
	bitwise_apply(&main_v->props.view_bars, pd->prefs[backup_file], TRUE, MODE_CREATE_BACKUP_ON_SAVE);
	bitwise_apply(&main_v->props.view_bars, pd->prefs[backup_cleanuponclose], TRUE, MODE_REMOVE_BACKUP_ON_CLOSE);
	bitwise_apply(&main_v->props.view_bars, pd->prefs[clear_undo_on_save], TRUE, MODE_CLEAR_UNDO_HISTORY_ON_SAVE);
	bitwise_apply(&main_v->props.view_bars, pd->prefs[restore_dimensions], TRUE, MODE_RESTORE_DIMENSION);
	bitwise_apply(&main_v->props.view_bars, pd->prefs[transient_htdialogs], TRUE, MODE_MAKE_LATEX_TRANSIENT);
	bitwise_apply(&main_v->props.view_bars, pd->prefs[filebrowser_two_pane_view], TRUE, MODE_FILE_BROWSERS_TWO_VIEW);

#ifdef WITH_MSG_QUEUE
	bitwise_apply(&main_v->props.view_bars, pd->prefs[open_in_running_bluefish], TRUE, MODE_REUSE_WINDOW);
#endif
/*			
	integer_apply(&main_v->props.editor_indent_wspaces, pd->prefs[editor_indent_wspaces], TRUE);
	integer_apply(&main_v->props.word_wrap, pd->prefs[word_wrap], TRUE);
	integer_apply(&main_v->props.defaulthighlight, pd->prefs[defaulthighlight], TRUE);
	integer_apply(&main_v->props.allow_multi_instances, pd->prefs[allow_multi_instances], TRUE);
#ifdef WITH_MSG_QUEUE
	integer_apply(&main_v->props.open_in_running_bluefish, pd->prefs[open_in_running_bluefish], TRUE);
#endif
*/
	
	integer_apply(&main_v->props.highlight_num_lines_count, pd->prefs[highlight_num_lines_count], FALSE);
	integer_apply(&main_v->props.bookmarks_default_store, pd->prefs[bookmarks_default_store], TRUE);
	main_v->props.bookmarks_filename_mode = gtk_option_menu_get_history(GTK_OPTION_MENU(pd->prefs[bookmarks_filename_mode]));
	string_apply(&main_v->props.newfile_default_encoding, GTK_COMBO(pd->prefs[newfile_default_encoding])->entry);
	/* integer_apply(&main_v->props.backup_file, pd->prefs[backup_file], TRUE); */
	string_apply(&main_v->props.backup_filestring, pd->prefs[backup_filestring]);
	main_v->props.backup_abort_action = gtk_option_menu_get_history(GTK_OPTION_MENU(pd->prefs[backup_abort_action]));
	/* integer_apply(&main_v->props.backup_cleanuponclose, pd->prefs[backup_cleanuponclose], TRUE); */
	integer_apply(&main_v->props.num_undo_levels, pd->prefs[num_undo_levels], FALSE);
	/* integer_apply(&main_v->props.clear_undo_on_save, pd->prefs[clear_undo_on_save], TRUE); */
	main_v->props.modified_check_type = gtk_option_menu_get_history(GTK_OPTION_MENU(pd->prefs[modified_check_type]));
	integer_apply(&main_v->props.max_recent_files, pd->prefs[max_recent_files], FALSE);
	
	/* integer_apply(&main_v->props.restore_dimensions, pd->prefs[restore_dimensions], TRUE); */
	if (!(main_v->props.view_bars &MODE_RESTORE_DIMENSION) ) {
		integer_apply(&main_v->props.left_panel_width, pd->prefs[left_panel_width], FALSE);
		integer_apply(&main_v->globses.main_window_h, pd->prefs[main_window_h], FALSE);
		integer_apply(&main_v->globses.main_window_w, pd->prefs[main_window_w], FALSE);
	}
	string_apply(&main_v->props.tab_font_string, pd->prefs[tab_font_string]);
	main_v->props.document_tabposition = gtk_option_menu_get_history(GTK_OPTION_MENU(pd->prefs[document_tabposition]));
	main_v->props.leftpanel_tabposition = gtk_option_menu_get_history(GTK_OPTION_MENU(pd->prefs[leftpanel_tabposition]));
	main_v->props.left_panel_left = gtk_option_menu_get_history(GTK_OPTION_MENU(pd->prefs[left_panel_left]));

	/* integer_apply(&main_v->props.transient_htdialogs, pd->prefs[transient_htdialogs], TRUE); */
	
	string_apply(&main_v->props.default_basedir, pd->prefs[default_basedir]);
#ifdef EXTERNAL_GREP
#ifdef EXTERNAL_FIND
	string_apply(&main_v->props.templates_dir, pd->prefs[templates_dir]);
#endif
#endif
	integer_apply(&main_v->props.marker_i, pd->prefs[marker_i], FALSE);
	integer_apply(&main_v->props.marker_ii, pd->prefs[marker_ii], FALSE);
	integer_apply(&main_v->props.marker_iii, pd->prefs[marker_iii], FALSE);
	/* integer_apply(&main_v->props.filebrowser_two_pane_view, pd->prefs[filebrowser_two_pane_view], TRUE); */
	string_apply(&main_v->props.filebrowser_unknown_icon, pd->prefs[filebrowser_unknown_icon]);
	string_apply(&main_v->props.filebrowser_dir_icon, pd->prefs[filebrowser_dir_icon]);
	
/* kyanh, removed, 20050219
	string_apply(&main_v->props.image_thumbnailstring, pd->prefs[image_thumbnailstring]);
	string_apply(&main_v->props.image_thumbnailtype, GTK_COMBO(pd->prefs[image_thumbnailtype])->entry);
*/

	/*filetype_apply_changes(pd);*/
	/*filefilter_apply_changes(pd);*/
	highlightpattern_apply_changes(pd);
	/*browsers_apply_changes(pd);*/
	/*externals_apply_changes(pd);*/
	/*outputbox_apply_changes(pd);*/

	free_arraylist(main_v->props.filetypes);
	main_v->props.filetypes = duplicate_arraylist(pd->lists[filetypes]);

	free_arraylist(main_v->props.filefilters);
	main_v->props.filefilters = duplicate_arraylist(pd->lists[filefilters]);

	free_arraylist(main_v->props.highlight_patterns);
	main_v->props.highlight_patterns = duplicate_arraylist(pd->lists[highlight_patterns]);
	
	free_arraylist(main_v->props.browsers);
	main_v->props.browsers = duplicate_arraylist(pd->lists[browsers]);
	
	free_arraylist(main_v->props.external_commands);
	main_v->props.external_commands = duplicate_arraylist(pd->lists[external_commands]);
	
	free_arraylist(main_v->props.outputbox);
	main_v->props.outputbox = duplicate_arraylist(pd->lists[outputbox]);

	/* autotext rebuild the list */
	if (pd->att.refreshed) {/* we only update the list after user press REFRESH */
		g_ptr_array_free(main_v->props.autotext_array, TRUE);
		main_v->props.autotext_array = g_ptr_array_new();
		g_hash_table_foreach_remove(main_v->props.autotext_hashtable, (GHRFunc)autotext_remove_key_value, NULL);
		{
			GList *tmplist = g_list_first(pd->lists[autotext]);
			gint index=0;
			while (tmplist) {
				gchar **strarr = (gchar **)tmplist->data;
				if ((strarr[0][0] == '/') && !g_hash_table_lookup(main_v->props.autotext_hashtable, strarr[0])) {
					if (strlen(strarr[1]) || strlen(strarr[2])) {
						gchar **tmp_array = g_malloc(3*sizeof(gchar *));
						tmp_array[0] = unescape_string(strarr[1], TRUE);
						tmp_array[1] = unescape_string(strarr[2], TRUE);
						tmp_array[2] = NULL;
						g_ptr_array_add(main_v->props.autotext_array,tmp_array);
						g_hash_table_insert(main_v->props.autotext_hashtable, g_strdup(strarr[0]), g_strdup_printf("%d", index));
						index++;
					}
				}
				tmplist = g_list_next(tmplist);
			}
		}
	}
	/* autotext: DONE */
	/* completion */
	if (pd->atc.refreshed) {
		GList *tmplist = NULL;
		GList *search = NULL;
		
		tmplist = g_list_first(main_v->props.completion->items);
		while (tmplist) {
			g_free(tmplist->data);
			tmplist = g_list_next(tmplist);
		}

		g_completion_clear_items(main_v->props.completion);
		main_v->props.completion->items = NULL;

		tmplist = g_list_first(pd->lists[completion]);
		gchar *retstr = NULL;
		gchar **strarr = NULL;
		while (tmplist) {
			strarr = (gchar **)tmplist->data;
			retstr = unescape_string(strarr[0], TRUE);
			search = g_list_find(main_v->props.completion->items, retstr);
			if (search == NULL) {
				main_v->props.completion->items = g_list_append(main_v->props.completion->items, retstr);
			}
			tmplist = g_list_next(tmplist);
		}
		g_list_free(tmplist);

		{
#ifdef DEBUG
			g_print("updating session word list\n");
#endif
			GList *retval2 = NULL;
			GList *tmp2list = NULL;
			tmp2list = g_list_first(main_v->props.completion_s->items);
			while (tmp2list) {
#ifdef DEBUG
				g_print("found %s", (gchar*)tmp2list->data);
#endif
				search = g_list_find_custom(main_v->props.completion->items,tmp2list->data, (GCompareFunc)strcmp);
				if (search==NULL) {
					retval2 = g_list_append(retval2, g_strdup((gchar*)tmp2list->data));
#ifdef DEBUG
					g_print("... added\n");
				}else{
					g_print("... skipped\n");
#endif
				}
				tmp2list = g_list_next(tmp2list);
			}
			g_list_free(tmp2list);
			GList *tmp3list = NULL;
			tmp3list = g_list_first(main_v->props.completion_s->items);
			while (tmp3list) {
				g_free(tmp3list->data);
				tmp3list = g_list_next(tmp3list);
			}
			g_completion_clear_items(main_v->props.completion_s);
			main_v->props.completion_s->items = retval2;
		}
	}

	/* completion: DONE */
	
	/* apply the changes to highlighting patterns and filetypes to the running program */
	filetype_highlighting_rebuild(TRUE);
	filebrowser_filters_rebuild();

	all_documents_apply_settings();
	{
		GList *tmplist = g_list_first(main_v->bfwinlist);
		while (tmplist) {
			Tbfwin *bfwin = BFWIN(tmplist->data);
			DEBUG_MSG("preferences_ok_clicked_lcb, calling encoding_menu_rebuild\n");
			encoding_menu_rebuild(bfwin);
			external_menu_rebuild(bfwin); /* browsers is also rebuild here! */
			filetype_menu_rebuild(bfwin,NULL);
			DEBUG_MSG("preferences_ok_clicked_lcb, calling gui_apply_settings\n");
			gui_apply_settings(bfwin);
			left_panel_rebuild(bfwin);
			DEBUG_MSG("preferences_ok_clicked_lcb, calling doc_force_activate\n");
			doc_force_activate(bfwin->current_document);
			tmplist = g_list_next(tmplist);
		}
	}
}

static void preferences_cancel_clicked_lcb(GtkWidget *wid, Tprefdialog *pd) {
	preferences_destroy_lcb(NULL, pd);
}
static void preferences_apply_clicked_lcb(GtkWidget *wid, Tprefdialog *pd) {
	preferences_apply(pd);
	rcfile_save_all();
	/* kyanh, 20050222, this is just a workaround
	if we donnot save the settings now, we may lost everything if
	winefish crashes for some reasons after then */
}
static void preferences_ok_clicked_lcb(GtkWidget *wid, Tprefdialog *pd) {
	preferences_apply(pd);
	preferences_destroy_lcb(NULL, pd);
}

static void restore_dimensions_toggled_lcb(GtkToggleButton *togglebutton,Tprefdialog *pd) {
	gtk_widget_set_sensitive(pd->prefs[left_panel_width], !togglebutton->active);
	gtk_widget_set_sensitive(pd->prefs[main_window_h], !togglebutton->active);
	gtk_widget_set_sensitive(pd->prefs[main_window_w], !togglebutton->active);
}
static void create_backup_toggled_lcb(GtkToggleButton *togglebutton,Tprefdialog *pd) {
	gtk_widget_set_sensitive(pd->prefs[backup_filestring], togglebutton->active);
	gtk_widget_set_sensitive(pd->prefs[backup_abort_action], togglebutton->active);
}

static void preferences_dialog() {
	Tprefdialog *pd;
	GtkWidget *dvbox, *frame, *vbox1, *vbox2;
	gchar *notebooktabpositions[] = {N_("left"), N_("right"), N_("top"), N_("bottom"), NULL};
	gchar *panellocations[] = {N_("right"), N_("left"), NULL};
	gchar *modified_check_types[] = {N_("no check"), N_("check mtime and size"), N_("check mtime"), N_("check size"), NULL};

	pd = g_new0(Tprefdialog,1);
	pd->win = window_full(_("Edit preferences"), GTK_WIN_POS_NONE, 0, G_CALLBACK(preferences_destroy_lcb), pd, TRUE);
	
	dvbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(pd->win), dvbox);
	pd->noteb = gtk_notebook_new();
	/* gtk_notebook_set_homogeneous_tabs(GTK_NOTEBOOK(pd->noteb), TRUE); */
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(pd->noteb), GTK_POS_LEFT);
	
	gtk_box_pack_start(GTK_BOX(dvbox), pd->noteb, TRUE, TRUE, 0);

	/* tab: editors */
	vbox1 = gtk_vbox_new(FALSE, 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(pd->noteb), vbox1, hbox_with_pix_and_text(_("Editor"),150,TRUE));

	frame = gtk_frame_new(_("Editor options"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);
	
	pd->prefs[editor_font_string] = prefs_string(_("Font"), main_v->props.editor_font_string, vbox2, pd, string_font);
	pd->prefs[editor_tab_width] = prefs_integer(_("Tab width"), main_v->props.editor_tab_width, vbox2, pd, 1, 50);
	pd->prefs[editor_indent_wspaces] = boxed_checkbut_with_value(_("Use spaces to indent, not tabs"), GET_BIT(main_v->props.view_bars, MODE_INDENT_WITH_SPACES), vbox2);
	pd->prefs[word_wrap] = boxed_checkbut_with_value(_("Word wrap default"), GET_BIT(main_v->props.view_bars,MODE_WRAP), vbox2);
	DEBUG_MSG("preferences_dialog, colormode = %d\n", GET_BIT(main_v->props.view_bars,VIEW_COLORIZED));
	pd->prefs[defaulthighlight] = boxed_checkbut_with_value(_("Highlight syntax by default"), GET_BIT(main_v->props.view_bars,VIEW_COLORIZED), vbox2);
	pd->prefs[highlight_num_lines_count] = prefs_integer(_("Highlight # lines"), main_v->props.highlight_num_lines_count, vbox2, pd, 1, 8);

	frame = gtk_frame_new(_("Undo"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	pd->prefs[num_undo_levels] = prefs_integer(_("Undo history size"), main_v->props.num_undo_levels, vbox2, pd, 50, 10000);
	pd->prefs[clear_undo_on_save] = boxed_checkbut_with_value(_("Clear undo history on save"), GET_BIT(main_v->props.view_bars,MODE_CLEAR_UNDO_HISTORY_ON_SAVE), vbox2);

	frame = gtk_frame_new(_("Bookmark options"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	pd->prefs[bookmarks_default_store] = boxed_checkbut_with_value(_("Make permanent by default"), main_v->props.bookmarks_default_store, vbox2);
	{
		gchar *actions[] = {N_("full path"), N_("path from basedir"), N_("filename"), NULL};
		pd->prefs[bookmarks_filename_mode] = boxed_optionmenu_with_value(_("Bookmarks filename display"), main_v->props.bookmarks_filename_mode, vbox2, actions);
	}

	/* tab: Files */
	vbox1 = gtk_vbox_new(FALSE, 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(pd->noteb), vbox1, hbox_with_pix_and_text(_("Files"),152,TRUE));

	frame = gtk_frame_new(_("Encoding"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);
	{
		GList *tmplist, *poplist = g_list_append(NULL, "");
		tmplist = g_list_first(main_v->props.encodings);
		while (tmplist) {
			gchar **strarr = (gchar **)tmplist->data;
			poplist = g_list_append(poplist, strarr[1]);
			tmplist = g_list_next(tmplist);
		}
		pd->prefs[newfile_default_encoding] = prefs_combo(_("Default character set"),main_v->props.newfile_default_encoding, vbox2, pd, poplist, TRUE);
		g_list_free(poplist);
	}	
/* kyanh, removed, 10050219	
	pd->prefs[auto_set_encoding_meta] = boxed_checkbut_with_value(_("Auto set <meta> encoding tag on change"), main_v->props.auto_set_encoding_meta, vbox2);
*/
	frame = gtk_frame_new(_("Backup"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);
	pd->prefs[backup_file] = boxed_checkbut_with_value(_("Create backup on save"), GET_BIT(main_v->props.view_bars, MODE_CREATE_BACKUP_ON_SAVE), vbox2);
	pd->prefs[backup_filestring] = prefs_string(_("Backup file suffix"), main_v->props.backup_filestring, vbox2, pd, string_none);
	{
		gchar *failureactions[] = {N_("save"), N_("abort"), N_("ask"), NULL};
		pd->prefs[backup_abort_action] = boxed_optionmenu_with_value(_("Action on backup failure"), main_v->props.backup_abort_action, vbox2, failureactions);
	}
	pd->prefs[backup_cleanuponclose] = boxed_checkbut_with_value(_("Remove backupfile on close"), GET_BIT(main_v->props.view_bars, MODE_REMOVE_BACKUP_ON_CLOSE), vbox2);
	create_backup_toggled_lcb(GTK_TOGGLE_BUTTON(pd->prefs[backup_file]), pd);
	g_signal_connect(G_OBJECT(pd->prefs[backup_file]), "toggled", G_CALLBACK(create_backup_toggled_lcb), pd);

	frame = gtk_frame_new(_("Misc"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	pd->prefs[allow_multi_instances] = boxed_checkbut_with_value(_("Allow multi instances of a file"), GET_BIT(main_v->props.view_bars, MODE_ALLOW_MULTIPLE_INSTANCE), vbox2);
#ifdef WITH_MSG_QUEUE
	pd->prefs[open_in_running_bluefish] = boxed_checkbut_with_value(_("Open files in already running winefish window"),GET_BIT(main_v->props.view_bars,MODE_REUSE_WINDOW), vbox2);
#endif /* WITH_MSG_QUEUE */		
	pd->prefs[modified_check_type] = boxed_optionmenu_with_value(_("File modified on disk check "), main_v->props.modified_check_type, vbox2, modified_check_types);
	pd->prefs[max_recent_files] = prefs_integer(_("Number of files in 'Open recent'"), main_v->props.max_recent_files, vbox2, pd, 3, 100);

	frame = gtk_frame_new(_("File browser"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);
	pd->prefs[default_basedir] = prefs_string(_("Default basedir"), main_v->props.default_basedir, vbox2, pd, string_none);
	pd->prefs[filebrowser_two_pane_view] = boxed_checkbut_with_value(_("Use separate file and directory view"), GET_BIT(main_v->props.view_bars, MODE_FILE_BROWSERS_TWO_VIEW), vbox2);
	pd->prefs[filebrowser_unknown_icon] = prefs_string(_("Unknown icon"), main_v->props.filebrowser_unknown_icon, vbox2, pd, string_file);
	pd->prefs[filebrowser_dir_icon] = prefs_string(_("Directory icon"), main_v->props.filebrowser_dir_icon, vbox2, pd, string_file);

	/* tab: user interface */
	vbox1 = gtk_vbox_new(FALSE, 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(pd->noteb), vbox1, hbox_with_pix_and_text(_("User interface"), 156,TRUE));

	frame = gtk_frame_new(_("Dimensions"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);
	pd->prefs[restore_dimensions] = boxed_checkbut_with_value(_("Restore last used dimensions"), GET_BIT(main_v->props.view_bars, MODE_RESTORE_DIMENSION), vbox2);
	pd->prefs[left_panel_width] = prefs_integer(_("Initial sidebar width"), main_v->props.left_panel_width, vbox2, pd, 1, 4000);
	pd->prefs[main_window_h] = prefs_integer(_("Initial window height"), main_v->globses.main_window_h, vbox2, pd, 1, 4000);
	pd->prefs[main_window_w] = prefs_integer(_("Initial window width"), main_v->globses.main_window_w, vbox2, pd, 1, 4000);
	restore_dimensions_toggled_lcb(GTK_TOGGLE_BUTTON(pd->prefs[restore_dimensions]), pd);
	g_signal_connect(G_OBJECT(pd->prefs[restore_dimensions]), "toggled", G_CALLBACK(restore_dimensions_toggled_lcb), pd);

	frame = gtk_frame_new(_("General"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);
	
	pd->prefs[transient_htdialogs] = boxed_checkbut_with_value(_("Make LaTeX dialogs transient"), GET_BIT(main_v->props.view_bars, MODE_MAKE_LATEX_TRANSIENT), vbox2);

	pd->prefs[tab_font_string] = prefs_string(_("Notebook tab font (leave empty for gtk default)"), main_v->props.tab_font_string, vbox2, pd, string_font);
	
	pd->prefs[document_tabposition] = boxed_optionmenu_with_value(_("Document notebook tab position"), main_v->props.document_tabposition, vbox2, notebooktabpositions);
	pd->prefs[leftpanel_tabposition] = boxed_optionmenu_with_value(_("Sidebar notebook tab position"), main_v->props.leftpanel_tabposition, vbox2, notebooktabpositions);
	pd->prefs[left_panel_left] = boxed_optionmenu_with_value(_("Sidebar location"), main_v->props.left_panel_left, vbox2, panellocations);

	/* tab: Filtetypes */
	vbox1 = gtk_vbox_new(FALSE, 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(pd->noteb), vbox1, hbox_with_pix_and_text(_("Filetypes"), 153,TRUE));

	frame = gtk_frame_new(_("Filetypes"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	create_filetype_gui(pd, vbox2);
	
	frame = gtk_frame_new(_("Filefilters"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);
	
	create_filefilter_gui(pd, vbox2);

	/* tab: Hilight */
	vbox1 = gtk_vbox_new(FALSE, 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(pd->noteb), vbox1, hbox_with_pix_and_text(_("Highlighting"), 158,TRUE));

	frame = gtk_frame_new(_("Patterns"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	create_highlightpattern_gui(pd, vbox2);

	/* tab: Viewers, Filters */
	vbox1 = gtk_vbox_new(FALSE, 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(pd->noteb), vbox1, hbox_with_pix_and_text(_("Viewers, Filters"), 151,TRUE));

	frame = gtk_frame_new(_("Viewers"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	create_browsers_gui(pd, vbox2);

	frame = gtk_frame_new(_("Utilities and Filters"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	create_externals_gui(pd, vbox2);

	frame = gtk_frame_new(_("Information"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);
	gtk_box_pack_start(GTK_BOX(vbox2),gtk_label_new(
_("%f: current filename\n\
%i: input (filters)\n\
%o: output filename (filters)\n\
%%: percent sign\
")), TRUE, TRUE, 2);
	
	/* tab: TeXbox */
	vbox1 = gtk_vbox_new(FALSE, 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(pd->noteb), vbox1, hbox_with_pix_and_text(_("TeXbox"), 157,TRUE));
	
	frame = gtk_frame_new(_("TeXbox"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	create_outputbox_gui(pd, vbox2);

	/* kyanh, added, 20050221 */
	frame = gtk_frame_new(_("Information"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	create_outputbox_info_gui(pd, vbox2);

	/* tab: AutoX */
	vbox1 = gtk_vbox_new(FALSE, 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(pd->noteb), vbox1, hbox_with_pix_and_text(_("AutoX"), 151,TRUE));

	frame = gtk_frame_new(_("Autotext"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	create_autotext_gui(pd, vbox2);

	frame = gtk_frame_new(_("Word List (for Autocompletion)"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	create_completion_gui(pd, vbox2);
	
	/* tab: misc */
	vbox1 = gtk_vbox_new(FALSE, 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(pd->noteb), vbox1, hbox_with_pix_and_text(_("Miscellaneous"), 151,TRUE));

#ifdef EXTERNAL_GREP
#ifdef EXTERNAL_FIND
	frame = gtk_frame_new(_("Templates Directory"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	pd->prefs[templates_dir] = prefs_string(NULL, main_v->props.templates_dir, vbox2, pd, string_none);
#endif /* EXTERNAL_FIND */
#endif /* EXTERNAL_GREP */
	frame = gtk_frame_new(_("Column Markers"));
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	pd->prefs[marker_i] = prefs_integer(_("Marker 1"), main_v->props.marker_i, vbox2, pd, 0, 100);
	pd->prefs[marker_ii] = prefs_integer(_("Marker 2"), main_v->props.marker_ii, vbox2, pd, 0, 100);
	pd->prefs[marker_iii] = prefs_integer(_("Marker 3"), main_v->props.marker_iii, vbox2, pd, 0, 100);

	/* end tab: misc. TODO: move to static function ;) */
	
	/* end, create buttons for dialog now */
	{
		GtkWidget *ahbox, *but;
		ahbox = gtk_hbutton_box_new();
		gtk_hbutton_box_set_layout_default(GTK_BUTTONBOX_END);
		gtk_button_box_set_spacing(GTK_BUTTON_BOX(ahbox), 12);

		gtk_box_pack_start(GTK_BOX(dvbox), ahbox, FALSE, FALSE, 0);
		but = bf_gtkstock_button(GTK_STOCK_APPLY, G_CALLBACK(preferences_apply_clicked_lcb), pd);
		gtk_box_pack_start(GTK_BOX(ahbox), but, TRUE, TRUE, 0);

		but = bf_stock_cancel_button(G_CALLBACK(preferences_cancel_clicked_lcb), pd);
		gtk_box_pack_start(GTK_BOX(ahbox), but, TRUE, TRUE, 0);

		but = bf_stock_ok_button(G_CALLBACK(preferences_ok_clicked_lcb), pd);
		gtk_box_pack_start(GTK_BOX(ahbox), but, TRUE, TRUE, 0);
		gtk_window_set_default(GTK_WINDOW(pd->win), but);
	}
	gtk_widget_show_all(pd->win);
}

void open_preferences_cb(GtkWidget *wid, gpointer data) {
	preferences_dialog();
}

void open_preferences_menu_cb(gpointer callback_data,guint action,GtkWidget *widget) {
	preferences_dialog();
}
