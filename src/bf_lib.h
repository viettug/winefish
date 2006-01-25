/* $Id: bf_lib.h,v 1.1.1.1 2005/06/29 11:03:16 kyanh Exp $ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * bf_lib.h - non-GUI general functions
 *
 * Copyright (C) 2000-2004 Olivier Sessink
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

#ifndef __BF_LIB_H_
#define __BF_LIB_H_

typedef struct {
	gint my_int;
	gchar *my_char;
} Tconvert_table;

typedef enum { 
	tcc2i_full_match = 0,
	tcc2i_firstchar,
	tcc2i_mycharlen,
	tcc2i_full_match_gettext
} Ttcc2i_mode;

gchar *get_filename_on_disk_encoding(const gchar *utf8filename);
gchar *get_utf8filename_from_on_disk_encoding(const gchar *encodedname);
gboolean string_is_color(const gchar *color);
gchar *filemode_to_string(mode_t statmode);
gchar *return_root_with_protocol(const gchar *url);
void pointer_switch_addresses(gpointer *a, gpointer *b);
void list_switch_order(GList *first, GList *second);
gboolean file_copy(gchar *source, gchar *dest);
gint find_common_prefixlen_in_stringlist(GList *stringlist);
gboolean append_string_to_file(gchar *filename, gchar *string);
gint table_convert_char2int(Tconvert_table *table, const gchar *my_char, Ttcc2i_mode mode);
gchar *table_convert_int2char(Tconvert_table *table, gint my_int);
gchar *expand_string(const gchar *string, const char specialchar, Tconvert_table *table);
gchar *unexpand_string(const gchar *original, const char specialchar, Tconvert_table *table);
gchar *replace_string_printflike(const gchar *string, Tconvert_table *table);
gchar *unescape_string(const gchar *original, gboolean escape_colon);
gchar *escape_string(const gchar *original, gboolean escape_colon);
Tconvert_table *new_convert_table(gint size, gboolean fill_standardescape);
void free_convert_table(Tconvert_table *tct);
/* kyanh, added, 20050223 */
gchar *convert_command(Tbfwin *bfwin, const gchar *command);

#define utf8_byteoffset_to_charsoffset(string,byteoffset) g_utf8_pointer_to_offset(string, string+byteoffset)
/*glong utf8_byteoffset_to_charsoffset(gchar *string, glong byteoffset);*/
void utf8_offset_cache_reset();
guint utf8_byteoffset_to_charsoffset_cached(gchar *string, glong byteoffset);

gchar *strip_any_whitespace(gchar *string);
gchar *trunc_on_char(gchar * string, gchar which_char);
gchar *strip_common_path(char *to_filename, char *from_filename);
gchar *most_efficient_filename(gchar *filename);
gchar *create_relative_link_to(gchar * current_filepath, gchar * link_to_filepath);
gchar *create_full_path(const gchar * filename, const gchar *basedir);
gchar *ending_slash(const gchar *dirname);
gchar *path_get_dirname_with_ending_slash(const gchar *filename);
gboolean file_exists_and_readable(const gchar * filename);
gchar *return_first_existing_filename(const gchar * filename, ...);
gboolean filename_test_extensions(gchar **extensions, gchar *filename);
gchar *bf_str_repeat(const gchar * str, gint number_of);
gint get_int_from_string(gchar *string);
gchar *create_secure_dir_return_filename();
void remove_secure_dir_and_filename(gchar *filename);
/*gchar *buf_replace_char(gchar *buf, gint len, gchar srcchar, gchar destchar);*/
void wordcount(gchar *text, guint *chars, guint *lines, guint *words);
GList *glist_from_gslist(GSList *src);
#endif /* __BF_LIB_H_ */
