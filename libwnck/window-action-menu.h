/* window action menu (ops on a single window) */

/*
 * Copyright (C) 2001 Havoc Pennington
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef WNCK_WINDOW_ACTION_MENU_H
#define WNCK_WINDOW_ACTION_MENU_H

#include <libwnck/window.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget* wnck_create_window_action_menu (WnckWindow *window);

G_END_DECLS

#endif /* WNCK_WINDOW_MENU_H */
