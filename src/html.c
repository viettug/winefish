/* $Id$ */

/* Winefish LaTeX Editor (based on Bluefish HTML Editor)
 * html.c - menu/toolbar callbacks, inserting functions, and other cool stuff 
 *
 * Copyright (C) 
 * 1998 Olivier Sessink and Chris Mazuc
 * 1999-2002 Olivier Sessink
 * rewrite November 2000 (C) Olivier Sessink
 * Modified for Winefish (C) 2005 2006 kyanh <kyanh@o2.pl> 
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
 * Changes by Antti-Juhani Kaijanaho <gaia@iki.fi> on 1999-10-20
 */
/*#define DEBUG*/

#include <gtk/gtk.h>

#include "bluefish.h"
#include "html.h" 	/* myself */
#include "html_diag.h" 	/* the new html dialog stuff  */
#include "bf_lib.h"	/* bf_str_repeat() */
#include "document.h" /* doc_insert_two_strings() */

void general_html_menu_cb(Tbfwin* bfwin,guint callback_action, GtkWidget *widget) {
	switch (callback_action) {
	case 1:
		doc_insert_two_strings(bfwin->current_document, "\\textbf{", "}");
		break;
	case 2:
		doc_insert_two_strings(bfwin->current_document, "\\textit{", "}");
		break;
	case 3:
		doc_insert_two_strings(bfwin->current_document, "\\underline{", "}");
		break;
	case 4:
		doc_insert_two_strings(bfwin->current_document, "{\\tiny ", "}");
		break;
	case 5:
		doc_insert_two_strings(bfwin->current_document, "{\\scriptsize ", "}");
		break;
	case 6:
		doc_insert_two_strings(bfwin->current_document, "{\\footnotesize ", "}");
		break;
	case 7:
		doc_insert_two_strings(bfwin->current_document, "{\\small ", "}");
		break;
	case 8:
		doc_insert_two_strings(bfwin->current_document, "{\\normalsize ", "}");
		break;
	case 9:
		doc_insert_two_strings(bfwin->current_document, "{\\large ", "}");
		break;
	case 10:
		doc_insert_two_strings(bfwin->current_document, "\\begin{center}\n", "\n\\end{center}");
		break;
	case 11:
		doc_insert_two_strings(bfwin->current_document, "{\\Large ", "}");
		break;
	case 12:
		doc_insert_two_strings(bfwin->current_document, "{\\huge ", "}");
		break;
	case 13:
		doc_insert_two_strings(bfwin->current_document, "\\begin{verbatim}\n", "\n\\end{verbatim}");
		break;
	case 14:
		doc_insert_two_strings(bfwin->current_document, "{\\Huge ", "}");
		break;
	case 15:
		doc_insert_two_strings(bfwin->current_document, "\\begin{split}\n","\n\\end{split}");
		break;
	case 16:
		doc_insert_two_strings(bfwin->current_document, "\\begin{cases}\n","\n\\end{cases}");
		break;
	case 17:
		doc_insert_two_strings(bfwin->current_document, "\\emph{", "}");
		break;
	case 18:
		doc_insert_two_strings(bfwin->current_document, "\\part{", "}\n");
		break;
	case 19:
		doc_insert_two_strings(bfwin->current_document, "\\chapter{", "}\n");
		break;
	case 20:
		doc_insert_two_strings(bfwin->current_document, "\\section{", "}\n");
		break;
	case 21:
		doc_insert_two_strings(bfwin->current_document, "\\subsection{", "}\n");
		break;
	case 22:
		doc_insert_two_strings(bfwin->current_document, "\\subsubsection{", "}\n");
		break;
	case 23:
		doc_insert_two_strings(bfwin->current_document, "\\paragraph{", "}\n");
		break;
	case 24:
		doc_insert_two_strings(bfwin->current_document, "\\begin{tabular}{", "}\n\n\\end{tabular}");
		break;
	case 25:
		doc_insert_two_strings(bfwin->current_document, "\\begin{aligned}\n", "\n\\end{aligned}");
		break;
	case 26:
		doc_insert_two_strings(bfwin->current_document, "\\begin{gathered}\n", "\n\\end{gathered}");
		break;
	case 27:
		doc_insert_two_strings(bfwin->current_document, "\\begin{equation}\n", "\n\\end{equation}");
		break;
	case 28:
		doc_insert_two_strings(bfwin->current_document, "\\begin{subequations}\n% \\renewcommand\\theequation{\\theparentequation \\roman{equation}}\n", "\n\\end{subequations}");
		break;
	case 29:
		doc_insert_two_strings(bfwin->current_document, "\\begin{gather}\n", "\n\\end{gather}");
		break;
	case 30:
		doc_insert_two_strings(bfwin->current_document, "\\begin{multline}\n", "\n\\end{multine}");
		break;
	case 31:
		doc_insert_two_strings(bfwin->current_document, "\\begin{align}\n", "\n\\end{align}");
		break;	
		/*		
	case 32:
		doc_insert_two_strings(bfwin->current_document, "\\begin{flalign}\n", "\n\\end{flalign}");
		break;	
		*/
	case 33:
		doc_insert_two_strings(bfwin->current_document, "\\begin{itemize}\n\\item", "\n\\item\n\\end{itemize}");
		break;
	case 34:
		doc_insert_two_strings(bfwin->current_document, "\\begin{enumerate}\n\\item", "\n\\item\n\\end{enumerate}");
		break;
	case 35:
		doc_insert_two_strings(bfwin->current_document, "\\item\n", "");
		break;
	case 36:
		doc_insert_two_strings(bfwin->current_document, "\\item[", "]\n");
		break;
	case 37:
		doc_insert_two_strings(bfwin->current_document, "\\begin{alignat}{", "}\n\n\\end{alignat}");
		break;
	case 38:
		doc_insert_two_strings(bfwin->current_document, "\\begin{equation*}\n", "\n\\end{equation*}");
		break;
	case 39:
		doc_insert_two_strings(bfwin->current_document, "\\begin{gather*}\n", "\n\\end{gather*}");
		break;
	case 40:
		doc_insert_two_strings(bfwin->current_document, "\\begin{multline*}\n", "\n\\end{multline*}");
		break;
	case 41:
		doc_insert_two_strings(bfwin->current_document, "\\begin{align*}\n", "\n\\end{align*}");
		break;
/*	case 42:
		doc_insert_two_strings(bfwin->current_document, "\\begin{flalign*}\n", "\n\\end{flalign*}");
		break;
		*/
	case 43:
		doc_insert_two_strings(bfwin->current_document, "\\begin{alignat*}{", "}\n\n\\end{alignat*}");
		break;
	case 48:
		doc_insert_two_strings(bfwin->current_document, "\\begin{verse}\n", "\n\\end{verse}");
		break;
	case 49:
		doc_insert_two_strings(bfwin->current_document, "\\begin{quote}\n", "\n\\end{quote}");
		break;
	case 50:
		doc_insert_two_strings(bfwin->current_document, "\\begin{flushleft}\n", "\n\\end{flushleft}");
		break;
	case 51:
		doc_insert_two_strings(bfwin->current_document, "\\begin{flushright}\n", "\n\\end{flushright}");
		break;
	case 52:
		doc_insert_two_strings(bfwin->current_document,  "\\begin{verbatim*}\n", "\n\\end{verbatim*}");
		break;
	case 100:
		doc_insert_two_strings(bfwin->current_document, "\\subparagraph{", "}\n");
		break;
	case 102:
		doc_insert_two_strings(bfwin->current_document, "\\begin{longtable}{", "}\n\n\\end{longtable}");	
		break;
	case 103:
		doc_insert_two_strings(bfwin->current_document, "\\begin{description}\n\\item[", "]\n\\item[]\n\\end{description}");
		break;
	case 104:
		doc_insert_two_strings(bfwin->current_document, "\\begin{enumerate}[", "]\n\\item\n\\item\n\\end{enumerate}");
		break;
	case 105:
		{
			gchar *tmpstr = g_strconcat("% ",bf_str_repeat("=",69),"\n",NULL);
			doc_insert_two_strings(bfwin->current_document, tmpstr, NULL);
			g_free(tmpstr);
		}
		break;
	case 108:
		doc_insert_two_strings(bfwin->current_document, "\\textsl{","}");
		break;
	case 110:
		doc_insert_two_strings(bfwin->current_document, "\\texttt{","}");
		break;
	case 111:
		doc_insert_two_strings(bfwin->current_document, "\\textsc{","}");
		break;
	case 112:
		doc_insert_two_strings(bfwin->current_document, "\\part*{", "}\n\\addcontentsline{toc}{part}{}\n");
		break;
	case 113:
		doc_insert_two_strings(bfwin->current_document, "\\chapter*{", "}\n\\addcontentsline{toc}{chapter}{}");
		break;
	case 114:
		doc_insert_two_strings(bfwin->current_document, "\\section*{", "}\n\\addcontentsline{toc}{section}{}");
		break;
	case 115:
		doc_insert_two_strings(bfwin->current_document, "\\subsection*{", "}\n\\addcontentsline{toc}{subsection}{}");
		break;
	case 116:
		doc_insert_two_strings(bfwin->current_document, "\\subsubsection*{", "}\n\\addcontentsline{toc}{subsubsection}{}");
		break;
/*
	case 117:
		doc_insert_two_strings(bfwin->current_document, "\\mathrm{","}");
		break;
	case 118:
		doc_insert_two_strings(bfwin->current_document, "\\mathit{","}");
		break;
	case 119:
		doc_insert_two_strings(bfwin->current_document, "\\mathbf{","}");
		break;
	case 120:
		doc_insert_two_strings(bfwin->current_document, "\\mathsf{","}");
		break;
	case 121:
		doc_insert_two_strings(bfwin->current_document, "\\mathtt{","}");
		break;
	case 122:
		doc_insert_two_strings(bfwin->current_document, "\\mathcal{","}");
		break;
	case 123:
		doc_insert_two_strings(bfwin->current_document, "\\mathbb{","}");
		break;
	case 124:
		doc_insert_two_strings(bfwin->current_document, "\\mathfrak{","}");
		break;
*/
/*
	case 125:
		doc_insert_two_strings(bfwin->current_document, "\\thinspace ","");
		break;
	case 126:
		doc_insert_two_strings(bfwin->current_document, "\\medspace ","");
		break;
	case 127:
		doc_insert_two_strings(bfwin->current_document, "\\thickspace ","");
		break;
	case 128:
		doc_insert_two_strings(bfwin->current_document, "\\quad ","");
		break;
	case 129:
		doc_insert_two_strings(bfwin->current_document, "\\quadquad ","");
		break;
	case 130:
		doc_insert_two_strings(bfwin->current_document, "\\negthinspace ","");
		break;
	case 131:
		doc_insert_two_strings(bfwin->current_document, "\\negmedspace ","");
		break;
	case 132:
		doc_insert_two_strings(bfwin->current_document, "\\negthickspace ","");
		break;
*/
	default:
		break;
	}
}


/************************************************************************/
/* END OF FILE */
