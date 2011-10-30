/* $Id: snr2.h 631 2007-03-30 17:14:06Z kyanh $ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * snr2.h - the search 'n replace rewrite prototypes
 *
 * Copyright (C) 2000-2002 Olivier Sessink
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

#ifndef __SNR2_H_
#define __SNR2_H_

typedef struct {
	/* span of characters */
	gint start;
	gint end;

	/* span of byte-data in buffer */
	gint bstart;
	gint bend;
	
	/* these data are only used (and alloc'ed) if want_submatches is set in the search backend,
	they should be freed by the calling function! */
	regmatch_t *pmatch;
	gint nmatch;
} Tsearch_result;

void snr2_init(Tbfwin *bfwin);

void snr2_run_extern_replace(Tdocument *doc, gchar *search_pattern, gint region,
							gint matchtype, gint is_case_sens, gchar *replace_pattern,
							gboolean store_as_last_snr2);
Tsearch_result doc_search_run_extern(Tdocument *doc, gchar *search_pattern, gint matchtype, gint is_case_sens);
void search_from_selection(Tbfwin *bfwin);
void search_cb(GtkWidget *widget, Tbfwin *bfwin);
void replace_again_cb(GtkWidget *widget, Tbfwin *bfwin);
void search_again_cb(GtkWidget *widget, Tbfwin *bfwin);
void replace_cb(GtkWidget *widget, Tbfwin *bfwin);
void update_filenames_in_file(Tdocument *doc, gchar *oldfilename, gchar *newfilename, gint doc_has_newfilename);
/* kyanh, removed, 20050220
void update_encoding_meta_in_file(Tdocument *doc, gchar *encoding);
*/
void snr2_cleanup(Tbfwin *bfwin);
#endif							/* __SNR_H_ */
