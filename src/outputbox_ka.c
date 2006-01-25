/* $Id$ */

/* Winefish LaTeX Editor
 Copyright (c) 2005 2006 kyanh <kyanh@o2.pl>
*/

#include "outputbox_cfg.h"

#ifdef __KA_BACKEND__

#include <gtk/gtk.h>

#include <sys/types.h>
#include <signal.h> /* kill() */
#include <sys/stat.h> /* open() */
#include <fcntl.h> /* kyanh, open() */ 
/* kyanh, 20050301,
Thanks to M. Garoche <...@easyconnect.fr>
Move from <wait.h> to <sys/wait.h>
*/
#include <sys/wait.h> /* wait(), waitid() */
#include <regex.h>
#include <stdlib.h>
#include <string.h> /* strlen() */

/* Using waitpid() to free child process's resources,
we don't need anymore <signal.h>. Anyway, thanks to
M. Garoche for reporting a problem with Mac OS */

#include "config.h"
#include "bluefish.h"
#include "document.h"
#include "gtk_easy.h"
#include "outputbox.h"
#include "bf_lib.h"
#include "outputbox_ka.h"
#include "outputbox_filter.h"

/* kyanh, added, 20050301 */
static void free_ob( Toutputbox *ob, gboolean have_retfile )
{
	DEBUG_MSG("free_ob: starting...\n");
	if ( have_retfile ) {
		/* free temporarily file */
		DEBUG_MSG( "free_ob: retfile=%s\n", ob->retfile );
		remove_secure_dir_and_filename( ob->retfile );
		/* g_free( ob->retfile ); */
	}
	g_free( ob->def->pattern );
	regfree( &ob->def->preg );
	g_free( ob->def->command );
	g_free( ob->def );
	ob->def = NULL; /* to be check for next using */
	ob->pid = 0;
	DEBUG_MSG("free_ob: finished.\n");
}

/* kyanh, 20050302, moved from continute_execute() */
void finish_execute( Toutputbox *ob )
{
	if (!(ob->OB_FETCHING == OB_IS_STOPPED)) {
		ob->OB_FETCHING = OB_IS_STOPPED;
		DEBUG_MSG("finish_execute: starting...\n");
		menuitem_set_sensitive(ob->bfwin->menubar, N_("/External/Stop..."), FALSE);
	
		g_io_channel_shutdown( ob->io_channel, FALSE, NULL);
		g_io_channel_unref( ob->io_channel );
		g_source_remove ( ob->pollID );
	
		DEBUG_MSG("finish_execute: kill the pid = %d\n", ob->pid);
		kill( ob->pid, SIGTERM );
		waitpid( ob->pid, &child_exit_status, WNOHANG );
	
		if ( WIFEXITED (child_exit_status) ){
			gint exitcode = WEXITSTATUS( child_exit_status );
			gchar *str_status = g_strdup_printf(_("Exit code: %d"), exitcode);
			outputbox_message( ob, str_status, "b" );
			g_free( str_status );
		}else{
			outputbox_message( ob, _("the child process exited abnormally"), "b");
		}
	
		gtk_tree_view_columns_autosize( GTK_TREE_VIEW( ob->lview ) );
		free_ob( ob, 1 );
		DEBUG_MSG("finish_execute: completed.\n");
	}
	ob->OB_FETCHING = OB_IS_READY;
}

/* Idea taken from SciTTEGTK.cxx
 Modified by kyanh@o2.pl
 kyanh, version 2 (20050302): use g_io_channel()
 kyanh, version 2.1 (20050315): readline(), not readchars()
*/

static gboolean continue_execute( Toutputbox *ob )
{
	DEBUG_MSG("continue_execute: starting...\n");
	switch (ob->OB_FETCHING) {
		case OB_GO_FETCHING:
			break;
		case OB_IS_FETCHING: /* never reach... see io_signal() and poll_tool() */
			DEBUG_MSG("continue_execute: fetching. Return now.\n");
			return FALSE;
			break; /* never reach here */
		case OB_STOP_REQUEST:
			DEBUG_MSG("continue_execute: stop request. Call finish_execute()...\n");
			finish_execute(ob);
			return FALSE;
			break;
		default:
			DEBUG_MSG("continue_execute: stoppped flag. Return...\n");
			return FALSE;
			break;
	}

	gsize count = 0;
	GIOStatus io_status;
	GError *error = NULL;
	gchar *buf = NULL;
	gsize terminator_pos = 0;
	gboolean continued = TRUE;

	ob->OB_FETCHING = OB_IS_FETCHING;
	while ( (ob->OB_FETCHING == OB_IS_FETCHING) && continued ) {
		continued = FALSE;
		buf = NULL;
		io_status = g_io_channel_read_line( ob->io_channel, &buf, &count, &terminator_pos, &error );
		switch ( io_status ) {
			case G_IO_STATUS_ERROR:
				DEBUG_MSG("continue_execute: G_IO_STATUS_ERROR\n");
				{
					gchar * tmpstr;
					tmpstr = g_strdup_printf( _("IOChannel Error: %s"), error->message );
					outputbox_message( ob, tmpstr, "b" );
					g_free( tmpstr );
					finish_execute( ob );
				}
				break;
				case G_IO_STATUS_EOF: /* without this, we dump into an infinite loop */
					DEBUG_MSG("continue_execute: G_IO_STATUS_EOF. Call finish_excute()\n");
					finish_execute( ob );
					break;
			case G_IO_STATUS_NORMAL:
				DEBUG_MSG("continue_execute: G_IO_STATUS_NORMAL. Get line from channel...\n");
				continued = TRUE;
				if ( terminator_pos < count ) {
					buf[ terminator_pos ] = '\0';
				}
				outputbox_filter_line( ob, buf );
				break;
			default:
				DEBUG_MSG("continue_execute: G_IO_STATUS_???\n");
				break;
		}
	}
	if (ob->OB_FETCHING == OB_IS_FETCHING) {
		ob->OB_FETCHING = OB_GO_FETCHING;
	}
	DEBUG_MSG("continue_execute: fetching = %d.\n", fetching);
	g_free( buf );
	g_clear_error( &error );
	DEBUG_MSG("continue_execute: finished.\n");
	return FALSE;
}

/* continue to read data */
/* written by kyanh, 20050301 */
static gboolean io_signal( GIOChannel *source, GIOCondition condition, Toutputbox *ob )
{
	DEBUG_MSG("io_signal:\n");
	if ( condition & (G_IO_NVAL|G_IO_ERR|G_IO_HUP) ) {
		ob->OB_FETCHING = OB_STOP_REQUEST;
	}
	if ( (ob->OB_FETCHING == OB_GO_FETCHING) || (ob->OB_FETCHING == OB_STOP_REQUEST) ) {
		return continue_execute( ob );
	}
#ifdef DEBUG
	else{
	DEBUG_MSG("io_signal: do nothing\n");
	return TRUE;
	}
#endif
	return FALSE;
}

/* Taken from SciTTEGTK.cxx
Detect if the tool has exited without producing any output */
static gboolean poll_tool( Toutputbox *ob )
{
	DEBUG_MSG("poll_tool:\n");
	if ( (ob->OB_FETCHING == OB_GO_FETCHING) || (ob->OB_FETCHING == OB_STOP_REQUEST) ) {
		continue_execute( ob );
	}
#ifdef DEBUG
	else{
	DEBUG_MSG("poll_tool: do nothing\n");
	}
#endif
	return FALSE;
}

/* Taken from SciTTEGTK.cxx
kyanh: everything emitted from `running' will be captured [ 2>&1 ]
*/
static gint xsystem( const gchar *command, const gchar *outfile )
{
	gint pid = 0;
	/* fork():
	create a child proccess the differs from the parent only in its PID and PPID;
	the resouce ultilisation are set to 0 */
	if ( ( pid = fork() ) == 0 ) {
		close( 0 );
		gint fh = open( outfile, O_WRONLY );
		close( 1 );
		dup( fh ); /* dup uses the lowest numbered unused descriptor for the new descriptor. */
		close( 2 );
		dup( fh );
		execlp( "/bin/sh", "sh", "-c", command, NULL );
		/* Functions that contain the letter l (execl, execlp, and
		execle) accept the argument list using the C language varargs mechanism. */
		/* The execvp function returns only if an error occurs. */
		exit( 127 );
	}
	/* This is the parent process. */
	return pid;
}

void run_command( Toutputbox *ob )
{
	DEBUG_MSG("run_command: starting...\n");
	file_save_cb( NULL, ob->bfwin );
	{
		gchar *format_str;
		if ( ob->bfwin->project && ( ob->bfwin->project->view_bars & PROJECT_MODE ) ) {
			format_str = g_strdup_printf(_("%s # project mode: ON # backend 1"), ob->def->command );
		} else {
			format_str = g_strdup_printf(_("%s # project mode: OFF # backend 1"), ob->def->command );
		}
		outputbox_message( ob, format_str, "i" );
		g_free( format_str );
		flush_queue();
	}

	if ( ob->bfwin->current_document->filename ) {
		/* if the user clicked cancel at file_save -> return */
		{
			gchar * tmpstring;
			if ( ob->bfwin->project && ( ob->bfwin->project->view_bars & PROJECT_MODE ) && ob->bfwin->project->basedir )
			{
				tmpstring = g_strdup( ob->bfwin->project->basedir );
			} else
			{
				tmpstring = g_path_get_dirname( ob->bfwin->current_document->filename );
			}
			/* outputbox_message(ob, g_strconcat("> working dir: ", tmpstring, NULL)); */
			chdir( tmpstring );
			g_free( tmpstring );
		}

		gchar *command = convert_command( ob->bfwin, ob->def->command );
		outputbox_message( ob, command, "b");

		ob->retfile = create_secure_dir_return_filename();

		gint fd = 1;
		if ( ob->retfile ) {
			DEBUG_MSG("run_command: retfile = %s\n", ob->retfile);
			fd = mkfifo( ob->retfile, S_IRUSR | S_IWUSR );
			if ( fd == 0 ) {
				menuitem_set_sensitive(ob->bfwin->menubar, N_("/External/Stop..."), TRUE);
				ob->pid = xsystem( command, ob->retfile );
				GError *error = NULL;
				ob->io_channel = g_io_channel_new_file( ob->retfile, "r", &error );
				if ( ob->io_channel != NULL ) {
					ob->OB_FETCHING = OB_GO_FETCHING;
					/* Fix the BUG[200503]#20 */
					g_io_channel_set_encoding( ob->io_channel, NULL, NULL );
					g_io_add_watch( ob->io_channel, G_IO_IN|G_IO_PRI|G_IO_NVAL|G_IO_ERR|G_IO_HUP, ( GIOFunc ) io_signal, ob );
					/* add a background task in case there is no output from the tool */
					ob->pollID = g_timeout_add( 200, ( GSourceFunc ) poll_tool, ob );
				} else {
					ob->OB_FETCHING = OB_ERROR;
					gchar *tmpstr;
					tmpstr = g_strdup_printf( _("Error: %s"), error->message );
					outputbox_message( ob, tmpstr, "b" );
					if ( error->code == G_FILE_ERROR_INTR ) {
						outputbox_message( ob, _("hint: you may call the tool again"), "i" );
					}
					g_free( tmpstr );
					outputbox_message( ob, _("tool finished."), "b" );
					free_ob( ob, 1 );
				}
				g_clear_error( &error );
			} else {
				ob->OB_FETCHING = OB_ERROR;
				outputbox_message( ob, _("error: cannot create PIPE file."), "b" );
				free_ob( ob, 1 );
			}
		} else {
			ob->OB_FETCHING = OB_ERROR;
			outputbox_message( ob, _("error: cannot create temporarily file."), "b" );
			free_ob( ob, 0 );
		}
		g_free( command );
	} else {
		ob->OB_FETCHING = OB_ERROR;
		outputbox_message( ob, _("tool canceled."), "b" );
		free_ob( ob, 0 );
	}
	DEBUG_MSG("run_command: finished.\n");
}

#endif /* __KA_BACKEND__ */
