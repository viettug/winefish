#ifndef __FUNC_COMMENT_H__
#define __FUNC_COMMENT_H__
void menu_comment_cb( Tbfwin *bfwin, guint callback_action, GtkWidget *widget );
gint func_comment(GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin);
gint func_uncomment(GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin);
#endif /* __FUNC_COMMENT_H__ */
