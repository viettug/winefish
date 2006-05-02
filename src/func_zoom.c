/* $Id$ */

/* Winefish LaTeX Editor (based on Bluefish)
 *
 * Copyright (c) 2006 kyanh <kyanh@o2.pl>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
 
#include <gtk/gtk.h>
#define DEBUG
#include "bluefish.h"

gint func_zoom( GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt) {
	DEBUG_MSG("func_zoom: started; zoom out = %d\n", opt & FUNC_VALUE_0);
	Tdocument *doc = bfwin->current_document;
	if (!doc) return 0;
	PangoFontDescription *font_desc;
	gchar *fontstring;
	gint size;

	font_desc =  (GTK_WIDGET(doc->view)->style)->font_desc;
	size = pango_font_description_get_size(font_desc);
	
#ifdef DEBUG
	PangoFontMask font_mask;
	font_mask = pango_font_description_get_set_fields(font_desc);
	DEBUG_MSG("func_zoom: size=%d; mask_size = %d\n", size, font_mask & PANGO_FONT_MASK_SIZE);
#endif /* DEBUG */
	if (!size) {
		DEBUG_MSG("func_zoom: the font size wasnot specified; return...\n");
		return 0;
	}
	if (pango_font_description_get_size_is_absolute(font_desc)) {
		DEBUG_MSG("func_zoom: absolute size was used\n");
		if (opt & FUNC_VALUE_0) {
			size -= 1;
		}else{
			size += 1;
		}
		pango_font_description_set_absolute_size(font_desc, size);
	}else{
		DEBUG_MSG("func_zoom: logical size was used\n");
		if (opt & FUNC_VALUE_0) {
			size -= PANGO_SCALE;
		}else{
			size += PANGO_SCALE;
		}
		pango_font_description_set_size(font_desc, size);
	}
	fontstring = pango_font_description_to_string(font_desc);
	DEBUG_MSG("func_zoom: new font string: %s\n", fontstring);
	if(main_v->props.editor_font_string) g_free(main_v->props.editor_font_string);
	main_v->props.editor_font_string = fontstring;
	gtk_widget_modify_font(doc->view, font_desc);
	/* (pango_font_description_to_string) after (gtk_widget_modify_font) may return
	a very strange result: 0 Oblique Ultra-Condensed 1353311744 */
	return 1;
}
