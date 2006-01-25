/* $Id: stringlist.h,v 1.1.1.1 2005/06/29 11:03:35 kyanh Exp $ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * stringlist.h - functions that deal with stringlists
 *
 * Copyright (C) 1999 Olivier Sessink and Hylke van der Schaaf
 * Copyright (C) 2000-2003 Olivier Sessink
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

#ifndef __STRINGLIST_H_
#define __STRINGLIST_H_

#include <glib.h>

/* void estrl_dialog(GList **which_list, gchar *title, gint what_list
				, gint column_num, gchar **column_titles, void (*post_dialog_func)());*/

gint count_array(gchar **array);
gchar *array_to_string(gchar **array);
gchar **string_to_array(gchar *string);
gchar **array_from_arglist(const gchar *string1, ...);
GList *list_from_arglist(gboolean allocate_strings, ...);
GList *duplicate_stringlist(GList *list, gint dup_data);
gint free_stringlist(GList * which_list);
GList *get_list(const gchar * filename, GList * which_list, gboolean is_arraylist);
GList *get_stringlist(const gchar * filename, GList * which_list);
gboolean put_stringlist_limited(gchar * filename, GList * which_list, gint maxentries);
gboolean put_stringlist(gchar * filename, GList * which_list);

gint free_arraylist(GList * which_list);
gchar **duplicate_stringarray(gchar **array);
GList *duplicate_arraylist(GList *arraylist);

/* removes a string from a stringlist if it is the same */
GList *remove_from_stringlist(GList *which_list, const gchar * string);
GList *limit_stringlist(GList *which_list, gint num_entries, gboolean keep_end);
GList *add_to_history_stringlist(GList *which_list, const gchar *string, gboolean recent_on_top, gboolean move_if_exists);
/* adds a string to a stringlist if it is not yet in there */
GList *add_to_stringlist(GList * which_list, const gchar * string);
gchar *stringlist_to_string(GList *stringlist, gchar *delimiter);

gint array_n_strings_identical(gchar **array1, gchar **array2, gboolean case_sensitive, gint testlevel);
GList *arraylist_delete_identical(GList *arraylist, gchar **compare, gint testlevel, gboolean case_sensitive);
GList *arraylist_append_identical_from_list(GList *thelist, GList *source, gchar **compare, gint testlevel, gboolean case_sensitive);
GList *arraylist_append_identical_from_file(GList *thelist, const gchar *sourcefilename, gchar **compare, gint testlevel, gboolean case_sensitive);

gboolean arraylist_value_exists(GList *arraylist, gchar **value, gint testlevel, gboolean case_sensitive);
GList *arraylist_load_new_identifiers_from_list(GList *mylist, GList *deflist, gint uniquelevel);
GList *arraylist_load_new_identifiers_from_file(GList *mylist, const gchar *fromfilename, gint uniquelevel);
#endif							/* __STRINGLIST_H_ */
