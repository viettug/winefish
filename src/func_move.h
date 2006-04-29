#ifndef __FUNC_MOVE_H__
#define __FUNC_MOVE_H__

#include "func_all.h"

enum {
	FUNC_MOVE_LINE_UP,
	FUNC_MOVE_LINE_DOWN,
	FUNC_MOVE_PAGE_UP,
	FUNC_MOVE_PAGE_DOWN,
	FUNC_MOVE_LINE_END,
	FUNC_MOVE_LINE_START,
	FUNC_MOVE_END,
	FUNC_MOVE_START,
	FUNC_MOVE_CHAR_LEFT,
	FUNC_MOVE_CHAR_RIGHT,
	FUNC_MOVE_WORD_LEFT,
	FUNC_MOVE_WORD_RIGHT,
};

gint func_move(GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt);

#endif /* __FUNC_MOVE_H__ */
