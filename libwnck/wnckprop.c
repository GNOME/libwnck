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
 * is GPL. Thus, this program is licensed under the GPL.
 */

#include <string.h>
#include <X11/Xlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <glib/gi18n.h>

#include <libwnck/libwnck.h>
#include <libwnck/class-group.h>

gulong xid = 0;

static GOptionEntry entries[] = {
	{"xid", 'x', 0, G_OPTION_ARG_INT, &xid, N_("X window ID of the window to examine"), N_("XID")},
	{ NULL }
};

static void clean_up (void);

static void
print_props (WnckWindow *window)
{
  gboolean           need_comma;
  WnckWindowType     type;
  int                x, y, w, h;
  WnckClassGroup    *class_group;
  WnckWorkspace     *space;
  WnckWindowActions  actions;

  g_print ("Name: ");
  if (wnck_window_has_name (window))
    g_print ("%s\n", wnck_window_get_name (window));
  else
    g_print ("<unset>\n");

  g_print ("Icon Name: ");
  if (wnck_window_has_icon_name (window))
    g_print ("%s\n", wnck_window_get_icon_name (window));
  else
    g_print ("<unset>\n");

  g_print ("Icons: ");
  if (!wnck_window_get_icon_is_fallback (window))
    g_print ("set\n");
  else
    g_print ("<unset>\n");

  space = wnck_window_get_workspace (window);
  g_print ("On Workspace: ");
  if (space)
    g_print ("%d (\"%s\")\n", wnck_workspace_get_number (space),
             wnck_workspace_get_name (space));
  else if (wnck_window_is_pinned (window))
      g_print ("all workspaces\n");
  else
      g_print ("none\n");

  type = wnck_window_get_window_type (window);
  g_print ("Window Type: ");
  switch (type)
    {
      case WNCK_WINDOW_NORMAL:
        g_print ("normal window\n");
        break;
      case WNCK_WINDOW_DESKTOP:
        g_print ("desktop\n");
        break;
      case WNCK_WINDOW_DOCK:
        g_print ("dock or panel\n");
        break;
      case WNCK_WINDOW_DIALOG:
        g_print ("dialog window\n");
        break;
      case WNCK_WINDOW_TOOLBAR:
        g_print ("tearoff toolbar\n");
        break;
      case WNCK_WINDOW_MENU:
        g_print ("tearoff menu\n");
        break;
      case WNCK_WINDOW_UTILITY:
        g_print ("utility window\n");
        break;
      case WNCK_WINDOW_SPLASHSCREEN:
        g_print ("splash screen\n");
        break;
    }

  wnck_window_get_geometry (window, &x, &y, &w, &h);
  g_print ("Geometry (x, y, width, height): %d, %d, %d, %d\n", x, y, w, h);

  class_group = wnck_window_get_class_group (window);
  g_print ("Class Group: ");
  if (strcmp (wnck_class_group_get_res_class (class_group), ""))
    g_print ("%s\n", wnck_class_group_get_res_class (class_group));
  else
    g_print ("<unset>\n");

  g_print ("XID: %lu\n", wnck_window_get_xid (window));

  g_print ("PID: ");
  if (wnck_window_get_pid (window) != 0)
    g_print ("%d\n", wnck_window_get_pid (window));
  else
    g_print ("<unset>\n");

  g_print ("Session ID: ");
  if (wnck_window_get_session_id (window))
    g_print ("%s\n", wnck_window_get_session_id (window));
  else
    g_print ("<unset>\n");

  if (wnck_window_get_group_leader (window) != wnck_window_get_xid (window))
    g_print ("Group Leader: %lu\n", wnck_window_get_group_leader (window));

  if (wnck_window_get_transient (window))
    g_print ("Transient for: %lu\n",
             wnck_window_get_xid (wnck_window_get_transient (window)));

#define PRINT_LIST_ITEM(func, string)                           \
  if (func (window))                                            \
    {                                                           \
      g_print ("%s%s", need_comma ? ", " : "", string);       \
      need_comma = TRUE;                                        \
    }
  need_comma = FALSE;
  g_print ("State: ");
  PRINT_LIST_ITEM (wnck_window_is_minimized, "minimized");
  PRINT_LIST_ITEM (wnck_window_is_maximized, "maximized");
  if (!wnck_window_is_maximized (window))
    {
      PRINT_LIST_ITEM (wnck_window_is_maximized_horizontally,
                       "maximized horizontally");
      PRINT_LIST_ITEM (wnck_window_is_maximized_vertically,
                       "maximized vertically");
    }
  PRINT_LIST_ITEM (wnck_window_is_shaded, "shaded");
  PRINT_LIST_ITEM (wnck_window_is_pinned, "pinned");
  PRINT_LIST_ITEM (wnck_window_is_sticky, "sticky");
  PRINT_LIST_ITEM (wnck_window_is_above, "above");
  PRINT_LIST_ITEM (wnck_window_is_fullscreen, "fullscreen");
  PRINT_LIST_ITEM (wnck_window_needs_attention, "needs attention");
  PRINT_LIST_ITEM (wnck_window_is_skip_pager, "skip pager");
  PRINT_LIST_ITEM (wnck_window_is_skip_tasklist, "skip tasklist");
  if (!need_comma)
    g_print ("normal");
  g_print ("\n");

#define PRINT_FLAGS_ITEM(bitmask, flag, string)                 \
  if (bitmask & flag)                                           \
    {                                                           \
      g_print ("%s%s", need_comma ? ", " : "", string);         \
      need_comma = TRUE;                                        \
    }
  need_comma = FALSE;
  g_print ("Possible Actions: ");
  actions = wnck_window_get_actions (window);
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_MOVE, "move");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_RESIZE, "resize");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_SHADE, "shade");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_UNSHADE, "unshade");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_STICK, "stick");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_UNSTICK, "unstick");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY,
                    "maximize horizontally");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY,
                    "unmaximize horizontally");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY,
                    "maximize vertically");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY,
                    "unmaximize vertically");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_CHANGE_WORKSPACE,
                    "change workspace, pin, unpin");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_MINIMIZE, "minimize");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_UNMINIMIZE, "unminimize");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_MAXIMIZE, "maximize");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_UNMAXIMIZE, "unmaximize");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_FULLSCREEN,
                    "change fullscreen mode");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_CLOSE, "close");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_ABOVE,
                    "make above, unmake above");
  PRINT_FLAGS_ITEM (actions, WNCK_WINDOW_ACTION_BELOW,
                    "make below, unmake below");
  if (!need_comma)
    g_print ("no action possible");
  g_print ("\n");

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
    print_props (window);
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
  WnckScreen     *screen;
  
  ctxt = g_option_context_new ("");
  g_option_context_add_main_entries (ctxt, entries, NULL);
  g_option_context_add_group (ctxt, gtk_get_option_group (TRUE));
  g_option_context_parse (ctxt, &argc, &argv, NULL);
  g_option_context_free (ctxt);
  ctxt = NULL;

  gtk_init (&argc, &argv);

  screen = wnck_screen_get_default ();

  /* because we don't respond to signals at the moment */
  wnck_screen_force_update (screen);
  
  if (xid != 0)
    {
      WnckWindow *window;

      window = wnck_window_get (xid);
      if (window)
        print_props (window);
      else
        g_print ("Cannot find window with XID %lu.\n", xid);
    }
  else
    {
      g_idle_add (get_target, NULL);
      
      gtk_main ();
    }
  
  return 0;
}
