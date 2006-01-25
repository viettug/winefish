/* $Id: html.h,v 1.1.1.1 2005/06/29 11:03:26 kyanh Exp $ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * html.h - menu/toolbar callback prototypes
 *
 * Copyright (C) 1998 Olivier Sessink and Chris Mazuc
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

#ifndef __HTML_H_
#define __HTML_H_

/* void insert_char_cb(Tbfwin* bfwin,guint callback_action, GtkWidget *widget); */
void general_html_menu_cb(Tbfwin* bfwin,guint callback_action, GtkWidget *widget);

#endif							/* __HTML_H_ */
