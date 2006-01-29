#ifndef __BRACE_FINDER_H_
#define _BRACE_FINDER_H_

enum {
BR_MOVE_IF_FOUND = 1<<0,
BR_FIND_FORWARD = 1<<1,
BR_FIND_BACKWARD = 1<<2
};

enum {
BR_RET_FOUND,
BR_RET_NOT_FOUND,
BR_RET_IS_ESCAPED,
BR_RET_IN_COMMENT,
BR_RET_TOO_LONG,
BR_RET_IN_SELECTION
};

gint brace_finder(GtkTextBuffer *buffer, gint opt);

#endif /* __BRACE_FINDER_H_ */
