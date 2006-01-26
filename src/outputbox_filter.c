/* $Id$ */

/* Winefish LaTeX Editor
 *
 * Copyright (c) 2006 kyanh <kyanh@o2.pl>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define DEBUG

#include <gtk/gtk.h>
#include <string.h>

#include "config.h"
#include "bluefish.h"
#include "document.h"
#include "bf_lib.h"

#include "outputbox.h"
#include "outputbox_filter.h"
#include "gtk_easy.h" /* flush_queue */

static regex_t on_input_line;
static gboolean first_time = TRUE;

void outputbox_filter_line( Toutputbox *ob, const gchar *source )
{
	if (!source || (strlen(source) == 0) ) {
		return;
	}
	/* handle non UTF8 strings */
	/*|| !g_utf8_validate(source, -1, NULL)  */
	if (first_time) {
		regcomp( &on_input_line, "lines? ([0-9]+)", REG_EXTENDED );
		first_time = FALSE;
	}

	DEBUG_MSG("outputbox_filter_line: starting...\n");
	GtkTreeIter iter;
	gchar *tmp_src = NULL;
	gboolean scroll= FALSE;
	if ( ob->def->show_all_output & OB_SHOW_ALL_OUTPUT ) {
		tmp_src = g_markup_escape_text(source,-1);
		gtk_list_store_append( GTK_LIST_STORE( ob->lstore ), &iter );
		gtk_list_store_set( GTK_LIST_STORE( ob->lstore ), &iter, 2, tmp_src, -1 );
		g_free(tmp_src);
	} else if ( regexec( &ob->def->preg, source, NUM_MATCH, ob->def->pmatch, 0 ) == 0 ) {
		/* we have a valid line */
		gchar * filename, *line, *output;
		filename = line = output = NULL;
		gtk_list_store_append( GTK_LIST_STORE( ob->lstore ), &iter );
		if ( ob->def->file_subpat >= 0 && ob->def->pmatch[ ob->def->file_subpat ].rm_so >= 0 ) {
			DEBUG_MSG( "outputbox_filter_line, filename from %d to %d\n", ob->def->pmatch[ ob->def->file_subpat ].rm_so , ob->def->pmatch[ ob->def->file_subpat ].rm_eo );
			filename = g_strndup( &source[ ob->def->pmatch[ ob->def->file_subpat ].rm_so ], ob->def->pmatch[ ob->def->file_subpat ].rm_eo - ob->def->pmatch[ ob->def->file_subpat ].rm_so );
		}
		if ( ob->def->line_subpat >= 0 && ob->def->pmatch[ ob->def->line_subpat ].rm_so >= 0 ) {
			DEBUG_MSG( "outputbox_filter_line, line from %d to %d\n", ob->def->pmatch[ ob->def->line_subpat ].rm_so , ob->def->pmatch[ ob->def->line_subpat ].rm_eo );
			line = g_strndup( &source[ ob->def->pmatch[ ob->def->line_subpat ].rm_so ], ob->def->pmatch[ ob->def->line_subpat ].rm_eo - ob->def->pmatch[ ob->def->line_subpat ].rm_so );
		}
		if ( ob->def->output_subpat >= 0 && ob->def->pmatch[ ob->def->output_subpat ].rm_so >= 0 ) {
			DEBUG_MSG( "outputbox_filter_line, output from %d to %d\n", ob->def->pmatch[ ob->def->output_subpat ].rm_so , ob->def->pmatch[ ob->def->output_subpat ].rm_eo );
			output = g_strndup( &source[ ob->def->pmatch[ ob->def->output_subpat ].rm_so ], ob->def->pmatch[ ob->def->output_subpat ].rm_eo - ob->def->pmatch[ ob->def->output_subpat ].rm_so );
		}
		if ( filename ) {
			gchar * fullpath;
			/* create_full_path uses the current directory if no basedir is set */
			/* TODO: better hanlder with full path :) */
			gchar *basepath = g_path_get_basename(filename);
			fullpath = create_full_path( filename, NULL );
			gtk_list_store_set( GTK_LIST_STORE( ob->lstore ), &iter, 3, fullpath, -1 );
			gchar *tmpstr;
			tmpstr = g_markup_escape_text(basepath,-1);
			if (!ob->basepath_cached || (strcmp(ob->basepath_cached, basepath) !=0 )) {
				ob->basepath_cached = g_strdup(basepath);
				ob->basepath_cached_color = !ob->basepath_cached_color;
			}
			if (!ob->basepath_cached_color ) {
				tmpstr = g_strdup_printf("   <i>%s</i>", tmpstr);
			}else{
				tmpstr = g_strdup_printf("%s", tmpstr);
			}
			gtk_list_store_set( GTK_LIST_STORE( ob->lstore ), &iter, 0, tmpstr, -1 );
			DEBUG_MSG("outputbox_filter: fullpath %s\n", fullpath);
			g_free( filename );
			g_free( fullpath );
			g_free( basepath );
			g_free( tmpstr );
			scroll=TRUE;
		}
		if ( line ) {
			gtk_list_store_set( GTK_LIST_STORE( ob->lstore ), &iter, 1, line, -1 );
			g_free( line );
			scroll=TRUE;
		}
		if ( output ) {
			tmp_src = g_markup_escape_text(output,-1);
			gtk_list_store_set( GTK_LIST_STORE( ob->lstore ), &iter, 2, tmp_src, -1 );
			g_free( output );
			g_free(tmp_src);
			scroll=TRUE;
		}
		/* TODO: filter function */	
	} else if ( regexec( &on_input_line, source, NUM_MATCH, ob->def->pmatch, 0 ) == 0 ) {
		DEBUG_MSG( "outputbox_filter_line, line from %d to %d\n", ob->def->pmatch[ 1 ].rm_so , ob->def->pmatch[ 2 ].rm_eo );
		gchar *line = NULL;
		line = g_strndup( &source[ ob->def->pmatch[ 1 ].rm_so ], ob->def->pmatch[ 1 ].rm_eo - ob->def->pmatch[ 1 ].rm_so );
		if ( line ) {
			gtk_list_store_append( GTK_LIST_STORE( ob->lstore ), &iter );
			gtk_list_store_set( GTK_LIST_STORE( ob->lstore ), &iter, 1, line, -1 );
			g_free( line );
			DEBUG_MSG("outputbox_filter_line, line number = %s\n", line);
			gchar *tmp_src = g_markup_escape_text(source,-1);
			gtk_list_store_set( GTK_LIST_STORE( ob->lstore ), &iter, 2, tmp_src, -1 );
			g_free(tmp_src);
			scroll=TRUE;
		}
	} else if ( strstr( source, "LaTeX Error" )
			   || strstr( source, "Output written on")
					   || strstr( source, "Warning")
					   || strstr( source, "Overfull")
		  )
	{
		/* kyanh, 20050226,
		special filter for latex *.log file;
		this happens for e.g. when a Package not found (! LaTeX Error: .... )
		*/
		tmp_src = g_markup_escape_text(source,-1);
		gtk_list_store_append( GTK_LIST_STORE( ob->lstore ), &iter );
		gtk_list_store_set( GTK_LIST_STORE( ob->lstore ), &iter, 2, tmp_src, -1 );
		g_free(tmp_src);
		scroll=TRUE;
	}
	if (scroll) {
		DEBUG_MSG("outputbox_filter_line: scrolling...\n");
		if ( iter.stamp == GTK_LIST_STORE(ob->lstore)->stamp ) {
			GtkTreePath *treepath = gtk_tree_model_get_path( GTK_TREE_MODEL( ob->lstore ), &iter );
			gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( ob->lview ), treepath, NULL, FALSE, 0, 0 );
			gtk_tree_path_free( treepath );
		}
		flush_queue();
	}
	DEBUG_MSG("outputbox_filter_line: finished.\n");
}

/*
void outputbox_filter_file( Toutputbox *ob, const gchar *filename )
{
	
}
*/
