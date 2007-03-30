/* $Id$ */

#ifndef __FUNC_GREP_H_
#define __FUNC_GREP_H_

#ifdef EXTERNAL_GREP
#ifdef EXTERNAL_FIND

/* void file_open_advanced_cb( Tbfwin *bfwin, gboolean open_files); */
gint func_grep( GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt );
void open_advanced_from_filebrowser( Tbfwin *bfwin, gchar *path );
/* void template_rescan_cb(Tbfwin *bfwin); */
gint func_template_list( GtkWidget *widget, GdkEventKey *kevent, Tbfwin *bfwin, gint opt );

#endif /* EXTERNAL_GREP */
#endif /* EXTERNAL_FIND */

#endif /* __FUNC_GREP_H_ */
