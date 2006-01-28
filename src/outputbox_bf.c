/* $Id$ */

/* Winefish LaTeX Editor (based on Bluefish)
 * external_commands.c - backend for external commands, filters and the outputbox
 *
 * Copyright (C) 2005 Olivier Sessink
 * Modified for Winefish (c) 2006 kyanh <kyanh@o2.pl>
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

#include "outputbox_cfg.h"

/*#define DEBUG*/

#ifdef __BF_BACKEND__

#include <gtk/gtk.h>
#include <string.h>
#include <sys/wait.h>

#include "config.h"
#include "bluefish.h"
#include "document.h"
#include "bf_lib.h"
#include "outputbox.h"
#include "outputbox_filter.h"
#include "outputbox_bf.h"
#include "gtk_easy.h"

/*
 * for the external commands, the external filters, and the outputbox, we have some general code
 * to create the command, and to start the command
 *
 * the external command is simple: it only needs formatting of the commandstring, and creation of any output files
 * for the outputbox, it needs the above, but also the output needs to go back to the outputbox widget
 * for filters it needs formatting of the commandstring, and the output should go to the text widget
 *
 */

static void start_command_backend(Texternalp *ep);
static gint child_pid_exit_code;

static void externalp_unref(Texternalp *ep) {
	ep->refcount--;
	DEBUG_MSG("externalp_unref, started for %p, refount=%d\n",ep,ep->refcount);
	if (ep->refcount <= 0) {
		DEBUG_MSG("externalp_unref, cleanup!\n");
		if (ep->commandstring) g_free(ep->commandstring);
		if (ep->fifo_in) {
			unlink(ep->fifo_in);
			g_free(ep->fifo_in);
		}
		if (ep->fifo_out) {
			unlink(ep->fifo_out);
			g_free(ep->fifo_out);
		}
		if (ep->tmp_in) {
			unlink(ep->tmp_in);
			g_free(ep->tmp_in);
		}
		if (ep->tmp_out) {
			unlink(ep->tmp_out);
			g_free(ep->tmp_out);
		}
		if (ep->inplace) {
			unlink(ep->inplace);
			g_free(ep->inplace);
		}
		
		if (ep->buffer_out) g_free(ep->buffer_out);
		if (ep->securedir) {
			rmdir(ep->securedir);
			g_free(ep->securedir);
		}
		g_free(ep);
		finish_execute(ep->ob);
	}
	/*finish_execute(ep->ob);*/ /* BUG#70 */
}

static gboolean start_command_write_lcb(GIOChannel *channel,GIOCondition condition,gpointer data) {
	Texternalp *ep = data;
	GIOStatus status;
	GError *error=NULL;
	gsize bytes_written=0;
	DEBUG_MSG("start_command_write_lcb, started, still %d bytes to go\n",strlen(ep->buffer_out_position));

	status = g_io_channel_write_chars(channel,ep->buffer_out_position,-1,&bytes_written,&error);
	DEBUG_MSG("start_command_write_lcb, %d bytes written\n",bytes_written);
	ep->buffer_out_position += bytes_written;
	if (strlen(ep->buffer_out) <= (ep->buffer_out_position - ep->buffer_out)) {
		DEBUG_MSG("start_command_write_lcb, finished, shutting down channel\n");
		g_io_channel_flush(channel,NULL);
		g_io_channel_shutdown(channel,TRUE,&error);
		/* TODO: g_io_channel_unref( ob->handle->channel_out ); */
		if (ep->tmp_in) {
			start_command_backend(ep);
		} else {
			externalp_unref(ep);
		}
		return FALSE;
	}
	return TRUE;
}

static void spawn_setup_lcb(gpointer data) {
	Texternalp *ep = data;
#ifndef WIN32
	/* because win32 does not have both fork() and excec(), this function is
	not called in the child but in the parent on win32 */
	if (ep->include_stderr) {
		dup2(STDOUT_FILENO,STDERR_FILENO);
	}
#endif
}
/* watch for child_pid when it exists */
static void child_watch_lcb(GPid pid,gint status,gpointer data) {
	Texternalp *ep = data;
	DEBUG_MSG("child_watch_lcb, status=%d\n",status);
	/* if there was a temporary output file, we should now open it and start to read it */
	if (ep->tmp_out) {
		ep->channel_out = g_io_channel_new_file(ep->tmp_out,"r",NULL);
		DEBUG_MSG("child_watch_lcb, created channel_out from file %s\n",ep->tmp_out);
	} else if (ep->inplace) {
		ep->channel_out = g_io_channel_new_file(ep->inplace,"r",NULL);
		DEBUG_MSG("child_watch_lcb, created channel_out from file %s\n",ep->inplace);
	}
	if (ep->tmp_out||ep->inplace){
		ep->refcount++;
		g_io_channel_set_flags(ep->channel_out,G_IO_FLAG_NONBLOCK,NULL);
		g_io_channel_set_encoding( ep->channel_out, NULL, NULL ); /* kyanh, BUG#20 */
		DEBUG_MSG("child_watch_lcb, add watch for channel_out\n");
		g_io_add_watch(ep->channel_out, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP,ep->channel_out_lcb,ep->channel_out_data);
	}
	g_spawn_close_pid(pid);
	externalp_unref(ep);
	if ( WIFEXITED (status) ){
		gint exitcode = WEXITSTATUS( status );
		child_pid_exit_code = exitcode;
	}else{
		child_pid_exit_code = -1;
	}
}

static void start_command_backend(Texternalp *ep) {
	gchar *argv[4];
	gint standard_input=0,standard_output=0;
	GError *error=NULL;
	gchar *tmpstr;
	argv[0] = "/bin/sh";
	argv[1] = "-c";
	argv[2] = ep->commandstring;
	argv[3] = NULL;
	if (ep->fifo_in) {
		if (mkfifo(ep->fifo_in, 0600) != 0) {
			tmpstr = g_strdup_printf(_("some error happened creating fifo %s"),ep->fifo_in);
			outputbox_message(ep->ob, tmpstr, OB_MESSAGE_RED);
			g_free(tmpstr);
			return;
		}
		DEBUG_MSG("start_command, created fifo %s\n",ep->fifo_in);
	}
	if (ep->fifo_out) {
		if (mkfifo(ep->fifo_out, 0600) != 0) {
			tmpstr = g_strdup_printf(_("some error happened creating fifo %s"),ep->fifo_out);
			outputbox_message(ep->ob, tmpstr, OB_MESSAGE_RED);
			g_free(tmpstr);
			return;
		}
	}
#ifdef WIN32
	ep->include_stderr = FALSE;
#endif
	DEBUG_MSG("start_command, pipe_in=%d, pipe_out=%d, fifo_in=%s, fifo_out=%s,include_stderr=%d\n",ep->pipe_in,ep->pipe_out,ep->fifo_in,ep->fifo_out,ep->include_stderr);
	DEBUG_MSG("start_command, about to spawn process /bin/sh -c %s\n",argv[2]);
	g_spawn_async_with_pipes(NULL,argv,NULL,G_SPAWN_DO_NOT_REAP_CHILD,(ep->include_stderr)?spawn_setup_lcb:NULL,ep,&ep->child_pid,
				(ep->pipe_in) ? &standard_input : NULL,
				(ep->pipe_out) ? &standard_output : NULL,
				NULL,&error);
	ep->refcount++;
	g_child_watch_add(ep->child_pid,child_watch_lcb,ep);
	
	if (error) {
		DEBUG_MSG("start_command, there is an error!!\n");
	}
	if (ep->pipe_in) {
		ep->channel_in = g_io_channel_unix_new(standard_input);
	} else if (ep->fifo_in) {
		DEBUG_MSG("start_command, connecting channel_in to fifo %s\n",ep->fifo_in);
		ep->channel_in = g_io_channel_new_file(ep->fifo_in,"w",NULL);
	}
	if (ep->pipe_in || ep->fifo_in) {
		ep->refcount++;
		ep->buffer_out = ep->buffer_out_position = doc_get_chars(OUTPUTBOX(ep->ob)->bfwin->current_document,0,-1);
		g_io_channel_set_flags(ep->channel_in,G_IO_FLAG_NONBLOCK,NULL);
		/* now we should start writing, correct ? */
		DEBUG_MSG("start_command, add watch for channel_in\n");
		g_io_channel_set_encoding( ep->channel_in, NULL, NULL ); /* kyanh, BUG#20 */
		g_io_add_watch(ep->channel_in,G_IO_OUT,start_command_write_lcb,ep);
	}
	if (ep->pipe_out) {
		ep->channel_out = g_io_channel_unix_new(standard_output);
		DEBUG_MSG("start_command, created channel_out from pipe\n");
	} else if (ep->fifo_out) {
		ep->channel_out = g_io_channel_new_file(ep->fifo_out,"r",NULL);
		DEBUG_MSG("start_command, created channel_out from fifo %s\n",ep->fifo_out);
	}
	if (ep->channel_out_lcb && (ep->pipe_out || ep->fifo_out)) {
		ep->refcount++;
		g_io_channel_set_flags(ep->channel_out,G_IO_FLAG_NONBLOCK,NULL);
		DEBUG_MSG("start_command, add watch for channel_out\n");
		g_io_channel_set_encoding( ep->channel_out, NULL, NULL ); /* kyanh, BUG#20 */
		g_io_add_watch(ep->channel_out, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP,ep->channel_out_lcb,ep->channel_out_data);
		OUTPUTBOX(ep->ob)->OB_FETCHING = OB_GO_FETCHING;
	}
	
}

static void start_command(Texternalp *ep) {
	if (ep->tmp_in) {
		/* first create tmp_in, then start the real command in the callback */
		ep->channel_in = g_io_channel_new_file(ep->tmp_in,"w",NULL);
		ep->buffer_out = ep->buffer_out_position = doc_get_chars(OUTPUTBOX(ep->ob)->bfwin->current_document,0,-1);
		g_io_channel_set_flags(ep->channel_in,G_IO_FLAG_NONBLOCK,NULL);
		DEBUG_MSG("start_command, add watch for channel_in\n");
		g_io_channel_set_encoding( ep->channel_in, NULL, NULL ); /* kyanh, BUG#20 */
		g_io_add_watch(ep->channel_in,G_IO_OUT,start_command_write_lcb,ep);
	} else {
		start_command_backend(ep);
	}
}

static gboolean outputbox_io_watch_lcb(GIOChannel *channel,GIOCondition condition,gpointer data) {
	Texternalp *ep = data;
	Toutputbox *ob = OUTPUTBOX(ep->ob);

	switch (ob->OB_FETCHING) {
		case OB_GO_FETCHING:
			break;
		case OB_IS_FETCHING:
			DEBUG_MSG("outputbox_io_watch_lcb: fetching. Return now.\n");
			return FALSE;
			break; /* never reach here */
		case OB_STOP_REQUEST:
			DEBUG_MSG("outputbox_io_watch_lcb: stop request. Call finish_execute()...\n");
			finish_execute(ep->ob);
			return FALSE;
			break;
		default:
			DEBUG_MSG("outputbox_io_watch_lcb: stoppped flag. return. fetching=%d\n", ob->OB_FETCHING);
			return FALSE;
			break;
	}

	ob->OB_FETCHING = OB_IS_FETCHING;

	DEBUG_MSG("outputbox_io_watch_lcb, called with condition %d\n",condition);
	if (condition & G_IO_IN) {
		gchar *buf=NULL;
		gsize buflen=0,termpos=0;
		GError *error=NULL;
		GIOStatus status = g_io_channel_read_line(channel,&buf,&buflen,&termpos,&error);
		while ((status == G_IO_STATUS_NORMAL) && (ob->OB_FETCHING == OB_IS_FETCHING)) {
			if (buflen > 0) {
				if (termpos < buflen) buf[termpos] = '\0';
				outputbox_filter_line(ep->ob, buf);
				g_free(buf);
			}
			if (ep->child_pid) {
				status = g_io_channel_read_line(channel,&buf,&buflen,&termpos,&error);
			}else{
				ob->OB_FETCHING = OB_IS_READY; /* tool is already stopped */
			}
		}
		if (status == G_IO_STATUS_EOF) {
			ob->OB_FETCHING = OB_STOP_REQUEST;
			finish_execute(ep->ob);
			return FALSE;
		}
	}
	if (condition & G_IO_OUT) {
		DEBUG_MSG("outputbox_io_watch_lcb, condition %d G_IO_OUT not handled\n",condition);
	}
	if (condition & G_IO_PRI) {
		DEBUG_MSG("outputbox_io_watch_lcb, condition %d G_IO_PRI not handled\n",condition);
	}
	if (condition & G_IO_ERR) {
		DEBUG_MSG("outputbox_io_watch_lcb, condition %d G_IO_ERR not handled\n",condition);
	}
	if (condition & G_IO_HUP) {
		DEBUG_MSG("outputbox_io_watch_lcb, condition %d G_IO_HUP\n",condition);
		ob->OB_FETCHING = OB_STOP_REQUEST;
		finish_execute(ep->ob);
		return FALSE;
	}
	if (condition & G_IO_NVAL) {
		DEBUG_MSG("outputbox_io_watch_lcb, condition %d G_IO_NVAL not handled\n",condition);
	}
	if (ob->OB_FETCHING == OB_IS_FETCHING) {
		ob->OB_FETCHING = OB_GO_FETCHING;
	}	
	return TRUE;
}

void run_command(Toutputbox *ob) {
	DEBUG_MSG("run_command, ==================================\n");
	/*if ( ob->bfwin->current_document->filename ) {*/
		{
			gchar * tmpstring;
			if ( ob->bfwin->project && ( ob->bfwin->project->view_bars & MODE_PROJECT ) && ob->bfwin->project->basedir )
			{
				tmpstring = g_strdup( ob->bfwin->project->basedir );
			} else
			{
				if (ob->bfwin->current_document->filename) {
					tmpstring = g_path_get_dirname( ob->bfwin->current_document->filename );
				}else{
					tmpstring = g_strdup(".");
				}
			}
			/* outputbox_message(ob, g_strconcat("> working dir: ", tmpstring, NULL)); */
			chdir( tmpstring );
			g_free( tmpstring );
		}
		
		Texternalp *ep;
		/* menuitem_set_sensitive(ob->bfwin->menubar, N_("/External/Stop..."), TRUE); */
		outputbox_set_status(ob, TRUE, FALSE);

		ep = g_new0(Texternalp,1);
		/* ep->formatstring = formatstring; */
		ep->commandstring = convert_command(ob->bfwin,ob->def->command);
		if (!ep->commandstring) {
			/**/
			g_free( ob->def->pattern );
			regfree( &ob->def->preg );
			g_free( ob->def->command );
			g_free( ob->def );
			/**/
			g_free(ep);
			/* BUG: is the user notified of the error ?*/
			return;
		}
		outputbox_message( ob, ep->commandstring, OB_MESSAGE_BOLD);
		ep->pipe_out = TRUE;
		ep->include_stderr = TRUE;
		ep->channel_out_lcb = outputbox_io_watch_lcb;
		ep->channel_out_data = ep;
		ob->handle = ep;
		ep->ob = ob;

		start_command(ep);
		/*	
	} else {
		ob->OB_FETCHING = OB_IS_READY;
		outputbox_message( ob, _("tool canceled."), "b" );
	}
		*/
}

static gboolean finish_execute_called = FALSE;

void finish_execute( Toutputbox *ob ) {
	if ((ob->handle->child_pid == 0) || finish_execute_called) {
		DEBUG_MSG("finish_execute: entering... refused.\n");
		return;
	}else{
		finish_execute_called = TRUE;
		DEBUG_MSG("finish_execute: entering... okay.\n");
	}
	if (!(ob->OB_FETCHING == OB_IS_STOPPED)) {
		ob->OB_FETCHING = OB_IS_STOPPED;
		g_io_channel_flush(ob->handle->channel_out,NULL);
		g_io_channel_shutdown(ob->handle->channel_out,TRUE/*flush*/,NULL);
		g_io_channel_unref( ob->handle->channel_out );
		if ( child_pid_exit_code > -1 ) {
			gchar *str_status = g_strdup_printf(_("exit code: %d"), child_pid_exit_code);
			outputbox_message( ob, str_status, OB_MESSAGE_BOLD );
			g_free( str_status );
		} else {
			outputbox_message( ob, _("the child process exited abnormally"), OB_MESSAGE_RED);
		}
	}
	/**/
	ob->basepath_cached_color = FALSE;
	g_free( ob->basepath_cached );
	g_free( ob->def->pattern );
	regfree( &ob->def->preg );
	g_free( ob->def->command );
	g_free( ob->def );
	/**/
	/* menuitem_set_sensitive(ob->bfwin->menubar, N_("/External/Stop..."), FALSE); */
	outputbox_set_status(ob, FALSE, FALSE);
	ob->handle->child_pid = 0;
	ob->OB_FETCHING = OB_IS_READY;
	finish_execute_called = FALSE;
	gtk_tree_view_columns_autosize( GTK_TREE_VIEW( ob->lview ) );
}
#endif /* __BF_BACKEND__ */
