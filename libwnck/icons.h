/* util header */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2006-2007 Vincent Untz
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__LIBWNCK_H_INSIDE__) && !defined (WNCK_COMPILATION)
#error "Only <libwnck/libwnck.h> can be included directly."
#endif

#ifndef WNCK_ICONS_H
#define WNCK_ICONS_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WNCK_DEFAULT_ICON_SIZE 32
#define WNCK_DEFAULT_MINI_ICON_SIZE 16

G_DEPRECATED_FOR(wnck_handle_set_default_icon_size)
void wnck_set_default_icon_size      (gsize size);

G_DEPRECATED_FOR(wnck_handle_set_default_mini_icon_size)
void wnck_set_default_mini_icon_size (gsize size);

G_END_DECLS

#endif /* WNCK_ICONS_H */
