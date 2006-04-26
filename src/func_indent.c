#include <gtk/gtk.h>
#include <string.h>

#include "bluefish.h"
#include "document.h"
#include "undo_redo.h"
#include "bf_lib.h"

/**
* doc_toggle_highlighting_cb:
 * @doc: a #Tdocument*
 * @unindent: #gboolean
 *
 * Indent the selected block in current document.
 * Set unindent to TRUE to unindent.
 *
 * Return value: void
 **/
static void doc_indent_selection( Tdocument *doc, gboolean unindent ) {
	GtkTextIter itstart, itend;
	if ( gtk_text_buffer_get_selection_bounds( doc->buffer, &itstart, &itend ) ) {
		GtkTextMark * end;
		doc_unbind_signals( doc );
		doc_unre_new_group( doc );
		/* we have a selection, now we loop trough the characters, and for every newline
		we add or remove a tab, we set the end with a mark */
		end = gtk_text_buffer_create_mark( doc->buffer, NULL, &itend, TRUE );
		if ( gtk_text_iter_get_line_offset( &itstart ) > 0 ) {
			gtk_text_iter_set_line_index( &itstart, 0 );
		}
		while ( gtk_text_iter_compare( &itstart, &itend ) < 0 ) {
			GtkTextMark * cur;
			cur = gtk_text_buffer_create_mark( doc->buffer, NULL, &itstart, TRUE );
			if ( unindent ) {
				/* when unindenting we try to set itend to the end of the indenting step
				which might be a tab or 'tabsize' spaces, then we delete that part */
				gboolean cont = TRUE;
				gchar *buf = NULL;
				gunichar cchar = gtk_text_iter_get_char( &itstart );
				if ( cchar == 9 ) { /* 9 is ascii for tab */
					itend = itstart;
					cont = gtk_text_iter_forward_char( &itend );
					buf = g_strdup( "\t" );
				} else if ( cchar == 32 ) { /* 32 is ascii for space */
					gint i = 0;
					itend = itstart;
					gtk_text_iter_forward_chars( &itend, main_v->props.editor_tab_width );
					buf = gtk_text_buffer_get_text( doc->buffer, &itstart, &itend, FALSE );
					DEBUG_MSG( "tab_width=%d, strlen(buf)=%d, buf='%s'\n", main_v->props.editor_tab_width, strlen( buf ), buf );
					while ( cont && buf[ i ] != '\0' ) {
						cont = ( buf[ i ] == ' ' );
						DEBUG_MSG( "doc_indent_selection, buf[%d]='%c'\n", i, buf[ i ] );
						i++;
					}
					if ( !cont ) {
						g_free ( buf );
					}
				} else {
					cont = FALSE;
				}
				if ( cont ) {
					gint offsetstart, offsetend;
					offsetstart = gtk_text_iter_get_offset( &itstart );
					offsetend = gtk_text_iter_get_offset( &itend );
					gtk_text_buffer_delete( doc->buffer, &itstart, &itend );
					doc_unre_add( doc, buf, offsetstart, offsetend, UndoDelete );
					g_free ( buf );
				}
			} else { /* indent */
				gint offsetstart = gtk_text_iter_get_offset( &itstart );
				gchar *indentstring;
				gint indentlen;
				if ( main_v->props.view_bars & MODE_INDENT_WITH_SPACES ) {
					indentstring = bf_str_repeat( " ", main_v->props.editor_tab_width );
					indentlen = main_v->props.editor_tab_width;
				} else {
					indentstring = g_strdup( "\t" );
					indentlen = 1;
				}
				gtk_text_buffer_insert( doc->buffer, &itstart, indentstring, indentlen );
				doc_unre_add( doc, indentstring, offsetstart, offsetstart + indentlen, UndoInsert );
				g_free( indentstring );
			}
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &itstart, cur );
			gtk_text_buffer_get_iter_at_mark( doc->buffer, &itend, end );
			gtk_text_buffer_delete_mark( doc->buffer, cur );
			gtk_text_iter_forward_line( &itstart );
			DEBUG_MSG( "doc_indent_selection, itstart at %d, itend at %d\n", gtk_text_iter_get_offset( &itstart ), gtk_text_iter_get_offset( &itend ) );
		}
		gtk_text_buffer_delete_mark( doc->buffer, end );
		doc_bind_signals( doc );
		doc_set_modified( doc, 1 );
	} else {
		/* there is no selection, work on the current line */
		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_mark( doc->buffer, &iter, gtk_text_buffer_get_insert( doc->buffer ) );
		gtk_text_iter_set_line_offset( &iter, 0 );
		if ( unindent ) {
			gint deletelen = 0;
			gchar *tmpstr, *tmp2str;
			GtkTextIter itend = iter;
			gtk_text_iter_forward_chars( &itend, main_v->props.editor_tab_width );
			tmpstr = gtk_text_buffer_get_text( doc->buffer, &iter, &itend, FALSE );
			tmp2str = bf_str_repeat( " ", main_v->props.editor_tab_width );
			if ( tmpstr[ 0 ] == '\t' ) {
				deletelen = 1;
			} else if ( tmpstr && strncmp( tmpstr, tmp2str, main_v->props.editor_tab_width ) == 0 ) {
				deletelen = main_v->props.editor_tab_width;
			}
			g_free( tmpstr );
			g_free( tmp2str );
			if ( deletelen ) {
				itend = iter;
				gtk_text_iter_forward_chars( &itend, deletelen );
				gtk_text_buffer_delete( doc->buffer, &iter, &itend );
			}
		} else { /* indent */
			gchar *indentstring;
			gint indentlen;
			if ( main_v->props.view_bars & MODE_INDENT_WITH_SPACES ) {
				indentstring = bf_str_repeat( " ", main_v->props.editor_tab_width );
				indentlen = main_v->props.editor_tab_width;
			} else {
				indentstring = g_strdup( "\t" );
				indentlen = 1;
			}
			gtk_text_buffer_insert( doc->buffer, &iter, indentstring, indentlen );
			g_free( indentstring );
		}
	}
}

void menu_indent_cb( Tbfwin *bfwin, guint callback_action, GtkWidget *widget ) {
	if ( bfwin->current_document ) {
		doc_indent_selection( bfwin->current_document, ( callback_action == 1 ) );
	}
}