/* Xlib utils */

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

#include "xutils.h"
#include <string.h>
#include "screen.h"
#include "window.h"
#include "private.h"

gboolean
_wnck_get_cardinal (Window  xwindow,
                    Atom    atom,
                    int    *val)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gulong *num;
  int err;

  *val = 0;
  
  _wnck_error_trap_push ();
  type = None;
  XGetWindowProperty (gdk_display,
                      xwindow,
                      atom,
                      0, G_MAXLONG,
		      False, XA_CARDINAL, &type, &format, &nitems,
		      &bytes_after, (guchar **)&num);  
  err = _wnck_error_trap_pop ();
  if (err != Success)
    return FALSE;
  
  if (type != XA_CARDINAL)
    return FALSE;

  *val = *num;
  
  XFree (num);

  return TRUE;
}

int
_wnck_get_wm_state (Window  xwindow)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gulong *num;
  int err;
  Atom wm_state;
  int retval;

  wm_state = _wnck_atom_get ("WM_STATE");
  retval = NormalState;
  
  _wnck_error_trap_push ();
  type = None;
  XGetWindowProperty (gdk_display,
                      xwindow,
                      wm_state,
                      0, G_MAXLONG,
		      False, wm_state, &type, &format, &nitems,
		      &bytes_after, (guchar **)&num);  
  err = _wnck_error_trap_pop ();
  if (err != Success)
    return retval;
  
  if (type != wm_state)
    return retval;

  retval = *num;
  
  XFree (num);

  return retval;
}

gboolean
_wnck_get_window (Window  xwindow,
                  Atom    atom,
                  Window *val)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Window *w;
  int err;

  *val = 0;
  
  _wnck_error_trap_push ();
  type = None;
  XGetWindowProperty (gdk_display,
                      xwindow,
                      atom,
                      0, G_MAXLONG,
		      False, XA_WINDOW, &type, &format, &nitems,
		      &bytes_after, (guchar **)&w);  
  err = _wnck_error_trap_pop ();
  if (err != Success)
    return FALSE;
  
  if (type != XA_WINDOW)
    return FALSE;

  *val = *w;
  
  XFree (w);

  return TRUE;
}

static char*
text_property_to_utf8 (const XTextProperty *prop)
{
  char **list;
  int count;
  char *retval;
  
  list = NULL;

  count = gdk_text_property_to_utf8_list (prop->encoding,
                                          prop->format,
                                          prop->value,
                                          prop->nitems,
                                          &list);

  if (count == 0)
    return NULL;

  retval = list[0];
  list[0] = g_strdup (""); /* something to free */
  
  g_strfreev (list);

  return retval;
}

char*
_wnck_get_text_property (Window  xwindow,
                         Atom    atom)
{
  XTextProperty text;
  char *retval;
  
  _wnck_error_trap_push ();

  text.nitems = 0;
  if (XGetTextProperty (gdk_display,
                        xwindow,
                        &text,
                        atom))
    {
      retval = text_property_to_utf8 (&text);

      if (text.nitems > 0)
        XFree (text.value);
    }
  else
    {
      retval = NULL;
    }
  
  _wnck_error_trap_pop ();

  return retval;
}

char*
_wnck_get_string_property_latin1 (Window  xwindow,
                                  Atom    atom)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  guchar *str;
  int result;
  char *retval;
  
  _wnck_error_trap_push ();
  str = NULL;
  XGetWindowProperty (gdk_display,
                      xwindow, atom,
                      0, G_MAXLONG,
		      False, XA_STRING, &type, &format, &nitems,
		      &bytes_after, (guchar **)&str);  

  result = _wnck_error_trap_pop ();
  if (result != Success)
    return NULL;
  
  if (type != XA_STRING)
    return NULL; /* FIXME memory leak? */

  retval = g_strdup (str);
  
  XFree (str);
  
  return retval;
}

char*
_wnck_get_utf8_property (Window  xwindow,
                         Atom    atom)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  guchar *val;
  int err;
  char *retval;
  Atom utf8_string;

  utf8_string = _wnck_atom_get ("UTF8_STRING");
  
  _wnck_error_trap_push ();
  type = None;
  val = NULL;
  XGetWindowProperty (gdk_display,
                      xwindow,
                      atom,
                      0, G_MAXLONG,
		      False, utf8_string,
                      &type, &format, &nitems,
		      &bytes_after, (guchar **)&val);  
  err = _wnck_error_trap_pop ();

  if (err != Success)
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
_wnck_get_window_list (Window   xwindow,
                       Atom     atom,
                       Window **windows,
                       int     *len)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Window *data;
  int err;

  *windows = NULL;
  *len = 0;
  
  _wnck_error_trap_push ();
  type = None;
  XGetWindowProperty (gdk_display,
                      xwindow,
                      atom,
                      0, G_MAXLONG,
		      False, XA_WINDOW, &type, &format, &nitems,
		      &bytes_after, (guchar **)&data);  
  err = _wnck_error_trap_pop ();
  if (err != Success)
    return FALSE;
  
  if (type != XA_WINDOW)
    return FALSE; /* FIXME memory leak? */

  *windows = g_new (Window, nitems);
  memcpy (*windows, data, sizeof (Window) * nitems);
  *len = nitems;
  
  XFree (data);

  return TRUE;  
}

gboolean
_wnck_get_atom_list (Window   xwindow,
                     Atom     atom,
                     Atom   **atoms,
                     int     *len)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Atom *data;
  int err;

  *atoms = NULL;
  *len = 0;
  
  _wnck_error_trap_push ();
  type = None;
  XGetWindowProperty (gdk_display,
                      xwindow,
                      atom,
                      0, G_MAXLONG,
		      False, XA_ATOM, &type, &format, &nitems,
		      &bytes_after, (guchar **)&data);  
  err = _wnck_error_trap_pop ();
  if (err != Success)
    return FALSE;
  
  if (type != XA_ATOM)
    return FALSE; /* FIXME memory leak? */

  *atoms = g_new (Atom, nitems);
  memcpy (*atoms, data, sizeof (Atom) * nitems);
  *len = nitems;
  
  XFree (data);

  return TRUE;
}

gboolean
_wnck_get_cardinal_list (Window   xwindow,
                         Atom     atom,
                         gulong **cardinals,
                         int     *len)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gulong *nums;
  int err;

  *cardinals = NULL;
  *len = 0;
  
  _wnck_error_trap_push ();
  type = None;
  XGetWindowProperty (gdk_display,
                      xwindow,
                      atom,
                      0, G_MAXLONG,
		      False, XA_CARDINAL, &type, &format, &nitems,
		      &bytes_after, (guchar **)&nums);  
  err = _wnck_error_trap_pop ();
  if (err != Success)
    return FALSE;
  
  if (type != XA_CARDINAL)
    return FALSE; /* FIXME memory leak ? */

  *cardinals = g_new (gulong, nitems);
  memcpy (*cardinals, nums, sizeof (gulong) * nitems);
  *len = nitems;
  
  XFree (nums);

  return TRUE;
}

void
_wnck_error_trap_push (void)
{
  gdk_error_trap_push ();
}

int
_wnck_error_trap_pop (void)
{
  XSync (gdk_display, False);
  return gdk_error_trap_pop ();
}

static GHashTable *atom_hash = NULL;
static GHashTable *reverse_atom_hash = NULL;

Atom
_wnck_atom_get (const char *atom_name)
{
  GdkAtom retval;
  
  g_return_val_if_fail (atom_name != NULL, GDK_NONE);

  if (!atom_hash)
    {
      atom_hash = g_hash_table_new (g_str_hash, g_str_equal);
      reverse_atom_hash = g_hash_table_new (NULL, NULL);
    }
      
  retval = GPOINTER_TO_UINT (g_hash_table_lookup (atom_hash, atom_name));
  if (!retval)
    {
      retval = XInternAtom (gdk_display, atom_name, FALSE);

      if (retval != None)
        {
          char *name_copy;

          name_copy = g_strdup (atom_name);
          
          g_hash_table_insert (atom_hash,
                               name_copy,
                               GUINT_TO_POINTER (retval));
          g_hash_table_insert (reverse_atom_hash,
                               GUINT_TO_POINTER (retval),
                               name_copy);
        }
    }

  return retval;
}

const char*
_wnck_atom_name (Atom atom)
{
  if (reverse_atom_hash)
    return g_hash_table_lookup (reverse_atom_hash, GUINT_TO_POINTER (atom));
  else
    return NULL;
}

static GdkFilterReturn
filter_func (GdkXEvent  *gdkxevent,
             GdkEvent   *event,
             gpointer    data)
{
  XEvent *xevent = gdkxevent;
  
  switch (xevent->type)
    {
    case PropertyNotify:
      {
        WnckScreen *screen;
        
        screen = wnck_screen_get_for_root (xevent->xany.window);
        if (screen != NULL)
          _wnck_screen_process_property_notify (screen, xevent);
        else
          {
            WnckWindow *window;

            window = wnck_window_get (xevent->xany.window);

            if (window)
              _wnck_window_process_property_notify (window, xevent);
          }
      }
      break;
    }
  
  return GDK_FILTER_CONTINUE;
}

void
_wnck_event_filter_init (void)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      gdk_window_add_filter (NULL, filter_func, NULL);
      initialized = TRUE;
    }
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
#if G_SIZEOF_LONG > 4
  return (guint) (val ^ (val >> 32));
#else
  return val;
#endif
}

void
_wnck_iconify (Window xwindow)
{
  _wnck_error_trap_push ();
  XIconifyWindow (gdk_display, xwindow, DefaultScreen (gdk_display));
  _wnck_error_trap_pop ();
}

void
_wnck_deiconify (Window xwindow)
{
  /* We need special precautions, because GDK doesn't like
   * XMapWindow() called on its windows, need to use the
   * GDK functions
   */
  GdkWindow *gdkwindow;

  gdkwindow = gdk_xid_table_lookup (xwindow);

  _wnck_error_trap_push ();
  if (gdkwindow)
    gdk_window_show (gdkwindow);
  else
    XMapRaised (gdk_display, xwindow);
  _wnck_error_trap_pop ();
}

void
_wnck_change_state (Window   xwindow,
                    gboolean add,
                    Atom     state1,
                    Atom     state2)
{
  XEvent xev;

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */  
  
  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_WM_STATE");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
  xev.xclient.data.l[1] = state1;
  xev.xclient.data.l[2] = state2;

  XSendEvent (gdk_display,
              gdk_x11_get_default_root_xwindow (),
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
}

void
_wnck_change_workspace (Window xwindow,
                        int    new_space)
{
  XEvent xev;
  
  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_WM_DESKTOP");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = new_space;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;

  XSendEvent (gdk_display,
              gdk_x11_get_default_root_xwindow (),
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
}

void
_wnck_activate (Window xwindow)
{
  XEvent xev;
  
  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_ACTIVE_WINDOW");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = 0;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;

  XSendEvent (gdk_display,
              gdk_x11_get_default_root_xwindow (),
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev); 
}

Window
_wnck_get_group_leader (Window xwindow)
{
  XWMHints *hints;
  Window ret;
  
  _wnck_error_trap_push ();
  hints = XGetWMHints (gdk_display, xwindow);
  _wnck_error_trap_pop ();

  /* default to window being its own leader */
  ret = xwindow;
  
  if (hints &&
      (hints->flags & WindowGroupHint))
    ret = hints->window_group;

  XFree (hints);

  return ret;
}

char*
_wnck_get_session_id (Window xwindow)
{
  Window client_leader;
  
  client_leader = None;
  _wnck_get_window (xwindow,
                    _wnck_atom_get ("WM_CLIENT_LEADER"),
                    &client_leader);

  if (client_leader == None)
    return NULL;

  return _wnck_get_string_property_latin1 (client_leader,
                                           _wnck_atom_get ("SM_CLIENT_ID"));
}

int
_wnck_get_pid (Window xwindow)
{
  int val;

  if (!_wnck_get_cardinal (xwindow,
                           _wnck_atom_get ("_NET_WM_PID"),
                           &val))
    return 0;
  else
    return val;
}

void
_wnck_select_input (Window xwindow,
                    int    mask)
{
  GdkWindow *gdkwindow;
  
  gdkwindow = gdk_xid_table_lookup (xwindow);

  _wnck_error_trap_push ();
  if (gdkwindow)
    {
      /* Avoid breaking GDK's setup,
       * this somewhat relies on people setting
       * event masks right after realization
       * and not changing them again
       */
      XWindowAttributes attrs;
      XGetWindowAttributes (gdk_display, xwindow, &attrs);
      mask |= attrs.your_event_mask;
    }
  
  XSelectInput (gdk_display, xwindow, mask);
  _wnck_error_trap_pop ();
}
  
