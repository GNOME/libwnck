/* screen object */

/*
 * Copyright (C) 2001 Havoc Pennington
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "screen.h"
#include "window.h"
#include "workspace.h"
#include "xutils.h"
#include "private.h"
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

static WnckScreen** screens = NULL;

struct _WnckScreenPrivate
{
  int number;
  Window xroot;
  Screen *xscreen;
  int width;
  int height;
  /* in bottom-to-top order */
  GList *windows;
  /* in 0-to-N order */
  GList *workspaces;

  WnckWindow *active_window;
  WnckWorkspace *active_workspace;

  guint update_handler;

  /* if you add flags, be sure to set them
   * when we create the screen so we get an initial update
   */
  guint need_update_stack_list : 1;
  guint need_update_workspace_list : 1;
  guint need_update_active_workspace : 1;
  guint need_update_active_window : 1;
};

enum {
  ACTIVE_WINDOW_CHANGED,
  ACTIVE_WORKSPACE_CHANGED,
  WINDOW_STACKING_CHANGED,
  WINDOW_OPENED,
  WINDOW_CLOSED,
  WORKSPACE_CREATED,
  WORKSPACE_DESTROYED,
  LAST_SIGNAL
};

static void wnck_screen_init        (WnckScreen      *screen);
static void wnck_screen_class_init  (WnckScreenClass *klass);
static void wnck_screen_finalize    (GObject         *object);

static void update_client_list      (WnckScreen      *screen);
static void update_workspace_list   (WnckScreen      *screen);
static void update_active_workspace (WnckScreen      *screen);
static void update_active_window    (WnckScreen      *screen);

static void queue_update            (WnckScreen      *screen);
static void unqueue_update          (WnckScreen      *screen);              

static void emit_active_window_changed    (WnckScreen *screen);
static void emit_active_workspace_changed (WnckScreen *screen);
static void emit_window_stacking_changed  (WnckScreen *screen);
static void emit_window_opened            (WnckScreen *screen,
                                           WnckWindow *window);
static void emit_window_closed            (WnckScreen *screen,
                                           WnckWindow *window);
static void emit_workspace_created        (WnckScreen *screen,
                                           WnckWorkspace *space);
static void emit_workspace_destroyed      (WnckScreen *screen,
                                           WnckWorkspace *space);

static gpointer parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

GType
wnck_screen_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();
  
  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (WnckScreenClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) wnck_screen_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (WnckScreen),
        0,              /* n_preallocs */
        (GInstanceInitFunc) wnck_screen_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "WnckScreen",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
wnck_screen_init (WnckScreen *screen)
{  
  screen->priv = g_new0 (WnckScreenPrivate, 1);

}

static void
wnck_screen_class_init (WnckScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = wnck_screen_finalize;

  signals[ACTIVE_WINDOW_CHANGED] =
    g_signal_new ("active_window_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, active_window_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[ACTIVE_WORKSPACE_CHANGED] =
    g_signal_new ("active_workspace_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, active_workspace_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  
  signals[WINDOW_STACKING_CHANGED] =
    g_signal_new ("window_stacking_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, window_stacking_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[WINDOW_OPENED] =
    g_signal_new ("window_opened",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, window_opened),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, WNCK_TYPE_WINDOW);

  signals[WINDOW_CLOSED] =
    g_signal_new ("window_closed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, window_closed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, WNCK_TYPE_WINDOW);

  
  signals[WORKSPACE_CREATED] =
    g_signal_new ("workspace_created",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, workspace_created),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, WNCK_TYPE_WORKSPACE);
  
  signals[WORKSPACE_DESTROYED] =
    g_signal_new ("workspace_destroyed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckScreenClass, workspace_destroyed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, WNCK_TYPE_WORKSPACE);
}

static void
wnck_screen_finalize (GObject *object)
{
  WnckScreen *screen;

  screen = WNCK_SCREEN (object);

  unqueue_update (screen);
  
  g_free (screen->priv);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
wnck_screen_construct (WnckScreen *screen,
                       int         number)
{
  /* Create the initial state of the screen. */
  screen->priv->xroot = RootWindow (gdk_display, number);
  screen->priv->xscreen = ScreenOfDisplay (gdk_display, number);
  screen->priv->number = number;
  
  _wnck_error_trap_push ();
  XSelectInput (gdk_display,
                screen->priv->xroot,
                PropertyChangeMask);
  _wnck_error_trap_pop ();

  screen->priv->width = WidthOfScreen (screen->priv->xscreen);
  screen->priv->height = HeightOfScreen (screen->priv->xscreen);

  screen->priv->need_update_workspace_list = TRUE;
  screen->priv->need_update_stack_list = TRUE;
  screen->priv->need_update_active_workspace = TRUE;
  screen->priv->need_update_active_window = TRUE;
  
  queue_update (screen);
}

/**
 * wnck_screen_get:
 * @index: screen number, starting from 0, as for Xlib
 * 
 * Gets the #WnckScreen for a given screen on the default display.
 * Creates the #WnckScreen if necessary.
 * 
 * Return value: the #WnckScreen for screen @index
 **/
WnckScreen*
wnck_screen_get (int index)
{
  g_return_val_if_fail (gdk_display != NULL, NULL);
  g_return_val_if_fail (index < ScreenCount (gdk_display), NULL);
  
  if (screens == NULL)
    {
      screens = g_new0 (WnckScreen*, ScreenCount (gdk_display));
      _wnck_event_filter_init ();
    }
  
  if (screens[index] == NULL)
    {
      screens[index] = g_object_new (WNCK_TYPE_SCREEN, NULL);
      
      wnck_screen_construct (screens[index], index);
    }

  return screens[index];
}

/**
 * wnck_screen_get_for_root:
 * @root_window_id: an Xlib window ID
 * 
 * Gets the #WnckScreen for the root window at @root_window_id, or
 * returns %NULL if no #WnckScreen exists for this root window. Never
 * creates a new #WnckScreen, unlike wnck_screen_get().
 * 
 * Return value: #WnckScreen or %NULL
 **/
WnckScreen*
wnck_screen_get_for_root (gulong root_window_id)
{
  int i;
  
  if (screens == NULL)
    return NULL;

  i = 0;
  while (i < ScreenCount (gdk_display))
    {
      if (screens[i]->priv->xroot == root_window_id)
        return screens[i];
      
      ++i;
    }

  return NULL;
}

void
_wnck_screen_process_property_notify (WnckScreen *screen,
                                      XEvent     *xevent)
{
  /* most frequently-changed properties first */
  if (xevent->xproperty.atom ==
      _wnck_atom_get ("_NET_ACTIVE_WINDOW"))
    {
      screen->priv->need_update_active_window = TRUE;
      queue_update (screen);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_CURRENT_DESKTOP"))
    {
      screen->priv->need_update_active_workspace = TRUE;
      queue_update (screen);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_CLIENT_LIST_STACKING"))
    {
      screen->priv->need_update_stack_list = TRUE;
      queue_update (screen);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_NUMBER_OF_DESKTOPS"))
    {
      screen->priv->need_update_workspace_list = TRUE;
      queue_update (screen);
    }
}

static gboolean
lists_equal (GList *a,
             GList *b)
{
  GList *a_iter;
  GList *b_iter;

  a_iter = a;
  b_iter = b;

  while (a_iter && b_iter)
    {
      if (a_iter->data != b_iter->data)
        return FALSE;
      
      a_iter = a_iter->next;
      b_iter = b_iter->next;
    }

  if (a_iter || b_iter)
    return FALSE;

  return TRUE;
}

static void
update_client_list (WnckScreen *screen)
{
  Window *stack = NULL;
  int stack_length = 0;
  GList *new_list;
  GList *created;
  GList *closed;
  GList *tmp;
  int i;
  GHashTable *new_hash;
  static int reentrancy_guard = 0;

  g_return_if_fail (reentrancy_guard == 0);
  
  if (!screen->priv->need_update_stack_list)
    return;

  ++reentrancy_guard;
  
  screen->priv->need_update_stack_list = FALSE;
  
  _wnck_get_window_list (screen->priv->xroot,
                         _wnck_atom_get ("_NET_CLIENT_LIST_STACKING"),
                         &stack,
                         &stack_length);

  created = NULL;
  closed = NULL;

  new_hash = g_hash_table_new (NULL, NULL);
  
  new_list = NULL;
  i = 0;
  while (i < stack_length)
    {
      WnckWindow *window;

      window = wnck_window_get (stack[i]);

      if (window == NULL)
        {
          /* create gives us a reference to own */
          window = _wnck_window_create (stack[i]);
          created = g_list_prepend (created, window);
        }

      new_list = g_list_prepend (new_list, window);

      g_hash_table_insert (new_hash, window, window);
      
      ++i;
    }
      
  /* put list back in order */
  new_list = g_list_reverse (new_list);

  /* Now we need to find windows in the old list that aren't
   * in this new list
   */
  tmp = screen->priv->windows;
  while (tmp != NULL)
    {
      WnckWindow *window = tmp->data;

      if (g_hash_table_lookup (new_hash, window) == NULL)
        closed = g_list_prepend (closed, window);
      
      tmp = tmp->next;
    }

  g_hash_table_destroy (new_hash);

  /* Now new_list becomes screen->priv->windows,
   * and we emit the opened/closed signals
   */
  if (lists_equal (screen->priv->windows, new_list))
    {
      g_assert (created == NULL);
      g_assert (closed == NULL);
      g_list_free (new_list);
      --reentrancy_guard;
      return;
    }

  g_list_free (screen->priv->windows);
  screen->priv->windows = new_list;

  /* Here we could get reentrancy if someone ran the main loop in
   * signal callbacks; though that would be a bit pathological, so we
   * don't handle it, but we do warn about it using reentrancy_guard
   */
  
  tmp = created;
  while (tmp != NULL)
    {
      emit_window_opened (screen, WNCK_WINDOW (tmp->data));
      
      tmp = tmp->next;
    }

  tmp = closed;
  while (tmp != NULL)
    {
      WnckWindow *window;

      window = WNCK_WINDOW (tmp->data);

      if (window == screen->priv->active_window)
        {
          screen->priv->active_window = NULL;
          emit_active_window_changed (screen);
        }
      
      emit_window_closed (screen, window);
      
      tmp = tmp->next;
    }

  emit_window_stacking_changed (screen);

  /* Now free the closed windows */
  tmp = closed;
  while (tmp != NULL)
    {
      WnckWindow *window = tmp->data;

      _wnck_window_destroy (window);
      
      tmp = tmp->next;
    }

  g_list_free (closed);
  g_list_free (created);

  --reentrancy_guard;

  /* Maybe the active window is now valid if it wasn't */
  if (screen->priv->active_window == NULL)
    {
      screen->priv->need_update_active_window = TRUE;
      queue_update (screen);
    }
}

static void
update_workspace_list (WnckScreen *screen)
{
  int n_spaces;
  int old_n_spaces;
  GList *tmp;
  GList *deleted;
  GList *created;
  static int reentrancy_guard = 0;

  g_return_if_fail (reentrancy_guard == 0);
  
  if (!screen->priv->need_update_workspace_list)
    return;
  
  screen->priv->need_update_workspace_list = FALSE;

  ++reentrancy_guard;
  
  n_spaces = 0;
  _wnck_get_cardinal (screen->priv->xroot,
                      _wnck_atom_get ("_NET_NUMBER_OF_DESKTOPS"),
                      &n_spaces);
  
  old_n_spaces = g_list_length (screen->priv->workspaces);

  deleted = NULL;
  created = NULL;
  
  if (old_n_spaces == n_spaces)
    {
      --reentrancy_guard;
      return; /* nothing changed */
    }
  else if (old_n_spaces > n_spaces)
    {
      /* Need to delete some workspaces */
      deleted = g_list_nth (screen->priv->workspaces, n_spaces);
      if (deleted->prev)
        deleted->prev->next = NULL;
      deleted->prev = NULL;

      if (deleted == screen->priv->workspaces)
        screen->priv->workspaces = NULL;
    }
  else
    {
      int i;
      
      g_assert (old_n_spaces < n_spaces);

      /* Need to create some workspaces */
      i = 0;
      while (i < (n_spaces - old_n_spaces))
        {
          WnckWorkspace *space;

          space = _wnck_workspace_create (old_n_spaces + i);

          screen->priv->workspaces = g_list_append (screen->priv->workspaces,
                                                    space);

          created = g_list_prepend (created, space);
          
          ++i;
        }

      created = g_list_reverse (created);
    }

  /* Here we allow reentrancy, going into the main
   * loop could confuse us
   */
  tmp = deleted;
  while (tmp != NULL)
    {
      WnckWorkspace *space = WNCK_WORKSPACE (tmp->data);

      if (space == screen->priv->active_workspace)
        {
          screen->priv->active_workspace = NULL;
          emit_active_workspace_changed (screen);
        }
      
      emit_workspace_destroyed (screen, space);
      
      tmp = tmp->next;
    }

  tmp = created;
  while (tmp != NULL)
    {
      emit_workspace_created (screen, WNCK_WORKSPACE (tmp->data));
      
      tmp = tmp->next;
    }

  tmp = deleted;
  while (tmp != NULL)
    {
      _wnck_workspace_destroy (WNCK_WORKSPACE (tmp->data));
      
      tmp = tmp->next;
    }

  /* Active workspace property may now be interpretable,
   * if it was a number larger than the active count previously
   */
  if (screen->priv->active_workspace == NULL)
    {
      screen->priv->need_update_active_workspace = TRUE;
      queue_update (screen);
    }
  
  --reentrancy_guard;
}

static void
update_active_workspace (WnckScreen *screen)
{
  int number;
  WnckWorkspace *space;
  
  if (!screen->priv->need_update_active_workspace)
    return;

  screen->priv->need_update_active_workspace = FALSE;

  number = 0;
  if (!_wnck_get_cardinal (screen->priv->xroot,
                           _wnck_atom_get ("_NET_CURRENT_DESKTOP"),
                           &number))
    number = -1;
  
  space = wnck_workspace_get (number);

  if (space == screen->priv->active_workspace)
    return;
  
  screen->priv->active_workspace = space;

  emit_active_workspace_changed (screen);
}

static void
update_active_window (WnckScreen *screen)
{
  WnckWindow *window;
  Window xwindow;
  
  if (!screen->priv->need_update_active_window)
    return;

  screen->priv->need_update_active_window = FALSE;
  
  xwindow = None;
  _wnck_get_window (screen->priv->xroot,
                    _wnck_atom_get ("_NET_ACTIVE_WINDOW"),
                    &xwindow);

  window = wnck_window_get (xwindow);

  if (window == screen->priv->active_window)
    return;

  screen->priv->active_window = window;

  emit_active_window_changed (screen);
}

static gboolean
update_idle (gpointer data)
{
  WnckScreen *screen;

  screen = data;

  screen->priv->update_handler = 0;

  /* First get our big-picture state in order */
  update_workspace_list (screen);
  update_client_list (screen);

  /* Then note any smaller-scale changes */
  update_active_window (screen);
  update_active_workspace (screen);
  
  return FALSE;
}

static void
queue_update (WnckScreen *screen)
{
  if (screen->priv->update_handler != 0)
    return;

  screen->priv->update_handler = g_idle_add (update_idle, screen);
}

static void
unqueue_update (WnckScreen *screen)
{
  if (screen->priv->update_handler != 0)
    {
      g_source_remove (screen->priv->update_handler);
      screen->priv->update_handler = 0;
    }
}

static void
emit_active_window_changed (WnckScreen *screen)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[ACTIVE_WINDOW_CHANGED],
                 0);
}

static void
emit_active_workspace_changed (WnckScreen *screen)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[ACTIVE_WORKSPACE_CHANGED],
                 0);
}

static void
emit_window_stacking_changed (WnckScreen *screen)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[WINDOW_STACKING_CHANGED],
                 0);
}

static void
emit_window_opened (WnckScreen *screen,
                    WnckWindow *window)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[WINDOW_OPENED],
                 0, window);  
}

static void
emit_window_closed (WnckScreen *screen,
                    WnckWindow *window)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[WINDOW_CLOSED],
                 0, window);
}

static void
emit_workspace_created (WnckScreen    *screen,
                        WnckWorkspace *space)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[WORKSPACE_CREATED],
                 0, space);  
}

static void
emit_workspace_destroyed (WnckScreen    *screen,
                          WnckWorkspace *space)
{
  g_signal_emit (G_OBJECT (screen),
                 signals[WORKSPACE_DESTROYED],
                 0, space);
}


