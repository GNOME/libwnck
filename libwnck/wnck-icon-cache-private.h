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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WNCK_ICON_CACHE_PRIVATE_H
#define WNCK_ICON_CACHE_PRIVATE_H

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <libwnck/screen.h>

G_BEGIN_DECLS

#define WNCK_TYPE_ICON_CACHE (_wnck_icon_cache_get_type ())
G_DECLARE_FINAL_TYPE (WnckIconCache, _wnck_icon_cache, WNCK, ICON_CACHE, GObject)

WnckIconCache *_wnck_icon_cache_new                  (Window          xwindow,
                                                      WnckScreen     *screen);
void           _wnck_icon_cache_property_changed     (WnckIconCache  *icon_cache,
                                                      Atom            atom,
                                                      XWMHints       *hints);
void           _wnck_icon_cache_set_want_fallback    (WnckIconCache  *icon_cache,
                                                      gboolean        setting);
gboolean       _wnck_icon_cache_get_is_fallback      (WnckIconCache  *icon_cache);

GdkPixbuf     *_wnck_icon_cache_get_icon             (WnckIconCache  *self);

GdkPixbuf     *_wnck_icon_cache_get_mini_icon        (WnckIconCache  *self);

void           _wnck_icon_cache_invalidate           (WnckIconCache  *self);

G_END_DECLS

#endif
