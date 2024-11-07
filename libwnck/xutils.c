/* Xlib utils */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2005-2007 Vincent Untz
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "xutils.h"
#include <string.h>
#include <stdio.h>
#include <cairo-xlib.h>
#if HAVE_CAIRO_XLIB_XRENDER
#include <cairo-xlib-xrender.h>
#endif
#ifdef HAVE_XRES
#include <X11/extensions/XRes.h>
#endif
#include "screen.h"
#include "window.h"
#include "private.h"
#include "wnck-handle-private.h"

gboolean
_wnck_get_cardinal (Screen *screen,
                    Window  xwindow,
                    Atom    atom,
                    int    *val)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gulong *num;
  int err, result;

  display = DisplayOfScreen (screen);

  *val = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_CARDINAL, &type, &format, &nitems,
			       &bytes_after, (void*)&num);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_CARDINAL)
    {
      XFree (num);
      return FALSE;
    }

  *val = *num;

  XFree (num);

  return TRUE;
}

int
_wnck_get_wm_state (Screen *screen,
                    Window  xwindow)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gulong *num;
  int err, result;
  Atom wm_state;
  int retval;

  display = DisplayOfScreen (screen);

  wm_state = _wnck_atom_get ("WM_STATE");
  retval = NormalState;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       wm_state,
			       0, G_MAXLONG,
			       False, wm_state, &type, &format, &nitems,
			       &bytes_after, (void*)&num);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return retval;

  if (type != wm_state)
    {
      XFree (num);
      return retval;
    }

  retval = *num;

  XFree (num);

  return retval;
}

gboolean
_wnck_get_window (Screen *screen,
                  Window  xwindow,
                  Atom    atom,
                  Window *val)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Window *w;
  int err, result;

  display = DisplayOfScreen (screen);

  *val = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_WINDOW, &type, &format, &nitems,
			       &bytes_after, (void*)&w);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_WINDOW)
    {
      XFree (w);
      return FALSE;
    }

  *val = *w;

  XFree (w);

  return TRUE;
}

gboolean
_wnck_get_pixmap (Screen *screen,
                  Window  xwindow,
                  Atom    atom,
                  Pixmap *val)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Window *w;
  int err, result;

  display = DisplayOfScreen (screen);

  *val = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_PIXMAP, &type, &format, &nitems,
			       &bytes_after, (void*)&w);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_PIXMAP)
    {
      XFree (w);
      return FALSE;
    }

  *val = *w;

  XFree (w);

  return TRUE;
}

gboolean
_wnck_get_atom (Screen *screen,
                Window  xwindow,
                Atom    atom,
                Atom   *val)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Atom *a;
  int err, result;

  display = DisplayOfScreen (screen);

  *val = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_ATOM, &type, &format, &nitems,
			       &bytes_after, (void*)&a);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_ATOM)
    {
      XFree (a);
      return FALSE;
    }

  *val = *a;

  XFree (a);

  return TRUE;
}

static char*
text_property_to_utf8 (Display             *display,
                       const XTextProperty *prop)
{
  GdkDisplay *gdkdisplay;
  char **list;
  int count;
  char *retval;

  list = NULL;

  gdkdisplay = _wnck_gdk_display_lookup_from_display (display);
  if (!gdkdisplay)
    return NULL;

  count = gdk_text_property_to_utf8_list_for_display (gdkdisplay,
                                          gdk_x11_xatom_to_atom (prop->encoding),
                                          prop->format,
                                          prop->value,
                                          prop->nitems,
                                          &list);

  if (count == 0)
    retval = NULL;
  else
    {
      retval = list[0];
      list[0] = g_strdup (""); /* something to free */
    }

  g_strfreev (list);

  return retval;
}

char*
_wnck_get_text_property (Screen *screen,
                         Window  xwindow,
                         Atom    atom)
{
  Display *display;
  XTextProperty text;
  char *retval;

  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);

  text.nitems = 0;
  if (XGetTextProperty (display,
                        xwindow,
                        &text,
                        atom))
    {
      retval = text_property_to_utf8 (display, &text);

      if (text.value)
        XFree (text.value);
    }
  else
    {
      retval = NULL;
    }

  _wnck_error_trap_pop (display);

  return retval;
}

static char*
_wnck_get_string_property_latin1 (Screen *screen,
                                  Window  xwindow,
                                  Atom    atom)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gchar *str;
  int err, result;
  char *retval;

  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);
  str = NULL;
  result = XGetWindowProperty (display,
			       xwindow, atom,
			       0, G_MAXLONG,
			       False, XA_STRING, &type, &format, &nitems,
			       &bytes_after, (guchar **)&str);

  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return NULL;

  if (type != XA_STRING)
    {
      XFree (str);
      return NULL;
    }

  retval = g_strdup (str);

  XFree (str);

  return retval;
}

char*
_wnck_get_utf8_property (Screen *screen,
                         Window  xwindow,
                         Atom    atom)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gchar *val;
  int err, result;
  char *retval;
  Atom utf8_string;

  display = DisplayOfScreen (screen);

  utf8_string = _wnck_atom_get ("UTF8_STRING");

  _wnck_error_trap_push (display);
  type = None;
  val = NULL;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, utf8_string,
			       &type, &format, &nitems,
			       &bytes_after, (guchar **)&val);
  err = _wnck_error_trap_pop (display);

  if (err != Success ||
      result != Success)
    return NULL;

  if (type != utf8_string ||
      format != 8 ||
      nitems == 0)
    {
      if (val)
        XFree (val);
      return NULL;
    }

  if (!g_utf8_validate (val, nitems, NULL))
    {
      g_warning ("Property %s contained invalid UTF-8\n",
                 _wnck_atom_name (atom));
      XFree (val);
      return NULL;
    }

  retval = g_strndup (val, nitems);

  XFree (val);

  return retval;
}

gboolean
_wnck_get_window_list (Screen  *screen,
                       Window   xwindow,
                       Atom     atom,
                       Window **windows,
                       int     *len)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Window *data;
  int err, result;

  display = DisplayOfScreen (screen);

  *windows = NULL;
  *len = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_WINDOW, &type, &format, &nitems,
			       &bytes_after, (void*)&data);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_WINDOW)
    {
      XFree (data);
      return FALSE;
    }

  *windows = g_new (Window, nitems);
  memcpy (*windows, data, sizeof (Window) * nitems);
  *len = nitems;

  XFree (data);

  return TRUE;
}

gboolean
_wnck_get_atom_list (Screen  *screen,
                     Window   xwindow,
                     Atom     atom,
                     Atom   **atoms,
                     int     *len)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Atom *data;
  int err, result;

  display = DisplayOfScreen (screen);

  *atoms = NULL;
  *len = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_ATOM, &type, &format, &nitems,
			       &bytes_after, (void*)&data);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_ATOM)
    {
      XFree (data);
      return FALSE;
    }

  if (nitems != 0)
    {
      *atoms = g_new (Atom, nitems);
      memcpy (*atoms, data, sizeof (Atom) * nitems);
      *len = nitems;
    }

  XFree (data);

  return TRUE;
}

gboolean
_wnck_get_cardinal_list (Screen  *screen,
                         Window   xwindow,
                         Atom     atom,
                         gulong **cardinals,
                         int     *len)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gulong *nums;
  int err, result;

  display = DisplayOfScreen (screen);

  *cardinals = NULL;
  *len = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_CARDINAL, &type, &format, &nitems,
			       &bytes_after, (void*)&nums);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_CARDINAL)
    {
      XFree (nums);
      return FALSE;
    }

  *cardinals = g_new (gulong, nitems);
  memcpy (*cardinals, nums, sizeof (gulong) * nitems);
  *len = nitems;

  XFree (nums);

  return TRUE;
}

char**
_wnck_get_utf8_list (Screen *screen,
                     Window  xwindow,
                     Atom    atom)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  char *val;
  int err, result;
  Atom utf8_string;
  char **retval;
  guint i;
  guint n_strings;
  char *p;

  display = DisplayOfScreen (screen);

  utf8_string = _wnck_atom_get ("UTF8_STRING");

  _wnck_error_trap_push (display);
  type = None;
  val = NULL;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, utf8_string,
			       &type, &format, &nitems,
			       &bytes_after, (void*)&val);
  err = _wnck_error_trap_pop (display);

  if (err != Success ||
      result != Success)
    return NULL;

  if (type != utf8_string ||
      format != 8 ||
      nitems == 0)
    {
      if (val)
        XFree (val);
      return NULL;
    }

  /* I'm not sure this is right, but I'm guessing the
   * property is nul-separated
   */
  i = 0;
  n_strings = 0;
  while (i < nitems)
    {
      if (val[i] == '\0')
        ++n_strings;
      ++i;
    }

  if (val[nitems - 1] != '\0')
    ++n_strings;

  /* we're guaranteed that val has a nul on the end
   * by XGetWindowProperty
   */

  retval = g_new0 (char*, n_strings + 1);

  p = val;
  i = 0;
  while (i < n_strings)
    {
      if (!g_utf8_validate (p, -1, NULL))
        {
          g_warning ("Property %s contained invalid UTF-8\n",
                     _wnck_atom_name (atom));
          XFree (val);
          g_strfreev (retval);
          return NULL;
        }

      retval[i] = g_strdup (p);

      p = p + strlen (p) + 1;
      ++i;
    }

  XFree (val);

  return retval;
}

void
_wnck_set_utf8_list (Screen  *screen,
                     Window   xwindow,
                     Atom     atom,
                     char   **list)
{
  Display *display;
  Atom utf8_string;
  GString *flattened;
  int i;

  display = DisplayOfScreen (screen);

  utf8_string = _wnck_atom_get ("UTF8_STRING");

  /* flatten to nul-separated list */
  flattened = g_string_new ("");
  i = 0;
  while (list[i] != NULL)
    {
      g_string_append_len (flattened, list[i],
                           strlen (list[i]) + 1);
      ++i;
    }

  _wnck_error_trap_push (display);

  XChangeProperty (display,
		   xwindow,
                   atom,
		   utf8_string, 8, PropModeReplace,
		   (guchar *) flattened->str, flattened->len);

  _wnck_error_trap_pop (display);

  g_string_free (flattened, TRUE);
}

void
_wnck_error_trap_push (Display *display)
{
  GdkDisplay *gdk_display;

  gdk_display = gdk_x11_lookup_xdisplay (display);
  g_assert (gdk_display != NULL);

  gdk_x11_display_error_trap_push (gdk_display);
}

int
_wnck_error_trap_pop (Display *display)
{
  GdkDisplay *gdk_display;

  gdk_display = gdk_x11_lookup_xdisplay (display);
  g_assert (gdk_display != NULL);

  gdk_display_flush (gdk_display);
  return gdk_x11_display_error_trap_pop (gdk_display);
}

int
_wnck_xid_equal (gconstpointer v1,
                 gconstpointer v2)
{
  return *((const gulong*) v1) == *((const gulong*) v2);
}

guint
_wnck_xid_hash (gconstpointer v)
{
  gulong val = * (const gulong *) v;

  /* I'm not sure this works so well. */
#if GLIB_SIZEOF_LONG > 4
  return (guint) (val ^ (val >> 32));
#else
  return val;
#endif
}

void
_wnck_iconify (Screen *screen,
               Window  xwindow)
{
  Display *display;

  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);
  XIconifyWindow (display, xwindow, DefaultScreen (display));
  _wnck_error_trap_pop (display);
}

void
_wnck_deiconify (Screen *screen,
                 Window  xwindow)
{
  /* We need special precautions, because GDK doesn't like
   * XMapWindow() called on its windows, need to use the
   * GDK functions
   */
  Display   *display;
  GdkWindow *gdkwindow;

  display = DisplayOfScreen (screen);
  gdkwindow = _wnck_gdk_window_lookup_from_window (screen, xwindow);

  _wnck_error_trap_push (display);
  if (gdkwindow)
    gdk_window_show (gdkwindow);
  else
    XMapRaised (display, xwindow);
  _wnck_error_trap_pop (display);
}

void
_wnck_close (WnckScreen *screen,
             Window      xwindow,
             Time        timestamp)
{
  WnckHandle *handle;
  Screen *xscreen;
  Display *display;
  Window root;
  XEvent xev;

  handle = wnck_screen_get_handle (screen);
  xscreen = _wnck_screen_get_xscreen (screen);
  display = DisplayOfScreen (xscreen);
  root = RootWindowOfScreen (xscreen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_CLOSE_WINDOW");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = timestamp;
  xev.xclient.data.l[1] = _wnck_handle_get_client_type (handle);
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  _wnck_error_trap_push (display);
  XSendEvent (display,
              root,
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
  _wnck_error_trap_pop (display);
}

#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10

void
_wnck_keyboard_move (WnckScreen *screen,
                     Window      xwindow)
{
  WnckHandle *handle;
  Screen *xscreen;
  Display *display;
  Window root;
  XEvent xev;

  handle = wnck_screen_get_handle (screen);
  xscreen = _wnck_screen_get_xscreen (screen);
  display = DisplayOfScreen (xscreen);
  root = RootWindowOfScreen (xscreen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_WM_MOVERESIZE");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = 0; /* unused */
  xev.xclient.data.l[1] = 0; /* unused */
  xev.xclient.data.l[2] = _NET_WM_MOVERESIZE_MOVE_KEYBOARD;
  xev.xclient.data.l[3] = 0; /* unused */
  xev.xclient.data.l[4] = _wnck_handle_get_client_type (handle);

  _wnck_error_trap_push (display);
  XSendEvent (display,
              root,
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_keyboard_size (WnckScreen *screen,
                     Window      xwindow)
{
  WnckHandle *handle;
  Screen *xscreen;
  Display *display;
  Window root;
  XEvent xev;

  handle = wnck_screen_get_handle (screen);
  xscreen = _wnck_screen_get_xscreen (screen);
  display = DisplayOfScreen (xscreen);
  root = RootWindowOfScreen (xscreen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_WM_MOVERESIZE");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = 0; /* unused */
  xev.xclient.data.l[1] = 0; /* unused */
  xev.xclient.data.l[2] = _NET_WM_MOVERESIZE_SIZE_KEYBOARD;
  xev.xclient.data.l[3] = 0; /* unused */
  xev.xclient.data.l[4] = _wnck_handle_get_client_type (handle);

  _wnck_error_trap_push (display);
  XSendEvent (display,
              root,
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_change_state (WnckScreen *screen,
                    Window      xwindow,
                    gboolean    add,
                    Atom        state1,
                    Atom        state2)
{
  WnckHandle *handle;
  Screen *xscreen;
  Display *display;
  Window root;
  XEvent xev;

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

  handle = wnck_screen_get_handle (screen);
  xscreen = _wnck_screen_get_xscreen (screen);
  display = DisplayOfScreen (xscreen);
  root = RootWindowOfScreen (xscreen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_WM_STATE");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
  xev.xclient.data.l[1] = state1;
  xev.xclient.data.l[2] = state2;
  xev.xclient.data.l[3] = _wnck_handle_get_client_type (handle);
  xev.xclient.data.l[4] = 0;

  _wnck_error_trap_push (display);
  XSendEvent (display,
	      root,
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_change_workspace (WnckScreen *screen,
                        Window      xwindow,
                        int         new_space)
{
  WnckHandle *handle;
  Screen *xscreen;
  Display *display;
  Window root;
  XEvent xev;

  handle = wnck_screen_get_handle (screen);
  xscreen = _wnck_screen_get_xscreen (screen);
  display = DisplayOfScreen (xscreen);
  root = RootWindowOfScreen (xscreen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_WM_DESKTOP");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = new_space;
  xev.xclient.data.l[1] = _wnck_handle_get_client_type (handle);
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  _wnck_error_trap_push (display);
  XSendEvent (display,
	      root,
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_activate (WnckScreen *screen,
                Window      xwindow,
                Time        timestamp)
{
  WnckHandle *handle;
  Screen *xscreen;
  Display *display;
  Window root;
  XEvent xev;

  if (timestamp == 0)
    g_warning ("Received a timestamp of 0; window activation may not "
               "function properly.\n");

  handle = wnck_screen_get_handle (screen);
  xscreen = _wnck_screen_get_xscreen (screen);
  display = DisplayOfScreen (xscreen);
  root = RootWindowOfScreen (xscreen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_ACTIVE_WINDOW");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = _wnck_handle_get_client_type (handle);
  xev.xclient.data.l[1] = timestamp;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  _wnck_error_trap_push (display);
  XSendEvent (display,
	      root,
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_activate_workspace (Screen *screen,
                          int     new_active_space,
                          Time    timestamp)
{
  Display *display;
  Window   root;
  XEvent   xev;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = root;
  xev.xclient.message_type = _wnck_atom_get ("_NET_CURRENT_DESKTOP");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = new_active_space;
  xev.xclient.data.l[1] = timestamp;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  _wnck_error_trap_push (display);
  XSendEvent (display,
	      root,
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_change_viewport (Screen *screen,
		       int     x,
		       int     y)
{
  Display *display;
  Window   root;
  XEvent   xev;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = root;
  xev.xclient.message_type = _wnck_atom_get ("_NET_DESKTOP_VIEWPORT");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = x;
  xev.xclient.data.l[1] = y;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  _wnck_error_trap_push (display);
  XSendEvent (display,
	      root,
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_toggle_showing_desktop (Screen  *screen,
                              gboolean show)
{
  Display *display;
  Window   root;
  XEvent   xev;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = root;
  xev.xclient.message_type = _wnck_atom_get ("_NET_SHOWING_DESKTOP");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = show != FALSE;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  _wnck_error_trap_push (display);
  XSendEvent (display,
	      root,
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
  _wnck_error_trap_pop (display);
}

char*
_wnck_get_session_id (Screen *screen,
                      Window  xwindow)
{
  Window client_leader;

  client_leader = None;
  _wnck_get_window (screen, xwindow,
                    _wnck_atom_get ("WM_CLIENT_LEADER"),
                    &client_leader);

  if (client_leader == None)
    return NULL;

  return _wnck_get_string_property_latin1 (screen, client_leader,
                                           _wnck_atom_get ("SM_CLIENT_ID"));
}

#ifdef HAVE_XRES
static int
xres_get_pid (WnckScreen *screen,
              Window      xwindow)
{
  int pid = -1;
  Screen *xscreen;
  XResClientIdSpec client_spec;
  long client_id_count = 0;
  XResClientIdValue *client_ids = NULL;

  if (!_wnck_handle_has_xres (wnck_screen_get_handle (screen)))
    return -1;

  xscreen = _wnck_screen_get_xscreen (screen);

  client_spec.client = xwindow;
  client_spec.mask = XRES_CLIENT_ID_PID_MASK;

  if (XResQueryClientIds (DisplayOfScreen (xscreen), 1, &client_spec,
                          &client_id_count, &client_ids) == Success)
    {
      long i;

      for (i = 0; i < client_id_count; i++)
        {
          pid = XResGetClientPid (&client_ids[i]);
          if (pid != -1)
            break;
        }

      XResClientIdsDestroy (client_id_count, client_ids);
    }

  return pid;
}
#endif

int
_wnck_get_pid (WnckScreen *screen,
               Window      xwindow)
{
  int pid = -1;

#ifdef HAVE_XRES
  pid = xres_get_pid (screen, xwindow);

  if (pid != -1)
    return pid;
#endif

  if (!_wnck_get_cardinal (_wnck_screen_get_xscreen (screen),
                           xwindow,
                           _wnck_atom_get ("_NET_WM_PID"),
                           &pid))
    return 0;
  else
    return pid;
}

char*
_wnck_get_name (Screen *screen,
                Window  xwindow)
{
  char *name;

  name = _wnck_get_utf8_property (screen, xwindow,
                                  _wnck_atom_get ("_NET_WM_VISIBLE_NAME"));

  if (name == NULL)
    name = _wnck_get_utf8_property (screen, xwindow,
                                    _wnck_atom_get ("_NET_WM_NAME"));

  if (name == NULL)
    name = _wnck_get_text_property (screen, xwindow,
                                    XA_WM_NAME);

  return name;
}

char*
_wnck_get_icon_name (Screen *screen,
                     Window  xwindow)
{
  char *name;

  name = _wnck_get_utf8_property (screen, xwindow,
                                  _wnck_atom_get ("_NET_WM_VISIBLE_ICON_NAME"));

  if (name == NULL)
    name = _wnck_get_utf8_property (screen, xwindow,
                                    _wnck_atom_get ("_NET_WM_ICON_NAME"));

  if (name == NULL)
    name = _wnck_get_text_property (screen, xwindow,
                                    XA_WM_ICON_NAME);

  return name;
}

static char*
latin1_to_utf8 (const char *latin1)
{
  GString *str;
  const char *p;

  str = g_string_new (NULL);

  p = latin1;
  while (*p)
    {
      g_string_append_unichar (str, (gunichar) *p);
      ++p;
    }

  return g_string_free (str, FALSE);
}

char*
_wnck_get_res_class_utf8 (Screen *screen,
                          Window  xwindow)
{
  char *res_class;

  _wnck_get_wmclass (screen, xwindow, &res_class, NULL);

  return res_class;
}

void
_wnck_get_wmclass (Screen *screen,
                   Window  xwindow,
                   char **res_class,
                   char **res_name)
{
  Display *display;
  XClassHint ch;

  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);

  ch.res_name = NULL;
  ch.res_class = NULL;

  XGetClassHint (display, xwindow, &ch);

  _wnck_error_trap_pop (display);

  if (res_class)
    *res_class = NULL;

  if (res_name)
    *res_name = NULL;

  if (ch.res_name)
    {
      if (res_name)
        *res_name = latin1_to_utf8 (ch.res_name);

      XFree (ch.res_name);
    }

  if (ch.res_class)
    {
      if (res_class)
        *res_class = latin1_to_utf8 (ch.res_class);

      XFree (ch.res_class);
    }
}

gboolean
_wnck_get_frame_extents (Screen *screen,
                         Window  xwindow,
                         int    *left_frame,
                         int    *right_frame,
                         int    *top_frame,
                         int    *bottom_frame)
{
  gulong   *p_size;
  int       n_size;
  gboolean  retval;

  retval = FALSE;
  p_size = NULL;
  n_size = 0;

  _wnck_get_cardinal_list (screen, xwindow,
                           _wnck_atom_get ("_NET_FRAME_EXTENTS"),
                           &p_size, &n_size);

  if (p_size != NULL && n_size == 4)
    {
      *left_frame   = p_size[0];
      *right_frame  = p_size[1];
      *top_frame    = p_size[2];
      *bottom_frame = p_size[3];

      retval = TRUE;
    }

  if (p_size == NULL)
    {
      _wnck_get_cardinal_list (screen, xwindow,
                               _wnck_atom_get ("_GTK_FRAME_EXTENTS"),
                               &p_size, &n_size);

      if (p_size != NULL && n_size == 4)
        {
          *left_frame   = -p_size[0];
          *right_frame  = -p_size[1];
          *top_frame    = -p_size[2];
          *bottom_frame = -p_size[3];

          retval = TRUE;
        }
    }

  if (p_size != NULL)
    g_free (p_size);

  return retval;
}

int
_wnck_select_input (Screen *screen,
                    Window  xwindow,
                    int     mask,
                    gboolean update)
{
  Display   *display;
  GdkWindow *gdkwindow;
  int old_mask = 0;

  display = DisplayOfScreen (screen);

  gdkwindow = _wnck_gdk_window_lookup_from_window (screen, xwindow);

  _wnck_error_trap_push (display);
  if (gdkwindow)
    {
      /* Avoid breaking GDK's setup,
       * this somewhat relies on people setting
       * event masks right after realization
       * and not changing them again
       */
      XWindowAttributes attrs;
      XGetWindowAttributes (display, xwindow, &attrs);
      old_mask = attrs.your_event_mask;

      if (update)
        mask |= attrs.your_event_mask;
    }

  XSelectInput (display, xwindow, mask);
  _wnck_error_trap_pop (display);

  return old_mask;
}

cairo_surface_t *
_wnck_cairo_surface_get_from_pixmap (Screen *screen,
                                     Pixmap  xpixmap)
{
  cairo_surface_t *surface;
  Display *display;
  Window root_return;
  int x_ret, y_ret;
  unsigned int w_ret, h_ret, bw_ret, depth_ret;
  XWindowAttributes attrs;

  surface = NULL;
  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);

  if (!XGetGeometry (display, xpixmap, &root_return,
                     &x_ret, &y_ret, &w_ret, &h_ret, &bw_ret, &depth_ret))
    goto TRAP_POP;

  if (depth_ret == 1)
    {
      surface = cairo_xlib_surface_create_for_bitmap (display,
                                                      xpixmap,
                                                      screen,
                                                      w_ret,
                                                      h_ret);
    }
  else
    {
      if (!XGetWindowAttributes (display, root_return, &attrs))
        goto TRAP_POP;

      if (depth_ret == (unsigned int) attrs.depth)
	{
	  surface = cairo_xlib_surface_create (display,
					       xpixmap,
					       attrs.visual,
					       w_ret, h_ret);
	}
      else
	{
#if HAVE_CAIRO_XLIB_XRENDER
	  int std;

	  switch (depth_ret) {
	  case 1: std = PictStandardA1; break;
	  case 4: std = PictStandardA4; break;
	  case 8: std = PictStandardA8; break;
	  case 24: std = PictStandardRGB24; break;
	  case 32: std = PictStandardARGB32; break;
	  default: goto TRAP_POP;
	  }

	  surface = cairo_xlib_surface_create_with_xrender_format (display,
								   xpixmap,
								   attrs.screen,
								   XRenderFindStandardFormat (display, std),
								   w_ret, h_ret);
#endif
	}
    }

TRAP_POP:
  _wnck_error_trap_pop (display);

  return surface;
}

GdkPixbuf*
_wnck_gdk_pixbuf_get_from_pixmap (Screen *screen,
                                  Pixmap  xpixmap)
{
  cairo_surface_t *surface;
  GdkPixbuf *retval;

  surface = _wnck_cairo_surface_get_from_pixmap (screen, xpixmap);

  if (surface == NULL)
    return NULL;

  retval = gdk_pixbuf_get_from_surface (surface,
                                        0,
                                        0,
                                        cairo_xlib_surface_get_width (surface),
                                        cairo_xlib_surface_get_height (surface));
  cairo_surface_destroy (surface);

  return retval;
}

GdkPixbuf *
_wnck_get_fallback_icon (int size)
{
  GdkPixbuf *base;

  base = gdk_pixbuf_new_from_resource ("/org/gnome/libwnck/default_icon.png", NULL);

  g_assert (base);

  if (gdk_pixbuf_get_width (base) == size &&
      gdk_pixbuf_get_height (base) == size)
    {
      return base;
    }
  else
    {
      GdkPixbuf *scaled;

      scaled = gdk_pixbuf_scale_simple (base, size, size, GDK_INTERP_BILINEAR);
      g_object_unref (G_OBJECT (base));

      return scaled;
    }
}

void
_wnck_get_window_geometry (Screen *screen,
			   Window  xwindow,
                           int    *xp,
                           int    *yp,
                           int    *widthp,
                           int    *heightp)
{
  Display *display;
  int x, y;
  unsigned int width, height, bw, depth;
  Window root_window;

  width = 1;
  height = 1;

  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);

  XGetGeometry (display,
                xwindow,
                &root_window,
                &x, &y, &width, &height, &bw, &depth);

  _wnck_error_trap_pop (display);

  _wnck_get_window_position (screen, xwindow, xp, yp);

  if (widthp)
    *widthp = width;
  if (heightp)
    *heightp = height;
}

void _wnck_set_window_geometry (Screen *screen,
                                Window  xwindow,
                                int     gravity_and_flags,
                                int     x,
                                int     y,
                                int     width,
                                int     height)
{
  Display *display;
  Window   root;
  XEvent   xev;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_MOVERESIZE_WINDOW");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = gravity_and_flags;
  xev.xclient.data.l[1] = x;
  xev.xclient.data.l[2] = y;
  xev.xclient.data.l[3] = width;
  xev.xclient.data.l[4] = height;

  _wnck_error_trap_push (display);
  XSendEvent (display,
              root,
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_get_window_position (Screen *screen,
			   Window  xwindow,
                           int    *xp,
                           int    *yp)
{
  Display *display;
  Window   root;
  int      x, y;
  Window   child;

  x = 0;
  y = 0;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  _wnck_error_trap_push (display);
  XTranslateCoordinates (display,
                         xwindow,
			 root,
                         0, 0,
                         &x, &y, &child);
  _wnck_error_trap_pop (display);

  if (xp)
    *xp = x;
  if (yp)
    *yp = y;
}

void
_wnck_set_icon_geometry  (Screen *screen,
                          Window  xwindow,
			  int     x,
			  int     y,
			  int     width,
			  int     height)
{
  Display *display;
  gulong data[4];

  display = DisplayOfScreen (screen);

  data[0] = x;
  data[1] = y;
  data[2] = width;
  data[3] = height;

  _wnck_error_trap_push (display);

  XChangeProperty (display,
		   xwindow,
		   _wnck_atom_get ("_NET_WM_ICON_GEOMETRY"),
		   XA_CARDINAL, 32, PropModeReplace,
		   (guchar *)&data, 4);

  _wnck_error_trap_pop (display);
}

GdkDisplay*
_wnck_gdk_display_lookup_from_display (Display *display)
{
  GdkDisplay *gdkdisplay = NULL;

  gdkdisplay = gdk_x11_lookup_xdisplay (display);

  if (!gdkdisplay)
    g_warning ("No GdkDisplay matching Display \"%s\" was found.\n",
               DisplayString (display));

  return gdkdisplay;
}

GdkWindow*
_wnck_gdk_window_lookup_from_window (Screen *screen,
                                     Window  xwindow)
{
  Display    *display;
  GdkDisplay *gdkdisplay;

  display = DisplayOfScreen (screen);
  gdkdisplay = _wnck_gdk_display_lookup_from_display (display);
  if (!gdkdisplay)
    return NULL;

  return gdk_x11_window_lookup_for_display (gdkdisplay, xwindow);
}

/* orientation of pager */
#define _NET_WM_ORIENTATION_HORZ 0
#define _NET_WM_ORIENTATION_VERT 1

/* starting corner for counting spaces */
#define _NET_WM_TOPLEFT     0
#define _NET_WM_TOPRIGHT    1
#define _NET_WM_BOTTOMRIGHT 2
#define _NET_WM_BOTTOMLEFT  3

void
_wnck_set_desktop_layout (Screen *xscreen,
                          int     rows,
                          int     columns)
{
  Display *display;
  Window   root;
  gulong   data[4];

  /* FIXME: hack, hack, hack so as not
   * to have to add a orientation param
   * to wnck_screen_try_set_workspace_layout.
   *
   * Remove this crack asap.
   */
  g_assert ((rows == 0) || (columns == 0));

  display = DisplayOfScreen (xscreen);
  root = RootWindowOfScreen (xscreen);

  data[0] = (columns == 0) ? _NET_WM_ORIENTATION_HORZ : _NET_WM_ORIENTATION_VERT;
  data[1] = columns;
  data[2] = rows;
  data[3] = _NET_WM_TOPLEFT;

  _wnck_error_trap_push (display);

  XChangeProperty (display,
                   root,
		   _wnck_atom_get ("_NET_DESKTOP_LAYOUT"),
		   XA_CARDINAL, 32, PropModeReplace,
		   (guchar *)&data, 4);

  _wnck_error_trap_pop (display);
}

typedef struct
{
  Window window;
  Atom timestamp_prop_atom;
} TimeStampInfo;

static Bool
timestamp_predicate (Display *display,
		     XEvent  *xevent,
		     XPointer arg)
{
  TimeStampInfo *info = (TimeStampInfo *)arg;

  if (xevent->type == PropertyNotify &&
      xevent->xproperty.window == info->window &&
      xevent->xproperty.atom == info->timestamp_prop_atom)
    return True;

  return False;
}

/**
 * get_server_time:
 * @display: display from which to get the time
 * @window: a #Window, used for communication with the server.
 *          The window must have PropertyChangeMask in its
 *          events mask or a hang will result.
 *
 * Routine to get the current X server time stamp.
 *
 * Return value: the time stamp.
 **/
static Time
get_server_time (Display *display,
                 Window   window)
{
  unsigned char c = 'a';
  XEvent xevent;
  TimeStampInfo info;

  info.timestamp_prop_atom = _wnck_atom_get ("_TIMESTAMP_PROP");
  info.window = window;

  XChangeProperty (display, window,
		   info.timestamp_prop_atom, info.timestamp_prop_atom,
		   8, PropModeReplace, &c, 1);

  XIfEvent (display, &xevent,
	    timestamp_predicate, (XPointer)&info);

  return xevent.xproperty.time;
}

typedef struct
{
  Display *display;
  int screen_number;
  int token;
  Window window;
  Atom selection_atom;
  Atom manager_atom;
} LayoutManager;

static GSList *layout_managers = NULL;
static int next_token = 1;

static void
_wnck_free_layout_manager (LayoutManager *lm)
{
  _wnck_error_trap_push (lm->display);
  XDestroyWindow (lm->display, lm->window);
  _wnck_error_trap_pop (lm->display);

  g_slice_free (LayoutManager, lm);

  layout_managers = g_slist_remove (layout_managers, lm);
}

int
_wnck_try_desktop_layout_manager (Screen *xscreen,
                                  int     current_token)
{
  Display *display;
  Window root;
  Atom selection_atom;
  Window owner;
  GSList *tmp;
  int number;
  Time timestamp;
  XClientMessageEvent xev;
  char buffer[256];
  LayoutManager *lm;

  display = DisplayOfScreen (xscreen);
  root = RootWindowOfScreen (xscreen);
  number = XScreenNumberOfScreen (xscreen);

  sprintf (buffer, "_NET_DESKTOP_LAYOUT_S%d", number);
  selection_atom = _wnck_atom_get (buffer);

  owner = XGetSelectionOwner (display, selection_atom);

  tmp = layout_managers;
  while (tmp != NULL)
    {
      lm = tmp->data;

      if (display == lm->display &&
          number == lm->screen_number)
        {
          if (current_token == lm->token)
            {
              if (owner == lm->window)
                return current_token; /* we still have the selection */
              else
                { /* we lost the selection */
                  _wnck_free_layout_manager (lm);
                  break;
                }
            }
          else
            return WNCK_NO_MANAGER_TOKEN; /* someone else has it */
        }

      tmp = tmp->next;
    }

  if (owner != None)
    return WNCK_NO_MANAGER_TOKEN; /* someone else has the selection */

  /* No one has the selection at the moment */

  lm = g_slice_new0 (LayoutManager);

  lm->display = display;
  lm->screen_number = number;
  lm->token = next_token;
  ++next_token;

  lm->selection_atom = selection_atom;
  lm->manager_atom = _wnck_atom_get ("MANAGER");

  _wnck_error_trap_push (display);

  lm->window = XCreateSimpleWindow (display,
                                    root,
                                    0, 0, 10, 10, 0,
                                    WhitePixel (display, number),
                                    WhitePixel (display, number));

  XSelectInput (display, lm->window, PropertyChangeMask);
  timestamp = get_server_time (display, lm->window);

  XSetSelectionOwner (display, lm->selection_atom,
		      lm->window, timestamp);

  _wnck_error_trap_pop (display);

  /* Check to see if we managed to claim the selection. */

  if (XGetSelectionOwner (display, lm->selection_atom) !=
      lm->window)
    {
      g_free (lm);
      return WNCK_NO_MANAGER_TOKEN;
    }

  xev.type = ClientMessage;
  xev.window = root;
  xev.message_type = lm->manager_atom;
  xev.format = 32;
  xev.data.l[0] = timestamp;
  xev.data.l[1] = lm->selection_atom;
  xev.data.l[2] = lm->window;
  xev.data.l[3] = 0;	/* manager specific data */
  xev.data.l[4] = 0;	/* manager specific data */

  _wnck_error_trap_push (display);
  XSendEvent (display, root,
              False, StructureNotifyMask, (XEvent *)&xev);
  _wnck_error_trap_pop (display);

  layout_managers = g_slist_prepend (layout_managers,
                                     lm);

  return lm->token;
}

void
_wnck_release_desktop_layout_manager (Screen *xscreen,
                                      int     current_token)
{
  Display *display;
  GSList *tmp;
  int number;
  LayoutManager *lm;

  display = DisplayOfScreen (xscreen);
  number = XScreenNumberOfScreen (xscreen);

  tmp = layout_managers;
  while (tmp != NULL)
    {
      lm = tmp->data;

      if (display == lm->display &&
          number == lm->screen_number)
        {
          if (current_token == lm->token)
            {
              _wnck_error_trap_push (display);

              /* release selection ownership */
              if (XGetSelectionOwner (display, lm->selection_atom) !=
                  lm->window)
                {
                  Time timestamp;

                  timestamp = get_server_time (display, lm->window);
                  XSetSelectionOwner (display, lm->selection_atom,
                                      None, timestamp);
                }

              _wnck_error_trap_pop (display);

              _wnck_free_layout_manager (lm);
              return;
            }
        }

      tmp = tmp->next;
    }
}

gboolean
_wnck_desktop_layout_manager_process_event (XEvent *xev)
{
  GSList *tmp;
  LayoutManager *lm;

  if (xev->type != SelectionClear)
    return FALSE;

  tmp = layout_managers;
  while (tmp != NULL)
    {
      lm = tmp->data;

      if (xev->xany.display == lm->display &&
          xev->xany.window == lm->window &&
          xev->xselectionclear.selection == lm->selection_atom)
        {
          _wnck_free_layout_manager (lm);
          return TRUE;
        }

      tmp = tmp->next;
    }

  return FALSE;
}
