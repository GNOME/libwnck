/* util header */

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

#include "util.h"
#include "xutils.h"
#include "private.h"
#include <gdk/gdkx.h>
#include <string.h>
#ifdef HAVE_XRES
#include <X11/extensions/XRes.h>
#endif

static void
set_dock_realize_handler (GtkWidget *widget, gpointer data)
{
  _wnck_set_dock_type_hint (GDK_WINDOW_XWINDOW (widget->window));
}

void
wnck_gtk_window_set_dock_type (GtkWindow *window)
{
  /*  FIXME don't set it 10 times if this function is called 10 times */
  g_signal_connect (G_OBJECT (window),
                    "realize",
                    G_CALLBACK (set_dock_realize_handler),
                    NULL);
}

typedef enum
{
  WNCK_EXT_UNKNOWN = 0,
  WNCK_EXT_FOUND = 1,
  WNCK_EXT_MISSING = 2
} WnckExtStatus;

void
wnck_xid_read_resource_usage (GdkDisplay        *gdisplay,
                              unsigned long      xid,
                              WnckResourceUsage *usage)
{
  int event, error;
  Display *xdisplay;
  WnckExtStatus status;

  xdisplay = GDK_DISPLAY_XDISPLAY (gdisplay);

  status = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (gdisplay),
                                               "wnck-xres-status"));

  if (status == WNCK_EXT_UNKNOWN)
    {
#ifdef HAVE_XRES
      if (!XResQueryExtension (xdisplay, &event, &error))
        status = WNCK_EXT_MISSING;
      else
        status = WNCK_EXT_FOUND;
#else
      status = WNCK_EXT_MISSING;
#endif
      
      g_object_set_data (G_OBJECT (gdisplay),
                         "wnck-xres-status",
                         GINT_TO_POINTER (status));
    }

  g_assert (status != WNCK_EXT_UNKNOWN);

  memset (usage, '\0', sizeof (*usage));
  
  if (status == WNCK_EXT_MISSING)
    return;

#ifdef HAVE_XRES
 {
   XResType *types;
   int n_types;
   unsigned long pixmap_bytes;
   int i;
   Atom pixmap_atom;
   Atom window_atom;
   Atom gc_atom;
   Atom picture_atom;
   Atom glyphset_atom;
   Atom font_atom;
   Atom colormap_entry_atom;
   Atom passive_grab_atom;
   Atom cursor_atom;
   
   types = NULL;
   n_types = 0;
   pixmap_bytes = 0;
   
  _wnck_error_trap_push ();

  XResQueryClientResources (xdisplay,
                             xid, &n_types,
                             &types);

   XResQueryClientPixmapBytes (xdisplay,
                               xid, &pixmap_bytes);
   _wnck_error_trap_pop ();   
   
   usage->pixmap_bytes = pixmap_bytes;

   pixmap_atom = _wnck_atom_get ("PIXMAP");
   window_atom = _wnck_atom_get ("WINDOW");
   gc_atom = _wnck_atom_get ("GC");
   picture_atom = _wnck_atom_get ("FONT");
   glyphset_atom = _wnck_atom_get ("GLYPHSET");
   font_atom = _wnck_atom_get ("PICTURE");
   colormap_entry_atom = _wnck_atom_get ("COLORMAP ENTRY");
   passive_grab_atom = _wnck_atom_get ("PASSIVE GRAB");
   cursor_atom = _wnck_atom_get ("CURSOR");
   
   i = 0;
   while (i < n_types)
     {
       int t = types[i].resource_type;
       
       if (t == pixmap_atom)
         usage->n_pixmaps += types[i].count;
       else if (t == window_atom)
         usage->n_windows += types[i].count;
       else if (t == gc_atom)
         usage->n_gcs += types[i].count;
       else if (t == picture_atom)
         usage->n_pictures += types[i].count;
       else if (t == glyphset_atom)
         usage->n_glyphsets += types[i].count;
       else if (t == font_atom)
         usage->n_fonts += types[i].count;
       else if (t == colormap_entry_atom)
         usage->n_colormap_entries += types[i].count;
       else if (t == passive_grab_atom)
         usage->n_passive_grabs += types[i].count;
       else if (t == cursor_atom)
         usage->n_cursors += types[i].count;
       else
         usage->n_other += types[i].count;
       
       ++i;
     }

   XFree(types);

   usage->total_bytes_estimate = usage->pixmap_bytes;

   /* FIXME look in the X server source and come up with better
    * answers here. Ideally we change XRes to return a number
    * like this since it can do things like divide the cost of
    * a shared resource among those sharing it.
    */
   usage->total_bytes_estimate += usage->n_windows * 24;
   usage->total_bytes_estimate += usage->n_gcs * 24;
   usage->total_bytes_estimate += usage->n_pictures * 24;
   usage->total_bytes_estimate += usage->n_glyphsets * 24;
   usage->total_bytes_estimate += usage->n_fonts * 1024;
   usage->total_bytes_estimate += usage->n_colormap_entries * 24;
   usage->total_bytes_estimate += usage->n_passive_grabs * 24;
   usage->total_bytes_estimate += usage->n_cursors * 24;
   usage->total_bytes_estimate += usage->n_other * 24;
 }
#else /* HAVE_XRES */
  g_assert_not_reached ();
#endif /* HAVE_XRES */
}

void
wnck_pid_read_resource_usage (GdkDisplay        *gdisplay,
                              unsigned long      pid,
                              WnckResourceUsage *usage)
{
  Display *xdisplay;
  int i;

  /* FIXME to be fast and correct, we need to avoid the linear search and
   * we should assume a process ID can have more than one X connection.
   * So we would essentially want to maintain a map from process ID
   * to the X resource base/mask, and query resources for the base/mask.
   */
  
  xdisplay = GDK_DISPLAY_XDISPLAY (gdisplay);

  memset (usage, '\0', sizeof (*usage));
  
  i = 0;
  while (i < ScreenCount (xdisplay))
    {
      WnckScreen *screen;
      GList *windows;
      GList *tmp;
      
      screen = wnck_screen_get (i);

      g_assert (screen != NULL);

      windows = wnck_screen_get_windows (screen);
      tmp = windows;
      while (tmp != NULL)
        {
          if (wnck_window_get_pid (tmp->data) == pid)
            {
              wnck_xid_read_resource_usage (gdisplay,
                                            wnck_window_get_xid (tmp->data),
                                            usage);

              /* stop on first window found */
              return;
            }

          tmp = tmp->next;
        }
      
      ++i;
    }
}

void
_wnck_init (void)
{
  static gboolean done = FALSE;

  if (!done)
    {
      bindtextdomain (GETTEXT_PACKAGE, WNCK_LOCALEDIR);
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
      done = TRUE;
    }
}
