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

#include <string.h>
#include <stdio.h>

#include "window-action-menu.h"
#include "private.h"

typedef enum
{
  CLOSE,
  MINIMIZE,
  MAXIMIZE,
  SHADE,
  MOVE,
  RESIZE,
  PIN,
  MOVE_TO_WORKSPACE
} WindowAction;

typedef struct _ActionMenuData ActionMenuData;

struct _ActionMenuData
{
  WnckWindow *window;
  GtkWidget *menu;
  GtkWidget *minimize_item;
  GtkWidget *maximize_item;
  GtkWidget *shade_item;
  GtkWidget *move_item;
  GtkWidget *resize_item;
  GtkWidget *close_item;
  GtkWidget *workspace_separator;
  GtkWidget *pin_item;
  GtkWidget *workspace_item;
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
      /* In an activate callback, so gtk_get_current_event_time() suffices */
      wnck_window_close (amd->window,
			 gtk_get_current_event_time ());
      break;
    case MINIMIZE:
      if (wnck_window_is_minimized (amd->window))
        wnck_window_unminimize (amd->window,
                                gtk_get_current_event_time ());
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
    case PIN:
      if (wnck_window_is_pinned (amd->window))
        wnck_window_unpin (amd->window);
      else
        wnck_window_pin (amd->window);
      break;
    case MOVE_TO_WORKSPACE: {
        int workspace_index;

        workspace_index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "workspace"));

        wnck_window_move_to_workspace (amd->window,
                                       wnck_screen_get_workspace (wnck_window_get_screen (amd->window),
                                       workspace_index));
       }
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
  WnckWindowActions  actions;  
  WnckScreen        *screen;

  amd->idle_handler = 0;
  
  actions = wnck_window_get_actions (amd->window);
  screen  = wnck_window_get_screen  (amd->window);
  
  if (wnck_window_is_minimized (amd->window))
    {
      set_item_text (amd->minimize_item, _("Unmi_nimize"));
      set_item_stock (amd->minimize_item, NULL);
      gtk_widget_set_sensitive (amd->minimize_item,
                                (actions & WNCK_WINDOW_ACTION_UNMINIMIZE) != 0);
    }
  else
    {
      set_item_text (amd->minimize_item, _("Mi_nimize"));
      set_item_stock (amd->minimize_item, WNCK_STOCK_MINIMIZE);
      gtk_widget_set_sensitive (amd->minimize_item,
                                (actions & WNCK_WINDOW_ACTION_MINIMIZE) != 0);
    }

  if (wnck_window_is_maximized (amd->window))
    {
      set_item_text (amd->maximize_item, _("Unma_ximize"));
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

  if (wnck_window_is_pinned (amd->window))
    {
      set_item_text (amd->pin_item, _("_Only on This Workspace"));
      set_item_stock (amd->pin_item, NULL);
      gtk_widget_set_sensitive (amd->pin_item,
                                (actions & WNCK_WINDOW_ACTION_CHANGE_WORKSPACE) != 0);
    }
  else
    {
      set_item_text (amd->pin_item, _("_Always on Visible Workspace"));
      set_item_stock (amd->pin_item, NULL);
      gtk_widget_set_sensitive (amd->pin_item,
                                (actions & WNCK_WINDOW_ACTION_CHANGE_WORKSPACE) != 0);
    }
  
  gtk_widget_set_sensitive (amd->close_item,
                            (actions & WNCK_WINDOW_ACTION_CLOSE) != 0);
  
  gtk_widget_set_sensitive (amd->move_item,
                            (actions & WNCK_WINDOW_ACTION_MOVE) != 0);

  gtk_widget_set_sensitive (amd->resize_item,
                            (actions & WNCK_WINDOW_ACTION_RESIZE) != 0);

  gtk_widget_set_sensitive (amd->workspace_item,
                            (actions & WNCK_WINDOW_ACTION_CHANGE_WORKSPACE) != 0);

  if (wnck_screen_get_workspace_count (screen) > 1)
    {
      gtk_widget_show (amd->workspace_separator);
      gtk_widget_show (amd->pin_item);
      gtk_widget_show (amd->workspace_item);
    }
  else
    {
      gtk_widget_hide (amd->workspace_separator);
      gtk_widget_hide (amd->pin_item);
      gtk_widget_hide (amd->workspace_item);
    }
  
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

static char *
get_workspace_name_with_accel (WnckWindow *window,
			       int index)
{
  const char *name;
  int number;
 
  name = wnck_workspace_get_name (wnck_screen_get_workspace (wnck_window_get_screen (window),
				  index));

  g_assert (name != NULL);

  /*
   * If the name is of the form "Workspace x" where x is an unsigned
   * integer, insert a '_' before the number if it is less than 10 and
   * return it
   */
  number = 0;
  if (sscanf (name, _("Workspace %d"), &number) == 1) {
      char *new_name;

      /*
       * Above name is a pointer into the Workspace struct. Here we make
       * a copy copy so we can have our wicked way with it.
       */
      if (number == 10)
        new_name = g_strdup_printf (_("Workspace 1_0"));
      else
        new_name = g_strdup_printf (_("Workspace %s%d"),
                                    number < 10 ? "_" : "",
                                    number);
      return new_name;
  }
  else {
      /*
       * Otherwise this is just a normal name. Escape any _ characters so that
       * the user's workspace names do not get mangled.  If the number is less
       * than 10 we provide an accelerator.
       */
      char *new_name;
      const char *source;
      char *dest;

      /*
       * Assume the worst case, that every character is a _.  We also
       * provide memory for " (_#)"
       */
      new_name = g_malloc0 (strlen (name) * 2 + 6 + 1);

      /*
       * Now iterate down the strings, adding '_' to escape as we go
       */
      dest = new_name;
      source = name;
      while (*source != '\0') {
          if (*source == '_')
            *dest++ = '_';
          *dest++ = *source++;
      }

      /* People don't start at workstation 0, but workstation 1 */
      if (index < 9) {
          g_snprintf (dest, 6, " (_%d)", index + 1);
      }
      else if (index == 9) {
          g_snprintf (dest, 6, " (_0)");
      }

      return new_name;
  }
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
  GtkWidget *menu, *submenu;
  ActionMenuData *amd;
  GtkWidget *separator;
  int num_workspaces, present_workspace, i;
  WnckWorkspace *workspace;

  _wnck_stock_icons_init ();
  
  amd = g_new0 (ActionMenuData, 1);
  amd->window = window;
  
  menu = gtk_menu_new ();
  g_object_ref (menu);
  gtk_object_sink (GTK_OBJECT (menu));

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

  set_item_text (amd->move_item, _("_Move"));
  set_item_stock (amd->move_item, NULL);
  
  amd->resize_item = make_menu_item (amd, RESIZE);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         amd->resize_item);

  set_item_text (amd->resize_item, _("_Resize"));
  set_item_stock (amd->move_item, NULL);

  separator = gtk_separator_menu_item_new ();
  gtk_widget_show (separator);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         separator);

  amd->close_item = make_menu_item (amd, CLOSE);
  
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         amd->close_item);

  set_item_text (amd->close_item, _("_Close"));
  set_item_stock (amd->close_item, WNCK_STOCK_DELETE);

  amd->workspace_separator = separator = gtk_separator_menu_item_new ();
  gtk_widget_show (separator);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         separator);

  amd->pin_item = make_menu_item (amd, PIN);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         amd->pin_item);
  set_item_stock (amd->pin_item, NULL);
  
  amd->workspace_item = gtk_menu_item_new_with_mnemonic (_("Move to Another _Workspace")); 
  gtk_widget_show (amd->workspace_item);

  submenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (amd->workspace_item), 
			     submenu);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         amd->workspace_item);

  num_workspaces = wnck_screen_get_workspace_count (wnck_window_get_screen (amd->window));
  workspace = wnck_window_get_workspace (amd->window);

  if (workspace)
    present_workspace = wnck_workspace_get_number (workspace);
  else 
    present_workspace = -1;

  for (i = 0; i < num_workspaces; i++)
    {
      char *name, *label;
      GtkWidget *item;
	
      name = get_workspace_name_with_accel (amd->window, i);
      label = g_strdup_printf (_("%s"), name);

      item = make_menu_item (amd, MOVE_TO_WORKSPACE);
      g_object_set_data (G_OBJECT (item), "workspace", GINT_TO_POINTER (i));

      if (i == present_workspace)
	gtk_widget_set_sensitive (item, FALSE);

      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
      set_item_text (item, label);
      set_item_stock (item, NULL);

      g_free (name);
      g_free (label);	
    }
		 
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

