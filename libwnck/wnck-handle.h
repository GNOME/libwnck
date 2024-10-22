/*
 * Copyright (C) 2021 Alberts Muktupāvels
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

#if !defined (__LIBWNCK_H_INSIDE__) && !defined (WNCK_COMPILATION)
#error "Only <libwnck/libwnck.h> can be included directly."
#endif

#ifndef WNCK_HANDLE_H
#define WNCK_HANDLE_H

#include <glib-object.h>
#include <libwnck/screen.h>
#include <libwnck/util.h>
#include <libwnck/wnck-macros.h>

G_BEGIN_DECLS

#define WNCK_TYPE_HANDLE (wnck_handle_get_type ())

WNCK_EXPORT
G_DECLARE_FINAL_TYPE (WnckHandle, wnck_handle, WNCK, HANDLE, GObject)

WNCK_EXPORT
WnckHandle      *wnck_handle_new                        (WnckClientType     client_type);

WNCK_EXPORT
WnckScreen      *wnck_handle_get_default_screen         (WnckHandle        *self);

WNCK_EXPORT
WnckScreen      *wnck_handle_get_screen                 (WnckHandle        *self,
                                                         int                index);

WNCK_EXPORT
WnckScreen      *wnck_handle_get_screen_for_root        (WnckHandle        *self,
                                                         gulong             root_window_id);

WNCK_EXPORT
void             wnck_handle_set_default_icon_size      (WnckHandle        *self,
                                                         gsize              icon_size);

WNCK_EXPORT
gsize            wnck_handle_get_default_icon_size      (WnckHandle        *self);

WNCK_EXPORT
void             wnck_handle_set_default_mini_icon_size (WnckHandle        *self,
                                                         gsize              icon_size);

WNCK_EXPORT
gsize            wnck_handle_get_default_mini_icon_size (WnckHandle        *self);

WNCK_EXPORT
WnckClassGroup  *wnck_handle_get_class_group            (WnckHandle        *self,
                                                         const char        *id);

WNCK_EXPORT
WnckApplication *wnck_handle_get_application            (WnckHandle        *self,
                                                         gulong             xwindow);

WNCK_EXPORT
WnckWindow      *wnck_handle_get_window                 (WnckHandle        *self,
                                                         gulong             xwindow);

G_END_DECLS

#endif
