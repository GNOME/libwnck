/*
 * Copyright (C) 2021 Alberts Muktupāvels
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

#include "util.h"

G_BEGIN_DECLS

typedef struct _WnckHandle WnckHandle;

WnckHandle     *_wnck_handle_new             (WnckClientType  client_type);

WnckClientType  _wnck_handle_get_client_type (WnckHandle     *self);

G_END_DECLS

#endif
