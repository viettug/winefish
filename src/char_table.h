/* $Id$ */
/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * char_table.h - character convertion functions and data structures
 *
 * Copyright (C) 2002-2004 Olivier Sessink
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
 */

gchar *convert_string_utf8_to_html(const gchar *utf8string, gboolean ascii, gboolean iso);
#define xml_escape(var) convert_string_utf8_to_html(var, TRUE, FALSE);
#define html_escape(var) convert_string_utf8_to_html(var, TRUE, FALSE);
