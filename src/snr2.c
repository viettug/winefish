/* $Id$ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * snr2.c - rewrite of search 'n replace functions
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
/*
 * THIS IS THE BACKEND FLOW DIAGRAM
 *
 *                              snr2_run
 *              ________________/ | \  \___________________________
 *             /      / |         |  \                   \  \      \
 *  search_bookmark  |  |         |  replace_prompt_all  |   \      \
 *     |            /   |         |         /           /     \      \
 *     |   search_all   |   _____replace_prompt_doc    /       \    replace_all
 *     |     |          |  /          /               /         \     /
 *     |     \   doc_show_result     /  replace_doc_once   replace_doc_multiple
 *     |      \                   __/                  \         /
 *     |  search_doc             /                   replace_backend
 *      \________ \             /  ___________________________/
 *               \ \           /  /
 *                 search_backend
 */
/*****************************************************/
/* #define DEBUG */

#include <gtk/gtk.h>

#include "config.h"

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>  /* _before_ regex.h for freeBSD */
#include <regex.h> 				/* regcomp() */
#include <pcre.h> 	/* pcre_compile */
#include <gdk/gdkkeysyms.h> /* GDK_Return */

#include "bluefish.h"
#include "bookmark.h" /* bmark_add_extern() */
#include "bf_lib.h"
#include "undo_redo.h" /* doc_unre_new_group */
#include "document.h"			/* doc_replace_text() */
#include "gui.h" /* switch_to_document_by_pointer() */
#include "gtk_easy.h"         /* a lot of GUI functions */
#include "snr2.h"
#include "highlight.h" /* doc_highlight_full() */
#include "stringlist.h" /* add_to_history_stringlist */

/* Updates, May 2003, by Ruben Dorta */

typedef enum { string, uppercase, lowercase } Treplace_types;
typedef enum { match_normal, match_posix, match_perl } Tmatch_types;
typedef enum { beginning, cursor, selection, opened_files } Tplace_types;

typedef struct {
	gint start;
	gint end;
	Tdocument *doc;
} Tsearch_all_result;

typedef struct {
	Tbfwin *bfwin;
	Tdocument *doc;
	Tsearch_result result;
	gint replace;
	gchar *search_pattern;
	gchar *replace_pattern;
	gint unescape;
	gint overlapping_search;
	gint prompt_before_replace;
	gint is_case_sens;
	gint replace_once;
	gint bookmark_results;
	Treplace_types replacetype_option;
	Tmatch_types matchtype_option;
	Tplace_types placetype_option;
} Tlast_snr2;

typedef struct {
	gint replace;
	Tbfwin *bfwin;
	GtkWidget *window;
	GtkWidget *search_combo;
	/*GtkWidget *search_entry;*/
	GtkWidget *search_label;
	/*GtkWidget *search_scrollbox;*/
	GtkWidget *replace_combo;
	/*GtkWidget *replace_entry;*/
	GtkWidget *replace_label;
	/*GtkWidget *replace_scrollbox;*/
	GtkWidget *subpat_help;
	GtkWidget *overlapping_search;
	GtkWidget *prompt_before_replace;
	GtkWidget *is_case_sens;
	GtkWidget *replace_once;
	GtkWidget *unescape;
	GtkWidget *replacetype_option;
	GtkWidget *matchtype_option;
	GtkWidget *placetype_option;
	GtkWidget *bookmark_results;
} Tsnr2_win;
#define LASTSNR2(var) ((Tlast_snr2 *)(var))
/***********************************************************/

void snr2_run(Tbfwin *bfwin,Tdocument *doc);

/***********************************************************/

void snr2_init(Tbfwin *bfwin) {
	Tlast_snr2 *lsnr2 = g_new0(Tlast_snr2,1);
	lsnr2->bfwin = bfwin;
	bfwin->snr2 = lsnr2;
}

void snr2_cleanup(Tbfwin *bfwin) {
	g_free(bfwin->snr2);
}

static void reset_last_snr2(Tbfwin *bfwin) {
	if (LASTSNR2(bfwin->snr2)->search_pattern) {
		g_free(LASTSNR2(bfwin->snr2)->search_pattern);
	}
	if (LASTSNR2(bfwin->snr2)->replace_pattern) {
		g_free(LASTSNR2(bfwin->snr2)->replace_pattern);
	}
	memset(LASTSNR2(bfwin->snr2), 0, sizeof(Tlast_snr2));
}

/***********************************************************/

/**
 * search_backend:
 * @bfwin: #Tbfwin* 
 * @search_pattern: #gchar* to search pattern
 * @matchtype: see #Tmatch_types
 * @is_case_sens: If the search is case sensitive, #gint
 * @buf: #gchar* to the document buffer
 * @byte_offset: #guint where in the buffer the search should start, in bytes, not characters
 * @want_submatches: #gint
 * 
 * Performs an actual search in a supplied buffer (#gchar*, aka string).
 * NOTE: If want_submatches is set, tsearch_result->pmatch should be free`ed by the calling function!
 *
 * Return value: #Tsearch_result, contains both character and byte offsets, for wide-char-compatibility. Note values for start/end are set to -1 on error.
 **/
Tsearch_result search_backend(Tbfwin *bfwin, gchar *search_pattern, Tmatch_types matchtype, gint is_case_sens, gchar *buf, guint byte_offset, gboolean want_submatches) {
	Tsearch_result returnvalue;
	int (*f) ();
	gint buflen, patlen, match, i;

	returnvalue.start = -1;
	returnvalue.end = -1;
	returnvalue.bstart = -1;
	returnvalue.bend = -1;
	returnvalue.pmatch = NULL;
	returnvalue.nmatch = 0;
	if ((!search_pattern) || (!buf)) {
		DEBUG_MSG("search_backend, search_pattern or buf is NULL\n");
		return returnvalue;
	}
	DEBUG_MSG("search_backend, starting for byte_offset=%u\n",byte_offset);
	if (matchtype == match_posix) {
		/* regex part start */
		regex_t reg_pat;
		regmatch_t *pmatch;
		gint nmatch, retval;

		retval = regcomp(&reg_pat, search_pattern, (is_case_sens ? REG_EXTENDED : REG_EXTENDED | REG_ICASE));
		DEBUG_MSG("search_backend, regcomp retval=%d\n", retval);
		if (retval != 0) {
#define ERRORSTR_SIZE 300
			gchar errorstr[ERRORSTR_SIZE], *errorstr2;
			
			regerror(retval,  &reg_pat, errorstr, ERRORSTR_SIZE);
			errorstr2 = g_strconcat(_("Regular expression error: "), errorstr, NULL);
			warning_dialog(bfwin->main_window,_("Search failed"), errorstr2);
			g_free(errorstr2);
			/* error compiling the search_pattern, returning the default result set,
			which is the 'nothing found' set */
			return returnvalue;
		}
		nmatch = (want_submatches) ? reg_pat.re_nsub+1 : 1;
		DEBUG_MSG("search_backend, expr. contains %d sub search_patterns\n", reg_pat.re_nsub );
		pmatch = g_malloc(nmatch*sizeof(regmatch_t));
		retval = regexec(&reg_pat, buf+byte_offset, nmatch, pmatch, 0);
		DEBUG_MSG("search_backend, regexec retval=%d\n", retval);
		if (retval != 0) {
			returnvalue.bstart = -1;
			returnvalue.bend = -1;
		} else {
			returnvalue.bstart = pmatch[0].rm_so + byte_offset;
			returnvalue.bend = pmatch[0].rm_eo + byte_offset;
		}
#ifdef DEBUG
		{	int i;
			for (i=0;i<nmatch;i++) {
				DEBUG_MSG("search_backend, sub search_pattern %d so=%d, eo=%d\n", i, pmatch[i].rm_so, pmatch[i].rm_eo);
			}
		}
#endif
		regfree(&reg_pat);
		if (want_submatches) {
			returnvalue.pmatch = pmatch;
			returnvalue.nmatch = nmatch;
			/* if want_submatches is set, pmatch should be 
			free`ed by the calling function! */
		} else {
			g_free(pmatch);
		}
		/* regex part end */
	} else if (matchtype == match_perl) {
		pcre *pcre_c;
		const char *err=NULL;
		int erroffset=0;
		int ovector[30];
		gint retval;
		pcre_c = pcre_compile(search_pattern, (is_case_sens ? PCRE_DOTALL|PCRE_MULTILINE : PCRE_DOTALL|PCRE_CASELESS|PCRE_MULTILINE),&err,&erroffset,NULL);
		if (err) {
			gchar *errstring;
			errstring = g_strdup_printf(_("Regular expression error: %s at offset %d"), err, erroffset);
			warning_dialog(bfwin->main_window,_("Search failed"), errstring);
			g_free(errstring);
			return returnvalue;/* error compiling the search_pattern, returning the default result set,which is the 'nothing found' set */
		}
		retval = pcre_exec(pcre_c,NULL,buf+byte_offset,strlen(buf+byte_offset),0,0,ovector,30);
		if (retval > 0) {
			returnvalue.bstart = ovector[0] + byte_offset;
			returnvalue.bend = ovector[1] + byte_offset;
		} else {
			returnvalue.bstart = -1;
			returnvalue.bend = -1;
		}
		if (want_submatches) {
			int nmatch,i;
			regmatch_t *pmatch;
			pcre_fullinfo(pcre_c, NULL, PCRE_INFO_CAPTURECOUNT, &nmatch);
			DEBUG_MSG("search_backend, nmatch=%d, retval=%d\n", nmatch, retval);
			pmatch = g_malloc((nmatch+1)*sizeof(regmatch_t));
			for (i=0;i<=nmatch;i++) { /* nmatch==1 means 1 subsearch_pattern, so 2 search_patterns in total*/
				pmatch[i].rm_so = ovector[i*2] + byte_offset;
				pmatch[i].rm_eo = ovector[i*2+1] + byte_offset;
			}
			returnvalue.pmatch = pmatch;
			returnvalue.nmatch = retval;
#ifdef DEBUG
			{	int i;
				for (i=0;i<returnvalue.nmatch;i++) {
					DEBUG_MSG("search_backend, sub search_pattern %d so=%d, eo=%d\n", i, returnvalue.pmatch[i].rm_so, returnvalue.pmatch[i].rm_eo);
				}
			}
#endif
			/* if want_submatches is set, pmatch should be 
			free`ed by the calling function! */
		}
		pcre_free(pcre_c);
	} else {
		/* non regex part start */
		if (!is_case_sens) {
			f = strncasecmp;
		} else {
			f = strncmp;
		}
		buflen = strlen(buf);
		patlen = strlen(search_pattern);
		
		for (i = byte_offset; i <= (buflen - patlen); i++) {
			match = f(&buf[i], search_pattern, patlen);
			if (match == 0) {
				returnvalue.bstart = i;
				returnvalue.bend = i + patlen;
				break;
			}
		}
		/* non regex part end */	
	}
	
	/* if we have a valid result, we now calculate the character offsets for this result */
	if (returnvalue.bstart >= 0 && returnvalue.bend >= 0) {
		/* utf8_offset_cache_reset(); */
		if (returnvalue.bstart >= 0) {
			returnvalue.start = utf8_byteoffset_to_charsoffset_cached(buf, returnvalue.bstart);
		}
		if (returnvalue.bend >= 0) {
			returnvalue.end = utf8_byteoffset_to_charsoffset_cached(buf, returnvalue.bend);
		}
		if (want_submatches) {
			int i;
			for (i=0;i<returnvalue.nmatch;i++) {
				returnvalue.pmatch[i].rm_so = utf8_byteoffset_to_charsoffset_cached(buf, returnvalue.pmatch[i].rm_so);
				returnvalue.pmatch[i].rm_eo = utf8_byteoffset_to_charsoffset_cached(buf, returnvalue.pmatch[i].rm_eo);
			}
		}
	} else {
		returnvalue.start = -1;
		returnvalue.end = -1;
		returnvalue.bstart = -1;
		returnvalue.bend = -1;
	}

	DEBUG_MSG("search_backend, returning result.start=%d, result.end=%d, bstart=%d, bend=%d\n", returnvalue.start, returnvalue.end, returnvalue.bstart, returnvalue.bend);
	return returnvalue;
}

/*****************************************************/

/**
 * search_doc:
 * @bfwin: #Tbfwin*
 * @document: a #Tdocument to search
 * @search_pattern: a #gchar* to the search pattern.
 * @matchtype: see #Tmatch_types
 * @is_case_sens: #gint
 * @startpos: #gint offset in document buffer
 *
 * Perform search by calling search_backend.
 * Updates last_snr2-values, but doesn't use them -- that is the callers duty.
 *
 * Return value: #Tsearch_result
 **/
Tsearch_result search_doc(Tbfwin *bfwin,Tdocument *document, gchar *search_pattern, Tmatch_types matchtype, gint is_case_sens, gint startpos, gboolean unescape) {
	gchar *fulltext, *realpat;
	Tsearch_result result;
	
	DEBUG_MSG("search_doc, started on document %p, startpos=%d\n", document, startpos);
	utf8_offset_cache_reset();
	fulltext = doc_get_chars(document, startpos, -1);
	DEBUG_MSG("search_doc, fulltext=%p, search_pattern=%p\n", fulltext, search_pattern);
	if (unescape) {
		realpat = unescape_string(search_pattern, FALSE);
	} else {
		realpat = search_pattern;
	}
	result = search_backend(bfwin,realpat, matchtype, is_case_sens, fulltext, 0, FALSE);
	if (unescape) {
		g_free(realpat);
	}
	g_free(fulltext);
	if (result.end > 0) {
		DEBUG_MSG("search_doc, received a result (start=%d), adding startpos (%d) to it\n", result.start, startpos);
		result.start += startpos;
		result.end += startpos;
		LASTSNR2(bfwin->snr2)->result.start = result.start;
		LASTSNR2(bfwin->snr2)->result.end = result.end;
		LASTSNR2(bfwin->snr2)->doc = document;
	} else {
		LASTSNR2(bfwin->snr2)->result.start = -1;
		LASTSNR2(bfwin->snr2)->result.end =  -1;
		LASTSNR2(bfwin->snr2)->doc = document;
	}
	DEBUG_MSG("search_doc, result.start=%d, result.end=%d\n", result.start, result.end);
	return result;
}

/*****************************************************/

/**
 * doc_show_result:
 * @document: a #Tdocument
 * @start: Selection start.
 * @end: Selection end.
 *
 * Focus a document and select the supplied range.
 *
 * Return value: void
 **/
void doc_show_result(Tdocument *document, gint start, gint end) {
	DEBUG_MSG("doc_show_result, select from start=%d to end=%d\n",start, end);
	if (document != BFWIN(document->bfwin)->current_document) {
		switch_to_document_by_pointer(BFWIN(document->bfwin),document);
	}
	doc_select_region(document, start, end, TRUE);
}

/*****************************************************/

/**
 * search_all:
 * @bfwin: #Tbfwin*
 * @search_pattern: #gchar to search pattern
 * @matchtype: see #Tmatch_types
 * is_case_sens: #gint set to 0 or 1.
 *
 * Perform a specified search, spanning all open documents.
 *
 * When called several times, the search continues from where it left off last time.
 * The current 'search-position' is stored in the internal last_snr2 structure.
 *
 * Return value: #Tsearch_all_result
 **/
Tsearch_all_result search_all(Tbfwin *bfwin,gchar *search_pattern, Tmatch_types matchtype, gint is_case_sens, gboolean unescape) {
	GList *tmplist;
	Tsearch_all_result result_all;

	DEBUG_MSG("search_all, started\n");
	result_all.start = -1;
	result_all.end = -1;
	result_all.doc = NULL;
	
	if (LASTSNR2(bfwin->snr2)->doc) {
		tmplist = g_list_find(bfwin->documentlist, LASTSNR2(bfwin->snr2)->doc);
	} else {
		LASTSNR2(bfwin->snr2)->result.end = 0;
		tmplist = g_list_first(bfwin->documentlist);
	}
	while (tmplist) {
		Tsearch_result result;

		result = search_doc(bfwin,(Tdocument *)tmplist->data, search_pattern, matchtype, is_case_sens, LASTSNR2(bfwin->snr2)->result.end, unescape);
		if (result.end > 0) {
			result_all.start = result.start;
			result_all.end = result.end;
			result_all.doc = tmplist->data;
			DEBUG_MSG("search_all, found!! start=%d, end=%d, doc=%p\n", result.start, result.end, tmplist->data);
			return result_all;
		}
		tmplist = g_list_next(tmplist);
		if (tmplist) {
			LASTSNR2(bfwin->snr2)->result.end = 0;
		}
	}
	DEBUG_MSG("search_all, not found..\n");
	return result_all;
}

/*****************************************************/

static gchar *reg_replace(gchar *replace_pattern, gint offset, Tsearch_result result, Tdocument *doc, gboolean standardescape) {
	Tconvert_table * tct;
	gchar *retval;
	gint i, size;
	DEBUG_MSG("reg_replace, started for pattern='%s',standardescape=%d\n",replace_pattern,standardescape);
	size = (result.nmatch <= 10) ? (result.nmatch == 0 ) ? 0 : result.nmatch -1 : 10;
	tct = new_convert_table(size, standardescape);
	for (i=0;i<size;i++) {
		tct[i].my_int = i+48;
		tct[i].my_char = doc_get_chars(doc, offset+result.pmatch[i+1].rm_so, offset+result.pmatch[i+1].rm_eo);
	}
	retval = expand_string(replace_pattern, '\\', tct);
	free_convert_table(tct);
	return retval;
}

/*
 * this function will parse the replace string and substitute the \0, \1 etc. with 
 * the subsearch_pattern matches from regexec()
 */
/*static gchar *oldreg_replace(gchar *replace_pattern, gint offset, Tsearch_result result, Tdocument *doc) {
	gchar *tmp1, *newstring;
	gchar *tmpstr1, *tmpstr2, *tmpstr3;
	gboolean escaped=0;
	guint from=0, to=0;

	newstring = g_strdup("");
	tmp1 = replace_pattern;
	while (*tmp1 != '\0') {
		if (escaped) {
			if (*tmp1 == '\\') {
				to++;
				tmpstr1 = newstring;
				tmpstr2 = g_strndup(&replace_pattern[from], to-from);
				newstring = g_strconcat(tmpstr1, tmpstr2, NULL);
				g_free(tmpstr1);
				g_free(tmpstr2);
				from = ++to;
				DEBUG_MSG("reg_replace, escaped backslash, newstring='%s'\n", newstring);			
			} else if ((gint)*tmp1 >= 48 && (gint)*tmp1 <= 57) {
				gint num = (gint)*tmp1 - 48;
				tmpstr1 = newstring;
				tmpstr2 = g_strndup(&replace_pattern[from], to-from);
				DEBUG_MSG("reg_replace, from=%d, to=%d, tmpstr2='%s'\n", from, to, tmpstr2);
				if (result.nmatch >= num+1) {
					DEBUG_MSG("reg_replace, wanted: sub search_pattern %d, offset=%d, so=%d, eo=%d\n", num, offset, result.pmatch[num+1].rm_so, result.pmatch[num+1].rm_eo);
					tmpstr3 = doc_get_chars(doc, offset+result.pmatch[num+1].rm_so, offset+result.pmatch[num+1].rm_eo);
					DEBUG_MSG("reg_replace, subsearch_pattern %d = '%s'\n", num, tmpstr3);
				} else {
					DEBUG_MSG("reg_replace, subsearch_pattern %d does not exist, nmatch=%d\n", num, result.nmatch);
					tmpstr3 = g_strdup("");
				}
				newstring = g_strconcat(tmpstr1, tmpstr2, tmpstr3, NULL);
				g_free(tmpstr1);
				g_free(tmpstr2);
				g_free(tmpstr3);
				to += 2;
				from = to;
				DEBUG_MSG("reg_replace, substituted subsearch_pattern, newstring='%s'\n", newstring);
			} else {
				to += 2;
			}
			escaped = 0;
		} else {
			if (*tmp1 == '\\') {
				escaped = 1;
			} else {
				to++;
			}
		}
		tmp1++;
	}
	tmpstr1 = newstring;
	tmpstr2 = g_strndup(&replace_pattern[from], to-from);
	newstring = g_strconcat(tmpstr1, tmpstr2, NULL);
	g_free(tmpstr1);
	g_free(tmpstr2);
	DEBUG_MSG("reg_replace, end, newstring='%s'\n", newstring);
	return newstring;
}*/

/**
 * replace_backend:
 * @bfwin: #Tbfwin with the window from this/these documents
 * @search_pattern: #gchar* to search pattern
 * @matchtype: See #Tmatch_types
 * @is_case_sens: #gint
 * @buf: #ghar* to buffer
 * @replace_pattern: The replace pattern.
 * @doc: the #Tdocument
 * @offset: The difference between the buffer and the text widget because of previous replace actions, so the first char in buf is actually number offset in the text widget.
 * @replacetype: see #Treplace_types
 * @replacelen: #gint*, set to -1 to calculate this automatically.
 * 
 * This will perform a search and the actual replacement of data in the buffer.
 * Note that *buf is probably an offset in the documents buffer.
 * 
 * Return value: #Tsearch_result
 **/
Tsearch_result replace_backend(Tbfwin *bfwin,gchar *search_pattern, Tmatch_types matchtype, gint is_case_sens
			, gchar *buf, guint byte_offset, gchar *replace_pattern, Tdocument *doc, gint offset, Treplace_types replacetype
			, gint *replacelen, gboolean unescape) {
/* the offset in this function is the difference between the buffer and the text widget because of previous replace 
actions, so the first char in buf is actually number offset in the text widget */
/* replacelen -1 means there is no replacelen known yet, so we have to calculate it */
	Tsearch_result result;
	gchar *tmpstr=NULL, *realpat;
	
	if (unescape) {
		realpat = unescape_string(search_pattern,FALSE);
		DEBUG_MSG("replace_backend, realpat='%s'\n",realpat);
	} else {
		realpat = search_pattern;
	}
	result = search_backend(bfwin,realpat, matchtype, is_case_sens, buf, byte_offset, (matchtype != match_normal));
	if (unescape) {
		DEBUG_MSG("replace_backend, free-ing realpat\n");
		g_free(realpat);
	}
	DEBUG_MSG("replace_backend, offset=%d, result.start=%d, result.end=%d\n", offset, result.start, result.end);
	if (result.end > 0) {
		switch (replacetype) {
		case string:
			if (matchtype == match_normal) {
				if (unescape) {
					DEBUG_MSG("replace_backend, replace_pattern='%s'\n",replace_pattern);
					tmpstr = unescape_string(replace_pattern, FALSE);
				} else {
					tmpstr = g_strdup(replace_pattern);
				}
			} else {
				tmpstr = reg_replace(replace_pattern, offset, result, doc, unescape);
			}
			DEBUG_MSG("replace_backend, tmpstr='%s'\n", tmpstr);
		break;
		case uppercase:
			tmpstr = g_strndup(&buf[result.bstart], result.bend - result.bstart);
			tmpstr = g_utf8_strup(tmpstr,-1);
		break;
		case lowercase:
			tmpstr = g_strndup(&buf[result.bstart], result.bend - result.bstart);
			tmpstr = g_utf8_strdown(tmpstr,-1);
		break;
		}
		DEBUG_MSG("replace_backend, len=%d, offset=%d, start=%d, end=%d, document=%p, tmpstr='%s'\n", result.end - result.start, offset, result.start + offset, result.end + offset, doc,tmpstr);
		doc_replace_text_backend(doc, tmpstr, result.start + offset, result.end + offset);
		if (*replacelen == -1) {
			*replacelen = g_utf8_strlen(tmpstr, -1);
		}
		g_free(tmpstr);
	}
	if (matchtype == match_posix) {
		g_free(result.pmatch);
		result.pmatch = NULL;
	}
	return result;
}

/*****************************************************/

/**
 * replace_doc_once:
 * @search_pattern: #gchar* to search pattern
 * @matchtype: see #Tmatch_types
 * @is_case_sens: #gint
 * @startpos: #gint offset in document.
 * @endpos: #gint where to stop replacing. Set to -1 to cover the entire buffer.
 * @replace_pattern: #gchar* to replace pattern
 * @doc: a #Tdocument* to work on
 * @replacetype: see #Treplace_types
 *
 * Performs a single replace with the selected parameters, by calling replace_backend().
 * Calls doc_unre_new_group(doc) to make this action undoable.
 * Updates the internal last_snr2 struct, but the _caller_ is responsible for honouring this data.
 * 
 * Return value: #Tsearch_result
 **/
Tsearch_result replace_doc_once(Tbfwin *bfwin,gchar *search_pattern, Tmatch_types matchtype, gint is_case_sens, gint startpos, gint endpos, gchar *replace_pattern, Tdocument *doc, Treplace_types replacetype, gboolean unescape) {
/* endpos -1 means do till end */
	gchar *fulltext;
	gint replacelen = 0; /* replacelen -1 means there is no replacelen known yet
								, but we don't need one, so we set it to 0 */
	Tsearch_result result;

	doc_unre_new_group(doc);
	fulltext = doc_get_chars(doc, startpos, endpos);
	result = replace_backend(bfwin,search_pattern, matchtype, is_case_sens, fulltext, 0, replace_pattern, doc, startpos, replacetype, &replacelen, unescape);
	if ( result.end > 0) {
		LASTSNR2(bfwin->snr2)->result.start = result.start + startpos;
		LASTSNR2(bfwin->snr2)->result.end = result.end + startpos;
		LASTSNR2(bfwin->snr2)->doc = doc;
	} else {
		LASTSNR2(bfwin->snr2)->result.start = -1;
		LASTSNR2(bfwin->snr2)->result.end = -1;
		LASTSNR2(bfwin->snr2)->doc = doc;
	}
	g_free(fulltext);

	doc_unre_new_group(doc);

	return result;
}
 
/*****************************************************/

/**
 * replace_doc_multiple:
 * @search_pattern: #gchar* to search pattern
 * @matchtype: see #Tmatch_types
 * @is_case_sens: #gint
 * @endpos: #gint where to stop replacing. Set to -1 to cover the entire buffer.
 * @replace_pattern: #gchar* to replace pattern
 * @doc: a #Tdocument* to work on
 * @replacetype: see #Treplace_types
 * 
 * Performs a replace on all occurences of the pattern in the supplied document.
 * The doc's buffer will be modified.
 *
 * last_snr2 is reset with .start = .end = -1, and .doc = doc.
 * 
 * Return value: void
 **/
void replace_doc_multiple(Tbfwin *bfwin,gchar *search_pattern, Tmatch_types matchtype, gint is_case_sens, gint startpos, gint endpos, gchar *replace_pattern, Tdocument *doc, Treplace_types replacetype, gboolean unescape) {
/* endpos -1 means do till end */
	gchar *fulltext, *realpats, *realpatr;
	gboolean realunesc;
	Tsearch_result result;
	gint buf_byte_offset=0;
	gint buf_text_offset=startpos;
	gint replacelen; /* replacelen -1 means there is no replacelen known yet */
	doc_unre_new_group(doc);

	DEBUG_MSG("replace_doc_multiple, startpos=%d, endpos=%d\n", startpos, endpos);
	if (matchtype == match_normal || replacetype != string) {
		/* the replace string has a fixed length if it is not regex, or it is not type string
		 in this case we can also do the unescaping in this function */
		if (unescape) {
			realpats = unescape_string(search_pattern, FALSE);
			realpatr = unescape_string(replace_pattern, FALSE);
			DEBUG_MSG("replace_doc_multiple, unescaped patterns, realpats='%s', realpatr='%s'\n",realpats, realpatr);
		} else {
			realpats = search_pattern;
			realpatr = replace_pattern;
		}
		replacelen = g_utf8_strlen(realpatr,-1);
		realunesc = FALSE;
	} else {
		replacelen=-1;
		realpats = search_pattern;
		realpatr = replace_pattern;
		realunesc = unescape;
	}
	fulltext = doc_get_chars(doc, startpos, endpos);
	result = replace_backend(bfwin,realpats, matchtype, is_case_sens, fulltext, 0, realpatr, doc, buf_text_offset, replacetype, &replacelen, realunesc);
	while (result.end > 0) {
		if (replacetype == string) {
			buf_text_offset += replacelen - (result.end - result.start);
		}
		if (LASTSNR2(bfwin->snr2)->overlapping_search) {
			buf_byte_offset = result.bstart + 1;
			/* buf_text_offset += result.start + 1; */
		} else {
			buf_byte_offset = result.bend;
			/* buf_text_offset += result.end; */
		}
		DEBUG_MSG("replace_doc_multiple, after first search, buf_text_offset=%d, buf_byte_offset=%d\n", buf_text_offset, buf_byte_offset);
		if (matchtype != match_normal && replacetype == string) {
			/* all regex replaces can have different replace lengths, so they have to be re-calculated */
			replacelen = -1;
		}
		result = replace_backend(bfwin,realpats, matchtype, is_case_sens, fulltext, buf_byte_offset, realpatr, doc, buf_text_offset, replacetype, &replacelen, realunesc);

		DEBUG_MSG("replace_doc_multiple, 1- buf_text_offset=%d, buf_byte_offset=%d, result.start=%d, result.end=%d\n", buf_text_offset, buf_byte_offset, result.start, result.end);
	}
	if (unescape && (matchtype == match_normal || replacetype != string)) {
		DEBUG_MSG("replace_doc_multiple, free-ing realpats and realpatr\n");
		g_free(realpats);
		g_free(realpatr);
	}

	doc_unre_new_group(doc);

	LASTSNR2(bfwin->snr2)->result.start = -1;
	LASTSNR2(bfwin->snr2)->result.end = -1;
	LASTSNR2(bfwin->snr2)->doc = doc;
	g_free(fulltext);
}

/*****************************************************/

/**
 * replace_all:
 * @bfwin: #Tbfwin* the window with the documents to act upon
 * @search_pattern: #gchar* to search pattern
 * @matchtype: see #Tmatch_types
 * @is_case_sens: #gint
 * @replace_pattern: #gchar* to replace pattern
 * @replacetype: see #Treplace_types
 * 
 * Perform a replace_doc_multiple() with supplied data on all open documents.
 * This will replace all occurences of search_pattern in all documents.
 * 
 * Return value: void
 **/
void replace_all(Tbfwin *bfwin,gchar *search_pattern, Tmatch_types matchtype, gint is_case_sens, gchar *replace_pattern, Treplace_types replacetype, gboolean unescape) {
	GList *tmplist;

	tmplist = g_list_first(bfwin->documentlist);
	while (tmplist) {
		replace_doc_multiple(bfwin,search_pattern, matchtype, is_case_sens, 0, -1, replace_pattern, (Tdocument *)tmplist->data, replacetype, unescape);
		tmplist = g_list_next(tmplist);
	}
}

/*****************************************************/
/*             Replace prompt callbacks              */
/*****************************************************/

/*
 *
 * Performs the actual replace-surgery by calls to doc_replace_text_backend() !
 * Continues the replace cycle by calling snr2_run(), unless this is a single replace.
 *
 */
static void replace_prompt_dialog_ok_lcb(GtkWidget *widget, Tbfwin *bfwin) {
	gchar *tmpstr;
	gint sel_start_pos, sel_end_pos;

	window_close_by_widget_cb(widget, NULL);
	
	doc_get_selection(bfwin->current_document, &sel_start_pos, &sel_end_pos);
	if ((sel_start_pos == LASTSNR2(bfwin->snr2)->result.start) &&
				(sel_end_pos == LASTSNR2(bfwin->snr2)->result.end)) {
		gint lenadded;
		if (LASTSNR2(bfwin->snr2)->replacetype_option==string) {
			tmpstr = g_strdup(LASTSNR2(bfwin->snr2)->replace_pattern);
			/* if it was a regex replace we need to do the sub-search_pattern matching */
			tmpstr = reg_replace(tmpstr, 0, LASTSNR2(bfwin->snr2)->result, bfwin->current_document, LASTSNR2(bfwin->snr2)->unescape);
			
		} else if (LASTSNR2(bfwin->snr2)->replacetype_option==uppercase) {
			tmpstr = doc_get_chars(bfwin->current_document, LASTSNR2(bfwin->snr2)->result.start ,LASTSNR2(bfwin->snr2)->result.end);
			tmpstr = g_utf8_strup(tmpstr,-1);
		} else {
			tmpstr = doc_get_chars(bfwin->current_document, LASTSNR2(bfwin->snr2)->result.start ,LASTSNR2(bfwin->snr2)->result.end);
			tmpstr = g_utf8_strdown(tmpstr,-1);
		}
		/* avoid new highlighting at this stage, so call the backend directly instead of the frontend function
		this because the highlighting interferes with the selection
		the better solution is to have the highlighting handle the selection better, 
		the problem starts in document.c in get_positions() because the selection is not saved there
		I don't know why the selection is gray, but that's basically the reason why it doesn't save the selection
		 */

		doc_unre_new_group(bfwin->current_document);
		doc_replace_text_backend(bfwin->current_document, tmpstr, LASTSNR2(bfwin->snr2)->result.start,LASTSNR2(bfwin->snr2)->result.end);
		doc_unre_new_group(bfwin->current_document);
		doc_set_modified(bfwin->current_document, 1);

		lenadded = strlen(tmpstr) - (LASTSNR2(bfwin->snr2)->result.end - LASTSNR2(bfwin->snr2)->result.start);
		DEBUG_MSG("lenadded=%d (streln=%d, end-start=%d)\n",lenadded,strlen(tmpstr),(LASTSNR2(bfwin->snr2)->result.end - LASTSNR2(bfwin->snr2)->result.start));
		g_free(tmpstr);
		if (LASTSNR2(bfwin->snr2)->result.pmatch) {
			g_free(LASTSNR2(bfwin->snr2)->result.pmatch);
			LASTSNR2(bfwin->snr2)->result.pmatch = NULL;
		}
		if (!LASTSNR2(bfwin->snr2)->replace_once) {
			if (!LASTSNR2(bfwin->snr2)->overlapping_search && lenadded > 0) {
				LASTSNR2(bfwin->snr2)->result.end += lenadded;
			}
			snr2_run(bfwin,NULL);
		}
	}
#ifdef DEBUG
	 else {
		DEBUG_MSG("replace_prompt_dialog_ok_lcb, selection != result, not replacing!!\n");
	}
#endif /* DEBUG */
}

static void replace_prompt_dialog_skip_lcb(GtkWidget *widget, Tbfwin *bfwin) {
	window_close_by_widget_cb(widget, NULL);
	if (LASTSNR2(bfwin->snr2)->result.pmatch) {
		g_free(LASTSNR2(bfwin->snr2)->result.pmatch);
		LASTSNR2(bfwin->snr2)->result.pmatch = NULL;
	}
	if (!LASTSNR2(bfwin->snr2)->replace_once) {
		snr2_run(bfwin,NULL);
	}
}

/* Alters last_snr2, setting no-prompt-mode, backtracking one step on the startpoint and .end = .start
 * continues by running snr2_run(). This will replace all occurrences of the string.. */
static void replace_prompt_dialog_all_lcb(GtkWidget *widget, Tbfwin *bfwin) {
	window_close_by_widget_cb(widget, NULL);
	LASTSNR2(bfwin->snr2)->prompt_before_replace = 0;
	LASTSNR2(bfwin->snr2)->result.start--;
	LASTSNR2(bfwin->snr2)->result.end = LASTSNR2(bfwin->snr2)->result.start;
	if (LASTSNR2(bfwin->snr2)->result.pmatch) {
		g_free(LASTSNR2(bfwin->snr2)->result.pmatch);
		LASTSNR2(bfwin->snr2)->result.pmatch = NULL;
	}
	snr2_run(bfwin,NULL);
}


/**
 * replace_prompt_dialog:
 * 
 * Prompt the user for a replace action. Simply creates and shows GUI.
 * Used by replace_prompt_doc().
 * 
 * Return value: void
 **/
void replace_prompt_dialog(Tbfwin *bfwin) {
	GtkWidget *win, *vbox, *hbox;
	GtkWidget *butok, *butclose, *butall, *butskip;
	GtkWidget *image, *label;

	DEBUG_MSG("replace_prompt_dialog, start\n");
	win = window_full(_("Confirm replace"), GTK_WIN_POS_MOUSE, 12, G_CALLBACK(window_close_by_widget_cb), NULL, TRUE);
	gtk_window_set_resizable (GTK_WINDOW (win), FALSE);	

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (win), vbox);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

	image = gtk_image_new_from_stock ("gtk-dialog-question", GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, TRUE, 0);

	label = gtk_label_new (_("Replace selected text?"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

	gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, TRUE, 12);

	hbox = gtk_hbutton_box_new ();
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX (hbox), 6);

	butclose = bf_gtkstock_button(GTK_STOCK_CLOSE, G_CALLBACK(window_close_by_widget_cb), NULL);
	butskip = bf_allbuttons_backend(_("_Skip"), TRUE, 1000, G_CALLBACK(replace_prompt_dialog_skip_lcb), bfwin);
	butok = bf_allbuttons_backend(_("_Replace"), TRUE, 1001, G_CALLBACK(replace_prompt_dialog_ok_lcb), bfwin);
	butall = bf_generic_mnemonic_button(_("Replace _all"), G_CALLBACK(replace_prompt_dialog_all_lcb), bfwin);
	gtk_box_pack_start(GTK_BOX(hbox), butclose, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), butall, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), butok, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), butskip, FALSE, FALSE, 0);
	gtk_widget_grab_focus(butskip);
	gtk_widget_show_all(win);
	DEBUG_MSG("replace_prompt_dialog, end\n");
}

/**
 * replace_prompt_doc:
 * @search_pattern: #gchar* to search pattern
 * @matchtype: see #Tmatch_types
 * @is_case_sens: #gint
 * @startpos: Start offset in document buffer.
 * @endpos: End offset of search area. Set to -1 to cover entire buffer.
 * @replace_pattern: #gchar to replace pattern
 * @doc: a #Tdocument
 * 
 * Finds the next occurence of search_pattern in *doc, shows and selects it in the document,
 * and finally shows a replace_prompt_dialog to the user.
 *
 * last_snr2 is updated if an occurrence of search_pattern is found.
 * 
 * Return value: #gint, 1 if a new occurence of the search_pattern was found and a dialog is shown. 0 else.
 **/
gint replace_prompt_doc(Tbfwin *bfwin, gchar *search_pattern, Tmatch_types matchtype, gint is_case_sens, gint startpos, gint endpos, gchar *replace_pattern, Tdocument *doc, gboolean unescape) {
/* endpos -1 means do till end , returns if the document still had a match*/
	gchar *fulltext, *realpat;
	Tsearch_result result;

	DEBUG_MSG("replace_prompt_doc, doc=%p, startpos=%d, endpos=%d\n",doc,startpos,endpos);
	if (LASTSNR2(bfwin->snr2)->result.pmatch) {
		g_free(LASTSNR2(bfwin->snr2)->result.pmatch);
		LASTSNR2(bfwin->snr2)->result.pmatch = NULL;
	}
	fulltext = doc_get_chars(doc, startpos, endpos);
	utf8_offset_cache_reset();
	if (unescape) {
		realpat = unescape_string(search_pattern, FALSE);
	} else {
		realpat = search_pattern;
	}
	result = search_backend(bfwin,realpat, matchtype, is_case_sens, fulltext,0, 1);
	if (unescape) {
		g_free(realpat);
	}
	LASTSNR2(bfwin->snr2)->doc = doc;
	g_free(fulltext);
	DEBUG_MSG("replace_prompt_doc, doc=%p, result.end=%d\n", doc, result.end);
	if (result.end > 0) {
		gint i;
		LASTSNR2(bfwin->snr2)->result.start = result.start + startpos;
		LASTSNR2(bfwin->snr2)->result.end = result.end + startpos;
		LASTSNR2(bfwin->snr2)->result.nmatch = result.nmatch;
		LASTSNR2(bfwin->snr2)->result.pmatch = result.pmatch;
		for (i=0;i<result.nmatch;i++) {
			DEBUG_MSG("replace_prompt_doc, adding offset %d to subsearch_pattern %d\n", startpos, i);
			LASTSNR2(bfwin->snr2)->result.pmatch[i].rm_so += startpos;
			LASTSNR2(bfwin->snr2)->result.pmatch[i].rm_eo += startpos;
		}
		doc_show_result(doc, result.start + startpos, result.end + startpos);
		replace_prompt_dialog(bfwin);
		return 1;
	} else {
		g_free(result.pmatch);
		return 0;
	}
}

/**
 * replace_prompt_all:
 * @search_pattern: #gchar* to search pattern
 * @matchtype: see #Tmatch_types
 * @is_case_sens: #gint
 * @replace_pattern: #gchar to replace pattern
 * 
 * Performs a replace_prompt_doc for all open documents.
 * 
 * Return value: void
 **/
void replace_prompt_all(Tbfwin *bfwin,gchar *search_pattern, Tmatch_types matchtype, gint is_case_sens, gchar *replace_pattern, gboolean unescape) {
	GList *tmplist;
	gint retvalue;
	Tdocument *tmpdoc;

	if (LASTSNR2(bfwin->snr2)->doc) {
		tmpdoc = LASTSNR2(bfwin->snr2)->doc;
	} else {
		tmplist = g_list_first(bfwin->documentlist);
		tmpdoc = (Tdocument *)tmplist->data;
	}
	DEBUG_MSG("replace_prompt_all, starting with tmpdoc=%p at position %d\n",tmpdoc,LASTSNR2(bfwin->snr2)->result.end);
	retvalue = replace_prompt_doc(bfwin,search_pattern, matchtype, is_case_sens, (LASTSNR2(bfwin->snr2)->result.end < 0)?0:LASTSNR2(bfwin->snr2)->result.end, -1, replace_pattern, tmpdoc, unescape);
	while (retvalue == 0) {
		tmplist = g_list_find(bfwin->documentlist, LASTSNR2(bfwin->snr2)->doc);
		tmplist = g_list_next(tmplist);
		if (tmplist) {
			DEBUG_MSG("replace_prompt_all, next document is %p\n",tmplist->data);
			retvalue = replace_prompt_doc(bfwin,search_pattern, matchtype, is_case_sens, 0, -1, replace_pattern, (Tdocument *)tmplist->data, unescape);
		} else {
			retvalue = 1;
		}
	}
}

static void search_doc_bookmark_backend(Tbfwin *bfwin,Tdocument *document, gchar *search_pattern, Tmatch_types matchtype, gint is_case_sens, gint startpos, gboolean unescape) {
	gchar *fulltext, *realpat;
	gint buf_byte_offset=0;
	Tsearch_result result;
	fulltext = doc_get_chars(document, 0, -1);
	utf8_offset_cache_reset();
	if (unescape) {
		realpat = unescape_string(search_pattern, FALSE);
	} else {
		realpat = search_pattern;
	}
	result = search_backend(bfwin,realpat, matchtype, is_case_sens, fulltext, 0, FALSE);
	while (result.end > 0) {
		gchar *text = doc_get_chars(document, result.start, result.end);
		DEBUG_MSG("search_bookmark, adding bookmark '%s' at %d\n", text, result.start);
		bmark_add_extern(document, result.start, NULL, text, !main_v->props.bookmarks_default_store);
		g_free(text);
		if (LASTSNR2(bfwin->snr2)->overlapping_search) {
			buf_byte_offset = result.bstart + 1;
		} else {
			buf_byte_offset = result.bend;
		}
		result = search_backend(bfwin,realpat, matchtype, is_case_sens, fulltext, buf_byte_offset, FALSE);
	}
	if (unescape) {
		g_free(realpat);
	}
	g_free(fulltext);
}

/**
 * search_bookmark:
 * @bfwin: #Tbfwin *
 * @startat: #gint
 *
 * will search, and bookmark all matches
 * 
 */
static void search_bookmark(Tbfwin *bfwin, gint startat) {
	DEBUG_MSG("search_bookmark, started\n");
	if (LASTSNR2(bfwin->snr2)->placetype_option==opened_files) {
		GList *tmplist = g_list_first(bfwin->documentlist);
		while (tmplist) {
			search_doc_bookmark_backend(bfwin,DOCUMENT(tmplist->data), LASTSNR2(bfwin->snr2)->search_pattern, LASTSNR2(bfwin->snr2)->matchtype_option, LASTSNR2(bfwin->snr2)->is_case_sens, 0, LASTSNR2(bfwin->snr2)->unescape);
			tmplist = g_list_next(tmplist);
		}
	} else {
		search_doc_bookmark_backend(bfwin,DOCUMENT(bfwin->current_document), LASTSNR2(bfwin->snr2)->search_pattern, LASTSNR2(bfwin->snr2)->matchtype_option, LASTSNR2(bfwin->snr2)->is_case_sens, startat, LASTSNR2(bfwin->snr2)->unescape);
	}
	DEBUG_MSG("search_bookmark, done\n");
}

/*****************************************************/

/**
 * snr2_run:
 * @bfwin: #Tbfwin*
 * @doc: a #Tdocument* If set to NULL, use bfwin->current_document
 *
 * Continues a search or replace action as specified by the last_snr2 struct.
 * 
 * Return value: void
 **/
void snr2_run(Tbfwin *bfwin, Tdocument *doc) {
	gint startpos, endpos;
	Tsearch_result result;
	Tsearch_all_result result_all;
	Treplace_types replacetype;

	if (doc==NULL) {
		doc = bfwin->current_document;
	}

	if (LASTSNR2(bfwin->snr2)->result.pmatch) {
		g_free(LASTSNR2(bfwin->snr2)->result.pmatch);
		LASTSNR2(bfwin->snr2)->result.pmatch = NULL;
	}

	/* should be more stuff here */
	if (LASTSNR2(bfwin->snr2)->placetype_option==beginning) {
		startpos = 0;
		endpos = -1;
	} else if (LASTSNR2(bfwin->snr2)->placetype_option==cursor) {
		startpos = doc_get_cursor_position(doc);
		endpos = -1;
	} else if (LASTSNR2(bfwin->snr2)->placetype_option==selection) {
		if (!doc_get_selection(doc,&startpos,&endpos)) {
			/* what to do if there was no selection ?*/
			DEBUG_MSG("snr2_run, no selection found, returning\n");
			return;
		}
		DEBUG_MSG("snr2_run, from selection: startpos=%d, endpos=%d\n", startpos, endpos);
	}
	if (LASTSNR2(bfwin->snr2)->doc == doc) {
		if (LASTSNR2(bfwin->snr2)->result.end > 0) {
			if (LASTSNR2(bfwin->snr2)->overlapping_search) {
				startpos = LASTSNR2(bfwin->snr2)->result.start + 1;
			} else {
				startpos = LASTSNR2(bfwin->snr2)->result.end;
			}
		}
		DEBUG_MSG("snr2_run, LASTSNR2(bfwin->snr2)->result.end=%d, startpos=%d\n", LASTSNR2(bfwin->snr2)->result.end, startpos);
	}
	if (LASTSNR2(bfwin->snr2)->replace) {
		if (LASTSNR2(bfwin->snr2)->replacetype_option==string) {
			replacetype = string;
		} else if (LASTSNR2(bfwin->snr2)->replacetype_option==uppercase) {
			replacetype = uppercase;
		} else {
			replacetype = lowercase;
		}
	
		if (LASTSNR2(bfwin->snr2)->prompt_before_replace) {
			if (LASTSNR2(bfwin->snr2)->placetype_option==opened_files) {
				replace_prompt_all(bfwin,LASTSNR2(bfwin->snr2)->search_pattern,LASTSNR2(bfwin->snr2)->matchtype_option, LASTSNR2(bfwin->snr2)->is_case_sens, LASTSNR2(bfwin->snr2)->replace_pattern, LASTSNR2(bfwin->snr2)->unescape);
			} else {
				replace_prompt_doc(bfwin,LASTSNR2(bfwin->snr2)->search_pattern, LASTSNR2(bfwin->snr2)->matchtype_option, LASTSNR2(bfwin->snr2)->is_case_sens, startpos, endpos, LASTSNR2(bfwin->snr2)->replace_pattern, doc, LASTSNR2(bfwin->snr2)->unescape);
			}
		} else {
			if (LASTSNR2(bfwin->snr2)->placetype_option==opened_files) {
				replace_all(bfwin,LASTSNR2(bfwin->snr2)->search_pattern, LASTSNR2(bfwin->snr2)->matchtype_option, LASTSNR2(bfwin->snr2)->is_case_sens, LASTSNR2(bfwin->snr2)->replace_pattern, replacetype, LASTSNR2(bfwin->snr2)->unescape);
			} else if (LASTSNR2(bfwin->snr2)->replace_once) {
				replace_doc_once(bfwin,LASTSNR2(bfwin->snr2)->search_pattern, LASTSNR2(bfwin->snr2)->matchtype_option, LASTSNR2(bfwin->snr2)->is_case_sens, startpos, endpos, LASTSNR2(bfwin->snr2)->replace_pattern, doc, replacetype, LASTSNR2(bfwin->snr2)->unescape);
			} else {
				replace_doc_multiple(bfwin,LASTSNR2(bfwin->snr2)->search_pattern, LASTSNR2(bfwin->snr2)->matchtype_option, LASTSNR2(bfwin->snr2)->is_case_sens, startpos, endpos, LASTSNR2(bfwin->snr2)->replace_pattern, doc, replacetype, LASTSNR2(bfwin->snr2)->unescape);
			}		
		}
	} else { /* find, not replace */
		if (LASTSNR2(bfwin->snr2)->bookmark_results) {
			search_bookmark(bfwin, startpos);
		} else {
			if (LASTSNR2(bfwin->snr2)->placetype_option==opened_files) {
				DEBUG_MSG("snr2dialog_ok_lcb, search = all\n");
				result_all = search_all(bfwin,LASTSNR2(bfwin->snr2)->search_pattern, LASTSNR2(bfwin->snr2)->matchtype_option, LASTSNR2(bfwin->snr2)->is_case_sens, LASTSNR2(bfwin->snr2)->unescape);
				DEBUG_MSG("snr2dialog_ok_lcb, result_all.doc=%p\n",result_all.doc);
				if (result_all.end > 0) {
					doc_show_result(result_all.doc, result_all.start, result_all.end);
				} else {
					info_dialog(bfwin->main_window,_("Search: no match found"), NULL);
				}
			} else {
				result = search_doc(bfwin,doc, LASTSNR2(bfwin->snr2)->search_pattern, LASTSNR2(bfwin->snr2)->matchtype_option, LASTSNR2(bfwin->snr2)->is_case_sens, startpos, LASTSNR2(bfwin->snr2)->unescape);
				if (result.end > 0) {
					doc_show_result(doc, result.start, result.end);
				} else {
					info_dialog(bfwin->main_window,_("Search: no match found"), NULL);
				}
			}
		}
	}
	/* if highlighting is needed for this document do this now !! */
	if (doc->need_highlighting && (doc->view_bars & VIEW_COLORIZED) ) {
		doc_highlight_full(doc);
	}
}

/**
 * snr2_run_extern_replace:
 * @doc: a #Tdocument
 * @search_pattern: #gchar* to search pattern
 * @region: #gint, 0 = region_from_beginning, 1 = region_from_cursor, 2 = region_selection, 3 = region_all_open_files
 * @matchtype: #gint, 0 = normal, 1 = posix, 2 = perl
 * @is_case_sens: #gint
 * @replace_pattern: #gchar* to replace pattern.
 * @store_as_last_snr2: Set to FALSE to keep the old last_snr2 after the snr has been completed.
 * 
 * Performs the specified replace action on the document by setting
 * a last_snr2 and calling snr2_run().
 *
 * Additional non-configureable arguments passed to snr2_run() via last_snr2:
 * replace = 1
 * prompt_before_replace = off
 * replace_once = off
 *
 * Return value: void
 **/
void snr2_run_extern_replace(Tdocument *doc, gchar *search_pattern, gint region,
							gint matchtype, gint is_case_sens, gchar *replace_pattern,
							gboolean store_as_last_snr2) {
	Tbfwin *bfwin = BFWIN(doc->bfwin);
	gchar *search_pattern_bck, *replace_pattern_bck;
	Tlast_snr2 last_snr2_bck;
	
	search_pattern_bck = LASTSNR2(bfwin->snr2)->search_pattern;
	replace_pattern_bck = LASTSNR2(bfwin->snr2)->replace_pattern;
	last_snr2_bck = *LASTSNR2(bfwin->snr2);
	DEBUG_MSG("snr2..extern..: last_snr2_bck.search_pattern=%p, replace_pattern=%p\n"
		,last_snr2_bck.search_pattern, last_snr2_bck.replace_pattern);

	if (!search_pattern || !replace_pattern || !strlen(search_pattern)) {
		DEBUG_MSG("snr2_run_extern, returning, non-valid arguments\n");
		return;
	}
	DEBUG_MSG("snr2..extern..: doc=%p, search_pattern='%s', region=%d, matchtype=%d, is_case_sens=%d, replace_pattern=%s, store_as_last=%d\n"
			,doc,search_pattern,region,matchtype,is_case_sens,replace_pattern,store_as_last_snr2);
	LASTSNR2(bfwin->snr2)->search_pattern = g_strdup(search_pattern);
	LASTSNR2(bfwin->snr2)->placetype_option = region;
 	LASTSNR2(bfwin->snr2)->is_case_sens = is_case_sens;
 	LASTSNR2(bfwin->snr2)->overlapping_search = 0;
	LASTSNR2(bfwin->snr2)->replace = 1;
	LASTSNR2(bfwin->snr2)->replace_pattern = g_strdup(replace_pattern);
 	LASTSNR2(bfwin->snr2)->prompt_before_replace = 0;
 	LASTSNR2(bfwin->snr2)->replace_once = 0;
 	LASTSNR2(bfwin->snr2)->unescape = 0;
	LASTSNR2(bfwin->snr2)->matchtype_option = matchtype;
 	LASTSNR2(bfwin->snr2)->replacetype_option = string;
 	LASTSNR2(bfwin->snr2)->bookmark_results = 0;
	snr2_run(BFWIN(doc->bfwin),doc);
	if (store_as_last_snr2) {
		DEBUG_MSG("free-ing old patterns at %p and %p\n",search_pattern_bck,replace_pattern_bck);
		g_free(search_pattern_bck);
		g_free(replace_pattern_bck);
	} else {
		g_free(LASTSNR2(bfwin->snr2)->search_pattern);
		g_free(LASTSNR2(bfwin->snr2)->replace_pattern);
		*LASTSNR2(bfwin->snr2) = last_snr2_bck;
	}
}

/**
 * doc_search_run_extern:
 * @doc: a #Tdocument
 * @search_pattern: #gchar search pattern
 * @matchtype: #gint, 0 = normal, 1 = posix, 2 = perl
 * @is_case_sens: #gint, case sensitive pattern?
 *
 * Frontend for search_doc, calling it with supplied arguments and startpos = 0.
 * 
 * Return value: #Tsearch_result_doc
 **/
Tsearch_result doc_search_run_extern(Tdocument *doc, gchar *search_pattern, gint matchtype, gint is_case_sens) {
	return search_doc(BFWIN(doc->bfwin),doc, search_pattern, matchtype, is_case_sens, 0, FALSE);
} 

/******************************************************/
/*        Search and replace dialogs callbacks        */
/******************************************************/

static void snr2dialog_destroy_lcb(GtkWidget *widget, gpointer data) {
	DEBUG_MSG("snr2dialog_destroy_lcb, started, about to call window_destroy\n");
	window_destroy(((Tsnr2_win *)data)->window);
	DEBUG_MSG("snr2dialog_destroy_lcb, about to free data %p\n", data);
	g_free(data);
	DEBUG_MSG("snr2dialog_destroy_lcb, done\n");
}

static void snr2dialog_cancel_lcb(GtkWidget *widget, gpointer data) {
	snr2dialog_destroy_lcb(NULL, data);
}

/*****************************************************/

/*
 * Sets the last_snr2 as specified by the user and calls snr2_run(NULL) (aka, run on current document)
 */
static void snr2dialog_ok_lcb(GtkWidget *widget, Tsnr2_win *data) {
	/*GtkTextIter itstart, itend;
	GtkTextBuffer *buf;*/
	Tbfwin *bfwin = data->bfwin;
	if (LASTSNR2(bfwin->snr2)->search_pattern) {
		g_free(LASTSNR2(bfwin->snr2)->search_pattern);
		LASTSNR2(bfwin->snr2)->search_pattern = NULL;
	}
	if (LASTSNR2(bfwin->snr2)->replace_pattern) {
		g_free(LASTSNR2(bfwin->snr2)->replace_pattern);
		LASTSNR2(bfwin->snr2)->replace_pattern = NULL;
	}
	/*buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->search_entry));
	gtk_text_buffer_get_bounds(buf,&itstart,&itend);
	LASTSNR2(bfwin->snr2)->search_pattern = gtk_text_buffer_get_text(buf,&itstart,&itend, FALSE);*/
	LASTSNR2(bfwin->snr2)->search_pattern = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(data->search_combo)->entry),0,-1);
	
	data->bfwin->session->searchlist = add_to_history_stringlist(data->bfwin->session->searchlist,LASTSNR2(bfwin->snr2)->search_pattern,TRUE,TRUE);
	
	LASTSNR2(bfwin->snr2)->unescape = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->unescape));
 	LASTSNR2(bfwin->snr2)->is_case_sens = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->is_case_sens));
 	LASTSNR2(bfwin->snr2)->overlapping_search = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->overlapping_search));
	if (data->replace) {
		/*GtkTextIter itstart, itend;
		GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->replace_entry));
		gtk_text_buffer_get_bounds(buf,&itstart,&itend);
		LASTSNR2(bfwin->snr2)->replace_pattern = gtk_text_buffer_get_text(buf,&itstart,&itend, FALSE);*/
		LASTSNR2(bfwin->snr2)->replace = 1;
		LASTSNR2(bfwin->snr2)->replace_pattern = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(data->replace_combo)->entry),0,-1);
		
		data->bfwin->session->replacelist = add_to_history_stringlist(data->bfwin->session->replacelist,LASTSNR2(bfwin->snr2)->replace_pattern,TRUE,TRUE);
		
	 	LASTSNR2(bfwin->snr2)->prompt_before_replace = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->prompt_before_replace));
	 	LASTSNR2(bfwin->snr2)->replace_once = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->replace_once));
	} else {
		LASTSNR2(bfwin->snr2)->replace = 0;
		LASTSNR2(bfwin->snr2)->bookmark_results = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->bookmark_results));
	}
	window_destroy(data->window);
	g_free(data);

	snr2_run(bfwin,NULL);
}

static void placetype_changed_lcb(GtkWidget *widget, Tsnr2_win *snr2win) {
	LASTSNR2(snr2win->bfwin->snr2)->placetype_option =  gtk_option_menu_get_history((GtkOptionMenu *) snr2win->placetype_option);
	DEBUG_MSG("placetype_changed_lcb, changing option to %d\n", LASTSNR2(snr2win->bfwin->snr2)->placetype_option);
}	

static void matchtype_changed_lcb(GtkWidget *widget, Tsnr2_win *snr2win) {
	LASTSNR2(snr2win->bfwin->snr2)->matchtype_option =  gtk_option_menu_get_history((GtkOptionMenu *) snr2win->matchtype_option);
	if (snr2win->replace) {
		if (LASTSNR2(snr2win->bfwin->snr2)->matchtype_option==0) {
			gtk_widget_hide(snr2win->subpat_help);
		}
		else {
			gtk_widget_show(snr2win->subpat_help);
		}
	}
	DEBUG_MSG("matchtype_changed_lcb, changing option to %d\n", LASTSNR2(snr2win->bfwin->snr2)->matchtype_option);
}	

static void replacetype_changed_lcb(GtkWidget *widget, Tsnr2_win *snr2win) {
	LASTSNR2(snr2win->bfwin->snr2)->replacetype_option =  gtk_option_menu_get_history((GtkOptionMenu *) snr2win->replacetype_option);
	/*gtk_widget_set_sensitive(snr2win->replace_entry, LASTSNR2(snr2win->bfwin->snr2)->replacetype_option==0);*/
	gtk_widget_set_sensitive(snr2win->replace_combo, LASTSNR2(snr2win->bfwin->snr2)->replacetype_option==0);
	DEBUG_MSG("replacetype_changed_lcb, changing option to %d\n", LASTSNR2(snr2win->bfwin->snr2)->replacetype_option);
}
/*
static gboolean search_entry_key_press_event_lcb(GtkWidget *widget,GdkEventKey *event,Tsnr2_win *snr2win) {
	if ((event->state & GDK_CONTROL_MASK) && (event->keyval == GDK_Return)) {
		snr2dialog_ok_lcb(NULL, snr2win);
		return TRUE;
	}
	return FALSE;
}*/

static void snr2dialog(Tbfwin *bfwin, gint is_replace, gint is_new_search) {
	Tsnr2_win *snr2win;
	GtkWidget *vbox, *hbox, *button, *table;
	const gchar *tmptext;
	gchar *buffer;
	GtkTextIter start, end;

	snr2win = g_malloc(sizeof(Tsnr2_win));
	snr2win->bfwin = bfwin;
	if (is_replace) {
		tmptext = _("Replace");
		snr2win->replace = 1;
	} else {
		tmptext = _("Find");
		snr2win->replace = 0;
	}
	if (is_new_search) {
		reset_last_snr2(bfwin);
	} else {
		LASTSNR2(bfwin->snr2)->result.start = -1;
		LASTSNR2(bfwin->snr2)->result.end = -1;
		LASTSNR2(bfwin->snr2)->doc = NULL;
	}
	snr2win->window = window_full(tmptext, GTK_WIN_POS_MOUSE, 12, G_CALLBACK(snr2dialog_destroy_lcb), snr2win, TRUE);
	gtk_window_set_role(GTK_WINDOW(snr2win->window), "snr");
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(snr2win->window), vbox);

	if (is_replace) {
		table = gtk_table_new (2, 2, FALSE);
	}
	else {
		table = gtk_table_new (1, 2, FALSE);
	}
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (table), 12);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);

	snr2win->search_label = gtk_label_new_with_mnemonic(_("_Search for: "));
	gtk_table_attach (GTK_TABLE (table), snr2win->search_label, 0, 1, 0, 1,
					(GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 2, 0);
	gtk_label_set_justify (GTK_LABEL (snr2win->search_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (snr2win->search_label), 0, 0.5);

	/*snr2win->search_scrollbox = textview_buffer_in_scrolwin(&snr2win->search_entry, 300, 50, LASTSNR2(bfwin->snr2)->search_pattern, GTK_WRAP_NONE);*/
	gtk_text_buffer_get_selection_bounds(bfwin->current_document->buffer, &start, &end);
	buffer = gtk_text_buffer_get_text(bfwin->current_document->buffer, &start, &end, FALSE);
	if (strchr(buffer,'\n')!=NULL) {
		/* a newline in the selection, we probably don't want this string as search string */
		g_free(buffer);
		buffer = NULL;
	}
	/* put the selected text in the search|find textfield */
	snr2win->search_combo = combo_with_popdown(buffer ? buffer : "", bfwin->session->searchlist, TRUE);
	if (buffer) g_free(buffer);
	/* TODO: should we free this variable ? */

	/* snr2win->search_combo = combo_with_popdown("", bfwin->session->searchlist, TRUE); */

	gtk_table_attach (GTK_TABLE (table), snr2win->search_combo, 1, 2, 0, 1,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);
	/*g_signal_connect(G_OBJECT(snr2win->search_entry), "key_press_event", G_CALLBACK(search_entry_key_press_event_lcb), snr2win);*/

	if (is_replace) {
		snr2win->replace_label = gtk_label_new_with_mnemonic(_("_Replace with: "));
		gtk_table_attach (GTK_TABLE (table), snr2win->replace_label, 0, 1, 1, 2,
						(GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 2, 0);
		gtk_label_set_justify (GTK_LABEL (snr2win->replace_label), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment (GTK_MISC (snr2win->replace_label), 0, 0.5);
		/*snr2win->replace_scrollbox = textview_buffer_in_scrolwin(&snr2win->replace_entry, 300, 50, LASTSNR2(bfwin->snr2)->replace_pattern, GTK_WRAP_NONE);*/
		snr2win->replace_combo = combo_with_popdown("", bfwin->session->replacelist, TRUE);
		gtk_table_attach (GTK_TABLE (table), snr2win->replace_combo, 1, 2, 1, 2,
						(GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);
		/*g_signal_connect(G_OBJECT(snr2win->replace_entry), "key_press_event", G_CALLBACK(search_entry_key_press_event_lcb), snr2win);*/
		snr2win->subpat_help = gtk_label_new(_("\\0 refers to the first subsearch_pattern, \\1 to the second etc."));
		gtk_box_pack_start(GTK_BOX(vbox), snr2win->subpat_help, FALSE, TRUE, 6);
		gtk_label_set_justify (GTK_LABEL (snr2win->subpat_help), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment (GTK_MISC (snr2win->subpat_help), 1, 0.5);
	}

	if (is_replace) {
		table = gtk_table_new (3, 2, FALSE);
	} else {
		table = gtk_table_new (2, 2, FALSE);
	}
	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 12);
	gtk_table_set_row_spacings (GTK_TABLE (table), 12);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	{
		gchar *placeactions[] = {N_("Beginning of document till end"), N_("Current position till end"),	N_("Beginning of selection till end of selection"), N_("All opened files begin till end"), NULL};
		gchar *matchactions[] = {N_("Disabled"), N_("POSIX type"),	N_("PERL type"), NULL};
		GtkWidget *matchlabel, *placelabel = gtk_label_new(_("Starts at:"));
		DEBUG_MSG("snr2dialog, LASTSNR2(bfwin->snr2)->placetype_option=%d\n", LASTSNR2(bfwin->snr2)->placetype_option);
		snr2win->placetype_option = optionmenu_with_value(placeactions, LASTSNR2(bfwin->snr2)->placetype_option);

		gtk_table_attach (GTK_TABLE (table), placelabel, 0, 1, 0, 1,
						(GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 2, 0);
		gtk_label_set_justify (GTK_LABEL (placelabel), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment (GTK_MISC (placelabel), 0, 0.5);
		gtk_table_attach (GTK_TABLE (table), snr2win->placetype_option, 1, 2, 0, 1, 
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);
		g_signal_connect(G_OBJECT((GtkWidget *) snr2win->placetype_option), "changed", G_CALLBACK(placetype_changed_lcb), snr2win);
		
		matchlabel = gtk_label_new(_("Regular expression:"));
		DEBUG_MSG("snr2dialog, LASTSNR2(bfwin->snr2)->matchtype_option=%d\n", LASTSNR2(bfwin->snr2)->matchtype_option);
		snr2win->matchtype_option = optionmenu_with_value(matchactions, LASTSNR2(bfwin->snr2)->matchtype_option);

		gtk_table_attach (GTK_TABLE (table), matchlabel, 0, 1, 1, 2,
						(GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 2, 0);
		gtk_label_set_justify (GTK_LABEL (matchlabel), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment (GTK_MISC (matchlabel), 0, 0.5);
		gtk_table_attach (GTK_TABLE (table), snr2win->matchtype_option, 1, 2, 1, 2, 
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);
		g_signal_connect(G_OBJECT((GtkWidget *) snr2win->matchtype_option), "changed", G_CALLBACK(matchtype_changed_lcb), snr2win);


		if (is_replace) {
			gchar *replaceactions[] = {N_("Normal"), N_("Uppercase"),	N_("Lowercase"), NULL};
			GtkWidget *replacelabel = gtk_label_new(_("Replace type:"));
			DEBUG_MSG("snr2dialog, LASTSNR2(bfwin->snr2)->replacetype_option=%d\n", LASTSNR2(bfwin->snr2)->replacetype_option);
			snr2win->replacetype_option = optionmenu_with_value(replaceactions, LASTSNR2(bfwin->snr2)->replacetype_option);
	
			gtk_table_attach (GTK_TABLE (table), replacelabel, 0, 1, 2, 3, 
							(GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 2, 0);
			gtk_label_set_justify (GTK_LABEL (replacelabel), GTK_JUSTIFY_LEFT);
			gtk_misc_set_alignment (GTK_MISC (replacelabel), 0, 0.5);
			gtk_table_attach (GTK_TABLE (table), snr2win->replacetype_option, 1, 2, 2, 3, 
							(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);
			g_signal_connect(G_OBJECT((GtkWidget *) snr2win->replacetype_option), "changed", G_CALLBACK(replacetype_changed_lcb), snr2win);
		}
	}
	snr2win->unescape = boxed_checkbut_with_value(_("_Patterns contain backslash escape sequences (\\n, \\t)"), LASTSNR2(bfwin->snr2)->unescape, vbox);
	snr2win->is_case_sens = boxed_checkbut_with_value(_("_Match case"), LASTSNR2(bfwin->snr2)->is_case_sens, vbox);
	snr2win->overlapping_search = boxed_checkbut_with_value(_("O_verlap searches"), LASTSNR2(bfwin->snr2)->overlapping_search, vbox);
	if (is_replace) {
		snr2win->prompt_before_replace = boxed_checkbut_with_value(_("Prompt _before replace"), LASTSNR2(bfwin->snr2)->prompt_before_replace, vbox);
		snr2win->replace_once = boxed_checkbut_with_value(_("Replace o_nce"), LASTSNR2(bfwin->snr2)->replace_once, vbox);
	} else {
		snr2win->bookmark_results = boxed_checkbut_with_value(_("Bookmark results"), LASTSNR2(bfwin->snr2)->bookmark_results, vbox);
	}

	hbox = gtk_hseparator_new ();
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 12);

	hbox = gtk_hbutton_box_new();
	gtk_hbutton_box_set_layout_default(GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), bf_stock_cancel_button(G_CALLBACK(snr2dialog_cancel_lcb), snr2win), FALSE, TRUE, 0);
	button = bf_stock_ok_button(G_CALLBACK(snr2dialog_ok_lcb), snr2win);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);
	gtk_window_set_default(GTK_WINDOW(snr2win->window), button);
	
	gtk_label_set_mnemonic_widget(GTK_LABEL(snr2win->search_label), GTK_COMBO(snr2win->search_combo)->entry);
	if(is_replace) {
		gtk_label_set_mnemonic_widget(GTK_LABEL(snr2win->replace_label), GTK_COMBO(snr2win->replace_combo)->entry);
	}
	gtk_widget_grab_focus(snr2win->search_combo);
	gtk_widget_show_all(vbox);

	gtk_widget_show(snr2win->window);
/*	{
		GtkTextIter itstart, itend;
		gtk_text_buffer_get_bounds(gtk_text_view_get_buffer(GTK_TEXT_VIEW(snr2win->search_entry)),&itstart,&itend);
		gtk_text_buffer_move_mark_by_name(gtk_text_view_get_buffer(GTK_TEXT_VIEW(snr2win->search_entry)),"insert",&itstart);
		gtk_text_buffer_move_mark_by_name(gtk_text_view_get_buffer(GTK_TEXT_VIEW(snr2win->search_entry)),"selection_bound",&itend);
	}*/
	
	if (is_replace) {
		matchtype_changed_lcb(NULL, snr2win);
		replacetype_changed_lcb(NULL, snr2win);
	}
}

/*****************************************************/

void search_from_selection(Tbfwin *bfwin) {
	gchar *string;
	GtkClipboard* cb;

	cb = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	string = gtk_clipboard_wait_for_text(cb);
	if (string) {
		if (LASTSNR2(bfwin->snr2)->search_pattern) {
			g_free(LASTSNR2(bfwin->snr2)->search_pattern);
		}
		if (LASTSNR2(bfwin->snr2)->replace_pattern) {
			g_free(LASTSNR2(bfwin->snr2)->replace_pattern);
			LASTSNR2(bfwin->snr2)->replace_pattern = NULL;
		}
		LASTSNR2(bfwin->snr2)->search_pattern = string;
		LASTSNR2(bfwin->snr2)->placetype_option = 0;
 		LASTSNR2(bfwin->snr2)->is_case_sens = 1;
	 	LASTSNR2(bfwin->snr2)->overlapping_search = 0;
		LASTSNR2(bfwin->snr2)->replace = 0;
		LASTSNR2(bfwin->snr2)->matchtype_option = 0;
		snr2_run(bfwin,bfwin->current_document);
	}
}

/**
 * search_cb:
 * @widget: unused #GtkWidget*
 * @data: unused #gpointer
 * 
 * Show the search-dialog.
 * 
 * Return value: void
 **/
void search_cb(GtkWidget *widget, Tbfwin *bfwin) {
	snr2dialog(bfwin, 0, 0);
}

/**
 * search_again_cb:
 * @widget: unused #GtkWidget*
 * @data: unused #gpointer
 * 
 * Repeat last search, if any.
 * 
 * Return value: void
 **/ 
void search_again_cb(GtkWidget *widget, Tbfwin *bfwin) {
	snr2_run(bfwin,NULL);	
}

/**
 * replace_again_cb:
 * @widget: unused #GtkWidget*
 * @data: unused #gpointer
 * 
 * Repeat last replace, if any.
 * 
 * Return value: void
 **/ 
void replace_again_cb(GtkWidget *widget, Tbfwin *bfwin) {
	snr2_run(bfwin,NULL);
}

/**
 * replace_cb:
 * @widget: unused #GtkWidget*
 * @data: unused #gpointer
 * 
 * Show replace dialog.
 * 
 * Return value: void
 **/ 
void replace_cb(GtkWidget *widget, Tbfwin *bfwin) {
	snr2dialog(bfwin, 1, 0);
}

/*****************************************************/

#ifdef NOTPORTEDYET

static gint do_filename_curfile_replace(gchar *fulltext, Tsearch_result result, gint offset, gchar *olddirname, gchar *newfilename, gint changelen, Tdocument *doc) {
/* returns the change in the lenght of the buffer compared to the actual document text */
	gchar *possible_filename;
	gchar *olddirfile;
	gint len;
	gchar *replacestring;
	gint change_lenght = changelen;

	possible_filename = g_strndup(&fulltext[offset + result.bstart +1], result.bend - result.bstart -2 );
	DEBUG_MSG("do_filename_change_replace, possible_filename=%s\n", possible_filename);
	len = strlen(possible_filename) + strlen(olddirname) + 2;
	olddirfile = strncat(strncat(strncpy(g_malloc(len), olddirname, len), "/", len), possible_filename, len);

	/* code to check if this is a file */
	if (file_exists_and_readable(olddirfile)) {
		DEBUG_MSG("do_filename_change_replace, olddirfile=%s does exist!!\n", olddirfile);
		/* code to create replace_pattern */
		olddirfile = most_efficient_filename(olddirfile);
		DEBUG_MSG("do_filename_change_replace, updated olddirfile=%s \n", olddirfile);
		replacestring = create_relative_link_to(newfilename, olddirfile);
		DEBUG_MSG("do_filename_change_replace, replacestring=%s, newfilename=%s\n", replacestring, newfilename);
		/* code to actual replace it */
		doc_replace_text(doc, replacestring, result.start + offset + 1 + changelen, result.end + offset + changelen -1);	
		change_lenght = strlen(replacestring) - strlen(possible_filename) + changelen;
		DEBUG_MSG("do_filename_change_replace, replacestring=%s, possible_filename=%s\n", replacestring, possible_filename);
		DEBUG_MSG("do_filename_change_replace, change_lenght=%d\n",change_lenght );		
		g_free(replacestring); 
	} else {
		DEBUG_MSG("do_filename_change_replace, olddirfile=%s does NOT exist\n", olddirfile);  
	}
	g_free(possible_filename);
	g_free(olddirfile);
	return change_lenght;
}

static gint do_filename_otherfile_replace(gchar *fulltext, Tsearch_result result, gint offset, gchar *oldfilename, gchar *newfilename, gint changelen, Tdocument *doc) {
	gchar *possible_filename, *replacestring, *foundstring;
	gint change_length = changelen;
	gchar *eff_my_filename;

	foundstring = g_strndup(&fulltext[offset + result.bstart +1], result.bend - result.bstart -2 );
	possible_filename = most_efficient_filename(create_full_path(foundstring, doc->filename));
	g_free(foundstring);
	DEBUG_MSG("do_filename_otherfile_replace, posname=%s\n", possible_filename);
	DEBUG_MSG("do_filename_otherfile_replace, newname=%s\n", oldfilename);
	if (strcmp(possible_filename, oldfilename) == 0) {
		eff_my_filename = most_efficient_filename(g_strdup(doc->filename));
		replacestring = create_relative_link_to(eff_my_filename, newfilename);
		doc_replace_text(doc, replacestring, result.start + offset + 1 + changelen, result.end + offset + changelen -1);
		change_length = strlen(replacestring) - strlen(possible_filename) + changelen;		
		g_free(eff_my_filename);
		g_free(replacestring);
	}
	g_free(possible_filename);
	return change_length;
}

/**
 * update_filenames_in_file:
 * @doc: a #Tdocument
 * @oldfilename: Filename to change from.
 * @newfilename: Filename to change to.
 * @doc_has_newfilename: If *doc (blabla?)
 * 
 * 
 *
 * Return value: void
 **/
void update_filenames_in_file(Tdocument *doc, gchar *oldfilename, gchar *newfilename, gint doc_has_newfilename) {
	gchar *fulltext;
	Tsearch_result result;
	gint cur_offset, changelen=0;
	gchar *search_pattern;
	Tmatch_types matchtype;
	gint is_case_sens;
	gchar *olddirname=NULL;

	if ((oldfilename == NULL)|| (newfilename == NULL)) {
		return;
	}
	search_pattern = "\"[^\"]+\"";
	matchtype = match_posix;
 	is_case_sens = 0;
	cur_offset = 0;

	if (doc_has_newfilename) {
			olddirname = g_path_get_dirname(oldfilename);
	}

	fulltext = doc_get_chars(doc, 0, -1);
	utf8_offset_cache_reset();
	result = search_backend(search_pattern, matchtype, is_case_sens, fulltext, 0);
	while (result.end > 0) {
		if (doc_has_newfilename) {
			changelen = do_filename_curfile_replace(fulltext, result, cur_offset, olddirname, newfilename, changelen, doc);
		} else {
			changelen = do_filename_otherfile_replace(fulltext, result, cur_offset, oldfilename, newfilename, changelen, doc);
		}
		cur_offset += result.bstart +1;
		result = search_backend(search_pattern, matchtype, is_case_sens, &fulltext[cur_offset], 0);
	}
	g_free(fulltext);
	if (doc_has_newfilename) {
		g_free(olddirname);
	}
}
#endif /* NOTPORTEDYET */
