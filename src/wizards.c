/* $Id: wizards.c 139 2006-02-08 07:55:35Z kyanh $ */

/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * wizards.c - much magic is contained within
 *
 * Copyright (C) 1998 Olivier Sessink and Chris Mazuc
 * Copyright (C) 1999-2002 Olivier Sessink
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h> /* strtod() */
#include <time.h>

#include "bluefish.h"
#include "wizards.h" 	/* myself */
#include "gtk_easy.h" /* spinbutwithvalue and stuff*/
#include "bf_lib.h"  /* bf_str_repeat() */
#include "html_diag.h"
#include "document.h" /* doc_insert_two_strings() */
#include "pixmap.h"  /* new_pixmap() */
#include "stringlist.h"

enum {
	TABLE_DEFAULT, /* table */
	TABLE_ARRAY,
	TABLE_MATRIX
#ifdef HAVE_CONTEXT
	, TABLE_CONTEXT_NATURAL
#endif
};

enum {
	LIST_ITEM_A = 1 << 0, /* itemize, enumerate: plain \item */
	LIST_ITEM_B = 1 << 1, /* \item[] */
	LIST_PARAM_A = 1 << 2, /* dinglist, dingautolist: one mandatory parameter {} */
	LIST_PARAM_B = 1 << 3 /* enhanced enumerate: one mandatory paramter [] */
};

/**
 * repeat a string sometimes, added some details
 * @param str item string
 * @param number_of number of repeating
 * @param offset start fromoffset
 * @param show_all show detail for every items
 * @param comment comment added before details
 * @return repeating of str
 */
static gchar *bf_str_repeat_with_count(const gchar * str, gint number_of, gint offset, gboolean show_all, const gchar* comment) {
	if (!show_all && (number_of <=5) ) {
		return bf_str_repeat(str, number_of);
	}
	gchar *retstr = g_strdup("");
	gchar *tmpstr=NULL;
	gint groups_index=1;
	gint index = 1;
	gint step = 5-4*show_all;
	while (index <= number_of) {
		if ( show_all || (index+offset) %5 == 0)  {
			tmpstr = g_strdup_printf("%%%% %s %d\n",comment, groups_index * step);
			retstr = g_strconcat(retstr, tmpstr, str, NULL);
			groups_index++;
		}else{
			retstr = g_strconcat(retstr, str, NULL);
		}
		index++;
	}
	g_free(tmpstr);
	return retstr;
}

static void table_wizard_ok_lcb(GtkWidget * widget, Thtml_diag *dg) {
	gint rows, cols, type;
	gchar *rowdata=NULL,
		*tablerow=NULL, 
		*format=NULL,
		*env = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(dg->combo[1])->entry), 0, -1),
			/* env = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(dg->combo[1])->entry)); */
		*tablecontent=g_strdup(""),
		*align=g_strdup(""),
		*vsep=g_strdup("");
	
	/* gtk forces the value of one if nothing is entered */
	rows = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dg->spin[1]));
	cols = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dg->spin[2]));
	
#ifdef HAVE_CONTEXT
	if (strncmp(env,"context",7) ==0 ) {
		DEBUG_MSG("creat_table: type = context natural table\n");
		type = TABLE_CONTEXT_NATURAL;
	} else
#endif
	if (g_str_has_suffix(env,"matrix")) {/* matrix */
		DEBUG_MSG("create table: type = matrix\n");
		type = TABLE_MATRIX;
	}else{
		gchar *tmpstr = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(dg->combo[2])->entry), 0, -1);
		align = g_strndup(tmpstr, 1); /* center ->, left -> l, right -> r */
		g_free(tmpstr);
		if (g_str_has_suffix(env,"array")) {/* is array */
			type = TABLE_ARRAY;
			DEBUG_MSG("create table: type = array\n");
		}else{/* tabular, longtable */
			type = TABLE_DEFAULT;
			DEBUG_MSG("create table: type = tabular or longtable\n");
			vsep = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(dg->combo[3])->entry), 0, -1);
			if (/* !g_ascii_strcasecmp */strstr(vsep,"none")) {/* g_ascii_strcasecmp() return 0 if matched */
				DEBUG_MSG("create table: column format = none\n");
				vsep = g_strdup("");
			}
		}
	}

	gchar *tmpstr = NULL;
	gint row_index = 1 ;
	while (row_index <= rows) {
		/* number of & = number of cols - 1 */
#ifdef HAVE_CONTEXT
		if (type == TABLE_CONTEXT_NATURAL) {
			rowdata = bf_str_repeat("\t\\bTD\n\t\\eTD\n", cols);
		}else
#endif
		if (cols >1) {
			tmpstr = g_strdup_printf(_("row %d col"), row_index);
			rowdata = bf_str_repeat_with_count("&\n", cols-1, 1, FALSE, tmpstr );
		}else{
			rowdata = g_strdup("");
		}
		rowdata = g_strconcat("\n",rowdata,NULL);
		/* kyanh, TODONE: disable hline for ARRAY */
		if ((type == TABLE_DEFAULT) && GTK_TOGGLE_BUTTON(dg->check[1])->active) {
			tablerow = g_strconcat(rowdata, "\\\\\\hline\n", NULL);
		}
#ifdef HAVE_CONTEXT
		else if (type == TABLE_CONTEXT_NATURAL ){
			tablerow = g_strconcat("\\bTR\n", rowdata, "\\eTR\n", NULL);
		}
#endif
		else{
			tablerow = g_strconcat(rowdata, "\\\\\n", NULL);
		}
		tmpstr = g_strdup_printf(_("%%%% row %d\n"), row_index);
		tablecontent = g_strconcat(tablecontent, tmpstr, tablerow,NULL);
		row_index ++;
	}
	g_free(rowdata);
	g_free(tmpstr);

	if ((type == TABLE_DEFAULT) && GTK_TOGGLE_BUTTON(dg->check[1])->active) {
		tablecontent = g_strconcat("\\hline\n",tablecontent,NULL); /* add head=\hline */
	}else{
#ifdef HAVE_CONTEXT
		if (type != TABLE_CONTEXT_NATURAL ) {
#endif
			/* kyanh, TODONE: remove \\ from the last row if no \hline was added */	
			tablecontent = g_strndup(tablecontent,strlen(tablecontent)-3); /* remove last \\ */
#ifdef HAVE_CONTEXT
		}
#endif
	}
	g_free(tablerow);
	
	if (type == TABLE_DEFAULT) {
		/* BUG#5869#bilco */
		/* better? use vsep = g_strdup("") (above) */
		format = g_strconcat(vsep,align,NULL); /* |c, */
		format = bf_str_repeat(format,cols);
		format = g_strconcat("{",format,vsep,"}",NULL);
	}
	else if (type == TABLE_ARRAY) {
		format = bf_str_repeat(align,cols);
		format = g_strconcat("{",format,"}",NULL);
	}else{/* context included :) */
		format = g_strdup("");
	}
	g_free(align);
	g_free(vsep);

	DEBUG_MSG("create table: full columns format = %s\n", format);

	gchar *before_str, *after_str;
	
#ifdef HAVE_CONTEXT
	if (type == TABLE_CONTEXT_NATURAL) {
		before_str = g_strdup("\\starttable\n");
	}else{
#endif
		before_str = g_strconcat("\\begin{",env,"}",format,"\n", NULL);
#ifdef HAVE_CONTEXT
	}
#endif
	g_free(format);
#ifdef HAVE_CONTEXT
	if (type == TABLE_CONTEXT_NATURAL) {
		after_str = g_strconcat(tablecontent, "\\stoptable", NULL);
	}else{
#endif
		after_str = g_strconcat(tablecontent, "\\end{",env,"}", NULL);
#ifdef HAVE_CONTEXT
	}
#endif
	g_free(env);
	g_free(tablecontent);

	doc_insert_two_strings(dg->bfwin->current_document, before_str, after_str);
	g_free(before_str);
	g_free(after_str);
	html_diag_destroy_cb(NULL, dg);
}

/* kyanh, 20050212, added */
void table_type_changed_cb(GtkWidget* widget, Thtml_diag *dg) {
	const gchar *tmpstr = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(dg->combo[1])->entry));
	if ( g_str_has_suffix(tmpstr, "matrix")
#ifdef HAVE_CONTEXT
		    || (strncmp (tmpstr, "context", 7) == 0)
#endif
	   ) {
		gtk_widget_set_sensitive(GTK_WIDGET(dg->combo[2]), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(dg->combo[3]), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(dg->check[1]), FALSE);
	} else if (/* NOT g_*/ strncmp(tmpstr,"array",5)==0 ) {
		gtk_widget_set_sensitive(GTK_WIDGET(dg->combo[2]), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(dg->combo[3]), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(dg->check[1]), FALSE);
	}else{
		gtk_widget_set_sensitive(GTK_WIDGET(dg->combo[2]), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(dg->combo[3]), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(dg->check[1]), TRUE);
	}
}

void tablewizard_dialog(Tbfwin *bfwin) {
	GtkWidget *dgtable;
	Thtml_diag *dg;
	dg = html_diag_new(bfwin,_("Table Wizard"));

	dgtable = gtk_table_new(4, 5, 0);
	gtk_table_set_row_spacings(GTK_TABLE(dgtable), 6);
	gtk_table_set_col_spacings(GTK_TABLE(dgtable), 12);
	gtk_box_pack_start(GTK_BOX(dg->vbox), dgtable, FALSE, FALSE, 0);

	dg->spin[1] = spinbut_with_value("1", 1, 100, 1.0, 5.0);
	bf_mnemonic_label_tad_with_alignment(_("Number of _rows:"), dg->spin[1], 0, 0.5, dgtable, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(dgtable), dg->spin[1], 1, 2, 0, 1);

	dg->spin[2] = spinbut_with_value("1", 1, 100, 1.0, 5.0);
	bf_mnemonic_label_tad_with_alignment(_("Number of colu_mns:"), dg->spin[2], 0, 0.5, dgtable, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(dgtable), dg->spin[2], 1, 2, 1, 2);

	dg->check[1] = gtk_check_button_new();
	bf_mnemonic_label_tad_with_alignment(_("Add Horizontal Rules:"), dg->check[1], 0, 0.5, dgtable, 0, 1, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(dgtable), dg->check[1], 1, 2, 2, 3);

	GList *dtd_cblist = NULL;
#ifdef HAVE_CONTEXT
	dtd_cblist = g_list_append(dtd_cblist, "context");
#endif
	dtd_cblist = g_list_append(dtd_cblist, "tabular");
	dtd_cblist = g_list_append(dtd_cblist, "longtable");
	dtd_cblist = g_list_append(dtd_cblist, "array");
	dtd_cblist = g_list_append(dtd_cblist, "matrix");
	dtd_cblist = g_list_append(dtd_cblist, "bmatrix");
	dtd_cblist = g_list_append(dtd_cblist, "vmatrix");
	dtd_cblist = g_list_append(dtd_cblist, "Vmatrix");
	dtd_cblist = g_list_append(dtd_cblist, "pmatrix");
#ifdef HAVE_CONTEXT
	dg->combo[1] = combo_with_popdown("context", dtd_cblist, 0); /* kyanh, default env = context */
#else
	dg->combo[1] = combo_with_popdown("tabular", dtd_cblist, 0); /* kyanh, default env = tabular */
#endif
	bf_mnemonic_label_tad_with_alignment(_("Type (Environment):"), dg->combo[1], 0, 0.5, dgtable, 0, 1, 4, 5);
	gtk_table_attach_defaults(GTK_TABLE(dgtable), dg->combo[1], 1, 2, 4, 5);
	g_signal_connect(G_OBJECT(GTK_ENTRY(GTK_COMBO(dg->combo[1])->entry)), "changed", G_CALLBACK(table_type_changed_cb), dg);

	dtd_cblist = NULL;

	dtd_cblist = g_list_append(dtd_cblist, "center");
	dtd_cblist = g_list_append(dtd_cblist, "left");
	dtd_cblist = g_list_append(dtd_cblist, "right");
	dg->combo[2] = combo_with_popdown("center", dtd_cblist, 0);
	bf_mnemonic_label_tad_with_alignment(_("Columns Alignmen:"), dg->combo[1], 0, 0.5, dgtable, 0, 1, 5, 6);
	gtk_table_attach_defaults(GTK_TABLE(dgtable), dg->combo[2], 1, 2, 5, 6);
	g_signal_connect(G_OBJECT(GTK_ENTRY(GTK_COMBO(dg->combo[2])->entry)), "changed", G_CALLBACK(table_type_changed_cb), dg);

	dtd_cblist = NULL;
/* TODO: save history */
	dtd_cblist = g_list_append(dtd_cblist, "none");
	dtd_cblist = g_list_append(dtd_cblist, "|");
	dtd_cblist = g_list_append(dtd_cblist, "||");
	dtd_cblist = g_list_append(dtd_cblist, "@{}"); /* TODO: @{text} */
	dg->combo[3] = combo_with_popdown("|", dtd_cblist, 1);

	g_list_free(dtd_cblist);

	bf_mnemonic_label_tad_with_alignment(_("Vertical Rules:"), dg->combo[1], 0, 0.5, dgtable, 0, 1, 6, 7);
	gtk_table_attach_defaults(GTK_TABLE(dgtable), dg->combo[3], 1, 2, 6, 7);
	g_signal_connect(G_OBJECT(GTK_ENTRY(GTK_COMBO(dg->combo[3])->entry)), "changed", G_CALLBACK(table_type_changed_cb), dg);

#ifdef HAVE_CONTEXT
	gtk_widget_set_sensitive(GTK_WIDGET(dg->combo[2]), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(dg->combo[3]), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(dg->check[1]), FALSE);
#endif
	html_diag_finish(dg, G_CALLBACK(table_wizard_ok_lcb));
}


static gchar *extract_time_string(char *original_string)
{
	static gchar *return_string;
	gchar *start_ptr;
	gchar *end_ptr;
	gint string_size;

	return_string = g_malloc0(32);
	start_ptr = strchr(original_string, '(');
	start_ptr++;
	end_ptr = strchr(original_string, ')');
	string_size = end_ptr - start_ptr;
	strncat(return_string, start_ptr, string_size);
	return return_string;
}

/************************************************************************/

/* time insert struct */
typedef struct {
	GtkWidget *check[6];
	GtkWidget *label[6];
	GtkWidget *dialog;
	Tbfwin *bfwin;
} TimeInsert;


static void insert_time_destroy_lcb(GtkWidget * widget, TimeInsert * data) {
	DEBUG_MSG("insert_time_destroy_lcb, data=%p\n", data);
	window_destroy(data->dialog);
	g_free(data);
}

static void insert_time_callback(GtkWidget * widget, TimeInsert * timeinsert)
{
	gchar *temp_string;
	gchar *insert_string;
	gchar *final_string;
	gint count;

	insert_string = g_malloc0(32);
	final_string = g_malloc0(255);
	for (count = 1; count < 6; count++) {
		if (GTK_TOGGLE_BUTTON(timeinsert->check[count])->active) {
			gtk_label_get(GTK_LABEL(timeinsert->label[count]), &temp_string);
			insert_string = extract_time_string(temp_string);
			strncat(final_string, insert_string, 31);
			strncat(final_string, " ", 31);
		}
		DEBUG_MSG("insert_time_callback, count=%d\n", count);
	}
	
	DEBUG_MSG("insert_time_callback, final_string=%s\n", final_string);
	doc_insert_two_strings(timeinsert->bfwin->current_document, final_string, "");
	DEBUG_MSG("insert_time_callback, text inserted\n");
	g_free(insert_string);
	g_free(final_string);
	insert_time_destroy_lcb(NULL, timeinsert);
	DEBUG_MSG("insert_time_callback, finished\n");
}

/************************************************************************/

static void insert_time_cancel(GtkWidget * widget, TimeInsert * data)
{
	DEBUG_MSG("insert_time_cancel, data=%p\n", data);
	insert_time_destroy_lcb(widget, data);
}

/************************************************************************/
void insert_time_dialog(Tbfwin *bfwin) {

	gint month, year, count;
	time_t time_var;
	gchar *temp = NULL;
	struct tm *time_struct;
	TimeInsert *timeinsert;
	GtkWidget *ok_b, *cancel_b, *vbox, *hbox;

	timeinsert = g_malloc(sizeof(TimeInsert));
	timeinsert->bfwin = bfwin;
	time_var = time(NULL);
	time_struct = localtime(&time_var);
	DEBUG_MSG("insert_time_cb, timeinsert=%p\n", timeinsert);
	timeinsert->dialog = window_full(_("Insert Time"), GTK_WIN_POS_MOUSE
			, 12, G_CALLBACK(insert_time_destroy_lcb), timeinsert, TRUE);
	vbox = gtk_vbox_new(FALSE, 1);
	gtk_container_add(GTK_CONTAINER(timeinsert->dialog), vbox);

	for (count = 1; count < 6; count++) {
		switch (count) {
		case 1:
			temp = g_strdup_printf(_("  _Time (%i:%i:%i)"), time_struct->tm_hour, time_struct->tm_min, time_struct->tm_sec);
			break;
		case 2:
			switch (time_struct->tm_wday) {
			case 0:
				temp = g_strdup(_("  Day of the _week (Sunday)"));
				break;
			case 1:
				temp = g_strdup(_("  Day of the _week (Monday)"));
				break;
			case 2:
				temp = g_strdup(_("  Day of the _week (Tuesday)"));
				break;
			case 3:
				temp = g_strdup(_("  Day of the _week (Wednesday)"));
				break;
			case 4:
				temp = g_strdup(_("  Day of the _week (Thursday)"));
				break;
			case 5:
				temp = g_strdup(_("  Day of the _week (Friday)"));
				break;
			case 6:
				temp = g_strdup(_("  Day of the _week (Saturday)"));
				break;
			default:
				g_message(_("You appear to have a non existant day!\n"));
				temp = g_strdup(" ** Error ** see stdout");
			}					/* end of switch day of week */
			break;
		case 3:
			month = time_struct->tm_mon + 1;
			year = time_struct->tm_year;
			year = 1900 + year;
			temp = g_strdup_printf(_("  _Date (%i/%i/%i)"), time_struct->tm_mday, month, year);
			break;
		case 4:
			temp = g_strdup_printf(_("  _Unix Time (%i)"), (int) time_var);
			break;
		case 5:
			temp = g_strdup_printf(_("  Unix Date _String (%s"), ctime(&time_var));
			/* Replace \n on ')' */
			temp[strlen(temp) - 1] = ')';
			break;
		default:
			break;
		}						/* end of switch count */
		timeinsert->check[count] = gtk_check_button_new();
		timeinsert->label[count] = gtk_label_new_with_mnemonic(temp);
		gtk_label_set_mnemonic_widget(GTK_LABEL(timeinsert->label[count]), timeinsert->check[count]);
		g_free(temp);
		gtk_container_add(GTK_CONTAINER(timeinsert->check[count]), GTK_WIDGET(timeinsert->label[count]));
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(timeinsert->check[count]), TRUE, TRUE, 0);
	}							/* end of for loop */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_hseparator_new(), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 10);
	hbox = gtk_hbutton_box_new();
	gtk_hbutton_box_set_layout_default(GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 12);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	ok_b = bf_stock_ok_button(GTK_SIGNAL_FUNC(insert_time_callback), (gpointer) timeinsert);
	gtk_window_set_default(GTK_WINDOW(timeinsert->dialog), ok_b);
	cancel_b = bf_stock_cancel_button(GTK_SIGNAL_FUNC(insert_time_cancel), (gpointer) timeinsert);
	gtk_box_pack_start(GTK_BOX(hbox), cancel_b, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), ok_b, TRUE, TRUE, 0);

	gtk_widget_show_all(timeinsert->dialog);
}

/* QUICK START */

static void quickstart_ok_lcb(GtkWidget *widget, Thtml_diag *dg) {
	gchar *finalstring, *class;
	class = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(dg->combo[1])->entry), 0, -1);
	finalstring = g_strconcat(class,
"\n\\usepackage{ifpdf}\n\
\\usepackage[colorlinks,bookmarksopen]{hyperref}\n"
		/* \\usepackage[left=25mm,right=20mm,top=25mm,bottom=25mm]{geometry}", */
 	,"\\begin{document}\n", NULL);
	g_free(class);
	/* kyanh, 20050225,
	i don't know why. after inserting two strings, the iter START/END change. */
	/*gchar *tmpstr = g_strdup("\n\n\\end{document}\n");*/
	doc_insert_two_strings(dg->doc, finalstring, "\n\\end{document}\n");
	g_free(finalstring);
	/*g_free(tmpstr);*/
	/*
	if ( dg->doc->highlightstate ) {
		dg->doc->need_highlighting = TRUE;
		doc_highlight_full( dg->doc );
	}
	*/
	html_diag_destroy_cb(NULL, dg);
}

void quickstart_dialog(Tbfwin *bfwin, Ttagpopup *data) {
	GtkWidget *dgtable;
	Thtml_diag *dg;

	dg = html_diag_new(bfwin,_("Quick Start"));
#ifdef HAVE_VNTEX
	dgtable = html_diag_table_in_vbox(dg, 4, 4); /* rows, cols */
#else
	dgtable = html_diag_table_in_vbox(dg, 2, 4); /* rows, cols */
#endif
	/* Document classes */
	/* TODO: save users' modification */
	GList *dtd_cblist = NULL;
	dtd_cblist = add_to_stringlist(dtd_cblist, "\\documentclass[12pt, oneside, a4paper]{article}");
	dtd_cblist = add_to_stringlist(dtd_cblist, "\\documentclass[12pt, oneside, a4paper]{report}");
	dtd_cblist = add_to_stringlist(dtd_cblist, "\\documentclass[12pt, oneside, a4paper]{book}");
	dg->combo[1] = combo_with_popdown("\\documentclass[12pt, oneside, a4paper]{article}", dtd_cblist, 1);

	gtk_widget_set_size_request(dg->combo[1], 425, -1);
	bf_mnemonic_label_tad_with_alignment(_("Document _Class:"), dg->combo[1], 0, 0.5, dgtable, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(dgtable), dg->combo[1], 1, 4, 0, 1);

#ifdef HAVE_VNTEX
	/* Vietnam package */
	dtd_cblist = NULL;
	dtd_cblist = add_to_stringlist(dtd_cblist, "none");
	dtd_cblist = add_to_stringlist(dtd_cblist, "\\usepackage[utf8x]{vietnam}");
	dtd_cblist = add_to_stringlist(dtd_cblist, "\\usepackage[utf8]{vietnam}");
	dtd_cblist = add_to_stringlist(dtd_cblist, "\\usepackage[viscii]{vietnam}");
	dtd_cblist = add_to_stringlist(dtd_cblist, "\\usepackage[tcvn]{vietnam}");
	dtd_cblist = add_to_stringlist(dtd_cblist, "\\usepackage[vps]{vietnam}");
	dtd_cblist = add_to_stringlist(dtd_cblist, "\\usepackage[mviscii]{vietnam}");
	
	dg->combo[2] = combo_with_popdown("\\usepackage[utf8x]{vietnam}", dtd_cblist, 1);
	gtk_widget_set_size_request(dg->combo[2], 425, -1);

	g_list_free(dtd_cblist);
	bf_mnemonic_label_tad_with_alignment(_("_VnTeX Support:"), dg->combo[1], 0, 0.5, dgtable, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(dgtable), dg->combo[2], 1, 4, 1, 2);
#endif

	/* TODO: add additional packages: geometry, amsmath */

	/* finish */
	html_diag_finish(dg, G_CALLBACK(quickstart_ok_lcb));
}

/* QUICK LIST */
/* result > extra > items > result1 */
static void quicklistok_lcb(GtkWidget * widget, Thtml_diag *dg)
{
	gint rows,type;
	gchar *env, *result, *result1, *extra, *anitem, *anitem_n;
	
	rows = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dg->spin[1]));
	env = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(dg->combo[1])->entry), 0, -1);

	if (g_str_has_prefix(env,"enhance")) {
		env = g_strdup("enumerate");
		type = LIST_PARAM_B;
		anitem = g_strdup("\\item");
		anitem_n = g_strdup("\\item\n");
	}else if (g_str_has_prefix(env,"ding")) {
		type = LIST_PARAM_A;
		anitem = g_strdup("\\item");
		anitem_n = g_strdup("\\item\n");
	}else if (strncmp(env,"description", 11) ==0) {
		type = LIST_ITEM_B;
		anitem = g_strdup("\\item[]"); /* not very good */
		anitem_n = g_strdup("\\item[]\n");
	}else{
		type = LIST_ITEM_A;
		anitem = g_strdup("\\item");
		anitem_n = g_strdup("\\item\n");
	}
	if (rows ==0 ) {
		anitem = g_strdup("");
		anitem_n = g_strdup("");
	}

	result1 = g_strdup("");
	switch (type) {
	case LIST_PARAM_A:
		result =  g_strconcat("\\begin{",env,"}{",NULL);
		extra = g_strdup("}\n");
		if (rows) { result1 = bf_str_repeat_with_count(anitem_n,rows,0,FALSE,_("item")); }
		break;
	case LIST_PARAM_B:
		result =  g_strconcat("\\begin{",env,"}[",NULL);
		extra = g_strdup("]\n");
		if (rows) { result1 = bf_str_repeat_with_count(anitem_n,rows,0,FALSE,_("item")); }
		break;
	default:
		result = g_strconcat("\\begin{",env,"}\n",anitem,NULL);
		if (rows>1) { result1 = bf_str_repeat_with_count(anitem_n, rows-1, 1,FALSE,_("item")); }
		extra = g_strdup("\n");
		break;
	};
	
	result1 = g_strconcat(extra, result1, "\\end{",env,"}",NULL);
	doc_insert_two_strings(dg->doc, result, result1);

	g_free(result);
	g_free(result1);
	g_free(env);
	g_free(extra);

	html_diag_destroy_cb(widget, dg);
}

void quicklist_dialog(Tbfwin *bfwin, Ttagpopup *data) {
	Thtml_diag *dg;
	GtkWidget *dgtable;

	dg = html_diag_new(bfwin,_("Quick List"));

	dgtable = html_diag_table_in_vbox(dg, 2, 10);

	/* kyanh, default value: a list should contain at leat 2 items */
	dg->spin[1] = spinbut_with_value("2", 0, 500, 1.0, 5.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(dg->spin[1]), 1);
	bf_mnemonic_label_tad_with_alignment(_("_Rows:"), dg->spin[1], 0, 0.5, dgtable, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(dgtable), dg->spin[1], 1, 2, 0, 1);

	GList *dtd_cblist = NULL;
	dtd_cblist = g_list_append(dtd_cblist, "itemize");
	dtd_cblist = g_list_append(dtd_cblist, "description");
	dtd_cblist = g_list_append(dtd_cblist, "enumerate");
	dtd_cblist = g_list_append(dtd_cblist, "enhanced enumerate");
	dtd_cblist = g_list_append(dtd_cblist, "dinglist");
	dtd_cblist = g_list_append(dtd_cblist, "dingautolist");
	dg->combo[1] = combo_with_popdown("itemize", dtd_cblist, 0);
	bf_mnemonic_label_tad_with_alignment(_("Type (_Environment):"), dg->combo[1], 0, 0.5, dgtable, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(dgtable), dg->combo[1], 1, 2, 1, 2);

	g_list_free(dtd_cblist);
	
	html_diag_finish(dg, G_CALLBACK(quicklistok_lcb));	
}
