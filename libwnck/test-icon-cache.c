/*
 * Copyright (C) 2022 Alberts Muktupāvels
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

#include <cairo-xlib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libwnck/libwnck.h>
#include <X11/Xatom.h>

#define WINDOW_NAME "Test Icon Cache"
#define NET_WM_ICON_NAME "face-cool"
#define WM_HINTS_ICON_NAME "face-monkey"

typedef struct
{
  GtkWidget *set_button;
  GtkWidget *nochange_button;
  GtkWidget *unset_button;

  GtkWidget *status_label;
} RowData;

typedef struct
{
  Display *xdisplay;

  Window   xwindow;

  Pixmap   icon_pixmap;
  Pixmap   icon_mask;
} IconCacheTest;

static void
unset_net_wm_icon (IconCacheTest *self)
{
  XDeleteProperty (self->xdisplay,
                   self->xwindow,
                   XInternAtom (self->xdisplay, "_NET_WM_ICON", False));
}

static void
set_net_wm_icon (IconCacheTest *self)
{
  GdkPixbuf *pixbuf;
  int width;
  int height;
  int size;
  unsigned long *data;
  unsigned long *p;

  pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                     NET_WM_ICON_NAME,
                                     24,
                                     GTK_ICON_LOOKUP_FORCE_SIZE,
                                     NULL);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  size = 2 + width * height;
  data = p = g_malloc (size * sizeof (unsigned long));

  *p++ = width;
  *p++ = height;

  {
    unsigned char *pixels;
    int n_channels;
    int stride;
    int x;
    int y;

    pixels = gdk_pixbuf_get_pixels (pixbuf);
    n_channels = gdk_pixbuf_get_n_channels (pixbuf);
    stride = gdk_pixbuf_get_rowstride (pixbuf);

    for (y = 0; y < height; y++)
      {
        for (x = 0; x < width; x++)
          {
            unsigned char r;
            unsigned char g;
            unsigned char b;
            unsigned char a;

            r = pixels[y * stride + x * n_channels + 0];
            g = pixels[y * stride + x * n_channels + 1];
            b = pixels[y * stride + x * n_channels + 2];

            if (n_channels >= 4)
              a = pixels[y * stride + x * n_channels + 3];
            else
              a = 255;

            *p++ = a << 24 | r << 16 | g << 8 | b ;
          }
      }
  }

  XChangeProperty (self->xdisplay,
                   self->xwindow,
                   XInternAtom (self->xdisplay, "_NET_WM_ICON", False),
                   XA_CARDINAL,
                   32,
                   PropModeReplace,
                   (unsigned char *) data,
                   size);

  g_object_unref (pixbuf);
  g_free (data);
}

static void
create_icon_pixmaps (IconCacheTest *self)
{
  int screen;
  GdkPixbuf *pixbuf;
  cairo_surface_t *surface;
  cairo_t *cr;

  screen = DefaultScreen (self->xdisplay);

  pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                     WM_HINTS_ICON_NAME,
                                     24,
                                     GTK_ICON_LOOKUP_FORCE_SIZE,
                                     NULL);

  g_assert (self->icon_pixmap == None);
  self->icon_pixmap = XCreatePixmap (self->xdisplay,
                                     self->xwindow,
                                     24,
                                     24,
                                     XDefaultDepth (self->xdisplay, screen));

  surface = cairo_xlib_surface_create (self->xdisplay,
                                       self->icon_pixmap,
                                       XDefaultVisual (self->xdisplay, screen),
                                       24,
                                       24);

  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);

  if (gdk_pixbuf_get_has_alpha (pixbuf))
    {
      cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);
      cairo_paint (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_SATURATE);
      cairo_paint (cr);
      cairo_pop_group_to_source (cr);
    }

  cairo_paint (cr);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  if (gdk_pixbuf_get_has_alpha (pixbuf))
    {
      Screen *xscreen;

      g_assert (self->icon_mask == None);
      self->icon_mask = XCreatePixmap (self->xdisplay,
                                       self->xwindow,
                                       24,
                                       24,
                                       1);

      xscreen = DefaultScreenOfDisplay (self->xdisplay);
      surface = cairo_xlib_surface_create_for_bitmap (self->xdisplay,
                                                      self->icon_mask,
                                                      xscreen,
                                                      24,
                                                      24);

      cr = cairo_create (surface);
      gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_paint (cr);
      cairo_destroy (cr);
      cairo_surface_destroy (surface);
    }

  g_object_unref (pixbuf);
}

static void
free_icon_pixmaps (IconCacheTest *self)
{
  if (self->icon_pixmap != None)
    {
      XFreePixmap (self->xdisplay, self->icon_pixmap);
      self->icon_pixmap = None;
    }

  if (self->icon_mask != None)
    {
      XFreePixmap (self->xdisplay, self->icon_mask);
      self->icon_mask = None;
    }
}

static void
unset_wm_hints (IconCacheTest *self)
{
  XWMHints *hints;

  hints = XGetWMHints (self->xdisplay, self->xwindow);
  if (hints == NULL)
    return;

  free_icon_pixmaps (self);

  hints->flags |= IconPixmapHint | IconMaskHint;
  hints->icon_pixmap = None;
  hints->icon_mask = None;

  XSetWMHints (self->xdisplay, self->xwindow, hints);
  XFree (hints);
}

static void
set_wm_hints (IconCacheTest *self)
{
  XWMHints *hints;

  hints = XGetWMHints (self->xdisplay, self->xwindow);
  if (hints == NULL)
    hints = XAllocWMHints ();

  free_icon_pixmaps (self);
  create_icon_pixmaps (self);

  hints->flags |= IconPixmapHint | IconMaskHint;
  hints->icon_pixmap = self->icon_pixmap;
  hints->icon_mask = self->icon_mask;

  XSetWMHints (self->xdisplay, self->xwindow, hints);
  XFree (hints);
}

static void
unset_net_wm_icon_cb (GtkButton     *button,
                      IconCacheTest *self)
{
  RowData *data;

  unset_net_wm_icon (self);

  data = g_object_get_data (G_OBJECT (button), "row-data");
  gtk_label_set_text (GTK_LABEL (data->status_label), "✘");
  gtk_widget_set_sensitive (data->unset_button, FALSE);
}

static void
set_net_wm_icon_cb (GtkButton     *button,
                    IconCacheTest *self)
{
  RowData *data;

  set_net_wm_icon (self);

  data = g_object_get_data (G_OBJECT (button), "row-data");
  gtk_label_set_text (GTK_LABEL (data->status_label), "✔");
  gtk_widget_set_sensitive (data->unset_button, TRUE);
}

static void
unset_wm_hints_cb (GtkButton     *button,
                   IconCacheTest *self)
{
  RowData *data;

  unset_wm_hints (self);

  data = g_object_get_data (G_OBJECT (button), "row-data");
  gtk_label_set_text (GTK_LABEL (data->status_label), "✘");
  gtk_widget_set_sensitive (data->unset_button, FALSE);
}

static void
set_wm_hints_cb (GtkButton     *button,
                 IconCacheTest *self)
{
  RowData *data;

  set_wm_hints (self);

  data = g_object_get_data (G_OBJECT (button), "row-data");
  gtk_label_set_text (GTK_LABEL (data->status_label), "✔");
  gtk_widget_set_sensitive (data->unset_button, TRUE);
}

static void
urgency_cb (GtkButton     *button,
            IconCacheTest *self)
{
  XWMHints *hints;

  hints = XGetWMHints (self->xdisplay, self->xwindow);
  if (hints == NULL)
    hints = XAllocWMHints ();

  hints->flags ^= XUrgencyHint;

  XSetWMHints (self->xdisplay, self->xwindow, hints);
  XFree (hints);
}

static GtkWidget *
append_icon_row (const char    *name,
                 const char    *icon_name,
                 GCallback      set_cb,
                 GCallback      nochange_cb,
                 GCallback      unset_cb,
                 IconCacheTest *self)
{
  GtkWidget *row;
  RowData *data;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *btn;
  GtkWidget *img;

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);

  data = g_new0 (RowData, 1);
  g_object_set_data_full (G_OBJECT (row), "row-data", data, g_free);

  label = gtk_label_new (name);
  gtk_box_pack_start (GTK_BOX (row), label, TRUE, TRUE, 0);
  gtk_label_set_xalign (GTK_LABEL (label), 1.);
  gtk_widget_show (label);

  label = data->status_label = gtk_label_new ("✘");
  g_object_set_data (G_OBJECT (label), "row-data", data);
  gtk_box_pack_end (GTK_BOX (row), label, FALSE, FALSE, 0);
  gtk_label_set_xalign (GTK_LABEL (label), 1.);
  gtk_widget_show (label);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_end (GTK_BOX (row), box, FALSE, FALSE, 0);
  gtk_widget_show (box);

  img = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_container_add (GTK_CONTAINER (box), img);
  gtk_widget_show (img);

  btn = data->set_button = gtk_button_new_with_label ("set");
  g_object_set_data (G_OBJECT (btn), "row-data", data);
  gtk_container_add (GTK_CONTAINER (box), btn);
  gtk_widget_show (btn);

  g_signal_connect (btn, "clicked", set_cb, self);

  btn = data->nochange_button = gtk_button_new_with_label ("nochange");
  g_object_set_data (G_OBJECT (btn), "row-data", data);
  gtk_container_add (GTK_CONTAINER (box), btn);
  gtk_widget_show (btn);

  if (nochange_cb != NULL)
    g_signal_connect (btn, "clicked", nochange_cb, self);
  else
    gtk_widget_set_sensitive (btn, FALSE);

  btn = data->unset_button = gtk_button_new_with_label ("unset");
  g_object_set_data (G_OBJECT (btn), "row-data", data);
  gtk_container_add (GTK_CONTAINER (box), btn);
  gtk_widget_set_sensitive (btn, FALSE);
  gtk_widget_show (btn);

  g_signal_connect (btn, "clicked", unset_cb, self);

  return row;
}

static void
icon_changed_cb (WnckWindow *window,
                 GtkImage   *image)
{
  GdkPixbuf *icon;

  icon = wnck_window_get_icon (window);
  gtk_image_set_from_pixbuf (image, icon);
}

static void
mini_icon_changed_cb (WnckWindow *window,
                      GtkImage   *image)
{
  GdkPixbuf *icon;

  icon = wnck_window_get_mini_icon (window);
  gtk_image_set_from_pixbuf (image, icon);
}

static void
icon_changed_count_cb (WnckWindow *window,
                       GtkLabel   *label)
{
  static int count = 0;
  char *text;

  text = g_strdup_printf ("%d", ++count);
  gtk_label_set_text (label, text);
  g_free (text);
}

static void
append_result_box (GtkWidget  *vbox,
                   WnckWindow *window)
{
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *image;
  GtkWidget *label;

  box1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start (GTK_BOX (vbox), box1, TRUE, FALSE, 0);
  gtk_widget_show (box1);

  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_box_pack_end (GTK_BOX (box1), box2, TRUE, FALSE, 0);
  gtk_widget_show (box2);

  image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (box2), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  g_signal_connect (window,
                    "icon-changed",
                    G_CALLBACK (icon_changed_cb),
                    image);

  icon_changed_cb (window, GTK_IMAGE (image));

  image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (box2), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  g_signal_connect (window,
                    "icon-changed",
                    G_CALLBACK (mini_icon_changed_cb),
                    image);

  mini_icon_changed_cb (window, GTK_IMAGE (image));

  label = gtk_label_new ("0");
  gtk_box_pack_start (GTK_BOX (box2), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  g_signal_connect (window,
                    "icon-changed",
                    G_CALLBACK (icon_changed_count_cb),
                    label);
}

static void
window_opened_cb (WnckScreen *screen,
                  WnckWindow *window,
                  GtkWidget  *vbox)
{
  const char *name;

  name = wnck_window_get_name (window);

  if (g_strcmp0 (name, WINDOW_NAME) != 0)
    return;

  append_result_box (vbox, window);
}

int
main (int    argc,
      char **argv)
{
  g_autoptr (WnckHandle) handle = NULL;
  WnckScreen *screen;
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *separator;
  GtkWidget *row;
  IconCacheTest self;

  gtk_init (&argc, &argv);

  handle = wnck_handle_new (WNCK_CLIENT_TYPE_APPLICATION);
  screen = wnck_handle_get_default_screen (handle);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), WINDOW_NAME);

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  g_object_set (G_OBJECT (vbox), "margin", 12, NULL);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);

  row = append_icon_row ("_NET_WM_ICON:",
                         NET_WM_ICON_NAME,
                         G_CALLBACK (set_net_wm_icon_cb),
                         NULL,
                         G_CALLBACK (unset_net_wm_icon_cb),
                         &self);

  gtk_box_pack_start (GTK_BOX (vbox), row, FALSE, FALSE, 0);
  gtk_widget_show (row);

  row = append_icon_row ("WM_HINTS:",
                         WM_HINTS_ICON_NAME,
                         G_CALLBACK (set_wm_hints_cb),
                         G_CALLBACK (urgency_cb),
                         G_CALLBACK (unset_wm_hints_cb),
                         &self);

  gtk_box_pack_start (GTK_BOX (vbox), row, FALSE, FALSE, 0);
  gtk_widget_show (row);

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_container_add (GTK_CONTAINER (vbox), separator);
  gtk_widget_show (separator);

  g_signal_connect (screen,
                    "window-opened",
                    G_CALLBACK (window_opened_cb),
                    vbox);

  gtk_window_present (GTK_WINDOW (window));

  self.xdisplay = gdk_x11_display_get_xdisplay (gdk_display_get_default ());
  self.xwindow = gdk_x11_window_get_xid (gtk_widget_get_window (window));
  self.icon_pixmap = None;
  self.icon_mask = None;

  gtk_main ();

  free_icon_pixmaps (&self);

  return EXIT_SUCCESS;
}
