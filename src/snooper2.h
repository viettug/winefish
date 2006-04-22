/* $Id$ */

#ifndef __SNOOPER_H__
#define __SNOOPER_H__

#include "config.h"

#ifdef SNOOPER2

#define SNOOPER_CONTROL_MASKS ( GDK_CONTROL_MASK | GDK_MOD1_MASK )
#define SNOOPER_SHOULD_CAPTURE(var) ( ( ( GDK_a <= var ) && ( (GDK_z >= var ) ) ) || ( ( GDK_Z >= var ) && ( ( GDK_A <= var ) ) )  || ( ( GDK_0 <= var ) && ( (GDK_9 >= var ) ) ) || ( var == GDK_percent) || (var == GDK_space) )
#define SNOOPER_IS_KEYSEQ(var) ( (var->state & SNOOPER_CONTROL_MASKS) && SNOOPER_SHOULD_CAPTURE( var->keyval ) )

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

typedef gint (*FUNCTION)(gpointer data);

typedef struct {
	guint id;
	gint stat;
	GdkEvent *last_event;
	GHashTable *key_hashtable;
	GHashTable *func_hashtable;
} Tsnooper;
#define SNOOPER(var) ( (Tsnooper*)var)

typedef struct {
	FUNCTION exec;
	gint type;
	/* other stuff (function description, for e.g.) */
} Tfunc;
#define FUNC(var) ( (Tfunc*)var)

void snooper_install(void);

#endif /* SNOOPER2 */
#endif /* __SNOOPER_H__ */
