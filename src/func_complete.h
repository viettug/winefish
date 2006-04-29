#ifndef __FUNC_COMPLETE_H__
#define __FUNC_COMPLETE_H__

typedef struct {
	GtkWidget *window; /* popup window */
	GtkWidget *treeview; /* we hold it so we scroll */
	gint show; /* 1: show; 0: hide; */
	gchar *cache; /* temporary buffer */
	/* gpointer bfwin; */
} Tcompletion;

#define COMPLETION(var) ( (Tcompletion*)var )

gint func_complete_show( GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt );
/* gint func_complete_force( GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt ); */
gint func_complete_hide( Tbfwin *bfwin );
gint func_complete_delete( GtkWidget *widget, Tbfwin *bfwin );
gint func_complete_move( GdkEventKey *kevent , Tbfwin *bfwin );
gint func_complete_do( Tbfwin *bfwin );
gint func_complete_eat( GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt );
gint func_complete_eat_env( GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt );
#endif /* __FUNC_COMPLETE_H__ */
