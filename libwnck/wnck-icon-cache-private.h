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
#include <libwnck/screen.h>

G_BEGIN_DECLS

typedef struct _WnckIconCache WnckIconCache;

WnckIconCache *_wnck_icon_cache_new                  (void);
void           _wnck_icon_cache_free                 (WnckIconCache  *icon_cache);
void           _wnck_icon_cache_property_changed     (WnckIconCache  *icon_cache,
                                                      Atom            atom);
gboolean       _wnck_icon_cache_get_icon_invalidated (WnckIconCache  *icon_cache);
void           _wnck_icon_cache_set_want_fallback    (WnckIconCache  *icon_cache,
                                                      gboolean        setting);
gboolean       _wnck_icon_cache_get_is_fallback      (WnckIconCache  *icon_cache);

gboolean       _wnck_read_icons                      (WnckScreen     *screen,
                                                      Window          xwindow,
                                                      WnckIconCache  *icon_cache,
                                                      GdkPixbuf     **iconp,
                                                      int             ideal_size,
                                                      GdkPixbuf     **mini_iconp,
                                                      int             ideal_mini_size);

G_END_DECLS

#endif
