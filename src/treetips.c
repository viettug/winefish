/* $Id$ */
#include "treetips.h"

static gint tree_tips_paint(TreeTips *tips)
{
  if (!tips->window) return FALSE;
  if (!GTK_WIDGET_VISIBLE(tips->window)) return FALSE;
  gtk_paint_flat_box (tips->window->style, tips->window->window,
                      GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_IN,
                      NULL, tips->window, "treetip",
                      0, 0, -1, -1);  
  gtk_paint_shadow (tips->window->style, tips->window->window,
                      GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_IN,
                      NULL, tips->window, "treetip",
                      0, 0, -1, -1);                        
  return FALSE;                      
}




void tree_tips_hide(TreeTips *tips)
{
  if (tips && tips->window)
  {
    gtk_widget_hide_all(tips->window);
  }  
  if (tips->hide_tout)
  g_source_remove (tips->hide_tout);  
}

gboolean tree_tips_hide_cb(GtkWidget *widget,GdkEventFocus *event,gpointer user_data)
{
  tree_tips_hide(user_data);
  return FALSE;
}


void tree_tips_show(TreeTips *tips)
{
  GtkWidget *label;
  gint x,y;
  gchar *pstr;

  if (!tips) return;  
  if (!tips->enabled) return;
  if (tips->window) return; 
 
  pstr = ((* tips->func)(tips->bfwin,tips->tree, tips->posx,tips->posy));
  if (pstr)
  {
     tips->window = gtk_window_new (GTK_WINDOW_POPUP);
     gtk_widget_set_app_paintable (tips->window, TRUE);
     gtk_window_set_resizable (GTK_WINDOW(tips->window), FALSE);
     gtk_widget_set_name (tips->window, "tree-tooltips");
     gtk_container_set_border_width (GTK_CONTAINER (tips->window), 4); 
     g_signal_connect_swapped(GTK_WINDOW(tips->window),"expose-event",G_CALLBACK(tree_tips_paint),tips);
     label = gtk_label_new (NULL);  
     gtk_label_set_markup(GTK_LABEL(label),pstr);
     gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
     gtk_container_add (GTK_CONTAINER (tips->window), GTK_WIDGET (label));  
     gtk_window_set_position (GTK_WINDOW(tips->window), GTK_WIN_POS_MOUSE);  
     gdk_window_get_pointer (NULL, &x, &y, NULL);
     gtk_window_move (GTK_WINDOW(tips->window), x + 8, y + 16);  
     gtk_widget_show_all(tips->window);       
     g_free(pstr);
     tips->hide_tout = g_timeout_add (tips->hide_interval, (GSourceFunc) tree_tips_hide, tips);         
  }  
}

gboolean tree_tips_mouse_enter(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  TreeTips *tips = TREE_TIPS(user_data);
  
  if ( event->type != GDK_ENTER_NOTIFY )
     return FALSE;
  if (!tips->enabled) return FALSE;     
  if ( ((GdkEventCrossing *) event)->x == tips->posx &&
       ((GdkEventCrossing *) event)->y == tips->posy  )   
     return FALSE;
  if (tips->window)
  {
    gtk_widget_destroy(tips->window);
    tips->window = NULL;
    g_source_remove (tips->hide_tout);      
  }
  tips->tip_outside = FALSE;
  return FALSE;
}

gboolean tree_tips_mouse_leave(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  TreeTips *tips = TREE_TIPS(user_data);
  
 if ( event->type != GDK_LEAVE_NOTIFY )  return FALSE;
 if (!tips->enabled) return FALSE; 
 if ( ((GdkEventCrossing *) event)->x == tips->posx &&
      ((GdkEventCrossing *) event)->y == tips->posy )   
     return FALSE;
 if (tips->window)
  {
    gtk_widget_destroy(tips->window);
    tips->window = NULL;
    g_source_remove (tips->hide_tout);      
  }
 if (tips->tip_on)
  {
    if (tips->show_tout)
       g_source_remove (tips->show_tout);
    tips->tip_on = FALSE;
  }    
  tips->tip_outside = TRUE;
  return FALSE;
}

gboolean tree_tips_mouse_motion(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  TreeTips *tips = TREE_TIPS(user_data);
  if (!tips->enabled) return FALSE;
  if (tips->window)
  {
    gtk_widget_destroy(tips->window);
    tips->window = NULL;
    if (tips->hide_tout)
       g_source_remove (tips->hide_tout);
  }
  
  if (event->type == GDK_MOTION_NOTIFY && tips->tip_outside == FALSE)
  {
    tips->posx = ((GdkEventMotion *) event)->x;
    tips->posy = ((GdkEventMotion *) event)->y;
    if (tips->tip_on)
    {
      g_source_remove (tips->show_tout);
      tips->tip_on = FALSE;
    }
    tips->show_tout = g_timeout_add (tips->show_interval, (GSourceFunc) tree_tips_show, tips);    
    tips->tip_on = TRUE;
  }  
  return FALSE;
}


TreeTips *tree_tips_new(Tbfwin *win)
{
  TreeTips *tips = g_new0(TreeTips,1);
  tips->bfwin = win;
  tips->window = NULL;
  tips->tree = NULL;
  tips->func = NULL;
  tips->posx=tips->posy=0;
  tips->tip_on = FALSE;
  tips->tip_outside = TRUE;
  tips->show_interval = 500;
  tips->hide_interval = 1500;  
  tips->enabled = TRUE;
  tips->show_tout=0;
  tips->hide_tout=0;
  return tips;
}

TreeTips *tree_tips_new_full(Tbfwin *win,GtkTreeView *tree,TreeTipsCFunc function)
{
  TreeTips *tips = g_new0(TreeTips,1);
  tips->bfwin = win;
  tips->window = NULL;
  tips->tree = tree;
  tips->func = function;
  tips->posx=tips->posy=0;
  tips->tip_on = FALSE;
  tips->tip_outside = TRUE;
  tips->enabled = TRUE;
  if (tree)
  {
    g_signal_connect(G_OBJECT(tree),"motion-notify-event",G_CALLBACK(tree_tips_mouse_motion),tips);                           
    g_signal_connect(G_OBJECT(tree),"enter-notify-event",G_CALLBACK(tree_tips_mouse_enter),tips);                           
    g_signal_connect(G_OBJECT(tree),"leave-notify-event",G_CALLBACK(tree_tips_mouse_leave),tips);
    g_signal_connect(G_OBJECT(tree),"focus-out-event",G_CALLBACK(tree_tips_hide_cb),tips);
  }      
  tips->show_interval = 500;  
  tips->hide_interval = 1500;    
  tips->show_tout=0;
  tips->hide_tout=0;  
  return tips;
}


void tree_tips_set_show_interval(TreeTips *tips,gint interv)
{
  if (tips)
     tips->show_interval = interv;
}

void tree_tips_set_hide_interval(TreeTips *tips,gint interv)
{
  if (tips)
     tips->hide_interval = interv;
}


void tree_tips_set_content_function(TreeTips *tips,TreeTipsCFunc func)
{
  if (tips)
     tips->func = func;
}

void tree_tips_set_tree(TreeTips *tips,GtkTreeView *tree)
{
  if (tips)
  {
     tips->tree = tree;
     if (tree)
     {
        g_signal_connect(G_OBJECT(tree),"motion-notify-event",G_CALLBACK(tree_tips_mouse_motion),tips->window);                           
        g_signal_connect(G_OBJECT(tree),"enter-notify-event",G_CALLBACK(tree_tips_mouse_enter),tips->window);                           
        g_signal_connect(G_OBJECT(tree),"leave-notify-event",G_CALLBACK(tree_tips_mouse_leave),tips->window);
        g_signal_connect(G_OBJECT(tree),"focus-out-event",G_CALLBACK(tree_tips_hide),tips);        
     }           
  }   
}

void tree_tips_destroy(TreeTips *tips)
{
 tips->tree = NULL;
 if (tips->window)
  {
    gtk_widget_destroy(tips->window);
    tips->window = NULL;
  }  
 g_free(tips);
}


void tree_tips_set_enabled(TreeTips *tips,gboolean en)
{
  if (!tips) return;
  tips->enabled = en;
}

