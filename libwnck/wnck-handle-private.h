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
#include "wnck-handle.h"

G_BEGIN_DECLS

WnckClientType   _wnck_handle_get_client_type            (WnckHandle     *self);

gboolean         _wnck_handle_has_xres                   (WnckHandle     *self);

void             _wnck_handle_insert_class_group         (WnckHandle      *self,
                                                          const char      *id,
                                                          WnckClassGroup  *class_group);

void             _wnck_handle_remove_class_group         (WnckHandle      *self,
                                                          const char      *id);

void             _wnck_handle_insert_application         (WnckHandle      *self,
                                                          gpointer         xwindow,
                                                          WnckApplication *app);

void             _wnck_handle_remove_application         (WnckHandle      *self,
                                                          gpointer         xwindow);

WnckApplication *_wnck_handle_get_application_from_res_class (WnckHandle  *self,
                                                              const char  *res_class);

void             _wnck_handle_insert_window              (WnckHandle      *self,
                                                          gpointer         xwindow,
                                                          WnckWindow      *window);

void             _wnck_handle_remove_window              (WnckHandle      *self,
                                                          gpointer         xwindow);

G_END_DECLS

#endif
