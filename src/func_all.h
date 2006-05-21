#ifndef __FUNC_ALL_H__
#define __FUNC_ALL_H__

#include "func_complete.h"
#include "func_delline.h"
#include "func_grep.h"
#include "func_shift.h"
#include "func_comment.h"
#include "func_indent.h"
#include "func_move.h"
#include "func_zoom.h"

#include "bfspell.h"

enum {
	FUNC_ANY = 1<<0,
	FUNC_TEXT_VIEW = 1<<1,
	FUNC_FROM_SNOOPER = 1<<2,
	FUNC_FROM_OTHER =1 <<3,
	FUNC_VALUE_0 = 1<<4,
	FUNC_VALUE_1 = 1<<5,
	FUNC_VALUE_2 = 1<<6,
};

#define FUNC_VALUE_  10

#define FUNC_VALID_TYPE(type,widget) ( (type & FUNC_ANY) ||  ( (type & FUNC_TEXT_VIEW ) && GTK_IS_TEXT_VIEW(widget) ) )

#endif /* __FUNC_ALL_H__ */
