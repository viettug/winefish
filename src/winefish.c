/* $Id$ */

/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 *
 * Original File: bluefish.c - the main function
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
/* #define DEBUG */

#include <gtk/gtk.h>
#include <unistd.h> /* getopt() */
#include <stdlib.h> /* getopt() exit() and abort() on Solaris */
#include <time.h> /* nanosleep */
#include <string.h> /* strcmp */

#include "bluefish.h"

#ifdef HAVE_ATLEAST_GNOMEUI_2_6
#include <libgnomeui/libgnomeui.h>
#endif

#ifdef ENABLE_NLS
#include <locale.h>
#endif

#include "document.h" /*  */
#include "gui.h" /* gui_create_main() */
#include "fref.h" /* fref_init() */
#include "bookmark.h"  /* bmark_init() */
#include "rcfile.h" /* rcfile_parse_main() */
#include "bf_lib.h" /* create_full_path() */
#include "highlight.h" /* hl_init() */
#include "msg_queue.h" /* msg_queue_start()*/
#include "stringlist.h" /* put_stringlist(), get_stringlist() */
#include "gtk_easy.h" /* flush_queue() */
#include "filebrowser.h" /* filters_rebuild() */
#include "project.h"
/* kyanh, removed, 20050309 */
/*#include "authen.h" *//* set_authen_callbacks() */
#include "autox.h" /* kyanh, completion */
#include "snooper.h" /* snooper_install() */

/*********************************************/
/* this var is global for all bluefish files */
/*********************************************/
Tmain *main_v;

/********************************/
/* functions used in bluefish.c */
/********************************/
#ifndef __GNUC__
void g_none(gchar *first, ...) {
	return;
}
#endif

static gint parse_commandline(int argc, char **argv
		, gboolean *root_override
		, GList **load_filenames
		, GList **load_projects
		, gboolean *open_in_new_win
		, gint *linenumber
			     ) {
	int c;
	gchar *tmpname;

	opterr = 0;
	DEBUG_MSG("parse_commandline, started\n");
	while ((c = getopt(argc, argv, "hsvn:l:p:?")) != -1) {
		switch (c) {
		case 's':
			*root_override = 1;
			break;
		case 'v':
			g_print(CURRENT_VERSION_NAME);
			g_print("\n");
			exit(1);
			break;
		case 'p':
			tmpname = create_full_path(optarg, NULL);
			*load_projects = g_list_append(*load_projects, tmpname);
			break;
		case 'l':
			*linenumber = strtoul(optarg, NULL,10);
			break;
		case 'h':
		case '?':
			g_print(CURRENT_VERSION_NAME);
			g_print(_("\nUsage: %s [options] [filenames ...]\n"), argv[0]);
			g_print(_("\nCurrently accepted options are:\n"));
			g_print(_("-s           skip root check\n"));
			g_print(_("-v           current version\n"));
			g_print(_("-n 0|1       open new window (1) or not (0)\n"));
			g_print(_("-p filename  open project\n"));
			g_print(_("-l number    set line<number>. Negative value takes no effect.\n"));
			g_print(_("-h           this help screen\n"));
			exit(1);
			break;
		case 'n':
			if (strncmp(optarg,"0",1)==0) {
				*open_in_new_win = 0;
			}else{	
				*open_in_new_win = 1;
			}
			break;
		default:
			DEBUG_MSG("parse_commandline, abort ?!?\n");
			abort();
		}
	}
	DEBUG_MSG("parse_commandline, optind=%d, argc=%d\n", optind, argc);
	while (optind < argc) {
		/* related to BUG#93 */
		/*if (file_exists_and_readable(argv[optind])){*/
			tmpname = create_full_path(argv[optind], NULL);
			DEBUG_MSG("parse_commandline, argv[%d]=%s, tmpname=%s\n", optind, argv[optind], tmpname);
			*load_filenames = g_list_append(*load_filenames, tmpname);
		/*}else{
			g_print("winefish: file '%s' is unreadable\n", argv[optind]);
		}*/
		optind++;
	}
	DEBUG_MSG("parse_commandline, finished, num files=%d, num projects=%d\n"
		, g_list_length(*load_filenames), g_list_length(*load_projects));
	return 0;
}


/*********************/
/* the main function */
/*********************/

int main(int argc, char *argv[])
{
	gboolean root_override=FALSE, open_in_new_window = FALSE;
	GList *filenames = NULL, *projectfiles=NULL;
	gint linenumber = -1;

	Tbfwin *firstbfwin;
#ifndef NOSPLASH
	GtkWidget *splash_window;
#endif /* #ifndef NOSPLASH */

#ifdef ENABLE_NLS
	setlocale(LC_ALL,"");
	bindtextdomain(PACKAGE,LOCALEDIR);
	DEBUG_MSG("set bindtextdomain for %s to %s\n",PACKAGE,LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);
#endif
#ifdef HAVE_ATLEAST_GNOMEUI_2_6
	gnome_init(PACKAGE, VERSION, argc, argv);
#else
	gtk_init(&argc, &argv);
#endif /* HAVE_ATLEAST_GNOMEUI_2_6
 */
	main_v = g_new0(Tmain, 1);
	main_v->session = g_new0(Tsessionvars,1);
	DEBUG_MSG("main, main_v is at %p\n", main_v);

	rcfile_check_directory();
	rcfile_parse_main();

	parse_commandline(argc, argv, &root_override, &filenames, &projectfiles, &open_in_new_window, &linenumber);
#ifdef WITH_MSG_QUEUE
	if (((filenames || projectfiles) && (main_v->props.view_bars & MODE_REUSE_WINDOW)) || open_in_new_window) {
		msg_queue_start(filenames, projectfiles, linenumber, open_in_new_window);
	}
#endif /* WITH_MSG_QUEUE */
#ifndef NOSPLASH
	/* start splash screen somewhere here */
	splash_window = start_splash_screen();
	splash_screen_set_label(_("parsing highlighting file..."));
#endif /* #ifndef NOSPLASH */

	{
		gchar *filename = g_strconcat(g_get_home_dir(), "/.winefish/dir_history", NULL);
		main_v->recent_directories = get_stringlist(filename, NULL);
		g_free(filename);
	}
	rcfile_parse_global_session();
	rcfile_parse_highlighting();
#ifndef NOSPLASH
	splash_screen_set_label(_("compiling highlighting patterns..."));
#endif /* #ifndef NOSPLASH */
	hl_init();
#ifndef NOSPLASH
	splash_screen_set_label(_("initialize some other things..."));
#endif /* #ifndef NOSPLASH */
	filebrowserconfig_init();
	filebrowser_filters_rebuild();
	autoclosing_init();
#ifndef NOSPLASH
	splash_screen_set_label(_("parsing autotext and words file..."));
#endif /* #ifndef NOSPLASH */
	autotext_init();
	completion_init();
#ifndef NOSPLASH
	splash_screen_set_label(_("parsing custom menu file..."));
#endif /* #ifndef NOSPLASH */
	rcfile_parse_custom_menu(FALSE,FALSE);
	main_v->tooltips = gtk_tooltips_new();
	/* initialize the completion window */
	main_v->completion.window = NULL;
	fref_init();
	bmark_init();
#ifdef WITH_MSG_QUEUE
	if (!filenames && !projectfiles && (main_v->props.view_bars & MODE_REUSE_WINDOW)) {
		msg_queue_start(NULL, NULL, -1, open_in_new_window);
	}
#endif /* WITH_MSG_QUEUE */
#ifndef NOSPLASH
	splash_screen_set_label(_("creating main gui..."));
#endif /* #ifndef NOSPLASH */

	/* create the first window */
	firstbfwin = g_new0(Tbfwin,1);
	firstbfwin->session = main_v->session;
	firstbfwin->bookmarkstore = main_v->bookmarkstore;
	main_v->bfwinlist = g_list_append(NULL, firstbfwin);
	gui_create_main(firstbfwin, filenames, linenumber);
	bmark_reload(firstbfwin);
#ifndef NOSPLASH
	splash_screen_set_label(_("showing main gui..."));
#endif /* #ifndef NOSPLASH */

	/* set GTK settings, must be AFTER the menu is created */
	{
		gchar *shortcutfilename;
		GtkSettings* gtksettings = gtk_settings_get_default();
		g_object_set(G_OBJECT(gtksettings), "gtk-can-change-accels", TRUE, NULL); 
		shortcutfilename = g_strconcat(g_get_home_dir(), "/.winefish/menudump_2", NULL);
		gtk_accel_map_load(shortcutfilename);
		g_free(shortcutfilename);
	}

	gui_show_main(firstbfwin);
	/*
	if (main_v->props.view_html_toolbar && main_v->globses.quickbar_items == NULL) {
		info_dialog(firstbfwin->main_window, _("Winefish tip:"), _("This message is shown since you DONOT have any items in the QuickBar.\n\nIf you right-click a button in the Standard toolbars you can add buttons to the Quickbar."));
	}
	*/
	if (projectfiles) {
		GList *tmplist = g_list_first(projectfiles);
		while (tmplist) {
			project_open_from_file(firstbfwin, tmplist->data, linenumber);
			tmplist = g_list_next(tmplist);
		}
	}

#ifndef NOSPLASH
	DEBUG_MSG("destroy splash\n");
	flush_queue();
	{
		static struct timespec const req = { 0, 10000000};
		nanosleep(&req, NULL);
	}
	gtk_widget_destroy(splash_window);
#endif /* #ifndef NOSPLASH */

	/* snooper must be installed after the main gui has shown;
	otherwise the program may be aborted */
	snooper_install();

	DEBUG_MSG("main, before gtk_main()\n");
	gtk_main();
	DEBUG_MSG("main, after gtk_main()\n");
#ifdef WITH_MSG_QUEUE	
	/* do the cleanup */
	msg_queue_cleanup();
#endif /* WITH_MSG_QUEUE */
	DEBUG_MSG("Winefish: exiting cleanly\n");
	return 0;
}

void bluefish_exit_request() {

	GList *tmplist;
	
	gboolean tmpb;
	DEBUG_MSG("winefish_exit_request, started\n");
	/* if we have modified documents we have to do something, file_close_all_cb()
	does exactly want we want to do */
	tmplist = return_allwindows_documentlist();
	tmpb = (tmplist && test_docs_modified(tmplist));
	g_list_free(tmplist);
	tmplist = g_list_first(main_v->bfwinlist);
	while (tmplist) {
		/* if there is a project, we anyway want to save & close the project */
		if (BFWIN(tmplist->data)->project) {
			if (!project_save_and_close(BFWIN(tmplist->data))) {
				/* cancelled or error! */
				DEBUG_MSG("winefish_exit_request, project_save_and_close returned FALSE\n");
				return;
			}
		}
		if (tmpb) {
			file_close_all_cb(NULL, BFWIN(tmplist->data));
		}
		tmplist = g_list_next(tmplist);
	}
	/* if we still have modified documents we don't do a thing,
	 if we don't have them we can quit */
	if (tmpb) {
		tmplist = return_allwindows_documentlist();
		tmpb = (tmplist && test_docs_modified(tmplist));
		g_list_free(tmplist);
		if (tmpb) {
			return;
		}
	}
/*	gtk_widget_hide(main_v->main_window);*/
	
	tmplist = g_list_first(gtk_window_list_toplevels());
	gchar *role=NULL;
	while (tmplist) {
		/* BUG#38 */
		if (GTK_IS_WIDGET(tmplist->data)) {
			role = g_strdup(gtk_window_get_role ((GtkWindow*)tmplist->data));
			gtk_widget_hide(GTK_WIDGET(tmplist->data));
			if (role && strncmp(role,"html_dialog",11) ==0) {
				window_destroy(GTK_WIDGET(tmplist->data));
			}
		}
		/* g_print("type = %s, role=%s\n", GTK_OBJECT_TYPE_NAME((GtkObject*) tmplist->data), role); */
		tmplist = g_list_next(tmplist);
	}
	g_free(role);

	flush_queue();

	rcfile_save_all();
	{
		gchar *filename = g_strconcat(g_get_home_dir(), "/.winefish/dir_history", NULL);
		put_stringlist_limited(filename, main_v->recent_directories, main_v->props.max_dir_history);
		g_free(filename);
	}
	
	gtk_main_quit();
}
