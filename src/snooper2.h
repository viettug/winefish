/* $Id$ */

#ifndef __SNOOPER2_H__
#define __SNOOPER2_H__

#include "config.h"

#ifdef SNOOPER2


#define SNOOPER_IS_AZ(var) ( ( ( GDK_a <= var ) && ( (GDK_z >= var ) ) ) || ( ( GDK_Z >= var ) && ( ( GDK_A <= var ) ) ) )

#define SNOOPER_IS_AZ09(var) ( SNOOPER_IS_AZ(var) || ( ( GDK_0 <= var ) && ( (GDK_9 >= var ) ) ) )

#define SNOOPER_IS_LBRACE(var) ( var == GDK_braceleft )

#define SNOOPER_CONTROL_MASKS ( GDK_CONTROL_MASK | GDK_MOD1_MASK )

#define SNOOPER_IS_KEYSEQ(var) ( var->state & SNOOPER_CONTROL_MASKS )

#define SNOOPER_A_CHARS(var) (  var && ( ( SNOOPER_IS_AZ(var->keyval) ) || SNOOPER_IS_LBRACE(var->keyval) ) )

#define SNOOPER_VALID_WIDGET(var) ( (GTK_IS_TEXT_VIEW(var) || GTK_IS_WINDOW(var) ) )

/** completion stuff **/
#define SNOOPER_COMPLETION_ON(bfwin) ( bfwin->completion && ( COMPLETION(bfwin->completion)->show > COMPLETION_WINDOW_HIDE ) )

#define SNOOPER_COMPLETION_MOVE(var) ( var == GDK_Up || var== GDK_Down || var == GDK_Page_Up || var == GDK_Page_Down)

#define SNOOPER_COMPLETION_ACCEPT(var) ( var == GDK_Return )
#define SNOOPER_COMPLETION_ESCAPE(var) ( var == GDK_Escape )
#define SNOOPER_COMPLETION_DELETE(var) ( var == GDK_Delete )

#define SNOOPER_COMPLETION_MOVE_TYPE(var) ( (var  == GDK_Up) ? COMPLETION_WINDOW_UP : ( (var == GDK_Down) ? COMPLETION_WINDOW_DOWN : ( ( var == GDK_Page_Down ) ? COMPLETION_WINDOW_PAGE_DOWN : COMPLETION_WINDOW_PAGE_UP ) ) )

enum {
	COMPLETION_WINDOW_INIT = 1 << 0, /* require `intitalize' the window */
	COMPLETION_WINDOW_HIDE =  1 << 1, /* hide window */
	COMPLETION_WINDOW_SHOW = 1 << 2, /* show winow */
	COMPLETION_WINDOW_UP = 1 << 3, /* select one item up */
	COMPLETION_WINDOW_PAGE_UP =  1 << 4, /* select many items up */
	COMPLETION_WINDOW_DOWN = 1 << 5, /* select one item down */
	COMPLETION_WINDOW_PAGE_DOWN =  1 << 6, /* select many items down */
	COMPLETION_WINDOW_ACCEPT=  1 << 7, /* accept one item */
	COMPLETION_DELETE =  1 << 8 , /* delete an item */
};

enum {
	SNOOPER_HALF_SEQ = 1<<1,
	SNOOPER_CANCEL_RELEASE_EVENT = 1<<2,
	SNOOPER_HAS_EXCUTING_FUNC = 1<<3,
	SNOOPER_ACTIVE = 1<<4
};

#define STAT_NAME(var) ( ( var & SNOOPER_HAS_EXCUTING_FUNC) ? "HAS_EXCUTED" : ( ( var & SNOOPER_HALF_SEQ) ? "HALF_SEQ" : ( (var & SNOOPER_CANCEL_RELEASE_EVENT)? "CANCEL_RELEASE" : "EMPTY" ) ) )

typedef gint (*FUNCTION)(GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt);

typedef struct {
	gint stat;
	GdkEvent *last_event;
	GdkEvent *last_seq;
} Tsnooper;
#define SNOOPER(var) ( (Tsnooper*)var)

typedef struct {
	FUNCTION exec;
	gint data;
	/* other stuff (function description, for e.g.) */
} Tfunc;
#define FUNC(var) ( (Tfunc*)var)

void snooper_install(Tbfwin *bfwin);

#endif /* SNOOPER2 */
#endif /* __SNOOPER2_H__ */
