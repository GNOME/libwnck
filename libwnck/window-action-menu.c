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

#include "window-action-menu.h"
#include "private.h"

typedef enum
{
  CLOSE,
  MINIMIZE,
  MAXIMIZE,
  SHADE,
  MOVE,
  RESIZE
} WindowAction;

typedef struct _ActionMenuData ActionMenuData;

struct _ActionMenuData
{
  WnckWindow *window;
  GtkWidget *menu;
  GtkWidget *close_item;
  GtkWidget *minimize_item;
  GtkWidget *maximize_item;
  GtkWidget *shade_item;
  GtkWidget *move_item;
  GtkWidget *resize_item;
  guint idle_handler;
};

static void object_weak_notify (gpointer data,
                                GObject *obj);
static void window_weak_notify (gpointer data,
                                GObject *window);

static void
window_weak_notify (gpointer data,
                    GObject *window)
{
  g_object_set_data (G_OBJECT (data), "wnck-action-data", NULL);
  g_object_weak_unref (G_OBJECT (data),
                       object_weak_notify,
                       window);
}


static void
object_weak_notify (gpointer data,
                    GObject *obj)
{
  g_object_weak_unref (G_OBJECT (data),
                       window_weak_notify,
                       obj);
}

static void
set_data (GObject        *obj,
          ActionMenuData *amd)
{
  g_object_set_data (obj, "wnck-action-data", amd);
  if (amd && amd->window)
    {
      g_object_weak_ref (G_OBJECT (amd->window), window_weak_notify, obj);
      g_object_weak_ref (obj, object_weak_notify, amd->window);
    }
}

static ActionMenuData*
get_data (GObject *obj)
{
  return g_object_get_data (obj, "wnck-action-data");
}

static void
item_activated_callback (GtkWidget *menu_item,
                         gpointer   data)
{
  ActionMenuData *amd = get_data (G_OBJECT (menu_item));
  WindowAction action = GPOINTER_TO_INT (data);
  
  if (amd == NULL)
    return;

  switch (action)
    {
    case CLOSE:
      wnck_window_close (amd->window);
      break;
    case MINIMIZE:
      if (wnck_window_is_minimized (amd->window))
        wnck_window_unminimize (amd->window);
      else
        wnck_window_minimize (amd->window);
      break;
    case MAXIMIZE:
      if (wnck_window_is_maximized (amd->window))
        wnck_window_unmaximize (amd->window);
      else
        wnck_window_maximize (amd->window);
      break;
    case SHADE:
      if (wnck_window_is_shaded (amd->window))
        wnck_window_unshade (amd->window);
      else
        wnck_window_shade (amd->window);
      break;
    case MOVE:
      wnck_window_keyboard_move (amd->window);
      break;
    case RESIZE:
      wnck_window_keyboard_size (amd->window);
      break;
    }
}

static void
set_item_text (GtkWidget  *mi,
               const char *text)
{
  gtk_label_set_text (GTK_LABEL (GTK_BIN (mi)->child),
                      text);
  gtk_label_set_use_underline (GTK_LABEL (GTK_BIN (mi)->child), TRUE);
}

static void
set_item_stock (GtkWidget  *mi,
                const char *stock_id)
{
  GtkWidget *image;
  
  image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (mi));

  if (stock_id == NULL)
    {
      if (image != NULL)
        gtk_widget_destroy (image);
      return;
    }

  if (image == NULL)
    {
      image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), image);
    }
  else
    {
      gtk_image_set_from_stock (GTK_IMAGE (image), stock_id,
                                GTK_ICON_SIZE_MENU);
    }
}

static gboolean
update_menu_state (ActionMenuData *amd)
{
  WnckWindowActions actions;  

  amd->idle_handler = 0;
  
  actions = wnck_window_get_actions (amd->window);
  
  if (wnck_window_is_minimized (amd->window))
    {
      set_item_text (amd->minimize_item, _("Un_minimize"));
      set_item_stock (amd->minimize_item, NULL);
      gtk_widget_set_sensitive (amd->minimize_item,
                                (actions & WNCK_WINDOW_ACTION_UNMINIMIZE) != 0);
    }
  else
    {
      set_item_text (amd->minimize_item, _("_Minimize"));
      set_item_stock (amd->minimize_item, WNCK_STOCK_MINIMIZE);
      gtk_widget_set_sensitive (amd->minimize_item,
                                (actions & WNCK_WINDOW_ACTION_MINIMIZE) != 0);
    }

  if (wnck_window_is_maximized (amd->window))
    {
      set_item_text (amd->maximize_item, _("_Unmaximize"));
      set_item_stock (amd->maximize_item, NULL);
      gtk_widget_set_sensitive (amd->maximize_item,
                                (actions & WNCK_WINDOW_ACTION_UNMAXIMIZE) != 0);
    }
  else
    {
      set_item_text (amd->maximize_item, _("Ma_ximize"));
      set_item_stock (amd->maximize_item, WNCK_STOCK_MAXIMIZE);
      gtk_widget_set_sensitive (amd->maximize_item,
                                (actions & WNCK_WINDOW_ACTION_MAXIMIZE) != 0);
    }

  if (wnck_window_is_shaded (amd->window))
    {
      set_item_text (amd->shade_item, _("_Unroll"));
      set_item_stock (amd->shade_item, NULL);
      gtk_widget_set_sensitive (amd->shade_item,
                                (actions & WNCK_WINDOW_ACTION_UNSHADE) != 0);
    }
  else
    {
      set_item_text (amd->shade_item, _("Roll _Up"));
      set_item_stock (amd->shade_item, NULL);
      gtk_widget_set_sensitive (amd->shade_item,
                                (actions & WNCK_WINDOW_ACTION_SHADE) != 0);
    }

  gtk_widget_set_sensitive (amd->close_item,
                            (actions & WNCK_WINDOW_ACTION_CLOSE) != 0);
  
  gtk_widget_set_sensitive (amd->move_item,
                            (actions & WNCK_WINDOW_ACTION_MOVE) != 0);

  gtk_widget_set_sensitive (amd->resize_item,
                            (actions & WNCK_WINDOW_ACTION_RESIZE) != 0);
  
  return FALSE;
}

static void
queue_update (ActionMenuData *amd)
{
  if (amd->idle_handler == 0)
    amd->idle_handler = g_idle_add ((GSourceFunc)update_menu_state, amd);
}

static void
state_changed_callback (WnckWindow     *window,
                        WnckWindowState changed_mask,
                        WnckWindowState new_state,
                        gpointer        data)
{
  ActionMenuData *amd;

  amd = get_data (data);

  if (amd)
    queue_update (amd);
}

static void
actions_changed_callback (WnckWindow       *window,
                          WnckWindowActions changed_mask,
                          WnckWindowActions new_actions,
                          gpointer          data)
{
  ActionMenuData *amd;

  amd = get_data (data);

  if (amd)
    queue_update (amd);
}

static GtkWidget*
make_menu_item (ActionMenuData *amd,
                WindowAction    action)
{
  GtkWidget *mi;
  
  mi = gtk_image_menu_item_new_with_label ("");
  
  set_data (G_OBJECT (mi), amd);
  
  g_signal_connect (G_OBJECT (mi), "activate",
                    G_CALLBACK (item_activated_callback),
                    GINT_TO_POINTER (action));
  
  gtk_widget_show (mi);

  return mi;
}

static void
amd_free (ActionMenuData *amd)
{
  if (amd->idle_handler)
    g_source_remove (amd->idle_handler);

  g_free (amd);
}

/**
 * wnck_create_window_action_menu:
 * @window: a #WnckWindow
 * 
 * Creates a menu of window operations for @window.
 * 
 * Return value: a new menu of window operations
 **/
GtkWidget*
wnck_create_window_action_menu (WnckWindow *window)
{
  GtkWidget *menu;
  ActionMenuData *amd;
  GtkWidget *separator;
  
  _wnck_stock_icons_init ();
  
  amd = g_new0 (ActionMenuData, 1);
  amd->window = window;
  
  menu = gtk_menu_new ();
  amd->menu = menu;
  
  g_object_set_data_full (G_OBJECT (menu), "wnck-action-data",
                          amd, (GDestroyNotify) amd_free);

  g_object_weak_ref (G_OBJECT (window), window_weak_notify, menu);
  g_object_weak_ref (G_OBJECT (menu), object_weak_notify, window);
  
  amd->minimize_item = make_menu_item (amd, MINIMIZE);
  
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         amd->minimize_item);

  amd->maximize_item = make_menu_item (amd, MAXIMIZE);
  
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         amd->maximize_item);

  amd->shade_item = make_menu_item (amd, SHADE);
  
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         amd->shade_item);

  amd->move_item = make_menu_item (amd, MOVE);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         amd->move_item);  

  set_item_text (amd->move_item, _("Mo_ve"));
  set_item_stock (amd->move_item, NULL);
  
  amd->resize_item = make_menu_item (amd, RESIZE);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         amd->resize_item);

  set_item_text (amd->resize_item, _("_Resize"));
  set_item_stock (amd->move_item, NULL);
  
  amd->close_item = make_menu_item (amd, CLOSE);

  separator = gtk_separator_menu_item_new ();
  gtk_widget_show (separator);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         separator);
  
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         amd->close_item);

  set_item_text (amd->close_item, _("_Close"));
  set_item_stock (amd->close_item, WNCK_STOCK_DELETE);
  
  g_signal_connect_object (G_OBJECT (amd->window), 
                           "state_changed",
                           G_CALLBACK (state_changed_callback),
                           G_OBJECT (menu),
                           0);

  g_signal_connect_object (G_OBJECT (amd->window), 
                           "actions_changed",
                           G_CALLBACK (actions_changed_callback),
                           G_OBJECT (menu),
                           0);

  update_menu_state (amd);
  
  return menu;
}

