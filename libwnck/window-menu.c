/* window menu */

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

#include "window-menu.h"

static void object_weak_notify (gpointer data,
                                GObject *obj);
static void window_weak_notify (gpointer data,
                                GObject *window);

static void
window_weak_notify (gpointer data,
                    GObject *window)
{
  g_object_set_data (G_OBJECT (data), "wnck-window-data", NULL);
  g_object_weak_unref (G_OBJECT (data),
                       object_weak_notify,
                       window);
}

static void
set_window (GObject    *obj,
            WnckWindow *win)
{
  g_object_set_data (obj, "wnck-window-data", win);
  if (win)
    {
      g_object_weak_ref (G_OBJECT (win), window_weak_notify, obj);
      g_object_weak_ref (obj, object_weak_notify, win);
    }
}

static void
object_weak_notify (gpointer data,
                    GObject *obj)
{
  g_object_weak_unref (G_OBJECT (data),
                       window_weak_notify,
                       obj);
}

static WnckWindow*
get_window (GObject *obj)
{
  return g_object_get_data (obj, "wnck-window-data");
}

static void
item_activated_callback (GtkWidget *menu_item,
                         gpointer   data)
{
  WnckWindow *win;

  win = get_window (G_OBJECT (menu_item));

  if (win == NULL)
    return;

  wnck_window_activate (win);
}

GtkWidget*
wnck_create_window_menu (GList *windows)
{
  GList *tmp;
  GtkWidget *menu;

  menu = gtk_menu_new ();
  
  tmp = windows;

  while (tmp != NULL)
    {
      WnckWindow *win = WNCK_WINDOW (tmp->data);
      GdkPixbuf *icon;
      const char *title;
      GtkWidget *mi;
      
      icon = wnck_window_get_icon (win);
      title = wnck_window_get_icon_name (win);

      if (icon)
        {
          GtkWidget *image;

          image = gtk_image_new_from_pixbuf (icon);
          
          mi = gtk_image_menu_item_new_with_label (title);
          gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi),
                                         image);
        }
      else
        {
          mi = gtk_menu_item_new_with_label (title);
        }

      g_signal_connect (G_OBJECT (mi), "activate",
                        G_CALLBACK (item_activated_callback), NULL);

      set_window (G_OBJECT (mi), win);
      
      gtk_widget_show (mi);
      
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      
      tmp = tmp->next;
    }

  return menu;
}

