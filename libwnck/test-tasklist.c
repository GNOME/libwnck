
#include <libwnck/libwnck.h>
#include <gtk/gtk.h>

int
main (int argc, char **argv)
{
  WnckScreen *screen;
  GtkWidget *win;
  
  gtk_init (&argc, &argv);

  screen = wnck_screen_get (0);
  
  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title (GTK_WINDOW (win), "Task List");

  /* quit on window close */
  g_signal_connect (G_OBJECT (win), "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);
  
  gtk_widget_show_all (win);
  
  gtk_main ();
  
  return 0;
}
