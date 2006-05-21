/* $Id$ */

/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * bfspell.c - aspell spell checker
 *
 * Copyright (C) 2002-2004 Olivier Sessink
 * Modified for Winefish (C) 2005 2006 kyanh <kyanh@o2.pl>
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

#include "config.h"

#ifdef HAVE_LIBASPELL

#include <aspell.h>
#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h> /* isdigit() */

#include "bluefish.h"
#include "gtk_easy.h"
#include "document.h"
#include "stringlist.h"
#include "bfspell.h"

/*
 * indent -ts4 -kr
 */
typedef enum {FILTER_NONE,FILTER_TEX,FILTER_HTML} Tspellfilter;

#ifdef TEST_ONLY
gboolean hilight = 1;
#endif /* TEST_ONLY */

typedef struct {
	AspellConfig *spell_config;
	AspellSpeller *spell_checker;
	Tspellfilter filtert;
	GtkWidget *win;
	GtkWidget *lang;
	GtkWidget *filter;
	GList *langs;
	GtkWidget *dict;
	GtkWidget *runbut;
	GtkWidget *repbut;
	GtkWidget *ignbut;
	GtkWidget *in_doc;
	GtkWidget *in_sel;
	/* during the checking */
	GtkWidget *incorrectword;
	GtkWidget *suggestions;
	Tdocument *doc;
	gint offset;
	gint stop_position;
	GtkTextMark* so;
	GtkTextMark* eo;
	Tbfwin *bfwin;
} Tbfspell;

static void spell_gui_set_button_status(Tbfspell *bfspell, gboolean is_running) {
	gtk_widget_set_sensitive(bfspell->runbut,!is_running);
	gtk_widget_set_sensitive(bfspell->lang,!is_running);
	gtk_widget_set_sensitive(bfspell->repbut,is_running);
	gtk_widget_set_sensitive(bfspell->ignbut,is_running);
}

static gboolean test_unichar(gunichar ch,gpointer data) {
	if (ch == GPOINTER_TO_INT(data)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/* kyanh, added, 20060123 */
/* A=65; Z=90; a=97; z=122; \\=92 */
/* TODO: added @ support for guru mode */
static gboolean test_command(gunichar ch, gpointer data) {
	if ( ((64 <ch) && (ch < 91))  || ((96<ch) && (ch<123)) || (ch == 92) ) {
		DEBUG_MSG("test_command: passed '%c', return FALSE\n", ch);
		return FALSE;
	}else{
		DEBUG_MSG("test_command: passed '%c', return TRUE\n", ch);
		return TRUE;
	}
}
		
/* return value should be freed by the calling function */
static gchar *doc_get_next_word(Tbfspell *bfspell, GtkTextIter *itstart, GtkTextIter *itend) {
	gboolean havestart=FALSE;
	gchar *retval;
	
	if (bfspell->eo && bfspell->offset ==-1) {
		gtk_text_buffer_get_iter_at_mark(bfspell->doc->buffer,itstart,bfspell->eo);
	} else {
		gtk_text_buffer_get_iter_at_offset(bfspell->doc->buffer,itstart,bfspell->offset);
	}
	/* determines whether iter ends a natural-language word */
	havestart = gtk_text_iter_starts_word(itstart);
	while (!havestart) {
		/* HTML filter */
		DEBUG_MSG("doc_get_next_word: current iter = %c\n", gtk_text_iter_get_char(itstart));
		switch(bfspell->filtert) {
		case FILTER_HTML:
			/* 60 is the ascii code for <, 62 for > */
			if (gtk_text_iter_get_char(itstart) == 60) {
				gtk_text_iter_forward_find_char(itstart, test_unichar,GINT_TO_POINTER(62), NULL);
			}
			break;
		case FILTER_TEX:
			/* \ = 92, next to non alphatetically */
			if (gtk_text_iter_get_char(itstart) == 92) {
				DEBUG_MSG("doc_get_next_word: start a command, find forward...\n");
				gtk_text_iter_forward_find_char(itstart, test_command,NULL, NULL);
			}
			break;
		case FILTER_NONE:
		default:
			break;
		}
		if (!gtk_text_iter_forward_char(itstart)) {
			return NULL;
		}
		havestart = gtk_text_iter_starts_word(itstart);
	}
	*itend = *itstart;
	gtk_text_iter_forward_word_end(itend);

	bfspell->offset = gtk_text_iter_get_offset(itend);
	if (bfspell->offset > bfspell->stop_position) {
		return NULL;
	}
	retval = gtk_text_buffer_get_text(bfspell->doc->buffer,itstart,itend,TRUE/*include hidden chars. why?*/);
	if (strlen(retval)<1) {
		g_free(retval);
		return NULL;
	}
	return retval;
}

static void spell_add_to_session(Tbfspell *bfspell, gboolean to_session, const gchar * word) {
	DEBUG_MSG("spell_add_to_session, to_session=%d, word=%s\n",to_session,word);
	if (word && strlen(word)) {
		if (to_session) {
			aspell_speller_add_to_session(bfspell->spell_checker, word, -1);
		} else {
			aspell_speller_add_to_personal(bfspell->spell_checker, word, -1);
		}
	}
}

/* spell check for a word */
static gboolean spell_check_word(Tbfspell *bfspell, gchar * tocheck, GtkTextIter *itstart, GtkTextIter *itend) {
	DEBUG_MSG("spell_check_word: check for '%s'\n", tocheck);
	if (tocheck && !isdigit(tocheck[0])) {
		int correct = aspell_speller_check(bfspell->spell_checker, tocheck, -1);
		DEBUG_MSG("word '%s' has correct=%d\n",tocheck,correct);
		if (!correct) {
#ifdef TEST_ONLY
			if (hilight) {
				gtk_text_buffer_apply_tag(bfspell->doc->buffer, BRACEFINDER(bfspell->doc->brace_finder)->tag, itstart, itend);
				return FALSE;
			}
#endif /* TEST_ONLY */
			AspellWordList *awl = (AspellWordList *)aspell_speller_suggest(bfspell->spell_checker, tocheck,-1);
			if (!bfspell->so || !bfspell->eo) {
				bfspell->so = gtk_text_buffer_create_mark(bfspell->doc->buffer,NULL,itstart,FALSE);
				bfspell->eo = gtk_text_buffer_create_mark(bfspell->doc->buffer,NULL,itend,TRUE);
			} else {
				gtk_text_buffer_move_mark(bfspell->doc->buffer,bfspell->so,itstart);
				gtk_text_buffer_move_mark(bfspell->doc->buffer,bfspell->eo,itend);
			}
			doc_select_region(bfspell->doc, gtk_text_iter_get_offset(itstart),gtk_text_iter_get_offset(itend) , TRUE);
			bfspell->offset = -1;
			gtk_entry_set_text(GTK_ENTRY(bfspell->incorrectword), tocheck);
			gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(bfspell->suggestions)->entry), "");
			if (awl == 0) {
				DEBUG_MSG("spell_check_word error: %s\n", aspell_speller_error_message(bfspell->spell_checker));
			} else {
				GList *poplist=NULL;
				AspellStringEnumeration *els = aspell_word_list_elements(awl);
				const char *word;
				while ((word = aspell_string_enumeration_next(els)) != 0) {
					poplist = g_list_append(poplist,g_strdup(word));
				}
				if (!poplist) {
					poplist = g_list_append(poplist, g_strdup("(no suggested words)"));
				}
				gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(bfspell->suggestions)->entry), "");
				gtk_combo_set_popdown_strings(GTK_COMBO(bfspell->suggestions), poplist);
				free_stringlist(poplist);
				delete_aspell_string_enumeration(els);
			}
			return FALSE;
		}
	}
	return TRUE;
}

static void spell_run_finished(Tbfspell *bfspell) {
	GList *poplist = NULL;
	DEBUG_MSG("spell_run_finished\n");
	gtk_entry_set_text(GTK_ENTRY(bfspell->incorrectword), "");
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(bfspell->suggestions)->entry), "");
	poplist = g_list_append(poplist,"");
	gtk_combo_set_popdown_strings(GTK_COMBO(bfspell->suggestions), poplist);
	g_list_free(poplist);
	bfspell->offset = 0;
	aspell_speller_save_all_word_lists(bfspell->spell_checker);
	delete_aspell_speller(bfspell->spell_checker);
	bfspell->spell_checker = NULL;
	spell_gui_set_button_status(bfspell, FALSE);
}

static gboolean spell_run(Tbfspell *bfspell) {
	GtkTextIter itstart,itend;
	gchar *word = doc_get_next_word(bfspell, &itstart,&itend);
	DEBUG_MSG("spell_run, started, bfspell=%p, word=%s\n",bfspell,word);
#ifdef TEST_ONLY
	if (hilight) {
		while (word) {
			if (!spell_check_word(bfspell,word,&itstart,&itend)) {
				/* hilight here */
				//g_print("spell_run: error=%s\n", word);
			}
			g_free(word);
			word = doc_get_next_word(bfspell,&itstart,&itend);
		}
		spell_run_finished(bfspell);
		return FALSE;
	}
#endif /* TEST_ONLY */
	if (!word) {
		spell_run_finished(bfspell);
		return FALSE; /* finished */
	}
	while (word) {
		DEBUG_MSG("spell_run: word '%s'\n", word);
		if (spell_check_word(bfspell,word,&itstart,&itend)) {
			g_free(word);
			word = doc_get_next_word(bfspell,&itstart,&itend);
			/* continue to end here ... */
			if (!word) {
				spell_run_finished(bfspell);
				return FALSE; /* finished */
			}
		} else { /* not correct ! */
			g_free(word);
			word = NULL;
		}
	}
	return TRUE; /* not yet finished */
}

static void spell_start(Tbfspell *bfspell) {
	DEBUG_MSG("spell_start, started for bfspell=%p\n",bfspell);
	bfspell->spell_config = new_aspell_config();
	DEBUG_MSG("spell_start, created spell_config at %p\n",bfspell->spell_config);
	/*
	 * default language should come from config file, runtime from GUI,
	 * should first set the default one
	 */
	/* if (main_v->props.spell_default_lang) { */
	aspell_config_replace(bfspell->spell_config, "lang", main_v->props.spell_default_lang);
	DEBUG_MSG("spell_start, default lang=%s\n",main_v->props.spell_default_lang);
	/*
	 * it is unclear from the manual if aspell supports utf-8 in the
	 * library, the utility does not support it.. 
	 */
	aspell_config_replace(bfspell->spell_config, "encoding", "utf-8");
	/*
	 * from the aspell manual 
	 */
}

static void spell_gui_destroy(GtkWidget * widget, Tbfspell *bfspell) {
	DEBUG_MSG("spell_gui_destroy started\n");
	window_destroy(bfspell->win);
	if (bfspell->spell_checker) {
		aspell_speller_save_all_word_lists(bfspell->spell_checker);
		delete_aspell_speller(bfspell->spell_checker);
	}
	delete_aspell_config(bfspell->spell_config);
	if (bfspell->so) {
		gtk_text_buffer_delete_mark(bfspell->doc->buffer, bfspell->so);
	}
	if (bfspell->eo) {
		gtk_text_buffer_delete_mark(bfspell->doc->buffer, bfspell->eo);
	}
	g_free(bfspell);
}

static void spell_gui_cancel_clicked_cb(GtkWidget *widget, Tbfspell *bfspell) {
	spell_gui_destroy(NULL, bfspell);
}

static void spell_gui_ok_clicked_cb(GtkWidget *widget, Tbfspell *bfspell) {
	AspellCanHaveError *possible_err;
	const gchar *lang;
	DEBUG_MSG("spell_gui_ok_clicked_cb, bfspell=%p, bfwin=%p\n",bfspell, bfspell->bfwin);
	bfspell->doc = bfspell->bfwin->current_document;
	
	gint indx;
	indx = gtk_option_menu_get_history(GTK_OPTION_MENU(bfspell->lang));
	lang = g_list_nth_data(bfspell->langs, indx);
	DEBUG_MSG("spell_gui_ok_clicked_cb, indx=%d has language %s\n",indx,lang);
	
	if (lang && strlen(lang)) {
		aspell_config_replace(bfspell->spell_config, "lang", lang);
	}

	DEBUG_MSG("spell_gui_ok_clicked_cb, about to create speller for spell_config=%p\n",bfspell->spell_config);
	possible_err = new_aspell_speller(bfspell->spell_config);
	bfspell->spell_checker = 0;
	if (aspell_error_number(possible_err) != 0) {
		DEBUG_MSG(aspell_error_message(possible_err));
		/* now send an error, and stop */
		return;
	} else {
		bfspell->spell_checker = to_aspell_speller(possible_err);
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bfspell->in_doc))) {
		bfspell->stop_position = gtk_text_buffer_get_char_count(bfspell->doc->buffer);
	} else {
		GtkTextIter start, end;
		gtk_text_buffer_get_selection_bounds(bfspell->doc->buffer,&start,&end);
		bfspell->offset = gtk_text_iter_get_offset(&start);
		bfspell->stop_position = gtk_text_iter_get_offset(&end);
	}
	DEBUG_MSG("about to start spell_run()\n");
	if (spell_run(bfspell)) {
		spell_gui_set_button_status(bfspell,TRUE);
	} else {
		spell_gui_set_button_status(bfspell,FALSE);
	}
}

static void spell_gui_fill_dicts(Tbfspell *bfspell) {
	GtkWidget *menu, *menuitem;
	AspellDictInfoEnumeration * dels;
	AspellDictInfoList * dlist;
	const AspellDictInfo * entry;

	dlist = get_aspell_dict_info_list(bfspell->spell_config);
	dels = aspell_dict_info_list_elements(dlist);
	free_stringlist(bfspell->langs);
	bfspell->langs = NULL;
	menu = gtk_menu_new();	
	gtk_option_menu_set_menu(GTK_OPTION_MENU(bfspell->lang), menu);
	while ( (entry = aspell_dict_info_enumeration_next(dels)) != 0) {
		GtkWidget *label;
		menuitem = gtk_menu_item_new();
		label = gtk_label_new(entry->name);
		gtk_misc_set_alignment(GTK_MISC(label),0,0.5);
		gtk_container_add(GTK_CONTAINER(menuitem), label);
/*		g_signal_connect(G_OBJECT (menuitem), "activate",G_CALLBACK(),GINT_TO_POINTER(0));*/
		if (strcmp(entry->name, main_v->props.spell_default_lang) == 0) {
			DEBUG_MSG("spell_gui_fill_dicts, lang %s is the default language, inserting menuitem %p at position 0\n",entry->name,menuitem);
			gtk_menu_shell_insert(GTK_MENU_SHELL(menu), menuitem, 0);
			bfspell->langs = g_list_prepend(bfspell->langs,g_strdup(entry->name));
		} else {
			DEBUG_MSG("spell_gui_fill_dicts, lang %s is not the default language, appending menuitem %p\n",entry->name,menuitem);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
			bfspell->langs = g_list_append(bfspell->langs,g_strdup(entry->name));
		}
	}
	delete_aspell_dict_info_enumeration(dels);
	gtk_widget_show_all(menu);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(bfspell->lang), menu);
	DEBUG_MSG("spell_gui_fill_dicts, set item 0 as selected item\n");
	gtk_option_menu_set_history(GTK_OPTION_MENU(bfspell->lang),0);
}

static void spell_gui_add_clicked(GtkWidget *widget, Tbfspell *bfspell) {
	const gchar *original = gtk_entry_get_text(GTK_ENTRY(bfspell->incorrectword));
	if (original && (strlen(original)>1)) {
		if (gtk_option_menu_get_history(GTK_OPTION_MENU(bfspell->dict))) {
			spell_add_to_session(bfspell, TRUE,original); /* add to session dict */
		} else {
			spell_add_to_session(bfspell, FALSE,original); /* add to personal dict */
		}
	}
	if (spell_run(bfspell)) {
		spell_gui_set_button_status(bfspell,TRUE);
	} else {
		spell_gui_set_button_status(bfspell,FALSE);
	}
}
static void spell_gui_ignore_clicked(GtkWidget *widget, Tbfspell *bfspell) {
	DEBUG_MSG("ignore\n");
	if (spell_run(bfspell)) {
		spell_gui_set_button_status(bfspell,TRUE);
	} else {
		spell_gui_set_button_status(bfspell,FALSE);
	}
}
static void spell_gui_replace_clicked(GtkWidget *widget, Tbfspell *bfspell) {
	gint start, end;
	GtkTextIter iter;
	const gchar *original = gtk_entry_get_text(GTK_ENTRY(bfspell->incorrectword));
	const gchar *newstring = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(bfspell->suggestions)->entry));

	if (!original || !newstring) return;

	if (strlen(original)>0 && strlen(newstring)>0) {
		aspell_speller_store_replacement(bfspell->spell_checker,original,-1,newstring,-1);
	}

	gtk_text_buffer_get_iter_at_mark(bfspell->doc->buffer,&iter,bfspell->so);
	start = gtk_text_iter_get_offset(&iter);
	gtk_text_buffer_get_iter_at_mark(bfspell->doc->buffer,&iter,bfspell->eo);
	end = gtk_text_iter_get_offset(&iter);
	DEBUG_MSG("set %s from %d to %d\n",newstring,start,end);
	doc_replace_text(bfspell->doc, newstring, start, end);
	if (spell_run(bfspell)) {
		spell_gui_set_button_status(bfspell, TRUE);
	} else {
		spell_gui_set_button_status(bfspell, FALSE);
	}
}

static void defaultlang_clicked_lcb(GtkWidget *widget,Tbfspell *bfspell) {
	const gchar *lang;
	gint indx;
	indx = gtk_option_menu_get_history(GTK_OPTION_MENU(bfspell->lang));
	lang = g_list_nth_data(bfspell->langs, indx);
	g_free(main_v->props.spell_default_lang);
	DEBUG_MSG("defaultlang_clicked_lcb, index=%d, default lang is now %s\n",indx, lang);
	main_v->props.spell_default_lang = g_strdup(lang);
}
static void filter_changed_lcb(GtkOptionMenu *optionmenu,Tbfspell *bfspell) {
	bfspell->filtert = gtk_option_menu_get_history(GTK_OPTION_MENU(bfspell->filter));
}

static void spell_gui(Tbfspell *bfspell) {
	GtkWidget *vbox, *hbox, *but, *frame, *table, *label;
	bfspell->win = window_full(_("Check Spelling"), GTK_WIN_POS_NONE, 3, G_CALLBACK(spell_gui_destroy),bfspell, TRUE);
	vbox = gtk_vbox_new(FALSE, 2);
	gtk_container_add(GTK_CONTAINER(bfspell->win), vbox);
	
	frame = gtk_frame_new(_("Checking"));
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	table = gtk_table_new(3,5,FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);
	gtk_container_add(GTK_CONTAINER(frame), table);
	
	bfspell->incorrectword = gtk_entry_new();
	gtk_entry_set_editable(GTK_ENTRY(bfspell->incorrectword),FALSE);
	bf_mnemonic_label_tad_with_alignment(_("_Misspelled word:"), bfspell->incorrectword, 1, 0.5, table, 1, 2, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(table), bfspell->incorrectword,2,3,0,1);
	
	bfspell->suggestions = gtk_combo_new();
	bf_mnemonic_label_tad_with_alignment(_("Change _to:"), bfspell->suggestions, 1, 0.5, table, 1, 2, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(table), bfspell->suggestions,2,3,1,2);

	bfspell->ignbut = bf_generic_mnemonic_button(_("I_gnore"), G_CALLBACK(spell_gui_ignore_clicked), bfspell);
	bfspell->repbut = bf_generic_mnemonic_button(_("_Replace"), G_CALLBACK(spell_gui_replace_clicked), bfspell);
	gtk_table_attach_defaults(GTK_TABLE(table), bfspell->ignbut,3,4,0,1);
	gtk_table_attach_defaults(GTK_TABLE(table), bfspell->repbut,3,4,1,2);
	
	gtk_widget_set_sensitive(bfspell->repbut,FALSE);
	gtk_widget_set_sensitive(bfspell->ignbut,FALSE);
	
	/* lower GUI part */
	table = gtk_table_new(5,3,FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);	
	gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);

	bfspell->in_doc = gtk_radio_button_new_with_mnemonic(NULL, _("In _document"));
	bfspell->in_sel = gtk_radio_button_new_with_mnemonic(gtk_radio_button_get_group(GTK_RADIO_BUTTON(bfspell->in_doc)), _("I_n selection"));
	gtk_table_attach_defaults(GTK_TABLE(table), bfspell->in_doc,0,1,0,1);
	gtk_table_attach_defaults(GTK_TABLE(table), bfspell->in_sel,1,2,0,1);

	{
		GtkWidget *menu, *menuitem;
		bfspell->dict = gtk_option_menu_new();
		menu = gtk_menu_new();
		gtk_option_menu_set_menu(GTK_OPTION_MENU(bfspell->dict), menu);
		menuitem = gtk_menu_item_new_with_label(_("personal dictionary"));
/*		g_signal_connect(G_OBJECT (menuitem), "activate",G_CALLBACK(),GINT_TO_POINTER(0));*/
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		menuitem = gtk_menu_item_new_with_label(_("session dictionary"));
/*		g_signal_connect(G_OBJECT (menuitem), "activate",G_CALLBACK(),GINT_TO_POINTER(1));*/
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		gtk_option_menu_set_history(GTK_OPTION_MENU(bfspell->dict),0);
	}

	label = gtk_label_new(_("Dictionary:"));
	gtk_table_attach_defaults(GTK_TABLE(table), label,0,1,2,3);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), bfspell->dict,1,2,2,3);
	but = bf_generic_mnemonic_button(_("_Add"), G_CALLBACK(spell_gui_add_clicked), bfspell);
	gtk_table_attach_defaults(GTK_TABLE(table), but,2,3,2,3);

	bfspell->lang = gtk_option_menu_new();
	label = gtk_label_new(_("Language:"));
	gtk_table_attach_defaults(GTK_TABLE(table), label,0,1,3,4);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), bfspell->lang,1,2,3,4);
	but = bf_generic_mnemonic_button(_("Set defa_ult"), G_CALLBACK(defaultlang_clicked_lcb), bfspell);
	gtk_table_attach_defaults(GTK_TABLE(table), but,2,3,3,4);

	bfspell->filter = gtk_option_menu_new();
	label = gtk_label_new(_("Filter:"));
	gtk_table_attach_defaults(GTK_TABLE(table), label,0,1,4,5);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), bfspell->filter,1,2,4,5);
	{
		GtkWidget *menu, *menuitem;
		menu = gtk_menu_new();
		gtk_option_menu_set_menu(GTK_OPTION_MENU(bfspell->filter), menu);		
		menuitem = gtk_menu_item_new_with_label(_("no filter"));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		menuitem = gtk_menu_item_new_with_label(_("TeX filter"));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		menuitem = gtk_menu_item_new_with_label(_("html filter"));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	g_signal_connect(G_OBJECT(bfspell->filter),"changed",G_CALLBACK(filter_changed_lcb),bfspell);
	gtk_option_menu_set_history(GTK_OPTION_MENU(bfspell->filter),1);

	gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), FALSE, FALSE, 12);

	hbox = gtk_hbutton_box_new();
	gtk_hbutton_box_set_layout_default(GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 12);
	
	but = bf_gtkstock_button(GTK_STOCK_CLOSE, G_CALLBACK(spell_gui_cancel_clicked_cb), bfspell);
	gtk_box_pack_start(GTK_BOX(hbox),but,FALSE, FALSE, 0);
	bfspell->runbut = bf_gtkstock_button(GTK_STOCK_SPELL_CHECK,G_CALLBACK(spell_gui_ok_clicked_cb),bfspell);
	gtk_box_pack_start(GTK_BOX(hbox),bfspell->runbut,FALSE, FALSE, 0);
	
	gtk_window_set_default(GTK_WINDOW(bfspell->win), bfspell->runbut);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	gtk_widget_show_all(bfspell->win);
}

void spell_check_cb(GtkWidget *widget, Tbfwin *bfwin) {
	Tbfspell *bfspell = NULL;

	bfspell = g_new0(Tbfspell,1);
	bfwin->bfspell = bfspell;
	bfspell->bfwin = bfwin;

	spell_gui(bfspell);
	flush_queue();
	spell_start(bfspell);
	spell_gui_fill_dicts(bfspell);
}

enum {
	SP_FREE,
	SP_RUNNING,
};

static gint func_spell_check_running = 0;

gint func_spell_check(GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt) {
	DEBUG_MSG("func_spell_check: opt = %d\n", opt);
	if (func_spell_check_running) {
		/* removed tag ... */
		return 0;
	}
	func_spell_check_running = 1;

	GtkTextIter itstart,itend;
	Tbfspell *bfspell = NULL;
	bfspell = g_new0(Tbfspell,1);
	bfwin->bfspell = bfspell;
	bfspell->bfwin = bfwin;
	bfspell->doc = bfwin->current_document;
	bfspell->spell_config = new_aspell_config();
	aspell_config_replace(bfspell->spell_config, "lang", main_v->props.spell_default_lang);
	aspell_config_replace(bfspell->spell_config, "encoding", "utf-8");

	AspellCanHaveError *possible_err = new_aspell_speller(bfspell->spell_config);
	bfspell->spell_checker = 0;
	bfspell->filtert = FILTER_TEX;
	if (aspell_error_number(possible_err) != 0) {
		DEBUG_MSG(aspell_error_message(possible_err));
		delete_aspell_config(bfspell->spell_config);
		g_free(bfspell);
		return 0;
	} else {
		bfspell->spell_checker = to_aspell_speller(possible_err);
	}

	if (opt & FUNC_VALUE_0 ) {/* auto start */
		GtkTextMark *mark;
		GtkTextIter start, end;
		gtk_text_buffer_get_start_iter(bfspell->doc->buffer, &start);
		gtk_text_buffer_get_end_iter(bfspell->doc->buffer, &end);
		gtk_text_buffer_remove_tag(bfspell->doc->buffer, bfspell->doc->spell_tag, &start, &end);
		mark = gtk_text_buffer_get_insert(bfspell->doc->buffer);
		gtk_text_buffer_get_iter_at_mark(bfspell->doc->buffer, &start, mark);
		/* gtk_text_buffer_get_selection_bounds(bfspell->doc->buffer,&start,&end); */
		end = start;
		/* gtk_text_iter_forward_chars(&end, 30);
		 gtk_text_iter_backward_chars(&start, 30); */
		/* gtk_text_iter_forward_to_line_end(&end);
		gtk_text_iter_set_line_offset(&start, 0);
		*/
		gtk_text_view_forward_display_line_end(GTK_TEXT_VIEW(bfspell->doc->view), &end);
		gtk_text_view_backward_display_line_start(GTK_TEXT_VIEW(bfspell->doc->view), &start);
		bfspell->offset = gtk_text_iter_get_offset(&start);
		bfspell->stop_position = gtk_text_iter_get_offset(&end);
	}else{
		bfspell->stop_position = gtk_text_buffer_get_char_count(bfspell->doc->buffer);
	}

	gchar *word = doc_get_next_word(bfspell, &itstart,&itend);
	while (word) {
		if (!isdigit(word[0]) & !aspell_speller_check(bfspell->spell_checker, word, -1)) {
			gtk_text_buffer_apply_tag(bfspell->doc->buffer, bfspell->doc->spell_tag, &itstart, &itend);
		}
		g_free(word);
		word = doc_get_next_word(bfspell,&itstart,&itend);
	}
	bfspell->offset = 0;
	delete_aspell_speller(bfspell->spell_checker);
	bfspell->spell_checker = NULL;
	g_free(bfspell);
	func_spell_check_running = 0;
	return 1;
}
#endif /* HAVE_LIBASPELL */
