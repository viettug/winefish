/* $Id: undo_redo.c 631 2007-03-30 17:14:06Z kyanh $ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 *
 * undo_redo.c - imrpoved undo/redo functionality
 * Copyright (C) 2001-2002 Olivier Sessink
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
#include <string.h>

#include "bluefish.h" 
#include "undo_redo.h"
#include "document.h" /* doc_bind_signals() */

typedef struct {
	char *text;			/* text to be inserted or deleted */
	int start;			/* starts at this position */
	int end;				/* ends at this position */
	undo_op_t op;		/* action to execute */	
} unreentry_t;

static unregroup_t *unregroup_new(Tdocument *doc) {
	unregroup_t *newgroup;
	
	newgroup = g_malloc(sizeof(unregroup_t));
	newgroup->changed = doc->modified;
	newgroup->entries = NULL;
	DEBUG_MSG("unregroup_new, at %p with modified=%d\n", newgroup, newgroup->changed);
	return newgroup;
}

static void unreentry_destroy(unreentry_t *remove_entry) {
	if (remove_entry->text) {
		g_free(remove_entry->text);
	}
	g_free(remove_entry);
}

static void unregroup_destroy(unregroup_t *to_remove) {
	GList *tmplist;
	
	tmplist = g_list_first(to_remove->entries);
	while (tmplist) {
		unreentry_destroy((unreentry_t *)tmplist->data);
		tmplist = g_list_next(tmplist);
	}
	g_list_free(tmplist);
	g_free(to_remove);
}

static void doc_unre_destroy_last_group(Tdocument *doc) {
	DEBUG_MSG("doc_unre_destroy_last_group, called, last=%p\n", doc->unre.last);
	if (doc->unre.last) {
		unregroup_t *to_remove = doc->unre.last->data;
		doc->unre.last = g_list_previous(doc->unre.last);
		g_list_remove (doc->unre.last, to_remove);
		doc->unre.num_groups--;
		unregroup_destroy(to_remove);
	}
}

static gint unregroup_activate(unregroup_t *curgroup, Tdocument *doc, gint is_redo) {
	GList *tmplist;
	gint lastpos=-1;

	if (is_redo) {
		tmplist = g_list_last(curgroup->entries);
	} else {
		tmplist = g_list_first(curgroup->entries);
	}
	while (tmplist) {
		GtkTextIter itstart;
		unreentry_t *entry = tmplist->data;
		gtk_text_buffer_get_iter_at_offset(doc->buffer,&itstart,entry->start);
		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(doc->view),&itstart,0.05,FALSE,0.0,0.0);
		if ((entry->op == UndoInsert && !is_redo) || (entry->op == UndoDelete && is_redo)) {
			GtkTextIter itend;
			DEBUG_MSG("unregroup_activate set start to %d, end to %d and delete\n", entry->start, entry->end);
			gtk_text_buffer_get_iter_at_offset(doc->buffer,&itend,entry->end);
			gtk_text_buffer_delete(doc->buffer,&itstart,&itend);
		} else {
			DEBUG_MSG("unregroup_activate set start to %d and insert %d chars: %s\n", entry->start, strlen(entry->text), entry->text);
			gtk_text_buffer_insert(doc->buffer,&itstart,entry->text,-1);
		}
		lastpos = entry->start;		
		if (is_redo) {
			tmplist = g_list_previous(tmplist);
		} else {
			tmplist = g_list_next(tmplist);
		}
	}
	if (is_redo) {
		doc_set_modified(doc, 1);
	} else {
		DEBUG_MSG("unregroup_activate, calling set modified with %d\n", curgroup->changed);
		doc_set_modified(doc, curgroup->changed);
	}
	return lastpos;
}

static unreentry_t *unreentry_new(const char *text, int start, int end, undo_op_t op) {
	unreentry_t *new_entry;
	new_entry = g_malloc(sizeof(unreentry_t));
	DEBUG_MSG("unreentry_new, for text='%s'\n", text);
	new_entry->text = g_strdup(text);
	new_entry->start = start;
	new_entry->end = end;
	new_entry->op = op;
	return new_entry;
}

static void unre_list_cleanup(GList **list) {
	if (list && *list) {
		GList *tmplist;
		tmplist = g_list_first(*list);
		while (tmplist) {
			unregroup_destroy(tmplist->data);
			tmplist = g_list_next(tmplist);
		}
		g_list_free(*list);
		*list = NULL;
	}
}

static gint doc_undo(Tdocument *doc) {
	unregroup_t *curgroup = NULL;
	if (g_list_length(doc->unre.current->entries) > 0) {
		/* if the current group has entries we have to undo that one */
		DEBUG_MSG("doc_undo, undo the entries of the current group, and create a new group\n");
		curgroup = doc->unre.current;
		/* hmm, when this group is created, the doc->modified is not yet in the 'undo' state
		because activate is not yet called, so this group will have the wrong 'changed' value*/
		doc->unre.current = unregroup_new(doc);
		doc->unre.current->changed = curgroup->changed;
	} else if (doc->unre.first) {
		/* we have to undo the first one in the list */
		DEBUG_MSG("doc_undo, current group is empty--> undo the entries of the previous group\n");
		curgroup = doc->unre.first->data;
		doc->unre.first = g_list_remove(doc->unre.first, curgroup);
		doc->unre.num_groups--;
		/* what happens when this was the last entry in the list?*/
		DEBUG_MSG("doc_undo, removed a group, num_groups =%d\n", doc->unre.num_groups);
		if (doc->unre.num_groups == 0) {
			doc->unre.first = NULL;
			doc->unre.last = NULL;
		} else {
			doc->unre.first = g_list_first(doc->unre.first);
		}
	}
	if (curgroup) {
		doc->unre.redofirst = g_list_prepend(doc->unre.redofirst,curgroup);
		/* since activate calls doc_set_modified, and doc_set_modified sets 
		the undo/redo widgets, the lenght of the redolist should be > 0 _before_
		activate is called */
		DEBUG_MSG("doc_undo, calling unregroup_activate\n");
		return unregroup_activate(curgroup, doc, 0);
	}
	return -1;
}

static gint doc_redo(Tdocument *doc) {
	if (doc->unre.redofirst) {
		unregroup_t *curgroup = NULL;
	
		curgroup = doc->unre.redofirst->data;
		doc->unre.redofirst = g_list_remove(doc->unre.redofirst, curgroup);
		/* what happens when this was the last one of the list? does it return NULL ? */
		DEBUG_MSG("doc_redo, doc->unre.redofirst=%p\n", doc->unre.redofirst);

		doc_unre_new_group(doc);
		doc->unre.first = g_list_prepend(doc->unre.first, curgroup);
		if (!doc->unre.last) {
			doc->unre.last = doc->unre.first;
		}
		doc->unre.num_groups++;
		DEBUG_MSG("doc_redo, added a group, num_groups =%d\n", doc->unre.num_groups);
		return unregroup_activate(curgroup, doc, 1);
	}
	return -1;
}

/**
 * doc_unre_add:
 * @doc: a #Tdocument
 * @text: a #const gchar * with the deleted/inserted text
 * @start: a #gint with the start position
 * @end: a #gint with the end position
 * @op: a #undo_op_t, if this is a insert or delete call
 * 
 * adds the text to the current undo/redo group for document doc
 * with action insert or undo, dependent on the value of op
 * 
 * Return value: void
 **/
void doc_unre_add(Tdocument *doc, const char *text, gint start, gint end, undo_op_t op) {
	unreentry_t *entry=NULL;
	gboolean handled = FALSE;
	
	if (end < start) {
		gint tmp = start;
		start = end;
		end = tmp;
	}
	DEBUG_MSG("doc_unre_add, start=%d, end=%d\n", start, end);
	if (doc->unre.current->entries) {
		entry = (unreentry_t *)(doc->unre.current->entries->data);
		DEBUG_MSG("doc_unre_add, currentgroup=%p, entry=%p\n", doc->unre.current,entry );
		DEBUG_MSG("doc_unre_add, entry=%p entry->end=%d, entry->start=%d\n", entry, entry->end, entry->start);
		DEBUG_MSG("doc_unre_add, entry->op=%d, op=%d, UndoInsert=%d\n", entry->op, op, UndoInsert);
		/* do efficient, add the current one to the previous one */
		if ((entry->end == start && entry->op == UndoInsert && op == UndoInsert) 
			|| ((entry->start == end || start == entry->start)&& entry->op == UndoDelete && op == UndoDelete)) {
			gchar *newstr;
			if (op == UndoInsert) {
				/* multiple inserts can be grouped together, just add them together, and set the end
				 * to the end of the new one */
				newstr = g_strconcat(entry->text, text, NULL);
				entry->end = end;
				DEBUG_MSG("doc_unre_add, INSERT, text=%s\n", newstr);
			} else if (entry->start == end) {
				/* multiple backspaces can be grouped together, just add the new one before the
				 * old one, and set the start to the start of the new one */
				newstr = g_strconcat(text, entry->text, NULL);
				entry->start = start;
				DEBUG_MSG("doc_unre_add, BACKSPACE, text=%s\n", newstr);
			} else {
				/* multiple delete's at the same position have the same start, but the second delete
				 * can be added to the right side of the previous delete, so only the end should 
				 * be increased */
				newstr = g_strconcat(entry->text,text,NULL);
				entry->end += (end - start);
				DEBUG_MSG("doc_unre_add, DELETE, text=%s\n", newstr);
			}
			g_free(entry->text);
			entry->text = newstr;
			handled = TRUE;
		} else {
			DEBUG_MSG("doc_unre_add, NOT grouped with previous entry\n");
		}
	}
	if (!handled) {
		unreentry_t *new_entry;
		new_entry = unreentry_new(text, start, end, op);
		DEBUG_MSG("doc_unre_add, not handled yet, new entry with text=%s\n", text);
		doc->unre.current->entries = g_list_prepend(doc->unre.current->entries, new_entry);
		if (doc->unre.redofirst) {
			/* destroy the redo list, groups and entries */
			unre_list_cleanup(&doc->unre.redofirst);
			DEBUG_MSG("doc_unre_add, redolist=%p\n", doc->unre.redofirst);
		}
	}
}


static void doc_unre_start(Tdocument *doc) {
	DEBUG_MSG("doc_unre_start, started\n");
	doc_unbind_signals(doc);
}

static void doc_unre_finish(Tdocument *doc, gint cursorpos) {
	DEBUG_MSG("doc_unre_finish, started\n");
	/* now re-establish the signals */
	doc_bind_signals(doc);
/*	if (doc->highlightstate) {
		doc_need_highlighting(doc);
	}*/
	{
		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_offset(doc->buffer,&iter,cursorpos);
		gtk_text_buffer_place_cursor(doc->buffer,&iter);
	}
}

/**
 * doc_unre_new_group:
 * @doc: a #Tdocument
 * 
 * starts a new undo/redo group for document doc, all items in one group
 * are processed as a single undo or redo operation
 * 
 * Return value: void
 **/
void doc_unre_new_group(Tdocument *doc) {
	DEBUG_MSG("doc_unre_new_group, started, num entries=%d\n", g_list_length(doc->unre.current->entries));
	if (g_list_length(doc->unre.current->entries) > 0) {
		doc->unre.first = g_list_prepend(doc->unre.first, doc->unre.current);
		if (!doc->unre.last) {
			doc->unre.last = doc->unre.first;
		}
		doc->unre.num_groups++;
		DEBUG_MSG("doc_unre_new_group, added a group, num_groups =%d\n", doc->unre.num_groups);
		doc->unre.current = unregroup_new(doc);
		if (doc->unre.num_groups > main_v->props.num_undo_levels) {
			doc_unre_destroy_last_group(doc);
		}
	}
}

/**
 * doc_unre_init:
 * @doc: a #Tdocument
 * 
 * initializes the Tdocument struct for undo/redo operations
 * 
 * Return value: void
 **/
void doc_unre_init(Tdocument *doc) {
	DEBUG_MSG("doc_unre_init, started\n");
	doc->unre.first = NULL;
	doc->unre.last = NULL;
	doc->unre.current = unregroup_new(doc);
	doc->unre.num_groups = 0;
	doc->unre.redofirst = NULL;
}
/**
 * doc_unre_destroy:
 * @doc: a #Tdocument
 * 
 * cleans/free's all undo/redo information for this document (for document close etc.)
 * 
 * Return value: void
 **/
void doc_unre_destroy(Tdocument *doc) {
	/* TODO */
	DEBUG_MSG("doc_unre_destroy, about to destroy undolist %p\n",doc->unre.first );
	unre_list_cleanup(&doc->unre.first);
	DEBUG_MSG("doc_unre_destroy, about to destroy redofirst %p\n", doc->unre.redofirst);
	unre_list_cleanup(&doc->unre.redofirst);
	DEBUG_MSG("doc_unre_destroy, about to destroy current %p\n", doc->unre.current);
	unregroup_destroy(doc->unre.current);
}
/**
 * doc_unre_clear_all:
 * @doc: a #Tdocument
 * 
 * cleans all undo/redo information for doc, but re-inits the doc for new undo/redo operations
 * 
 * Return value: void
 **/

void doc_unre_clear_all(Tdocument *doc) {
	doc_unre_destroy(doc);
	doc_unre_init(doc);
}
/**
 * doc_undo_op_compare:
 * @doc: a #Tdocument
 * @testfor: a #undo_op_t, test for the last operation
 * @position: a #gint, test if this was the last position
 * 
 * tests the last undo/redo operation, if it was insert or delete, and if the position
 * does match the previous position
 * 
 * Return value: gboolean, TRUE if everything matches or if there was no previous operation, FALSE if not
 **/
/* gint doc_undo_op_compare(Tdocument *doc, undo_op_t testfor, gint position) {
	DEBUG_MSG("doc_undo_op_compare, testfor=%d, position=%d\n", testfor, position);
	if (doc->unre.current->entries && doc->unre.current->entries->data) {
		unreentry_t *entry = doc->unre.current->entries->data;
		DEBUG_MSG("doc_undo_op_compare, entry->op=%d, entry->start=%d, entry->end=%d\n", entry->op, entry->start, entry->end);
		if (entry->op == testfor) {
			gint testforpos = (entry->op == UndoDelete) ? entry->start : entry->end;
			if (testforpos == position) {
				return 1;
			}
		}
		return 0;
	}
	return 1;
} */

/**
 * doc_unre_test_last_entry:
 * @doc: a #Tdocument
 * @testfor: a #undo_op_t, test for the last operation
 * @start: a #gint, test if this was the start position, -1 if not to be tested
 * @end: a #gint, test if this was the end position, -1 if not to be tested
 * 
 * tests the last undo/redo operation, if it was (insert or delete) AND if the start AND end
 * are equal, use -1 for start or end if they do not need testing
 * 
 * Return value: gboolean, TRUE if everything matches or if there was no previous operation, FALSE if not
 **/
gboolean doc_unre_test_last_entry(Tdocument *doc, undo_op_t testfor, gint start, gint end) {
	if (doc->unre.current->entries && doc->unre.current->entries->data) {
		gboolean retval;
		unreentry_t *entry = doc->unre.current->entries->data;
		DEBUG_MSG("doc_unre_test_last_entry, start=%d, entry->start=%d, end=%d, entry->end=%d\n",start, entry->start, end, entry->end);
		retval = ((entry->op == testfor) 
				&& (start == -1 || start == entry->start) 
				&& (end == -1 || end == entry->end));
		DEBUG_MSG("doc_unre_test_last_entry, return %d\n",retval);
		return retval;
	}
	return TRUE;
}

/**
 * undo_cb:
 * @widget: a #GtkWidget *, ignored
 * @bfwin: a #Tbfwin* with the window
 * 
 * activates the last undo group on the current document
 * 
 * Return value: void
 **/
void undo_cb(GtkWidget * widget, Tbfwin *bfwin) {
	DEBUG_MSG("undo_cb, started\n");
	if (bfwin->current_document) {
		gint lastpos;
		doc_unre_start(bfwin->current_document);
		lastpos = doc_undo(bfwin->current_document);
		doc_unre_finish(bfwin->current_document, lastpos);
	}
}
/**
 * redo_cb:
 * @widget: a #GtkWidget *, ignored
 * @bfwin: a #Tbfwin* with the window
 * 
 * activates the last redo group on the current document
 * 
 * Return value: void
 **/
void redo_cb(GtkWidget * widget, Tbfwin *bfwin) {
	if (bfwin->current_document) {
		gint lastpos;
		doc_unre_start(bfwin->current_document);
		lastpos = doc_redo(bfwin->current_document);
		doc_unre_finish(bfwin->current_document,lastpos);
	}
}
/**
 * undo_all_cb:
 * @widget: a #GtkWidget *, ignored
 * @bfwin: a #Tbfwin* with the window
 * 
 * activates all undo groups on the current document
 * 
 * Return value: void
 **/
void undo_all_cb(GtkWidget * widget, Tbfwin *bfwin) {
	/* TODO  -> what needs to be done?? */
	if (bfwin->current_document) {
		gint lastpos = -1;
		doc_unre_start(bfwin->current_document);
		while (bfwin->current_document->unre.first) {
			lastpos = doc_undo(bfwin->current_document);
		}
		doc_unre_finish(bfwin->current_document, lastpos);
	}
}
/**
 * redo_all_cb:
 * @widget: a #GtkWidget *, ignored
 * @bfwin: a #Tbfwin* with the window
 * 
 * activates all redo groups on the current document
 * 
 * Return value: void
 **/
void redo_all_cb(GtkWidget * widget, Tbfwin *bfwin) {
	/* TODO -> what needs to be done?? */
	if (bfwin->current_document) {
		gint lastpos = -1;
		doc_unre_start(bfwin->current_document);
		while (bfwin->current_document->unre.redofirst) {
			lastpos = doc_redo(bfwin->current_document);
		}
		doc_unre_finish(bfwin->current_document, lastpos);
	}
}
/**
 * doc_has_undo_list:
 * @doc: a #Tdocument
 * 
 * returns TRUE if the document doc has a undo list
 * returns FALSE if there is nothing to undo
 * 
 * Return value: gboolean, TRUE if the doc has a undo list, else FALSE
 **/
#ifdef __GNUC__
__inline__ 
#endif
gboolean doc_has_undo_list(Tdocument *doc) {
	return (doc->unre.first || doc->unre.current->entries) ? TRUE : FALSE;
}
/**
 * doc_has_redo_list:
 * @doc: a #Tdocument
 * 
 * returns TRUE if the document doc has a redo list
 * returns FALSE if there is nothing to redo
 * 
 * Return value: gboolean, TRUE if the doc has a redo list, else FALSE
 **/
#ifdef __GNUC__
__inline__
#endif
gboolean doc_has_redo_list(Tdocument *doc) {
	return (doc->unre.redofirst) ? TRUE : FALSE;
}
