#ifndef __FUNC_COMPLETE_H__
#define __FUNC_COMPLETE_H__
gint func_complete_show( GtkWidget *widget, Tbfwin *win );
gint func_complete_hide();
gint func_complete_delete( GtkWidget *widget, Tbfwin *bfwin );
gint func_complete_move( GdkEventKey *kevent );
gint func_complete_do( Tdocument *doc );
gint func_complete_eat( GtkWidget *widget, GdkEventKey *kevent, Tdocument *doc );
#endif /* __FUNC_COMPLETE_H__ */
