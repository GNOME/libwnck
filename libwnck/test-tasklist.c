
#include <libwnck/libwnck.h>
#include <gtk/gtk.h>

static gboolean display_all = FALSE;

static GOptionEntry entries[] = {
	{"display-all", 'a', 0, G_OPTION_ARG_NONE, &display_all, N_("Display windows from all workspaces"), NULL},
	{NULL }
};

int
main (int argc, char **argv)
{
  GOptionContext* ctxt;
  WnckScreen *screen;
  GtkWidget *win;
  GtkWidget *frame;
  GtkWidget *tasklist;
  
  ctxt = g_option_context_new ("");
  g_option_context_add_main_entries (ctxt, entries, NULL);
  g_option_context_add_group (ctxt, gtk_get_option_group (TRUE));
  g_option_context_parse (ctxt, &argc, &argv, NULL);
  g_option_context_free (ctxt);
  ctxt = NULL;

  screen = wnck_screen_get_default ();
  
  /* because the pager doesn't respond to signals at the moment */
  wnck_screen_force_update (screen);
  
  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 200, 100);
  gtk_window_stick (GTK_WINDOW (win));
  /*   wnck_gtk_window_set_dock_type (GTK_WINDOW (win)); */

  gtk_window_set_title (GTK_WINDOW (win), "Task List");
  gtk_window_set_policy (GTK_WINDOW (win),
			 TRUE, TRUE, FALSE);

  /* quit on window close */
  g_signal_connect (G_OBJECT (win), "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  tasklist = wnck_tasklist_new (screen);

  wnck_tasklist_set_include_all_workspaces (WNCK_TASKLIST (tasklist), display_all);
  wnck_tasklist_set_grouping (WNCK_TASKLIST (tasklist), WNCK_TASKLIST_ALWAYS_GROUP);
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (win), frame);

  gtk_container_add (GTK_CONTAINER (frame), tasklist);  

  gtk_widget_show (tasklist);
  gtk_widget_show (frame);

  gtk_window_move (GTK_WINDOW (win), 0, 0);
  
  gtk_widget_show (win);
  
  gtk_main ();
  
  return 0;
}
