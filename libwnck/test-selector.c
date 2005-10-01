
#include <libwnck/libwnck.h>
#include <gtk/gtk.h>

int
main (int argc, char **argv)
{
  WnckScreen *screen;
  GtkWidget *win;
  GtkWidget *frame;
  GtkWidget *selector;
  
  gtk_init (&argc, &argv);

  screen = wnck_screen_get_default ();
  
  /* because the pager doesn't respond to signals at the moment */
  wnck_screen_force_update (screen);
  
  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 200, 32);
  gtk_window_stick (GTK_WINDOW (win));
  /*   wnck_gtk_window_set_dock_type (GTK_WINDOW (win)); */

  gtk_window_set_title (GTK_WINDOW (win), "Window Selector");
  gtk_window_set_policy (GTK_WINDOW (win),
			 TRUE, TRUE, FALSE);

  /* quit on window close */
  g_signal_connect (G_OBJECT (win), "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  selector = wnck_selector_new (screen);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (win), frame);

  gtk_container_add (GTK_CONTAINER (frame), selector);  

  gtk_widget_show (selector);
  gtk_widget_show (frame);

  gtk_window_move (GTK_WINDOW (win), 0, 0);
  
  gtk_widget_show (win);
  
  gtk_main ();
  
  return 0;
}
