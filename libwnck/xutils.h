/* Xlib utilities */

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

#ifndef WNCK_XUTILS_H
#define WNCK_XUTILS_H

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

G_BEGIN_DECLS

#define WNCK_APP_WINDOW_EVENT_MASK (PropertyChangeMask | StructureNotifyMask)

gboolean _wnck_get_cardinal      (Window  xwindow,
                                  Atom    atom,
                                  int    *val);
int      _wnck_get_wm_state      (Window  xwindow);
gboolean _wnck_get_window        (Window  xwindow,
                                  Atom    atom,
                                  Window *val);
gboolean _wnck_get_pixmap        (Window  xwindow,
                                  Atom    atom,
                                  Pixmap *val);
gboolean _wnck_get_atom          (Window  xwindow,
                                  Atom    atom,
                                  Atom   *val);
char*    _wnck_get_text_property (Window  xwindow,
                                  Atom    atom);
char*    _wnck_get_utf8_property (Window  xwindow,
                                  Atom    atom);
gboolean _wnck_get_window_list   (Window   xwindow,
                                  Atom     atom,
                                  Window **windows,
                                  int     *len);
gboolean _wnck_get_atom_list     (Window   xwindow,
                                  Atom     atom,
                                  Atom   **atoms,
                                  int     *len);
gboolean _wnck_get_cardinal_list (Window   xwindow,
                                  Atom     atom,
                                  gulong **cardinals,
                                  int     *len);
char**   _wnck_get_utf8_list     (Window   xwindow,
                                  Atom     atom);

void _wnck_error_trap_push (void);
int  _wnck_error_trap_pop  (void);

Atom        _wnck_atom_get  (const char *atom_name);
const char* _wnck_atom_name (Atom        atom);

void _wnck_event_filter_init (void);

int   _wnck_xid_equal (gconstpointer v1,
                       gconstpointer v2);
guint _wnck_xid_hash  (gconstpointer v);

void _wnck_iconify   (Window xwindow);
void _wnck_deiconify (Window xwindow);

void _wnck_close     (Screen *screen,
		      Window  xwindow);

void _wnck_change_state     (Screen  *screen,
			     Window   xwindow,
                             gboolean add,
                             Atom     state1,
                             Atom     state2);
void _wnck_change_workspace (Screen  *screen,
			     Window   xwindow,
                             int      new_space);
void _wnck_activate         (Screen  *screen,
			     Window   xwindow);
void _wnck_activate_workspace (Screen *screen,
			       int     new_active_space);

Window _wnck_get_group_leader   (Window xwindow);
char*  _wnck_get_session_id     (Window xwindow);
int    _wnck_get_pid            (Window xwindow);
char*  _wnck_get_name           (Window xwindow);
char*  _wnck_get_res_class_utf8 (Window xwindow);

void   _wnck_select_input     (Window xwindow,
                               int    mask);

typedef struct _WnckIconCache WnckIconCache;

WnckIconCache *_wnck_icon_cache_new                  (void);
void           _wnck_icon_cache_free                 (WnckIconCache *icon_cache);
void           _wnck_icon_cache_property_changed     (WnckIconCache *icon_cache,
                                                      Atom           atom);
gboolean       _wnck_icon_cache_get_icon_invalidated (WnckIconCache *icon_cache);
void           _wnck_icon_cache_set_want_fallback    (WnckIconCache *icon_cache,
                                                      gboolean       setting);
gboolean       _wnck_icon_cache_get_is_fallback      (WnckIconCache *icon_cache);

gboolean _wnck_read_icons         (Window          xwindow,
                                   WnckIconCache  *icon_cache,
                                   GdkPixbuf     **iconp,
                                   int             ideal_width,
                                   int             ideal_height,
                                   GdkPixbuf     **mini_iconp,
                                   int             ideal_mini_width,
                                   int             ideal_mini_height);
void _wnck_get_fallback_icons (GdkPixbuf     **iconp,
                               int             ideal_width,
                               int             ideal_height,
                               GdkPixbuf     **mini_iconp,
                               int             ideal_mini_width,
                               int             ideal_mini_height);



void _wnck_get_window_geometry (Screen *screen,
				Window  xwindow,
                                int    *xp,
                                int    *yp,
                                int    *widthp,
                                int    *heightp);

void _wnck_get_window_position (Screen *screen,
				Window  xwindow,
                                int    *xp,
                                int    *yp);

void _wnck_set_icon_geometry  (Window xwindow,
			       int    x,
			       int    y,
			       int    width,
			       int    height);

void _wnck_set_dock_type_hint (Window xwindow);

GdkPixbuf* _wnck_gdk_pixbuf_get_from_pixmap (GdkPixbuf   *dest,
                                             Pixmap       xpixmap,
                                             int          src_x,
                                             int          src_y,
                                             int          dest_x,
                                             int          dest_y,
                                             int          width,
                                             int          height);

G_END_DECLS

#endif /* WNCK_XUTILS_H */
