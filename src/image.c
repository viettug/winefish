/* $Id$ */
/*
 * Modified for Winefish (C) 2005 Ky Anh <kyanh@o2.pl> 
 */

#include <gtk/gtk.h>
#include <string.h>

/*#define DEBUG*/

#include "bluefish.h"
#include "html_diag.h"
#include "document.h"
#include "gtk_easy.h"
#include "bf_lib.h"
#include "stringlist.h"

typedef struct
{
  Thtml_diag *dg;
  GtkWidget *frame;
  GdkPixbuf *pb;
  GtkWidget *im;

  /* and some options for the thumbnails */
  /* kyanh, removed, 20050219
  	gboolean is_thumbnail;
  */
  GtkAdjustment *adjustment; /* no-widget object */
  /* kyanh, removed, 20050225,
  guint adj_changed_id; */
}
Timage_diag;

void image_diag_destroy_cb(GtkWidget * widget, Timage_diag *imdg)
{
  html_diag_destroy_cb(widget,imdg->dg); /* destroy the dialog */
  if (imdg->pb)
  {
    g_object_unref(imdg->pb); /* destroy the image (PixBuf Object) */
  }
  g_free(imdg);
}

static void image_insert_dialogok_lcb(GtkWidget * widget, Timage_diag *imdg)
{
  gchar *finalstring,*scale,*caption,*label;

  finalstring = gtk_editable_get_chars(GTK_EDITABLE(imdg->dg->entry[3]), 0, -1);
  if (strlen(finalstring) ==0)
  {
    DEBUG_MSG("image_insert_dialogok_lcb: empty relative path; use the absolute one.\n");
    finalstring = gtk_editable_get_chars(GTK_EDITABLE(imdg->dg->entry[0]), 0, -1);
  }
  label = gtk_editable_get_chars(GTK_EDITABLE(imdg->dg->entry[1]), 0, -1);
  caption = gtk_editable_get_chars(GTK_EDITABLE(imdg->dg->entry[2]), 0, -1);
  if (strlen(label) && (strcmp(label,"fig:") != 0) )
  {
    label = g_strconcat("\n\\label{",label,"}",NULL);
  }
  else
  {
    /* remove label=fig: */
    label = g_strdup("");
  }
  if (strcmp(caption,"")!=0)
  {
    caption = g_strconcat("\n\\caption{",caption,"}",NULL);
  }
  scale = g_strdup_printf("scale=%.2f",imdg->adjustment->value);
  /* fix BUG#27 */
  finalstring = g_strconcat("\\begin{figure}[htb]\n\\begin{center}\n\\ifpdf\n\t\\includegraphics[",scale,"]{",finalstring,"}\n\\else\n%\t\\includegraphics[",scale,"]{",finalstring,"}\n\\fi",caption,label,"\n\\end{center}\n\\end{figure}",NULL);
  g_free(scale);
  g_free(label);
  g_free(caption);
  doc_insert_two_strings(imdg->dg->doc, finalstring, NULL);
  g_free(finalstring);
  image_diag_destroy_cb(NULL, imdg);
}

void image_diag_cancel_clicked_cb(GtkWidget *widget, gpointer data)
{
  image_diag_destroy_cb(NULL, data);
}

static void image_filename_changed(GtkWidget * widget, Timage_diag *imdg)
{
  /* kyanh, add checking G_FILE_TEST_EXISTS, 20050225
  if file doesnot exist (for e.g., when user enters some stupid char in text box),
  although gdk_pixbuf_new_from_file() return NULL (NO ERROR:), we still receive some
  GLib-GObject-CRITICAL ERROR when the progam exits.

  Here we can user the ERRORS CODE returned by gdk_pixbuf_new_from_file (FILE ERROR or PIX_ERROR)
  */
  /*
  if (g_file_test(gtk_editable_get_chars(GTK_EDITABLE(imdg->dg->entry[0]), 0, -1),G_FILE_TEST_EXISTS)) {
  */
  gint pb_width, pd_height, toobig;
  GdkPixbuf *tmp_pb;
  DEBUG_MSG("image_filename_changed() started. GTK_IS_WIDGET(imdg->im) == %d\n", GTK_IS_WIDGET(imdg->im));
  if (imdg->pb)
  {
    g_object_unref(imdg->pb);
  }

  imdg->pb = gdk_pixbuf_new_from_file(gtk_entry_get_text(GTK_ENTRY(imdg->dg->entry[0])), NULL);

  if (!imdg->pb)
  {
    return;
  }
  toobig = 1;
  pb_width = gdk_pixbuf_get_width(imdg->pb);
  if ((pb_width / 250) > toobig)
  {
    toobig = pb_width / 250;
  }
  pd_height = gdk_pixbuf_get_height(imdg->pb);
  if ((pd_height / 300) > toobig)
  {
    toobig = pd_height / 300;
  }

  tmp_pb = gdk_pixbuf_scale_simple(imdg->pb, (pb_width / toobig), (pd_height / toobig), /* kyanh, removed, 20020220, main_v->props.image_thumbnail_refresh_quality ? */ GDK_INTERP_BILINEAR /* :GDK_INTERP_NEAREST */);

  if (GTK_IS_WIDGET(imdg->im))
  {
    DEBUG_MSG("imdg->im == %p\n", imdg->im);
    DEBUG_MSG("gtk_widget_destroy() %p\n", imdg->im);
    gtk_widget_destroy(imdg->im);
  }

  imdg->im = gtk_image_new_from_pixbuf(tmp_pb);

  g_object_unref(tmp_pb);
  gtk_container_add(GTK_CONTAINER(imdg->frame), imdg->im);

  /* Update Relative Path */
  gchar *relpath;
  relpath = gtk_editable_get_chars(GTK_EDITABLE(imdg->dg->entry[0]),0,-1);
  if (imdg->dg->bfwin->project)
  {
    relpath  = create_relative_link_to(imdg->dg->bfwin->project->basedir,relpath);
  }
  else
  {
    relpath  = create_relative_link_to(imdg->dg->doc->filename,relpath);
  }
  if (!relpath)
  {
    relpath = gtk_editable_get_chars(GTK_EDITABLE(imdg->dg->entry[0]),0,-1);
  }
  DEBUG_MSG("relpath: %s\n",relpath);
  gtk_entry_set_text(GTK_ENTRY(imdg->dg->entry[3]),relpath);
  g_free(relpath);

  /* Show the widget */
  gtk_widget_show(imdg->im);
  DEBUG_MSG("imdg->im == %p\n", imdg->im);
  DEBUG_MSG("image_filename_changed() finished. GTK_IS_WIDGET(imdg->im) == %d\n", GTK_IS_WIDGET(imdg->im));
}

static void image_adjust_changed(GtkAdjustment * adj, Timage_diag *imdg)
{
  /*
  if (g_file_test(gtk_editable_get_chars(GTK_EDITABLE(imdg->dg->entry[0]), 0, -1),G_FILE_TEST_EXISTS)) {
  */
  GdkPixbuf *tmp_pb;
  gint tn_width, tn_height;
  DEBUG_MSG("image_adjust_changed started. GTK_IS_WIDGET(imdg->im) == %d\n", GTK_IS_WIDGET(imdg->im));
  if (!imdg->pb)
  {
    image_filename_changed(NULL, imdg);
    return;
  }

  tn_width = imdg->adjustment->value * gdk_pixbuf_get_width(imdg->pb);
  tn_height = imdg->adjustment->value * gdk_pixbuf_get_height(imdg->pb);

  tmp_pb = gdk_pixbuf_scale_simple(imdg->pb, tn_width, tn_height, /* kyanh, removed, 20020220, main_v->props.image_thumbnail_refresh_quality ? */ GDK_INTERP_BILINEAR /*: GDK_INTERP_NEAREST */);

  if (GTK_IS_WIDGET(imdg->im))
  {
    DEBUG_MSG("imdg->im == %p\n", imdg->im);
    DEBUG_MSG("gtk_widget_destroy() %p\n", imdg->im);
    gtk_widget_destroy(imdg->im);
  }

  imdg->im = gtk_image_new_from_pixbuf(tmp_pb);
  g_object_unref(tmp_pb);
  gtk_container_add(GTK_CONTAINER(imdg->frame), imdg->im);
  gtk_widget_show(imdg->im);
  DEBUG_MSG("image_adjust_changed finished. GTK_IS_WIDGET(imdg->im) == %d\n", GTK_IS_WIDGET(imdg->im));
}

void image_insert_dialog_backend(gchar *filename,Tbfwin *bfwin, Ttagpopup *data,/* kyanh, tobe removed */gboolean is_thumbnail)
{
  gchar *tagvalues[6];
  gchar *custom = NULL;
  Timage_diag *imdg;
  GtkWidget *dgtable;

  imdg = g_new(Timage_diag, 1);
  imdg->pb = NULL;
  imdg->im = NULL;
  /* kyanh, removed, 20050219
  	imdg->is_thumbnail = is_thumbnail;
  */
  imdg->dg = html_diag_new(bfwin,_("Insert image"));
  {
    /* kyanh, because it is 'static', the value can pass through ...? */
    static gchar *tagitems[] = {"src","label","caption","scale","relsrc", NULL };
    fill_dialogvalues(tagitems, tagvalues, &custom, (Ttagpopup *) data, imdg->dg);
  }

  imdg->frame = gtk_frame_new(_("Preview"));
  /*	gtk_widget_set_usize(imdg->frame, -1, 50);*/
  gtk_box_pack_start(GTK_BOX(imdg->dg->vbox), imdg->frame, TRUE, TRUE, 0);
  /* SCALING */
  GtkWidget *scale;
  imdg->adjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0.5/*init*/, 0.0001/*min*/, 1.1/*max*/, 0.001/*small step*/, 0.1/*bigstep*/, 0.1/*pagesize*/));
  scale = gtk_hscale_new(imdg->adjustment);
  /* kyanh, removed unused variable, 20050225
  imdg->adj_changed_id = */
  g_signal_connect(G_OBJECT(imdg->adjustment), "value_changed", G_CALLBACK(image_adjust_changed), imdg);
  gtk_scale_set_digits(GTK_SCALE(scale), 3);
  gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_DISCONTINUOUS);
  gtk_box_pack_start(GTK_BOX(imdg->dg->vbox), scale, FALSE, FALSE, 0);

  dgtable = html_diag_table_in_vbox(imdg->dg, 5, 9); /* rows, cols */
  if (filename)
  {
    imdg->dg->entry[0] = entry_with_text(filename, 1024);
    DEBUG_MSG("image_insert: from filebrowser.\n");
  }
  else
  {
    imdg->dg->entry[0] = entry_with_text(tagvalues[0], 1024);
  }
  bf_mnemonic_label_tad_with_alignment(_("_Image location:"), imdg->dg->entry[0], 0, 0.5, dgtable, 0, 1, 0, 1);
  gtk_table_attach_defaults(GTK_TABLE(dgtable), imdg->dg->entry[0], 1, 7, 0, 1);
  gtk_table_attach_defaults(GTK_TABLE(dgtable), file_but_new(imdg->dg->entry[0], 1, bfwin), 7, 9, 0, 1);
  g_signal_connect(G_OBJECT(imdg->dg->entry[0]), "changed", G_CALLBACK(image_filename_changed), imdg);

  imdg->dg->entry[3] = entry_with_text(tagvalues[4], 1024);
  bf_mnemonic_label_tad_with_alignment(_("Relative Path:"), imdg->dg->entry[3], 0, 0.5, dgtable, 0, 1, 1, 2);
  gtk_table_attach_defaults(GTK_TABLE(dgtable), imdg->dg->entry[3], 1, 7, 1, 2);

  imdg->dg->entry[1] = entry_with_text(tagvalues[1], 1024);
  bf_mnemonic_label_tad_with_alignment(_("Label:"), imdg->dg->entry[1], 0, 0.5, dgtable, 0, 1, 2, 3);
  gtk_table_attach_defaults(GTK_TABLE(dgtable), imdg->dg->entry[1], 1, 7, 2, 3);
  gtk_entry_set_text(GTK_ENTRY(imdg->dg->entry[1]),"fig:");

  imdg->dg->entry[2] = entry_with_text(tagvalues[2], 1024);
  bf_mnemonic_label_tad_with_alignment(_("Caption:"), imdg->dg->entry[2], 0, 0.5, dgtable, 0, 1, 3, 4);
  gtk_table_attach_defaults(GTK_TABLE(dgtable), imdg->dg->entry[2], 1, 7, 3, 4);

  if (filename/* kyanh, removed, 20050226, || tagvalues[0] */)
  {
    /* BUG[200502]#8 */
    g_signal_emit_by_name(G_OBJECT(imdg->dg->entry[0]), "changed"/*, imdg*/);
  }

  {
    /* the function image_diag_finish() */
    /* add CANCEL and OK buttons */
    GtkWidget *hbox;

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_hseparator_new(), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(imdg->dg->vbox), hbox, FALSE, FALSE, 12);
    hbox = gtk_hbutton_box_new();
    gtk_hbutton_box_set_layout_default(GTK_BUTTONBOX_END);
    gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 12);

    imdg->dg->obut = bf_stock_ok_button(G_CALLBACK(image_insert_dialogok_lcb), imdg);
    imdg->dg->cbut = bf_stock_cancel_button(G_CALLBACK(image_diag_cancel_clicked_cb), imdg);

    gtk_box_pack_start(GTK_BOX(hbox),imdg->dg->cbut , FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox),imdg->dg->obut , FALSE, FALSE, 0);
    gtk_window_set_default(GTK_WINDOW(imdg->dg->dialog), imdg->dg->obut);

    gtk_box_pack_start(GTK_BOX(imdg->dg->vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show_all(GTK_WIDGET(imdg->dg->dialog));
  }
  /* TODO: removed */
  if (custom)
  {
    g_free(custom);
  }
}

/* two instances of image_insert_dialog_backend */

void image_insert_dialog(Tbfwin *bfwin, Ttagpopup *data)
{
  image_insert_dialog_backend(NULL, bfwin, data, FALSE);
}
void image_insert_from_filename(Tbfwin *bfwin, gchar *filename)
{
  image_insert_dialog_backend(filename,bfwin, NULL, FALSE);
}

