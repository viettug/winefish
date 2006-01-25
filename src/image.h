/* $Id: image.h,v 1.1.1.1 2005/06/29 11:03:26 kyanh Exp $ */
/*
 * Modified for Winefish (C) 2005 Ky Anh <kyanh@o2.pl> 
 */

#ifndef __IMAGE_H_
#define __IMAGE_H_

#include "html_diag.h"

void image_insert_dialog(Tbfwin *bfwin, Ttagpopup *data);
void image_insert_from_filename(Tbfwin *bfwin, gchar *filename);
#endif /* __IMAGE_H_ */
