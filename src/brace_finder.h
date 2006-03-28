#ifndef __BRACE_FINDER_H_
#define _BRACE_FINDER_H_

enum {
BR_MOVE_IF_FOUND = 1<<0,
BR_FIND_FORWARD = 1<<1,
BR_FIND_BACKWARD = 1<<2,
BR_AUTO_FIND = 1<<3
};

enum {
BR_RET_FOUND = 1<<0,
BR_RET_NOT_FOUND =1 <<1,
BR_RET_IN_COMMENT =1<<2,
BR_RET_IN_SELECTION =1<<3,
BR_RET_FOUND_RIGHT_DOLLAR =1<<4,
BR_RET_FOUND_LEFT_BRACE =1<<5,
BR_RET_FOUND_RIGHT_BRACE =1<<6,
BR_RET_NOOP = 1<<7,
BR_RET_MISS_MID_BRACE = 1<<8,
BR_RET_FOUND_LEFT_DOLLAR =1<<9,
BR_RET_MOVED_LEFT =1<<10,
BR_RET_MOVED_RIGHT =1<<11,
BR_RET_FOUND_WITH_LIMIT_O =1<<12
};

guint16 brace_finder(GtkTextBuffer *buffer, Tbracefinder *brfinder, gint opt, gint limit);

#define VALID_LEFT_BRACE(Lch) ( (Lch == 123) || (Lch == 91) || (Lch==40) )
#define VALID_RIGHT_BRACE(Lch) ( (Lch == 125) || (Lch==93) || (Lch==41) )
#define VALID_LEFT_RIGHT_BRACE(Lch) ( VALID_LEFT_BRACE(Lch) || VALID_RIGHT_BRACE(Lch) )
#define VALID_BRACE(Lch) ( VALID_LEFT_RIGHT_BRACE(Lch) || (Lch == 36) )

#endif /* __BRACE_FINDER_H_ */
