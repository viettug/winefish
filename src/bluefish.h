/* $Id$ */

/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * bluefish.h - global prototypes
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
 
/* if you define DEBUG here you will get debug output from all Bluefish parts */
/* #define DEBUG */

#ifndef __BLUEFISH_H_
#define __BLUEFISH_H_

#include "config.h"
#define WINEFISH_SPLASH_FILENAME PKGDATADIR"winefish_splash.png"

#ifdef HAVE_SYS_MSG_H
#define WITH_MSG_QUEUE
#endif

#ifdef DEBUG
#define DEBUG_MSG g_print
#else /* not DEBUG */
#ifdef __GNUC__
#define DEBUG_MSG(args...)
 /**/
#else/* notdef __GNUC__ */
extern void g_none(gchar *first, ...);
#define DEBUG_MSG g_none
#endif /* __GNUC__ */
#endif /* DEBUG */

#ifdef ENABLE_NLS

#include <libintl.h>
#define _(String) gettext (String)
#define N_(String) (String)

#else

#define _(String)(String)
#define N_(String)(String)

#endif


#define DIRSTR "/"
#define DIRCHR '/'

#include <sys/types.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pcre.h>

/*********************/
/* undo/redo structs */
/*********************/
typedef enum {
	UndoDelete = 1, UndoInsert
} undo_op_t;

typedef struct {
	GList *entries;	/* the list of entries that should be undone in one action */
	gint changed;		/* doc changed status at this undo node */
} unregroup_t;

typedef struct {
	GList *first;
	GList *last;
	unregroup_t *current;
	gint num_groups;
	GList *redofirst;
} unre_t;

/************************/
/* filetype struct      */
/************************/

typedef struct {
	gchar *type;
	gchar **extensions;
	GdkPixbuf *icon;
	gchar *update_chars;
	GList *highlightlist;
	gboolean editable; /* this a type that can be edited by Bluefish */
	gint autoclosingtag; /* 0=off, 1=xml mode, 2=html mode */
	gchar *content_regex; /* a regex pattern to test the filetype using the content */
} Tfiletype;

/* brace_finder */
typedef struct {
	GtkTextMark *mark_left;
	GtkTextMark *mark_mid;
	GtkTextMark *mark_right;
	GtkTextTag *tag;
	guint16 last_status;
} Tbracefinder;
#define BRACEFINDER(var)((Tbracefinder *)(var))

/*******************/
/* document struct */
/*******************/
#define BFWIN(var) ((Tbfwin *)(var))
#define DOCUMENT(var) ((Tdocument *)(var))

typedef struct {
	gchar *filename; /* this is the UTF-8 encoded filename, before you use it on disk you need convert to disk-encoding! */
	gchar *encoding;
	gint modified;
/*	time_t mtime; */ /* from stat() */
	struct stat statbuf;
	gint is_symlink; /* file is a symbolic link */
	gulong del_txt_id; /* text delete signal */
	gulong ins_txt_id; /* text insert signal */
	gulong ins_aft_txt_id; /* text insert-after signal, for auto-indenting */
	unre_t unre;
	GtkWidget *view;
	GtkWidget *tab_label;
	GtkWidget *tab_eventbox;
	GtkWidget *tab_menu;
	GtkTextBuffer *buffer;
	gpointer paste_operation;
	gint last_rbutton_event; /* index of last 3rd button click */
	Tfiletype *hl; /* filetype & highlighting set to use for this document */
	gint need_highlighting; /* if you open 10+ documents you don't need immediate highlighting, just set this var, and notebook_switch() will trigger the actual highlighting when needed */
	guint32 view_bars;
	gpointer floatingview; /* a 2nd textview widget that has its own window */
	gpointer bfwin;
	GtkTreeIter *bmark_parent; /* if NULL this document doesn't have bookmarks, if 
		it does have bookmarks they are children of this GtkTreeIter */
	gpointer brace_finder;
} Tdocument;

typedef struct {
	gint filebrowser_show_hidden_files;
	gint filebrowser_show_backup_files;
	/* gint filebrowser_two_pane_view; *//* have one or two panes in the filebrowser */
	gint filebrowser_focus_follow; /* have the directory of the current document in focus */
	gchar *filebrowser_unknown_icon;
	gchar *filebrowser_dir_icon;
	gchar *editor_font_string;		/* editor font */
	gint editor_tab_width;	/* editor tabwidth */
	/* gint editor_indent_wspaces; *//* indent with spaces, not tabs */
	gchar *tab_font_string;		/* notebook tabs font */
	GList *browsers; /* browsers array */
	GList *external_commands;	/* external commands array */
	GList *cust_menu; 		/* DEPRECATED entries in the custom menu */
	GList *cmenu_insert; /* custom menu inserts */
	GList *cmenu_replace; /* custom menu replaces */
	gint highlight_num_lines_count; /* number of lines to highlight in continous highlighting */	
	GList *filetypes; /* filetypes for highlighting and filtering */
	gint numcharsforfiletype; /* maximum number of characters in the file to use to find the filetype */
	GList *filefilters; /* filebrowser.c filtering */
	gchar *last_filefilter;	/* last filelist filter type */
	GList *highlight_patterns; /* the highlight patterns */
	gint left_panel_width; 	/* width of filelist */
	gint left_panel_left; /* 1 = left, 0 = right */
	gint max_recent_files;	/* length of Open Recent list */
	gint max_dir_history;	/* length of directory history */
	gchar *backup_filestring;  /* the string to append to the backup file */
	gint backup_abort_action;/* if the backup fails, continue save, abort save, or ask the user */
	gint modified_check_type; /* 0=no check, 1=by mtime and size, 2=by mtime, 3=by size, 4,5,...not implemented (md5sum?) */
	gint num_undo_levels; 	/* number of undo levels per document */
	gchar *newfile_default_encoding; /* if you open a new file, what encoding will it use */
	GList *encodings; /* all encodings you can choose from */
	GList *outputbox; /* all outputbox commands */
	GList *reference_files; /* all reference files */
	gint bookmarks_default_store; /* 0= temporary by default, 1= permanent by default */
	gint bookmarks_filename_mode; /* 0=FULLPATH, 1=DIR FROM BASE 2=BASENAME */
	gint document_tabposition;
	gint leftpanel_tabposition;
	gchar *default_basedir;
	gchar *project_suffix;
#ifdef HAVE_LIBASPELL
	gchar *spell_default_lang;
#endif /* HAVE_LIBASPELL */
	GCompletion *completion;
	GCompletion *completion_s;
	GHashTable *autotext_hashtable; /* a hash table with (key,form) = (string,integer) */
	GPtrArray *autotext_array; /* an array contains (start string, end string) */
	guint32 view_bars;
#ifdef EXTERNAL_GREP
#ifdef EXTERNAL_FIND
	gchar *templates_dir;
#endif /* EXTERNAL_FIND */
#endif /* EXTERNAL_GREP */
#ifdef ENABLE_COLUMN_MARKER
	gint marker_i;
	gint marker_ii;
	gint marker_iii;
#endif /* ENABLE_COLUMN_MARKER */
} Tproperties;

/* the Tglobalsession contains all settings that can change 
over every time you run Winefish, so things that *need* to be
saved after every run! */
typedef struct {
	GList *quickbar_items; /* items in the quickbar toolbar */	
	gint main_window_h;			/* main window height */
	gint main_window_w;			/* main window width */
	gint two_pane_filebrowser_height; /* position of the pane separater on the two paned file browser */
	gint fref_ldoubleclick_action; /* left doubleclick in the function reference */
	gint fref_info_type; /* type of info shown in a small function reference window */
	gint lasttime_cust_menu; /* the last time the defaultfile was checked for new entries */
	gint lasttime_highlighting; /* see above */
	gint lasttime_filetypes; /* see above */
	gint lasttime_encodings; /* see above */
	GList *recent_projects;
} Tglobalsession;

typedef struct {
	GList *classlist;
	GList *colorlist;
	GList *targetlist;
	/* GList *urllist; */ /* kyanh, removed, 20050309 */
	GList *fontlist;
	/* GList *dtd_cblist; */ /* is this used ?? */
	/* GList *headerlist; */ /* is this used ?? */
	/* GList *positionlist; */ /* is this used ?? */
	GList *searchlist; /* used in snr2 */
	GList *replacelist; /* used in snr2 */
	GList *bmarks;
	GList *recent_files;
	GList *recent_dirs;
	gchar *opendir;
	gchar *savedir;
	guint32 view_bars;
} Tsessionvars;

typedef struct {
	gchar *filename;
	gchar *name;
	GList *files;
	gchar *basedir;
	gchar *basefile;
	gchar *template;
	gpointer editor;
	guint32 view_bars;
	gint word_wrap;
	Tsessionvars *session;
	GtkTreeStore *bookmarkstore; /* project bookmarks */
} Tproject;

typedef struct {
	Tsessionvars *session; /* points to the global session, or to the project session */
	Tdocument *current_document; /* one object out of the documentlist, the current visible document */
	GList *documentlist; /* document.c and others: all Tdocument objects */
	Tdocument *last_activated_doc;
	Tproject *project; /* might be NULL for a default project */
	GtkWidget *main_window;
	GtkWidget *menubar;
	gint last_notebook_page; /* a check to see if the notebook changed to a new page */
	gulong notebook_switch_signal;
	GtkWidget *notebook;
	GtkWidget *notebook_fake;
	GtkWidget *notebook_box; /* Container for notebook and notebook_fake */
	GtkWidget *middlebox; /* we need this to show/hide the filebrowser */
	GtkWidget *hpane; /* we need this to show/hide the filebrowser */
	GtkWidget *statusbar;
	GtkWidget *statusbar_lncol; /* where we have the line number */
	GtkWidget *statusbar_insovr; /* insert/overwrite indicator */
	GtkWidget *statusbar_editmode; /* editor mode and doc encoding */
	/* the following list contains toolbar widgets we like to reference later on */
	/* GtkWidget *toolbar_undo; */
	/* GtkWidget *toolbar_redo; */
	/* GtkWidget *toolbar_quickbar; *//* the quickbar widget */
	/*GList *toolbar_quickbar_children;*/ /* this list is needed to remove widgets from the quickbar */
	/* following widgets are used to show/hide stuff */
	/*
	GtkWidget *main_toolbar_hb;
	GtkWidget *html_toolbar_hb;
	*/
	guint32 view_bars;
	GtkWidget *custom_menu_hb; /* handle box for custom menu */
	GtkWidget *output_box;
	GtkWidget *leftpanel_notebook;
	/* following are lists with dynamic menu entries */
	GList *menu_recent_files;
	GList *menu_recent_projects;
	GList *menu_external;
	GList *menu_encodings;
	GList *menu_outputbox;
	GList *menu_windows;
	GtkWidget *menu_cmenu;
	GList *menu_cmenu_entries;
	GList *menu_filetypes;
	GtkWidget *vpane;
	gpointer outputbox; /* a outputbox */
#ifdef EXTERNAL_FIND
#ifdef EXTERNAL_GREP
	gpointer grepbox; /* a grep box */
	gpointer templatebox; /* box for templates */
#endif /* EXTERNAL_FIND */
#endif /* EXTERNAL_GREP */
#ifdef HAVE_VTE_TERMINAL
	GtkWidget *terminal;
#endif /* HAVE_VTE_TERMINAL */
	GtkWidget *ob_notebook; /* notebook of outputboxes */
	GtkWidget *ob_hbox; /* hbox to save all ob_boxes */
	gpointer bfspell;
	gpointer filebrowser;
	gpointer snr2;
	gpointer fref;
	gpointer bmark;
	GtkTreeStore *bookmarkstore; /* this is a link to project->bookmarkstore OR main_v->bookmarkstore
		and it is only here for convenience !!!! */
	GHashTable *bmark_files;     /* no way, I have to have list of file iters. Other way I 
	                                cannot properly load bmarks for closed files */
} Tbfwin;

typedef struct {
	GtkWidget *window; /* popup window */
	GtkWidget *treeview; /* we hold it so we scroll */
	gint show; /* 1: show; 0: hide; */
	gchar *cache; /* temporary buffer */
	gpointer bfwin;
} Tcompletionwin;

typedef struct {
	Tproperties props; /* preferences */
	Tglobalsession globses; /* global session */
	GList *filetypelist; /* highlighting.c: a list of all filetypes with their icons and highlighting sets */
	GList *bfwinlist;
	GList *recent_directories; /* a stringlist with the most recently used directories */
	Tsessionvars *session; /* holds all session variables for non-project windows */
	gpointer filebrowserconfig;
	gpointer frefdata;
	gpointer bmarkdata;
	GtkTreeStore *bookmarkstore; /* the global bookmarks from the global session */
	gint num_untitled_documents;
	GtkTooltips *tooltips;
	GdkEvent *last_kevent;
#ifdef BUG84
	guint16 lastkp_hardware_keycode; /* for the autoclosing, we need to know the last pressed key, in the key release callback, */
	guint lastkp_keyval;             /* this is different if the modifier key is not pressed anymore during the key-release */
#endif
	pcre *autoclosingtag_regc; /* the regular expression to check for a valid tag in tag autoclosing*/
	pcre *autoclosingtag_be_regc; /* for context \start...\stop and plain text \begin...\end */
	pcre *anycommand_regc;

	Tcompletionwin completion; /* a popup window for completion */
#ifdef SNOOPER2
	gpointer snooper;
	GtkAccelGroup *accel_group;
#else
	guint snooper; /* snooper */
#endif /* SNOOPER2 */
} Tmain;

extern Tmain *main_v;

#define SET_BIT(var,bit,value) (var & ~bit)|(value*bit)
#define GET_BIT(var,bit) ((var &bit) != 0)

enum {
	VIEW_LEFT_PANEL = 1<<0,
	VIEW_LINE_NUMBER =1<<1,
	VIEW_CUSTOM_MENU = 1<<2,
	VIEW_COLORIZED = 1<<3,
	MODE_WRAP = 1<<4,
	MODE_PROJECT = 1<<5,
	MODE_OVERWRITE=1<<6,
	MODE_AUTO_COMPLETE=1<<7,
	MODE_INDENT_WITH_SPACES=1<<8,
	MODE_AUTO_INDENT=1<<9,
	MODE_REUSE_WINDOW=1<<10,
	MODE_ALLOW_MULTIPLE_INSTANCE=1<<11,
	MODE_CLEAR_UNDO_HISTORY_ON_SAVE=1<<12,
	MODE_MAKE_PERMANENT=1<<13,
	MODE_CREATE_BACKUP_ON_SAVE=1<<14,
	MODE_REMOVE_BACKUP_ON_CLOSE=1<<15,
	MODE_SEPERATE_FILE_DIR_VIEW=1<<16,
	MODE_RESTORE_DIMENSION=1<<17,
	MODE_MAKE_LATEX_TRANSIENT=1<<18,
	MODE_FILE_BROWSERS_TWO_VIEW=1<<19,
	MODE_AUTO_BRACE_FINDER=1<<20
};

#define MODE_DEFAULT \
	MODE_AUTO_INDENT \
	+ MODE_REUSE_WINDOW \
	+ VIEW_COLORIZED \
	+ MODE_MAKE_LATEX_TRANSIENT \
	+ MODE_RESTORE_DIMENSION \
	+ MODE_CREATE_BACKUP_ON_SAVE \
	+ MODE_FILE_BROWSERS_TWO_VIEW \
	+ MODE_AUTO_COMPLETE

/*
Note: VIEW_LINE_NUMBER is session variable;
adding it to MODE_DEFAULT takes no effect.
*/

/* public functions from winefish.c */
void bluefish_exit_request();
#endif /* __BLUEFISH_H_ */
