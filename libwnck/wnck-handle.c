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

/**
 * SECTION: wnck-handle
 * @short_description: an object representing a handle.
 *
 * This is the main entry point into the libwnck library.
 */

#include "config.h"
#include "wnck-handle-private.h"

struct _WnckHandle
{
  GObject parent;
};

G_DEFINE_TYPE (WnckHandle, wnck_handle, G_TYPE_OBJECT)

static void
wnck_handle_class_init (WnckHandleClass *self_class)
{
}

static void
wnck_handle_init (WnckHandle *self)
{
}
