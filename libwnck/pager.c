/* pager object */

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

#include "pager.h"
#include "workspace.h"
#include "window.h"

#define N_SCREEN_CONNECTIONS 9

struct _WnckPagerPrivate
{
  WnckScreen *screen;
  int n_rows; /* really columns for vertical orientation */
  GtkOrientation orientation;
  int workspace_size;
  guint screen_connections[N_SCREEN_CONNECTIONS];
};

enum
{
  LAST_SIGNAL
};

static void wnck_pager_init        (WnckPager      *pager);
static void wnck_pager_class_init  (WnckPagerClass *klass);
static void wnck_pager_finalize    (GObject        *object);

static void     wnck_pager_realize       (GtkWidget        *widget);
static void     wnck_pager_unrealize     (GtkWidget        *widget);
static void     wnck_pager_size_request  (GtkWidget        *widget,
                                          GtkRequisition   *requisition);
static void     wnck_pager_size_allocate (GtkWidget        *widget,
                                          GtkAllocation    *allocation);
static gboolean wnck_pager_expose_event  (GtkWidget        *widget,
                                          GdkEventExpose   *event);
static gboolean wnck_pager_button_press  (GtkWidget        *widget,
                                          GdkEventButton   *event);
static gboolean wnck_pager_focus         (GtkWidget        *widget,
                                          GtkDirectionType  direction);

static void wnck_pager_connect_screen    (WnckPager  *pager,
                                          WnckScreen *screen);
static void wnck_pager_connect_window    (WnckPager  *pager,
                                          WnckWindow *window);
static void wnck_pager_disconnect_screen (WnckPager  *pager);


static gpointer parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

GType
wnck_pager_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();
  
  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (WnckPagerClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) wnck_pager_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (WnckPager),
        0,              /* n_preallocs */
        (GInstanceInitFunc) wnck_pager_init,
      };
      
      object_type = g_type_register_static (GTK_TYPE_WIDGET,
                                            "WnckPager",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
wnck_pager_init (WnckPager *pager)
{  
  pager->priv = g_new0 (WnckPagerPrivate, 1);

  pager->priv->n_rows = 1;
  pager->priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  pager->priv->workspace_size = 48;
}

static void
wnck_pager_class_init (WnckPagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = wnck_pager_finalize;

  widget_class->realize = wnck_pager_realize;
  widget_class->unrealize = wnck_pager_unrealize;
  widget_class->size_request = wnck_pager_size_request;
  widget_class->size_allocate = wnck_pager_size_allocate;
  widget_class->expose_event = wnck_pager_expose_event;
  widget_class->button_press_event = wnck_pager_button_press;
  widget_class->focus = wnck_pager_focus;
}

static void
wnck_pager_finalize (GObject *object)
{
  WnckPager *pager;

  pager = WNCK_PAGER (object);

  wnck_pager_disconnect_screen (pager);
  
  g_free (pager->priv);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
wnck_pager_realize (GtkWidget *widget)
{

  GdkWindowAttr attributes;
  gint attributes_mask;
  WnckPager *pager;

  pager = WNCK_PAGER (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
wnck_pager_unrealize (GtkWidget *widget)
{

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
wnck_pager_size_request  (GtkWidget      *widget,
                          GtkRequisition *requisition)
{
  WnckPager *pager;
  int n_spaces;
  int spaces_per_row;
  
  pager = WNCK_PAGER (widget);
  
  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);

  g_assert (pager->priv->n_rows > 0);
  spaces_per_row = n_spaces /  pager->priv->n_rows;
  
  if (pager->priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      requisition->width = pager->priv->workspace_size * pager->priv->n_rows;
      requisition->height = pager->priv->workspace_size * spaces_per_row;
    }
  else
    {
      requisition->width = pager->priv->workspace_size * spaces_per_row;
      requisition->height = pager->priv->workspace_size * pager->priv->n_rows;
    }
}

static void
wnck_pager_size_allocate (GtkWidget      *widget,
                          GtkAllocation  *allocation)
{
  WnckPager *pager;

  pager = WNCK_PAGER (widget);

  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
}

static void
get_workspace_rect (WnckPager    *pager,
                    int           space,
                    GdkRectangle *rect)
{
  int n_spaces;
  int spaces_per_row;
  GtkWidget *widget;

  widget = GTK_WIDGET (pager);
  
  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);

  g_assert (pager->priv->n_rows > 0);
  spaces_per_row = n_spaces /  pager->priv->n_rows;

  if (pager->priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      rect->width = widget->allocation.width / pager->priv->n_rows;
      rect->height = widget->allocation.height / spaces_per_row;
      rect->x = rect->width * (space % pager->priv->n_rows); 
      rect->y = rect->height * (space % spaces_per_row);
    }
  else
    {
      rect->width = widget->allocation.width / spaces_per_row;
      rect->height = widget->allocation.height / pager->priv->n_rows;
      rect->x = rect->width * (space % spaces_per_row);
      rect->y = rect->height * (space % pager->priv->n_rows);
    }
}
                    

static gboolean
wnck_pager_expose_event  (GtkWidget      *widget,
                          GdkEventExpose *event)
{
  WnckPager *pager;
  int i;
  int n_spaces;
  WnckWorkspace *active_space;
  
  pager = WNCK_PAGER (widget);

  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);
  active_space = wnck_screen_get_active_workspace (pager->priv->screen);
  
  i = 0;
  while (i < n_spaces)
    {
      GdkRectangle rect;

      get_workspace_rect (pager, i, &rect);
      
      gdk_draw_rectangle (widget->window,
                          widget->style->black_gc,
                          FALSE,
                          rect.x, rect.y, rect.width - 1, rect.height - 1);

      if (active_space &&
          i == wnck_workspace_get_number (active_space))
        gdk_draw_rectangle (widget->window,
                            widget->style->white_gc,
                            TRUE,
                            rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2);
      
      ++i;
    }

  return FALSE;
}

#define POINT_IN_RECT(xcoord, ycoord, rect) \
 ((xcoord) >= (rect).x &&                   \
  (xcoord) <  ((rect).x + (rect).width) &&  \
  (ycoord) >= (rect).y &&                   \
  (ycoord) <  ((rect).y + (rect).height))

static gboolean
wnck_pager_button_press  (GtkWidget      *widget,
                          GdkEventButton *event)
{
  WnckPager *pager;
  int i;
  int n_spaces;
  
  pager = WNCK_PAGER (widget);

  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);
  
  i = 0;
  while (i < n_spaces)
    {
      GdkRectangle rect;

      get_workspace_rect (pager, i, &rect);
      

      if (POINT_IN_RECT (event->x, event->y, rect))
        {
          WnckWorkspace *space = wnck_workspace_get (i);

          if (space)
            wnck_workspace_activate (space);

          break;
        }
      
      ++i;
    }

  return TRUE;
}

static gboolean
wnck_pager_focus (GtkWidget        *widget,
                  GtkDirectionType  direction)
{
  WnckPager *pager;

  pager = WNCK_PAGER (widget);
  
  return GTK_WIDGET_CLASS (parent_class)->focus (widget, direction);
}

GtkWidget*
wnck_pager_new (WnckScreen *screen)
{
  WnckPager *pager;
  
  pager = g_object_new (WNCK_TYPE_PAGER, NULL);

  wnck_pager_connect_screen (pager, screen);

  return GTK_WIDGET (pager);
}

void
wnck_pager_set_orientation (WnckPager     *pager,
                            GtkOrientation orientation)
{
  g_return_if_fail (WNCK_IS_PAGER (pager));

  if (pager->priv->orientation == orientation)
    return;

  pager->priv->orientation = orientation;
  gtk_widget_queue_resize (GTK_WIDGET (pager));
}

static void
active_window_changed_callback    (WnckScreen      *screen,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
active_workspace_changed_callback (WnckScreen      *screen,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
window_stacking_changed_callback  (WnckScreen      *screen,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
window_opened_callback            (WnckScreen      *screen,
                                   WnckWindow      *window,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);

  wnck_pager_connect_window (pager, window);
  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
window_closed_callback            (WnckScreen      *screen,
                                   WnckWindow      *window,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);

  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
workspace_created_callback        (WnckScreen      *screen,
                                   WnckWorkspace   *space,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  gtk_widget_queue_resize (GTK_WIDGET (pager));
}

static void
workspace_destroyed_callback      (WnckScreen      *screen,
                                   WnckWorkspace   *space,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  gtk_widget_queue_resize (GTK_WIDGET (pager));
}

static void
application_opened_callback       (WnckScreen      *screen,
                                   WnckApplication *app,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
}

static void
application_closed_callback       (WnckScreen      *screen,
                                   WnckApplication *app,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
}

static void
window_name_changed_callback      (WnckWindow      *window,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
window_state_changed_callback     (WnckWindow      *window,
                                   WnckWindowState  changed,
                                   WnckWindowState  new,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
window_workspace_changed_callback (WnckWindow      *window,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
window_icon_changed_callback      (WnckWindow      *window,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
window_geometry_changed_callback  (WnckWindow      *window,
                                   gpointer         data)
{
  WnckPager *pager = WNCK_PAGER (data);
  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
wnck_pager_connect_screen (WnckPager  *pager,
                           WnckScreen *screen)
{
  int i;
  guint *c;

  g_return_if_fail (pager->priv->screen == NULL);
  
  pager->priv->screen = screen;
  
  i = 0;
  c = pager->priv->screen_connections;
  
  c[i] = g_signal_connect (G_OBJECT (screen), "active_window_changed",
                           G_CALLBACK (active_window_changed_callback),
                           pager);
  ++i;
  
  c[i] = g_signal_connect (G_OBJECT (screen), "active_workspace_changed",
                           G_CALLBACK (active_workspace_changed_callback),
                           pager);
  ++i;  

  c[i] = g_signal_connect (G_OBJECT (screen), "window_stacking_changed",
                           G_CALLBACK (window_stacking_changed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "window_opened",
                           G_CALLBACK (window_opened_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "window_closed",
                           G_CALLBACK (window_closed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "workspace_created",
                           G_CALLBACK (workspace_created_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "workspace_destroyed",
                           G_CALLBACK (workspace_destroyed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "application_opened",
                           G_CALLBACK (application_opened_callback),
                           pager);
  ++i;  

  c[i] = g_signal_connect (G_OBJECT (screen), "application_closed",
                           G_CALLBACK (application_closed_callback),
                           pager);
  ++i;

  g_assert (i == N_SCREEN_CONNECTIONS);
}

static void
wnck_pager_connect_window (WnckPager  *pager,
                           WnckWindow *window)
{  
  g_signal_connect_object (G_OBJECT (window), "name_changed",
                           G_CALLBACK (window_name_changed_callback),
                           pager, 0);
  g_signal_connect_object (G_OBJECT (window), "state_changed",
                           G_CALLBACK (window_state_changed_callback),
                           pager, 0);
  g_signal_connect_object (G_OBJECT (window), "workspace_changed",
                           G_CALLBACK (window_workspace_changed_callback),
                           pager, 0);
  g_signal_connect_object (G_OBJECT (window), "icon_changed",
                           G_CALLBACK (window_icon_changed_callback),
                           pager, 0);
  g_signal_connect_object (G_OBJECT (window), "geometry_changed",
                           G_CALLBACK (window_geometry_changed_callback),
                           pager, 0);
}

static void
wnck_pager_disconnect_screen (WnckPager  *pager)
{
  int i;

  if (pager->priv->screen == NULL)
    return;
  
  i = 0;
  while (i < N_SCREEN_CONNECTIONS)
    {
      if (pager->priv->screen_connections[i] != 0)
        g_signal_handler_disconnect (G_OBJECT (pager->priv->screen),
                                     pager->priv->screen_connections[i]);

      pager->priv->screen_connections[i] = 0;
      
      ++i;
    }

  pager->priv->screen = NULL;
}
