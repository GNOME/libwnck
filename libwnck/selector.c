/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2001 Free Software Foundation, Inc.
 * Copyright (C) 2000 Helix Code, Inc.
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
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 *      George Lebl <jirka@5z.com>
 *      Jacob Berkman <jacob@helixcode.com>
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "selector.h"
#include "inlinepixbufs.h"
#include "libwnck.h"
#include "screen.h"

typedef struct
{
  GtkWidget *item;
  GtkWidget *label;
} window_hash_item;

#define WNCK_SELECTOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), WNCK_TYPE_SELECTOR, WnckSelectorPrivate))

struct _WnckSelectorPrivate {
  GtkWidget *image;
  GtkWidget *menu;
  GtkWidget *menu_bar;
  GtkWidget *menu_item;
  GtkWidget *no_windows_item;

  GdkPixbuf *icon_pixbuf;
  WnckWindow *icon_window;
  GHashTable *window_hash;

  int size;
  WnckScreen *screen;
};

static void wnck_selector_init              (WnckSelector      *tasklist);
static void wnck_selector_class_init        (WnckSelectorClass *klass);
static void wnck_selector_finalize          (GObject           *object);
static void wnck_selector_connect_to_window (WnckSelector      *selector,
                                             WnckWindow        *window);

static gpointer selector_parent_class;

static void
wnck_selector_destroy (GtkWidget *widget, WnckSelector *selector)
{
  WnckSelectorPrivate *priv;
  priv = WNCK_SELECTOR_GET_PRIVATE (selector);
  if (priv->menu)
    gtk_widget_destroy (priv->menu);
  priv->menu = NULL;
  priv->no_windows_item = NULL;

  if (priv->icon_pixbuf)
    g_object_unref (priv->icon_pixbuf);
  priv->icon_pixbuf = NULL;
}

static WnckScreen *
wnck_selector_get_screen (WnckSelector *selector)
{
  GdkScreen *screen;
  WnckSelectorPrivate *priv;

  priv = WNCK_SELECTOR_GET_PRIVATE (selector);
  if (!gtk_widget_has_screen (GTK_WIDGET (selector)))
    return priv->screen;

  screen = gtk_widget_get_screen (GTK_WIDGET (selector));

  return wnck_screen_get (gdk_screen_get_number (screen));
}

static GdkPixbuf *
wnck_selector_get_default_window_icon (void)
{
  static GdkPixbuf *retval = NULL;

  if (retval)
    return retval;

  retval = gdk_pixbuf_new_from_inline (-1, default_icon_data, FALSE, NULL);

  g_assert (retval);

  return retval;
}

static void
wnck_selector_dimm_icon (GdkPixbuf *pixbuf)
{
  int x, y, pixel_stride, row_stride;
  guchar *row, *pixels;
  int w, h;

  w = gdk_pixbuf_get_width (pixbuf);
  h = gdk_pixbuf_get_height (pixbuf);

  g_assert (gdk_pixbuf_get_has_alpha (pixbuf));

  pixel_stride = 4;

  row = gdk_pixbuf_get_pixels (pixbuf);
  row_stride = gdk_pixbuf_get_rowstride (pixbuf);

  for (y = 0; y < h; y++)
    {
      pixels = row;
      for (x = 0; x < w; x++)
        {
          pixels[3] /= 2;
          pixels += pixel_stride;
        }
      row += row_stride;
    }
}

static void
wnck_selector_set_window_icon (WnckSelector *selector,
                               GtkWidget *image,
                               WnckWindow *window, gboolean use_icon_size)
{
  GdkPixbuf *pixbuf, *freeme;
  int width, height;
  int icon_size = -1;
  WnckSelectorPrivate *priv = WNCK_SELECTOR_GET_PRIVATE (selector);

  pixbuf = NULL;
  freeme = NULL;

  if (window)
    pixbuf = wnck_window_get_icon (window);

  if (!pixbuf)
    pixbuf = wnck_selector_get_default_window_icon ();

  if (!use_icon_size && priv->size > 1)
    icon_size = priv->size;

  if (icon_size == -1)
    gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, NULL, &icon_size);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  if (icon_size != -1 && (width > icon_size || height > icon_size))
    {
      double scale;

      scale = ((double) icon_size) / MAX (width, height);

      pixbuf = gdk_pixbuf_scale_simple (pixbuf, width * scale,
                                        height * scale, GDK_INTERP_BILINEAR);
      freeme = pixbuf;
    }

  if (window && wnck_window_is_minimized (window))
    wnck_selector_dimm_icon (pixbuf);

  gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);

  if (freeme)
    g_object_unref (freeme);
}

static void
wnck_selector_set_active_window (WnckSelector *selector, WnckWindow *window)
{
  WnckSelectorPrivate *priv = WNCK_SELECTOR_GET_PRIVATE (selector);
  wnck_selector_set_window_icon (selector, priv->image, window, FALSE);
  priv->icon_window = window;
}

/* The results of this function will need to be freed. */
static char *
wnck_selector_get_window_name (WnckWindow *window)
{
  const char *const_name;
  char *return_value;
  char *name;

  const_name = wnck_window_get_name (window);
  if (!const_name)
    name = g_strdup (_("Unknown Window"));
  else
    name = g_strdup (const_name);

  if (wnck_window_demands_attention (window))
    {
      return_value = g_strdup_printf ("<b>%s</b>", name);
      g_free (name);
      name = return_value;
    }

  if (wnck_window_is_shaded (window))
    {
      return_value = g_strdup_printf ("=%s=", name);
      g_free (name);
    }
  else if (wnck_window_is_minimized (window))
    {
      return_value = g_strdup_printf ("[%s]", name);
      g_free (name);
    }
  else
    return_value = name;

  return return_value;
}

static void
wnck_selector_window_icon_changed (WnckWindow *window,
                                   WnckSelector *selector)
{
  window_hash_item *item;
  GtkWidget *image;
  WnckSelectorPrivate *priv = WNCK_SELECTOR_GET_PRIVATE (selector);

  if (priv->icon_window == window)
    wnck_selector_set_active_window (selector, window);

  item = NULL;

  item = g_hash_table_lookup (priv->window_hash, window);
  if (item != NULL)
    {
      image = gtk_image_new ();
      wnck_selector_set_window_icon (selector, image, window, TRUE);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item->item),
                                     GTK_WIDGET (image));
      gtk_widget_show (image);
    }
}

static void
wnck_selector_window_name_changed (WnckWindow *window,
                                   WnckSelector *selector)
{
  window_hash_item *item;
  char *window_name;
  WnckSelectorPrivate *priv = WNCK_SELECTOR_GET_PRIVATE (selector);

  item = NULL;
  window_name = NULL;

  item = g_hash_table_lookup (priv->window_hash, window);
  if (item != NULL)
    {
      window_name = wnck_selector_get_window_name (window);
      gtk_label_set_text (GTK_LABEL (item->label), window_name);
      if (window_name != NULL)
        g_free (window_name);
    }
}

static void
wnck_selector_window_state_changed (WnckWindow *window,
                                    WnckWindowState changed_mask,
                                    WnckWindowState new_state,
                                    WnckSelector *selector)
{
  window_hash_item *item;
  char *window_name;
  WnckSelectorPrivate *priv = WNCK_SELECTOR_GET_PRIVATE (selector);

  if (!
      (changed_mask &
       (WNCK_WINDOW_STATE_MINIMIZED | WNCK_WINDOW_STATE_SHADED |
        WNCK_WINDOW_STATE_SKIP_TASKLIST |
        WNCK_WINDOW_STATE_DEMANDS_ATTENTION)))
    return;

  item = NULL;
  window_name = NULL;

  item = g_hash_table_lookup (priv->window_hash, window);
  if (item == NULL)
    return;

  if (changed_mask & WNCK_WINDOW_STATE_SKIP_TASKLIST)
    {
      if (wnck_window_is_skip_tasklist (window))
        {
          gtk_widget_hide (item->item);
        }
      else
        gtk_widget_show (item->item);
    }

  if (changed_mask &
      (WNCK_WINDOW_STATE_MINIMIZED | WNCK_WINDOW_STATE_SHADED |
       WNCK_WINDOW_STATE_DEMANDS_ATTENTION))
    {
      window_name = wnck_selector_get_window_name (window);
      gtk_label_set_text (GTK_LABEL (item->label), window_name);
      if (window_name != NULL)
        g_free (window_name);
    }
}

static void
wnck_selector_active_window_changed (WnckScreen *screen,
                                     WnckSelector *selector)
{
  WnckWindow *window;
  WnckSelectorPrivate *priv = WNCK_SELECTOR_GET_PRIVATE (selector);

  window = wnck_screen_get_active_window (screen);

  if (priv->icon_window != window)
    wnck_selector_set_active_window (selector, window);
}

static void
wnck_selector_activate_window (WnckWindow *window)
{
  WnckWorkspace *workspace;

  workspace = wnck_window_get_workspace (window);
  wnck_workspace_activate (workspace);

  if (wnck_window_is_minimized (window))
    wnck_window_unminimize (window);

  wnck_window_activate (window);
}

#define SELECTOR_MAX_WIDTH 50   /* maximum width in characters */

static gint
wnck_selector_get_width (GtkWidget *widget, const char *text)
{
  PangoContext *context;
  PangoFontMetrics *metrics;
  gint char_width;
  PangoLayout *layout;
  PangoRectangle natural;
  gint max_width;
  gint screen_width;
  gint width;

  gtk_widget_ensure_style (widget);

  context = gtk_widget_get_pango_context (widget);
  metrics = pango_context_get_metrics (context, widget->style->font_desc,
                                       pango_context_get_language (context));
  char_width = pango_font_metrics_get_approximate_char_width (metrics);
  pango_font_metrics_unref (metrics);
  max_width = PANGO_PIXELS (SELECTOR_MAX_WIDTH * char_width);

  layout = gtk_widget_create_pango_layout (widget, text);
  pango_layout_get_pixel_extents (layout, NULL, &natural);
  g_object_unref (G_OBJECT (layout));

  screen_width = gdk_screen_get_width (gtk_widget_get_screen (widget));

  width = MIN (natural.width, max_width);
  width = MIN (width, 3 * (screen_width / 4));

  return width;
}

static GtkWidget *
wnck_selector_item_new (WnckSelector *selector,
                        const gchar *label, WnckWindow *window)
{
  GtkWidget *item;
  GtkWidget *ellipsizing_label;
  window_hash_item *hash_item;
  WnckSelectorPrivate *priv = WNCK_SELECTOR_GET_PRIVATE (selector);

  item = gtk_image_menu_item_new ();

  ellipsizing_label = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (ellipsizing_label), 0.0, 0.5);
  gtk_label_set_ellipsize (GTK_LABEL (ellipsizing_label),
                           PANGO_ELLIPSIZE_END);
  /* if window demands attention, we need markup */
  if (window != NULL)
    gtk_label_set_use_markup (GTK_LABEL (ellipsizing_label), TRUE);

  if (window != NULL)
    {
      hash_item = g_new0 (window_hash_item, 1);
      hash_item->item = item;
      hash_item->label = ellipsizing_label;
      g_hash_table_insert (priv->window_hash, window, hash_item);
    }

  gtk_container_add (GTK_CONTAINER (item), ellipsizing_label);

  gtk_widget_show (ellipsizing_label);

  gtk_widget_set_size_request (ellipsizing_label,
                               wnck_selector_get_width (GTK_WIDGET (selector),
                                                        label), -1);
  return item;
}

static void
wnck_selector_add_window (WnckSelector *selector, WnckWindow *window)
{
  WnckWorkspace *workspace;
  GtkWidget *item;
  GtkWidget *image;
  char *name;
  WnckSelectorPrivate *priv = WNCK_SELECTOR_GET_PRIVATE (selector);

  if (wnck_window_is_skip_tasklist (window))
    return;

  name = wnck_selector_get_window_name (window);

  item = wnck_selector_item_new (selector, name, window);

  if (name != NULL)
    g_free (name);

  image = gtk_image_new ();

  wnck_selector_set_window_icon (selector, image, window, TRUE);

  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                 GTK_WIDGET (image));
  gtk_widget_show (image);

  workspace =
    wnck_screen_get_active_workspace (wnck_selector_get_screen (selector));

  if (wnck_window_get_workspace (window) == workspace)
    gtk_menu_shell_prepend (GTK_MENU_SHELL (priv->menu), item);
  else
    gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

  g_signal_connect_swapped (item, "activate",
                            G_CALLBACK (wnck_selector_activate_window),
                            window);

  gtk_widget_show (item);
}

static void
wnck_selector_window_opened (WnckScreen *screen,
                             WnckWindow *window, WnckSelector *selector)
{
  WnckSelectorPrivate *priv = WNCK_SELECTOR_GET_PRIVATE (selector);

  if (priv->menu && GTK_WIDGET_VISIBLE (priv->menu))
    {
      if (priv->no_windows_item
          && GTK_WIDGET_VISIBLE (priv->no_windows_item))
        gtk_widget_hide (priv->no_windows_item);
      wnck_selector_add_window (selector, window);
      gtk_menu_reposition (GTK_MENU (priv->menu));
    }

  wnck_selector_connect_to_window (selector, window);
}

static void
wnck_selector_window_closed (WnckScreen *screen,
                             WnckWindow *window, WnckSelector *selector)
{
  WnckSelectorPrivate *priv = WNCK_SELECTOR_GET_PRIVATE (selector);
  window_hash_item *item;

  if (window == priv->icon_window)
    wnck_selector_set_active_window (selector, NULL);

  if (!priv->menu || !GTK_WIDGET_VISIBLE (priv->menu))
    return;

  item = g_hash_table_lookup (priv->window_hash, window);
  if (!item)
    return;

  gtk_widget_hide (item->item);
  gtk_menu_reposition (GTK_MENU (priv->menu));
}

static void
wncklet_connect_while_alive (gpointer object,
                             const char *signal,
                             GCallback func,
                             gpointer func_data, gpointer alive_object)
{
  GClosure *closure;

  closure = g_cclosure_new (func, func_data, NULL);
  g_object_watch_closure (G_OBJECT (alive_object), closure);
  g_signal_connect_closure_by_id (object,
                                  g_signal_lookup (signal,
                                                   G_OBJECT_TYPE (object)), 0,
                                  closure, FALSE);
}

static void
wnck_selector_connect_to_window (WnckSelector *selector, WnckWindow *window)
{
  wncklet_connect_while_alive (window, "icon_changed",
                               G_CALLBACK (wnck_selector_window_icon_changed),
                               selector, selector);
  wncklet_connect_while_alive (window, "name_changed",
                               G_CALLBACK (wnck_selector_window_name_changed),
                               selector, selector);
  wncklet_connect_while_alive (window, "state_changed",
                               G_CALLBACK
                               (wnck_selector_window_state_changed), selector,
                               selector);
}

static void
wnck_selector_connect_to_screen (WnckSelector *selector, WnckScreen *screen)
{
  wncklet_connect_while_alive (screen, "active_window_changed",
                               G_CALLBACK
                               (wnck_selector_active_window_changed),
                               selector, selector);

  wncklet_connect_while_alive (screen, "window_opened",
                               G_CALLBACK (wnck_selector_window_opened),
                               selector, selector);

  wncklet_connect_while_alive (screen, "window_closed",
                               G_CALLBACK (wnck_selector_window_closed),
                               selector, selector);
}

static void
wnck_selector_destroy_menu (GtkWidget *widget, WnckSelector *selector)
{
  WnckSelectorPrivate *priv = WNCK_SELECTOR_GET_PRIVATE (selector);
  priv->menu = NULL;

  if (priv->window_hash)
    g_hash_table_destroy (priv->window_hash);
  priv->window_hash = NULL;
  priv->no_windows_item = NULL;
}

static void
wnck_selector_menu_hidden (GtkWidget *menu, WnckSelector *selector)
{
  gtk_widget_set_state (GTK_WIDGET (selector), GTK_STATE_NORMAL);
}

static void
wnck_selector_on_show (GtkWidget *widget, WnckSelector *selector)
{
  GtkWidget *separator;
  WnckScreen *screen;
  GList *windows;
  GList *l, *children;
  WnckSelectorPrivate *priv = WNCK_SELECTOR_GET_PRIVATE (selector);

  /* Remove existing items */
  children = gtk_container_get_children (GTK_CONTAINER (priv->menu));
  for (l = children; l; l = l->next)
    gtk_container_remove (GTK_CONTAINER (priv->menu), l->data);
  g_list_free (children);

  priv->no_windows_item = NULL;

  /* Add separator */
  separator = gtk_separator_menu_item_new ();
  gtk_widget_show (separator);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), separator);


  /* Add windows */
  screen = wnck_selector_get_screen (selector);
  windows = wnck_screen_get_windows (screen);

  if (priv->window_hash)
    g_hash_table_destroy (priv->window_hash);
  priv->window_hash = g_hash_table_new_full (g_direct_hash,
                                                 g_direct_equal,
                                                 NULL, g_free);

  for (l = windows; l; l = l->next)
    wnck_selector_add_window (selector, l->data);

  /* Remove separator if it is at the start or the end of the menu */
  l = GTK_MENU_SHELL (priv->menu)->children;

  if ((separator == l->data) || separator == g_list_last (l)->data)
    gtk_widget_destroy (separator);

  /* Check if a no-windows item is needed */
  if (!GTK_MENU_SHELL (priv->menu)->children)
    {
      priv->no_windows_item =
        wnck_selector_item_new (selector, _("No Windows Open"), NULL);

      gtk_widget_set_sensitive (priv->no_windows_item, FALSE);
      gtk_widget_show (priv->no_windows_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu),
                             priv->no_windows_item);
    }
}

static void
wnck_selector_setup_menu (WnckSelector *selector)
{
  WnckScreen *screen;
  GList *windows, *l;

  screen = wnck_selector_get_screen (selector);
  windows = wnck_screen_get_windows (screen);

  for (l = windows; l; l = l->next)
    if (wnck_window_is_active (l->data))
      break;

  wnck_selector_set_active_window (selector, l ? l->data : NULL);

  for (l = windows; l; l = l->next)
    wnck_selector_connect_to_window (selector, l->data);

  wnck_selector_connect_to_screen (selector, screen);
}

static void
wnck_selector_fill (WnckSelector *selector)
{
  WnckSelectorPrivate *priv = WNCK_SELECTOR_GET_PRIVATE (selector);

  g_signal_connect (selector, "destroy",
                    G_CALLBACK (wnck_selector_destroy), selector);

  priv->menu_item = gtk_menu_item_new ();
  gtk_widget_show (priv->menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (selector), priv->menu_item);

  priv->image = gtk_image_new ();
  gtk_widget_show (priv->image);
  gtk_container_add (GTK_CONTAINER (priv->menu_item), priv->image);

  priv->menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->menu_item),
                             priv->menu);
  g_signal_connect (priv->menu, "hide",
                    G_CALLBACK (wnck_selector_menu_hidden), selector);
  g_signal_connect (priv->menu, "destroy",
                    G_CALLBACK (wnck_selector_destroy_menu), selector);
  g_signal_connect (priv->menu, "show",
                    G_CALLBACK (wnck_selector_on_show), selector);

  gtk_widget_set_name (GTK_WIDGET (selector),
                       "gnome-panel-window-menu-menu-bar-style");

  gtk_rc_parse_string ("style \"gnome-panel-window-menu-menu-bar-style\" {\n"
                       "        GtkMenuBar::shadow-type = none\n"
                       "        GtkMenuBar::internal-padding = 0\n"
                       "}\n"
                       "widget \"*gnome-panel-window-menu-menu-bar*\" style : highest \"gnome-panel-window-menu-menu-bar-style\"");

  wnck_selector_setup_menu (selector);
  gtk_widget_show (GTK_WIDGET (selector));
}

GType
wnck_selector_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();

  if (!object_type)
    {
      static const GTypeInfo object_info = {
        sizeof (WnckSelectorClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) wnck_selector_class_init,
        NULL,                   /* class_finalize */
        NULL,                   /* class_data */
        sizeof (WnckSelector),
        0,                      /* n_preallocs */
        (GInstanceInitFunc) wnck_selector_init,
      };

      object_type = g_type_register_static (GTK_TYPE_MENU_BAR,
                                            "WnckSelector", &object_info, 0);
    }
  return object_type;
}

static void
wnck_selector_init (WnckSelector *selector)
{
  AtkObject *atk_obj;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (selector));
  atk_object_set_name (atk_obj, _("Window Selector"));
  atk_object_set_description (atk_obj, _("Tool to switch between windows"));
}

static void
wnck_selector_class_init (WnckSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  selector_parent_class = g_type_class_peek_parent (klass);
  object_class->finalize = wnck_selector_finalize;
  g_type_class_add_private (klass, sizeof (WnckSelectorPrivate));
}

static void
wnck_selector_finalize (GObject *object)
{
  WnckSelector *selector;
  WnckSelectorPrivate *priv;

  selector = WNCK_SELECTOR (object);
  priv = WNCK_SELECTOR_GET_PRIVATE (selector);

  if (priv->window_hash)
    g_hash_table_destroy (priv->window_hash);
  priv->window_hash = NULL;

  G_OBJECT_CLASS (selector_parent_class)->finalize (object);
}

GtkWidget *
wnck_selector_new (WnckScreen *screen)
{
  WnckSelector *selector;
  WnckSelectorPrivate *priv;
  selector = g_object_new (WNCK_TYPE_SELECTOR, NULL);
  priv = WNCK_SELECTOR_GET_PRIVATE (selector);
  priv->screen = screen;
  wnck_selector_fill (selector);
  return GTK_WIDGET (selector);
}
