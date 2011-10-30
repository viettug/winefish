/* $Id: filebrowser.c 399 2006-03-28 12:35:05Z kyanh $ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * filebrowser.c the filebrowser
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

/* ******* NEW FILEBROWSER DESIGN ********
I'm thinking about a new design for the filebrowser, the 
code is too complicated right now. For now I'll just write some thoughts, so 
after we have the next stable releae we can implement it.

we'll drop the one-pane-view, we'll only have the files and directories separate

the public API:
--------------
GtkWidget *fb2_init(Tbfwin *bfwin);
void fb2_cleanup(Tbfwin *bfwin);
void fb2_set_basedir(Tbfwin *bfwin, gchar *basedir);
void fb2_focus_document(Tbfwin *bfwin, Tdocument *doc);
--------------

in the treemodel for the directories, we'll have two columns. A visible column (with the name), 
and a column with the full path,  so if a click on an item is done, it is very easy to see which 
full path corresponds to that item.

to make 'focus document' very easy, each document can have a GtkTreeIter 
pointing to the directory where the file located. If it does not have the treeiter, the 
document has not been focused before (so it is not required to have a treeiter for 
every document).

the most difficult thing to code now is when we for example open a new file, we'll have to 
find which directory item corresponds to that filename, and if it is not yet there, we'll 
have to find where we should add it. Same for a document that has not been focused before.
A possibility to do this is to have a hashtable with TreeIters to each directory that is already 
in the tree. If you then want to open some directory, you check the full path in the hashtable, 
if not exists you remove the last directory component, check again etc. until you have found a
position in the tree where you can add things.

USER INTERFACE NOTES:

each directory should be expandable by default (so have a dummy item) unless we know there are 
no subdirectories (and not the other way around like it is right now)

*/
#include <gtk/gtk.h>
#include <sys/types.h>	/* stat() getuid */
#include <sys/stat.h>	/* stat() */
#include <unistd.h>		/* stat() getuid */
#include <string.h>		/*strchr() */
#include <stdio.h>		/* rename() */	
#include <stdlib.h>		/* atoi() */

#include "bluefish.h"

#include "filebrowser.h"
#include "bf_lib.h"
#include "document.h"
#include "gtk_easy.h"	/* *_dialog(), flush_queue() */
#include "gui.h"			/* statusbar_message() */
#include "image.h" 		/* image_insert_from_filename() */
#include "menu.h" 		/* translation */
#include "project.h" 	/* project_open_from_file() */
#include "stringlist.h" /* count_array() */

#include "func_grep.h" /* open_advanced_from_filebrowser */

/*#define DEBUG_SORTING
#define DEBUG_FILTER
#define DEBUG_ADDING_TO_TREE*/

#define FILEBROWSER(var) ((Tfilebrowser *)(var))
#define FILEBROWSERCONFIG(var) ((Tfilebrowserconfig *)(var))

enum {
   TYPE_DIR,
   TYPE_FILE
};
enum {
   PIXMAP_COLUMN,
   FILENAME_COLUMN,
   N_COLUMNS
};

typedef struct {
	gchar *name;
	gboolean mode; /* 0= hide matching files, 1=show matching files */
	GList *filetypes; /* if NULL all files are OK */
} Tfilter;

typedef struct {
	gchar *name;
	struct stat stat;
	gint type;
	gboolean has_widget;
	GdkPixbuf *icon;
} Tdir_entry;

/* the Tfilebbrowser is specific for every bfwin, 
 * it is located at FILEBROWSER(bfwin->filebrowser)
 * 
 * the filebrowser is initialised if both bfwin->filebrowser
 * and filebrowser->tree are non-NULL pointers
 *
 * the tree2 and store2 pointers are only used if the two-paned filebrowser
 * is chosen in the config
 *
 * the basedir pointer might be NULL if no basedir is set, if so 
 * the showfulltree widget should be set insensitive
 *
 * if the basedir is set, the state of the toggle widget showfulltree
 * can be used to find if the basedir should be used
 */

typedef struct {
	Tfilter *curfilter;
	GtkWidget *tree;
	GtkTreeStore *store;
	GtkWidget *tree2;
	GtkListStore *store2;
	GtkWidget *dirmenu;
	GtkWidget *showfulltree;
	GtkWidget *focus_follow;
	GList *dirmenu_entries;
	gchar *last_opened_dir; /* SHOULD end on a '/' */
	gboolean last_popup_on_dir;
	Tbfwin *bfwin;
	gchar *basedir;
} Tfilebrowser;

/* the Tfilebrowserconfig is only once present in the Bluefish memory, located
 * at FILEBROWSERCONFIG(main_v->filebrowserconfig)
 *
 * it contains the filters and the filetypes with their icons
 */
typedef struct {
	GdkPixbuf *unknown_icon;
	GdkPixbuf *dir_icon;
	GList *filters;
	GList *filetypes_with_icon;
} Tfilebrowserconfig;

/***********************************************************************************/

/* some functions need to be referenced before they are declared*/
static void row_expanded_lcb(GtkTreeView *tree,GtkTreeIter *iter,GtkTreePath *path,Tfilebrowser *filebrowser);
static void populate_dir_history(Tfilebrowser *filebrowser,gboolean firsttime, gchar *activedir);

#ifdef DEBUG
void DEBUG_DUMP_TREE_PATH(GtkTreePath *path) {
	if (path) {
		gchar *tmpstr = gtk_tree_path_to_string(path);
		DEBUG_MSG("path='%s'\n", tmpstr);
		g_free(tmpstr);
	} else {
		DEBUG_MSG("path=NULL\n");
	}
}
#else
#define DEBUG_DUMP_TREE_PATH(path)
 /**/
#endif /* DEBUG */

/*
 returns TRUE if there is no basedir, or if the file is indeed in the basedir, else returns FALSE
*/
static gboolean path_in_basedir(Tfilebrowser *filebrowser, const gchar *path) {
	if (!filebrowser->basedir || strncmp(filebrowser->basedir, path, strlen(filebrowser->basedir))==0) {
		return TRUE;
	}
	return FALSE;
}


/* MODIFIES THE ITER POINTER TO BY 'found' !!!!!!!! */
static gboolean get_iter_by_filename_from_parent(GtkTreeStore *store, GtkTreeIter *parent, GtkTreeIter *found, gchar *filename) {
	gboolean valid;
	GtkTreeIter iter;
	valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(store),&iter,parent);
	while (valid) {
		gchar *name;
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, FILENAME_COLUMN, &name, -1);
		DEBUG_MSG("get_iter_by_filename_from_parent, comparing '%s' and '%s'\n",name,filename);
		if (strcmp(name, filename)==0) {
			*found = iter;
			DEBUG_MSG("get_iter_by_filename_from_parent, found!!\n");
			g_free(name);
			return TRUE;
		}
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
		g_free(name);
	}
	DEBUG_MSG("get_iter_by_filename_from_parent, NOT found..\n");
	return FALSE;
}

static gchar *return_filename_from_path(Tfilebrowser *filebrowser, GtkTreeModel *store,const GtkTreePath *thispath) {
	gboolean valid = TRUE;
	gchar *retval = NULL, *tmp;
	gboolean showfulltree = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(filebrowser->showfulltree));
	GtkTreePath *path = gtk_tree_path_copy(thispath);
	DEBUG_MSG("return_filename_from_path, ");
	DEBUG_DUMP_TREE_PATH(path);
	if (!path) {
		return NULL;
	}
	while (valid) {
		gchar *name = NULL, *tmpstr;
		GtkTreeIter iter;

		gtk_tree_model_get_iter(GTK_TREE_MODEL(store),&iter,path);
		gtk_tree_model_get (GTK_TREE_MODEL(store), &iter, FILENAME_COLUMN, &name, -1);
		tmpstr = gtk_tree_path_to_string(path);
		DEBUG_MSG("return_filename_from_path, path='%s' contains '%s'\n", tmpstr, name);
		g_free(tmpstr);
		if (!showfulltree && filebrowser->basedir && strcmp(name, filebrowser->basedir) == 0) {
			tmp = retval;
			retval = g_strconcat(name, retval,NULL);
			g_free(tmp);
			DEBUG_MSG("return_filename_from_path, found the root (%s ?), retval=%s\n", name,retval);
			valid = FALSE;
		} else if ((showfulltree || filebrowser->basedir == NULL) && name[strlen(name)-1]=='/') {
			/* found the root or protocol/server */
			tmp = retval;
			retval = g_strconcat(name, retval,NULL);
			g_free(tmp);
			DEBUG_MSG("return_filename_from_path, found the root (%s ?), retval=%s\n", name,retval);
			valid = FALSE;
		} else {
			tmp = retval;
			if (retval) {
				retval = g_strconcat(name, "/", retval,NULL);
			} else {
				retval = g_strdup(name);
			}
			g_free(tmp);
			DEBUG_MSG("return_filename_from_path, added a file or directory component %s, retval=%s\n",name,retval);
			valid = gtk_tree_path_up(path);
		}
		g_free(name);
	}
	gtk_tree_path_free(path);
	DEBUG_MSG("return_filename_from_path, returning %s\n",retval);
	return retval;
}

static gboolean view_filter(Tfilebrowser *filebrowser, Tdir_entry *entry) {
	if (!main_v->props.filebrowser_show_hidden_files) {
		if (entry->name[0] == '.') {
#ifdef DEBUG_FILTER
			DEBUG_MSG("view_filter, hidden file %s\n", entry->name);
#endif
			return FALSE;
		}
	}
	
	if (entry->type == TYPE_DIR) {
		/* all other filter types don't apply to directories */
		return TRUE;
	}
	
	if (!main_v->props.filebrowser_show_backup_files) {
		if (entry->name[strlen(entry->name)-1] == '~') {
#ifdef DEBUG_FILTER
			DEBUG_MSG("view_filter, backup file %s\n", entry->name);
#endif
			return FALSE;
		}
	}
	
	{
		gboolean default_retval;
		GList *tmplist;
		if (filebrowser->curfilter && filebrowser->curfilter->filetypes && filebrowser->curfilter->mode == 1) {
		/* there is some filter active, set the default to FALSE except if we find the extension */
			tmplist = g_list_first(filebrowser->curfilter->filetypes);
			default_retval = FALSE;
		} else {
			if (filebrowser->curfilter && filebrowser->curfilter->filetypes) { /* mode == 0, hide certain files */
				tmplist = g_list_first(filebrowser->curfilter->filetypes);
				while (tmplist) {
					Tfiletype *filetype = (Tfiletype *)tmplist->data;
					if (filename_test_extensions(filetype->extensions, entry->name)) {
						return FALSE;
					}
					tmplist = g_list_next(tmplist);
				}
			}
			/* everything that comes here only needs the icon */
			tmplist = g_list_first(FILEBROWSERCONFIG(main_v->filebrowserconfig)->filetypes_with_icon);
			default_retval = TRUE;
		}
		while (tmplist) {
			Tfiletype *filetype = (Tfiletype *)tmplist->data;
			if (filename_test_extensions(filetype->extensions, entry->name)) {
				entry->icon = filetype->icon;
				return TRUE;
			}
			tmplist = g_list_next(tmplist);
		}
		return default_retval;
	}
}


static gboolean find_name_in_dir_entries(GList *list, gchar *name) {
	GList *tmplist;
	
	tmplist = g_list_first(list);
	while (tmplist) {
		Tdir_entry *entry = (Tdir_entry *)tmplist->data;
		if (strcmp(entry->name, name)==0) {
			entry->has_widget = TRUE;
			return TRUE;
		}
		tmplist = g_list_next(tmplist);
	}
	return FALSE;
}
static GList *return_dir_entries(Tfilebrowser *filebrowser,gchar *dirname) {
	GDir *dir;
	GList *tmplist=NULL;
	Tdir_entry *entry;
	const gchar *name;
	gchar *ode_dirname = get_filename_on_disk_encoding(dirname);
	dir = g_dir_open(ode_dirname, 0, NULL);
	while ((name = g_dir_read_name(dir))) {
		gchar *fullpath;
		entry = g_new(Tdir_entry,1);
		entry->icon = NULL;
		entry->name = get_utf8filename_from_on_disk_encoding(name);
		fullpath = g_strconcat(ode_dirname, "/", entry->name, NULL);
		stat(fullpath, &entry->stat);
		g_free(fullpath);
		if (S_ISDIR(entry->stat.st_mode)) {
			entry->type = TYPE_DIR;
		} else {
			entry->type = TYPE_FILE;
			
		}
		if (!view_filter(filebrowser,entry)) {
			/* free entry */
			g_free(entry->name);
			g_free(entry);
		} else {
			entry->has_widget = FALSE;
			tmplist = g_list_append(tmplist, entry);
		}
	}
	g_dir_close(dir);
	g_free(ode_dirname);
	return tmplist;
}
static void free_dir_entries(GList *direntrylist) {
	GList *tmplist = g_list_first(direntrylist);
	while (tmplist) {
		g_free(((Tdir_entry *)(tmplist->data))->name);
		g_free(tmplist->data);
		tmplist = g_list_next(tmplist);
	}
	g_list_free(direntrylist);
}

static GtkTreePath *return_path_from_filename(Tfilebrowser *filebrowser,gchar *this_filename) {
	gchar *tmpstr, *p, *filepath;
	gint totlen, curlen, prevlen=1;
	gboolean found=TRUE;
	gboolean showfulltree = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(filebrowser->showfulltree));
	GtkTreeStore *store = filebrowser->store;
	GtkTreeIter iter,parent;

	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
	parent = iter;
	if (showfulltree) {
		/* the first thing we do is that we try to find the root element where this filename should belong to */
		gchar *root = return_root_with_protocol(this_filename);
		DEBUG_MSG("return_path_from_filename, root=%s\n",root);
		if (root && root[0] != '/') {
			DEBUG_MSG("return_path_from_filename, the root is an URL\n");
			/* we have an URL like sftp:// or http:// or something like that */
			found = FALSE;
			while (!found && gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter)) {
				gchar *found_root;
				gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, FILENAME_COLUMN, &found_root, -1);
				DEBUG_MSG("return_path_from_filename, searching for the root, comparing %s and %s\n",root,found_root);
				if (strcmp(found_root, root)==0) {
					/* we found the root for this protocol/server */
					parent = iter;
					prevlen = strlen(root);
					found = TRUE;
					DEBUG_MSG("return_path_from_filename, we found '%s'!\n",found_root);
				}
				g_free(found_root);
			}
			if (found && strcmp(root, this_filename)==0) {
				/* we are looking for the root!! */
				g_free(root);
				DEBUG_MSG("return_path_from_filename, 'this_filename' and 'root' are equal! return the GtkTreePath for 'found_root'\n");
				return gtk_tree_model_get_path(GTK_TREE_MODEL(store),&parent);
			}
			g_free(root);
		} else {
			DEBUG_MSG("return_path_from_filename, the root is local\n");
			g_free(root);
			/* it is a local file, that normally means that we can use the first toplevel node in the tree */
			if (!showfulltree && filebrowser->basedir) {
				gint basedirlen = strlen(filebrowser->basedir);
				gint thislen = strlen(this_filename);
				DEBUG_MSG("return_path_from_filename, basedirlen=%d, strlen(filename)=%d\n",basedirlen,thislen);
				if (thislen == basedirlen && strcmp(this_filename, filebrowser->basedir)==0) {
					/* we are looking for the basedir itself */
					return gtk_tree_model_get_path(GTK_TREE_MODEL(store),&parent);
				} else if ((strlen(this_filename)>basedirlen) && (strncmp(filebrowser->basedir, this_filename, basedirlen)==0)) {
					prevlen = strlen(filebrowser->basedir);
				} else {
					DEBUG_MSG("return_path_from_filename, the path is outside the current basedir?!?\n");
					found = FALSE;
				}
			}
		}
	} else { /* there is no showfulltree */
		if (filebrowser->basedir) { 
			gint basedirlen = strlen(filebrowser->basedir);
			if (strncmp(this_filename,filebrowser->basedir,basedirlen)==0) {
				prevlen = strlen(filebrowser->basedir);
			} else {
				DEBUG_MSG("return_path_from_filename, file %s is not in basedir %s, return NULL\n",this_filename,filebrowser->basedir);
				return NULL;
			}
		}
	}

	totlen = strlen(this_filename);
	filepath = g_strdup(this_filename);
	
	p = strchr(&filepath[prevlen], '/');
	
	if (p) {
		curlen = strlen(p);
	} else {
		curlen = prevlen;
	}
	DEBUG_MSG("return_path_from_filename, filepath=%s\n",filepath);
	while (p && found) {
		gboolean valid;
		
		found = FALSE;
		curlen = strlen(p);
		tmpstr = g_strndup(&filepath[prevlen], (totlen - curlen - prevlen));
		/* now search for this dirname */
		DEBUG_MSG("return_path_from_filename, searching for '%s'\n",tmpstr);
		valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(store),&iter,&parent);
		while (valid) {
			gchar *found_name;
			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, FILENAME_COLUMN, &found_name, -1);
			DEBUG_MSG("return_path_from_filename, comparing %s and %s\n", found_name, tmpstr);
			if (strcmp(found_name, tmpstr)==0) {
				/* we found the node!!, make this node the next parent */
				found = TRUE;
				parent = iter;
				g_free(found_name);
				break;
			}
			valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
			g_free(found_name);
		}
		g_free(tmpstr);
		prevlen = (totlen - curlen+1);
		p = strchr(&filepath[prevlen], '/');
	}
	g_free(filepath);
	if (found) {
		DEBUG_MSG("return_path_from_filename, we DID found the path, return a path\n");
		return gtk_tree_model_get_path(GTK_TREE_MODEL(store),&iter);
	} else {
		DEBUG_MSG("return_path_from_filename, we did NOT found the path, return NULL\n");
		return NULL;
	}
}

static int comparefunc(GtkTreeModel *store, gchar *name, GtkTreeIter *iter, const gchar *text, gint type) {
#ifdef DEBUG_SORTING
	DEBUG_MSG("comparefunc, comparing %s and %s\n", name, text);
#endif
	gboolean iter_is_dir;
	iter_is_dir = gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), iter);
	if (type == TYPE_DIR) {
		if (iter_is_dir) {
			return strcmp(name, text);
		} else {
			return 1;
		}
	} else {
		if (iter_is_dir) {		
			return -1;
		} else {
			return strcmp(name, text);
		}
	}
}

static gboolean get_iter_at_correct_position(GtkTreeModel *store, GtkTreeIter *parent, GtkTreeIter *iter, const gchar *text, gint type, gboolean two_paned) {
	gint num_children, jumpsize, child, possible_positions;
	gchar *name;

	DEBUG_MSG("get_iter_at_correct_position, started, parent=%p, two_paned=%d\n",parent,two_paned);
	/* if parent=NULL it will return the top level, which is correct for the liststore */
	num_children = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), parent);
	if (num_children == 0) {
		DEBUG_MSG("get_iter_at_correct_position, 0 children so we return FALSE\n");
		return FALSE;
	}
	possible_positions = num_children+1;
	jumpsize = (possible_positions+1)/2;
	child = possible_positions - jumpsize;

#ifdef DEBUG_SORTING
	DEBUG_MSG("get_iter_at_correct_position, num_children=%d, jumpsize=%d\n", num_children, jumpsize);
#endif
	if (num_children == 1) {
		gint compare;
		/* if parent=NULL it will return the child from the toplevel which is correct for the liststore */
		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), iter, parent, 0);
		gtk_tree_model_get(GTK_TREE_MODEL(store),iter,FILENAME_COLUMN,&name, -1);
		if (parent == NULL || two_paned) {
			compare = strcmp(name,text);
		} else {
			compare = comparefunc(store, name, iter, text, type);
		}
		g_free(name);
		if (compare == 0) {
			return FALSE;
		} else if (compare < 0) {
			return FALSE;
		} else {
			return TRUE;
		}
	}
	while (jumpsize > 0) {
		gint compare;
		if (child > num_children) child = num_children;
		if (child < 1) child = 1;
#ifdef DEBUG_SORTING
		DEBUG_MSG("get_iter_at_correct_position, compare %d\n", child);
#endif
		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), iter, parent, child-1);
		gtk_tree_model_get(GTK_TREE_MODEL(store),iter,FILENAME_COLUMN,&name, -1);
		if (parent == NULL || two_paned) {
			compare = strcmp(name,text);
		} else {
			compare = comparefunc(store, name, iter, text, type);
		}
		g_free(name);
		if (compare == 0) {
			return TRUE;
		} else if (compare > 0) {
			jumpsize = (jumpsize > 3) ? (jumpsize+1)/2 : jumpsize-1;
			child = child - jumpsize;
		} else {
			jumpsize = (jumpsize > 3) ? (jumpsize+1)/2 : jumpsize-1;
			child = child + jumpsize;
			if (jumpsize<=0) {
#ifdef DEBUG_SORTING
				DEBUG_MSG("get_iter_at_correct_position, jumpsize too small and compare negative --> next iter\n");
#endif
				return gtk_tree_model_iter_next(GTK_TREE_MODEL(store), iter);
			}
		}
#ifdef DEBUG_SORTING
		DEBUG_MSG("get_iter_at_correct_position, compare=%d, jumpsize=%d, child=%d\n", compare, jumpsize, child);
#endif
	}
	return TRUE;
}

static GtkTreeIter add_tree_item(GtkTreeIter *parent, Tfilebrowser *filebrowser, const gchar *text, gint type, GdkPixbuf *pixbuf) {
	GtkTreeIter iter1, iter2;
	DEBUG_MSG("add_tree_item, started for %s\n",text);
	if (!pixbuf) {
		if (type == TYPE_DIR) {	
			pixbuf = FILEBROWSERCONFIG(main_v->filebrowserconfig)->dir_icon;
		} else {
			pixbuf = FILEBROWSERCONFIG(main_v->filebrowserconfig)->unknown_icon;
		}
	}

	if (type == TYPE_FILE && filebrowser->store2 && parent==NULL) {
		if (get_iter_at_correct_position(GTK_TREE_MODEL(filebrowser->store2),parent,&iter2,text,type,TRUE)) {
			DEBUG_MSG("add_tree_item, inserting %s in store2\n", text);
			gtk_list_store_insert_before(GTK_LIST_STORE(filebrowser->store2),&iter1,&iter2);
		} else {
			DEBUG_MSG("add_tree_item, appending %s in store2\n", text);
			gtk_list_store_append(GTK_LIST_STORE(filebrowser->store2),&iter1);
		}
		gtk_list_store_set(GTK_LIST_STORE(filebrowser->store2),&iter1,
					PIXMAP_COLUMN, pixbuf,
					FILENAME_COLUMN, text,
					-1);
	} else {
		if (get_iter_at_correct_position(GTK_TREE_MODEL(filebrowser->store),parent,&iter2,text,type,(filebrowser->store2 != NULL))) {
	#ifdef DEBUG
			DEBUG_MSG("add_tree_item, inserting %s in store1\n", text);
	#endif
			gtk_tree_store_insert_before(GTK_TREE_STORE(filebrowser->store),&iter1,parent,&iter2);
		} else {
	#ifdef DEBUG
			DEBUG_MSG("add_tree_item, appending %s in store1\n", text);
	#endif
			gtk_tree_store_append(GTK_TREE_STORE(filebrowser->store), &iter1, parent);
		}
		gtk_tree_store_set(GTK_TREE_STORE(filebrowser->store), &iter1,
					PIXMAP_COLUMN, pixbuf,
					FILENAME_COLUMN, text,
					-1);
	}
	return iter1;
}

static void refresh_dir_by_path_and_filename(Tfilebrowser *filebrowser, GtkTreePath *thispath, gchar *filename) {
	GList *direntrylist;
	gboolean valid, remove_dot = FALSE;
	GtkTreeIter myiter, remove_dot_iter;
#ifdef DEBUG_ADDING_TO_TREE
	DEBUG_MSG("refresh_dir_by_path_and_filename, filename='%s'\n", filename);
#endif
	if (filebrowser->last_opened_dir) g_free(filebrowser->last_opened_dir);
	filebrowser->last_opened_dir = ending_slash(filename);
	populate_dir_history(filebrowser,FALSE, filebrowser->last_opened_dir);
	direntrylist = return_dir_entries(filebrowser,filename);
	{
		GtkTreePath *path = gtk_tree_path_copy(thispath);		
		DEBUG_DUMP_TREE_PATH(path);
		gtk_tree_path_down(path);
		valid = gtk_tree_model_get_iter(GTK_TREE_MODEL(filebrowser->store),&myiter,path);
		gtk_tree_path_free(path);
	}
	while (valid) {
		gchar *name;
		gtk_tree_model_get(GTK_TREE_MODEL(filebrowser->store), &myiter, 1, &name, -1);
		if (!find_name_in_dir_entries(direntrylist, name)) {
			if (direntrylist == NULL && strcmp(name, ".")==0) {
#ifdef DEBUG_ADDING_TO_TREE
				DEBUG_MSG("refresh_dir_by_path_and_filename, not removing fake entry for empty \n");
#endif
				valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(filebrowser->store), &myiter);
			} else {
				if (strcmp(name, ".")==0) {
					/* the dot entry is removed as last one, because the rows fail expanding if they are emptied on the way*/
					remove_dot_iter = myiter;
					valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(filebrowser->store), &myiter);
					remove_dot = TRUE;
				} else {
					GtkTreeIter myiter2 = myiter;
					valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(filebrowser->store), &myiter);
#ifdef DEBUG_ADDING_TO_TREE
					DEBUG_MSG("refresh_dir_by_path_and_filename, removing row for name %s\n", name);
#endif
					gtk_tree_store_remove(filebrowser->store,&myiter2);
				}
			}
		} else {
			valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(filebrowser->store), &myiter);
		}
		g_free(name);
	}
	/* we should also check the entries in the listtore, if they have to be refreshed as well */
	if (filebrowser->store2) {
		valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(filebrowser->store2),&myiter);
		while (valid) {
			gchar *name;
			gtk_tree_model_get(GTK_TREE_MODEL(filebrowser->store2), &myiter, 1, &name, -1);
			if (!find_name_in_dir_entries(direntrylist, name)) {
				GtkTreeIter myiter2 = myiter;
				valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(filebrowser->store2), &myiter);
				DEBUG_MSG("refresh_dir_by_path_and_filename, removing %s from liststore\n",name);
				gtk_list_store_remove(filebrowser->store2,&myiter2);
			} else {
				DEBUG_MSG("refresh_dir_by_path_and_filename, %s is valid\n",name);
				valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(filebrowser->store2), &myiter);
			}
			g_free(name);
		}
	}
	
	/* now add the new entries */
	{
		GList *tmplist;
		gtk_tree_model_get_iter(GTK_TREE_MODEL(filebrowser->store),&myiter,thispath);
		
		tmplist = g_list_first(direntrylist);
		while (tmplist) {
			Tdir_entry *entry = (Tdir_entry *)tmplist->data;
			if (!entry->has_widget) {
				GtkTreeIter myiter2;
#ifdef DEBUG_ADDING_TO_TREE
				DEBUG_MSG("refresh_dir_by_path_and_filename, adding row for name=%s\n", entry->name);
#endif
				if (entry->type == TYPE_FILE && filebrowser->store2) {
					myiter2 = add_tree_item(NULL, filebrowser, entry->name, entry->type, entry->icon);
				} else {
					myiter2 = add_tree_item(&myiter, filebrowser, entry->name, entry->type, entry->icon);
				}
				if (entry->type == TYPE_DIR && !filebrowser->store2) {
#ifdef DEBUG_ADDING_TO_TREE
					DEBUG_MSG("refresh_dir_by_path_and_filename, %s is TYPE_DIR so we setup the fake item\n", entry->name);
#endif
					add_tree_item(&myiter2, filebrowser, ".", TYPE_FILE, entry->icon);
				}
			}
			tmplist = g_list_next(tmplist);
		}
	}
	if (remove_dot) {
		gtk_tree_store_remove(filebrowser->store,&remove_dot_iter);
	}
	free_dir_entries(direntrylist);
	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(filebrowser->tree));
}

/* can return NULL if for example the filepath is not in the basedir */
static GtkTreePath *build_tree_from_path(Tfilebrowser *filebrowser, const gchar *filepath) {
	GtkTreeIter iter;
	gboolean showfulltree = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(filebrowser->showfulltree));
	DEBUG_MSG("build_tree_from_path, started for filebrowser=%p and filepath=%s\n",filebrowser,filepath);

	if (!showfulltree && filebrowser->basedir && strncmp(filepath, filebrowser->basedir, strlen(filebrowser->basedir))!=0) {
		DEBUG_MSG("build_tree_from_path, filepath %s outside basedir %s, returning NULL\n",filepath, filebrowser->basedir);
		return NULL;
	}
	
	/* first build path from root to here */
	{
		gchar *tmpstr, *p;
		gint totlen, curlen, prevlen=0;
		/* if we're working on an URL, prevlen should be 0, else it should be 1 */
		if (filepath[0] == '/') prevlen = 1;
		
		if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(filebrowser->store), &iter)) {
			if (!showfulltree && filebrowser->basedir) {
				DEBUG_MSG("build_tree_from_path, creating root %s\n",filebrowser->basedir);
				iter = add_tree_item(NULL, filebrowser, filebrowser->basedir, TYPE_DIR, NULL);
			} else {
				DEBUG_MSG("build_tree_from_path, creating root /\n");
				iter = add_tree_item(NULL, filebrowser, "/", TYPE_DIR, NULL);
			}
		}
		
		totlen = strlen(filepath);
		DEBUG_MSG("build_tree_from_path, totlen=%d\n", totlen);
		if (prevlen > totlen) {
			prevlen = totlen;
		}
		if (!showfulltree && filebrowser->basedir) {
			prevlen = strlen(filebrowser->basedir);
		}
		DEBUG_MSG("build_tree_from_path, initial prevlen=%d, searching for / in '%s'\n", prevlen, &filepath[prevlen]);
		if (prevlen >= totlen) {
			DEBUG_MSG("build_tree_from_path, it seems we're building only the root ?!?\n");
			p = NULL;
		} else {
			gchar *root = return_root_with_protocol(filepath);
			if (root && root[0] != '/') {
				DEBUG_MSG("finding iter for '%s' in the root of the tree\n",root);
				if (!get_iter_by_filename_from_parent(filebrowser->store, NULL, &iter, root)) {
					DEBUG_MSG("not found --> adding '%s'\n",root);
					iter = add_tree_item(NULL, filebrowser, root, TYPE_DIR, NULL);
				}
				prevlen = strlen(root);
			}
			g_free(root);
			p = strchr(&filepath[prevlen], '/');
		}
		while (p) {
			curlen = strlen(p);
	#ifdef DEBUG_ADDING_TO_TREE
			DEBUG_MSG("build_tree_from_path, curlen=%d\n", curlen);
	#endif
			tmpstr = g_strndup(&filepath[prevlen], (totlen - curlen - prevlen));
	#ifdef DEBUG_ADDING_TO_TREE
			DEBUG_MSG("build_tree_from_path, tmpstr='%s'\n", tmpstr);
	#endif
			if (!get_iter_by_filename_from_parent(filebrowser->store, &iter, &iter, tmpstr)) {
				iter = add_tree_item(&iter, filebrowser, tmpstr, TYPE_DIR, NULL);
			}
			g_free(tmpstr);
			prevlen = (totlen - curlen+1);
			p = strchr(&filepath[prevlen], '/');
		}
	}
	{
		gchar *dirname;
		GList *direntrylist, *tmplist;
		
		dirname = g_path_get_dirname(filepath);
		DEBUG_MSG("build_tree_from_path, dirname='%s'\n",dirname);
		
		if (filebrowser->last_opened_dir) g_free(filebrowser->last_opened_dir);
		DEBUG_MSG("build_tree_from_path, freed last_opened_dir\n");
		filebrowser->last_opened_dir = ending_slash(dirname);
		populate_dir_history(filebrowser,FALSE, filebrowser->last_opened_dir);
		DEBUG_MSG("build_tree_from_path, after ending slash\n");

		/* we need to add an empty item to each dir so it can be expanded */		
		direntrylist = return_dir_entries(filebrowser,dirname);
		DEBUG_MSG("build_tree_from_path, after return_dir_entries\n");
		tmplist = g_list_first(direntrylist);
		while (tmplist) {
			Tdir_entry *entry = (Tdir_entry *)tmplist->data;
			DEBUG_MSG("build_tree_from_path, entry=%s\n",entry->name);
			if (entry->type == TYPE_DIR) {
				GtkTreeIter tmp;
				tmp = add_tree_item(&iter, filebrowser, entry->name, TYPE_DIR, NULL);
				add_tree_item(&tmp, filebrowser, ".", TYPE_FILE, NULL);
			} else {
				if (filebrowser->store2) {
					add_tree_item(NULL, filebrowser, entry->name, TYPE_FILE, entry->icon);
				} else {
					add_tree_item(&iter, filebrowser, entry->name, TYPE_FILE, entry->icon);
				}
			}
			tmplist = g_list_next(tmplist);
		}
		g_free(dirname);
		DEBUG_MSG("build_tree_from_path, freed dirname\n");
		free_dir_entries(direntrylist);
		DEBUG_MSG("build_tree_from_path, cleaned direntrylist\n");
		/* if the toplevel is an empty directory, it needs a '.' item as well */
		if (!gtk_tree_model_iter_has_child(GTK_TREE_MODEL(filebrowser->store), &iter)) {
			add_tree_item(&iter, filebrowser, ".", TYPE_FILE, NULL);
		}
	}
	return gtk_tree_model_get_path(GTK_TREE_MODEL(filebrowser->store),&iter);
}

/* I guess the dir must have a trailing slash in this function .. not sure 
if the GtkTreePath is already known, one should call refresh_dir_by_path_and_filename() directly
instead of this function
*/
void filebrowser_refresh_dir(Tfilebrowser *filebrowser, gchar *dir) {
	if (filebrowser->tree && dir) {
		/* get the path for this dir */
		GtkTreePath *path = return_path_from_filename(filebrowser, dir);
		DEBUG_DUMP_TREE_PATH(path);
		if (!path) return;
		/* check if the dir is expanded, or if we have a two paned view, return if not */	
		if (filebrowser->store2 || gtk_tree_view_row_expanded(GTK_TREE_VIEW(filebrowser->tree), path)) {
			DEBUG_MSG("refresh_dir, it IS expanded, or we have a two paned view\n");
			/* refresh it */
			refresh_dir_by_path_and_filename(filebrowser, path, dir);
		} else {
			DEBUG_MSG("refresh_dir, it is NOT expanded in a single paned view, returning\n");
		}
		gtk_tree_path_free(path);
	}
}



void bfwin_filebrowser_refresh_dir(Tbfwin *bfwin, gchar *dir) {
	if (bfwin->filebrowser) {
		gboolean showfulltree = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(FILEBROWSER(bfwin->filebrowser)->showfulltree));
		if (!showfulltree && !path_in_basedir(FILEBROWSER(bfwin->filebrowser), dir)) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FILEBROWSER(bfwin->filebrowser)->showfulltree),TRUE);
		}
		filebrowser_refresh_dir(FILEBROWSER(bfwin->filebrowser), dir);
	}
}

static void filebrowser_expand_to_root(Tfilebrowser *filebrowser, const GtkTreePath *this_path) {
	GtkTreePath *path = gtk_tree_path_copy(this_path);
	g_signal_handlers_block_matched(G_OBJECT(filebrowser->tree), G_SIGNAL_MATCH_FUNC,
					0, 0, NULL, row_expanded_lcb, NULL);
#ifndef HAVE_ATLEAST_GTK_2_2
	gtktreepath_expand_to_root(filebrowser->tree, path);
#else 
	gtk_tree_view_expand_to_path(GTK_TREE_VIEW(filebrowser->tree), path);
#endif
	g_signal_handlers_unblock_matched(G_OBJECT(filebrowser->tree), G_SIGNAL_MATCH_FUNC,
					0, 0, NULL, row_expanded_lcb, NULL);
	gtk_tree_path_free(path);
}


/**
 * filebrowser_get_path_from_selection:
 * @model: #GtkTreeModel*
 * @treeview: #GtkTreeView*
 * @iter: #GtkTreeIter
 *
 * Retrieves a path to the currently selected item in the filebrowser->
 * iter may be omitted, set to NULL (and usually is).
 *
 * Returns: #GtkTreePath identifying currently selected item. NULL if no item is selected.
 **/
static GtkTreePath *filebrowser_get_path_from_selection(GtkTreeModel *model, GtkTreeView *treeview, GtkTreeIter *iter) {
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter localiter;
	GtkTreeIter *usethisiter = (iter != NULL) ? iter : &localiter;
	DEBUG_MSG("filebrowser_get_path_from_selection, selection=%p for treeview %p and model %p\n", selection, treeview, model);
	if (gtk_tree_selection_get_selected(selection,&model,usethisiter)) {
		return gtk_tree_model_get_path(GTK_TREE_MODEL(model),usethisiter);
	}
	return NULL;
}

/* is_directory is only meaningful if you have a two paned view and you want the directory name 
*/
static gchar *get_selected_filename(Tfilebrowser *filebrowser, gboolean is_directory) {
	GtkTreePath *path;
	gchar *retval=NULL;
	if (filebrowser->store2 && !is_directory) {
		path = filebrowser_get_path_from_selection(GTK_TREE_MODEL(filebrowser->store2),GTK_TREE_VIEW(filebrowser->tree2),NULL);
		if (path) {
			gchar *tmp2, *name;
			GtkTreeIter iter;
			gtk_tree_model_get_iter(GTK_TREE_MODEL(filebrowser->store2),&iter,path);
			gtk_tree_model_get(GTK_TREE_MODEL(filebrowser->store2), &iter, FILENAME_COLUMN, &name, -1);
			tmp2 = g_strconcat(filebrowser->last_opened_dir, name, NULL);
			gtk_tree_path_free(path);
			g_free(name);
			retval = tmp2;
		}
	} else {
		path = filebrowser_get_path_from_selection(GTK_TREE_MODEL(filebrowser->store),GTK_TREE_VIEW(filebrowser->tree),NULL);
		if (path) {
			gchar *filename = return_filename_from_path(filebrowser,GTK_TREE_MODEL(filebrowser->store), path);
			gtk_tree_path_free(path);
			retval = filename;
		}
	}
	return retval;
}

/**
 * filebrowser_open_dir:
 * bfwin: #Tbfwin* with filebrowser window
 * @dirarg const #char * dirname or filename to focus on
 *
 * This function makes the filebrowser zoom to a designated directory,
 * if the dirname is a directory, it should end on a slash /
 *
 **/
void filebrowser_open_dir(Tbfwin *bfwin,const gchar *dirarg) {
	Tfilebrowser *filebrowser = FILEBROWSER(bfwin->filebrowser);
	DEBUG_MSG("filebrowser_open_dir, filebrowser=%p\n",filebrowser);
	if (filebrowser && filebrowser->tree) {
		gchar *dir;
		GtkTreePath *path;
		gboolean showfulltree = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(filebrowser->showfulltree));
		if (!showfulltree && !path_in_basedir(filebrowser, dirarg)) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(filebrowser->showfulltree),TRUE);
		}
		
		dir = path_get_dirname_with_ending_slash(dirarg);
		path = return_path_from_filename(filebrowser, dir);
		DEBUG_MSG("filebrowser_open_dir, called for '%s', dir is '%s'\n", dirarg, dir);
		DEBUG_DUMP_TREE_PATH(path);
		if (!path) {
			DEBUG_MSG("filebrowser_open_dir, '%s' does NOT exist in the tree, building..\n", dir);
			path = build_tree_from_path(filebrowser, dir);
		} else {
			DEBUG_MSG("filebrowser_open_dir, it exists in tree, refreshing\n");
			refresh_dir_by_path_and_filename(filebrowser, path, dir);
		}
		if (path) {
			DEBUG_MSG("filebrowser_open_dir, now scroll to the path\n");
			filebrowser_expand_to_root(filebrowser,path);
			gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(filebrowser->tree),path,0,TRUE,0.5,0.5);
			gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(filebrowser->tree)),path);
			gtk_tree_path_free(path);
		}
		g_free(dir);
	}
}

typedef struct {
	GtkWidget *win;
	GtkWidget *entry;
	gchar *basedir; /* must have a trailing slash */
	gint is_file;
	Tfilebrowser *filebrowser;
} Tcfod;

static void create_file_or_dir_destroy_lcb(GtkWidget *widget, Tcfod *ws) {
	window_destroy(ws->win);
	g_free(ws->basedir);
	g_free(ws);
}

static void create_file_or_dir_cancel_clicked_lcb(GtkWidget *widget, Tcfod *ws) {
	create_file_or_dir_destroy_lcb(NULL, ws);
}

static void create_file_or_dir_ok_clicked_lcb(GtkWidget *widget, Tcfod *ws) {
	gchar *newname, *name;

	name = gtk_editable_get_chars(GTK_EDITABLE(ws->entry), 0, -1);	
	if (name) {
		if (strlen(name)) {
			newname = g_strconcat(ws->basedir,name, NULL);
			DEBUG_MSG("create_file_or_dir_ok_clicked_lcb, newname=%s\n", newname);
			if (file_exists_and_readable(newname)) {
				error_dialog(ws->filebrowser->bfwin->main_window,_("Error creating path"),_("The specified pathname already exists."));
			} else {
				if (ws->is_file) {
					doc_new_with_new_file(ws->filebrowser->bfwin,newname);
				} else {
					gchar *ondiskencoding;
					ondiskencoding = get_filename_on_disk_encoding(newname);
					if(mkdir(ondiskencoding, 0755)== -1) {
/*						error_dialog(_("Error creating directory"),strerror(errno));*/
					}
					g_free(ondiskencoding);
				}
			}
			g_free(newname);
			DEBUG_MSG("create_file_or_dir_ok_clicked_lcb, refreshing basedir %s\n", ws->basedir);
			filebrowser_refresh_dir(ws->filebrowser,ws->basedir);
		}
		g_free(name);
	}
	create_file_or_dir_destroy_lcb(NULL, ws);
}

static void create_file_or_dir_win(Tfilebrowser *filebrowser, gint is_file) {
	/* if we have a one pane view, we need to find the selection; where to add the directory, 
	if we have a two pane view, we need to find out IF we have a selection, if not we add it 
	in the current directory*/
	GtkTreePath *selection;
	GtkTreeIter iter;
	gchar *basedir = NULL;
	selection = filebrowser_get_path_from_selection(GTK_TREE_MODEL(filebrowser->store),GTK_TREE_VIEW(filebrowser->tree),&iter);
	DEBUG_MSG("create_file_or_dir_win, selection ");
	DEBUG_DUMP_TREE_PATH(selection);
	if (filebrowser->store2 && !selection) {
		/* there is no selection, we'll use the current last used dir */
		if (filebrowser->last_opened_dir) {
			basedir = ending_slash(filebrowser->last_opened_dir);
		}
	} else {
		GtkTreePath *path;
		gchar *tmp;
		if (filebrowser->store2 || gtk_tree_model_iter_has_child(GTK_TREE_MODEL(filebrowser->store), &iter)) {
			/* the selection does point to a directory, not a file */
			DEBUG_MSG("create_file_or_dir_win, is a directory: filebrowser->store2=%p, tree_model_iter_has_child=%d\n",filebrowser->store2 ,gtk_tree_model_iter_has_child(GTK_TREE_MODEL(filebrowser->store), &iter));
		} else {
			/* the selection points to a file, get the parent */
			GtkTreeIter parent;
			DEBUG_MSG("create_file_or_dir_win, a file is selected\n");
			gtk_tree_model_iter_parent(GTK_TREE_MODEL(filebrowser->store), &parent, &iter);
			iter = parent;
		}
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(filebrowser->store),&iter);
		tmp = return_filename_from_path(filebrowser,GTK_TREE_MODEL(filebrowser->store),path);
		basedir = ending_slash(tmp);
		gtk_tree_path_free(path);
	}
	DEBUG_MSG("create_file_or_dir_win, basedir=%s, selection=%p\n",basedir,selection);
	if (selection) gtk_tree_path_free(selection);
	if (basedir) {
		Tcfod *ws;
		gchar *title;
		GtkWidget *vbox, *but, *hbox;

		ws = g_malloc(sizeof(Tcfod));
		ws->filebrowser = filebrowser;
		ws->basedir = basedir;
		if (is_file) {
			title = _("File name");
		} else {
			title = _("Directory name");
		}
		ws->is_file = is_file;

		ws->win = window_full2(title, GTK_WIN_POS_MOUSE, 5,G_CALLBACK(create_file_or_dir_destroy_lcb), ws, TRUE, NULL);
		vbox = gtk_vbox_new(FALSE, 12);
		gtk_container_add(GTK_CONTAINER(ws->win), vbox);
		ws->entry = boxed_entry_with_text(NULL, 250, vbox);
	
		hbox = gtk_hbutton_box_new();
		gtk_hbutton_box_set_layout_default(GTK_BUTTONBOX_END);
		gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 6);
	
		but = bf_stock_cancel_button(G_CALLBACK(create_file_or_dir_cancel_clicked_lcb), ws);
		gtk_box_pack_start(GTK_BOX(hbox),but , FALSE, FALSE, 0);	
	
		but = bf_stock_ok_button(G_CALLBACK(create_file_or_dir_ok_clicked_lcb), ws);
		gtk_box_pack_start(GTK_BOX(hbox),but , FALSE, FALSE, 0);	
		
		gtk_entry_set_activates_default(GTK_ENTRY(ws->entry), TRUE);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		gtk_widget_grab_focus(ws->entry);
		gtk_widget_grab_default(but);
		gtk_widget_show_all(GTK_WIDGET(ws->win));
	}
}


static void row_expanded_lcb(GtkTreeView *tree,GtkTreeIter *iter,GtkTreePath *path,Tfilebrowser *filebrowser) {
	gchar *filename = return_filename_from_path(filebrowser,GTK_TREE_MODEL(filebrowser->store),path);
	DEBUG_MSG("row_expanded_lcb, started on filename=%s\n", filename);
	statusbar_message(filebrowser->bfwin,_("opening directory..."), 500);
	flush_queue();
	refresh_dir_by_path_and_filename(filebrowser, path, filename);
	g_free(filename);
	DEBUG_MSG("row_expanded_lcb, finished\n");
}

/* needs the UTF8 encoded filename */
static void handle_activate_on_file(Tfilebrowser *filebrowser, gchar *filename) {
	Tfiletype *ft = get_filetype_by_filename_and_content(filename, NULL);
	DEBUG_MSG("handle_activate_on_file, file %s has type %p\n",filename, ft);
	if (ft == NULL || ft->editable) {
		doc_new_with_file(filebrowser->bfwin,filename, FALSE, FALSE);
	} else if (strcmp(ft->type, "image")==0) {
		/* gchar *relfilename = create_relative_link_to(filebrowser->bfwin->current_document->filename, filename);
		*/
		/* kyanh, 20050227, BUG[200502]#8
		Orignal:
			image_insert_from_filename(filebrowser->bfwin,relfilename);
		*/
		image_insert_from_filename(filebrowser->bfwin,filename);
		/* g_free(relfilename); */
	} else if (strcmp(ft->type, "wfproject") == 0) {
		project_open_from_file(filebrowser->bfwin, filename, -1);
	} else {
		DEBUG_MSG("handle_activate_on_file, file %s is not-editable, do something special now?\n",filename);
	}
	DEBUG_MSG("handle_activate_on_file, finished\n");
}

/* Called both by the filebrowser "activate"-signal and filebrowser_rpopup_open_lcb */
static void row_activated_lcb(GtkTreeView *tree, GtkTreePath *path,GtkTreeViewColumn *column, Tfilebrowser *filebrowser) {
	if (filebrowser->store2) {
		/* it is a dir, expand it */
		if (!gtk_tree_view_row_expanded(tree, path)) {
			gtk_tree_view_expand_row(tree,path,FALSE);
		}
	} else {
		GtkTreeIter iter;
		gchar *filename = return_filename_from_path(filebrowser,GTK_TREE_MODEL(filebrowser->store),path);
		DEBUG_MSG("row_activated_lcb, filename=%s\n", filename);
		/* test if this is a dir */
		gtk_tree_model_get_iter(GTK_TREE_MODEL(filebrowser->store),&iter,path);
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(filebrowser->store),&iter)) {
			if (gtk_tree_view_row_expanded(tree, path)) {
				gtk_tree_view_collapse_row(tree,path);
			} else {
				gtk_tree_view_expand_row(tree,path,FALSE);
			}
		} else {
			handle_activate_on_file(filebrowser,filename);
		}
		g_free(filename);
	}
	DEBUG_MSG("row_activated_lcb, finished\n");
}

/*
 * filebrowser_rpopup_rename
 *
 * Called by filebrowser-> Renames the currently selected item.
 * If the selected item is an opened document, doc_save() is used.
 * Else, it uses the same logic as the former callback to filebrowser->
 *
 * widget and data are not used.
 */
static void filebrowser_rpopup_rename(Tfilebrowser *filebrowser) {
	gchar *oldfilename;
	/* this function should, together with doc_save() use a common backend.. */
	oldfilename = get_selected_filename(filebrowser, FALSE);
	if (oldfilename) {
		Tdocument *tmpdoc;
		GList *alldocs;
		gchar *newfilename=NULL, *errmessage = NULL, *dirname;
		/* Use doc_save(doc, 1, 1) if the file is open. */
		alldocs = return_allwindows_documentlist();
		tmpdoc = documentlist_return_document_from_filename(alldocs,oldfilename);
		g_list_free(alldocs);
		if (tmpdoc != NULL) {
			DEBUG_MSG("File is open. Calling doc_save().\n");
	
			/* If an error occurs, doc_save takes care of notifying the user.
			 * Currently, nothing is done here.
			 */	
			doc_save(tmpdoc, TRUE, TRUE, FALSE);
		} else {
			/* Promt user, "File/Move To"-style. */
			newfilename = ask_new_filename(filebrowser->bfwin, oldfilename, oldfilename, TRUE);
			if (newfilename) {
				gchar *old_OnDiEn, *new_OnDiEn; /* OnDiskEncoding */
				old_OnDiEn = get_filename_on_disk_encoding(oldfilename);
				new_OnDiEn = get_filename_on_disk_encoding(newfilename);
				if(rename(old_OnDiEn, new_OnDiEn) != 0) {
					errmessage = g_strconcat(_("Could not rename\n"), oldfilename, NULL);
				}
				g_free(old_OnDiEn);
				g_free(new_OnDiEn);
			}
		} /* if(oldfilename is open) */
	
		if (errmessage) {
			error_dialog(filebrowser->bfwin->main_window,errmessage, NULL);
			g_free(errmessage);
		} else {
			/* Refresh the appropriate parts of the filebrowser-> */
			char *tmp;
	
			tmp = g_path_get_dirname(oldfilename);
			DEBUG_MSG("Got olddirname %s\n", tmp);
			dirname = ending_slash(tmp);
			filebrowser_refresh_dir(filebrowser,dirname);
			g_free(dirname);
			g_free(tmp);
			
			if (newfilename && (tmp = path_get_dirname_with_ending_slash(newfilename))) { /* Don't refresh the NULL-directory.. */
				DEBUG_MSG("Got newdirname %s\n", tmp);
				dirname = ending_slash(tmp);
				filebrowser_refresh_dir(filebrowser,dirname);
				g_free(dirname);
				g_free(tmp);
			}
		} /* if(error) */
		if (newfilename) {
			g_free(newfilename);
		}
		g_free(oldfilename);
	}
}

static void filebrowser_rpopup_delete(Tfilebrowser *filebrowser) {
	gchar *filename = get_selected_filename(filebrowser, FALSE);
	if (filename) {
		gchar *buttons[] = {GTK_STOCK_CANCEL, GTK_STOCK_DELETE, NULL};
		gchar *label;
		gint retval;
		label = g_strdup_printf(_("Are you sure you want to delete \"%s\" ?"), filename);
		retval = multi_query_dialog(filebrowser->bfwin->main_window,label, _("If you delete this file, it will be permanently lost."), 0, 0, buttons);
		g_free(label);
		if (retval == 1) {
			gchar *errmessage = NULL;
			gchar *tmp, *dir, *ondiskenc;
			DEBUG_MSG("file_list_rpopup_file_delete %s\n", filename);
			ondiskenc = get_filename_on_disk_encoding(filename);
			if( unlink(ondiskenc) != 0) {
				errmessage = g_strconcat(_("Could not delete \n"), filename, NULL);
			}
			g_free(ondiskenc);
			if (errmessage) {
				error_dialog(filebrowser->bfwin->main_window,errmessage, NULL);
				g_free(errmessage);
			} else {
				GList *alldocs = return_allwindows_documentlist();
				Tdocument *exdoc = documentlist_return_document_from_filename(alldocs, filename);
				if (exdoc) document_unset_filename(exdoc);
				g_list_free(alldocs);
			}
			tmp = g_path_get_dirname(filename);
			dir = ending_slash(tmp);
			g_free(tmp);
			filebrowser_refresh_dir(filebrowser,dir);
			g_free(dir);
			
		}
		g_free(filename);
	}
}

static void filebrowser_rpopup_refresh(Tfilebrowser *filebrowser) {
	GtkTreePath *path1;
	GtkTreeIter iter;
	path1 = filebrowser_get_path_from_selection(GTK_TREE_MODEL(filebrowser->store),GTK_TREE_VIEW(filebrowser->tree),&iter);
	if (path1) {
		gchar *tmp, *dir;
		GtkTreePath *path2;
		if (filebrowser->store2 || gtk_tree_model_iter_has_child(GTK_TREE_MODEL(filebrowser->store), &iter)) {
			DEBUG_MSG("create_file_or_dir_win, a dir is selected\n");
		} else {
			GtkTreeIter parent;
			DEBUG_MSG("create_file_or_dir_win, a file is selected\n");
			gtk_tree_model_iter_parent(GTK_TREE_MODEL(filebrowser->store), &parent, &iter);
			iter = parent;
		}
		path2 = gtk_tree_model_get_path(GTK_TREE_MODEL(filebrowser->store),&iter);
		tmp = return_filename_from_path(filebrowser,GTK_TREE_MODEL(filebrowser->store),path2);
		DEBUG_MSG("filebrowser_rpopup_refresh_lcb, return_filename_from_path returned %s for path %p\n", tmp, path2);
		dir = ending_slash(tmp);
		DEBUG_MSG("filebrowser_rpopup_refresh_lcb, ending_slash returned %s\n", dir);
		refresh_dir_by_path_and_filename(filebrowser, path2, dir);
		g_free(tmp);
		g_free(dir);
		gtk_tree_path_free(path2);
		gtk_tree_path_free(path1);
	}
}

static void filebrowser_rpopup_action_lcb(Tfilebrowser *filebrowser,guint callback_action, GtkWidget *widget) {
	switch (callback_action) {
	case 1: {
		gchar *filename = get_selected_filename(filebrowser, FALSE);
		if (filename) {
			GdkRectangle r;
			gtk_tree_view_get_visible_rect(GTK_TREE_VIEW(filebrowser->tree),&r);
			handle_activate_on_file(filebrowser, filename);
			g_free(filename);
			gtk_tree_view_scroll_to_point(GTK_TREE_VIEW(filebrowser->tree),-1,r.y);
		}
	} break;
	case 2:
		filebrowser_rpopup_rename(filebrowser);
	break;
	case 3:
		filebrowser_rpopup_delete(filebrowser);
	break;
	case 4:
		create_file_or_dir_win(filebrowser, TRUE);
	break;
	case 5:
		create_file_or_dir_win(filebrowser,FALSE);
	break;
	case 6:
		filebrowser_rpopup_refresh(filebrowser);
	break;
#ifdef EXTERNAL_GREP
#ifdef EXTERNAL_FIND
	case 7: {
		gchar *path = get_selected_filename(filebrowser, TRUE);
		if (path) {
			open_advanced_from_filebrowser(filebrowser->bfwin, path);
			g_free(path);
		}
	} break;
#endif
#endif
	case 8: {
		gchar *path = get_selected_filename(filebrowser, TRUE);
		DEBUG_MSG("filebrowser_rpopup_action_lcb, path=%s\n", path);
		if (path) {
			filebrowser_set_basedir(filebrowser->bfwin, path);
			g_free(path);
		}
	} break;
	}
}

static Tfilter *find_filter_by_name(const gchar *name) {
	GList *tmplist = g_list_first(FILEBROWSERCONFIG(main_v->filebrowserconfig)->filters);
	while(tmplist) {
		Tfilter *filter = (Tfilter *)tmplist->data;
		if (strcmp(filter->name, name)==0) {
			return filter;
		}
		tmplist = g_list_next(tmplist);
	}
	return NULL;
}

static void filebrowser_rpopup_filter_toggled_lcb(GtkWidget *widget, Tfilebrowser *filebrowser) {
	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		/* loop trough the filters for a filter with this name */
		const gchar *name = gtk_label_get_text(GTK_LABEL(GTK_BIN(widget)->child));
		Tfilter *filter = find_filter_by_name(name);
		DEBUG_MSG("filebrowser_rpopup_filter_toggled_lcb, setting curfilter to %p from name %s\n", filter, name);
		filebrowser->curfilter = filter;
		if (main_v->props.last_filefilter) g_free(main_v->props.last_filefilter);
		main_v->props.last_filefilter = g_strdup(filter->name);
		filebrowser_rpopup_refresh(filebrowser);
	}
}

static void filebrowser_rpopup_shf_toggled_lcb(GtkWidget *widget, Tfilebrowser *filebrowser) {
	main_v->props.filebrowser_show_hidden_files = GTK_CHECK_MENU_ITEM(widget)->active;
	filebrowser_rpopup_refresh(filebrowser);
}

static void filebrowser_rpopup_sbf_toggled_lcb(GtkWidget *widget, Tfilebrowser *filebrowser) {
	main_v->props.filebrowser_show_backup_files = GTK_CHECK_MENU_ITEM(widget)->active;
	filebrowser_rpopup_refresh(filebrowser);
}

static GtkItemFactoryEntry filebrowser_dirmenu_entries[] = {
#ifdef EXTERNAL_GREP
#ifdef EXTERNAL_FIND	
	{ N_("/Open _Advanced..."),			NULL,	filebrowser_rpopup_action_lcb,		7,	"<Item>" },
#endif
#endif
	
	{ N_("/_Refresh"),			NULL,	filebrowser_rpopup_action_lcb,		6,	"<Item>" },
	{ N_("/_Set as basedir"),			NULL,	filebrowser_rpopup_action_lcb,		8,	"<Item>" }
};

static GtkItemFactoryEntry filebrowser_bothmenu_entries[] = {
	{ N_("/New _File"),			NULL,	filebrowser_rpopup_action_lcb,	4,	"<Item>" },
	{ N_("/_New Directory"),	NULL,	filebrowser_rpopup_action_lcb,		5,	"<Item>" },
	{ "/sep",	NULL,	NULL,		0,	"<Separator>" }
};

static GtkItemFactoryEntry filebrowser_filemenu_entries[] = {
	{ N_("/_Open"),			NULL,	filebrowser_rpopup_action_lcb,		1,	"<Item>" },
	{ N_("/Rena_me"),		NULL,	filebrowser_rpopup_action_lcb,			2,	"<Item>" },
	{ N_("/_Delete"),		NULL,	filebrowser_rpopup_action_lcb,			3,	"<Item>" }
};

static GtkWidget *filebrowser_rpopup_create_menu(Tfilebrowser *filebrowser, gboolean is_directory) {
	GtkWidget *menu, *menu_item;
	GtkItemFactory *menumaker;

	filebrowser->last_popup_on_dir = is_directory;

	/* Create menu as defined in filebrowser_menu_entries[] */
	menumaker = gtk_item_factory_new(GTK_TYPE_MENU, "<Filebrowser>", NULL);
#ifdef ENABLE_NLS
	gtk_item_factory_set_translate_func(menumaker,menu_translate,"<Filebrowser>",NULL);
#endif
	if (is_directory) {
		gtk_item_factory_create_items(menumaker, sizeof(filebrowser_dirmenu_entries)/sizeof(GtkItemFactoryEntry), filebrowser_dirmenu_entries, filebrowser);
	} else {
		gtk_item_factory_create_items(menumaker, sizeof(filebrowser_filemenu_entries)/sizeof(GtkItemFactoryEntry), filebrowser_filemenu_entries, filebrowser);
	}
	gtk_item_factory_create_items(menumaker, sizeof(filebrowser_bothmenu_entries)/sizeof(GtkItemFactoryEntry), filebrowser_bothmenu_entries, filebrowser);
	menu = gtk_item_factory_get_widget(menumaker, "<Filebrowser>");
	/* set toggle options */
	menu_item = gtk_check_menu_item_new_with_label(_("Show hidden files"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), main_v->props.filebrowser_show_hidden_files);
	g_signal_connect(GTK_OBJECT(menu_item), "toggled", G_CALLBACK(filebrowser_rpopup_shf_toggled_lcb), filebrowser);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	
	menu_item = gtk_check_menu_item_new_with_label(_("Show backup files"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), main_v->props.filebrowser_show_backup_files);
	g_signal_connect(GTK_OBJECT(menu_item), "toggled", G_CALLBACK(filebrowser_rpopup_sbf_toggled_lcb), filebrowser);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	
	/* Add filter submenu */
	menu_item = gtk_menu_item_new_with_label(_("Filter"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	{
		GtkWidget *fmenu;
		GSList *group=NULL;
		GList *tmplist;
		fmenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), fmenu);
		tmplist = g_list_first(FILEBROWSERCONFIG(main_v->filebrowserconfig)->filters);
		while (tmplist) {
			Tfilter *filter = (Tfilter *)tmplist->data;
			menu_item = gtk_radio_menu_item_new_with_label(group, filter->name);
			if (filebrowser->curfilter == filter) {
				gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), TRUE);
			}
			g_signal_connect(GTK_OBJECT(menu_item), "toggled", G_CALLBACK(filebrowser_rpopup_filter_toggled_lcb), filebrowser);
			if (!group) {
				group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item));
			}
			gtk_menu_shell_append(GTK_MENU_SHELL(fmenu), menu_item);
			tmplist = g_list_next(tmplist);
		}
	}
	gtk_widget_show_all(menu);
	g_signal_connect_after(G_OBJECT(menu), "destroy", G_CALLBACK(destroy_disposable_menu_cb), menu);
	return menu;
}

static gboolean filebrowser_button_press_lcb(GtkWidget *widget, GdkEventButton *event, Tfilebrowser *filebrowser) {
	DEBUG_MSG("filebrowser_button_press_lcb, button=%d, store2=%p\n",event->button, filebrowser->store2);
	if (event->button == 3) {
		GtkWidget *menu = NULL;;
		if (filebrowser->store2) {
			menu = filebrowser_rpopup_create_menu(filebrowser, TRUE);
		} else {
			GtkTreePath *path;
			GtkTreeIter iter;
			gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(filebrowser->tree), event->x, event->y, &path, NULL, NULL, NULL);
			if (path) {
				gtk_tree_model_get_iter(GTK_TREE_MODEL(filebrowser->store),&iter,path);
				gtk_tree_path_free(path);
				if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(filebrowser->store),&iter)) {
					/* it is a directory */
					menu = filebrowser_rpopup_create_menu(filebrowser, TRUE);
				} else {
					/* it is a file */
					menu = filebrowser_rpopup_create_menu(filebrowser, FALSE);
				}
			}
		}
		if (menu) gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
	}
	if (event->button==1) {
		GtkTreeIter iter;
		GtkTreePath *path;
		/* path = filebrowser_get_path_from_selection(GTK_TREE_MODEL(filebrowser->store), GTK_TREE_VIEW(filebrowser->tree),NULL);*/
		gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(filebrowser->tree), event->x, event->y, &path, NULL, NULL, NULL);
		DEBUG_MSG("filebrowser_button_press_lcb, path for position x=%f,y=%f is %p\n",event->x, event->y, path);
		if (path) {
			/* now we have two things: for the two paned, we know it is a directory, so we should refresh it, for the 
			one paned we need to know if it is a directory or a file and handle appropriately */
			if (filebrowser->store2) {
				gchar *tmp, *dir;
				tmp = return_filename_from_path(filebrowser,GTK_TREE_MODEL(filebrowser->store),path);
				dir = ending_slash(tmp);
				DEBUG_MSG("two paned view, refresh the directory %s\n", dir);
				refresh_dir_by_path_and_filename(filebrowser, path, dir);
				/* filebrowser_refresh_dir(filebrowser,dir); */
				g_free(tmp);
				g_free(dir);
			} else {
				DEBUG_MSG("one paned view (store2=%p), check if it is a file or directory\n", filebrowser->store2);
				gtk_tree_model_get_iter(GTK_TREE_MODEL(filebrowser->store),&iter,path);
				if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(filebrowser->store),&iter)) {
					DEBUG_MSG("filebrowser_button_press_lcb, clicked a directory, refresh!\n");
					if (!filebrowser->store2 && !gtk_tree_view_row_expanded(GTK_TREE_VIEW(filebrowser->tree), path)) {
						DEBUG_MSG("filebrowser_button_press_lcb, was not yet explanded, in single paned view, refresh makes no sense\n");
					} else {
						gchar *tmp, *dir;
						tmp = return_filename_from_path(filebrowser,GTK_TREE_MODEL(filebrowser->store),path);
						dir = ending_slash(tmp);
						DEBUG_MSG("filebrowser_button_press_lcb, ending_slash returned %s\n", dir);
						refresh_dir_by_path_and_filename(filebrowser, path, dir);
						/* filebrowser_refresh_dir(filebrowser,dir);*/
						g_free(tmp);
						g_free(dir);
					}
				} else if (event->type == GDK_2BUTTON_PRESS) {
					GdkRectangle r;
					gchar *filename = return_filename_from_path(filebrowser,GTK_TREE_MODEL(filebrowser->store),path);
					DEBUG_MSG("filebrowser_button_press_lcb, doubleclick!! open the file %s\n", filename);
					gtk_tree_view_get_visible_rect(GTK_TREE_VIEW(filebrowser->tree),&r);
					handle_activate_on_file(filebrowser,filename);
					g_free(filename);
					gtk_tree_view_scroll_to_point(GTK_TREE_VIEW(filebrowser->tree),-1,r.y);
					gtk_tree_path_free (path);
					return TRUE;
				}
			}
			gtk_tree_path_free(path);
		}
	}
	return FALSE; /* pass the event on */
}

static gboolean filebrowser_tree2_button_press_lcb(GtkWidget *widget, GdkEventButton *event, Tfilebrowser *filebrowser) {
	GtkTreePath *path2;
	DEBUG_MSG("filebrowser_tree2_button_press_lcb, button=%d\n",event->button);
	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(filebrowser->tree2), event->x, event->y, &path2, NULL, NULL, NULL);
	/* path2 = filebrowser_get_path_from_selection(GTK_TREE_MODEL(filebrowser->store2), GTK_TREE_VIEW(filebrowser->tree2),NULL); */
	if (event->button==1 && event->type == GDK_2BUTTON_PRESS && path2) {
		gchar *tmp1, *tmp3;
		GtkTreeIter iter;
		gtk_tree_model_get_iter(GTK_TREE_MODEL(filebrowser->store2),&iter,path2);
		gtk_tree_model_get(GTK_TREE_MODEL(filebrowser->store2), &iter, FILENAME_COLUMN, &tmp1, -1);
		DEBUG_MSG("filebrowser_tree2_button_press_lcb, last_opened=%s,tmp1=%s\n",filebrowser->last_opened_dir,tmp1);
		tmp3 = g_strconcat(filebrowser->last_opened_dir,tmp1,NULL);
		handle_activate_on_file(filebrowser,tmp3);
		g_free(tmp1);
		g_free(tmp3);
		gtk_tree_path_free(path2);
		return TRUE;
	} else if (event->button == 3) {
		GtkWidget *menu;
		/* hmm, just after the very first click, there is no selection yet, because this handler 
		is called *before* the gtk handler crates the selection... */
		if (path2) {
			menu = filebrowser_rpopup_create_menu(filebrowser, FALSE);
		} else {
			menu = filebrowser_rpopup_create_menu(filebrowser, TRUE);
		}
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
	}
	if (path2) gtk_tree_path_free(path2);
	return FALSE; /* pass the event on */
}

static Tfilter *new_filter(gchar *name, gchar *mode, gchar *filetypes) {
	Tfilter *filter = g_new(Tfilter,1);
	filter->name = g_strdup(name);
	filter->mode = atoi(mode);
	filter->filetypes = NULL;
	if (filetypes){
		GList *tmplist;
		gchar **types = g_strsplit(filetypes, ":", 127);
		
		tmplist = g_list_first(main_v->filetypelist);
		while (tmplist) {
			Tfiletype *filetype = (Tfiletype *)tmplist->data;
			gchar **type = types;
			while (*type) {
				if (strcmp(*type, filetype->type)==0) {
					filter->filetypes = g_list_append(filter->filetypes, filetype);
					break;
				}
				type++;
			}
			tmplist = g_list_next(tmplist);
		}
		g_strfreev(types);
	}
	return filter;
}

static void filter_destroy(Tfilter *filter) {
	g_free(filter->name);
	g_list_free(filter->filetypes);
	g_free(filter);
}

/**
 * filebrowser_filters_rebuild:
 *	
 * Reinitializes the filebrowser filters.
 **/
void filebrowser_filters_rebuild() {
	GList *tmplist;

	if (!FILEBROWSERCONFIG(main_v->filebrowserconfig)->dir_icon || !FILEBROWSERCONFIG(main_v->filebrowserconfig)->unknown_icon) {
		g_print("the dir_icon and unknown_icon items in the config file are invalid\n");
/*		return gtk_label_new("cannot load icons");*/
	}

	tmplist = g_list_first(FILEBROWSERCONFIG(main_v->filebrowserconfig)->filters);
	while (tmplist) {
		filter_destroy(tmplist->data);
		tmplist = g_list_next(tmplist);
	}
	g_list_free(FILEBROWSERCONFIG(main_v->filebrowserconfig)->filters);
	FILEBROWSERCONFIG(main_v->filebrowserconfig)->filters = NULL;

	FILEBROWSERCONFIG(main_v->filebrowserconfig)->filetypes_with_icon = NULL;
	tmplist = g_list_first(main_v->filetypelist);
	while (tmplist) {
		if (((Tfiletype *)tmplist->data)->icon) {
			FILEBROWSERCONFIG(main_v->filebrowserconfig)->filetypes_with_icon = g_list_append(FILEBROWSERCONFIG(main_v->filebrowserconfig)->filetypes_with_icon, tmplist->data);
		}
		tmplist = g_list_next(tmplist);
	}
	
	FILEBROWSERCONFIG(main_v->filebrowserconfig)->filters = g_list_append(NULL, new_filter(_("All files"), "0", NULL));
	
	tmplist = g_list_first(main_v->props.filefilters);
	while (tmplist) {
		gchar **strarr = (gchar **) tmplist->data;
		if (count_array(strarr) == 3) {
			Tfilter *filter = new_filter(strarr[0], strarr[1], strarr[2]);
			FILEBROWSERCONFIG(main_v->filebrowserconfig)->filters = g_list_append(FILEBROWSERCONFIG(main_v->filebrowserconfig)->filters, filter);
		}
		tmplist = g_list_next(tmplist);
	}
}

static void showfulltree_toggled_lcb(GtkToggleButton *togglebutton, Tbfwin *bfwin) {
	GtkTreePath *path;
	gchar *to_open;

	/* now rebuild the tree */
	DEBUG_MSG("showfulltree_toggled_lcb, clearing the tree\n");
	gtk_tree_store_clear(FILEBROWSER(bfwin->filebrowser)->store);
	if (FILEBROWSER(bfwin->filebrowser)->store2) {
		gtk_list_store_clear(FILEBROWSER(bfwin->filebrowser)->store2);
	}
	if (togglebutton->active) {
		to_open = FILEBROWSER(bfwin->filebrowser)->last_opened_dir ? FILEBROWSER(bfwin->filebrowser)->last_opened_dir : "/";
	} else {
		g_assert(FILEBROWSER(bfwin->filebrowser)->basedir);
		to_open = FILEBROWSER(bfwin->filebrowser)->basedir;
	}
	DEBUG_MSG("showfulltree_toggled_lcb, build tree for %s\n", to_open);
	path = build_tree_from_path(FILEBROWSER(bfwin->filebrowser), to_open);
	if (path) {
		filebrowser_expand_to_root(FILEBROWSER(bfwin->filebrowser),path);
		gtk_tree_path_free(path);
	}
}

/**
 * filebrowser_set_basedir:
 * @bfwin: #Tbfwin*
 * @basedir: #const gchar* 
 *
 * will set this dir as the basedir for the filebrowser
 * a call with NULL is basically the same as setting it to the default value
 *
 * it there is no slash / appended, this function will add a slash to the end dir
 *
 * Return value: #void
 **/
void filebrowser_set_basedir(Tbfwin *bfwin, const gchar *basedir) {
	if (bfwin->filebrowser) {
		gchar *newbasedir = NULL;
		if (basedir && strlen(basedir)>2) {
			newbasedir = ending_slash(basedir);
		} else if (main_v->props.default_basedir && strlen(main_v->props.default_basedir)>2) {
			newbasedir = ending_slash(main_v->props.default_basedir);
		}

		if (newbasedir && FILEBROWSER(bfwin->filebrowser)->basedir && strcmp(newbasedir, FILEBROWSER(bfwin->filebrowser)->basedir)==0) {
			/* if the old and new basedir are the same: return; */
			g_free(newbasedir);
			return;
		}
		if (FILEBROWSER(bfwin->filebrowser)->basedir) g_free(FILEBROWSER(bfwin->filebrowser)->basedir);
		FILEBROWSER(bfwin->filebrowser)->basedir = newbasedir;
		gtk_widget_set_sensitive(GTK_WIDGET(FILEBROWSER(bfwin->filebrowser)->showfulltree), (newbasedir != NULL));
		if (newbasedir) {
			DEBUG_MSG("filebrowser_set_basedir, set basedir to %s\n",FILEBROWSER(bfwin->filebrowser)->basedir);
			if (!GTK_TOGGLE_BUTTON(FILEBROWSER(bfwin->filebrowser)->showfulltree)->active) {
				/* it is already inactive, call the callback directly */
				DEBUG_MSG("filebrowser_set_basedir, calling showfulltree_toggled_lcb()\n");
				showfulltree_toggled_lcb(GTK_TOGGLE_BUTTON(FILEBROWSER(bfwin->filebrowser)->showfulltree), bfwin);
			} else {
				DEBUG_MSG("filebrowser_set_basedir, calling gtk_toggle_button_set_active()\n");
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FILEBROWSER(bfwin->filebrowser)->showfulltree), FALSE);
			}
		}
	}
}

static void filebrowser_two_pane_notify_position_lcb(GObject *object,GParamSpec *pspec,gpointer data){
	gint position;
	g_object_get(object, pspec->name, &position, NULL);
	if (main_v->props.view_bars & MODE_RESTORE_DIMENSION) {
		main_v->globses.two_pane_filebrowser_height = position;
	}
}

static void dirmenu_activate_lcb(GtkMenuItem *widget, Tfilebrowser *filebrowser) {
	const gchar *dir = gtk_label_get_text(GTK_LABEL(GTK_BIN(widget)->child));
	if (dir) {
		DEBUG_MSG("dirmenu_activate_lcb, dir=%s\n", dir);
		filebrowser_open_dir(filebrowser->bfwin,dir);
	}
}

static void populate_dir_history(Tfilebrowser *filebrowser,gboolean firsttime, gchar *activedir) {
	GtkWidget *menu;
	gchar *tmpchar, *tmpdir;
	GList *tmplist;
	GList *new_history_list=NULL;
/*	if (!firsttime) { */
		DEBUG_MSG("populate_dir_history,remove menu, free stringlist etc.\n ");
		gtk_option_menu_remove_menu(GTK_OPTION_MENU(filebrowser->dirmenu));
		free_stringlist(filebrowser->dirmenu_entries);
		filebrowser->dirmenu_entries = NULL;
	
		menu = gtk_menu_new();
		gtk_option_menu_set_menu(GTK_OPTION_MENU(filebrowser->dirmenu), menu);
		gtk_widget_show(menu);
/*	} else {
		DEBUG_MSG("populate_dir_history, first time, get the menu\n");
		filebrowser->dirmenu_entries = NULL;
		menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(filebrowser->dirmenu));
		gtk_widget_show(menu);
	}*/

	if (activedir) {
		gint rootlen=0;
		gchar *root;
		DEBUG_MSG("populate_dir_history, activedir=%s\n",activedir);
		tmpdir = ending_slash(activedir);
		
		new_history_list = add_to_stringlist(new_history_list, tmpdir);
		root = return_root_with_protocol(tmpdir);
		if (root) {
			rootlen = strlen(root)-1;
			g_free(root);
		}
		while ((tmpchar = strrchr(tmpdir, DIRCHR)) && (tmpchar - tmpdir) >= rootlen) {
			tmpchar++;
			*tmpchar = '\0';
			DEBUG_MSG("populate_dir_history, adding %s\n", tmpdir);
			new_history_list = add_to_stringlist(new_history_list, tmpdir);
			tmpchar--;
			*tmpchar = '\0';
		}
		g_free(tmpdir);
	}
	tmpdir = g_strconcat(g_get_home_dir(), DIRSTR, NULL);
	new_history_list = add_to_stringlist(new_history_list, tmpdir);
	g_free(tmpdir);

	tmplist = g_list_last(main_v->recent_directories);
	while (tmplist) {
		new_history_list = add_to_stringlist(new_history_list, (gchar *)tmplist->data);
		tmplist = g_list_previous(tmplist);
	}

	/* make the actual option menu */
	tmplist = g_list_first(new_history_list);
	while (tmplist) {
		GtkWidget *menuitem;
		gchar *dirname = ending_slash((gchar *)tmplist->data);
		DEBUG_MSG("populate_dir_history, adding %s to menu\n", dirname);
		menuitem = gtk_menu_item_new_with_label(dirname);
		g_signal_connect(G_OBJECT(menuitem),"activate",G_CALLBACK(dirmenu_activate_lcb),filebrowser);
		g_free(dirname);
		gtk_menu_append(GTK_MENU(menu), menuitem);
		gtk_widget_show(menuitem);
		g_free(tmplist->data);
		tmplist = g_list_next(tmplist);
	} 
	g_list_free(new_history_list);

	gtk_option_menu_set_history(GTK_OPTION_MENU(filebrowser->dirmenu), 0);
}

static void focus_follow_toggled_lcb(GtkToggleButton *togglebutton, Tbfwin *bfwin) {
	main_v->props.filebrowser_focus_follow = togglebutton->active;
}

/**
 * filebrowsel_init:
 * @bfwin: #Tbfwin*
 *
 * Initializer. Currently called from left_panel_build().
 * builds the filebrowser GUI
 *
 * Return value: #void
 **/
GtkWidget *filebrowser_init(Tbfwin *bfwin) {
	Tfilebrowser *filebrowser;
	if (bfwin->filebrowser) {
		filebrowser = (Tfilebrowser *)bfwin->filebrowser;
	} else {
		filebrowser =  g_new0(Tfilebrowser,1);
		bfwin->filebrowser = filebrowser;
		filebrowser->bfwin = bfwin;
		if (bfwin->project && bfwin->project->basedir && strlen(bfwin->project->basedir)>2) {
			filebrowser->basedir = ending_slash(bfwin->project->basedir);
		} else if (main_v->props.default_basedir && strlen(main_v->props.default_basedir)>2) {
			filebrowser->basedir = ending_slash(main_v->props.default_basedir);
		}
		DEBUG_MSG("filebrowser_init, the basedir is set to %s\n",filebrowser->basedir);
	}
	DEBUG_MSG("filebrowser_init, filebrowser is at %p for bfwin %p\n", filebrowser, bfwin);
	if (!filebrowser->curfilter) {
		/* get the default filter */
		filebrowser->curfilter = find_filter_by_name(main_v->props.last_filefilter);
	}
	
	filebrowser->dirmenu = gtk_option_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(filebrowser->dirmenu), gtk_menu_new());
/*	populate_dir_history(filebrowser,TRUE, NULL);*/
	
	filebrowser->store = gtk_tree_store_new (N_COLUMNS,GDK_TYPE_PIXBUF,G_TYPE_STRING);
	filebrowser->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL(filebrowser->store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(filebrowser->tree), FALSE);	
	/*gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(filebrowser->tree), TRUE);*/
	{
		GtkCellRenderer *renderer;
		GtkTreeViewColumn *column;
		renderer = gtk_cell_renderer_pixbuf_new();
		column = gtk_tree_view_column_new ();
		gtk_tree_view_column_pack_start (column, renderer, FALSE);
		
		gtk_tree_view_column_set_attributes(column,renderer
			,"pixbuf",PIXMAP_COLUMN
			,"pixbuf_expander_closed",PIXMAP_COLUMN
			,"pixbuf_expander_open",PIXMAP_COLUMN
			,NULL);
/*		g_object_set(G_OBJECT(renderer), "xpad", 1, NULL);
		g_object_set(G_OBJECT(renderer), "ypad", 1, NULL);
		gtk_tree_view_column_set_spacing(column, 1);
		gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);*/
		gtk_tree_view_append_column (GTK_TREE_VIEW(filebrowser->tree), column);
		
		renderer = gtk_cell_renderer_text_new();
/*		column = gtk_tree_view_column_new ();*/
		g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL); /* Not editable. */
/*		g_object_set(G_OBJECT(renderer), "xpad", 1, NULL);
		g_object_set(G_OBJECT(renderer), "ypad", 1, NULL);*/
/*		g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(renderer_edited_lcb), filebrowser->store);	 */
		gtk_tree_view_column_pack_start (column, renderer, TRUE);
		gtk_tree_view_column_set_attributes(column,renderer
			,"text", FILENAME_COLUMN,NULL);
/*		gtk_tree_view_column_set_spacing(column, 1);
		gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);*/
/*		gtk_tree_view_append_column(GTK_TREE_VIEW(filebrowser->tree), column);*/
	}
	
	if (main_v->props.view_bars & MODE_FILE_BROWSERS_TWO_VIEW) {
		GtkCellRenderer *renderer;
		GtkTreeViewColumn *column;

		filebrowser->store2 = gtk_list_store_new (N_COLUMNS,GDK_TYPE_PIXBUF,G_TYPE_STRING);
		filebrowser->tree2 = gtk_tree_view_new_with_model(GTK_TREE_MODEL(filebrowser->store2));
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(filebrowser->tree2), FALSE);
		/*gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(filebrowser->tree2), TRUE);*/
		renderer = gtk_cell_renderer_pixbuf_new();
		column = gtk_tree_view_column_new ();
		gtk_tree_view_column_pack_start (column, renderer, FALSE);
		gtk_tree_view_column_set_attributes(column,renderer
			,"pixbuf",PIXMAP_COLUMN
			,"pixbuf_expander_closed",PIXMAP_COLUMN
			,"pixbuf_expander_open",PIXMAP_COLUMN
			,NULL);
		renderer = gtk_cell_renderer_text_new();
		g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL); /* Not editable. */
		gtk_tree_view_column_pack_start(column, renderer, TRUE);
		gtk_tree_view_column_set_attributes(column,renderer,"text", FILENAME_COLUMN,NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW(filebrowser->tree2), column);
	} else {
		filebrowser->store2 = NULL;
		filebrowser->tree2 = NULL;
	}
	
	{
		GtkTreePath *path = NULL;
		gchar *buildfrom = NULL;
		
		filebrowser->showfulltree = checkbut_with_value(_("Show full tree"), FALSE);
		filebrowser->focus_follow = checkbut_with_value(_("Follow focus"), main_v->props.filebrowser_focus_follow);
		if (bfwin->project && bfwin->project->basedir && strlen(bfwin->project->basedir)>2) {
			buildfrom = bfwin->project->basedir;
		} else {
			if (bfwin->current_document && bfwin->current_document->filename){
				DEBUG_MSG("filebrowser_init, build tree from current doc %s\n",bfwin->current_document->filename);
				buildfrom = bfwin->current_document->filename; 
			} else if (filebrowser->basedir && strlen(filebrowser->basedir)>2) {
				buildfrom = filebrowser->basedir;
			} else {
				gchar curdir[1024];
				getcwd(curdir, 1023);
				strncat(curdir, "/", 1023);
				DEBUG_MSG("filebrowser_init, build tree from curdir=%s\n",curdir);
				buildfrom = curdir;
			}
		}
		path = build_tree_from_path(filebrowser, buildfrom);
		if (filebrowser->last_opened_dir) g_free(filebrowser->last_opened_dir);
		filebrowser->last_opened_dir = ending_slash(buildfrom);
		populate_dir_history(filebrowser,FALSE, filebrowser->last_opened_dir);
		if (path) {
			filebrowser_expand_to_root(filebrowser,path);
			gtk_tree_path_free(path);
		}
	}
	g_signal_connect(G_OBJECT(filebrowser->tree), "row-expanded", G_CALLBACK(row_expanded_lcb), filebrowser);
	g_signal_connect(G_OBJECT(filebrowser->tree), "row-activated",G_CALLBACK(row_activated_lcb),filebrowser);
	g_signal_connect(G_OBJECT(filebrowser->tree), "button_press_event",G_CALLBACK(filebrowser_button_press_lcb),filebrowser);
	if (filebrowser->store2) {
		g_signal_connect(G_OBJECT(filebrowser->tree2), "button_press_event",G_CALLBACK(filebrowser_tree2_button_press_lcb),filebrowser);
	}

	{
		GtkWidget *vbox, *scrolwin;
	/*	GtkAdjustment* adj;*/
		vbox = gtk_vbox_new(FALSE, 0);

		gtk_box_pack_start(GTK_BOX(vbox),filebrowser->dirmenu, FALSE, FALSE, 0);
		
		scrolwin = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_container_add(GTK_CONTAINER(scrolwin), filebrowser->tree);
		if (filebrowser->store2) {
			GtkWidget *vpaned, *scrolwin2;
			vpaned = gtk_vpaned_new();
			gtk_paned_add1(GTK_PANED(vpaned), scrolwin);
			scrolwin2 = gtk_scrolled_window_new(NULL, NULL);
			gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin2), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
			gtk_container_add(GTK_CONTAINER(scrolwin2), filebrowser->tree2);
			gtk_paned_add2(GTK_PANED(vpaned), scrolwin2);
			gtk_widget_set_size_request(vpaned, main_v->props.left_panel_width, -1);
			gtk_box_pack_start(GTK_BOX(vbox), vpaned, TRUE, TRUE, 0);
			gtk_paned_set_position(GTK_PANED(vpaned), main_v->globses.two_pane_filebrowser_height);
			g_signal_connect(G_OBJECT(vpaned),"notify::position",G_CALLBACK(filebrowser_two_pane_notify_position_lcb), NULL);
		} else {
			gtk_widget_set_size_request(scrolwin, main_v->props.left_panel_width, -1);
			gtk_box_pack_start(GTK_BOX(vbox), scrolwin, TRUE, TRUE, 0);
		}
/*		adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrolwin));
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj), GTK_ADJUSTMENT(adj)->lower);
		adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolwin));
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj), GTK_ADJUSTMENT(adj)->lower);*/
		
		gtk_box_pack_start(GTK_BOX(vbox), filebrowser->showfulltree, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), filebrowser->focus_follow, FALSE, FALSE, 0);
		g_signal_connect(G_OBJECT(filebrowser->showfulltree), "toggled", G_CALLBACK(showfulltree_toggled_lcb), bfwin);
		g_signal_connect(G_OBJECT(filebrowser->focus_follow), "toggled", G_CALLBACK(focus_follow_toggled_lcb), bfwin);
		if (filebrowser->basedir == NULL) {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(filebrowser->showfulltree), TRUE);
			gtk_widget_set_sensitive(GTK_WIDGET(filebrowser->showfulltree), FALSE);
		}
		return vbox;
	}
}

void filebrowser_scroll_initial(Tbfwin *bfwin) {
	DEBUG_MSG("filebrowser_scroll_initial, bfwin=%p, filebrowser=%p\n",bfwin,bfwin->filebrowser);
	if (FILEBROWSER(bfwin->filebrowser) && FILEBROWSER(bfwin->filebrowser)->tree) {
		gtk_tree_view_scroll_to_point(GTK_TREE_VIEW(FILEBROWSER(bfwin->filebrowser)->tree), 2000, 2000);
	}
}

/**
 * filebrowser_cleanup:
 * @bfwin: #Tbfwin*
 *
 * this function is called when the filebrowser is hidden, all memory can be 
 * free-ed here, and everything set to zero
 *
 * Return value: void
 */

void filebrowser_cleanup(Tbfwin *bfwin) {
	/* is this cleanup complete ? I wonder... we need some memleak detection here.. */
	if (bfwin->filebrowser) {
		DEBUG_MSG("filebrowser_cleanup, cleanup store\n");
		gtk_tree_store_clear(GTK_TREE_STORE(FILEBROWSER(bfwin->filebrowser)->store));
		g_object_unref(G_OBJECT(FILEBROWSER(bfwin->filebrowser)->store));
		if (FILEBROWSER(bfwin->filebrowser)->store2) {
			DEBUG_MSG("filebrowser_cleanup, cleanup store2\n");
			gtk_list_store_clear(GTK_LIST_STORE(FILEBROWSER(bfwin->filebrowser)->store2));
			g_object_unref(G_OBJECT(FILEBROWSER(bfwin->filebrowser)->store2));
		}
		if (FILEBROWSER(bfwin->filebrowser)->basedir)
			g_free(FILEBROWSER(bfwin->filebrowser)->basedir);
		if (FILEBROWSER(bfwin->filebrowser)->last_opened_dir) 
			g_free(FILEBROWSER(bfwin->filebrowser)->last_opened_dir);
		
		DEBUG_MSG("filebrowser_cleanup, free filebrowser\n");
		g_free(bfwin->filebrowser);
		bfwin->filebrowser = NULL;
	}
}

/* this function is omly called once for every bluefish process */
void filebrowserconfig_init() {
	gchar *filename;
	Tfilebrowserconfig *fc = g_new0(Tfilebrowserconfig,1);
	main_v->filebrowserconfig = fc;

	filename = return_first_existing_filename(main_v->props.filebrowser_unknown_icon,
					"icon_unknown.png","../icons/icon_unknown.png",
					"icons/icon_unknown.png",NULL);
		
	fc->unknown_icon = gdk_pixbuf_new_from_file(filename, NULL);
	g_free(filename);
		
	filename = return_first_existing_filename(main_v->props.filebrowser_dir_icon,
					"icon_dir.png","../icons/icon_dir.png",
					"icons/icon_dir.png",NULL);

	fc->dir_icon = gdk_pixbuf_new_from_file(filename, NULL);
	g_free(filename);
}

