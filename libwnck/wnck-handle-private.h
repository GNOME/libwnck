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

#ifndef WNCK_HANDLE_PRIVATE_H
#define WNCK_HANDLE_PRIVATE_H

#include "class-group.h"
#include "util.h"

G_BEGIN_DECLS

typedef struct _WnckHandle WnckHandle;

WnckHandle      *_wnck_handle_new                        (WnckClientType  client_type);

WnckClientType   _wnck_handle_get_client_type            (WnckHandle     *self);

WnckScreen      *_wnck_handle_get_default_screen         (WnckHandle     *self);

WnckScreen      *_wnck_handle_get_screen                 (WnckHandle     *self,
                                                          int             index);

WnckScreen      *_wnck_handle_get_screen_for_root        (WnckHandle     *self,
                                                          gulong          root_window_id);

WnckScreen      *_wnck_handle_get_existing_screen        (WnckHandle     *self,
                                                          int             number);

void             _wnck_handle_set_default_icon_size      (WnckHandle     *self,
                                                          gsize           icon_size);

gsize            _wnck_handle_get_default_icon_size      (WnckHandle     *self);

void             _wnck_handle_set_default_mini_icon_size (WnckHandle     *self,
                                                          gsize           icon_size);

gsize            _wnck_handle_get_default_mini_icon_size (WnckHandle     *self);

void             _wnck_handle_insert_class_group         (WnckHandle      *self,
                                                          const char      *id,
                                                          WnckClassGroup  *class_group);

void             _wnck_handle_remove_class_group         (WnckHandle      *self,
                                                          const char      *id);

WnckClassGroup  *_wnck_handle_get_class_group            (WnckHandle      *self,
                                                          const char      *id);

void             _wnck_handle_insert_application         (WnckHandle      *self,
                                                          gpointer         xwindow,
                                                          WnckApplication *app);

void             _wnck_handle_remove_application         (WnckHandle      *self,
                                                          gpointer         xwindow);

WnckApplication *_wnck_handle_get_application            (WnckHandle      *self,
                                                          gulong           xwindow);

void             _wnck_handle_insert_window              (WnckHandle      *self,
                                                          gpointer         xwindow,
                                                          WnckWindow      *window);

void             _wnck_handle_remove_window              (WnckHandle      *self,
                                                          gpointer         xwindow);

WnckWindow      *_wnck_handle_get_window                 (WnckHandle      *self,
                                                          gulong           xwindow);

G_END_DECLS

#endif
