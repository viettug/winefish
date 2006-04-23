#ifndef __FUNC_COMPLETE_H__
#define __FUNC_COMPLETE_H__
gboolean completion_popup_menu( GtkWidget *widget, GdkEventKey *kevent, Tdocument *doc );
gint func_complete_delete();
gint func_complete_move();
gint func_complete_do(Tdocument *doc);
gint func_complete_eat( GtkWidget *widget, GdkEventKey *kevent, Tdocument *doc );
#endif /* __FUNC_COMPLETE_H__ */
