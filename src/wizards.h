/* $Id: wizards.h 631 2007-03-30 17:14:06Z kyanh $ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * wizards.h - wizard callback prototypes
 *
 * Copyright (C) 1998 Olivier Sessink and Chris Mazuc
 * Copyright (C) 1999-2002 Olivier Sessink
 * Modified for Winefish (C) 2005 Ky Anh <kyanh@o2.pl> 
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
 */

#ifndef __WIZARDS_H_
#define __WIZARDS_H_

#include "html_diag.h" /* Ttagpopup */

void tablewizard_dialog(Tbfwin *bfwin);
void insert_time_dialog(Tbfwin *bfwin);
void quickstart_dialog(Tbfwin *bfwin, Ttagpopup *data);
void quicklist_dialog(Tbfwin *bfwin, Ttagpopup *data);

#endif /* __WIZARDS_H_ */
