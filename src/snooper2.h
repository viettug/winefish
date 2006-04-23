/* $Id$ */

#ifndef __SNOOPER2_H__
#define __SNOOPER2_H__

#include "config.h"

#ifdef SNOOPER2

#define SNOOPER_CONTROL_MASKS ( GDK_CONTROL_MASK | GDK_MOD1_MASK )
#define SNOOPER_SHOULD_CAPTURE(var) ( ( ( GDK_a <= var ) && ( (GDK_z >= var ) ) ) || ( ( GDK_Z >= var ) && ( ( GDK_A <= var ) ) )  || ( ( GDK_0 <= var ) && ( (GDK_9 >= var ) ) ) || ( var == GDK_percent) || (var == GDK_space) )
#define SNOOPER_IS_KEYSEQ(var) ( (var->state & SNOOPER_CONTROL_MASKS) && SNOOPER_SHOULD_CAPTURE( var->keyval ) )
#define SNOOPER_VALID_WIDGET(var) ( (GTK_IS_TEXT_VIEW(var) || GTK_IS_WINDOW(var) ) )

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
	COMPLETION_FIRST_CALL =  1 << 9, /* first press CTRL+Space in the period */
	COMPLETION_AUTO_CALL = 1<< 10 /* auto call */
};

enum {
	SNOOPER_HALF_SEQ = 1<<0,
	SNOOPER_FULL_SEQ  = 1<<1,
	SNOOPER_CANCEL_RELEASE_EVENT = 1<<2,
};

enum {
	FUNC_ANY = 1<<0,
	FUNC_TEXT_VIEW = 1<<1
};
#define FUNC_VALID_TYPE(type,widget) ( (type & FUNC_ANY) ||  ( (type & FUNC_TEXT_VIEW ) && GTK_IS_TEXT_VIEW(widget) ) )

typedef gint (*FUNCTION)(GtkWidget *widget, Tbfwin *bfwin);

typedef struct {
	guint id;
	gint stat;
	GdkEvent *last_event;
} Tsnooper;
#define SNOOPER(var) ( (Tsnooper*)var)

typedef struct {
	FUNCTION exec;
	gint type;
	/* other stuff (function description, for e.g.) */
} Tfunc;
#define FUNC(var) ( (Tfunc*)var)

void snooper_install(Tbfwin *bfwin);

#endif /* SNOOPER2 */
#endif /* __SNOOPER2_H__ */
