/* $Id: snooper.h 627 2007-03-30 17:09:14Z kyanh $ */

/* Winefish LaTeX Editor
 * 
 * Completion support
 *
 * Copyright (c) 2005 KyAnh <kyanh@o2.pl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __SNOOPER_H_
#define __SNOOPER_H_

#include "config.h"

#ifdef SNOOPER2

#include "snooper2.h"

#else /* SNOOPER2 */

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

void snooper_install(void);

#endif /* SNOOPER2 */

#endif /* __SNOOPER_H_ */
