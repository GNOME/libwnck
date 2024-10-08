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

#ifndef WNCK_UTIL_H
#define WNCK_UTIL_H

#include <libwnck/misc.h>
#include <libwnck/icons.h>
#include <libwnck/resource.h>
#include <libwnck/window.h>

G_BEGIN_DECLS

WnckHandle *wnck_get_handle (void);

void wnck_selector_set_window_icon (GtkWidget  *image,
                                    WnckWindow *window);

void wnck_make_gtk_label_bold   (GtkLabel *label);
void wnck_make_gtk_label_normal (GtkLabel *label);

G_END_DECLS

#endif /* WNCK_UTIL_H */
