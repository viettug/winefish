/* $Id: char_table.c,v 1.1.1.1 2005/06/29 11:03:17 kyanh Exp $ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * char_table.h - character convertion prototypes
 *
 * Complete rewrite for UTF8 Copyright (C) 2002 Olivier Sessink
 * some ideas from original version Copyright (C) 2000 Pablo De Napoli
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
/* #define DEBUG */

#include <string.h>
#include <gtk/gtk.h>

#include "char_table.h"
#include "bluefish.h"

typedef struct {
	gunichar id;
	char *entity;
} Tchar_entity;

Tchar_entity ascii_charset[] = {
	{34, "&quot;"}
	, {38, "&amp;"}
	, {60, "&lt;"}
	, {62, "&gt;"}
	, {0, NULL}
};

/* Do not modify this table
 * for convert_unichar_to_htmlstring to work properly 
 */

Tchar_entity iso8859_1_charset[] = {
	{160, "&nbsp;"}
	, {161, "&iexcl;"}
	, {162, "&cent;"}
	, {163, "&pound;"}
	,
	{164, "&curren;"}
	, {165, "&yen;"}
	, {166, "&brvbar;"}
	, {167, "&sect;"}
	,
	{168, "&uml;"}
	, {169, "&copy;"}
	, {170, "&ordf;"}
	, {171, "&laquo;"}
	,
	{172, "&not;"}
	, {173, "&shy;"}
	, {174, "&reg;"}
	, {175, "&macr;"}
	,
	{176, "&deg;"}
	, {177, "&plusmn;"}
	, {178, "&sup2;"}
	, {179, "&sup3;"}
	,
	{180, "&acute;"}
	, {181, "&micro;"}
	, {182, "&para;"}
	, {183, "&middot;"}
	,
	{184, "&cedil;"}
	, {185, "&sup1;"}
	, {186, "&ordm;"}
	, {187, "&raquo;"}
	,
	{188, "&frac14;"}
	, {189, "&frac12;"}
	, {190, "&frac34;"}
	, {191, "&iquest;"}
	,
	{192, "&Agrave;"}
	, {193, "&Aacute;"}
	, {194, "&Acirc;"}
	, {195, "&Atilde;"}
	,
	{196, "&Auml;"}
	, {197, "&Aring;"}
	, {198, "&AElig;"}
	, {199, "&Ccedil;"}
	,
	{200, "&Egrave;"}
	, {201, "&Eacute;"}
	, {202, "&Ecirc;"}
	, {203,
	   "&Euml;"}
	,
	{204, "&Igrave;"}
	, {205, "&Iacute;"}
	, {206, "&Icirc;"}
	, {207,
	   "&Iuml;"}
	,
	{208, "&ETH;"}
	, {209, "&Ntilde;"}
	, {210, "&Ograve;"}
	, {211,
	   "&Oacute;"}
	,
	{212, "&Ocirc;"}
	, {213, "&Otilde;"}
	, {214, "&Ouml;"}
	, {215,
	   "&times;"}
	,
	{216, "&Oslash;"}
	, {217, "&Ugrave;"}
	, {218, "&Uacute;"}
	, {219,
	   "&Ucirc;"}
	,
	{220, "&Uuml;"}
	, {221, "&Yacute;"}
	, {222, "&THORN;"}
	, {223,
	   "&szlig;"}
	,
	{224, "&agrave;"}
	, {225, "&aacute;"}
	, {226, "&acirc;"}
	, {227,
	   "&atilde;"}
	,
	{228, "&auml;"}
	, {229, "&aring;"}
	, {230, "&aelig;"}
	, {231,
	   "&ccedil;"}
	,
	{232, "&egrave;"}
	, {233, "&eacute;"}
	, {234, "&ecirc;"}
	, {235,
	   "&euml;"}
	,
	{236, "&igrave;"}
	, {237, "&iacute;"}
	, {238, "&icirc;"}
	, {239,
	   "&iuml;"}
	,
	{240, "&eth;"}
	, {241, "&ntilde;"}
	, {242, "&ograve;"}
	, {243,
	   "&oacute;"}
	,
	{244, "&ocirc;"}
	, {245, "&otilde;"}
	, {246, "&ouml;"}
	, {247,
	   "&divide;"}
	,
	{248, "&oslash;"}
	, {249, "&ugrave;"}
	, {250, "&uacute;"}
	, {251,
	   "&ucirc;"}
	,
	{252, "&uuml;"}
	, {253, "&yacute;"}
	, {254, "&thorn;"}
	, {255,
	   "&yuml;"}
	, {0, NULL}
};

static void convert_unichar_to_htmlstring(gunichar unichar, gchar *deststring, gboolean ascii, gboolean iso) {
	if (ascii) {
		gint j=0;
		while (ascii_charset[j].id != 0) {
			if (ascii_charset[j].id == unichar) {
				deststring[0]='\0';
				strncat(deststring, ascii_charset[j].entity, 8);
				return;
			}
			j++;
		}
	}
	if (iso) {
		if (unichar >= 160 && unichar < 256) {
			deststring[0]='\0';
			strncat(deststring, iso8859_1_charset[unichar - 160].entity, 8);
			return;
		}
	}
	{
		gint len= g_unichar_to_utf8(unichar, deststring);
		deststring[len] = '\0';
	}
}

/* utf8string MUST BE VALIDATED UTF8 otherwise this function is broken!!
so text from the TextBuffer is OK to use */
gchar *convert_string_utf8_to_html(const gchar *utf8string, gboolean ascii, gboolean iso) {
	if (!utf8string || utf8string[0] == '\0' || (!ascii && !iso)) {
		return g_strdup(utf8string);
	} else {
		/* optimize for speed, not for memory usage because that is very temporary */
		gchar *converted_string = g_malloc0(8 * strlen(utf8string)*sizeof(gchar));
		const gchar *srcp = utf8string;
		gunichar unichar = g_utf8_get_char(srcp);
		DEBUG_MSG("convert_string_utf8_to_html, utf8string='%s'\n", utf8string);
		while (unichar) {
			gchar converted[9];
			convert_unichar_to_htmlstring(unichar, converted, ascii, iso);
			converted_string = strncat(converted_string, converted, 8);
			srcp = g_utf8_next_char(srcp);
			unichar = g_utf8_get_char (srcp);
		}
		DEBUG_MSG("convert_string_utf8_to_html, converted string='%s'\n", converted_string);
		return converted_string;
	}
}

