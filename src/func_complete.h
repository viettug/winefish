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

gint func_complete_show( GtkWidget *widget, Tbfwin *win );
gint func_complete_hide( Tbfwin *bfwin );
gint func_complete_delete( GtkWidget *widget, Tbfwin *bfwin );
gint func_complete_move( GdkEventKey *kevent , Tbfwin *win );
gint func_complete_do( Tbfwin *bfwin );
gint func_complete_eat( GtkWidget *widget, GdkEventKey *kevent, Tdocument *doc );

#endif /* __FUNC_COMPLETE_H__ */
