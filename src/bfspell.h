/* $Id: bfspell.h 608 2006-05-21 06:36:01Z kyanh $ */
/*
 * Modified for Winefish (C) 2005 Ky Anh <kyanh@o2.pl>
 */

#ifdef HAVE_LIBASPELL
void spell_check_cb(GtkWidget *widget, Tbfwin *bfwin);
gint func_spell_check(GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt);
#endif /* HAVE_LIBASPELL */
