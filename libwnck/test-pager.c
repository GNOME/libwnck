
#include <libwnck/libwnck.h>
#include <gtk/gtk.h>

static void
create_pager_window (WnckScreen *screen,
                     GtkOrientation orientation)
{
  GtkWidget *win;
  GtkWidget *pager;
  GtkWidget *frame;
  
  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_stick (GTK_WINDOW (win));
#if 0
  wnck_gtk_window_set_dock_type (GTK_WINDOW (win));
#endif
  
  gtk_window_set_title (GTK_WINDOW (win), "Pager");

  /* very very random */
  gtk_window_move (GTK_WINDOW (win),
                   gdk_screen_width () / 2,
                   gdk_screen_height () - 20);
  
  /* quit on window close */
  g_signal_connect (G_OBJECT (win), "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (win), frame);
  
  pager = wnck_pager_new (screen);

  wnck_pager_set_orientation (WNCK_PAGER (pager), orientation);
  wnck_pager_set_n_rows (WNCK_PAGER (pager), 3);
  
  gtk_container_add (GTK_CONTAINER (frame), pager);  
  
  gtk_widget_show_all (win);
}

int
main (int argc, char **argv)
{
  WnckScreen *screen;
  
  gtk_init (&argc, &argv);

  screen = wnck_screen_get (0);

  /* because the pager doesn't respond to signals at the moment */
  wnck_screen_force_update (screen);
  
  create_pager_window (screen, GTK_ORIENTATION_HORIZONTAL);
  /*   create_pager_window (screen, GTK_ORIENTATION_VERTICAL); */
  
  gtk_main ();
  
  return 0;
}
