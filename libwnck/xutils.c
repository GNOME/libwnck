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

#include <config.h>
#include "xutils.h"
#include <string.h>
#include "screen.h"
#include "window.h"
#include "private.h"
#include "inlinepixbufs.h"

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
    {
      XFree (num);
      return FALSE;
    }

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
    {
      XFree (num);
      return retval;
    }

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
    {
      XFree (w);
      return FALSE;
    }

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

  count = gdk_text_property_to_utf8_list (gdk_x11_xatom_to_atom (prop->encoding),
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
    {
      XFree (str);
      return NULL;
    }

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
    {
      XFree (data);
      return FALSE;
    }

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
  Atom retval;
  
  g_return_val_if_fail (atom_name != NULL, None);

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

    case ConfigureNotify:
      {
        WnckWindow *window;
        
        window = wnck_window_get (xevent->xconfigure.window);
        
        if (window)
          _wnck_window_process_configure_notify (window, xevent);
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
_wnck_close (Window xwindow)
{
  XEvent xev;
  
  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_CLOSE_WINDOW");
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

void
_wnck_activate_workspace (int new_active_space)
{
  XEvent xev;
  
  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = gdk_x11_get_default_root_xwindow ();
  xev.xclient.message_type = _wnck_atom_get ("_NET_CURRENT_DESKTOP");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = new_active_space;
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
  
/* The icon-reading code is copied
 * from metacity, please sync bugfixes
 */
static gboolean
find_largest_sizes (gulong *data,
                    int     nitems,
                    int    *width,
                    int    *height)
{
  *width = 0;
  *height = 0;
  
  while (nitems > 0)
    {
      int w, h;
      gboolean replace;

      replace = FALSE;
      
      if (nitems < 3)
        return FALSE; /* no space for w, h */
      
      w = data[0];
      h = data[1];
      
      if (nitems < ((w * h) + 2))
        return FALSE; /* not enough data */

      *width = MAX (w, *width);
      *height = MAX (h, *height);
      
      data += (w * h) + 2;
      nitems -= (w * h) + 2;
    }

  return TRUE;
}

static gboolean
find_best_size (gulong  *data,
                int      nitems,
                int      ideal_width,
                int      ideal_height,
                int     *width,
                int     *height,
                gulong **start)
{
  int best_w;
  int best_h;
  gulong *best_start;
  int max_width, max_height;
  
  *width = 0;
  *height = 0;
  *start = NULL;

  if (!find_largest_sizes (data, nitems, &max_width, &max_height))
    return FALSE;

  if (ideal_width < 0)
    ideal_width = max_width;
  if (ideal_height < 0)
    ideal_height = max_height;
  
  best_w = 0;
  best_h = 0;
  best_start = NULL;
  
  while (nitems > 0)
    {
      int w, h;
      gboolean replace;

      replace = FALSE;
      
      if (nitems < 3)
        return FALSE; /* no space for w, h */
      
      w = data[0];
      h = data[1];
      
      if (nitems < ((w * h) + 2))
        break; /* not enough data */

      if (best_start == NULL)
        {
          replace = TRUE;
        }
      else
        {
          /* work with averages */
          const int ideal_size = (ideal_width + ideal_height) / 2;
          int best_size = (best_w + best_h) / 2;
          int this_size = (w + h) / 2;
          
          /* larger than desired is always better than smaller */
          if (best_size < ideal_size &&
              this_size >= ideal_size)
            replace = TRUE;
          /* if we have too small, pick anything bigger */
          else if (best_size < ideal_size &&
                   this_size > best_size)
            replace = TRUE;
          /* if we have too large, pick anything smaller
           * but still >= the ideal
           */
          else if (best_size > ideal_size &&
                   this_size >= ideal_size &&
                   this_size < best_size)
            replace = TRUE;
        }

      if (replace)
        {
          best_start = data + 2;
          best_w = w;
          best_h = h;
        }

      data += (w * h) + 2;
      nitems -= (w * h) + 2;
    }

  if (best_start)
    {
      *start = best_start;
      *width = best_w;
      *height = best_h;
      return TRUE;
    }
  else
    return FALSE;
}

static void
argbdata_to_pixdata (gulong *argb_data, int len, guchar **pixdata)
{
  guchar *p;
  int i;
  
  *pixdata = g_new (guchar, len * 4);
  p = *pixdata;

  /* One could speed this up a lot. */
  i = 0;
  while (i < len)
    {
      guint argb;
      guint rgba;
      
      argb = argb_data[i];
      rgba = (argb << 8) | (argb >> 24);
      
      *p = rgba >> 24;
      ++p;
      *p = (rgba >> 16) & 0xff;
      ++p;
      *p = (rgba >> 8) & 0xff;
      ++p;
      *p = rgba & 0xff;
      ++p;
      
      ++i;
    }
}

static gboolean
read_rgb_icon (Window         xwindow,
               int            ideal_width,
               int            ideal_height,
               int            ideal_mini_width,
               int            ideal_mini_height,
               int           *width,
               int           *height,
               guchar       **pixdata,
               int           *mini_width,
               int           *mini_height,
               guchar       **mini_pixdata)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  int result;
  gulong *data; /* FIXME should be guint? */
  gulong *best;
  int w, h;
  gulong *best_mini;
  int mini_w, mini_h;
  
  if (sizeof (gulong) != 4)
    g_warning ("%s: Whoops, I think this function may be broken on 64-bit\n",
               G_GNUC_FUNCTION);
  
  _wnck_error_trap_push ();
  type = None;
  data = NULL;
  XGetWindowProperty (gdk_display,
                      xwindow,
                      _wnck_atom_get ("_NET_WM_ICON"),
		      0, G_MAXLONG,
		      False, XA_CARDINAL, &type, &format, &nitems,
		      &bytes_after, ((guchar **)&data));
  
  result = _wnck_error_trap_pop ();
  
  if (result != Success)
    return FALSE;

  if (type != XA_CARDINAL)
    {
      XFree (data);
      return FALSE;
    }
  
  if (!find_best_size (data, nitems,
                       ideal_width, ideal_height,
                       &w, &h, &best))
    {
      XFree (data);
      return FALSE;
    }

  if (!find_best_size (data, nitems,
                       ideal_mini_width, ideal_mini_height,
                       &mini_w, &mini_h, &best_mini))
    {
      XFree (data);
      return FALSE;
    }
  
  *width = w;
  *height = h;

  *mini_width = mini_w;
  *mini_height = mini_h;

  argbdata_to_pixdata (best, w * h, pixdata);
  argbdata_to_pixdata (best_mini, mini_w * mini_h, mini_pixdata);

  XFree (data);
  
  return TRUE;
}

static void
free_pixels (guchar *pixels, gpointer data)
{
  g_free (pixels);
}

static void
get_pixmap_geometry (Pixmap       pixmap,
                     int         *w,
                     int         *h,
                     int         *d)
{
  Window root_ignored;
  int x_ignored, y_ignored;
  guint width, height;
  guint border_width_ignored;
  guint depth;

  if (w)
    *w = 1;
  if (h)
    *h = 1;
  if (d)
    *d = 1;
  
  XGetGeometry (gdk_display,
                pixmap, &root_ignored, &x_ignored, &y_ignored,
                &width, &height, &border_width_ignored, &depth);

  if (w)
    *w = width;
  if (h)
    *h = height;
  if (d)
    *d = depth;
}

static GdkPixbuf*
apply_mask (GdkPixbuf *pixbuf,
            GdkPixbuf *mask)
{
  int w, h;
  int i, j;
  GdkPixbuf *with_alpha;
  guchar *src;
  guchar *dest;
  int src_stride;
  int dest_stride;
  
  w = MIN (gdk_pixbuf_get_width (mask), gdk_pixbuf_get_width (pixbuf));
  h = MIN (gdk_pixbuf_get_height (mask), gdk_pixbuf_get_height (pixbuf));
  
  with_alpha = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);

  dest = gdk_pixbuf_get_pixels (with_alpha);
  src = gdk_pixbuf_get_pixels (mask);

  dest_stride = gdk_pixbuf_get_rowstride (with_alpha);
  src_stride = gdk_pixbuf_get_rowstride (mask);
  
  i = 0;
  while (i < h)
    {
      j = 0;
      while (j < w)
        {
          guchar *s = src + i * src_stride + j * 3;
          guchar *d = dest + i * dest_stride + j * 4;
          
          /* s[0] == s[1] == s[2], they are 255 if the bit was set, 0
           * otherwise
           */
          if (s[0] == 0)
            d[3] = 0;   /* transparent */
          else
            d[3] = 255; /* opaque */
          
          ++j;
        }
      
      ++i;
    }

  return with_alpha;
}

static GdkColormap*
get_cmap (GdkPixmap *pixmap)
{
  GdkColormap *cmap;

  cmap = gdk_drawable_get_colormap (pixmap);
  if (cmap)
    g_object_ref (G_OBJECT (cmap));

  if (cmap == NULL)
    {
      if (gdk_drawable_get_depth (pixmap) == 1)
        {
          /* try null cmap */
          cmap = NULL;
        }
      else
        {
          /* Try system cmap */
          cmap = gdk_colormap_get_system ();
          g_object_ref (G_OBJECT (cmap));
        }
    }

  return cmap;
}

static GdkPixbuf*
_wnck_gdk_pixbuf_get_from_pixmap (GdkPixbuf   *dest,
                                  Pixmap       xpixmap,
                                  int          src_x,
                                  int          src_y,
                                  int          dest_x,
                                  int          dest_y,
                                  int          width,
                                  int          height)
{
  GdkDrawable *drawable;
  GdkPixbuf *retval;
  GdkColormap *cmap;
  
  retval = NULL;
  
  drawable = gdk_xid_table_lookup (xpixmap);

  if (drawable)
    g_object_ref (G_OBJECT (drawable));
  else
    drawable = gdk_pixmap_foreign_new (xpixmap);

  cmap = get_cmap (drawable);
  
  retval = gdk_pixbuf_get_from_drawable (dest,
                                         drawable,
                                         cmap,
                                         src_x, src_y,
                                         dest_x, dest_y,
                                         width, height);

  if (cmap)
    g_object_unref (G_OBJECT (cmap));
  g_object_unref (G_OBJECT (drawable));

  return retval;
}

static gboolean
try_pixmap_and_mask (Pixmap      src_pixmap,
                     Pixmap      src_mask,
                     GdkPixbuf **iconp,
                     int         ideal_width,
                     int         ideal_height,
                     GdkPixbuf **mini_iconp,
                     int         ideal_mini_width,
                     int         ideal_mini_height)
{
  GdkPixbuf *unscaled = NULL;
  GdkPixbuf *mask = NULL;
  int w, h;

  if (src_pixmap == None)
    return FALSE;
      
  _wnck_error_trap_push ();

  get_pixmap_geometry (src_pixmap, &w, &h, NULL);
      
  unscaled = _wnck_gdk_pixbuf_get_from_pixmap (NULL,
                                               src_pixmap,
                                               0, 0, 0, 0,
                                               w, h);

  if (unscaled && src_mask != None)
    {
      get_pixmap_geometry (src_mask, &w, &h, NULL);
      mask = _wnck_gdk_pixbuf_get_from_pixmap (NULL,
                                               src_mask,
                                               0, 0, 0, 0,
                                               w, h);
    }
  
  _wnck_error_trap_pop ();

  if (mask)
    {
      GdkPixbuf *masked;
      
      masked = apply_mask (unscaled, mask);
      g_object_unref (G_OBJECT (unscaled));
      unscaled = masked;

      g_object_unref (G_OBJECT (mask));
      mask = NULL;
    }
  
  if (unscaled)
    {
      *iconp =
        gdk_pixbuf_scale_simple (unscaled,
                                 ideal_width > 0 ? ideal_width :
                                 gdk_pixbuf_get_width (unscaled),
                                 ideal_height > 0 ? ideal_height :
                                 gdk_pixbuf_get_height (unscaled),
                                 GDK_INTERP_BILINEAR);
      *mini_iconp =
        gdk_pixbuf_scale_simple (unscaled,
                                 ideal_mini_width > 0 ? ideal_mini_width :
                                 gdk_pixbuf_get_width (unscaled),
                                 ideal_mini_height > 0 ? ideal_mini_height :
                                 gdk_pixbuf_get_height (unscaled),
                                 GDK_INTERP_BILINEAR);      
      
      g_object_unref (G_OBJECT (unscaled));
      return TRUE;
    }
  else
    return FALSE;
}

static void
get_kwm_win_icon (Window  xwindow,
                  Pixmap *pixmap,
                  Pixmap *mask)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Pixmap *icons;
  int result;

  *pixmap = None;
  *mask = None;
  
  _wnck_error_trap_push ();
  icons = NULL;
  XGetWindowProperty (gdk_display, xwindow,
                      _wnck_atom_get ("KWM_WIN_ICON"),
                      0, G_MAXLONG,
		      False,
                      _wnck_atom_get ("KWM_WIN_ICON"),
                      &type, &format, &nitems,
		      &bytes_after, (guchar **)&icons);  

  result = _wnck_error_trap_pop ();
  if (result != Success)
    return;
  
  if (type != _wnck_atom_get ("KWM_WIN_ICON"))
    {
      XFree (icons);
      return;
    }
  
  *pixmap = icons[0];
  *mask = icons[1];
  
  XFree (icons);

  return;
}

typedef enum
{
  /* These MUST be in ascending order of preference;
   * i.e. if we get _NET_WM_ICON and already have
   * WM_NORMAL_HINTS, we prefer _NET_WM_ICON
   */
  USING_NO_ICON,
  USING_FALLBACK_ICON,
  USING_KWM_WIN_ICON,
  USING_WM_NORMAL_HINTS,
  USING_NET_WM_ICON
} IconOrigin;

struct _WnckIconCache
{
  IconOrigin origin;
  Pixmap prev_pixmap;
  Pixmap prev_mask;
  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;
  int ideal_width;
  int ideal_height;
  int ideal_mini_width;
  int ideal_mini_height;
  guint want_fallback : 1;
  /* TRUE if these props have changed */
  guint wm_normal_hints_dirty : 1;
  guint kwm_win_icon_dirty : 1;
  guint net_wm_icon_dirty : 1;
};

WnckIconCache*
_wnck_icon_cache_new (void)
{
  WnckIconCache *icon_cache;

  icon_cache = g_new (WnckIconCache, 1);

  icon_cache->origin = USING_NO_ICON;
  icon_cache->prev_pixmap = None;
  icon_cache->icon = NULL;
  icon_cache->mini_icon = NULL;
  icon_cache->ideal_width = -1; /* won't be a legit width */
  icon_cache->ideal_height = -1;
  icon_cache->ideal_mini_width = -1;
  icon_cache->ideal_mini_height = -1;
  icon_cache->want_fallback = TRUE;
  icon_cache->wm_normal_hints_dirty = TRUE;
  icon_cache->kwm_win_icon_dirty = TRUE;
  icon_cache->net_wm_icon_dirty = TRUE;
  
  return icon_cache;
}

static void
clear_icon_cache (WnckIconCache *icon_cache,
                  gboolean       dirty_all)
{
  if (icon_cache->icon)
    g_object_unref (G_OBJECT (icon_cache->icon));
  icon_cache->icon = NULL;
  
  if (icon_cache->mini_icon)
    g_object_unref (G_OBJECT (icon_cache->mini_icon));
  icon_cache->mini_icon = NULL;

  icon_cache->origin = USING_NO_ICON;

  if (dirty_all)
    {
      icon_cache->wm_normal_hints_dirty = TRUE;
      icon_cache->kwm_win_icon_dirty = TRUE;
      icon_cache->net_wm_icon_dirty = TRUE;
    }
}

void
_wnck_icon_cache_free (WnckIconCache *icon_cache)
{
  clear_icon_cache (icon_cache, FALSE);
  
  g_free (icon_cache);
}

void
_wnck_icon_cache_property_changed (WnckIconCache *icon_cache,
                                   Atom           atom)
{  
  if (atom == _wnck_atom_get ("_NET_WM_ICON"))
    icon_cache->net_wm_icon_dirty = TRUE;
  else if (atom == _wnck_atom_get ("KWM_WIN_ICON"))
    icon_cache->kwm_win_icon_dirty = TRUE;
  else if (atom == _wnck_atom_get ("WM_NORMAL_HINTS"))
    icon_cache->wm_normal_hints_dirty = TRUE;
}

gboolean
_wnck_icon_cache_get_icon_invalidated (WnckIconCache *icon_cache)
{
  if (icon_cache->origin <= USING_KWM_WIN_ICON &&
      icon_cache->kwm_win_icon_dirty)
    return TRUE;
  else if (icon_cache->origin <= USING_WM_NORMAL_HINTS &&
           icon_cache->wm_normal_hints_dirty)
    return TRUE;
  else if (icon_cache->origin <= USING_NET_WM_ICON &&
           icon_cache->net_wm_icon_dirty)
    return TRUE;
  else if (icon_cache->origin < USING_FALLBACK_ICON &&
           icon_cache->want_fallback)
    return TRUE;
  else if (icon_cache->origin == USING_NO_ICON)
    return TRUE;
  else if (icon_cache->origin == USING_FALLBACK_ICON &&
           !icon_cache->want_fallback)
    return TRUE;
  else
    return FALSE;
}

void
_wnck_icon_cache_set_want_fallback (WnckIconCache *icon_cache,
                                    gboolean       setting)
{
  icon_cache->want_fallback = TRUE;
}

static void
replace_cache (WnckIconCache *icon_cache,
               IconOrigin     origin,
               GdkPixbuf     *new_icon,
               GdkPixbuf     *new_mini_icon)
{
  clear_icon_cache (icon_cache, FALSE);
  
  icon_cache->origin = origin;

  if (new_icon)
    g_object_ref (G_OBJECT (new_icon));

  icon_cache->icon = new_icon;

  if (new_mini_icon)
    g_object_ref (G_OBJECT (new_mini_icon));

  icon_cache->mini_icon = new_mini_icon;
}

gboolean
_wnck_read_icons (Window         xwindow,
                  WnckIconCache *icon_cache,
                  GdkPixbuf    **iconp,
                  int            ideal_width,
                  int            ideal_height,
                  GdkPixbuf    **mini_iconp,
                  int            ideal_mini_width,
                  int            ideal_mini_height)
{
  guchar *pixdata;     
  int w, h;
  guchar *mini_pixdata;
  int mini_w, mini_h;
  Pixmap pixmap;
  Pixmap mask;
  XWMHints *hints;

  /* Return value is whether the icon changed */
  
  g_return_val_if_fail (icon_cache != NULL, FALSE);
  
  *iconp = NULL;
  *mini_iconp = NULL;
  
  if (ideal_width != icon_cache->ideal_width ||
      ideal_height != icon_cache->ideal_height ||
      ideal_mini_width != icon_cache->ideal_mini_width ||
      ideal_mini_height != icon_cache->ideal_mini_height)
    clear_icon_cache (icon_cache, TRUE);
  
  icon_cache->ideal_width = ideal_width;
  icon_cache->ideal_height = ideal_height;
  icon_cache->ideal_mini_width = ideal_mini_width;
  icon_cache->ideal_mini_height = ideal_mini_height;

  if (!_wnck_icon_cache_get_icon_invalidated (icon_cache))
    return FALSE; /* we have no new info to use */
  
  pixdata = NULL;

  /* Our algorithm here assumes that we can't have for example origin
   * < USING_NET_WM_ICON and icon_cache->net_wm_icon_dirty == FALSE
   * unless we have tried to read NET_WM_ICON.
   *
   * Put another way, if an icon origin is not dirty, then we have
   * tried to read it at the current size. If it is dirty, then
   * we haven't done that since the last change.
   */
   
  if (icon_cache->origin <= USING_NET_WM_ICON &&
      icon_cache->net_wm_icon_dirty)

    {
      icon_cache->net_wm_icon_dirty = FALSE;
      
      if (read_rgb_icon (xwindow,
                         ideal_width, ideal_height,
                         ideal_mini_width, ideal_mini_height,
                         &w, &h, &pixdata,
                         &mini_w, &mini_h, &mini_pixdata))
        {
          *iconp = gdk_pixbuf_new_from_data (pixdata,
                                             GDK_COLORSPACE_RGB,
                                             TRUE,
                                             8,
                                             w, h, w * 4,
                                             free_pixels,
                                             NULL);
          
          *mini_iconp = gdk_pixbuf_new_from_data (mini_pixdata,
                                                  GDK_COLORSPACE_RGB,
                                                  TRUE,
                                                  8,
                                                  mini_w, mini_h, mini_w * 4,
                                                  free_pixels,
                                                  NULL);

          replace_cache (icon_cache, USING_NET_WM_ICON,
                         *iconp, *mini_iconp);

          return TRUE;
        }
    }

  if (icon_cache->origin <= USING_WM_NORMAL_HINTS &&
      icon_cache->wm_normal_hints_dirty)
    {
      icon_cache->wm_normal_hints_dirty = FALSE;
      
      _wnck_error_trap_push ();
      hints = XGetWMHints (gdk_display, xwindow);
      _wnck_error_trap_pop ();
      pixmap = None;
      mask = None;
      if (hints)
        {
          if (hints->flags & IconPixmapHint)
            pixmap = hints->icon_pixmap;
          if (hints->flags & IconMaskHint)
            mask = hints->icon_mask;

          XFree (hints);
          hints = NULL;
        }

      /* We won't update if pixmap is unchanged;
       * avoids a get_from_drawable() on every geometry
       * hints change
       */
      if ((pixmap != icon_cache->prev_pixmap ||
           mask != icon_cache->prev_mask) &&
          pixmap != None)
        {
          if (try_pixmap_and_mask (pixmap, mask,
                                   iconp, ideal_width, ideal_height,
                                   mini_iconp, ideal_mini_width, ideal_mini_height))
            {
              icon_cache->prev_pixmap = pixmap;
              icon_cache->prev_mask = mask;

              replace_cache (icon_cache, USING_WM_NORMAL_HINTS,
                             *iconp, *mini_iconp);

              return TRUE;
            }
        }
    }

  if (icon_cache->origin <= USING_KWM_WIN_ICON &&
      icon_cache->kwm_win_icon_dirty)
    {
      icon_cache->kwm_win_icon_dirty = FALSE;
      
      get_kwm_win_icon (xwindow, &pixmap, &mask);

      if ((pixmap != icon_cache->prev_pixmap ||
           mask != icon_cache->prev_mask) &&
          pixmap != None)
        {
          if (try_pixmap_and_mask (pixmap, mask,
                                   iconp, ideal_width, ideal_height,
                                   mini_iconp, ideal_mini_width, ideal_mini_height))
            {
              icon_cache->prev_pixmap = pixmap;
              icon_cache->prev_mask = mask;

              replace_cache (icon_cache, USING_KWM_WIN_ICON,
                             *iconp, *mini_iconp);
              
              return TRUE;
            }
        }
    }
  
  if (icon_cache->want_fallback &&
      icon_cache->origin < USING_FALLBACK_ICON)
    {
      _wnck_get_fallback_icons (iconp,
                                ideal_width,
                                ideal_height,
                                mini_iconp,
                                ideal_mini_width,
                                ideal_mini_height);

      replace_cache (icon_cache, USING_FALLBACK_ICON,
                     *iconp, *mini_iconp);

      return TRUE;
    }

  if (!icon_cache->want_fallback &&
      icon_cache->origin == USING_FALLBACK_ICON)
    {
      /* Get rid of current icon */
      clear_icon_cache (icon_cache, FALSE);

      return TRUE;
    }

  /* found nothing new */
  return FALSE;
}

#ifdef HAVE_GDK_PIXBUF_NEW_FROM_STREAM
#define gdk_pixbuf_new_from_inline gdk_pixbuf_new_from_stream
#endif

static GdkPixbuf*
default_icon_at_size (int width,
                      int height)
{  

  GdkPixbuf *base;
  
  base = gdk_pixbuf_new_from_inline (-1, default_icon_data,
                                     FALSE,
                                     NULL);
  
  g_assert (base);

  if ((width < 0 && height < 0) ||
      (gdk_pixbuf_get_width (base) == width &&
       gdk_pixbuf_get_height (base) == height))
    {
      return base;
    }
  else
    {
      GdkPixbuf *scaled;
      
      scaled = gdk_pixbuf_scale_simple (base,
                                        width > 0 ? width :
                                        gdk_pixbuf_get_width (base),
                                        height > 0 ? height :
                                        gdk_pixbuf_get_height (base),
                                        GDK_INTERP_BILINEAR);
      
      g_object_unref (G_OBJECT (base));
      
      return scaled;
    }
}

void
_wnck_get_fallback_icons (GdkPixbuf **iconp,
                          int         ideal_width,
                          int         ideal_height,
                          GdkPixbuf **mini_iconp,
                          int         ideal_mini_width,
                          int         ideal_mini_height)
{
  *iconp = default_icon_at_size (ideal_width > 0 ? ideal_width :
                                 DEFAULT_ICON_WIDTH,
                                 ideal_height > 0 ? ideal_height :
                                 DEFAULT_ICON_HEIGHT);

  *mini_iconp = default_icon_at_size (ideal_mini_width > 0 ? ideal_mini_width :
                                      DEFAULT_MINI_ICON_WIDTH,
                                      ideal_mini_height > 0 ? ideal_mini_height :
                                      DEFAULT_MINI_ICON_HEIGHT);
}


void
_wnck_get_window_geometry (Window xwindow,
                           int   *xp,
                           int   *yp,
                           int   *widthp,
                           int   *heightp)
{
  int x, y, width, height, bw, depth;
  Window root_window;

  width = 1;
  height = 1;
  
  _wnck_error_trap_push ();

  XGetGeometry (gdk_display,
                xwindow,
                &root_window,
                &x, &y, &width, &height, &bw, &depth);
  
  _wnck_error_trap_pop ();

  _wnck_get_window_position (xwindow, xp, yp);

  if (widthp)
    *widthp = width;
  if (heightp)
    *heightp = height;
}

void
_wnck_get_window_position (Window xwindow,
                           int   *xp,
                           int   *yp)
{
  int x, y;
  Window child;

  x = 0;
  y = 0;
  
  _wnck_error_trap_push ();
  XTranslateCoordinates (gdk_display,
                         xwindow,
                         gdk_x11_get_default_root_xwindow (),
                         0, 0,
                         &x, &y, &child);
  _wnck_error_trap_pop ();

  if (xp)
    *xp = x;
  if (yp)
    *yp = y;
}

void
_wnck_set_icon_geometry  (Window xwindow,
			  int    x,
			  int    y,
			  int    width,
			  int    height)
{
  gulong data[4];

  data[0] = x;
  data[1] = y;
  data[2] = width;
  data[3] = height;
  
  _wnck_error_trap_push ();

  XChangeProperty (gdk_display,
		   xwindow,
		   _wnck_atom_get ("_NET_WM_ICON_GEOMETRY"),
		   XA_CARDINAL, 32, PropModeReplace,
		   (guchar *)&data, 4);

  _wnck_error_trap_pop ();
}

void
_wnck_set_dock_type_hint (Window xwindow)
{
  Atom atom;

  atom = _wnck_atom_get ("_NET_WM_WINDOW_TYPE_DOCK");
  
  _wnck_error_trap_push ();

  XChangeProperty (gdk_display,
                   xwindow, 
                   _wnck_atom_get ("_NET_WM_WINDOW_TYPE"),
		   XA_ATOM, 32, PropModeReplace,
		   (guchar *)&atom, 1);

  _wnck_error_trap_pop ();
}
