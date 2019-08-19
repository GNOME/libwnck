/*
 * Copyright (C) 2019 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WNCK_HANDLE_PRIVATE_H
#define WNCK_HANDLE_PRIVATE_H

#include "wnck-handle.h"

G_BEGIN_DECLS

WnckClientType  wnck_handle_get_client_type            (WnckHandle *self);

WnckScreen     *wnck_handle_get_existing_screen        (WnckHandle *self,
                                                        int         number);

gsize           wnck_handle_get_default_icon_size      (WnckHandle *self);

gsize           wnck_handle_get_default_mini_icon_size (WnckHandle *self);

G_END_DECLS

#endif
