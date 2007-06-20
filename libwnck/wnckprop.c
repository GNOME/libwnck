/* vim: set sw=2 et: */
/*
 * Copyright (C) 2007 Vincent Untz 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 *
 * Some of this code is based on code from gnome-panel/panel-force-quit.c
 * This part of the code was written by Mark McLoughlin <mark@skynet.ie> and
 * copyrighted by Sun. Sun (through Glynn Foster <glynn.foster@sun.com>)
 * agreed to relicense it from the GPL to the LGPL.
 */

/* TODO:
 *  investigate why those doesn't work:
 *   --set-n-workspaces
 *   --activate-workspace (timestamp related?)
 *   --activate (and probably --unminimize) (timestamp related?)
 *   --show-desktop and --unshow-desktop
 *   --keyboard-move and --keyboard-resize
 *
 *  uncomment code that prints the workspace layout when API is public.
 *
 *  add --list-screen
 */
#include <config.h>

#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <glib/gi18n.h>

#include <libwnck/libwnck.h>
#include <libwnck/class-group.h>

gboolean screen_mode  = FALSE;
gboolean screen_props = FALSE;
gboolean window_props = FALSE;

gulong   xid = 0;
int      interact_screen = -1;

gboolean list_windows = FALSE;
int      list_windows_workspace = -1;
gboolean list_workspaces = FALSE;
int      set_activate_workspace = -1;
int      set_n_workspaces = -1;
int      set_workspace_rows = 0;
int      set_workspace_cols = 0;
gboolean set_show_desktop = FALSE;
gboolean set_unshow_desktop = FALSE;
int      set_viewport_x = -1;
int      set_viewport_y = -1;

gboolean set_minimize = FALSE;
gboolean set_unminimize = FALSE;
gboolean set_maximize = FALSE;
gboolean set_unmaximize = FALSE;
gboolean set_maximize_horizontally = FALSE;
gboolean set_unmaximize_horizontally = FALSE;
gboolean set_maximize_vertically = FALSE;
gboolean set_unmaximize_vertically = FALSE;
gboolean set_keyboard_move = FALSE;
gboolean set_keyboard_resize = FALSE;
gboolean set_activate = FALSE;
gboolean set_close = FALSE;

gboolean set_fullscreen = FALSE;
gboolean set_unfullscreen = FALSE;
gboolean set_make_above = FALSE;
gboolean set_unmake_above = FALSE;
gboolean set_shade = FALSE;
gboolean set_unshade = FALSE;
gboolean set_stick = FALSE;
gboolean set_unstick = FALSE;
gboolean set_skip_pager = FALSE;
gboolean set_unskip_pager = FALSE;
gboolean set_skip_tasklist = FALSE;
gboolean set_unskip_tasklist = FALSE;
gboolean set_pin = FALSE;
gboolean set_unpin = FALSE;
int      set_workspace = -1;
int      set_x = G_MAXINT;
int      set_y = G_MAXINT;
int      set_width = -1;
int      set_height = -1;
char     *set_window_type = NULL;
WnckWindowType set_window_type_t = WNCK_WINDOW_NORMAL;

static GOptionEntry main_entries[] = {
	{ "xid", 0, 0, G_OPTION_ARG_INT, &xid,
          N_("X window ID of the window to examine or modify"), N_("XID") },
	{ "screen", 0, 0, G_OPTION_ARG_INT, &interact_screen,
          N_("NUMBER of the screen to examine or modify"), N_("NUMBER") },
	{ NULL }
};

static GOptionEntry list_entries[] = {
	{ "list-windows", 0, 0, G_OPTION_ARG_NONE, &list_windows,
          N_("List windows of the screen (format: \"XID: Window Name\")"), NULL },
	{ "list-windows-workspace", 0, 0, G_OPTION_ARG_INT, &list_windows_workspace,
          N_("List windows of the screen in workspace NUMBER (format: \"XID: Window Name\")"), N_("NUMBER") },
	{ "list-workspaces", 0, 0, G_OPTION_ARG_NONE, &list_workspaces,
          N_("List workspaces of the screen (format: \"Number: Workspace Name\")"), NULL },
	{ NULL }
};

static GOptionEntry screen_entries[] = {
	{ "activate-workspace", 0, 0, G_OPTION_ARG_INT, &set_activate_workspace,
          N_("Change the current workspace to workspace NUMBER"), N_("NUMBER") },
	{ "set-n-workspaces", 0, 0, G_OPTION_ARG_INT, &set_n_workspaces,
          N_("Change the number of workspaces of the screen to NUMBER"), N_("NUMBER") },
	{ "set-workspace-rows", 0, 0, G_OPTION_ARG_INT, &set_workspace_rows,
          N_("Change the workspace layout of the screen to use NUMBER rows"), N_("NUMBER") },
	{ "set-workspace-columns", 0, 0, G_OPTION_ARG_INT, &set_workspace_cols,
          N_("Change the workspace layout of the screen to use NUMBER columns"), N_("NUMBER") },
	{ "show-desktop", 0, 0, G_OPTION_ARG_NONE, &set_show_desktop,
          N_("Show the desktop"), NULL },
	{ "unshow-desktop", 0, 0, G_OPTION_ARG_NONE, &set_unshow_desktop,
          N_("Stop showing the desktop"), NULL },
	{ "move-viewport-x", 0, 0, G_OPTION_ARG_INT, &set_viewport_x,
          N_("Move the viewport of the current workspace to X coordinate X"), N_("X") },
	{ "move-viewport-y", 0, 0, G_OPTION_ARG_INT, &set_viewport_y,
          N_("Move the viewport of the current workspace to Y coordinate Y"), N_("Y") },
	{ NULL }
};

static GOptionEntry window_entries[] = {
	{ "minimize", 0, 0, G_OPTION_ARG_NONE, &set_minimize,
          N_("Minimize the window"), NULL },
	{ "unminimize", 0, 0, G_OPTION_ARG_NONE, &set_unminimize,
          N_("Unminimize the window"), NULL },
	{ "maximize", 0, 0, G_OPTION_ARG_NONE, &set_maximize,
          N_("Maximize the window"), NULL },
	{ "unmaximize", 0, 0, G_OPTION_ARG_NONE, &set_unmaximize,
          N_("Unmaximize the window"), NULL },
	{ "maximize-horizontally", 0, 0, G_OPTION_ARG_NONE, &set_maximize_horizontally,
          N_("Maximize horizontally the window"), NULL },
	{ "unmaximize-horizontally", 0, 0, G_OPTION_ARG_NONE, &set_unmaximize_horizontally,
          N_("Unmaximize horizontally the window"), NULL },
	{ "maximize-vertically", 0, 0, G_OPTION_ARG_NONE, &set_maximize_vertically,
          N_("Maximize vertically the window"), NULL },
	{ "unmaximize-vertically", 0, 0, G_OPTION_ARG_NONE, &set_unmaximize_vertically,
          N_("Unmaximize vertically the window"), NULL },
	{ "keyboard-move", 0, 0, G_OPTION_ARG_NONE, &set_keyboard_move,
          N_("Start moving the window via the keyboard"), NULL },
	{ "keyboard-resize", 0, 0, G_OPTION_ARG_NONE, &set_keyboard_resize,
          N_("Start resizing the window via the keyboard"), NULL },
	{ "activate", 0, 0, G_OPTION_ARG_NONE, &set_activate,
          N_("Activate the window"), NULL },
	{ "close", 0, 0, G_OPTION_ARG_NONE, &set_close,
          N_("Close the window"), NULL },

	{ "make-fullscreen", 0, 0, G_OPTION_ARG_NONE, &set_fullscreen,
          N_("Make the window fullscreen"), NULL },
	{ "unmake-fullscreen", 0, 0, G_OPTION_ARG_NONE, &set_unfullscreen,
          N_("Make the window quit fullscreen mode"), NULL },
	{ "make-above", 0, 0, G_OPTION_ARG_NONE, &set_make_above,
          N_("Make the window always on top"), NULL },
	{ "unmake-above", 0, 0, G_OPTION_ARG_NONE, &set_unmake_above,
          N_("Make the window not always on top"), NULL },
	{ "shade", 0, 0, G_OPTION_ARG_NONE, &set_shade,
          N_("Shade the window"), NULL },
	{ "unshade", 0, 0, G_OPTION_ARG_NONE, &set_unshade,
          N_("Unshade the window"), NULL },
	{ "stick", 0, 0, G_OPTION_ARG_NONE, &set_stick,
          N_("Make the window have a fixed position in the viewport"), NULL },
	{ "unstick", 0, 0, G_OPTION_ARG_NONE, &set_unstick,
          N_("Make the window not have a fixed position in the viewport"), NULL },
	{ "skip-pager", 0, 0, G_OPTION_ARG_NONE, &set_skip_pager,
          N_("Make the window not appear in pagers"), NULL },
	{ "unskip-pager", 0, 0, G_OPTION_ARG_NONE, &set_unskip_pager,
          N_("Make the window appear in pagers"), NULL },
	{ "skip-tasklist", 0, 0, G_OPTION_ARG_NONE, &set_skip_tasklist,
          N_("Make the window not appear in tasklists"), NULL },
	{ "unskip-tasklist", 0, 0, G_OPTION_ARG_NONE, &set_unskip_tasklist,
          N_("Make the window appear in tasklists"), NULL },
	{ "pin", 0, 0, G_OPTION_ARG_NONE, &set_pin,
          N_("Make the window visible on all workspaces"), NULL },
	{ "unpin", 0, 0, G_OPTION_ARG_NONE, &set_unpin,
          N_("Make the window visible on the current workspace only"), NULL },
	{ "set-workspace", 0, 0, G_OPTION_ARG_INT, &set_workspace,
          N_("Move the window to workspace NUMBER (first workspace is 0)"), N_("NUMBER") },
	{ "set-x", 0, 0, G_OPTION_ARG_INT, &set_x,
          N_("Change the X coordinate of the window to X"), N_("X") },
	{ "set-y", 0, 0, G_OPTION_ARG_INT, &set_y,
          N_("Change the Y coordinate of the window to Y"), N_("Y") },
	{ "set-width", 0, 0, G_OPTION_ARG_INT, &set_width,
          N_("Change the width of the window to WIDTH"), N_("WIDTH") },
	{ "set-height", 0, 0, G_OPTION_ARG_INT, &set_height,
          N_("Change the height of the window to HEIGHT"), N_("HEIGHT") },
	{ "set-window-type", 0, 0, G_OPTION_ARG_STRING, &set_window_type,
          /* Translators: do not translate "normal, desktop, dock..." */
          N_("Change the type of the window to TYPE (valid values: normal, desktop, dock, dialog, toolbar, menu, utility, splash"), N_("TYPE") },
	{ NULL }
};

static void clean_up (void);

static gboolean
validate_options (void)
{
#define CHECK_DUAL_OPTIONS(name, props)                                 \
  if (set_##name && set_un##name)                                       \
    {                                                                   \
      g_printerr (_("Conflicting options are present: %s and %s\n"),    \
                  #name, "un"#name);                                    \
      return FALSE;                                                     \
    }                                                                   \
  if (set_##name || set_un##name)                                       \
    props = TRUE;

#define CHECK_BOOL(name, props)                                         \
  if (set_##name)                                                       \
    props = TRUE;

#define CHECK_INT(var, props, name)                                     \
  if (var != G_MAXINT)                                                  \
    props = TRUE;

#define CHECK_POSITIVE_STRICT_INT(var, props, name)                     \
  if (var != -1 && var <= 0)                                            \
    {                                                                   \
      g_printerr (_("Invalid argument \"%d\" for %s: the argument must be strictly positive\n"), \
                  var, name);                                           \
      return FALSE;                                                     \
    }                                                                   \
  if (var != -1)                                                        \
    props = TRUE;

#define CHECK_POSITIVE_INT(var, props, name)                            \
  if (var != -1 && var < 0)                                             \
    {                                                                   \
      g_printerr (_("Invalid argument \"%d\" for %s: the argument must be positive\n"), \
                  var, name);                                           \
      return FALSE;                                                     \
    }                                                                   \
  if (var != -1)                                                        \
    props = TRUE;

#define CHECK_BOOL_NO_SCREEN_PROPS(var, name)                           \
  if (var)                                                              \
    {                                                                   \
      if (screen_props)                                                 \
        {                                                               \
          g_printerr (_("Conflicting options are present: an "          \
                        "option to modify a property of the screen and %s\n"), \
                      name);                                            \
          return FALSE;                                                 \
        }                                                               \
      screen_mode = TRUE;                                               \
    }

  CHECK_POSITIVE_INT (interact_screen, screen_mode, "screen")
  CHECK_POSITIVE_INT (set_activate_workspace, screen_props,
                      "activate-workspace")
  CHECK_POSITIVE_STRICT_INT (set_n_workspaces, screen_props,
                             "set-n-workspaces")
  if (set_workspace_rows > 0)
    screen_props = TRUE;
  if (set_workspace_cols > 0)
    screen_props = TRUE;
  CHECK_DUAL_OPTIONS (show_desktop, screen_props)
  CHECK_POSITIVE_INT (set_viewport_x, screen_props, "move-viewport-x")
  CHECK_POSITIVE_INT (set_viewport_y, screen_props, "move-viewport-y")

  /* do this after all setters of screen_props */
  CHECK_BOOL_NO_SCREEN_PROPS (list_windows, "list-windows")
  CHECK_BOOL_NO_SCREEN_PROPS (list_workspaces, "list-workspaces")
  CHECK_POSITIVE_INT (list_windows_workspace, screen_mode,
                      "list-windows-workspace")

  if (list_windows_workspace != -1 && list_windows)
    {
      g_printerr (_("Conflicting options are present: %s and %s\n"),
                  "list-windows", "list-windows-workspace");
      return FALSE;
    }
  if (list_windows_workspace != -1 && list_workspaces)
    {
      g_printerr (_("Conflicting options are present: %s and %s\n"),
                  "list-windows-workspace", "list-workspaces");
      return FALSE;
    }
  if (list_windows_workspace != -1)
    list_windows = TRUE;

  if (list_windows && list_workspaces)
    {
      g_printerr (_("Conflicting options are present: %s and %s\n"),
                  "list-windows", "list-workspaces");
      return FALSE;
    }

  CHECK_DUAL_OPTIONS (minimize, window_props)
  CHECK_DUAL_OPTIONS (maximize, window_props)
  CHECK_DUAL_OPTIONS (maximize_horizontally, window_props)
  CHECK_DUAL_OPTIONS (maximize_vertically, window_props)
  if (set_keyboard_move && set_keyboard_resize)
    {
      g_printerr (_("Conflicting options are present: %s and %s\n"),
                  "keyboard-move", "keyboard-resize");
      return FALSE;
    }
  CHECK_BOOL (keyboard_move, window_props)
  CHECK_BOOL (keyboard_resize, window_props)
  CHECK_BOOL (activate, window_props)
  CHECK_BOOL (close, window_props)

  CHECK_DUAL_OPTIONS (fullscreen, window_props)
  CHECK_DUAL_OPTIONS (make_above, window_props)
  CHECK_DUAL_OPTIONS (shade, window_props)
  CHECK_DUAL_OPTIONS (stick, window_props)
  CHECK_DUAL_OPTIONS (skip_pager, window_props)
  CHECK_DUAL_OPTIONS (skip_tasklist, window_props)
  CHECK_DUAL_OPTIONS (pin, window_props)

  if ((set_pin || set_unpin) && set_workspace != -1)
    {
      g_printerr (_("Conflicting options are present: %s or %s, and %s\n"),
                  "pin", "unpin", "set-workspace");
      return FALSE;
    }

  CHECK_POSITIVE_INT (set_workspace, window_props, "set-workspace")
  CHECK_INT (set_x, window_props, "set-x")
  CHECK_INT (set_y, window_props, "set-y")
  CHECK_POSITIVE_INT (set_width, window_props, "set-width")
  CHECK_POSITIVE_INT (set_height, window_props, "set-height")

  if (set_window_type != NULL)
    {
      if (strcmp (set_window_type, "normal") == 0)
        set_window_type_t = WNCK_WINDOW_NORMAL;
      else if (strcmp (set_window_type, "desktop") == 0)
        set_window_type_t = WNCK_WINDOW_DESKTOP;
      else if (strcmp (set_window_type, "dock") == 0)
        set_window_type_t = WNCK_WINDOW_DOCK;
      else if (strcmp (set_window_type, "dialog") == 0)
        set_window_type_t = WNCK_WINDOW_DIALOG;
      else if (strcmp (set_window_type, "toolbar") == 0)
        set_window_type_t = WNCK_WINDOW_TOOLBAR;
      else if (strcmp (set_window_type, "menu") == 0)
        set_window_type_t = WNCK_WINDOW_MENU;
      else if (strcmp (set_window_type, "utility") == 0)
        set_window_type_t = WNCK_WINDOW_UTILITY;
      else if (strcmp (set_window_type, "splash") == 0)
        set_window_type_t = WNCK_WINDOW_SPLASHSCREEN;
      else
        {
          g_printerr (_("Invalid argument \"%s\" for %s, valid values are: %s\n"),
                      set_window_type, "set-window-type",
                      "normal, desktop, dock, dialog, toolbar, menu, utility, splash");
        }

      window_props = TRUE;
    }

  if (screen_props)
    screen_mode = TRUE;

  if (screen_mode && (window_props || xid != 0))
    {
      g_printerr (_("Options for window and screen are present: the options "
                    "cannot be mixed\n"));
      return FALSE;
    }

  return TRUE;
}

static void
update_screen_props (WnckScreen *screen)
{
  int viewport_x;
  int viewport_y;
  unsigned int timestamp;

  // TODO: get a valid timestamp
  timestamp = gdk_x11_display_get_user_time (gdk_display_get_default ());

  if (set_n_workspaces > 0)
    wnck_screen_change_workspace_count (screen, set_n_workspaces);

  if (set_workspace_rows > 0 || set_workspace_cols > 0)
    {
      int token;

      token = wnck_screen_try_set_workspace_layout (screen, 0,
                                                    set_workspace_rows,
                                                    set_workspace_cols);
      if (token)
        wnck_screen_release_workspace_layout (screen, token);
      else
        g_printerr (_("Cannot change the workspace layout on the screen: "
                      "the layout is already owned\n"));
    }

  /* now, the workspace number/layout won't change again */
  if (set_activate_workspace != -1)
    {
      WnckWorkspace *space;
      WnckWorkspace *active_space;

      space = wnck_screen_get_workspace (screen, set_activate_workspace);
      active_space = wnck_screen_get_active_workspace (screen);

      if (space && space != active_space)
        wnck_workspace_activate (space, timestamp);
      else
        g_printerr (_("Workspace %d cannot be activated: "
                      "the workspace does not exist\n"),
                    set_activate_workspace);
    }

  if (set_show_desktop)
    wnck_screen_toggle_showing_desktop (screen, TRUE);
  else if (set_unshow_desktop)
    wnck_screen_toggle_showing_desktop (screen, FALSE);

  if (set_viewport_x != -1 || set_viewport_y != -1)
    {
       WnckWorkspace *active_space;

       active_space = wnck_screen_get_active_workspace (screen);

       if (active_space != NULL)
         {
           if (wnck_workspace_is_virtual (active_space))
             {
               if (set_viewport_x != -1)
                 viewport_x = set_viewport_x;
               else
                 viewport_x = wnck_workspace_get_viewport_x (active_space);

               if (set_viewport_y != -1)
                 viewport_y = set_viewport_y;
               else
                 viewport_y = wnck_workspace_get_viewport_y (active_space);

               wnck_screen_move_viewport (screen, viewport_x, viewport_y);
             }
           else
         g_printerr (_("Viewport cannot be moved: "
                       "the current workspace does not contain a viewport\n"));
         }
       else
         g_printerr (_("Viewport cannot be moved: "
                       "there is no current workspace\n"));
    }
}

static void
update_window_props (WnckWindow *window)
{
  WnckWindowActions        actions;
  WnckWindowMoveResizeMask geometry_mask;
  unsigned int             timestamp;
  // TODO: get a valid timestamp

  actions = wnck_window_get_actions (window);
  timestamp = gdk_x11_display_get_user_time (gdk_display_get_default ());

#define SET_PROPERTY(name, action)                                      \
  if (set_##name)                                                       \
    {                                                                   \
      if (actions & action)                                             \
        wnck_window_##name (window);                                    \
      else                                                              \
        g_printerr (_("Action not allowed\n"));                         \
    }

#define SET_PROPERTY_TIMESTAMP(name, action)                            \
  if (set_##name)                                                       \
    {                                                                   \
      if (actions & action)                                             \
        wnck_window_##name (window, timestamp);                         \
      else                                                              \
        g_printerr (_("Action not allowed\n"));                         \
    }

#define SET_PROPERTY_DUAL(name, action1, action2)                       \
  if (set_##name)                                                       \
    {                                                                   \
      if (actions & action1)                                            \
        wnck_window_##name (window);                                    \
      else                                                              \
        g_printerr (_("Action not allowed\n"));                         \
    }                                                                   \
  else if (set_un##name)                                                \
    {                                                                   \
      if (actions & action2)                                            \
        wnck_window_un##name (window);                                  \
      else                                                              \
        g_printerr (_("Action not allowed\n"));                         \
    }

/* FIXME: why do we have dual & boolean API. This is not consistent! */
#define SET_PROPERTY_BOOLEAN(name, action1, action2)                    \
  if (set_##name)                                                       \
    {                                                                   \
      if (actions & action1)                                            \
        wnck_window_set_##name (window, TRUE);                          \
      else                                                              \
        g_printerr (_("Action not allowed\n"));                         \
    }                                                                   \
  else if (set_un##name)                                                \
    {                                                                   \
      if (actions & action2)                                            \
        wnck_window_set_##name (window, FALSE);                         \
      else                                                              \
        g_printerr (_("Action not allowed\n"));                         \
    }

#define SET_GEOMETRY(param, unset, action, change)                      \
  if (set_##param != unset)                                             \
    {                                                                   \
      if (actions & action)                                             \
        geometry_mask |= change;                                        \
      else                                                              \
        g_printerr (_("Action not allowed\n"));                         \
    }

  SET_PROPERTY (minimize, WNCK_WINDOW_ACTION_MINIMIZE)
  SET_PROPERTY_TIMESTAMP (unminimize, WNCK_WINDOW_ACTION_UNMINIMIZE)

  SET_PROPERTY_DUAL (maximize,
                     WNCK_WINDOW_ACTION_MAXIMIZE,
                     WNCK_WINDOW_ACTION_UNMAXIMIZE)
  SET_PROPERTY_DUAL (maximize_horizontally,
                     WNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY,
                     WNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY)
  SET_PROPERTY_DUAL (maximize_vertically,
                     WNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY,
                     WNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY)

  SET_PROPERTY_BOOLEAN (fullscreen,
                        WNCK_WINDOW_ACTION_FULLSCREEN,
                        WNCK_WINDOW_ACTION_FULLSCREEN)
  SET_PROPERTY_DUAL (make_above,
                     WNCK_WINDOW_ACTION_ABOVE, WNCK_WINDOW_ACTION_ABOVE)
  SET_PROPERTY_DUAL (shade,
                     WNCK_WINDOW_ACTION_SHADE, WNCK_WINDOW_ACTION_UNSHADE)
  SET_PROPERTY_DUAL (stick,
                     WNCK_WINDOW_ACTION_STICK, WNCK_WINDOW_ACTION_UNSTICK)
  SET_PROPERTY_BOOLEAN (skip_pager, actions, actions)
  SET_PROPERTY_BOOLEAN (skip_tasklist, actions, actions)
  SET_PROPERTY_DUAL (pin,
                     WNCK_WINDOW_ACTION_CHANGE_WORKSPACE,
                     WNCK_WINDOW_ACTION_CHANGE_WORKSPACE)

  if (set_workspace != -1)
    {
      if (actions & WNCK_WINDOW_ACTION_CHANGE_WORKSPACE)
        {
           WnckScreen    *screen;
           WnckWorkspace *space;

           screen = wnck_window_get_screen (window);
           space = wnck_screen_get_workspace (screen, set_workspace);
           if (space)
             wnck_window_move_to_workspace (window, space);
           else
             g_printerr (_("Window cannot be moved to workspace %d: "
                           "the workspace does not exist\n"), set_workspace);
        }
      else
        g_printerr (_("Action not allowed\n"));
    }

  /* do activation after the workspace change */
  if (set_activate)
    {
       WnckScreen    *screen;
       WnckWorkspace *space;

       screen = wnck_window_get_screen (window);
       space = wnck_window_get_workspace (window);
       if (space != NULL)
         {
           WnckWorkspace *active_space;

           active_space = wnck_screen_get_active_workspace (screen);
           if (space != active_space)
             wnck_workspace_activate (space, timestamp);
         }

      wnck_window_activate_transient (window, timestamp);
    }

  geometry_mask = 0;
  SET_GEOMETRY (x, G_MAXINT, WNCK_WINDOW_ACTION_MOVE, WNCK_WINDOW_CHANGE_X)
  SET_GEOMETRY (y, G_MAXINT, WNCK_WINDOW_ACTION_MOVE, WNCK_WINDOW_CHANGE_Y)
  SET_GEOMETRY (width, -1, WNCK_WINDOW_ACTION_RESIZE, WNCK_WINDOW_CHANGE_WIDTH)
  SET_GEOMETRY (height, -1, WNCK_WINDOW_ACTION_RESIZE, WNCK_WINDOW_CHANGE_HEIGHT)

  if (geometry_mask != 0)
    wnck_window_set_geometry (window,
                              WNCK_WINDOW_GRAVITY_CURRENT, geometry_mask,
                              set_x, set_y, set_width, set_height);

  if (set_window_type != NULL)
    wnck_window_set_window_type (window, set_window_type_t);

  /* interactive actions at the end */
  SET_PROPERTY (keyboard_move, WNCK_WINDOW_ACTION_MOVE)
  /* FIXME: hack: we should rename the API */
  #define wnck_window_keyboard_resize wnck_window_keyboard_size
  SET_PROPERTY (keyboard_resize, WNCK_WINDOW_ACTION_RESIZE)
  SET_PROPERTY_TIMESTAMP (close, WNCK_WINDOW_ACTION_CLOSE)
}

static void
print_screen_props (WnckScreen *screen)
{
  const char *buf;
  char       *free_buf;

  if (list_workspaces)
    {
      WnckWorkspace *space;
      int            i;

      for (i = 0; i < wnck_screen_get_workspace_count (screen); i++)
        {
          space = wnck_screen_get_workspace (screen, i);
          if (space)
            buf = wnck_workspace_get_name (space);
          else
            buf = _("<unexisting workspace?>");

          /* Translators: %d is a workspace number and %s a workspace name */
          g_print (_("%d: %s\n"), i, buf);
        }
    }
  else if (list_windows)
    {
      WnckWorkspace *space;
      WnckWindow    *window;
      GList         *windows;
      GList         *l;

      if (list_windows_workspace != -1)
        {
          space = wnck_screen_get_workspace (screen, list_windows_workspace);
          if (!space)
            {
              g_printerr (_("Cannot list windows in workspace %d: "
                            "the workspace does not exist\n"),
                          list_windows_workspace);
              return;
            }
        }
      else
        space = NULL;

      windows = wnck_screen_get_windows (screen);

      for (l = windows; l; l = l->next)
        {
          window = WNCK_WINDOW (l->data);

          if (space && (wnck_window_get_workspace (window) != NULL &&
                        wnck_window_get_workspace (window) != space))
            continue;

          if (wnck_window_has_name (window))
            buf = wnck_window_get_name (window);
          else
            buf = _("<name unset>");

          /* Translators: %lu is a window number and %s a window name */
          g_print (_("%lu: %s\n"), wnck_window_get_xid (window), buf);
        }
    }
  else
    {
      WnckWorkspace *space;
      WnckWindow    *window;
#if 0
      WnckLayoutOrientation orientation;
      int            rows;
      int            columns;
#endif

      g_print (_("Screen Number: %d\n"), wnck_screen_get_number (screen));

      g_print (_("Geometry (width, height): %d, %d\n"),
          wnck_screen_get_width (screen), wnck_screen_get_height (screen));

      g_print (_("Number of Workspaces: %d\n"),
          wnck_screen_get_workspace_count (screen));

#if 0
      wnck_screen_get_workspace_layout (screen, &orientation, &rows, &columns,
                                        NULL);
      g_print (_("Workspace Layout (rows, columns, orientation): "
                 "%d, %d, %s\n"),
               rows, columns,
               orientation == WNCK_LAYOUT_ORIENTATION_VERTICAL ? "vertical" :
                                                                 "horizontal");
#endif

      if (wnck_screen_get_window_manager_name (screen) != NULL)
        buf = wnck_screen_get_window_manager_name (screen);
      else
        buf = _("<no EWMH-compliant window manager>");
      g_print (_("Window Manager: %s\n"), buf);

      space = wnck_screen_get_active_workspace (screen);
      if (space)
        /* Translators: %d is a workspace number and %s a workspace name */
        free_buf = g_strdup_printf (_("%d (\"%s\")"),
                                    wnck_workspace_get_number (space),
                                    wnck_workspace_get_name (space));
      else
        free_buf = g_strdup (_("none"));
      g_print (_("Active Workspace: %s\n"), free_buf);
      g_free (free_buf);

      window = wnck_screen_get_active_window (screen);
      if (window)
        {
          char *name;

          if (wnck_window_has_name (window))
            name = g_strdup_printf (_("\"%s\""), wnck_window_get_name (window));
          else
            name = g_strdup (_("<name unset>"));

          /* Translators: %lu is a window number and %s a window name */
          free_buf = g_strdup_printf (_("%lu (%s)"),
                                      wnck_window_get_xid (window), name);
          g_free (name);
        }
      else
        free_buf = g_strdup (_("none"));
      g_print (_("Active Window: %s\n"), free_buf);
      g_free (free_buf);

      g_print (_("Showing the desktop: %s\n"),
          wnck_screen_get_showing_desktop (screen) ? _("true") : _("false"));
    }
}

static void
print_window_props (WnckWindow *window)
{
  WnckWindowType     type;
  int                x, y, w, h;
  WnckClassGroup    *class_group;
  WnckWorkspace     *space;
  WnckScreen        *screen;
  WnckWindowActions  actions;
  const char        *buf;
  char              *free_buf;

  if (wnck_window_has_name (window))
    buf = wnck_window_get_name (window);
  else
    buf = _("<unset>");
  g_print (_("Name: %s\n"), buf);

  if (wnck_window_has_icon_name (window))
    buf = wnck_window_get_icon_name (window);
  else
    buf = _("<unset>");
  /* Translators: note that "Icon" here has a specific window
   * management-related meaning. It means minimized. */
  g_print (_("Icon Name: %s\n"), buf);

  if (!wnck_window_get_icon_is_fallback (window))
    buf = _("set");
  else
    buf = _("<unset>");
  g_print (_("Icons: %s\n"), buf);

  space = wnck_window_get_workspace (window);
  if (space)
    /* Translators: %d is a workspace number and %s a workspace name */
    free_buf = g_strdup_printf (_("%d (\"%s\")"),
                                wnck_workspace_get_number (space),
                                wnck_workspace_get_name (space));
  else if (wnck_window_is_pinned (window))
    free_buf = g_strdup (_("all workspaces"));
  else
    free_buf = g_strdup (_("none"));
  g_print (_("On Workspace: %s\n"), free_buf);
  g_free (free_buf);

  screen = wnck_window_get_screen (window);
  if (wnck_screen_get_window_manager_name (screen) != NULL)
    buf = wnck_screen_get_window_manager_name (screen);
  else
    buf = _("<no EWMH-compliant window manager>");
  g_print (_("On Screen: %d (Window Manager: %s)\n"),
           wnck_screen_get_number (screen), buf);

  type = wnck_window_get_window_type (window);
  switch (type)
    {
      case WNCK_WINDOW_NORMAL:
        buf = _("normal window");
        break;
      case WNCK_WINDOW_DESKTOP:
        buf = _("desktop");
        break;
      case WNCK_WINDOW_DOCK:
        buf = _("dock or panel");
        break;
      case WNCK_WINDOW_DIALOG:
        buf = _("dialog window");
        break;
      case WNCK_WINDOW_TOOLBAR:
        buf = _("tearoff toolbar");
        break;
      case WNCK_WINDOW_MENU:
        buf = _("tearoff menu");
        break;
      case WNCK_WINDOW_UTILITY:
        buf = _("utility window");
        break;
      case WNCK_WINDOW_SPLASHSCREEN:
        buf = _("splash screen");
        break;
      default:
        g_assert_not_reached ();
    }
  g_print (_("Window Type: %s\n"), buf);

  wnck_window_get_geometry (window, &x, &y, &w, &h);
  g_print (_("Geometry (x, y, width, height): %d, %d, %d, %d\n"), x, y, w, h);

  class_group = wnck_window_get_class_group (window);
  if (strcmp (wnck_class_group_get_res_class (class_group), ""))
    buf = wnck_class_group_get_res_class (class_group);
  else
    buf = _("<unset>");
  g_print (_("Class Group: %s\n"), buf);

  g_print (_("XID: %lu\n"), wnck_window_get_xid (window));

  if (wnck_window_get_pid (window) != 0)
    free_buf = g_strdup_printf ("%d", wnck_window_get_pid (window));
  else
    free_buf = g_strdup (_("<unset>"));
  g_print (_("PID: %s\n"), free_buf);
  g_free (free_buf);

  if (wnck_window_get_session_id (window))
    buf = wnck_window_get_session_id (window);
  else
    buf = _("<unset>");
  g_print (_("Session ID: %s\n"), buf);

  if (wnck_window_get_group_leader (window) != wnck_window_get_xid (window))
    g_print (_("Group Leader: %lu\n"), wnck_window_get_group_leader (window));

  if (wnck_window_get_transient (window))
    g_print (_("Transient for: %lu\n"),
             wnck_window_get_xid (wnck_window_get_transient (window)));

#define PRINT_LIST_ITEM(func, string)                           \
  if (func (window))                                            \
    {                                                           \
      char *tmp;                                                \
                                                                \
      /* Translators: we're building a list of items here.      \
       * The end result is something like "a, b, c"             \
       * In this case, the first string is "a, b", the second   \
       * string is ", " and the third string is "c" */          \
      tmp = g_strdup_printf (_("%1$s%2$s%3$s"),                 \
                             free_buf ? free_buf : "",          \
                             free_buf ? _(", ") : "",           \
                             string);                           \
      g_free (free_buf);                                        \
      free_buf = tmp;                                           \
    }
  free_buf = NULL;
  PRINT_LIST_ITEM (wnck_window_is_minimized, _("minimized"));
  PRINT_LIST_ITEM (wnck_window_is_maximized, _("maximized"));
  if (!wnck_window_is_maximized (window))
    {
      PRINT_LIST_ITEM (wnck_window_is_maximized_horizontally,
                       _("maximized horizontally"));
      PRINT_LIST_ITEM (wnck_window_is_maximized_vertically,
                       _("maximized vertically"));
    }
  PRINT_LIST_ITEM (wnck_window_is_shaded, _("shaded"));
  PRINT_LIST_ITEM (wnck_window_is_pinned, _("pinned"));
  PRINT_LIST_ITEM (wnck_window_is_sticky, _("sticky"));
  PRINT_LIST_ITEM (wnck_window_is_above, _("above"));
  PRINT_LIST_ITEM (wnck_window_is_fullscreen, _("fullscreen"));
  PRINT_LIST_ITEM (wnck_window_needs_attention, _("needs attention"));
  PRINT_LIST_ITEM (wnck_window_is_skip_pager, _("skip pager"));
  PRINT_LIST_ITEM (wnck_window_is_skip_tasklist, _("skip tasklist"));
  if (!free_buf)
    free_buf = g_strdup (_("normal"));
  g_print (_("State: %s\n"), free_buf);
  g_free (free_buf);

#define PRINT_FLAGS_ITEM(bitmask, flag, string)                 \
  if (bitmask & flag)                                           \
    {                                                           \
      char *tmp;                                                \
                                                                \
      /* Translators: we're building a list of items here.      \
       * The end result is something like "a, b, c"             \
       * In this case, the first string is "a, b", the second   \
       * string is ", " and the third string is "c" */          \
      tmp = g_strdup_printf (_("%1$s%2$s%3$s"),                 \
                             free_buf ? free_buf : "",          \
                             free_buf ? _(", ") : "",           \
                             string);                           \
      g_free (free_buf);                                        \
      free_buf = tmp;                                           \
    }
  free_buf = NULL;
  actions = wnck_window_get_actions (window);
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_MOVE, _("move"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_RESIZE, _("resize"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_SHADE, _("shade"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_UNSHADE, _("unshade"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_STICK, _("stick"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_UNSTICK, _("unstick"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY,
                    _("maximize horizontally"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY,
                    _("unmaximize horizontally"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY,
                    _("maximize vertically"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY,
                    _("unmaximize vertically"));
  /* we're calling PRINT_FLAGS_ITEM() three times for l10n reasons */
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_CHANGE_WORKSPACE,
                    _("change workspace"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_CHANGE_WORKSPACE,
                    _("pin"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_CHANGE_WORKSPACE,
                    _("unpin"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_MINIMIZE, _("minimize"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_UNMINIMIZE, _("unminimize"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_MAXIMIZE, _("maximize"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_UNMAXIMIZE, _("unmaximize"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_FULLSCREEN,
                    _("change fullscreen mode"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_CLOSE, _("close"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_ABOVE,
                    _("make above"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_ABOVE,
                    _("unmake above"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_BELOW,
                    _("make below"));
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_BELOW,
                    _("unmake below"));
  if (!free_buf)
    free_buf = g_strdup (_("no action possible"));
  g_print (_("Possible Actions: %s\n"), free_buf);
  g_free (free_buf);

}

static gboolean
wm_state_set (Window window)
{
  Atom    wm_state;
  gulong  nitems;
  gulong  bytes_after;
  gulong *prop;
  Atom    ret_type = None;
  int     ret_format;
  int     err, result;

  wm_state = gdk_x11_get_xatom_by_name ("WM_STATE");

  gdk_error_trap_push ();
  result = XGetWindowProperty (gdk_display,
                               window,
                               wm_state,
                               0, G_MAXLONG,
                               False, wm_state, &ret_type, &ret_format, &nitems,
                               &bytes_after, (gpointer) &prop);
  err = gdk_error_trap_pop ();
  if (err != Success ||
      result != Success)
    return FALSE;

  XFree (prop);

  if (ret_type != wm_state)
    return FALSE;

  return TRUE;
}

static WnckWindow *
find_managed_window (Window window)
{
  Window      root;
  Window      parent;
  Window     *kids = NULL;
  WnckWindow *retval;
  guint       nkids;
  int         i, result;

  if (wm_state_set (window))
    return wnck_window_get (window);

  gdk_error_trap_push ();
  result = XQueryTree (gdk_display, window, &root, &parent, &kids, &nkids);
  if (gdk_error_trap_pop () || !result)
    return NULL;

  retval = NULL;

  for (i = 0; i < nkids; i++)
    {
      if (wm_state_set (kids [i]))
        {
          retval = wnck_window_get (kids [i]);
          break;
        }

      retval = find_managed_window (kids [i]);
      if (retval != NULL)
        break;
    }

  if (kids)
    XFree (kids);

  return retval;
}

static void 
handle_button_press_event (XKeyEvent *event)
{
  WnckWindow *window;

  if (event->subwindow == None)
    return;

  window = find_managed_window (event->subwindow);

  if (window)
    {
      if (window_props)
        update_window_props (window);
      else
        print_window_props (window);
    }
}

static GdkFilterReturn
target_filter (GdkXEvent *gdk_xevent,
               GdkEvent  *event,
               gpointer   data)
{
  XEvent *xevent = (XEvent *) gdk_xevent;

  switch (xevent->type)
    {
      case ButtonPress:
        handle_button_press_event (&xevent->xkey);
        clean_up ();
        return GDK_FILTER_REMOVE;
      case KeyPress:
        if (xevent->xkey.keycode == XKeysymToKeycode (gdk_display, XK_Escape))
          {
            clean_up ();
            return GDK_FILTER_REMOVE;
          }
        break;
      default:
        break;
    }

  return GDK_FILTER_CONTINUE;
}

static gboolean
get_target (gpointer data)
{
  GdkGrabStatus  status;
  GdkCursor     *cross;
  GdkWindow     *root;

  root = gdk_get_default_root_window ();

  gdk_window_add_filter (root, (GdkFilterFunc) target_filter, NULL);

  cross = gdk_cursor_new (GDK_CROSS);
  status = gdk_pointer_grab (root, FALSE, GDK_BUTTON_PRESS_MASK,
                             NULL, cross, GDK_CURRENT_TIME);
  gdk_cursor_unref (cross);

  if (status != GDK_GRAB_SUCCESS)
    {
      g_warning ("Pointer grab failed.\n");
      clean_up ();
      return FALSE;
    }

  status = gdk_keyboard_grab (root, FALSE, GDK_CURRENT_TIME);
  if (status != GDK_GRAB_SUCCESS)
    {
      g_warning ("Keyboard grab failed.\n");
      clean_up ();
      return FALSE;
    }

  gdk_flush ();

  return FALSE;
}

static void
clean_up (void)
{
  GdkWindow *root;

  root = gdk_get_default_root_window ();
  gdk_window_remove_filter (root, (GdkFilterFunc) target_filter, NULL);

  gdk_pointer_ungrab (GDK_CURRENT_TIME);
  gdk_keyboard_ungrab (GDK_CURRENT_TIME);

  gtk_main_quit ();
}

int
main (int argc, char **argv)
{
  GOptionContext *ctxt;
  GOptionGroup   *group;
  GError         *error;
  WnckScreen     *screen;
  
  bindtextdomain (GETTEXT_PACKAGE, WNCK_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  ctxt = g_option_context_new ("");
  g_option_context_set_summary (ctxt,
                                N_("Print or modify the properties of a "
                                   "screen or a window, or interact with a "
                                   "screen or a window, "
                                   "following the EWMH specification.\n"
                                   "For information about this specification, "
                                   "see:\n\t"
                                   "http://freedesktop.org/wiki/Specifications/wm-spec"));

  g_option_context_add_main_entries (ctxt, main_entries, GETTEXT_PACKAGE);

  group = g_option_group_new ("list",
                              N_("Options to list windows or workspaces"),
                              N_("Show options to list windows or workspaces"),
                              NULL, NULL);
  g_option_group_add_entries (group, list_entries);
  g_option_context_add_group (ctxt, group);

  group = g_option_group_new ("window",
                              N_("Options to modify properties of a window"),
                              N_("Show options to modify properties of a window"),
                              NULL, NULL);
  g_option_group_add_entries (group, window_entries);
  g_option_context_add_group (ctxt, group);

  group = g_option_group_new ("screen",
                              N_("Options to modify properties of a screen"),
                              N_("Show options to modify properties of a screen"),
                              NULL, NULL);
  g_option_group_add_entries (group, screen_entries);
  g_option_context_add_group (ctxt, group);

  g_option_context_add_group (ctxt, gtk_get_option_group (TRUE));

  error = NULL;
  if (!g_option_context_parse (ctxt, &argc, &argv, &error))
    {
      g_printerr (_("Error while parsing arguments: %s\n"), error->message);
      g_option_context_free (ctxt);
      g_error_free (error);
      return 1;
    }

  g_option_context_free (ctxt);
  ctxt = NULL;

  if (!validate_options ())
    return 1;

  gtk_init (&argc, &argv);

  wnck_set_client_type (WNCK_CLIENT_TYPE_PAGER);

  if (interact_screen >= 0)
    {
      screen = wnck_screen_get (interact_screen);
      if (!screen)
        {
         g_printerr (_("Cannot interact with screen %d: "
                       "the screen does not exist\n"), interact_screen);
          return 0;
        }
    }
  else
    screen = wnck_screen_get_default ();

  /* because we don't respond to signals at the moment */
  wnck_screen_force_update (screen);
  
  if (screen_mode)
    {
      if (screen_props)
        update_screen_props (screen);
      else
        print_screen_props (screen);
    }
  else
    {
      if (xid != 0)
        {
          WnckWindow *window;

          window = wnck_window_get (xid);
          if (window)
            {
              if (window_props)
                update_window_props (window);
              else
                print_window_props (window);
            }
          else
            g_printerr ("Cannot interact with window with XID %lu: "
                        "the window cannot be found\n", xid);
        }
      else
        {
          g_idle_add (get_target, NULL);
          
          gtk_main ();
        }
    }
  
  return 0;
}
