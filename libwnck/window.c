/* window object */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Kim Woelders
 * Copyright (C) 2003 Red Hat, Inc.
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

#include <string.h>
#include "window.h"
#include "xutils.h"
#include "private.h"
#include "wnck-enum-types.h"
#include "wnck-marshal.h"

#define FALLBACK_NAME _("untitled window")
#define ALL_WORKSPACES (0xFFFFFFFF)

static GHashTable *window_hash = NULL;

/* Keep 0-7 in sync with the numbers in the WindowState enum. Yeah I'm
 * a loser.
 */
#define COMPRESS_STATE(window)                          \
  ( ((window)->priv->is_minimized        << 0) |        \
    ((window)->priv->is_maximized_horz   << 1) |        \
    ((window)->priv->is_maximized_vert   << 2) |        \
    ((window)->priv->is_shaded           << 3) |        \
    ((window)->priv->skip_pager          << 4) |        \
    ((window)->priv->skip_taskbar        << 5) |        \
    ((window)->priv->is_sticky           << 6) |        \
    ((window)->priv->is_hidden           << 7) |        \
    ((window)->priv->is_fullscreen       << 8) )

struct _WnckWindowPrivate
{
  Window xwindow;
  WnckScreen *screen;
  WnckApplication *app;
  Window group_leader;
  Window transient_for;
  char *name;
  char *icon_name;
  char *session_id;
  char *session_id_utf8;
  int pid;
  int workspace;

  WnckWindowType wintype;
  
  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;

  WnckIconCache *icon_cache;
  
  WnckWindowActions actions;

  int x;
  int y;
  int width;
  int height;

  char *startup_id;

  char *res_class;
  char *res_name;
  
  /* true if transient_for points to root window,
   * not another app window
   */
  guint transient_for_root : 1;
  
  /* window state */
  guint is_minimized : 1;
  guint is_maximized_horz : 1;
  guint is_maximized_vert : 1;
  guint is_shaded : 1;
  guint skip_pager : 1;
  guint skip_taskbar : 1;
  guint is_sticky : 1;
  guint is_hidden : 1;
  guint is_fullscreen : 1;

  /* _NET_WM_STATE_HIDDEN doesn't map directly into an
   * externally-visible state (it determines the WM_STATE
   * interpretation)
   */
  guint net_wm_state_hidden : 1;  
  guint wm_state_iconic : 1;
  
  /* idle handler for updates */
  guint update_handler;

  /* if you add flags, be sure to set them
   * when we create the window so we get an initial update
   */
  guint need_update_name : 1;
  guint need_update_state : 1;
  guint need_update_wm_state : 1;
  guint need_update_icon_name : 1;
  guint need_update_workspace : 1;
  guint need_emit_icon_changed : 1;
  guint need_update_actions : 1;
  guint need_update_wintype : 1;
  guint need_update_transient_for : 1;
  guint need_update_startup_id : 1;
  guint need_update_wmclass : 1;
};

enum {
  NAME_CHANGED,
  STATE_CHANGED,
  WORKSPACE_CHANGED,
  ICON_CHANGED,
  ACTIONS_CHANGED,
  GEOMETRY_CHANGED,
  LAST_SIGNAL
};

static void wnck_window_init        (WnckWindow      *window);
static void wnck_window_class_init  (WnckWindowClass *klass);
static void wnck_window_finalize    (GObject        *object);

static void emit_name_changed      (WnckWindow      *window);
static void emit_state_changed     (WnckWindow      *window,
                                    WnckWindowState  changed_mask,
                                    WnckWindowState  new_state);
static void emit_workspace_changed (WnckWindow      *window);
static void emit_icon_changed      (WnckWindow      *window);
static void emit_actions_changed   (WnckWindow       *window,
                                    WnckWindowActions changed_mask,
                                    WnckWindowActions new_actions);
static void emit_geometry_changed  (WnckWindow      *window);

static void update_name      (WnckWindow *window);
static void update_state     (WnckWindow *window);
static void update_wm_state  (WnckWindow *window);
static void update_icon_name (WnckWindow *window);
static void update_workspace (WnckWindow *window);
static void update_actions   (WnckWindow *window);
static void update_wintype   (WnckWindow *window);
static void update_transient_for (WnckWindow *window);
static void update_startup_id (WnckWindow *window);
static void update_wmclass    (WnckWindow *window);
static void unqueue_update   (WnckWindow *window);
static void queue_update     (WnckWindow *window);
static void force_update_now (WnckWindow *window);

static gpointer parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

GType
wnck_window_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (WnckWindowClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) wnck_window_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (WnckWindow),
        0,              /* n_preallocs */
        (GInstanceInitFunc) wnck_window_init,
      };

      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "WnckWindow",
                                            &object_info, 0);
    }

  return object_type;
}

static void
wnck_window_init (WnckWindow *window)
{
  window->priv = g_new0 (WnckWindowPrivate, 1);

  window->priv->name = g_strdup (FALLBACK_NAME);
  window->priv->icon_name = NULL;
  window->priv->workspace = ALL_WORKSPACES;

  window->priv->icon_cache = _wnck_icon_cache_new ();
}

static void
wnck_window_class_init (WnckWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = wnck_window_finalize;

  signals[NAME_CHANGED] =
    g_signal_new ("name_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckWindowClass, name_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[STATE_CHANGED] =
    g_signal_new ("state_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckWindowClass, state_changed),
                  NULL, NULL,
                  _wnck_marshal_VOID__FLAGS_FLAGS,
                  G_TYPE_NONE, 2,
                  WNCK_TYPE_WINDOW_STATE, WNCK_TYPE_WINDOW_STATE);

  signals[WORKSPACE_CHANGED] =
    g_signal_new ("workspace_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckWindowClass, workspace_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[ICON_CHANGED] =
    g_signal_new ("icon_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckWindowClass, icon_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[ACTIONS_CHANGED] =
    g_signal_new ("actions_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckWindowClass, actions_changed),
                  NULL, NULL,
                  _wnck_marshal_VOID__FLAGS_FLAGS,
                  G_TYPE_NONE, 2,
                  WNCK_TYPE_WINDOW_ACTIONS,
                  WNCK_TYPE_WINDOW_ACTIONS);

  signals[GEOMETRY_CHANGED] =
    g_signal_new ("geometry_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckWindowClass, geometry_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);  
}

static void
wnck_window_finalize (GObject *object)
{
  WnckWindow *window;

  window = WNCK_WINDOW (object);

  unqueue_update (window);

  if (window->priv->icon)
    g_object_unref (G_OBJECT (window->priv->icon));

  if (window->priv->mini_icon)
    g_object_unref (G_OBJECT (window->priv->mini_icon));
  
  _wnck_icon_cache_free (window->priv->icon_cache);

  g_free (window->priv->name);
  g_free (window->priv->icon_name);
  g_free (window->priv->session_id);
  g_free (window->priv->startup_id);
  g_free (window->priv->res_class);
  g_free (window->priv->res_name);
  
  if (window->priv->app)
    g_object_unref (G_OBJECT (window->priv->app));

  g_free (window->priv);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * wnck_window_get:
 * @xwindow: an Xlib window ID
 *
 * Gets a preexisting #WnckWindow for the X window @xwindow.
 * Will not create a #WnckWindow if none exists. Robust
 * against bogus window IDs.
 *
 * Return value: the #WnckWindow for this X window
 **/
WnckWindow*
wnck_window_get (gulong xwindow)
{
  if (window_hash == NULL)
    return NULL;
  else
    return g_hash_table_lookup (window_hash, &xwindow);
}

/**
 * wnck_window_get_screen:
 * @window: a #WnckWindow
 *
 * Gets the screen this window is on.
 *
 * Return value: a #WnckScreen
 **/
WnckScreen*
wnck_window_get_screen (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);

  return window->priv->screen;
}

WnckWindow*
_wnck_window_create (Window      xwindow,
                     WnckScreen *screen)
{
  WnckWindow *window;

  if (window_hash == NULL)
    window_hash = g_hash_table_new (_wnck_xid_hash, _wnck_xid_equal);

  g_return_val_if_fail (g_hash_table_lookup (window_hash, &xwindow) == NULL,
                        NULL);

  window = g_object_new (WNCK_TYPE_WINDOW, NULL);
  window->priv->xwindow = xwindow;
  window->priv->screen = screen;

  g_hash_table_insert (window_hash, &window->priv->xwindow, window);

  /* Hash now owns one ref, caller gets none */

  /* Note that xwindow may correspond to a WnckApplication's xwindow,
   * that's why we select the union of the mask we want for Application
   * and the one we want for window
   */
  _wnck_select_input (window->priv->xwindow,
                      WNCK_APP_WINDOW_EVENT_MASK);

  window->priv->group_leader =
    _wnck_get_group_leader (window->priv->xwindow);

  window->priv->session_id =
    _wnck_get_session_id (window->priv->xwindow);

  window->priv->pid =
    _wnck_get_pid (window->priv->xwindow);

  _wnck_get_window_geometry (WNCK_SCREEN_XSCREEN (window->priv->screen),
			     xwindow,
                             &window->priv->x,
                             &window->priv->y,
                             &window->priv->width,
                             &window->priv->height);
  
  window->priv->need_update_name = TRUE;
  window->priv->need_update_state = TRUE;
  window->priv->need_update_icon_name = TRUE;
  window->priv->need_update_wm_state = TRUE;
  window->priv->need_update_workspace = TRUE;
  window->priv->need_update_actions = TRUE;
  window->priv->need_update_wintype = TRUE;
  window->priv->need_update_transient_for = TRUE;
  window->priv->need_update_startup_id = TRUE;
  window->priv->need_update_wmclass = TRUE;
  force_update_now (window);

  return window;
}

void
_wnck_window_destroy (WnckWindow *window)
{
  g_return_if_fail (wnck_window_get (window->priv->xwindow) == window);

  g_hash_table_remove (window_hash, &window->priv->xwindow);

  g_return_if_fail (wnck_window_get (window->priv->xwindow) == NULL);

  window->priv->xwindow = None;

  /* remove hash's ref on the window */
  g_object_unref (G_OBJECT (window));
}

/**
 * wnck_window_get_name:
 * @window: a #WnckWindow
 *
 * Gets the name of the window, as it should be displayed in a pager
 * or tasklist. Always returns some value, even if the window
 * hasn't set a name.
 *
 * For icons titles, use wnck_window_get_icon_name() instead.
 *
 * Return value: name of the window
 **/
const char*
wnck_window_get_name (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);

  return window->priv->name;
}

/**
 * wnck_window_get_icon_name:
 * @window: a #WnckWindow
 *
 * Gets the name of the window, as it should be displayed for an
 * icon. Always returns some value, even if the window hasn't set a
 * name. Contrast with wnck_window_get_name(), which returns the
 * window title, not the icon title.
 *
 * Return value: name of the window
 **/
const char*
wnck_window_get_icon_name (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);

  if (window->priv->icon_name)
    return window->priv->icon_name;
  else
    return window->priv->name;
}

WnckApplication*
wnck_window_get_application  (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);

  return window->priv->app;
}

gulong
wnck_window_get_group_leader (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), None);

  return window->priv->group_leader;
}

gulong
wnck_window_get_xid (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), None);

  return window->priv->xwindow;
}

/**
 * wnck_window_get_session_id:
 * @window: a #WnckWindow
 *
 * Gets the session ID for @window in Latin-1 encoding.
 * NOTE: this is invalid UTF-8. You can't display this
 * string in a GTK widget without converting to UTF-8.
 * See wnck_window_get_session_id_utf8().
 *
 * Return value: session ID in Latin-1
 **/
const char*
wnck_window_get_session_id (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);

  return window->priv->session_id;
}

const char*
wnck_window_get_session_id_utf8 (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);

  if (window->priv->session_id_utf8 == NULL &&
      window->priv->session_id != NULL)
    {
      GString *str;
      char *p;

      str = g_string_new ("");

      p = window->priv->session_id;
      while (*p)
        {
          g_string_append_unichar (str, g_utf8_get_char (p));
          p = g_utf8_next_char (p);
        }

      window->priv->session_id_utf8 = g_string_free (str, FALSE);
    }

  return window->priv->session_id_utf8;
}

/**
 * wnck_window_get_pid:
 * @window: a #WnckWindow
 *
 * Gets the process ID of @window, if available.
 *
 * Return value: PID of @window, or 0 if none available
 **/
int
wnck_window_get_pid (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), 0);

  return window->priv->pid;
}

/**
 * wnck_window_get_window_type:
 * @window: a #WnckWindow
 * 
 * Retrieves the semantic type of the window.
 * 
 * Return value: semantic type
 **/
WnckWindowType
wnck_window_get_window_type (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), 0);
  
  return window->priv->wintype;
}

/**
 * wnck_window_is_minimized:
 * @window: a #WnckWindow
 *
 * If the window is minimized returns %TRUE. Minimization state
 * may change anytime a state_changed signal gets emitted.
 *
 * Return value: %TRUE if window is minimized
 **/
gboolean
wnck_window_is_minimized (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);

  return window->priv->is_minimized;
}

gboolean
wnck_window_is_maximized_horizontally (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);

  return window->priv->is_maximized_horz;
}

gboolean
wnck_window_is_maximized_vertically   (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);

  return window->priv->is_maximized_vert;
}

const char*
_wnck_window_get_startup_id (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);
  
  if (window->priv->startup_id == NULL &&
      window->priv->group_leader != None)
    {
      WnckApplication *app;

      /* Fall back to group leader property */
      
      app = wnck_application_get (window->priv->group_leader);
      
      if (app != NULL)
        return wnck_application_get_startup_id (app);
      else
        return NULL;
    }

  return window->priv->startup_id;
}

const char*
_wnck_window_get_resource_class (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);

  return window->priv->res_class;
}

const char*
_wnck_window_get_resource_name (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);

  return window->priv->res_name;
}

/**
 * wnck_window_is_maximized:
 * @window: a #WnckWindow
 *
 * As for GDK, "maximized" means both vertically and horizontally.
 * If only one, then the window isn't considered maximized.
 *
 * Return value: %TRUE if the window is maximized in both directions
 **/
gboolean
wnck_window_is_maximized (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);

  return
    window->priv->is_maximized_horz &&
    window->priv->is_maximized_vert;
}

gboolean
wnck_window_is_shaded                 (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);

  return window->priv->is_shaded;
}

gboolean
wnck_window_is_skip_pager             (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);

  return window->priv->skip_pager;
}

void
wnck_window_set_skip_pager (WnckWindow *window,
                            gboolean skip)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));
  _wnck_change_state (WNCK_SCREEN_XSCREEN (window->priv->screen),
		      window->priv->xwindow,
                      skip,
                      _wnck_atom_get ("_NET_WM_STATE_SKIP_PAGER"),
                      0);
}

gboolean
wnck_window_is_skip_tasklist          (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);

  return window->priv->skip_taskbar;
}

gboolean
wnck_window_is_fullscreen                 (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);

  return window->priv->is_fullscreen;
}

void
wnck_window_set_skip_tasklist (WnckWindow *window,
                               gboolean skip)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));
  _wnck_change_state (WNCK_SCREEN_XSCREEN (window->priv->screen),
		      window->priv->xwindow,
                      skip,
                      _wnck_atom_get ("_NET_WM_STATE_SKIP_TASKBAR"),
                      0);
}

void
wnck_window_set_fullscreen (WnckWindow *window,
                               gboolean fullscreen)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));
  _wnck_change_state (WNCK_SCREEN_XSCREEN (window->priv->screen),
		      window->priv->xwindow,
                      fullscreen,
                      _wnck_atom_get ("_NET_WM_STATE_FULLSCREEN"),
                      0);
}

/**
 * wnck_window_is_sticky:
 * @window: a #WnckWindow
 *
 * Sticky here means "stuck to the glass," i.e. does not scroll with
 * the viewport. In GDK/GTK,
 * e.g. gdk_window_stick()/gtk_window_stick(), sticky means stuck to
 * the glass and _also_ that the window is on all workspaces.
 * But here it only means the viewport aspect of it.
 *
 * Return value: %TRUE if the window is "stuck to the glass"
 **/
gboolean
wnck_window_is_sticky                 (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);

  return window->priv->is_sticky;
}

void
wnck_window_close (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_close (WNCK_SCREEN_XSCREEN (window->priv->screen),
	       window->priv->xwindow);
}

void
wnck_window_minimize                (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_iconify (window->priv->xwindow);
}

void
wnck_window_unminimize              (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_deiconify (window->priv->xwindow);
}

void
wnck_window_maximize                (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_change_state (WNCK_SCREEN_XSCREEN (window->priv->screen),
		      window->priv->xwindow,
                      TRUE,
                      _wnck_atom_get ("_NET_WM_STATE_MAXIMIZED_VERT"),
                      _wnck_atom_get ("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

void
wnck_window_unmaximize              (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_change_state (WNCK_SCREEN_XSCREEN (window->priv->screen),
		      window->priv->xwindow,
                      FALSE,
                      _wnck_atom_get ("_NET_WM_STATE_MAXIMIZED_VERT"),
                      _wnck_atom_get ("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

void
wnck_window_maximize_horizontally   (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_change_state (WNCK_SCREEN_XSCREEN (window->priv->screen),
		      window->priv->xwindow,
                      TRUE,
                      _wnck_atom_get ("_NET_WM_STATE_MAXIMIZED_HORZ"),
                      0);
}

void
wnck_window_unmaximize_horizontally (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_change_state (WNCK_SCREEN_XSCREEN (window->priv->screen),
		      window->priv->xwindow,
                      FALSE,
                      _wnck_atom_get ("_NET_WM_STATE_MAXIMIZED_HORZ"),
                      0);
}

void
wnck_window_maximize_vertically     (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_change_state (WNCK_SCREEN_XSCREEN (window->priv->screen),
		      window->priv->xwindow,
                      TRUE,
                      _wnck_atom_get ("_NET_WM_STATE_MAXIMIZED_VERT"),
                      0);
}

void
wnck_window_unmaximize_vertically   (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_change_state (WNCK_SCREEN_XSCREEN (window->priv->screen),
		      window->priv->xwindow,
                      FALSE,
                      _wnck_atom_get ("_NET_WM_STATE_MAXIMIZED_VERT"),
                      0);
}

void
wnck_window_shade                   (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_change_state (WNCK_SCREEN_XSCREEN (window->priv->screen),
		      window->priv->xwindow,
                      TRUE,
                      _wnck_atom_get ("_NET_WM_STATE_SHADED"),
                      0);
}

void
wnck_window_unshade                 (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_change_state (WNCK_SCREEN_XSCREEN (window->priv->screen),
		      window->priv->xwindow,
                      FALSE,
                      _wnck_atom_get ("_NET_WM_STATE_SHADED"),
                      0);
}

void
wnck_window_stick                   (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_change_state (WNCK_SCREEN_XSCREEN (window->priv->screen),
		      window->priv->xwindow,
                      TRUE,
                      _wnck_atom_get ("_NET_WM_STATE_STICKY"),
                      0);
}

void
wnck_window_unstick                 (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_change_state (WNCK_SCREEN_XSCREEN (window->priv->screen),
		      window->priv->xwindow,
                      FALSE,
                      _wnck_atom_get ("_NET_WM_STATE_STICKY"),
                      0);
}

void
wnck_window_keyboard_move (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_keyboard_move (WNCK_SCREEN_XSCREEN (window->priv->screen),
                       window->priv->xwindow);
}

void
wnck_window_keyboard_size (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_keyboard_size (WNCK_SCREEN_XSCREEN (window->priv->screen),
                       window->priv->xwindow);
}

/**
 * wnck_window_get_workspace:
 * @window: a #WnckWindow
 *
 * Gets the window's current workspace. If the window is
 * pinned (on all workspaces), or not on any workspaces,
 * %NULL may be returned.
 *
 * Return value: single current workspace or %NULL
 **/
WnckWorkspace*
wnck_window_get_workspace (WnckWindow *window)
{
  if (window->priv->workspace == ALL_WORKSPACES)
    return NULL;
  else
    return wnck_screen_get_workspace (window->priv->screen, window->priv->workspace);
}

void
wnck_window_move_to_workspace (WnckWindow    *window,
                               WnckWorkspace *space)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));
  g_return_if_fail (WNCK_IS_WORKSPACE (space));

  _wnck_change_workspace (WNCK_SCREEN_XSCREEN (window->priv->screen),
			  window->priv->xwindow,
                          wnck_workspace_get_number (space));
}

/**
 * wnck_window_is_pinned:
 * @window: a #WnckWindow
 *
 * %TRUE if the window is on all workspaces. Note that if this
 * changes, it's signalled by a workspace_changed signal,
 * not state_changed.
 *
 * Return value: %TRUE if on all workspaces
 **/
gboolean
wnck_window_is_pinned (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);

  return window->priv->workspace == ALL_WORKSPACES;
}

void
wnck_window_pin (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_change_workspace (WNCK_SCREEN_XSCREEN (window->priv->screen),
			  window->priv->xwindow,
                          ALL_WORKSPACES);
}

/**
 * wnck_window_unpin:
 * @window: a #WnckWindow
 *
 * Sets @window's workspace to only the currently active workspace,
 * if the window was previously pinned. If the window wasn't pinned,
 * doesn't change the window's workspace. If the active workspace
 * isn't known for some reason (shouldn't happen much), sets the
 * window's workspace to 0.
 *
 **/
void
wnck_window_unpin (WnckWindow *window)
{
  WnckWorkspace *active;

  g_return_if_fail (WNCK_IS_WINDOW (window));

  if (window->priv->workspace != ALL_WORKSPACES)
    return;

  active = wnck_screen_get_active_workspace (window->priv->screen);

  _wnck_change_workspace (WNCK_SCREEN_XSCREEN (window->priv->screen),
			  window->priv->xwindow,
                          active ? wnck_workspace_get_number (active) : 0);
}

/**
 * wnck_window_activate:
 * @window: a #WnckWindow
 *
 * Ask the window manager to make @window the active window.  The
 * window manager may choose to raise @window along with focusing it.
 **/
void
wnck_window_activate (WnckWindow *window)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  _wnck_activate (WNCK_SCREEN_XSCREEN (window->priv->screen),
		  window->priv->xwindow);
}

/**
 * wnck_window_is_active:
 * @window: a #WnckWindow
 *
 *
 *
 * Return value: %TRUE if the window is the active window
 **/
gboolean
wnck_window_is_active (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);

  return window == wnck_screen_get_active_window (window->priv->screen);
}


static WnckWindow*
find_last_transient_for (GList *windows,
                         Window xwindow)
{
  GList *tmp; 
  WnckWindow *retval;

  /* find _last_ transient for xwindow in the list */
  
  retval = NULL;
  
  tmp = windows;
  while (tmp != NULL)
    {
      WnckWindow *w = tmp->data;

      if (w->priv->transient_for == xwindow)
        retval = w;
      
      tmp = tmp->next;
    }

  return retval;
}

/**
 * wnck_window_activate_transient:
 * @window: a #WnckWindow
 *
 * If @window has transients, activates the most likely transient
 * instead of the window itself. Otherwise activates @window.
 * 
 * FIXME the ideal behavior of this function is probably to activate
 * the most recently active window among @window and its transients.
 * This is probably best implemented on the window manager side.
 * 
 **/
void
wnck_window_activate_transient (WnckWindow *window)
{
  GList *windows;
  WnckWindow *transient;
  WnckWindow *next;
  
  g_return_if_fail (WNCK_IS_WINDOW (window));

  windows = wnck_screen_get_windows_stacked (window->priv->screen);

  transient = NULL;
  next = find_last_transient_for (windows, window->priv->xwindow);
  
  while (next != NULL)
    {
      if (next == window)
        {
          /* catch transient cycles */
          transient = NULL;
          break;
        }

      transient = next;
      
      next = find_last_transient_for (windows, transient->priv->xwindow);
    }

  if (transient != NULL)
    {
      /* Raise but don't focus the main window */
      XRaiseWindow (gdk_display, window->priv->xwindow);
      /* then activate the transient */
      wnck_window_activate (transient);
    }
  else
    {
      wnck_window_activate (window);
    }
}

static void
get_icons (WnckWindow *window)
{
  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;

  icon = NULL;
  mini_icon = NULL;
  
  if (_wnck_read_icons (window->priv->xwindow,
                        window->priv->icon_cache,
                        &icon,
                        DEFAULT_ICON_WIDTH, DEFAULT_ICON_HEIGHT,
                        &mini_icon,
                        DEFAULT_MINI_ICON_WIDTH,
                        DEFAULT_MINI_ICON_HEIGHT))
    {
      window->priv->need_emit_icon_changed = TRUE;

      if (window->priv->icon)
        g_object_unref (G_OBJECT (window->priv->icon));

      if (window->priv->mini_icon)
        g_object_unref (G_OBJECT (window->priv->mini_icon));

      window->priv->icon = icon;
      window->priv->mini_icon = mini_icon;
    }

  g_assert ((window->priv->icon && window->priv->mini_icon) ||
            !(window->priv->icon || window->priv->mini_icon));
}

GdkPixbuf*
wnck_window_get_icon (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);

  get_icons (window);
  if (window->priv->need_emit_icon_changed)
    queue_update (window); /* not done in get_icons since we call that from
                            * the update
                            */

  return window->priv->icon;
}

GdkPixbuf*
wnck_window_get_mini_icon (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);
  
  get_icons (window);
  if (window->priv->need_emit_icon_changed)
    queue_update (window); /* not done in get_icons since we call that from
                            * the update
                            */
  
  return window->priv->mini_icon;
}

/**
 * wnck_window_get_icon_is_fallback:
 * @window: a #WnckWindow
 *
 * Checks if we are using a default fallback icon because
 * none was set on the window.
 * 
 * Return value: %TRUE if icon is a fallback
 **/
gboolean
wnck_window_get_icon_is_fallback (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);

  return _wnck_icon_cache_get_is_fallback (window->priv->icon_cache);
}

/**
 * wnck_window_get_actions:
 * @window: a #WnckWindow
 * 
 * Gets the window operations that should be sensitive for @window.
 * 
 * Return value: bitmask of actions the window supports
 **/
WnckWindowActions
wnck_window_get_actions (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), 0);

  return window->priv->actions;
}


/**
 * wnck_window_get_state:
 * @window: a #WnckWindow
 * 
 * Gets the state of a window as a bitmask.
 * 
 * Return value: bitmask of active state flags.
 **/
WnckWindowState
wnck_window_get_state (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), 0);

  return COMPRESS_STATE (window);
}

/**
 * wnck_window_get_geometry:
 * @window: a #WnckWindow
 * @xp: return location for X coordinate of window 
 * @yp: return location for Y coordinate of window
 * @widthp: return location for width of window
 * @heightp: return location for height of window
 *
 * Gets the size and position of the window, as last received
 * in a configure notify (i.e. this call does not round-trip
 * to the server, just gets the last size we were notified of).
 * The X and Y coordinates are relative to the root window.
 * 
 **/
void
wnck_window_get_geometry (WnckWindow *window,
                          int        *xp,
                          int        *yp,
                          int        *widthp,
                          int        *heightp)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));

  if (xp)
    *xp = window->priv->x;
  if (yp)
    *yp = window->priv->y;
  if (widthp)
    *widthp = window->priv->width;
  if (heightp)
    *heightp = window->priv->height;
}

/**
 * wnck_window_is_visible_on_workspace:
 * @window: a #WnckWindow
 * @workspace: a #WnckWorkspace
 * 
 * Like wnck_window_is_on_workspace(), but also checks that
 * the window is in a visible state (i.e. not minimized or shaded).
 * 
 * Return value: %TRUE if @window appears on @workspace in normal state
 **/
gboolean
wnck_window_is_visible_on_workspace (WnckWindow    *window,
                                     WnckWorkspace *workspace)
{
  WnckWindowState state;

  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (WNCK_IS_WORKSPACE (workspace), FALSE);
  
  state = wnck_window_get_state (window);

  if (state & WNCK_WINDOW_STATE_HIDDEN)
    return FALSE; /* not visible */

  return wnck_window_is_on_workspace (window, workspace);
}

/**
 * wnck_window_set_icon_geometry:
 * @window: a #WnckWindow
 * @x: x coordinate in pixels
 * @y: y coordinate in pixels
 * @width: width in pixels
 * @height: height in pixels
 *
 * Set the icon geometry for a WnckWindow.
 */
void
wnck_window_set_icon_geometry (WnckWindow *window,
			       int         x,
			       int         y,
			       int         width,
			       int         height)
{
  _wnck_set_icon_geometry (window->priv->xwindow,
                           x, y, width, height);
}

/**
 * wnck_window_is_on_workspace:
 * @window: a #WnckWindow
 * @workspace: a #WnckWorkspace
 * 
 * 
 * 
 * Return value: %TRUE if @window appears on @workspace
 **/
gboolean
wnck_window_is_on_workspace (WnckWindow    *window,
                             WnckWorkspace *workspace)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (WNCK_IS_WORKSPACE (workspace), FALSE);
  
  return wnck_window_is_pinned (window) ||
    wnck_window_get_workspace (window) == workspace;
}

/**
 * wnck_window_is_in_viewport:
 * @window: a #WnckWindow
 * @workspace: a #WnckWorkspace
 * 
 * Returns #TRUE if the window is inside the current viewport
 * of the given workspace.
 * 
 * Return value: %TRUE if @window appears in current viewport
 **/
gboolean
wnck_window_is_in_viewport (WnckWindow    *window,
                            WnckWorkspace *workspace)
{
  GdkRectangle window_rect;
  GdkRectangle viewport_rect;
  
  g_return_val_if_fail (WNCK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (WNCK_IS_WORKSPACE (workspace), FALSE);

  if (wnck_window_is_pinned (window) )
    return TRUE;

  if (wnck_window_get_workspace (window) != workspace)
    return FALSE;

  viewport_rect.x = wnck_workspace_get_viewport_x (workspace);
  viewport_rect.y = wnck_workspace_get_viewport_y (workspace);
  viewport_rect.width = wnck_screen_get_width (window->priv->screen);
  viewport_rect.height = wnck_screen_get_height (window->priv->screen);

  window_rect.x = window->priv->x + viewport_rect.x;
  window_rect.y = window->priv->y + viewport_rect.y;
  window_rect.width = window->priv->width;
  window_rect.height = window->priv->height;

  return gdk_rectangle_intersect (&viewport_rect, &window_rect, &window_rect);
}

void
_wnck_window_set_application (WnckWindow      *window,
                              WnckApplication *app)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));
  g_return_if_fail (app == NULL || WNCK_IS_APPLICATION (app));

  if (app)
    g_object_ref (G_OBJECT (app));
  if (window->priv->app)
    g_object_unref (G_OBJECT (window->priv->app));
  window->priv->app = app;
}

void
_wnck_window_process_property_notify (WnckWindow *window,
                                      XEvent     *xevent)
{
  if (xevent->xproperty.atom ==
      _wnck_atom_get ("_NET_WM_STATE"))
    {
      window->priv->need_update_state = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
      _wnck_atom_get ("WM_STATE"))
    {
      window->priv->need_update_wm_state = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           XA_WM_NAME ||
           xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_WM_NAME") ||
           xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_WM_VISIBLE_NAME"))
    {
      window->priv->need_update_name = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           XA_WM_ICON_NAME ||
           xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_WM_ICON_NAME") ||
           xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_WM_VISIBLE_ICON_NAME"))
    {
      window->priv->need_update_icon_name = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_WM_ALLOWED_ACTIONS"))
    {
      window->priv->need_update_actions = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_WM_DESKTOP"))
    {
      window->priv->need_update_workspace = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_WM_WINDOW_TYPE"))
    {
      window->priv->need_update_wintype = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("WM_TRANSIENT_FOR"))
    {
      window->priv->need_update_transient_for = TRUE;
      window->priv->need_update_wintype = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_STARTUP_ID"))
    {
      window->priv->need_update_startup_id = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom == XA_WM_CLASS)
    {
      window->priv->need_update_wmclass = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_WM_ICON") ||
           xevent->xproperty.atom ==
           _wnck_atom_get ("KWM_WIN_ICON") ||
           xevent->xproperty.atom ==
           _wnck_atom_get ("WM_HINTS"))
    {
      _wnck_icon_cache_property_changed (window->priv->icon_cache,
                                         xevent->xproperty.atom);
      queue_update (window);
    }
}

void
_wnck_window_process_configure_notify (WnckWindow *window,
                                       XEvent     *xevent)
{
  if (xevent->xconfigure.send_event)
    {
      window->priv->x = xevent->xconfigure.x;
      window->priv->y = xevent->xconfigure.y;
    }
  else
    {
      _wnck_get_window_position (WNCK_SCREEN_XSCREEN (window->priv->screen),
				 window->priv->xwindow,
                                 &window->priv->x,
                                 &window->priv->y);
    }

  window->priv->width = xevent->xconfigure.width;
  window->priv->height = xevent->xconfigure.height;

  emit_geometry_changed (window);
}

static void
update_wm_state (WnckWindow *window)
{
  int state;

  if (!window->priv->need_update_wm_state)
    return;

  window->priv->need_update_wm_state = FALSE;

  window->priv->wm_state_iconic = FALSE;

  state = _wnck_get_wm_state (window->priv->xwindow);

  if (state == IconicState)
    window->priv->wm_state_iconic = TRUE;
}

static void
update_state (WnckWindow *window)
{
  Atom *atoms;
  int n_atoms;
  int i;
  gboolean reread_net_wm_state;

  reread_net_wm_state = window->priv->need_update_state;

  window->priv->need_update_state = FALSE;

  /* This is a bad hack, we always add the
   * state based on window type in to the state,
   * even if no state update is pending (since the
   * state update just means the _NET_WM_STATE prop
   * changed
   */
  
  if (reread_net_wm_state)
    {
      window->priv->is_maximized_horz = FALSE;
      window->priv->is_maximized_vert = FALSE;
      window->priv->is_sticky = FALSE;
      window->priv->is_shaded = FALSE;
      window->priv->skip_taskbar = FALSE;
      window->priv->skip_pager = FALSE;
      window->priv->net_wm_state_hidden = FALSE;
      
      atoms = NULL;
      n_atoms = 0;
      _wnck_get_atom_list (window->priv->xwindow,
                           _wnck_atom_get ("_NET_WM_STATE"),
                           &atoms, &n_atoms);

      i = 0;
      while (i < n_atoms)
        {
          if (atoms[i] == _wnck_atom_get ("_NET_WM_STATE_MAXIMIZED_VERT"))
            window->priv->is_maximized_vert = TRUE;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_STATE_MAXIMIZED_HORZ"))
            window->priv->is_maximized_horz = TRUE;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_STATE_HIDDEN"))
            window->priv->net_wm_state_hidden = TRUE;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_STATE_STICKY"))
            window->priv->is_sticky = TRUE;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_STATE_SHADED"))
            window->priv->is_shaded = TRUE;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_STATE_FULLSCREEN"))
            window->priv->is_fullscreen = TRUE;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_STATE_SKIP_TASKBAR"))
            window->priv->skip_taskbar = TRUE;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_STATE_SKIP_PAGER"))
            window->priv->skip_pager = TRUE;

          ++i;
        }

      g_free (atoms);
    }

  switch (window->priv->wintype)
    {
    case WNCK_WINDOW_DESKTOP:
    case WNCK_WINDOW_DOCK:
    case WNCK_WINDOW_SPLASHSCREEN:
      window->priv->skip_taskbar = TRUE;
      break;

    case WNCK_WINDOW_TOOLBAR:
    case WNCK_WINDOW_MENU:
    case WNCK_WINDOW_UTILITY:
    case WNCK_WINDOW_DIALOG:
    case WNCK_WINDOW_MODAL_DIALOG:
      /* Skip taskbar if the window is transient
       * for some main application window
       */
      if (window->priv->transient_for != None &&
          !window->priv->transient_for_root)
        window->priv->skip_taskbar = TRUE;
      break;
      
    case WNCK_WINDOW_NORMAL:
      break;
    }

  switch (window->priv->wintype)
    {
    case WNCK_WINDOW_DESKTOP:
    case WNCK_WINDOW_DOCK:
    case WNCK_WINDOW_TOOLBAR:
    case WNCK_WINDOW_MENU:
    case WNCK_WINDOW_SPLASHSCREEN:
      window->priv->skip_pager = TRUE;
      break;
      
    case WNCK_WINDOW_NORMAL:
    case WNCK_WINDOW_DIALOG:
    case WNCK_WINDOW_MODAL_DIALOG:
    case WNCK_WINDOW_UTILITY:
      break;
    }
  
  /* FIXME we need to recompute this if the window manager changes */
  if (wnck_screen_net_wm_supports (window->priv->screen,
                                   "_NET_WM_STATE_HIDDEN"))
    {
      window->priv->is_hidden = window->priv->net_wm_state_hidden;

      /* FIXME this is really broken; need to bring it up on
       * wm-spec-list. It results in showing an "Unminimize" menu
       * item on task list, for shaded windows.
       */
      window->priv->is_minimized = window->priv->is_hidden;
    }
  else
    {
      window->priv->is_minimized = window->priv->wm_state_iconic;
      
      window->priv->is_hidden = window->priv->is_minimized || window->priv->is_shaded;
    }
}

static void
update_name (WnckWindow *window)
{
  g_return_if_fail (window->priv->name == NULL);

  if (!window->priv->need_update_name)
    return;

  window->priv->need_update_name = FALSE;

  window->priv->name = _wnck_get_name (window->priv->xwindow);

  if (window->priv->name == NULL)
    window->priv->name = g_strdup (FALLBACK_NAME);
}

static void
update_icon_name (WnckWindow *window)
{
  g_return_if_fail (window->priv->icon_name == NULL);

  if (!window->priv->need_update_icon_name)
    return;

  window->priv->need_update_icon_name = FALSE;

  if (window->priv->icon_name == NULL)
    window->priv->icon_name =
      _wnck_get_utf8_property (window->priv->xwindow,
                               _wnck_atom_get ("_NET_WM_VISIBLE_ICON_NAME"));

  if (window->priv->icon_name == NULL)
    window->priv->icon_name =
      _wnck_get_utf8_property (window->priv->xwindow,
                               _wnck_atom_get ("_NET_WM_ICON_NAME"));

  if (window->priv->icon_name == NULL)
    window->priv->icon_name =
      _wnck_get_text_property (window->priv->xwindow,
                               XA_WM_ICON_NAME);

  /* no fallback, get_icon_name falls back to regular window title */
}

static void
update_workspace (WnckWindow *window)
{
  int val;
  int old;

  if (!window->priv->need_update_workspace)
    return;

  window->priv->need_update_workspace = FALSE;

  old = window->priv->workspace;

  val = ALL_WORKSPACES;
  _wnck_get_cardinal (window->priv->xwindow,
                      _wnck_atom_get ("_NET_WM_DESKTOP"),
                      &val);

  window->priv->workspace = val;

  if (old != window->priv->workspace)
    emit_workspace_changed (window);
}

static void
update_actions (WnckWindow *window)
{
  Atom *atoms;
  int   n_atoms;
  int   i;

  if (!window->priv->need_update_actions)
    return;

  window->priv->need_update_actions = FALSE;

  window->priv->actions = 0;

  if (!_wnck_get_atom_list (window->priv->xwindow,
                            _wnck_atom_get ("_NET_WM_ALLOWED_ACTIONS"),
                            &atoms,
                            &n_atoms))
    {
      window->priv->actions = 
                WNCK_WINDOW_ACTION_MOVE                    |
                WNCK_WINDOW_ACTION_RESIZE                  |
                WNCK_WINDOW_ACTION_SHADE                   |
                WNCK_WINDOW_ACTION_STICK                   |
                WNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY   |
                WNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY     |
                WNCK_WINDOW_ACTION_CHANGE_WORKSPACE        |
                WNCK_WINDOW_ACTION_CLOSE                   |
                WNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY |
                WNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY   |
                WNCK_WINDOW_ACTION_UNSHADE                 |
                WNCK_WINDOW_ACTION_UNSTICK                 |
                WNCK_WINDOW_ACTION_MAXIMIZE                |
                WNCK_WINDOW_ACTION_UNMAXIMIZE              |
                WNCK_WINDOW_ACTION_MINIMIZE                |
                WNCK_WINDOW_ACTION_UNMINIMIZE;
      return;
    }

  i = 0;
  while (i < n_atoms)
    {
      if (atoms[i] == _wnck_atom_get ("_NET_WM_ACTION_MOVE"))
        window->priv->actions |= WNCK_WINDOW_ACTION_MOVE;

      else if (atoms[i] == _wnck_atom_get ("_NET_WM_ACTION_RESIZE"))
        window->priv->actions |= WNCK_WINDOW_ACTION_RESIZE;

      else if (atoms[i] == _wnck_atom_get ("_NET_WM_ACTION_SHADE"))
        window->priv->actions |= WNCK_WINDOW_ACTION_SHADE |
                                 WNCK_WINDOW_ACTION_UNSHADE;

      else if (atoms[i] == _wnck_atom_get ("_NET_WM_ACTION_STICK"))
        window->priv->actions |= WNCK_WINDOW_ACTION_STICK |
                                 WNCK_WINDOW_ACTION_UNSTICK;

      else if (atoms[i] == _wnck_atom_get ("_NET_WM_ACTION_MAXIMIZE_HORZ"))
        window->priv->actions |= WNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY |
                                 WNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY;

      else if (atoms[i] == _wnck_atom_get ("_NET_WM_ACTION_MAXIMIZE_VERT"))
        window->priv->actions |= WNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY |
                                 WNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY;

      else if (atoms[i] == _wnck_atom_get ("_NET_WM_ACTION_CHANGE_DESKTOP"))
        window->priv->actions |= WNCK_WINDOW_ACTION_CHANGE_WORKSPACE;

      else if (atoms[i] == _wnck_atom_get ("_NET_WM_ACTION_CLOSE"))
        window->priv->actions |= WNCK_WINDOW_ACTION_CLOSE;
      else
        g_warning ("Unhandled action type %s", _wnck_atom_name (atoms [i]));

      i++;
    }

  g_free (atoms);

  if ((window->priv->actions & WNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY) &&
      (window->priv->actions & WNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY))
    window->priv->actions |=
        WNCK_WINDOW_ACTION_MAXIMIZE   |
        WNCK_WINDOW_ACTION_UNMAXIMIZE;

  /* These are always enabled */
  window->priv->actions |=
        WNCK_WINDOW_ACTION_MINIMIZE   |
        WNCK_WINDOW_ACTION_UNMINIMIZE;
}

static void
update_wintype (WnckWindow *window)
{
  Atom *atoms;
  int n_atoms;
  WnckWindowType type;
  gboolean found_type;
  
  if (!window->priv->need_update_wintype)
    return;

  window->priv->need_update_wintype = FALSE;

  found_type = FALSE;
  type = WNCK_WINDOW_NORMAL;
  
  if (_wnck_get_atom_list (window->priv->xwindow,
                           _wnck_atom_get ("_NET_WM_WINDOW_TYPE"),
                           &atoms,
                           &n_atoms))
    {
      int i;
      
      i = 0;
      while (i < n_atoms && !found_type)
        {
          /* We break as soon as we find one we recognize,
           * supposed to prefer those near the front of the list
           */
          found_type = TRUE;
          if (atoms[i] == _wnck_atom_get ("_NET_WM_WINDOW_TYPE_DESKTOP"))
            type = WNCK_WINDOW_DESKTOP;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_WINDOW_TYPE_DOCK"))
            type = WNCK_WINDOW_DOCK;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_WINDOW_TYPE_TOOLBAR"))
            type = WNCK_WINDOW_TOOLBAR;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_WINDOW_TYPE_MENU"))
            type = WNCK_WINDOW_MENU;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_WINDOW_TYPE_DIALOG"))
            type = WNCK_WINDOW_DIALOG;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_WINDOW_TYPE_NORMAL"))
            type = WNCK_WINDOW_NORMAL;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_WINDOW_TYPE_MODAL_DIALOG"))
            type = WNCK_WINDOW_MODAL_DIALOG;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_WINDOW_TYPE_UTILITY"))
            type = WNCK_WINDOW_UTILITY;
          else if (atoms[i] == _wnck_atom_get ("_NET_WM_WINDOW_TYPE_SPLASH"))
            type = WNCK_WINDOW_SPLASHSCREEN;
          else
            found_type = FALSE;
          
          ++i;
        }

      g_free (atoms);
    }

  if (!found_type)
    {
      if (window->priv->transient_for != None)
        {
          type = WNCK_WINDOW_DIALOG;
        }
      else
        {
          type = WNCK_WINDOW_NORMAL;
        }
      found_type = TRUE;
    }

  window->priv->wintype = type;
}

static void
update_transient_for (WnckWindow *window)
{
  Window parent;

  if (!window->priv->need_update_transient_for)
    return;

  window->priv->need_update_transient_for = FALSE;
  
  if (_wnck_get_window (window->priv->xwindow,
                        _wnck_atom_get ("WM_TRANSIENT_FOR"),
                        &parent))
    {
      window->priv->transient_for = parent;

      if (wnck_screen_get_for_root (window->priv->transient_for) != NULL)
        window->priv->transient_for_root = TRUE;
      else
        window->priv->transient_for_root = FALSE;
    }
  else
    {
      window->priv->transient_for = None;
      window->priv->transient_for_root = FALSE;
    }
}

static void
update_startup_id (WnckWindow *window)
{
  if (!window->priv->need_update_startup_id)
    return;

  window->priv->need_update_startup_id = FALSE;

  g_free (window->priv->startup_id);
  window->priv->startup_id = 
    _wnck_get_utf8_property (window->priv->xwindow,
                             _wnck_atom_get ("_NET_STARTUP_ID"));
}

static void
update_wmclass (WnckWindow *window)
{
  if (!window->priv->need_update_wmclass)
    return;

  window->priv->need_update_wmclass = FALSE;

  g_free (window->priv->res_class);
  g_free (window->priv->res_name);

  _wnck_get_wmclass (window->priv->xwindow,
                     &window->priv->res_class,
                     &window->priv->res_name);
}

static void
force_update_now (WnckWindow *window)
{
  WnckWindowState old_state;
  WnckWindowState new_state;
  WnckWindowActions old_actions;
  char *old_name;
  char *old_icon_name;
  
  unqueue_update (window);

  /* Name must be done before all other stuff,
   * because we have iconsistent state across the
   * update_name/update_icon_name functions (no window name),
   * and we have to fix that before we emit any other signals
   */

  old_name = window->priv->name;
  window->priv->name = NULL;

  update_name (window);

  if (window->priv->name == NULL)
    window->priv->name = old_name;
  else
    {
      if (strcmp (window->priv->name, old_name) != 0)
        emit_name_changed (window);
      g_free (old_name);
    }

  old_icon_name = window->priv->icon_name;
  window->priv->icon_name = NULL;

  update_icon_name (window);

  if (window->priv->icon_name == NULL)
    window->priv->icon_name = old_icon_name;
  else
    {
      if (old_icon_name == NULL ||
          strcmp (window->priv->icon_name, old_icon_name) != 0)
        emit_name_changed (window);
      g_free (old_icon_name);
    }

  old_state = COMPRESS_STATE (window);
  old_actions = window->priv->actions;

  update_startup_id (window);    /* no side effects */
  update_wmclass (window);
  update_transient_for (window); /* wintype needs this to be first */
  update_wintype (window);
  update_wm_state (window);
  update_state (window);     /* must come after the above, since they affect
                              * our calculated state
                              */
  update_workspace (window); /* emits signals */
  update_actions (window);

  get_icons (window);
  
  new_state = COMPRESS_STATE (window);

  if (old_state != new_state)
    emit_state_changed (window, old_state ^ new_state, new_state);

  if (old_actions != window->priv->actions)
    emit_actions_changed (window, old_actions ^ window->priv->actions,
                          window->priv->actions);
  
  if (window->priv->need_emit_icon_changed)
    emit_icon_changed (window);
}


static gboolean
update_idle (gpointer data)
{
  WnckWindow *window = WNCK_WINDOW (data);

  window->priv->update_handler = 0;
  force_update_now (window);
  return FALSE;
}

static void
queue_update (WnckWindow *window)
{
  if (window->priv->update_handler != 0)
    return;

  window->priv->update_handler = g_idle_add (update_idle, window);
}

static void
unqueue_update (WnckWindow *window)
{
  if (window->priv->update_handler != 0)
    {
      g_source_remove (window->priv->update_handler);
      window->priv->update_handler = 0;
    }
}

static void
emit_name_changed (WnckWindow *window)
{
  g_signal_emit (G_OBJECT (window),
                 signals[NAME_CHANGED],
                 0);
}

static void
emit_state_changed (WnckWindow     *window,
                    WnckWindowState changed_mask,
                    WnckWindowState new_state)
{
  g_signal_emit (G_OBJECT (window),
                 signals[STATE_CHANGED],
                 0, changed_mask, new_state);
}

static void
emit_workspace_changed (WnckWindow *window)
{
  g_signal_emit (G_OBJECT (window),
                 signals[WORKSPACE_CHANGED],
                 0);
}

static void
emit_icon_changed (WnckWindow *window)
{
  window->priv->need_emit_icon_changed = FALSE;
  g_signal_emit (G_OBJECT (window),
                 signals[ICON_CHANGED],
                 0);
}

static void
emit_actions_changed   (WnckWindow       *window,
                        WnckWindowActions changed_mask,
                        WnckWindowActions new_actions)
{
  g_signal_emit (G_OBJECT (window),
                 signals[ACTIONS_CHANGED],
                 0, changed_mask, new_actions);
}

static void
emit_geometry_changed (WnckWindow *window)
{
  g_signal_emit (G_OBJECT (window),
                 signals[GEOMETRY_CHANGED],
                 0);
}
