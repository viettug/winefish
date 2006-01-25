/* $Id: msg_queue.c,v 1.2 2005/07/06 01:27:09 kyanh Exp $ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * msg_queue.c - message queue handling
 *
 * Copyright (C) 2003 Olivier Sessink
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

/* #define DEBUG */

#include "bluefish.h"

#ifdef WITH_MSG_QUEUE

#include <sys/types.h>
#include <sys/ipc.h>			/* msgsnd() */
#include <sys/msg.h>			/* msgsnd() */
#include <time.h>
#include <errno.h>				/* errno */
#include <unistd.h> /* getpid, getuid */
#include <string.h> /* strncpy */
#include <stdlib.h> /* exit() */

#include "stringlist.h"
#include "gtk_easy.h" /* *_dialog */
#include "gui.h" /* notebook_changed() */
#include "document.h"
#include "project.h"

#define BLUEFISH_MSG_QUEUE 9723475
#define MSQ_QUEUE_SIZE 1024
#define MSQ_QUEUE_SMALL_SIZE 7
#define MSQ_QUEUE_CHECK_TIME 300	/* miliseconds for gtk_timeout*/

/* send alive must have the highest number, because it is the only type that 
 * should not be read by the master process. The sending processes reads this
 * to check if the queue is alive
 */
#define MSG_QUEUE_SEND_ALIVE 46064
#define MSG_QUEUE_OPENFILE 46063
#define MSG_QUEUE_OPENPROJECT 46062
#define MSG_QUEUE_OPENNEWWIN 46061
#define MSG_QUEUE_SETLINE 46060

/* from man msgrcv: 'the first message on the queue with the lowest type less 
 * than or equal to the absolute value of msgtyp will be read'
 * that means the requestalive should have the lowest number, because
 * it has the highest priority
 */
#define MSG_QUEUE_ASK_ALIVE 46009

#define MSG_QUEUE_PER_DOCUMENT_TIMEOUT 20000000	/* nanoseconds */

/* 
the message queue system is quite easy:

if there is no queue --> open it and start listening
on a MSG_QUEUE_OPENFILE, open the file, on a MSG_QUEUE_ASK_ALIVE return
the same data with type MSG_QUEUE_SEND_ALIVE (which we are not
listening for)

if there is a queue and there are files loaded on the commandline --> 
check the server (send a keepalive containing the pid) and send the 
files to the queue, after sending the files check if a 
MSG_QUEUE_SEND_ALIVE is received contaning the pid. If so, there is
a server and we can quit. If not, we continue starting and load them

if there is a queue and we do not have files loaded on the commandline
we just start, but we don't listen to the queue!
*/

typedef struct {
	gboolean functional;
	gboolean server;
	int msgid;
	GList *file_error_list;
	gint linenumber;
} Tmsg_queue;

/******************************/
/* global var for this module */
/******************************/
Tmsg_queue msg_queue = { TRUE, FALSE, -1, NULL, -1};

/**
 * msg_queue_check_alive:
 *
 * checks the message queue for messages of type MSG_QUEUE_SEND_ALIVE
 * if we receive such a message, there must be another process
 * active on this message queue
 */
static gboolean msg_queue_check_alive(gboolean wait_first)
{
	struct small_msgbuf {
		long mtype;
		char mtext[MSQ_QUEUE_SMALL_SIZE];
	} small_msgp;
	gchar *pid_string = g_strdup_printf("%d", (int) getpid());

	if (wait_first) {
		static struct timespec const req = { 0, MSQ_QUEUE_CHECK_TIME * 1000000};
		static struct timespec rem;
		nanosleep(&req, &rem);
	}

	while (msgrcv
		   (msg_queue.msgid, &small_msgp, MSQ_QUEUE_SMALL_SIZE * sizeof(char), MSG_QUEUE_SEND_ALIVE,
			IPC_NOWAIT) != -1) {
		DEBUG_MSG("msg_queue_check_alive, received a keepalive, mtext=%s!\n", small_msgp.mtext);
		if (strncmp(pid_string, small_msgp.mtext, MSQ_QUEUE_SMALL_SIZE - 1) == 0) {
			DEBUG_MSG("msg_queue_check_alive, keepalive matches request!\n");
			/* we did receive a keepalive on our request !! */
			g_free(pid_string);
			return TRUE;
		} else {
			DEBUG_MSG("msg_queue_check_alive, keepalive does NOT match request %s\n", pid_string);
		}
	}
	DEBUG_MSG("msg_queue_check_alive, errno=%d, error=%s\n", errno, g_strerror(errno));
	g_free(pid_string);
	return FALSE;
}

/**
 * msg_queue_open:
 *
 *  returns 1 if another process has the queue open already
 *  returns 0 if we opened the queue
 */
static gboolean msg_queue_open(void)
{
	DEBUG_MSG("msg_queue_open, started\n");
	msg_queue.msgid = msgget((key_t) BLUEFISH_MSG_QUEUE + getuid(), 0666 | IPC_CREAT | IPC_EXCL);
#ifdef DEBUG
	if (msg_queue.msgid == -1) {
		DEBUG_MSG("msg_queue_open, errno=%d, error=%s\n", errno, g_strerror(errno));
	}
#endif
	/* if msg_queue.msgid == -1 the message queue was already opened by another process */
	DEBUG_MSG("msg_queue_open, msg_queue.msgid=%d\n", msg_queue.msgid);
	if (msg_queue.msgid == -1) {
		msg_queue.msgid = msgget((key_t) BLUEFISH_MSG_QUEUE + getuid(), 0666);
		DEBUG_MSG("msg_queue_open, connected to existing message queue, id=%d\n", msg_queue.msgid);
		
		/* now we want to avoid the situation where the message queue is full (because the server died)
		so we cannot send a keepalive, so we check if the queue is filled (assume when there are >5 messages)
		and the last completed msgrcv() call was > 5 seconds ago */
		{
			struct msqid_ds msg_stat;
			gint timediff;
			/* check if there are messages on the queue, if so, check when the last msgrcv() call was on this queue */
			msgctl(msg_queue.msgid, IPC_STAT, &msg_stat);
			if (msg_stat.msg_qnum > 5) {
				timediff = time(NULL) - msg_stat.msg_ctime;
				if (timediff > 2) {
					DEBUG_MSG("msg_queue_request_alive, more then 2 seconds no reads on message_queue, timediff=%d, deleting queue\n", timediff);
					msgctl(msg_queue.msgid, IPC_RMID, NULL);
					msg_queue.msgid = msgget((key_t) BLUEFISH_MSG_QUEUE + getuid(), 0666 | IPC_CREAT | IPC_EXCL);
					return FALSE;
				}
			}
		}
		if (msg_queue.msgid == -1) {
			DEBUG_MSG("msg_queue_open, errno=%d, error=%s\n", errno, g_strerror(errno));
			msg_queue.functional = FALSE;
		}
		return TRUE;
	}
	return FALSE;
}
/**
 * msg_queue_check:
 *
 * checks the queue for any messages
 * this is called by the master program, usually by gtk_timeout_add()
 * the messages it will listen to are the types:
 * - MSG_QUEUE_ASK_ALIVE - we should respond with a type MSG_QUEUE_SEND_ALIVE
 * - MSG_QUEUE_OPENFILE - open a filename
 * - MSG_QUEUE_OPENPROJECT - open a filename as project
 * - MSG_QUEUE_OPENNEWWIN - open a new window
 */
static gboolean msg_queue_check(gint started_by_gtk_timeout)
{
	struct msgbuf {
		long mtype;
		char mtext[MSQ_QUEUE_SIZE];
	} msgp;
	gint retval;
	if (main_v->bfwinlist == NULL || BFWIN(main_v->bfwinlist->data)->documentlist == NULL) {
		DEBUG_MSG("msg_queue_check, no documentlist yet, so we do not continue\n");
		return TRUE;
	}

	if (msg_queue.msgid == -1) {
		return FALSE;
	}
	retval = msgrcv(msg_queue.msgid, &msgp, MSQ_QUEUE_SIZE, -MSG_QUEUE_OPENFILE, IPC_NOWAIT);
	if (retval != -1) {
		DEBUG_MSG("msg_queue_check, found type %ld\n", msgp.mtype);
		if (msgp.mtype == MSG_QUEUE_ASK_ALIVE) {
			struct small_msgbuf {
				long mtype;
				char mtext[MSQ_QUEUE_SMALL_SIZE];
			} small_msgp;
			DEBUG_MSG("msg_queue_check, a keepalive is asked from %s, sending!\n", msgp.mtext);
			small_msgp.mtype = MSG_QUEUE_SEND_ALIVE;
			strncpy(small_msgp.mtext, msgp.mtext, MSQ_QUEUE_SMALL_SIZE - 1);
			msgsnd(msg_queue.msgid, (void *) &small_msgp, MSQ_QUEUE_SMALL_SIZE * sizeof(char),
				   IPC_NOWAIT);
		} else if (msgp.mtype == MSG_QUEUE_OPENFILE) {
			GList *lastlist = g_list_last(main_v->bfwinlist);
			DEBUG_MSG("msg_queue_check, a filename %s is received\n", msgp.mtext);
			Tdocument *tmpdoc = doc_new_with_file(BFWIN(lastlist->data),msgp.mtext, FALSE, TRUE);
			if (!tmpdoc) {
				msg_queue.file_error_list = g_list_append(msg_queue.file_error_list, g_strdup(msgp.mtext));
			} else if (msg_queue.linenumber >=0) {
				doc_activate( tmpdoc );
				doc_select_line( tmpdoc, msg_queue.linenumber, TRUE );
				gtk_widget_grab_focus( GTK_WIDGET( tmpdoc->view ) );
			}
			msg_queue_check(0); /* call myself again, there may have been multiple files */
			if (started_by_gtk_timeout) {
				if (msg_queue.file_error_list) {
					gchar *message, *tmp;
					tmp = stringlist_to_string(msg_queue.file_error_list, "\n");
					free_stringlist(msg_queue.file_error_list);
					msg_queue.file_error_list = NULL;
					message = g_strconcat(_("These files were not opened:\n"), tmp, NULL);
					g_free(tmp);
					warning_dialog(BFWIN(main_v->bfwinlist->data)->main_window,_("Unable to open file(s)\n"), message);
					g_free(message);
				}
/*				gtk_notebook_set_page(GTK_NOTEBOOK(main_v->notebook),g_list_length(main_v->documentlist) - 1);
				notebook_changed(-1);*/
			}
		} else if (msgp.mtype == MSG_QUEUE_OPENPROJECT) {
			GList *lastlist = g_list_last(main_v->bfwinlist);
			DEBUG_MSG("msg_queue_check, a project %s is received\n", msgp.mtext);
			project_open_from_file(BFWIN(lastlist->data), msgp.mtext);
			msg_queue_check(0);	/* call myself again, there may have been multiple projects */
		} else if (msgp.mtype == MSG_QUEUE_OPENNEWWIN) {
			/* now check if this is indeed send by another process
			if the message queue was dead during the startup of this process,
			it might be started by this very process */
			int otherpid = atoi(msgp.mtext);
			DEBUG_MSG("msg_queue_check, a new window is requested by PID=%d\n",otherpid);
			if (otherpid != (int) getpid()) {
				DEBUG_MSG("msg_queue_check, the PID is not ours, opening new window\n");
				gui_new_window(NULL, NULL);
			}
		} else if (msgp.mtype == MSG_QUEUE_SETLINE) {
			msg_queue.linenumber = strtoul(msgp.mtext, NULL,10);
		}
#ifdef DEBUG
		 else {
		 	DEBUG_MSG("msg_queue_check, unknown message queue type %ld\n", msgp.mtype);
		 }
#endif
		
	} else {
#ifdef MSG_QUEUE_DEBUG
		DEBUG_MSG("msg_queue_check, found errno(%d)=%s\n", errno, g_strerror(errno));
#endif
	/*
	43 = Identifier removed
	*/
		if (errno == 22 || errno == 43) {
			DEBUG_MSG("msg_queue_check, re-opening message queue ?!?!?\n");
			/* the msg_queue was removed !?!?! */
			if (msg_queue_open()) {
				DEBUG_MSG("msg_queue_check, another process has opened the message_queue, stopping server\n");
				msg_queue.server = FALSE;
				return FALSE;
			}
		}
	}
	return TRUE;
}

/**
 * msg_queue_send_names:
 *
 *
 * returns FALSE if we never received a keepalive, so the server process seems to be non-responsive
 */
static gboolean msg_queue_send_names(gint send_with_id, GList * names, gboolean received_keepalive)
{
	struct msgbuf {
		long mtype;
		char mtext[MSQ_QUEUE_SIZE];
	} msgp;
	gint success = 1, check_keepalive_cnt = 0, send_failure_cnt = 0;
	GList *tmplist;
	
	/* we have a message queue now, opened by another bluefish process */
	msgp.mtype = send_with_id;
	tmplist = g_list_first(names);
	while (tmplist && success) {
		gint retval;
		gint len = strlen((gchar *) tmplist->data);

		/* we start with checking for keepalives */
		if (!received_keepalive) {
			if (msg_queue_check_alive(TRUE)) {
				received_keepalive = TRUE;
				DEBUG_MSG("msg_queue_send_files, received keepalive\n");
			} else {
				check_keepalive_cnt++;
				DEBUG_MSG("msg_queue_send_files, no keepalive (try %d)\n", check_keepalive_cnt);
			}
		}
		
		if (len < MSQ_QUEUE_SIZE - 1) {
			strncpy(msgp.mtext, (gchar *) tmplist->data, MSQ_QUEUE_SIZE - 1);
			retval = msgsnd(msg_queue.msgid, (void *) &msgp, MSQ_QUEUE_SIZE * sizeof(char), IPC_NOWAIT);
			if (retval == -1) {
				DEBUG_MSG("msg_queue_send_files, failed sending, errno=%d\n", errno);
				if (errno == EAGAIN) { /* EAGAIN = 11 */
					static struct timespec const req = { 0, MSG_QUEUE_PER_DOCUMENT_TIMEOUT};
					static struct timespec rem;
					nanosleep(&req, &rem);
					send_failure_cnt++;
				} else {
					DEBUG_MSG("msg_queue_send_files, failing to send, errno=%d, aborting\n", errno);
					success = 0;
				}
			} else {
				if (!received_keepalive) {
					/* if we fill the message queue with loads of data, the server 
					   process doesn't even get a chance of reply-ing. So as long as we 
					   don't know a thing about it, we give it some time and check for
					   a reply often */
					if (msg_queue_check_alive(TRUE)) {
						received_keepalive = TRUE;
						DEBUG_MSG("msg_queue_send_files, received keepalive\n");
					} else {
						check_keepalive_cnt++;
						DEBUG_MSG("msg_queue_send_files, no keepalive (try %d)\n", check_keepalive_cnt);
					}
				}
				DEBUG_MSG("msg_queue_send_files, sending %s succeeded\n", (gchar *) tmplist->data);
				send_failure_cnt = 0;
				tmplist = g_list_next(tmplist);
			}
		} else {
			DEBUG_MSG("msg_queue_send_files, failed sending, length increased message size\n");
			success = 0;
		}
		if ((check_keepalive_cnt > 5) || (send_failure_cnt > 60)) {
			DEBUG_MSG
				("msg_queue_send_files, to many tries, check_keepalive_cnt=%d, send_failure_cnt=%d\n",
				 check_keepalive_cnt, send_failure_cnt);
			success = 0;
		}
	}
	if (success) {
		DEBUG_MSG
			("msg_queue_send_files, sending filenames complete and successfull, received_keepalive=%d\n",
			 received_keepalive);
/*		/ * all filenames send to other process, test if it is alive * /
		if (received_keepalive) {
			exit(0);
		} else {
			/ * the other process should have enough time to check the queue * /
			/ * the macro is in milliseconds, usleep is microseconds * /
			if (msg_queue_check_alive(TRUE)) {
				/ * we did receive a keep alive message! * /
				exit(0);
			}
		}*/
		return received_keepalive;
	}
	return FALSE;
}

static gboolean msg_queue_send_files(GList * filenames, gboolean received_keepalive) {
	return msg_queue_send_names(MSG_QUEUE_OPENFILE, filenames, received_keepalive);
}

static gboolean msg_queue_send_projects(GList * filenames, gboolean received_keepalive) {
	return msg_queue_send_names(MSG_QUEUE_OPENPROJECT, filenames, received_keepalive);
}

/**
 * msg_queue_request_alive:
 *
 * sends a message of type MSG_QUEUE_ASK_ALIVE to the already existing queue
 * to check if the queue is alive
 */
static void msg_queue_request_alive(void)
{
	gboolean ask_alive;
	struct small_msgbuf {
		long mtype;
		char mtext[MSQ_QUEUE_SMALL_SIZE];
	} small_msgp;
	gchar *pid_string = g_strdup_printf("%d", (int) getpid());

	DEBUG_MSG("msg_queue_request_alive, asking for keepalive, string %s\n", pid_string);
	small_msgp.mtype = MSG_QUEUE_ASK_ALIVE;
	strncpy(small_msgp.mtext, pid_string, MSQ_QUEUE_SMALL_SIZE - 1);
	ask_alive =
		msgsnd(msg_queue.msgid, (void *) &small_msgp, MSQ_QUEUE_SMALL_SIZE * sizeof(char),
			   IPC_NOWAIT);
	g_free(pid_string);
	if (ask_alive == -1) {
		if (errno == 11) {
			/* the resource is temporary unavailable - perhaps the queue is full, this could mean a very busy
			message queue or a dead server */
			struct msqid_ds msg_stat;
			gint timediff;

			/* check the last time a process listened to the queue */
			msgctl(msg_queue.msgid, IPC_STAT, &msg_stat);
			timediff = time(NULL) - msg_stat.msg_rtime;
			if (timediff > 2) {
				DEBUG_MSG("msg_queue_request_alive, more then 2 seconds no reads on message_queue, timediff=%d, deleting queue\n", timediff);
				
			}
		}
		DEBUG_MSG("msg_queue_request_alive, errno=%d, error=%s\n", errno, g_strerror(errno));
		msg_queue.functional = FALSE;
	}
}

static void msg_queue_send_new_window(void) {
	int retval;
	struct msgbuf {
		long mtype;
		char mtext[MSQ_QUEUE_SMALL_SIZE];
	} small_msgp;
	/* perhaps we should first check if the queue is alive */
	gchar *pid_string = g_strdup_printf("%d", (int) getpid());
	DEBUG_MSG("msg_queue_send_new_window, requesting new window using our PID %s!\n",pid_string);
	small_msgp.mtype = MSG_QUEUE_OPENNEWWIN;
	strncpy(small_msgp.mtext, pid_string, MSQ_QUEUE_SMALL_SIZE - 1);
	retval = msgsnd(msg_queue.msgid,(void *) &small_msgp, MSQ_QUEUE_SMALL_SIZE * sizeof(char),IPC_NOWAIT);
	/* if (retval == -1) */
		/* hmm an error, we have to do some error handling here */
}

static void msg_queue_send_line_number(gint linenumber) {
	int retval;
	struct msgbuf {
		long mtype;
		char mtext[MSQ_QUEUE_SMALL_SIZE];
	} small_msgp;
	/* perhaps we should first check if the queue is alive */
	gchar *linestr = g_strdup_printf("%d", linenumber);
	DEBUG_MSG("msg_queue_send_line_number, requesting line number = %s!\n",linestr);
	small_msgp.mtype = MSG_QUEUE_SETLINE;
	strncpy(small_msgp.mtext, linestr, MSQ_QUEUE_SMALL_SIZE - 1);
	retval = msgsnd(msg_queue.msgid,(void *) &small_msgp, MSQ_QUEUE_SMALL_SIZE * sizeof(char),IPC_NOWAIT);
}

/*
	static struct timespec const req = { 0, 200000000};
	static struct timespec rem;
	nanosleep(&req, &rem);
*/
void msg_queue_start(GList * filenames, GList *projectfiles, gint linenumber, gboolean open_new_window) {
	gboolean received_keepalive = FALSE;
	gboolean queue_already_open;

	DEBUG_MSG("msg_queue_start, open message queue\n");
	queue_already_open = msg_queue_open();
	if (queue_already_open && msg_queue.functional) {
		msg_queue_request_alive();

		if (linenumber < 0) {
			linenumber = -1;
		}
		msg_queue_send_line_number(linenumber);

		if (open_new_window) {
			msg_queue_send_new_window();
		}
		/* if we have filenames to open, we start sending them now, else we just check if we have to be master or not */
		if (filenames || projectfiles) {
			if (filenames) {
				received_keepalive = msg_queue_send_files(filenames, received_keepalive);
			}
			if (projectfiles) {
				received_keepalive = msg_queue_send_projects(projectfiles, received_keepalive);
			}
			DEBUG_MSG("msg_queue_start, after sending files and projects, keepalive=%d\n",received_keepalive);
		}

		if (!received_keepalive) {
			gint check_keepalive_cnt = 0;
			/* if the message queue is still open and the process listening is killed
			   we should be the server process --> we have to check if the process is still running */
			while (!received_keepalive && check_keepalive_cnt < 10) {
				DEBUG_MSG("msg_queue_start, no keepalive yet, check_keepalive_cnt=%d\n", check_keepalive_cnt);
				if (msg_queue_check_alive(TRUE)) {
					received_keepalive = TRUE;
				}
				check_keepalive_cnt++;
			}
			if ((filenames || projectfiles || open_new_window) && received_keepalive) {
				DEBUG_MSG("msg_queue_start, we did send all our messages to an active queue, exiting!\n");
				exit(0);
			}
		} else {
			DEBUG_MSG("msg_queue_start, we did send all our messages to an active queue, exiting!\n");
			exit(0);
		}
	}

	/* if (queue_already_open) */
	/* if we opened the queue, or we did not get a keepalive */
	if (msg_queue.functional
		&& (!queue_already_open || (queue_already_open && !received_keepalive))) {
		msg_queue.server = TRUE;
		DEBUG_MSG
			("msg_queue_start, we opened the queue, or we didn't get a keepalive, we will be server!\n");
		gtk_timeout_add(MSQ_QUEUE_CHECK_TIME, (GtkFunction)msg_queue_check, GINT_TO_POINTER(1));
	} else {
		DEBUG_MSG("msg_queue_start, we didn't open the queue, and we received a keepalive, further ignoring the mssage queue\n");
	}
}

void msg_queue_cleanup(void)
{
	if (msg_queue.functional && msg_queue.server) {
		DEBUG_MSG("msg_queue_cleanup, removing msg_queue()\n");
		msgctl(msg_queue.msgid, IPC_RMID, NULL);
	}
}

#endif /* WITH_MSG_QUEUE */
