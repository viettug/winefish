/* $Id: image.h 631 2007-03-30 17:14:06Z kyanh $ */
/*
 * Modified for Winefish (C) 2005 Ky Anh <kyanh@o2.pl> 
 */

#ifndef __IMAGE_H_
#define __IMAGE_H_

#include "html_diag.h"

void image_insert_dialog(Tbfwin *bfwin, Ttagpopup *data);
void image_insert_from_filename(Tbfwin *bfwin, gchar *filename);
#endif /* __IMAGE_H_ */
