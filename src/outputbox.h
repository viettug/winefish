/* $Id$ */

/* void outputbox_init(Tbfwin *bfwin,GtkWidget *vbox); */

#ifndef __OUTPUTBOX_H_
#define __OUTPUTBOX_H_

#include "outputbox_cfg.h"

/*
 in order to HIDE/SHOW the output box, we must export Toutputbox, so that
 gui.c can regcornizes this type...
*/
#define NUM_MATCH 30

/* Status of executing a tool
 0: continue fetching data from channel;
 1: fetching; return [ do nothing ] in continue_execute
 2: 'stop' signal from user;
 3: stopped;
 4: ready to start;
*/
enum {
	OB_GO_FETCHING = 1 << 0, /* 0 */
	OB_IS_FETCHING = 1 << 1,
	OB_STOP_REQUEST  = 1 << 2, /* 2 */
	OB_IS_STOPPED = 1 << 3,
	OB_ERROR =  1<< 4,
	OB_IS_READY = 1 << 5/* highest value */
};

enum {
	OB_NEED_SAVE_FILE = 1<<0, /* need save file */
	OB_SHOW_ALL_OUTPUT = 1<<1 /* show all output */
};

typedef struct
{
	gchar *command;
	gchar *pattern;
	gint file_subpat;
	gint line_subpat;
	gint output_subpat;
	gint show_all_output;
	regmatch_t pmatch[ NUM_MATCH ];
	regex_t preg;
}
Toutput_def;

#ifdef __BF_BACKEND__
typedef struct
{
	gint refcount;
	/* Tbfwin *bfwin; */
	/* const gchar *formatstring; */
	gchar *commandstring;

	gchar *securedir; /* if we need any local temporary files for input or output we'll use fifo's */
	gchar *fifo_in;
	gchar *fifo_out;
	gchar *tmp_in;
	gchar *tmp_out;
	gchar *inplace;
	gboolean pipe_in;
	gboolean pipe_out;
	
	gboolean include_stderr;
	
	GIOChannel* channel_in;
	gchar *buffer_out;
	gchar *buffer_out_position;
	GIOChannel* channel_out;
	
	GIOFunc channel_out_lcb;
	gpointer channel_out_data;
	
	GPid child_pid;
	
	gpointer data;
	gpointer ob; /* kyanh, added, 20060124 */
}
Texternalp;
#define EXTERNALP(var) ((Texternalp *)(var))
#endif

typedef struct
{
	gint OB_FETCHING;
	GtkListStore *lstore;
	GtkWidget *lview; /* ~> GtkTreeView */
	/* GtkWidget *hbox; *//* handle => add close button */
	gint page_number;
	Toutput_def *def;
	Tbfwin *bfwin;
	gchar *basepath_cached;
	gint basepath_cached_color;
#ifdef __KA_BACKEND__
	/* kyanh, added, 20050220 */
	guint pollID;
	gchar *retfile;
	gint pid; /* kyanh, added, 20050226, pid of command */
	GIOChannel *io_channel;
#endif
#ifdef __BF_BACKEND__
	Texternalp *handle;
#endif
}
Toutputbox;

#define OUTPUTBOX(var) ((Toutputbox *)(var))

void outputbox(
	/*ob frontend */
	Tbfwin *bfwin, gpointer *ob, const gchar *title,
	/* output */
	gchar *pattern, gint file_subpat, gint line_subpat, gint output_subpat, gchar *command, gint show_all_output );
void outputbox_stop (Toutputbox *ob);
void outputbox_message( Toutputbox *ob, const char *string, const char *markup );

Toutputbox *outputbox_new_box( Tbfwin *bfwin, const gchar *title );
Toutputbox *outputbox_get_box( Tbfwin *bfwin, guint page);
void outputbox_set_status( Toutputbox *ob, gboolean status, gboolean force);

#endif /* __OUTPUTBOX_H_ */
