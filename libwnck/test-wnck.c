
#include <libwnck/libwnck.h>
#include <gtk/gtk.h>
static void active_window_changed_callback    (WnckScreen      *screen,
                                               gpointer         data);
static void active_workspace_changed_callback (WnckScreen      *screen,
                                               gpointer         data);
static void window_stacking_changed_callback  (WnckScreen      *screen,
                                               gpointer         data);
static void window_opened_callback            (WnckScreen      *screen,
                                               WnckWindow      *window,
                                               gpointer         data);
static void window_closed_callback            (WnckScreen      *screen,
                                               WnckWindow      *window,
                                               gpointer         data);
static void workspace_created_callback        (WnckScreen      *screen,
                                               WnckWorkspace   *space,
                                               gpointer         data);
static void workspace_destroyed_callback      (WnckScreen      *screen,
                                               WnckWorkspace   *space,
                                               gpointer         data);
static void application_opened_callback       (WnckScreen      *screen,
                                               WnckApplication *app);
static void application_closed_callback       (WnckScreen      *screen,
                                               WnckApplication *app);
static void window_name_changed_callback      (WnckWindow      *window,
                                               gpointer         data);
static void window_state_changed_callback     (WnckWindow      *window,
                                               WnckWindowState  changed,
                                               WnckWindowState  new,
                                               gpointer         data);
static void window_workspace_changed_callback (WnckWindow      *window,
                                               gpointer         data);



int
main (int argc, char **argv)
{
  WnckScreen *screen;
  
  gtk_init (&argc, &argv);

  screen = wnck_screen_get (0);

  g_signal_connect (G_OBJECT (screen), "active_window_changed",
                    G_CALLBACK (active_window_changed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "active_workspace_changed",
                    G_CALLBACK (active_workspace_changed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "window_stacking_changed",
                    G_CALLBACK (window_stacking_changed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "window_opened",
                    G_CALLBACK (window_opened_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "window_closed",
                    G_CALLBACK (window_closed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "workspace_created",
                    G_CALLBACK (workspace_created_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "workspace_destroyed",
                    G_CALLBACK (workspace_destroyed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "application_opened",
                    G_CALLBACK (application_opened_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "application_closed",
                    G_CALLBACK (application_closed_callback),
                    NULL);

  gtk_main ();
  
  return 0;
}

static void
active_window_changed_callback    (WnckScreen    *screen,
                                   gpointer       data)
{
  g_print ("Active window changed\n");
}

static void
active_workspace_changed_callback (WnckScreen    *screen,
                                   gpointer       data)
{
  g_print ("Active workspace changed\n");
}

static void
window_stacking_changed_callback  (WnckScreen    *screen,
                                   gpointer       data)
{
  g_print ("Stacking changed\n");
}

static void
window_opened_callback            (WnckScreen    *screen,
                                   WnckWindow    *window,
                                   gpointer       data)
{
  g_print ("Window '%s' opened (pid = %d session_id = %s)\n",
           wnck_window_get_name (window),
           wnck_window_get_pid (window),
           wnck_window_get_session_id (window) ?
           wnck_window_get_session_id (window) : "none");
  
  g_signal_connect (G_OBJECT (window), "name_changed",
                    G_CALLBACK (window_name_changed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (window), "state_changed",
                    G_CALLBACK (window_state_changed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (window), "workspace_changed",
                    G_CALLBACK (window_workspace_changed_callback),
                    NULL);
}

static void
window_closed_callback            (WnckScreen    *screen,
                                   WnckWindow    *window,
                                   gpointer       data)
{
  g_print ("Window '%s' closed\n",
           wnck_window_get_name (window));
}

static void
workspace_created_callback        (WnckScreen    *screen,
                                   WnckWorkspace *space,
                                   gpointer       data)
{
  g_print ("Workspace created\n");
}

static void
workspace_destroyed_callback      (WnckScreen    *screen,
                                   WnckWorkspace *space,
                                   gpointer       data)
{
  g_print ("Workspace destroyed\n");
}

static void
application_opened_callback (WnckScreen      *screen,
                             WnckApplication *app)
{
  g_print ("Application opened\n");
}

static void
application_closed_callback (WnckScreen      *screen,
                             WnckApplication *app)
{
  g_print ("Application closed\n");
}

static void
window_name_changed_callback (WnckWindow    *window,
                              gpointer       data)
{
  g_print ("Name changed on window '%s'\n",
           wnck_window_get_name (window));
}

static void
window_state_changed_callback (WnckWindow     *window,
                               WnckWindowState changed,
                               WnckWindowState new,
                               gpointer        data)
{
  g_print ("State changed on window '%s'\n",
           wnck_window_get_name (window));

  if (changed & WNCK_WINDOW_STATE_MINIMIZED)
    g_print (" minimized = %d\n", wnck_window_is_minimized (window));

  if (changed & WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY)
    g_print (" maximized horiz = %d\n", wnck_window_is_maximized_horizontally (window));

  if (changed & WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY)
    g_print (" maximized vert = %d\n", wnck_window_is_maximized_vertically (window));

  if (changed & WNCK_WINDOW_STATE_SHADED)
    g_print (" shaded = %d\n", wnck_window_is_shaded (window));

  if (changed & WNCK_WINDOW_STATE_SKIP_PAGER)
    g_print (" skip pager = %d\n", wnck_window_is_skip_pager (window));

  if (changed & WNCK_WINDOW_STATE_SKIP_TASKLIST)
    g_print (" skip tasklist = %d\n", wnck_window_is_skip_tasklist (window));

  if (changed & WNCK_WINDOW_STATE_STICKY)
    g_print (" sticky = %d\n", wnck_window_is_sticky (window));

  g_assert ( ((new & WNCK_WINDOW_STATE_MINIMIZED) != 0) ==
             wnck_window_is_minimized (window) );
  g_assert ( ((new & WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY) != 0) ==
             wnck_window_is_maximized_horizontally (window) );
  g_assert ( ((new & WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY) != 0) ==
             wnck_window_is_maximized_vertically (window) );
  g_assert ( ((new & WNCK_WINDOW_STATE_SHADED) != 0) ==
             wnck_window_is_shaded (window) );
  g_assert ( ((new & WNCK_WINDOW_STATE_SKIP_PAGER) != 0) ==
             wnck_window_is_skip_pager (window) );
  g_assert ( ((new & WNCK_WINDOW_STATE_SKIP_TASKLIST) != 0) ==
             wnck_window_is_skip_tasklist (window) );
  g_assert ( ((new & WNCK_WINDOW_STATE_STICKY) != 0) ==
             wnck_window_is_sticky (window) );
}

static void
window_workspace_changed_callback (WnckWindow    *window,
                                   gpointer       data)
{
  WnckWorkspace *space;

  space = wnck_window_get_workspace (window);

  if (space)
    g_print ("Workspace changed on window '%s' to %d\n",
             wnck_window_get_name (window),
             wnck_workspace_get_number (space));
  else
    g_print ("Window '%s' is now pinned to all workspaces\n",
             wnck_window_get_name (window));
}

