#ifndef __FUNC_ALL_H__
#define __FUNC_ALL_H__

#include "func_complete.h"
#include "func_delline.h"
#include "func_grep.h"
#include "func_shift.h"
#include "func_comment.h"
#include "func_indent.h"

enum {
	FUNC_VALUE_0 = 1<<0,
	FUNC_VALUE_1 = 1<<1,
	FUNC_ANY = 1<<2,
	FUNC_TEXT_VIEW = 1<<3,
	FUNC_FROM_SNOOPER = 1<<4,
	FUNC_FROM_OTHER =1 <<5
};

#define FUNC_VALID_TYPE(type,widget) ( (type & FUNC_ANY) ||  ( (type & FUNC_TEXT_VIEW ) && GTK_IS_TEXT_VIEW(widget) ) )

#endif /* __FUNC_ALL_H__ */
