/* $Id: bookmark.h,v 1.1.1.1 2005/06/29 11:03:17 kyanh Exp $ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor) - bookmarks
 *
 * Copyright (C) 2003 Oskar Swida
 * modifications (C) 2004 Olivier Sessink
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
#ifndef __BOOKMARK_H__
#define __BOOKMARK_H__

void bmark_store_all(Tbfwin *bfwin);
GtkWidget *bmark_gui(Tbfwin *bfwin); /* used in gui.c to build the bookmark panel */
void bmark_init(void); /* only used once from bluefish.c */
void bmark_reload(Tbfwin *bfwin);
void bmark_set_store(Tbfwin *bfwin);
void bmark_clean_for_doc(Tdocument *doc); /* set bookmark's doc to NULL when closing file */ 
void bmark_set_for_doc(Tdocument *doc); /* set bookmark's doc to proper doc when opening file */ 
GHashTable *bmark_get_bookmarked_lines(Tdocument * doc, GtkTextIter *fromit, GtkTextIter *toit);
void bmark_add_extern(Tdocument *doc, gint offset, const gchar *name, const gchar *text, gboolean is_temp);
void bmark_add(Tbfwin *bfwin);
gboolean bmark_have_bookmark_at_stored_bevent(Tdocument * doc);
void bmark_store_bevent_location(Tdocument * doc, gint charoffset);
void bmark_del_at_bevent(Tdocument *doc);
void bmark_add_at_bevent(Tdocument *doc);
void bmark_del_all(Tbfwin *bfwin,gboolean ask);
void bmark_check_length(Tbfwin *bfwin,Tdocument *doc);
void bmark_cleanup(Tbfwin *bfwin);

#endif /* __BOOKMARK_H__ */
