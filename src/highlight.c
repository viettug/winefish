/* $Id$ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * highlight2.c - the syntax highlighting with perl compatible regular expressions
 *
 * Copyright (C) 2002 Olivier Sessink
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
 *
 *
 * indenting is done with
 * indent --line-length 100 --k-and-r-style --tab-size 4 -bbo --ignore-newlines highlight.c
 */
/*#define HL_TIMING
#define HL_DEBUG 
#define DEBUG*/

#ifdef DEBUG
#define DEVELOPMENT
#endif

#ifdef HL_TIMING /* some overall profiling information, not per pattern, but per type of code , interesting to bluefish programmers*/
#include <sys/times.h>
#include <unistd.h>
#endif

#include <gtk/gtk.h>
#include <sys/types.h>			/* _before_ regex.h for freeBSD */
#include <pcre.h>				/* pcre_*() */
#include <string.h>				/* strerror() */
#include <stdlib.h>				/* atoi() */

#include "config.h" /* HL_PROFILING might be defined there */

#ifdef HL_PROFILING /* per pattern profiling information, interesting for users making a new pattern */
#include <sys/times.h>
#include <unistd.h>
#endif

#include "bluefish.h"
#include "bf_lib.h"				/* filename_test_extensions() */
#include "rcfile.h"				/* array_from_arglist() */
#include "stringlist.h" 	/* count_array() */
#include "menu.h" 			/* menu_current_document_set_toggle_wo_activate */
#include "document.h" /* doc_get_chars() */
#include "highlight.h"
#include "gtk_easy.h" /* error_dialog() */

#define MAX_OVECTOR 30 /* should be a multiple of three for pcre_exec(),
and at maximum 2/3 of this size can really be used for substrings */
#define MIN_OVECTOR 9 /* the minimum size for the ovector */
typedef struct
{
	pcre *pcre;
	pcre_extra *pcre_e;
}
Treg;

typedef struct
{
gchar *name;
GtkTextTag *tag;
}
Thlstyle;

typedef struct
{
Treg reg1;
Treg reg2;
gint numsub;
gint mode;
GList *childs;
GtkTextTag *tag;
#ifdef HL_PROFILING
gint runcount;
gint applycount;
struct tms tms1;
struct tms tms2;
glong total_ms;
#endif
int *ovector;
gint ovector_size;
gboolean is_match;
}
Tpattern;

#define PATTERN(var) ((Tpattern *)(var))

/*
proposed improvement (for speedup, but also simplicity):
	-add the int ovector[MAX_OVECTOR]; and gboolean is_match; fields to Tpattern
	-use the pcre_fullinfo() function to find the minimum required ovector[] length (remind the 3X!!)
	and if there are no child-patterns that need a subpattern we use the minimum
	-the results from a subpattern search are now automatically stored, improving the success ratio
	for applying/matching*100% as in the profiling information
	-the Tpatmatch structure is not needed anymore, also the start of applylevel() where this 
	structure is initialized is now more simple
*/


typedef struct
{
gchar *filetype;
gchar *name;
gchar *parentmatch;
pcre *pcre;
Tpattern *pat;
}
Tmetapattern; /* mostly used during compiling since it contains some more meta-info about the pattern */

typedef struct
{
GtkTextTagTable *tagtable; /* this one should ultimately move to Tfiletype, so every set would have it's own tagtable, but there is currently no way to switch a document to a new tagtable */
GList *all_highlight_patterns; /* contains Tmetapattern, not Tpattern !! */
#ifdef HL_PROFILING
struct tms tms1; /* start time for profiling info */
struct tms tms2; /* stop time for profiling info */
#endif
}
Thighlight;

/*typedef struct {
	int ovector[MAX_OVECTOR];
	gboolean is_match;
	Tpattern *pat;
} Tpatmatch;*/

/***************************************************************
how it works:
 
-types:
	1 - a start pattern and an end pattern
	2 - a pattern that defines the complete match
	3 - a sub-pattern , this must be a child pattern for type 2
 
-matching
   all top-level patterns are matched, a matching table is formed
	with all the start positions for every type, the lowest start 
	position is the pattern where we start.
	for this pattern we crate a new matching table with all the start 
	values AND the end value for the parent pattern. If the parent 
	pattern is type 2 or 3 this value is fixed, and no pattern or 
	child-pattern can go beyond that value, if the parent is type 1 
	the end value is re-evaluated once another pattern has gone beyond
	it's value.
	Once a match is found, the text is tagged with the right style, and
***************************************************************/

/*********************************/
/* global vars for this module   */
/*********************************/
static Thighlight highlight;

/*********************************/
/* debugging                     */
/*********************************/
#ifdef DEBUG

static void print_meta_for_pattern(Tpattern *pat)
{
	GList *tmplist = g_list_first(highlight.all_highlight_patterns);
	while (tmplist) {
		if (((Tmetapattern *)tmplist->data)->pat == pat) {
			/*			g_print("pattern %p: filetype=%s, name=%s, num_childs=%d, numsub=%d, mode=%d\n", pat, ((Tmetapattern *)tmplist->data)->filetype, ((Tmetapattern *)tmplist->data)->name, g_list_length(pat->childs), pat->numsub, pat->mode);*/
			g_print("pattern %p: name=%s\n",pat,((Tmetapattern *)tmplist->data)->name);
			return;
		}
		tmplist = g_list_next(tmplist);
	}
}

static Tmetapattern *get_metapattern_from_tag(GtkTextTag *tag)
{
	GList *tmplist = g_list_first(highlight.all_highlight_patterns);
	while (tmplist) {
		if (((Tmetapattern *)tmplist->data)->pat->tag == tag) {
			return ((Tmetapattern *)tmplist->data);
		}
		tmplist = g_list_next(tmplist);
	}
	return NULL;
}

static void print_meta_for_tag(GtkTextTag *tag)
{
	Tmetapattern *mpat = get_metapattern_from_tag(tag);
	if (mpat) {
		DEBUG_MSG("tag %p: filetype=%s, name=%s, num_childs=%d\n", tag, mpat->filetype, mpat->name, g_list_length(mpat->pat->childs));
	}
}

static char *get_metaname_from_tag(GtkTextTag *tag)
{
	Tmetapattern *mpat = get_metapattern_from_tag(tag);
	if (mpat) {
		return mpat->name;
	}
	return "";
}

/*
because the gtk functions crash in one case, I tries these, and that works!
*/
static gboolean my_own_iter_backward_to_tag_toggle(GtkTextIter *iter,GtkTextTag *tag)
{
	while (gtk_text_iter_backward_char(iter)) {
		if (gtk_text_iter_toggles_tag(iter, tag))	return TRUE;
	}
	return FALSE;
}
static gboolean my_own_iter_forward_to_tag_toggle(GtkTextIter *iter,GtkTextTag *tag)
{
	while (gtk_text_iter_forward_char(iter)) {
		if (gtk_text_iter_toggles_tag(iter, tag))	return TRUE;
	}
	return FALSE;
}
#endif /* DEBUG */

#ifdef HL_PROFILING
static void hl_profiling_reset(Tdocument *doc)
{
	/* reset all patterns for the current filetype */
	GList *tmplist = g_list_first(highlight.all_highlight_patterns);
	while (tmplist) {
		Tmetapattern *mpat = (Tmetapattern *)tmplist->data;
		if (strcmp(mpat->filetype, doc->hl->type)==0) {
			mpat->pat->runcount = 0;
			mpat->pat->applycount = 0;
			mpat->pat->total_ms = 0;
		}
		tmplist = g_list_next(tmplist);
	}
	times(&highlight.tms1);
}

static void hl_profiling_tagstart(Tpattern *pat)
{
	times(&pat->tms1);
	pat->runcount++;
}

static void hl_profiling_tagstop(Tpattern *pat)
{
	times(&pat->tms2);
	pat->total_ms += (glong) (double) ((pat->tms2.tms_utime - pat->tms1.tms_utime) * 1000 / sysconf(_SC_CLK_TCK));
}

static void hl_profiling_tagapply(Tpattern *pat)
{
	pat->applycount++;
}

static void hl_profiling_print(Tdocument *doc)
{
	GList *tmplist;
	glong tot_ms;
	/* print results for all patterns for the current filetype */
	times(&highlight.tms2);
	tot_ms = (glong) (double) ((highlight.tms2.tms_utime - highlight.tms1.tms_utime) * 1000 / sysconf(_SC_CLK_TCK));
	g_print("PROFILING: total time %ld ms\n", tot_ms);
	tmplist = g_list_first(highlight.all_highlight_patterns);
	while (tmplist) {
		Tmetapattern *mpat = (Tmetapattern *)tmplist->data;
		if (strcmp(mpat->filetype, doc->hl->type)==0) {
			g_print("PROFILING: patterns %s ran %d times, %d times successful (%d%%), and was matching %ld ms\n", mpat->name, mpat->pat->runcount, mpat->pat->applycount,(int)(100.0*mpat->pat->applycount/mpat->pat->runcount),mpat->pat->total_ms);
		}
		tmplist = g_list_next(tmplist);
	}
	g_print("-- -- -- -- -- --\n");
}

#endif /* HL_PROFILING */

#ifdef HL_TIMING
typedef struct
{
	struct tms tms1;
	struct tms tms2;
	glong total_ms;
	gint numtimes;
}
Ttiming;
#define TIMING_TEXTBUF 0
#define TIMING_PCRE_EXEC 1
#define TIMING_TOTAL 2
#define TIMING_UTF8 3
#define TIMING_UTF8_INV 4
#define TIMING_TEXTBUF_ITER 5
#define TIMING_TEXTBUF_TAG 6
#define TIMING_LINE_HIGHLIGHTING 7
#define TIMING_NUM 8
static Ttiming timing[TIMING_NUM];
static void timing_init()
{
	gint i;
	for (i=0;i<TIMING_NUM;i++) {
		timing[i].total_ms = 0;
		timing[i].numtimes = 0;
	}
}
static void timing_start(gint id)
{
	times(&timing[id].tms1);
}
static void timing_stop(gint id)
{
	times(&timing[id].tms2);
	timing[id].numtimes++;
	timing[id].total_ms += (int) (double) ((timing[id].tms2.tms_utime - timing[id].tms1.tms_utime) * 1000.0 / sysconf(_SC_CLK_TCK));
}
#endif /* HL_TIMING */


/*********************************/
/* initializing the highlighting */
/*********************************/

static void highlight_error(gboolean gui_errors, gchar *str1, gchar *str2)
{
	if (gui_errors) {
		error_dialog(NULL,str1, str2);
	} else {
		gchar *message = g_strconcat(str1, ", ", str2, "\n", NULL);
		g_print(message);
		g_free(message);
	}
}

static void compile_pattern(gboolean gui_errors, gchar *filetype, gchar *name, gint case_insens
                            , gchar *pat1, gchar *pat2, gint mode, gchar *parentmatch
                            , gchar *fgcolor, gchar *bgcolor, gint weight, gint style)
{
	/*
	 * test if the pattern is correct 
	 */
	if (!name || strlen(name) == 0) {
		g_print("error compiling nameless pattern: name is not set\n");
		return;
	}
	switch (mode) {
	case 1:
		if (!(pat1 && pat2 && strlen(pat1) && strlen(pat2))) {
			g_print("error compiling pattern '%s' for mode 1: some pattern(s) missing\n", name);
			return;
		}
		break;
	case 2:
		if (!(pat1 && strlen(pat1))) {
			g_print("error compiling pattern '%s' for mode 2: pattern missing\n", name);
			return;
		}
		break;
	case 3:
		if (!(pat1 && strlen(pat1) && atoi(pat1) > 0 && atoi(pat1) < MAX_OVECTOR)) {
			g_print("error compiling pattern '%s' for mode 3: sub-pattern number missing, too large or incorrect\n", name);
			return;
		}
		break;
	default:
		g_print("error compiling pattern '%s', mode %d unknown\n", name, mode);
		return;
		break;
	}

	{
		Tmetapattern *mpat;
		Tpattern *pat = g_new0(Tpattern, 1);
		pat->mode = mode;
		if (mode == 1 || mode ==2) {
			const char *err=NULL;
			int erroffset=0;
			DEBUG_MSG("compiling pat1 '%s'\n", pat1);
			pat->reg1.pcre = pcre_compile(pat1,
			                              (case_insens) ? PCRE_CASELESS|PCRE_MULTILINE|PCRE_DOTALL : PCRE_MULTILINE|PCRE_DOTALL,
			                              &err,&erroffset,NULL);
			if (err) {
				gchar *str1, *str2;
				str1 = g_strconcat(_("Syntax highlighting error for "),filetype," - ",name,NULL);
				str2 = g_strdup_printf(_("compiling pattern '%s': %s at offset %d"), pat1, err, erroffset);
				highlight_error(gui_errors, str1, str2);
				g_free(str1);
				g_free(str2);
				g_free(pat);
				return;
			} else {
				DEBUG_MSG("result: pat->reg1.pcre=%p\n", pat->reg1.pcre);
				pat->reg1.pcre_e = pcre_study(pat->reg1.pcre,0,&err);
				if (err) {
					gchar *str1, *str2;
					str1 = g_strconcat(_("Syntax highlighting error for "),filetype," - ",name,NULL);
					str2 = g_strdup_printf(_("studying pattern '%s': %s"), pat1, err);
					highlight_error(gui_errors, str1, str2);
					g_free(str1);
					g_free(str2);
					pcre_free(pat->reg1.pcre);
					g_free(pat);
					return;
				} else {
					if (pcre_fullinfo(pat->reg1.pcre,pat->reg1.pcre_e,PCRE_INFO_CAPTURECOUNT,&pat->ovector_size)!=0) {
						g_print("Error for %s - %s, gettting info for pattern '%s'\n", filetype, name, pat1);
					}
					if (pat->ovector_size > MAX_OVECTOR) pat->ovector_size = MAX_OVECTOR;
					if (pat->ovector_size < MIN_OVECTOR) pat->ovector_size = MIN_OVECTOR;
					pat->ovector = g_malloc((pat->ovector_size+1)*3*sizeof(int));
				}
			}
		}
		if (mode == 1) {
			const char *err=NULL;
			int erroffset=0;
			DEBUG_MSG("compiling pat2 '%s'\n", pat2);
			pat->reg2.pcre = pcre_compile(pat2,
			                              (case_insens) ? PCRE_CASELESS|PCRE_MULTILINE|PCRE_DOTALL : PCRE_MULTILINE|PCRE_DOTALL,
			                              &err,&erroffset,NULL);
			if (err) {
				gchar *str1, *str2;
				str1 = g_strconcat(_("Syntax highlighting error for "),filetype," - ",name,NULL);
				str2 = g_strdup_printf(_("compiling 2nd pattern '%s': %s at offset %d"), pat2, err, erroffset);
				highlight_error(gui_errors, str1, str2);
				g_free(str1);
				g_free(str2);
				pcre_free(pat->reg1.pcre);
				g_free(pat->ovector);
				g_free(pat);
				return;
			}
			DEBUG_MSG("result: pat->reg2.pcre=%p\n", pat->reg2.pcre);
			pat->reg2.pcre_e = pcre_study(pat->reg2.pcre,0,&err);
			if (err) {
				gchar *str1, *str2;
				str1 = g_strconcat(_("Syntax highlighting error for "),filetype," - ",name,NULL);
				str2 = g_strdup_printf(_("studying 2nd pattern '%s': %s"), pat2, err);
				highlight_error(gui_errors, str1, str2);
				g_free(str1);
				g_free(str2);
				pcre_free(pat->reg1.pcre);
				pcre_free(pat->reg2.pcre);
				g_free(pat->ovector);
				g_free(pat);
				return;
			}
		}
		if (mode == 3) {
			pat->numsub = atoi(pat1);
			pat->ovector_size = 3;
			pat->ovector = g_malloc((pat->ovector_size+1)*3*sizeof(int));
		}

		pat->tag = gtk_text_tag_new(NULL);
		if (strlen(fgcolor)) {
			g_object_set(pat->tag, "foreground", fgcolor, NULL);
		}
		if (strlen(bgcolor)) {
			g_object_set(pat->tag, "background", bgcolor, NULL);
		}
		if (weight > 0) {
			if (1 == weight) {
				g_object_set(pat->tag, "weight", PANGO_WEIGHT_NORMAL, NULL);
			} else {
				g_object_set(pat->tag, "weight", PANGO_WEIGHT_BOLD, NULL);
			}
		}
		if (style > 0) {
			if (1 == style) {
				g_object_set(pat->tag, "style", PANGO_STYLE_NORMAL, NULL);
			} else {
				g_object_set(pat->tag, "style", PANGO_STYLE_ITALIC, NULL);
			}
		}
		DEBUG_MSG("adding tag %p to table %p\n", pat->tag, highlight.tagtable);
		gtk_text_tag_table_add(highlight.tagtable, pat->tag);
		/* this might fix a memory leak reported by Jim Hayward <jimhayward@linuxexperience.com> Fri, 13 Aug 2004 14:45:23 -0700 */
		g_object_unref(pat->tag);
		/* from the documentation:
			When adding a tag to a tag table, it will be assigned the highest priority in the table by 
			default; so normally the precedence of a set of tags is the order in which they were added 
			to the table.
			so the order of the styles in the list will be the order in the tagtable */

		mpat = g_new(Tmetapattern, 1);
		mpat->pat = pat;
		mpat->name = g_strdup(name);
		if (strlen(parentmatch)) {
			mpat->parentmatch = g_strdup(parentmatch);
		} else {
			mpat->parentmatch = g_strdup("^top$");
		}
		mpat->filetype = g_strdup(filetype);
		DEBUG_MSG("adding mpat %s (%p) to list %p\n", mpat->name, mpat, highlight.all_highlight_patterns);
		highlight.all_highlight_patterns = g_list_append(highlight.all_highlight_patterns, mpat);
		DEBUG_MSG("finished compiling pattern %s and added it to the list\n", mpat->name);
	}
}

static void add_patterns_to_parent(GList **list, gchar *filetype, gchar *name)
{
	GList *tmplist = g_list_first(highlight.all_highlight_patterns);
	while (tmplist) {
		Tmetapattern *mpat = (Tmetapattern *)tmplist->data;
		if (strcmp(filetype, mpat->filetype)==0) {
			int ovector[9];
			if (pcre_exec(mpat->pcre,NULL, name,strlen(name),0,0,ovector,9) > 0) {
				*list = g_list_append(*list, mpat->pat);
			}
		}
		tmplist = g_list_next(tmplist);
	}
}

/*
 * if gui_errors is set, we can send a popup with an error message,
 * else (during startup) we use g_print()
 */
void filetype_highlighting_rebuild(gboolean gui_errors)
{
	GList *tmplist;
	GList *alldoclist;

	alldoclist = return_allwindows_documentlist();
	/* remove filetypes from documents, but to reconnect them
	again after the rebuild, we temporary put a string with 
	the filetype on that pointer */
	if (alldoclist) {
		tmplist = g_list_first(alldoclist);
		while (tmplist) {
			Tdocument *thisdoc = (Tdocument *)tmplist->data;
			if (thisdoc->hl) {
				DEBUG_MSG("doc %p has type %p named %s\n", thisdoc, thisdoc->hl, thisdoc->hl->type);
				DEBUG_MSG("disconnected document %p from filetype %s\n", thisdoc, thisdoc->hl->type);
				thisdoc->hl = (gpointer)g_strdup(thisdoc->hl->type);
			}
			tmplist = g_list_next(tmplist);
		}
	}

	/* first remove the menu widgets, and delete the filetype structs */
	DEBUG_MSG("filetype_highlighting_rebuild, testing for filetypelist existance\n");
	filetype_menus_empty();
	tmplist = g_list_first(main_v->filetypelist);
	while (tmplist) {
		Tfiletype *filetype = (Tfiletype *)tmplist->data;
		g_free(filetype->type);
		g_strfreev(filetype->extensions);
		g_free(filetype->update_chars);
		if (filetype->icon) {
			g_object_unref(filetype->icon);
		}
		g_free(filetype->content_regex);
		/* the highlightpatterns are freed separately, see below */
		g_free(filetype);
		tmplist = g_list_next(tmplist);
	}
	g_list_free(main_v->filetypelist);
	main_v->filetypelist = NULL;

	DEBUG_MSG("filetype_highlighting_rebuild, testing for metapattern existence\n");
	if (highlight.all_highlight_patterns) {
		tmplist = g_list_first(highlight.all_highlight_patterns);
		while (tmplist) {
			Tmetapattern *mpat = tmplist->data;
			pcre_free(mpat->pat->reg1.pcre);
			pcre_free(mpat->pat->reg1.pcre_e);
			pcre_free(mpat->pat->reg2.pcre);
			pcre_free(mpat->pat->reg2.pcre_e);
			g_free(mpat->pat->ovector);
			gtk_text_tag_table_remove(highlight.tagtable,mpat->pat->tag);
			g_free(mpat->pat);
			g_free(mpat->name);
			g_free(mpat->parentmatch);
			g_free(mpat->filetype);
			g_free(mpat);
			tmplist = g_list_next(tmplist);
		}
		g_list_free(highlight.all_highlight_patterns);
		highlight.all_highlight_patterns = NULL;
	}


	DEBUG_MSG("filetype_highlighting_rebuild, rebuilding the filetype list\n");
	/* now rebuild the filetype list */
	tmplist = g_list_first(main_v->props.filetypes);
	while (tmplist) {
		gint arrcount;
		gchar **strarr;
		Tfiletype *filetype;
		strarr = (gchar **) tmplist->data;
		arrcount = count_array(strarr);
		if (arrcount ==7  ) {
			filetype = g_new(Tfiletype, 1);
			filetype->editable = (strarr[4][0] != '0');
			filetype->content_regex = g_strdup(strarr[5]);
			filetype->type = g_strdup(strarr[0]);
			filetype->autoclosingtag = atoi(strarr[6]);
			DEBUG_MSG("extensions for %s loaded from %s\n", strarr[0], strarr[1]);
			filetype->extensions = g_strsplit(strarr[1], ":", 127);
			filetype->update_chars = g_strdup(strarr[2]);
			if (strlen(strarr[3])) {
				GError *error=NULL;
				filetype->icon = gdk_pixbuf_new_from_file(strarr[3], &error);
				if (error != NULL) {
					/* Report error to user, and free error */
					g_print("ERROR while loading icon: %s\n", error->message);
					g_error_free(error);
					filetype->icon = NULL;
				}
			} else {
				filetype->icon = NULL;
			}
			filetype->highlightlist = NULL;
			main_v->filetypelist = g_list_append(main_v->filetypelist, filetype);
		}
#ifdef DEBUG
		else {
			DEBUG_MSG("filetype_list_rebuild, filetype needs 4 params in array\n");
		}
#endif
		tmplist = g_list_next(tmplist);
	}

	DEBUG_MSG("filetype_highlighting_rebuild, compile configpatterns into metapatterns\n");
	/* now compile the patterns in metapatterns, they should come from the configfile */
	tmplist = g_list_first(main_v->props.highlight_patterns);
	while (tmplist) {
		gchar **strarr;
		strarr = (gchar **) tmplist->data;
		if (count_array(strarr) == 11) {
			compile_pattern(gui_errors, strarr[0], strarr[1], atoi(strarr[2]), strarr[3], strarr[4], atoi(strarr[5]), strarr[6]
			                , strarr[7], strarr[8], atoi(strarr[9]), atoi(strarr[10]));
		}
#ifdef DEBUG
		else {
			g_print("filetype_list_rebuild, pattern %s does NOT have 11 parameters in array\n", strarr[0]);
		}
#endif
		tmplist = g_list_next(tmplist);
	}

	/* now start adding the patterns to the right filetype and the right pattern using the meta-info */

	/* first compile the parentmatch pattern */
	tmplist = g_list_first(highlight.all_highlight_patterns);
	DEBUG_MSG("filetype_highlighting_rebuild, compile parentmatch pattern\n");
	while (tmplist) {
		const char *err=NULL;
		int erroffset=0;
		Tmetapattern *mpat = (Tmetapattern *)tmplist->data;
		mpat->pcre = pcre_compile(mpat->parentmatch, 0, &err, &erroffset,NULL);
		if (err) {
			g_print("error compiling parentmatch %s at %d\n", err, erroffset);
		}
		tmplist = g_list_next(tmplist);
	}
	/* now match the top-level patterns */
	tmplist = g_list_first(main_v->filetypelist);
	DEBUG_MSG("filetype_highlighting_rebuild, match toplevel patterns\n");
	while (tmplist) {
		Tfiletype *filetype = (Tfiletype *)tmplist->data;
		add_patterns_to_parent(&filetype->highlightlist, filetype->type, "top");
		tmplist = g_list_next(tmplist);
	}
	/* now match the rest of the patterns */
	DEBUG_MSG("filetype_highlighting_rebuild, match rest of the patterns\n");
	tmplist = g_list_first(highlight.all_highlight_patterns);
	while (tmplist) {
		Tmetapattern *mpat = (Tmetapattern *)tmplist->data;
		add_patterns_to_parent(&mpat->pat->childs, mpat->filetype, mpat->name);
		tmplist = g_list_next(tmplist);
	}
	/* free the parentmatch pattern now */
	DEBUG_MSG("filetype_highlighting_rebuild, free-ing mpat->pcre\n");
	tmplist = g_list_first(highlight.all_highlight_patterns);
	while (tmplist) {
		Tmetapattern *mpat = (Tmetapattern *)tmplist->data;
		pcre_free(mpat->pcre);
		tmplist = g_list_next(tmplist);
	}


	/* now we have finished the rebuilding of the filetypes, we
	have to connect all the documents with their filetypes again, we 
	stored the name of the filetype temporary in the place of the Tfiletype,
	undo that now */
	if (alldoclist) {
		tmplist = g_list_first(alldoclist);
		while (tmplist) {
			if (((Tdocument *)tmplist->data)->hl) {
				gchar *tmpstr = (gchar *)((Tdocument *)tmplist->data)->hl;
				((Tdocument *)tmplist->data)->hl = get_filetype_by_name(tmpstr);
				DEBUG_MSG("reconnecting document %p to filetype %s\n", tmplist->data, tmpstr);
				g_free(tmpstr);
				((Tdocument *)tmplist->data)->need_highlighting = TRUE;
			}
			tmplist = g_list_next(tmplist);
		}
	}
	g_list_free(alldoclist);
}

void hl_init()
{
	/* init main_v->filetypelist, the first set is the defaultset */
	highlight.all_highlight_patterns = NULL;
	highlight.tagtable = gtk_text_tag_table_new();

	filetype_highlighting_rebuild(FALSE);
}

/**************************************/
/* end of initialisation code         */
/**************************************/

/*****************************/
/* applying the highlighting */
/*****************************/

static void patmatch_init_run(GList *level)
{
	GList *tmplist = g_list_first(level);
	while (tmplist) {
		Tpattern * pat = (Tpattern *)tmplist->data;
		memset(pat->ovector,0,sizeof(pat->ovector));
		pat->is_match = FALSE;
		patmatch_init_run(pat->childs);
		tmplist = g_list_next(tmplist);
	}
}

/**
 * patmatch_rematch:
 * @is_parentmatch: #gboolean if we search for the end pattern from (a mode 1) parent
 * @pat: #Tpattern* the pattern
 * @offset: #gint where to start the search, in bytes
 * @buf: #gchar* the buffer we work on
 * @to: #gint with the end location where to stop searching in the buffer
 * @parentpat: #Tpattern with the parent pattern, needed for a mode 3 subpattern
 */
static void patmatch_rematch(gboolean is_parentmatch, Tpattern *pat, gint offset, gchar *buf, gint to, Tpattern *parentpat)
{
#ifdef DEVELOPMENT
	if (to < offset) {
		DEBUG_MSG("patmatch_rematch: impossible, to < offset, to=%d, offset=%d\n", to, offset);
		DEBUG_MSG("is_parentmatch=%d, pat=%p, offset=%d, to=%d, parentpat=%p\n", is_parentmatch, pat, offset, to, parentpat);
		exit(23);
	}
	if (!pat) {
		g_print("no pattern, BUG, exiting!\n");
		exit(32);
	}
#endif
#ifdef DEBUG
	if ((to-offset) < 20) {
		gchar *tmp = g_strndup(&buf[offset],to-offset);
		DEBUG_MSG("patmatch_rematch, searching offset=%d,to=%d, len=%d '%s' with pat ",offset,to,to-offset,tmp);
		g_free(tmp);
	} else {
		DEBUG_MSG("patmatch_rematch, searching offset=%d,to=%d, len=%d with pat ",offset,to, to-offset);
	}
	print_meta_for_pattern(pat);
#endif
	if (is_parentmatch) {
#ifdef HL_PROFILING
		hl_profiling_tagstart(pat);
#endif

#ifdef HL_TIMING
		timing_start(TIMING_PCRE_EXEC);
#endif
		/* probably 'length' refers to the total buffer length, so if we want to stop at 'to', we need to pass 'to' as length*/
		pat->is_match = pcre_exec(pat->reg2.pcre, pat->reg2.pcre_e, buf, to, offset, 0, pat->ovector, pat->ovector_size);
#ifdef HL_TIMING
		timing_stop(TIMING_PCRE_EXEC);
#endif
#ifdef HL_PROFILING
		hl_profiling_tagstop(pat);
#endif

	} else {
		if (pat->mode == 3) {
#ifdef DEBUG
			DEBUG_MSG("patmatch_rematch, getting value from  parentpat->ovector[%d]\n", pat->numsub*2+1);
			if ((pat->numsub*2+1) >= MAX_OVECTOR || (pat->numsub*2+1) >=parentpat->ovector_size) {
				DEBUG_MSG("wanted subpattern is out of bounds!!, parentpat->ovector_size=%d\n", parentpat->ovector_size);
			} else {
				DEBUG_MSG("parentpat=%p with ovector_size=%d,requested ovector segment=%d\n",parentpat,parentpat->ovector_size,pat->numsub*2+1);
			}
#endif
#ifdef HL_PROFILING
			hl_profiling_tagstart(pat);
#endif
			pat->ovector[0] = parentpat->ovector[pat->numsub*2];
			pat->ovector[1] = parentpat->ovector[pat->numsub*2+1];
			pat->is_match = TRUE;
#ifdef HL_PROFILING
			hl_profiling_tagstop(pat);
#endif

		} else {
#ifdef HL_PROFILING
			hl_profiling_tagstart(pat);
#endif
#ifdef HL_TIMING
			timing_start(TIMING_PCRE_EXEC);
#endif
			/* probably 'length' refers to the total buffer length, so if we want to stop at 'to', we need to pass 'to' as length*/
			pat->is_match = pcre_exec(pat->reg1.pcre, pat->reg1.pcre_e, buf, to, offset, 0, pat->ovector, pat->ovector_size);
			if (pat->is_match == -1) {
				pat->ovector[0] = to;
				pat->ovector[1] = to;
			}
#ifdef HL_TIMING
			timing_stop(TIMING_PCRE_EXEC);
#endif
#ifdef HL_PROFILING
			hl_profiling_tagstop(pat);
#endif

		}
	}
#ifdef DEVELOPMENT
	if (pat->ovector[1] > to) {
		g_print("BUG: patmatch_rematch,end > to, setting ovector[1] to to (=%d)\n", to);
		/*pat->ovector[1] = to;*/
		exit(567);
	}
#endif
}

static void applystyle(Tdocument *doc, gchar *buf, guint buf_char_offset, gint so, gint eo, Tpattern *pat)
{
	GtkTextIter itstart, itend;
	gint istart, iend;
	guint char_start, char_end, byte_char_diff_start;

#ifdef DEBUG
	{
		gchar *tmp;
		DEBUG_MSG("applystyle, coloring from so=%d to eo=%d", so, eo);
		tmp = g_strndup(&buf[so],eo-so);
		DEBUG_MSG("'%s'\n", tmp);
		g_free(tmp);
	}
#endif
#ifdef HL_PROFILING
	hl_profiling_tagapply(pat);
#endif

#ifdef HL_TIMING
	timing_start(TIMING_UTF8);
#endif
	char_start = utf8_byteoffset_to_charsoffset_cached(buf, so);
	char_end = utf8_byteoffset_to_charsoffset_cached(buf, eo);
	byte_char_diff_start = so-char_start;
#ifdef HL_TIMING
	timing_stop(TIMING_UTF8);
#endif
	istart = char_start+buf_char_offset;
	iend = char_end+buf_char_offset;
#ifdef HL_DEBUG1
	DEBUG_MSG("applystyle, byte_char_diff=%d\n", byte_char_diff_start);
	DEBUG_MSG("applystyle, coloring from %d to %d\n", istart, iend);
#endif
#ifdef HL_TIMING
	timing_start(TIMING_TEXTBUF);
#endif

#ifdef HL_TIMING
	timing_start(TIMING_TEXTBUF_ITER);
#endif
	gtk_text_buffer_get_iter_at_offset(doc->buffer, &itstart, istart);
	gtk_text_buffer_get_iter_at_offset(doc->buffer, &itend, iend);
#ifdef HL_TIMING
	timing_stop(TIMING_TEXTBUF_ITER);
#endif
#ifdef HL_TIMING
	timing_start(TIMING_TEXTBUF_TAG);
#endif
	gtk_text_buffer_apply_tag(doc->buffer, pat->tag, &itstart, &itend);
#ifdef HL_TIMING
	timing_stop(TIMING_TEXTBUF_TAG);
#endif
#ifdef HL_TIMING
	timing_stop(TIMING_TEXTBUF);
#endif
}

/* applylevel(Tdocument * doc, gchar * buf, guint buf_char_offset, gint offset, gint length, Tpatmatch *parentmatch, GList *childs_list)
 * doc: the document
 * buf: the buffer with all characters to apply the highlighting to
 * buf_char_offset: the character offset of the first char of the buffer 
 * offset: the byte offset where we start in the buffer
 * to: the byte offset where we stop the search for this level
 * parentmatch: if there is a parent with mode 1 we have to search for the end together with it's children
 * childs_list: a list of Tpattern that needs to be applied in this region
 *
 * if childs_list is NULL, it MUST BE a parent with mode 1 match!
 * this not checked in the code for performance reasons!
 */

static void applylevel(Tdocument * doc, gchar * buf, guint buf_char_offset, gint offset, const gint to, Tpattern *parentpat, GList *childs_list)
{
	gint parent_mode1_start=offset;
	gint parent_mode1_offset=offset;
#ifdef DEVELOPMENT
	if (offset >= to ) {
		DEBUG_MSG("applylevel, impossible start, offset >= to, offset=%d, to=%d\n", offset, to);
		exit(24);
	}

	if (!childs_list && (!parentpat || parentpat->mode != 1)) {
		g_print("applylevel: BUG, no child list, but also no mode 1 parent pattern!");
		exit(31);
	}
#endif
	if (parentpat && parentpat->mode == 1) {
		/* before any patmatch_rematch, this has the end of the start pattern */
		parent_mode1_offset = parentpat->ovector[1];
	}
	DEBUG_MSG("applylevel, started with offset=%d, to=%d, len=%d\n", offset, to, to - offset);
	if (!childs_list) {
		/* we assume the parent is mode 1, IF NOT we are called with INVALID ARGUMENTS */
		patmatch_rematch(TRUE, parentpat, offset > parent_mode1_offset ? offset : parent_mode1_offset, buf, to, parentpat);
		if (parentpat->is_match<1) {
			DEBUG_MSG("no childs list, mode 1 parent has no match, set matching to 'to'=%d\n", to);
			parentpat->ovector[1] = to;
			parentpat->is_match = 1;
		}
		applystyle(doc, buf,buf_char_offset, offset, parentpat->ovector[1], parentpat);
		DEBUG_MSG("no childs list, mode 1 parent is applied, returning\n");
		return;
	} else {
		GList *tmplist;
		Tpattern *lowest_pm = NULL;
		tmplist = g_list_first(childs_list);
		while (tmplist) {
			/* check if we need to match the pattern */
			if (PATTERN(tmplist->data)->ovector[0] <= offset) {
				DEBUG_MSG("1, rematching pattern %p, ovector[0]=%d, offset=%d\n",PATTERN(tmplist->data),PATTERN(tmplist->data)->ovector[0], offset);
				patmatch_rematch(FALSE, PATTERN(tmplist->data), offset, buf, to, parentpat);
#ifdef DEVELOPMENT
				if (PATTERN(tmplist->data)->ovector[1] > to) {
					g_print("BUG: PATTERN(tmplist->data)->ovector[1] > to\n");
					exit(321);
				}
#endif

			}
#ifdef DEBUG
			else {
				DEBUG_MSG("1 not rematching pattern %p, ovector[0]=%d, offset=%d, pattern ",PATTERN(tmplist->data), PATTERN(tmplist->data)->ovector[0], offset);
				print_meta_for_pattern(PATTERN(tmplist->data));
			}
#endif
			if ((PATTERN(tmplist->data)->is_match > 0) && (PATTERN(tmplist->data)->ovector[1]<=to) &&(lowest_pm == NULL || (lowest_pm->ovector[0] > PATTERN(tmplist->data)->ovector[0]))) {
				lowest_pm = PATTERN(tmplist->data);
#ifdef DEBUG
				DEBUG_MSG("1 new lowest_pm=%p, pat->is_match=%d, start=%d, pattern=",lowest_pm,PATTERN(tmplist->data)->is_match, PATTERN(tmplist->data)->ovector[0]);
				print_meta_for_pattern(lowest_pm);
#endif

			}
#ifdef DEBUG
			else {
				DEBUG_MSG("1 pat %p is NOT the lowest pm with is_match=%d and ovector[0]=%d\n", PATTERN(tmplist->data), PATTERN(tmplist->data)->is_match, PATTERN(tmplist->data)->ovector[0]);
			}
#endif
			tmplist = g_list_next(tmplist);
		}
		if (parentpat && parentpat->mode == 1) {
			/* the end of the parent pattern needs matching too */
			DEBUG_MSG("matching the parent-end with offset %d\n", offset > parent_mode1_start ? offset : parent_mode1_start);
			patmatch_rematch(TRUE, parentpat, offset > parent_mode1_offset ? offset : parent_mode1_offset, buf, to, parentpat);
			if (parentpat->is_match <=0) {
				DEBUG_MSG("mode 1 parent has no match, matching to=%d\n", to);
				parentpat->ovector[1] = to;
				parentpat->ovector[0] = to;
				parentpat->is_match = 1;
			}
#ifdef DEVELOPMENT
			if (parentpat->ovector[1] > to) {
				g_print("BUG: parentpat->ovector[1] > to\n");
				exit(765);
			}
#endif
			if ((lowest_pm == NULL || (lowest_pm->ovector[0] > parentpat->ovector[0]))) {
				lowest_pm = parentpat;
				DEBUG_MSG("3 lowest_pm=%p (parentpat!!), parentpat->is_match=%d, start=%d\n",lowest_pm,parentpat->is_match, parentpat->ovector[0]);
			}
#ifdef DEBUG
			else {
				DEBUG_MSG("3 (parent!!) is NOT the lowest pm with is_match=%d and ovector[0]=%d\n", parentpat->is_match, parentpat->ovector[0]);
			}
#endif

		}
#ifdef DEVELOPMENT
		if (offset > to ) {
			g_print("BUG: applylevel, impossible before applying, offset > to, offset=%d, to=%d\n", offset, to);
			g_print("lowest_pm->mode=%d, lowest_pm->childs=%p\n",lowest_pm->mode,lowest_pm->childs);
			exit(24);
		}
#endif

		/* apply the lowest match */
		while (lowest_pm != NULL) {
			if (lowest_pm == parentpat) {
#ifdef DEBUG
				DEBUG_MSG("*apply: offset=%d, lowest_pm=%p with ovector=%d, ovector[1]=%d\n", offset, lowest_pm, parentpat->ovector[0],parentpat->ovector[1]);
#endif
				/* apply the parent, and return from this level */
				applystyle(doc, buf,buf_char_offset, parent_mode1_start, parentpat->ovector[1], parentpat);
				lowest_pm = NULL; /* makes us return */
			} else {
#ifdef DEBUG
				DEBUG_MSG("*apply:a child is the lowest match, offset=%d, lowest_pm=%p with start=%d and ovector[1]=%d ", offset, lowest_pm, lowest_pm->ovector[0],lowest_pm->ovector[1]);
				print_meta_for_pattern(lowest_pm);
#endif
				switch (lowest_pm->mode) {
				case 1:
					/* if mode==1 the style is applied within the applylevel for the children because the end is not yet
					known, the end is set in ovector[1] after applylevel for the children is finished */
					applylevel(doc, buf,buf_char_offset, lowest_pm->ovector[0], to, lowest_pm, lowest_pm->childs);
					offset = lowest_pm->ovector[1];
					break;
				case 2:
					/* ovector[0] and ovector[1] are both offsets in the buffer matched by the entire pattern */
					applystyle(doc, buf,buf_char_offset, lowest_pm->ovector[0], lowest_pm->ovector[1], lowest_pm);
					if (lowest_pm->childs) applylevel(doc, buf,buf_char_offset, lowest_pm->ovector[0], lowest_pm->ovector[1], lowest_pm, lowest_pm->childs);
					offset = lowest_pm->ovector[1];
					break;
				case 3:
					applystyle(doc, buf,buf_char_offset, lowest_pm->ovector[0], lowest_pm->ovector[1], lowest_pm);
					if (lowest_pm->childs) applylevel(doc, buf,buf_char_offset, lowest_pm->ovector[0], lowest_pm->ovector[1], lowest_pm, lowest_pm->childs);
					offset = lowest_pm->ovector[1];
					break;
#ifdef DEBUG
				default:
					/* unknown mode, cannot pass the pattern-compile-stage??? */
					g_print("applylevel, unknown mode, cannot pass the pattern-compile-stage???\n");
					exit(2);
					break;
#endif

				}
				if (offset == to) {
					DEBUG_MSG("offset==to, there is no further pattern that will match inb this level\n");
					DEBUG_MSG("applylevel, finished, to=%d, returning to previous level\n", to);
					return;
				}

				/* init for the next round, rematch the patterns that have a startpoint < offset */
				lowest_pm = NULL;

				tmplist = g_list_first(childs_list);
				while (tmplist) {
					if (((PATTERN(tmplist->data)->ovector[1] <=to) && (PATTERN(tmplist->data)->ovector[0] <= offset))
					        || PATTERN(tmplist->data)->ovector[1] > to) {
						if (PATTERN(tmplist->data)->mode == 3) {
							PATTERN(tmplist->data)->is_match = FALSE; /* mode 3 types can only match as first match */
						} else {
							patmatch_rematch(FALSE, PATTERN(tmplist->data), offset, buf, to, parentpat);
							DEBUG_MSG("2 rematch: pat=%p, pat.is_match=%d, start=%d\n",PATTERN(tmplist->data),PATTERN(tmplist->data)->is_match,PATTERN(tmplist->data)->ovector[0]);
						}
					} else if (PATTERN(tmplist->data)->ovector[1] > to) {
						PATTERN(tmplist->data)->is_match = 0;
					}
					if ((PATTERN(tmplist->data)->is_match>0) && (lowest_pm == NULL || (lowest_pm->ovector[0] > PATTERN(tmplist->data)->ovector[0]))) {
						lowest_pm = PATTERN(tmplist->data);
						DEBUG_MSG("2 new lowest_pm=%p, start=%d\n",lowest_pm,lowest_pm->ovector[0]);
					}
#ifdef DEBUG
					else {
						DEBUG_MSG("2 pat %p is NOT the lowest pm with is_match=%d and ovector[0]=%d\n", PATTERN(tmplist->data), PATTERN(tmplist->data)->is_match, PATTERN(tmplist->data)->ovector[0]);
					}
#endif
					tmplist = g_list_next(tmplist);
				}

				if (parentpat && parentpat->mode == 1) {
					/* the end of the parent pattern needs matching too */
					if (parentpat->ovector[0] < offset) {
						patmatch_rematch(TRUE, parentpat, offset, buf, to, parentpat);
					}
					if (parentpat->is_match<=0) {
						DEBUG_MSG("4 mode 1 parent has no match, matching to 'to'=%d\n", to);
						parentpat->ovector[1] = to;
						parentpat->ovector[0] = to;
						parentpat->is_match = 1;
					}
					if (lowest_pm == NULL || (lowest_pm->ovector[0] > parentpat->ovector[0])) {
						lowest_pm = parentpat;
						DEBUG_MSG("4 new lowest_pm=%p (parentmatch!!), start=%d\n",lowest_pm,lowest_pm->ovector[0]);
					}
#ifdef DEBUG
					else {
						DEBUG_MSG("4 (parent!!) is NOT the lowest pm with is_match=%d and ovector[0]=%d\n", parentpat->is_match, parentpat->ovector[0]);
					}
#endif

				}
#ifdef DEVELOPMENT
				if (lowest_pm && lowest_pm->ovector[1] > to) {
					g_print("BUG: lowest_pm->ovector[1] > to !!\n");
					g_print("is_parentpat=%d, lowest_pm->mode=%d\n",(lowest_pm == parentpat), lowest_pm->mode);
					exit(345);
				}
#endif
#ifdef DEBUG
				if (lowest_pm) {
					if (lowest_pm == parentpat) {
						DEBUG_MSG("lowest_match %p (parentpat) has start %d\n", lowest_pm, parentpat->ovector[0]);
					} else {
						DEBUG_MSG("lowest_match %p (some child) has start %d\n", lowest_pm, lowest_pm->ovector[0]);
					}
				}
#endif

			}
		}
	}
	DEBUG_MSG("applylevel, finished, to=%d, returning to previous level\n", to);
}

void doc_remove_highlighting(Tdocument * doc)
{
	GtkTextIter itstart, itend;
	gtk_text_buffer_get_bounds(doc->buffer, &itstart, &itend);
	gtk_text_buffer_remove_all_tags(doc->buffer, &itstart, &itend);
}

void doc_highlight_full(Tdocument * doc)
{
	if (!doc->hl || !doc->hl->highlightlist) {
		return;
	} else {
		gchar *buf;
		guint charcount;
		/*		GdkCursor *cursor = gdk_cursor_new(GDK_WATCH);
				gdk_window_set_cursor(main_v->main_window->window, cursor);
				gdk_window_set_cursor(main_v->current_document->view->window, cursor);
				flush_queue();*/
		charcount = gtk_text_buffer_get_char_count(doc->buffer);
		doc_remove_highlighting(doc);
#ifdef HL_TIMING
		timing_init();
#endif
#ifdef HL_PROFILING
		hl_profiling_reset(doc);
#endif
#ifdef HL_TIMING
		timing_start(TIMING_TOTAL);
#endif
		utf8_offset_cache_reset();
		buf = doc_get_chars(doc, 0, charcount);
#ifdef DEVELOPMENT
		g_assert(doc);
		g_assert(doc->hl);
		g_assert(buf);
		g_assert(doc->hl->highlightlist);
#endif
		patmatch_init_run(doc->hl->highlightlist);
		applylevel(doc,buf,0,0,strlen(buf),NULL,doc->hl->highlightlist);
		g_free(buf);
#ifdef HL_TIMING
		timing_stop(TIMING_TOTAL);
		g_print("doc_highlight_full done, %ld ms total, %ld ms tagging (%dX), %ld ms matching (%dX)\n",timing[TIMING_TOTAL].total_ms, timing[TIMING_TEXTBUF].total_ms, timing[TIMING_TEXTBUF].numtimes, timing[TIMING_PCRE_EXEC].total_ms, timing[TIMING_PCRE_EXEC].numtimes);
		g_print("%ld ms utf8-shit (%dX), %ld ms utf8-invalidate (%dX)\n", timing[TIMING_UTF8].total_ms, timing[TIMING_UTF8].numtimes, timing[TIMING_UTF8_INV].total_ms, timing[TIMING_UTF8_INV].numtimes);
		g_print("%ld ms setting iters, %ld ms setting tags\n", timing[TIMING_TEXTBUF_ITER].total_ms, timing[TIMING_TEXTBUF_TAG].total_ms);
#endif
#ifdef HL_PROFILING
		hl_profiling_print(doc);
#endif
		doc->need_highlighting = FALSE;
		/*		gdk_window_set_cursor(main_v->main_window->window, NULL);
				gdk_cursor_unref(cursor);*/
	}
}

static void remove_tag_by_list_in_region(Tdocument * doc, GList * patlist, GtkTextIter * itstart, GtkTextIter * itend)
{
	GList *tmplist = g_list_first(patlist);
	DEBUG_MSG("remove_tag_by_list_in_region, started on list %p\n", patlist);
	/* remove all tags that are children of patlist */
	while (tmplist) {
		DEBUG_MSG("remove_tag_by_list_in_region, removing tags for pattern %p\n", tmplist->data);
		gtk_text_buffer_remove_tag(doc->buffer, ((Tpattern *) tmplist->data)->tag, itstart, itend);
		if (((Tpattern *) tmplist->data)->childs) {
			remove_tag_by_list_in_region(doc, ((Tpattern *) tmplist->data)->childs, itstart, itend);
		}
		tmplist = g_list_next(tmplist);
	}
}

static Tpattern *find_pattern_by_tag(GList * parentlist, GtkTextTag * tag)
{
	GList *tmplist;

	tmplist = g_list_first(parentlist);
	while (tmplist) {
		if (((Tpattern *) tmplist->data)->tag == tag) {
			return (Tpattern *) tmplist->data;
		}
		tmplist = g_list_next(tmplist);
	}
	return NULL;
}

static gboolean pattern_has_mode3_child(Tpattern *pat)
{
	GList *tmplist;

	tmplist = g_list_first(pat->childs);
	while (tmplist) {
		if (((Tpattern *)tmplist->data)->mode == 3) return TRUE;
		tmplist = g_list_next(tmplist);
	}
	return FALSE;
}

void doc_highlight_region(Tdocument * doc, guint startof, guint endof)
{
	Tpattern *pat = NULL;
	guint so=-1, eo=-1;
	GList *patternlist = doc->hl->highlightlist;
#ifdef HL_TIMING
	timing_init();
	timing_start(TIMING_LINE_HIGHLIGHTING);
#endif
	if (startof < endof) {
		GtkTextIter itstart, itend, itsearch;
		GSList *taglist = NULL, *slist;
		gtk_text_buffer_get_iter_at_offset(doc->buffer,&itstart,startof);
		gtk_text_buffer_get_iter_at_offset(doc->buffer,&itend,endof);

		/* get all the tags that itstart is in */
		taglist = gtk_text_iter_get_tags(&itstart);
		DEBUG_MSG("doc_highlight_line, (1) getting all tags at itstart %d\n",gtk_text_iter_get_offset(&itstart));
		/* find for every tag if it ends _after_ itend or not */
		itsearch = itstart;
		slist = taglist;
		while (slist && slist->data) {
			gboolean tag_found;
#ifdef DEBUG
			{
				Tpattern *testpat;
				gchar *test;
				testpat = find_pattern_by_tag(patternlist, GTK_TEXT_TAG(slist->data));
				if (testpat)
				{
					DEBUG_MSG("doc_highlight_line, (1) tag %p goes with ", testpat->tag);
					print_meta_for_pattern(testpat);
				} else
				{
					DEBUG_MSG("doc_highlight_line, (1) tag %p doesn't have pattern??\n",slist->data);
					print_meta_for_tag(GTK_TEXT_TAG(slist->data));
					/*exit(9);*/
				}
				test = gtk_text_buffer_get_text(doc->buffer, &itstart, &itend,FALSE);
				DEBUG_MSG("doc_highlight_line, (1) current string='%s'\n", test);
				g_free(test);
			}
#endif
			/* if the tags ends at itstart there is no need to search forward to the end */
			if (!gtk_text_iter_ends_tag(&itstart, GTK_TEXT_TAG(slist->data))) {
				DEBUG_MSG("doc_highlight_line, (1) forward looking for tag %p (%s) from so=%d to eo=%d\n", slist->data, get_metaname_from_tag(slist->data),
				          gtk_text_iter_get_offset(&itstart), gtk_text_iter_get_offset(&itend));
				tag_found = gtk_text_iter_forward_to_tag_toggle(&itsearch, GTK_TEXT_TAG(slist->data));
				if (!tag_found) {
					/* this happens with several gtk versions,
						up to 2.2.4 is buggy
						inbetween is unknown,
						2.4.4 is fixed */
					DEBUG_MSG("doc_highlight_line (1), very weird situation, the tag %p is started but it doesn't end ??, itsearch now is at %d\n", slist->data, gtk_text_iter_get_offset(&itsearch));
					/* itsearch is now at the end of the buffer so we do a backward search to find the first start */
					tag_found = gtk_text_iter_backward_to_tag_toggle(&itsearch, GTK_TEXT_TAG(slist->data));
					DEBUG_MSG("(1) a backward search to the tag finds %d\n", gtk_text_iter_get_offset(&itsearch));
					if (!tag_found) {
						g_print("doc_highlight_line, (1) a tag that is started is nowhere to be found ??? BUG!\n");
						exit(123);
					}
					if (gtk_text_iter_compare(&itsearch, &itend) < 0) {
						/* there is no tag toggle found between itend and the end of the buffer, we
						 probably have hit the gtk bug here, so we just set itsearch here to itend+1 */
						itsearch = itend;
						gtk_text_iter_forward_char(&itsearch);
					}
				}
				DEBUG_MSG("doc_highlight_line, (1) tag %p (%s) ends at itsearch=%d\n", slist->data, get_metaname_from_tag(slist->data),gtk_text_iter_get_offset(&itsearch));
				if (gtk_text_iter_compare(&itsearch, &itend) > 0) {
					/* both the start and endpoint are within this
					   tag --> pattern matching can start with this
					   subpattern */
					pat = find_pattern_by_tag(patternlist, GTK_TEXT_TAG(slist->data));
#ifdef DEBUG
					DEBUG_MSG("found pattern %p with tag %p and childs %p", pat, pat->tag,pat->childs);
					print_meta_for_pattern(pat);
#endif
					if (pat && (pat->mode == 1)) {
						gchar *string;
						int ovector[MAX_OVECTOR];
						/* but first we do a quick test if the parent-pattern is indeed still valid */
						string = gtk_text_buffer_get_text(doc->buffer, &itstart, &itend, FALSE);
						if (0 < pcre_exec(pat->reg2.pcre, pat->reg2.pcre_e, string, strlen(string), 0, 0, ovector, MAX_OVECTOR)) {
							/* the current line does not have the start of the tag or the end of the tag, but now
							   it does have a match on the end pattern --> so the pattern should be invalidated */
							pat = NULL;
							DEBUG_MSG("doc_highlight_line, (1) a match of the endpattern is found on this line '%s', the pattern is invalidated\n",string);
							itend = itsearch;
							gtk_text_iter_backward_to_tag_toggle(&itstart, slist->data);
						}
						g_free(string);
					}
					if (pat && !pattern_has_mode3_child(pat)) {
						/* what happens if patternlist = NULL ?
						   that means we are inside a match without any subpatterns, but 
						   perhaps the subpattern should be invalidated... hmm..
						 */
						patternlist = pat->childs;
#ifdef DEBUG
						DEBUG_MSG("doc_highlight_line, (1) going to use patternlist %p from pattern ", patternlist);
						print_meta_for_pattern(pat);
#endif

					} else {
						DEBUG_MSG("doc_highlight_line, (1) no pat or pat has a mode3 child, continue with next tag\n");
					}
				} else {
					/* this tag stops somewhere in the middle of the line, move
					   itstart to the beginning of this tag, 
						there is also no need anymore to look further in slist, we have to start with this patternlist */
					if (gtk_text_iter_begins_tag(&itstart, GTK_TEXT_TAG(slist->data))) {
						DEBUG_MSG("doc_highlight_line, (1) itstart at %d is already at the beginning of tag %p (%s)\n",gtk_text_iter_get_offset(&itstart), slist->data, get_metaname_from_tag(slist->data));
					} else {
						DEBUG_MSG("doc_highlight_line, (1) move itstart from %d to beginning of tag %p (%s)\n",
						          gtk_text_iter_get_offset(&itstart), slist->data, get_metaname_from_tag(slist->data));
						gtk_text_iter_backward_to_tag_toggle(&itstart, GTK_TEXT_TAG(slist->data));
						DEBUG_MSG("doc_highlight_line, (1) itstart is set back to %d\n", gtk_text_iter_get_offset(&itstart));
					}
					if (pat) {
						DEBUG_MSG("doc_highlight_line, (1) skip all other tags, slist=g_list_last()\n");
						slist = g_slist_last(slist);
					}
				}
			}
			itsearch = itstart;
			slist = g_slist_next(slist);
		}
		g_slist_free(taglist);

		/* get all the tags that itend is in */
		taglist = gtk_text_iter_get_tags(&itend);
		DEBUG_MSG("doc_highlight_line, (2) getting all tags for itend at %d\n", gtk_text_iter_get_offset(&itend));
		/* find for every tag if it starts _before_ itstart (that means all of this line-highlighting
		 * is within that tag) or _after_ itstart (which means that we should remove that tag, move
		 * our highlighting endpoint to the end of that tag, and rehighlight the whole bit) */
		itsearch = itend;
		slist = taglist;
		while (slist && slist->data) {
			gboolean tag_found;
			/* if the tags starts at itend there is no need to search backward to the start */
			if (!gtk_text_iter_begins_tag(&itend, GTK_TEXT_TAG(slist->data))) {
				DEBUG_MSG("doc_highlight_line, (2) backwards looking for tag %p (%s) from eo=%d to so=%d, itsearch=%d\n", slist->data,
				          get_metaname_from_tag(slist->data), gtk_text_iter_get_offset(&itend),gtk_text_iter_get_offset(&itstart) ,gtk_text_iter_get_offset(&itsearch));
#ifdef DEBUG
				DEBUG_MSG("does the itsearch position (%d) toggle(%d), begin(%d) or end(%d) the tag %p (%s)?\n"
				          ,gtk_text_iter_get_offset(&itsearch)
				          ,gtk_text_iter_toggles_tag(&itsearch,GTK_TEXT_TAG(slist->data)),gtk_text_iter_begins_tag(&itsearch,GTK_TEXT_TAG(slist->data))
				          ,gtk_text_iter_ends_tag(&itsearch,GTK_TEXT_TAG(slist->data)),slist->data, get_metaname_from_tag(slist->data));
#endif
				/* this next function does crash sometimes, and I wonder why..... */
				tag_found = gtk_text_iter_backward_to_tag_toggle(&itsearch, GTK_TEXT_TAG(slist->data));
				if (!tag_found) {
					/* this happens with several gtk versions,
						up to 2.2.4 is buggy
						inbetween is unknown,
						2.4.4 is fixed */
					if (gtk_text_iter_begins_tag(&itsearch, GTK_TEXT_TAG(slist->data))) {
						/* we have hit the bug in gtk, this tag should have been found by gtk_text_iter_backward_to_tag_toggle */
					} else {
						DEBUG_MSG("doc_highlight_line (2), very weird situation, the tag is ended but it doesn't start ??, itsearch now is at %d\n", gtk_text_iter_get_offset(&itsearch));
						/* itsearch is now at offset 0, so we do a forward search to find the first start */
						tag_found = gtk_text_iter_forward_to_tag_toggle(&itsearch, GTK_TEXT_TAG(slist->data));
						DEBUG_MSG("(2) a forward search to the tag finds %d\n", gtk_text_iter_get_offset(&itsearch));
						if (!tag_found) {
							g_print("doc_highlight_line, (2) a tag that is started is nowhere to be found ??? BUG!\n");
							exit(123);
						}
						if (gtk_text_iter_compare(&itsearch, &itend) > 0) {
							itsearch = itstart;
							gtk_text_iter_backward_char(&itsearch);
						}
					}
				}
				DEBUG_MSG("doc_highlight_line, (2) tag %p (%s) starts at itsearch=%d\n", slist->data,get_metaname_from_tag(slist->data),gtk_text_iter_get_offset(&itsearch));
				if (gtk_text_iter_compare(&itsearch, &itstart) <= 0) {
					/* both the start and endpoint are within this
					   tag --> pattern matching can start with this
					   subpattern,	since we did run the same algorithm for itstart we can skip a bit now */
				} else {
					/* this tag starts somewhere in the middle of the line, move
					   itend to the end of this tag */
					if (gtk_text_iter_ends_tag(&itend, GTK_TEXT_TAG(slist->data))) {
						DEBUG_MSG("doc_highlight_line, (2) itend at %d is already at the end of tag %p (%s)\n",
						          gtk_text_iter_get_offset(&itend), slist->data,get_metaname_from_tag(slist->data));
					} else {
						DEBUG_MSG("doc_highlight_line, (2) move itend from %d to end of tag %p (%s)\n",gtk_text_iter_get_offset(&itend), slist->data, get_metaname_from_tag(slist->data));
						gtk_text_iter_forward_to_tag_toggle(&itend, GTK_TEXT_TAG(slist->data));
						DEBUG_MSG("doc_highlight_line, (2) itend is set forward to %d\n", gtk_text_iter_get_offset(&itend));
					}
				}
			}
			itsearch = itend;
			slist = g_slist_next(slist);
		}
		g_slist_free(taglist);

		/* this function removes some specific tags from the region */
		remove_tag_by_list_in_region(doc, patternlist, &itstart, &itend);
		so = gtk_text_iter_get_offset(&itstart);
		eo = gtk_text_iter_get_offset(&itend);
	} else {
		DEBUG_MSG("doc_highlight_line, so >= eo, not highlighting!\n");
	}
#ifdef HL_TIMING
	timing_stop(TIMING_LINE_HIGHLIGHTING);
#endif
	if (patternlist) {
		gchar *buf;
		DEBUG_MSG("doc_highlight_line from so=%d to eo=%d\n", so, eo);

		patmatch_init_run(doc->hl->highlightlist);

#ifdef HL_TIMING
		timing_start(TIMING_TOTAL);
#endif
		buf = doc_get_chars(doc, so, eo);
		applylevel(doc,buf,so,0,strlen(buf),NULL,patternlist);
		g_free(buf);
#ifdef HL_TIMING
		timing_stop(TIMING_TOTAL);
		g_print("doc_highlight_line done, %ld ms total, %ld ms line_highlighting\n",timing[TIMING_TOTAL].total_ms, timing[TIMING_LINE_HIGHLIGHTING].total_ms);
#endif

	} else {
		DEBUG_MSG("doc_highlight_line, no patternlist, not highlighting!\n");
	}
	doc->need_highlighting = FALSE;
}

void doc_highlight_line(Tdocument * doc)
{
	GtkTextIter itstart, itend;
	GtkTextMark *mark = gtk_text_buffer_get_insert(doc->buffer);
	gtk_text_buffer_get_iter_at_mark(doc->buffer, &itstart, mark);
	/* move to the beginning of the line */
	gtk_text_iter_set_line_offset(&itstart, 0);
	if (main_v->props.highlight_num_lines_count) {
		gtk_text_iter_backward_lines(&itstart, main_v->props.highlight_num_lines_count);
	}
	gtk_text_buffer_get_iter_at_mark(doc->buffer, &itend, mark);
	/*		gtk_text_iter_forward_to_line_end(&itend);
		gtk_text_iter_set_line_offset(&itend, 0);*/
	if (main_v->props.highlight_num_lines_count) {
		gtk_text_iter_forward_lines(&itend, main_v->props.highlight_num_lines_count);
	}
	if (gtk_text_iter_forward_to_line_end(&itend)) {
		gtk_text_iter_forward_char(&itend);
	}
	doc_highlight_region(doc, gtk_text_iter_get_offset(&itstart), gtk_text_iter_get_offset(&itend));
}

GtkTextTagTable *highlight_return_tagtable()
{
	return highlight.tagtable;
}
