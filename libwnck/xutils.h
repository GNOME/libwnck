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

gboolean _wnck_get_cardinal      (Window  xwindow,
                                  Atom    atom,
                                  int    *val);
int      _wnck_get_wm_state      (Window  xwindow);
gboolean _wnck_get_window        (Window  xwindow,
                                  Atom    atom,
                                  Window *val);
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

void _wnck_change_state     (Window   xwindow,
                             gboolean add,
                             Atom     state1,
                             Atom     state2);
void _wnck_change_workspace (Window   xwindow,
                             int      new_space);
void _wnck_activate         (Window   xwindow);

Window _wnck_get_group_leader (Window xwindow);
char*  _wnck_get_session_id   (Window xwindow);
int    _wnck_get_pid          (Window xwindow);

void   _wnck_select_input     (Window xwindow,
                               int    mask);


void _wnck_read_icons         (Window      xwindow,
                               GdkPixbuf **iconp,
                               int         ideal_width,
                               int         ideal_height,
                               GdkPixbuf **mini_iconp,
                               int         ideal_mini_width,
                               int         ideal_mini_height);
void _wnck_get_fallback_icons (GdkPixbuf **iconp,
                               int         ideal_width,
                               int         ideal_height,
                               GdkPixbuf **mini_iconp,
                               int         ideal_mini_width,
                               int         ideal_mini_height);


G_END_DECLS

#endif /* WNCK_XUTILS_H */
