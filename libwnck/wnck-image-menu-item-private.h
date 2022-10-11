/*
 * Copyright (C) 2016 Alberts Muktupāvels
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

#ifndef WNCK_IMAGE_MENU_ITEM_PRIVATE_H
#define WNCK_IMAGE_MENU_ITEM_PRIVATE_H

#include <gtk/gtk.h>
#include "window.h"

G_BEGIN_DECLS

#define WNCK_TYPE_IMAGE_MENU_ITEM wnck_image_menu_item_get_type ()
G_DECLARE_FINAL_TYPE (WnckImageMenuItem, wnck_image_menu_item,
                      WNCK, IMAGE_MENU_ITEM, GtkMenuItem)

GtkWidget *wnck_image_menu_item_new                        (void);

GtkWidget *wnck_image_menu_item_new_with_label             (const gchar       *label);

void       wnck_image_menu_item_set_image_from_icon_pixbuf (WnckImageMenuItem *item,
                                                            GdkPixbuf         *pixbuf);

void       wnck_image_menu_item_set_image_from_window      (WnckImageMenuItem *item,
                                                            WnckWindow        *window);

void       wnck_image_menu_item_make_label_bold            (WnckImageMenuItem *item);

void       wnck_image_menu_item_make_label_normal          (WnckImageMenuItem *item);

void       _wnck_image_menu_item_set_max_chars             (WnckImageMenuItem *self,
                                                            int                n_chars);

G_END_DECLS

#endif
