
#include <libwnck/libwnck.h>
#include <gtk/gtk.h>

static GtkWidget *global_tree_view;
static GtkTreeModel *global_tree_model;
static guint refill_idle;

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

static GtkTreeModel* create_tree_model (void);
static GtkWidget*    create_tree_view  (void);
static void          refill_tree_model (GtkTreeModel *model,
                                        WnckScreen   *screen);
static void          update_window     (GtkTreeModel *model,
                                        WnckWindow   *window);
static void          queue_refill_model (void);

int
main (int argc, char **argv)
{
  WnckScreen *screen;
  GtkWidget *sw;
  GtkWidget *win;
  
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

  global_tree_model = create_tree_model ();
  global_tree_view = create_tree_view ();
  
  gtk_tree_view_set_model (GTK_TREE_VIEW (global_tree_view),
                           global_tree_model);
  
  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title (GTK_WINDOW (win), "Window List");
  
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  
  gtk_container_add (GTK_CONTAINER (sw), global_tree_view);
  gtk_container_add (GTK_CONTAINER (win), sw);

  gtk_window_set_default_size (GTK_WINDOW (win), 650, 550);

  /* quit on window close */
  g_signal_connect (G_OBJECT (win), "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);
  
  gtk_widget_show_all (win);
  
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
  queue_refill_model ();
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

  queue_refill_model ();
}

static void
window_closed_callback            (WnckScreen    *screen,
                                   WnckWindow    *window,
                                   gpointer       data)
{
  g_print ("Window '%s' closed\n",
           wnck_window_get_name (window));

  queue_refill_model ();
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

  update_window (global_tree_model, window);
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

  update_window (global_tree_model, window);
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

  update_window (global_tree_model, window);
}


static GtkTreeModel*
create_tree_model (void)
{
  GtkListStore *store;
  
  store = gtk_list_store_new (1, WNCK_TYPE_WINDOW);

  return GTK_TREE_MODEL (store);
}

static void
refill_tree_model (GtkTreeModel *model,
                   WnckScreen   *screen)
{
  GList *tmp;

  /* We remove the model right away, since its old contents are probably invalid */
  gtk_tree_view_set_model (GTK_TREE_VIEW (global_tree_view), NULL);
  
  gtk_list_store_clear (GTK_LIST_STORE (model));

  tmp = wnck_screen_get_windows (screen);
  while (tmp != NULL)
    {
      GtkTreeIter iter;
      WnckWindow *window = tmp->data;

      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, window, -1);

      tmp = tmp->next;
    }

  gtk_tree_view_set_model (GTK_TREE_VIEW (global_tree_view), model);
  
  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (global_tree_view));
}

static void
update_window (GtkTreeModel *model,
               WnckWindow   *window)
{
  GtkTreeIter iter;
  GList *windows;
  int i;
  
  /* The trick here is to find the right row, for now we assume
   * the screen and the model are in sync
   */
  windows = wnck_screen_get_windows (wnck_window_get_screen (window));

  i = g_list_index (windows, window);

  g_return_if_fail (i >= 0);

  if (gtk_tree_model_iter_nth_child (model, &iter, NULL, i))
    /* Reset the list store value to trigger a redraw */
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, window, -1);
  else
    g_warning ("Tree model has no row %d", i);
}

static WnckWindow*
get_window (GtkTreeModel *model,
            GtkTreeIter  *iter)
{
  WnckWindow *window;
  
  gtk_tree_model_get (model, iter,
                      0, &window,
                      -1);

  /* we know the model and screen are still holding a reference,
   * so cheat a bit
   */
  g_object_unref (G_OBJECT (window));

  return window;
}

static void
icon_set_func (GtkTreeViewColumn *tree_column,
               GtkCellRenderer   *cell,
               GtkTreeModel      *model,
               GtkTreeIter       *iter,
               gpointer           data)
{
  WnckWindow *window;
  
  window = get_window (model, iter);
  
  g_object_set (GTK_CELL_RENDERER (cell),
                "pixbuf", NULL,
                NULL);
}

static void
title_set_func (GtkTreeViewColumn *tree_column,
                GtkCellRenderer   *cell,
                GtkTreeModel      *model,
                GtkTreeIter       *iter,
                gpointer           data)
{
  WnckWindow *window;

  window = get_window (model, iter);
  
  g_object_set (GTK_CELL_RENDERER (cell),
                "text", wnck_window_get_name (window),
                NULL);
}


static void
shaded_set_func (GtkTreeViewColumn *tree_column,
                 GtkCellRenderer   *cell,
                 GtkTreeModel      *model,
                 GtkTreeIter       *iter,
                 gpointer           data)
{
  WnckWindow *window;

  window = get_window (model, iter);

  gtk_cell_renderer_toggle_set_active (GTK_CELL_RENDERER_TOGGLE (cell),
                                       wnck_window_is_shaded (window));
}

static void
shaded_toggled_callback (GtkCellRendererToggle *cell,
                         char                  *path_string,
                         gpointer               data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (data);
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;
  WnckWindow *window;

  gtk_tree_model_get_iter (model, &iter, path);
  window = get_window (model, &iter);

  if (wnck_window_is_shaded (window))
    wnck_window_unshade (window);
  else
    wnck_window_shade (window);
  
  gtk_tree_path_free (path);
}

static void
minimized_set_func (GtkTreeViewColumn *tree_column,
                    GtkCellRenderer   *cell,
                    GtkTreeModel      *model,
                    GtkTreeIter       *iter,
                    gpointer           data)
{
  WnckWindow *window;

  window = get_window (model, iter);

  gtk_cell_renderer_toggle_set_active (GTK_CELL_RENDERER_TOGGLE (cell),
                                       wnck_window_is_minimized (window));
}

static void
minimized_toggled_callback (GtkCellRendererToggle *cell,
                            char                  *path_string,
                            gpointer               data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (data);
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;
  WnckWindow *window;

  gtk_tree_model_get_iter (model, &iter, path);
  window = get_window (model, &iter);

  if (wnck_window_is_minimized (window))
    wnck_window_unminimize (window);
  else
    wnck_window_minimize (window);
  
  gtk_tree_path_free (path);
}

static GtkWidget*
create_tree_view (void)
{
  GtkWidget *tree_view;
  GtkCellRenderer *cell_renderer;
  GtkTreeViewColumn *column;
  GtkTreeSelection *selection;
  
  tree_view = gtk_tree_view_new ();

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree_view), TRUE);
  
  /* The icon and title are in the same column, so pack
   * two cell renderers into that column
   */
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, "Window");

  cell_renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column,
                                   cell_renderer,
                                   FALSE);
  gtk_tree_view_column_set_cell_data_func (column, cell_renderer,
                                           icon_set_func, NULL, NULL);
  cell_renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column,
                                   cell_renderer,
                                   TRUE);
  gtk_tree_view_column_set_cell_data_func (column, cell_renderer,
                                           title_set_func, NULL, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
                               column);
  
  /* Then create a toggle column, only one renderer in this column
   * so we get to use insert_column convenience function
   */
  cell_renderer = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree_view),
                                              -1, /* append */
                                              "Shaded",
                                              cell_renderer,
                                              shaded_set_func,
                                              NULL,
                                              NULL);
  g_signal_connect (G_OBJECT (cell_renderer), "toggled",
                    G_CALLBACK (shaded_toggled_callback),
                    tree_view);

  /* Minimized check */
  cell_renderer = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree_view),
                                              -1, /* append */
                                              "Minimized",
                                              cell_renderer,
                                              minimized_set_func,
                                              NULL,
                                              NULL);
  g_signal_connect (G_OBJECT (cell_renderer), "toggled",
                    G_CALLBACK (minimized_toggled_callback),
                    tree_view);

  
  /* The selection will track the active window, so we need to
   * handle it with a custom function
   */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  /* FIXME */
  
  return tree_view;
}

static gboolean
do_refill_model (gpointer data)
{
  refill_idle = 0;

  refill_tree_model (global_tree_model,
                     wnck_screen_get (0));

  return FALSE;
}

static void
queue_refill_model (void)
{
  if (refill_idle != 0)
    return;

  g_idle_add_full (G_PRIORITY_LOW, do_refill_model, NULL, NULL);
}
