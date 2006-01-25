/* $Id$ */

/* Winefish LaTeX Editor (based on Bluefish)
 * external_commands.c - backend for external commands, filters and the outputbox
 *
 * Copyright (C) 1999-2005 Olivier Sessink
 * Modified for Winefish (C) 2006 kyanh <kyanh@o2.pl>
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

#ifndef __OUTPUTBOX_BF_H_
#define __OUTPUTBOX_BF_H_

#include "outputbox_cfg.h"

#ifdef __BF_BACKEND__

void run_command( Toutputbox *ob );
void finish_execute( Toutputbox *ob );

/*
void filter_command(Tbfwin *bfwin, const gchar *formatstring);
void external_command(Tbfwin *bfwin, const gchar *formatstring);
*/

#endif /* __BF_BACKEND__ */

#endif /* __EXTERNAL_COMMANDS_H_ */
