/* vim: set sw=2 et: */

#include <libwnck/libwnck.h>
#include <gtk/gtk.h>

static int n_rows = 1;
static gboolean only_current = FALSE;
static gboolean rtl = FALSE;
static gboolean show_name = FALSE;
static gboolean simple_scrolling = FALSE;
static gboolean vertical = FALSE;
static gboolean wrap_on_scroll = FALSE;

static GOptionEntry entries[] = {
	{"n-rows", 'n', 0, G_OPTION_ARG_INT, &n_rows, "Use N_ROWS rows", "N_ROWS"},
	{"only-current", 'c', 0, G_OPTION_ARG_NONE, &only_current, "Only show current workspace", NULL},
	{"rtl", 'r', 0, G_OPTION_ARG_NONE, &rtl, "Use RTL as default direction", NULL},
	{"show-name", 's', 0, G_OPTION_ARG_NONE, &show_name, "Show workspace names instead of workspace contents", NULL},
	{"simple-scrolling", 'd', 0, G_OPTION_ARG_NONE, &simple_scrolling, "Use the simple 1d scroll mode", NULL},
	{"vertical-orientation", 'v', 0, G_OPTION_ARG_NONE, &vertical, "Use a vertical orientation", NULL},
	{"wrap-on-scroll", 'w', 0, G_OPTION_ARG_NONE, &wrap_on_scroll, "Wrap on scrolling over borders", NULL},
	{NULL }
};

static void
create_pager_window (WnckHandle           *handle,
                     GtkOrientation        orientation,
                     gboolean              show_all,
                     WnckPagerDisplayMode  mode,
                     WnckPagerScrollMode   scroll_mode,
                     int                   rows,
                     gboolean              wrap)
{
  GtkWidget *win;
  GtkWidget *pager;

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_stick (GTK_WINDOW (win));
#if 0
  wnck_gtk_window_set_dock_type (GTK_WINDOW (win));
#endif

  gtk_window_set_title (GTK_WINDOW (win), "Pager");

  /* very very random */
  gtk_window_move (GTK_WINDOW (win), 0, 0);

  /* quit on window close */
  g_signal_connect (G_OBJECT (win), "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  pager = wnck_pager_new_with_handle (handle);

  wnck_pager_set_show_all (WNCK_PAGER (pager), show_all);
  wnck_pager_set_display_mode (WNCK_PAGER (pager), mode);
  wnck_pager_set_scroll_mode (WNCK_PAGER (pager), scroll_mode);
  wnck_pager_set_orientation (WNCK_PAGER (pager), orientation);
  wnck_pager_set_n_rows (WNCK_PAGER (pager), rows);
  wnck_pager_set_shadow_type (WNCK_PAGER (pager), GTK_SHADOW_IN);
  wnck_pager_set_wrap_on_scroll (WNCK_PAGER (pager), wrap);

  gtk_container_add (GTK_CONTAINER (win), pager);

  gtk_widget_show_all (win);
}

int
main (int argc, char **argv)
{
  GOptionContext *ctxt;
  GtkOrientation  orientation;
  WnckPagerDisplayMode mode;
  WnckPagerScrollMode scroll_mode;
  WnckHandle *handle;
  WnckScreen *screen;

  ctxt = g_option_context_new ("");
  g_option_context_add_main_entries (ctxt, entries, NULL);
  g_option_context_add_group (ctxt, gtk_get_option_group (TRUE));
  g_option_context_parse (ctxt, &argc, &argv, NULL);
  g_option_context_free (ctxt);
  ctxt = NULL;

  gtk_init (&argc, &argv);

  if (rtl)
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  handle = wnck_handle_new (WNCK_CLIENT_TYPE_APPLICATION);
  screen = wnck_handle_get_default_screen (handle);

  /* because the pager doesn't respond to signals at the moment */
  wnck_screen_force_update (screen);

  if (vertical)
	  orientation = GTK_ORIENTATION_VERTICAL;
  else
	  orientation = GTK_ORIENTATION_HORIZONTAL;

  if (show_name)
	  mode = WNCK_PAGER_DISPLAY_NAME;
  else
	  mode = WNCK_PAGER_DISPLAY_CONTENT;

  if (simple_scrolling)
	  scroll_mode = WNCK_PAGER_SCROLL_1D;
  else
	  scroll_mode = WNCK_PAGER_SCROLL_2D;

  create_pager_window (handle, orientation, !only_current, mode, scroll_mode, n_rows, wrap_on_scroll);

  gtk_main ();

  g_object_unref (handle);

  return 0;
}
