#ifndef __FUNC_COMPLETE_H__
#define __FUNC_COMPLETE_H__
gint func_complete_show( GtkWidget *widget, Tdocument *doc );
gint func_complete_delete();
gint func_complete_move();
gint func_complete_do( Tdocument *doc );
gint func_complete_eat( GtkWidget *widget, GdkEventKey *kevent, Tdocument *doc );
#endif /* __FUNC_COMPLETE_H__ */
