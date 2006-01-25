/* $Id: bfspell.h,v 1.1.1.1 2005/06/29 11:03:16 kyanh Exp $ */
/*
 * Modified for Winefish (C) 2005 Ky Anh <kyanh@o2.pl>
 */

#ifdef HAVE_LIBASPELL
void spell_check_cb(GtkWidget *widget, Tbfwin *bfwin);
#endif /* HAVE_LIBASPELL */
