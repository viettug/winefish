#ifndef __BRACE_FINDER_H_
#define _BRACE_FINDER_H_

enum {
BR_MOVE_IF_FOUND = 1<<0,
BR_FIND_FORWARD = 1<<1,
BR_FIND_BACKWARD = 1<<2,
BR_HILIGHT_IF_FOUND = 1<<3
};

enum {
BR_RET_FOUND = 1<<0,
BR_RET_NOT_FOUND =1 <<1,
BR_RET_IS_ESCAPED = 1<<2,
BR_RET_IN_COMMENT =1<<3,
BR_RET_IN_SELECTION =1<<4,
BR_RET_WRONG_OPERATION = 1<<5,
BR_RET_FOUND_DOLLAR_EXTRA =1<<6,
BR_RET_FOUND_LEFT_BRACE =1 <<7,
BR_RET_FOUND_RIGHT_BRACE =1 <<8,
BR_RET_NOOPS = 1 <<9
};

gint brace_finder(GtkTextBuffer *buffer, gpointer *brfinder, gint opt, gint limit);

#define VALID_LEFT_BRACE(Lch) ( (Lch == 123) || (Lch == 91) || (Lch==40) )
#define VALID_RIGHT_BRACE(Lch) ( (Lch == 125) || (Lch==93) || (Lch==41) )
#define VALID_LEFT_RIGHT_BRACE(Lch) ( VALID_LEFT_BRACE(Lch) || VALID_RIGHT_BRACE(Lch) )
#define VALID_BRACE(Lch) ( (Lch == 36) || VALID_LEFT_RIGHT_BRACE(Lch) )

#endif /* __BRACE_FINDER_H_ */
