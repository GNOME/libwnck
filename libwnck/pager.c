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
#include "window-action-menu.h"
#include "xutils.h"

#define N_SCREEN_CONNECTIONS 10

struct _WnckPagerPrivate
{
  WnckScreen *screen;
  
  int n_rows; /* really columns for vertical orientation */
  WnckPagerDisplayMode display_mode;
  gboolean show_all_workspaces;
  
  GtkOrientation orientation;
  int workspace_size;
  guint screen_connections[N_SCREEN_CONNECTIONS];
  int drag_start_x;
  int drag_start_y;
  int drag_start_x_workspace_relative;
  int drag_start_y_workspace_relative;
  WnckWindow *drag_window;
  int drag_window_x;
  int drag_window_y;
  WnckWindow *action_window;
  int action_click_x;
  int action_click_y;
  guint dragging : 1;

  GdkPixbuf *bg_cache;
};

enum
{
  dummy, /* remove this when you add more signals */
  LAST_SIGNAL
};


#define POINT_IN_RECT(xcoord, ycoord, rect) \
 ((xcoord) >= (rect).x &&                   \
  (xcoord) <  ((rect).x + (rect).width) &&  \
  (ycoord) >= (rect).y &&                   \
  (ycoord) <  ((rect).y + (rect).height))

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
static gboolean wnck_pager_motion        (GtkWidget        *widget,
                                          GdkEventMotion   *event);
static gboolean wnck_pager_button_release (GtkWidget        *widget,
                                           GdkEventButton   *event);
static gboolean wnck_pager_focus         (GtkWidget        *widget,
                                          GtkDirectionType  direction);

static void wnck_pager_connect_screen    (WnckPager  *pager,
                                          WnckScreen *screen);
static void wnck_pager_connect_window    (WnckPager  *pager,
                                          WnckWindow *window);
static void wnck_pager_disconnect_screen (WnckPager  *pager);

static void wnck_pager_clear_drag (WnckPager *pager);

static GdkPixbuf* wnck_pager_get_background (WnckPager *pager,
                                             int        width,
                                             int        height);

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
  pager->priv->display_mode = WNCK_PAGER_DISPLAY_CONTENT;
  pager->priv->show_all_workspaces = TRUE;
  pager->priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  pager->priv->workspace_size = 48;
  pager->priv->bg_cache = NULL;
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
  widget_class->button_release_event = wnck_pager_button_release;
  widget_class->motion_notify_event = wnck_pager_motion;
  widget_class->focus = wnck_pager_focus;
}

static void
wnck_pager_finalize (GObject *object)
{
  WnckPager *pager;

  pager = WNCK_PAGER (object);

  wnck_pager_disconnect_screen (pager);

  if (pager->priv->bg_cache)
    {
      g_object_unref (G_OBJECT (pager->priv->bg_cache));
      pager->priv->bg_cache = NULL;
    }
  
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
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
    GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
wnck_pager_unrealize (GtkWidget *widget)
{
  WnckPager *pager;
  
  pager = WNCK_PAGER (widget);

  wnck_pager_clear_drag (pager);
  
  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
wnck_pager_size_request  (GtkWidget      *widget,
                          GtkRequisition *requisition)
{
  WnckPager *pager;
  int n_spaces;
  int spaces_per_row;
  double screen_aspect;
  int other_dimension_size;
  int u_width, u_height;
  int size;
  int n_rows;
  
  pager = WNCK_PAGER (widget);
  
  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);

  g_assert (pager->priv->n_rows > 0);
  spaces_per_row = (n_spaces + pager->priv->n_rows - 1) / pager->priv->n_rows;
  
  gtk_widget_get_size_request (widget, &u_width, &u_height);
  
  if (pager->priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      screen_aspect = (double) gdk_screen_height () / (double) gdk_screen_width ();

      /* TODO: Handle WNCK_PAGER_DISPLAY_NAME for this case */

      if (pager->priv->show_all_workspaces)
	{
	  size = pager->priv->workspace_size;
	  if (u_width != -1)
	    size = (u_width - (pager->priv->n_rows - 1))  / pager->priv->n_rows;
 
	  n_rows = pager->priv->n_rows;
	}
      else
	{
	  size = pager->priv->workspace_size;
	  if (u_width != -1)
	    size = u_width;

	  n_rows = 1;
	  spaces_per_row = 1;
	}
      
      other_dimension_size = screen_aspect * size;
      
      requisition->width = size * n_rows + (n_rows - 1);
      requisition->height = other_dimension_size * spaces_per_row  + (spaces_per_row - 1);
    }
  else
    {
      screen_aspect = (double) gdk_screen_width () / (double) gdk_screen_height ();
      
      if (pager->priv->show_all_workspaces)
	{
	  size = pager->priv->workspace_size;
	  if (u_height != -1)
	    size = (u_height - (pager->priv->n_rows - 1))/ pager->priv->n_rows;
 
	  n_rows = pager->priv->n_rows;
	}
      else
	{
	  size = pager->priv->workspace_size;
	  if (u_height != -1)
	    size = u_height;

	  n_rows = 1;
	  spaces_per_row = 1;
	}

      if (pager->priv->display_mode == WNCK_PAGER_DISPLAY_CONTENT)
	{
	  other_dimension_size = screen_aspect * size;
	}
      else
	{
	  int n_spaces, i, w;
	  WnckScreen *screen;
	  PangoLayout *layout;

	  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);
	  layout = gtk_widget_create_pango_layout  (widget, NULL);
	  screen = pager->priv->screen;
	  other_dimension_size = 1;
	  
	  for (i = 0; i < n_spaces; i++)
	    {
	      pango_layout_set_text (layout,
				     wnck_workspace_get_name (wnck_screen_get_workspace (screen, i)),
				     -1);
	      pango_layout_get_pixel_size (layout, &w, NULL);
	      other_dimension_size = MAX (other_dimension_size, w);
	    }
	  
	  g_object_unref (layout);
	  
	  other_dimension_size += 2;
	}
      
      requisition->width = other_dimension_size * spaces_per_row + (spaces_per_row - 1);
      requisition->height = size * n_rows + (n_rows - 1);
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
  int col, row;
  
  widget = GTK_WIDGET (pager);

  if (!pager->priv->show_all_workspaces)
    {
      WnckWorkspace *active_space;
  
      active_space = wnck_screen_get_active_workspace (pager->priv->screen);

      rect->x = 0;
      rect->y = 0;
      if (active_space && space == wnck_workspace_get_number (active_space))
	{
	  rect->width = widget->allocation.width;
	  rect->height = widget->allocation.height;
	}
      else
	{
	  rect->width = 0;
	  rect->height = 0;
	}

      return;
    }
  
  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);

  g_assert (pager->priv->n_rows > 0);
  spaces_per_row = (n_spaces + pager->priv->n_rows - 1) / pager->priv->n_rows;
  
  if (pager->priv->orientation == GTK_ORIENTATION_VERTICAL)
    {      
      rect->width = (widget->allocation.width - (pager->priv->n_rows - 1)) / pager->priv->n_rows;
      rect->height = (widget->allocation.height - (spaces_per_row - 1)) / spaces_per_row;

      col = space / spaces_per_row;
      row = space % spaces_per_row;
      rect->x = (rect->width + 1) * col; 
      rect->y = (rect->height + 1) * row;
      
      if (col == pager->priv->n_rows - 1)
	rect->width = widget->allocation.width - rect->x;
      
      if (row  == spaces_per_row - 1)
	rect->height = widget->allocation.height - rect->y;
    }
  else
    {
      rect->width = (widget->allocation.width - (spaces_per_row - 1)) / spaces_per_row;
      rect->height = (widget->allocation.height - (pager->priv->n_rows - 1)) / pager->priv->n_rows;
      
      col = space % spaces_per_row;
      row = space / spaces_per_row;
      rect->x = (rect->width + 1) * col; 
      rect->y = (rect->height + 1) * row;

      if (col == spaces_per_row - 1)
	rect->width = widget->allocation.width - rect->x;
      
      if (row  == pager->priv->n_rows - 1)
	rect->height = widget->allocation.height - rect->y;
    }
}
                    
static GList*
get_windows_for_workspace_in_bottom_to_top (WnckScreen    *screen,
                                            WnckWorkspace *workspace)
{
  GList *result;
  GList *windows;
  GList *tmp;
  
  result = NULL;

  windows = wnck_screen_get_windows_stacked (screen);
  tmp = windows;
  while (tmp != NULL)
    {
      WnckWindow *win = WNCK_WINDOW (tmp->data);

      if (wnck_window_is_visible_on_workspace (win, workspace) &&
          (wnck_window_get_state (win) & WNCK_WINDOW_STATE_SKIP_PAGER) == 0)
        result = g_list_prepend (result, win);
      
      tmp = tmp->next;
    }

  result = g_list_reverse (result);

  return result;
}

static void
get_window_rect (WnckWindow         *window,
                 const GdkRectangle *workspace_rect,
                 GdkRectangle       *rect)
{
  double width_ratio, height_ratio;
  int x, y, width, height;
  
  width_ratio = (double) workspace_rect->width / (double) gdk_screen_width ();
  height_ratio = (double) workspace_rect->height / (double) gdk_screen_height ();
  
  wnck_window_get_geometry (window, &x, &y, &width, &height);
  
  x *= width_ratio;
  y *= height_ratio;
  width *= width_ratio;
  height *= height_ratio;
  
  x += workspace_rect->x;
  y += workspace_rect->y;
  
  if (width < 3)
    width = 3;
  if (height < 3)
    height = 3;

  rect->x = x;
  rect->y = y;
  rect->width = width;
  rect->height = height;
}

static void
draw_window (GdkDrawable        *drawable,
             GtkWidget          *widget,
             WnckWindow         *win,
             const GdkRectangle *winrect)
{
  GdkPixbuf *icon;
  int icon_x, icon_y, icon_w, icon_h;
  gboolean is_active;

  is_active = wnck_window_is_active (win);
          
  gdk_draw_rectangle (drawable,
                      is_active ?
                      widget->style->bg_gc[GTK_STATE_SELECTED] :
                      widget->style->bg_gc[GTK_STATE_NORMAL],
                      TRUE,
                      winrect->x + 1, winrect->y + 1,
                      winrect->width - 2, winrect->height - 2);

  icon = wnck_window_get_icon (win);

  icon_w = icon_h = 0;
          
  if (icon)
    {              
      icon_w = gdk_pixbuf_get_width (icon);
      icon_h = gdk_pixbuf_get_height (icon);

      /* If the icon is too big, fall back to mini icon.
       * We don't arbitrarily scale the icon, because it's
       * just too slow on my Athlon 850.
       */
      if (icon_w > (winrect->width - 2) ||
          icon_h > (winrect->height - 2))
        {
          icon = wnck_window_get_mini_icon (win);
          if (icon)
            {
              icon_w = gdk_pixbuf_get_width (icon);
              icon_h = gdk_pixbuf_get_height (icon);

              /* Give up. */
              if (icon_w > (winrect->width - 2) ||
                  icon_h > (winrect->height - 2))
                icon = NULL;
            }
        }
    }

  if (icon)
    {
      icon_x = winrect->x + (winrect->width - icon_w) / 2;
      icon_y = winrect->y + (winrect->height - icon_h) / 2;
                
      {
        /* render_to_drawable should take a clip rect to save
         * us this mess...
         */
        GdkRectangle pixbuf_rect;
        GdkRectangle draw_rect;
                
        pixbuf_rect.x = icon_x;
        pixbuf_rect.y = icon_y;
        pixbuf_rect.width = icon_w;
        pixbuf_rect.height = icon_h;
                
        if (gdk_rectangle_intersect ((GdkRectangle *)winrect, &pixbuf_rect,
                                     &draw_rect))
          {
            gdk_pixbuf_render_to_drawable_alpha (icon,
                                                 drawable,
                                                 draw_rect.x - pixbuf_rect.x,
                                                 draw_rect.y - pixbuf_rect.y,
                                                 draw_rect.x, draw_rect.y,
                                                 draw_rect.width,
                                                 draw_rect.height,
                                                 GDK_PIXBUF_ALPHA_FULL,
                                                 128,
                                                 GDK_RGB_DITHER_NORMAL,
                                                 0, 0);
          }
      }
    }
          
  gdk_draw_rectangle (drawable,
                      is_active ?
                      widget->style->fg_gc[GTK_STATE_SELECTED] :
                      widget->style->fg_gc[GTK_STATE_NORMAL],
                      FALSE,
                      winrect->x, winrect->y,
                      winrect->width - 1, winrect->height - 1);
}            

static int
workspace_at_point (WnckPager *pager,
                    int        x,
                    int        y)
{
  int i;
  int n_spaces;

  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);
  
  i = 0;
  while (i < n_spaces)
    {
      GdkRectangle rect;
      
      get_workspace_rect (pager, i, &rect);

      if (POINT_IN_RECT (x, y, rect))
        return i;

      ++i;
    }

  return -1;
}


static void
wnck_pager_draw_workspace (WnckPager    *pager,
			   int           workspace,
			   GdkRectangle *rect,
                           GdkPixbuf    *bg_pixbuf)
{
  GList *windows;
  GList *tmp;
  gboolean is_current;
  WnckWorkspace *active_space;
  
  active_space = wnck_screen_get_active_workspace (pager->priv->screen);

  is_current = active_space &&
    workspace == wnck_workspace_get_number (active_space);

  /* FIXME in names mode, should probably draw things like a button.
   */
  
  if (is_current)
    gdk_draw_rectangle (GTK_WIDGET (pager)->window,
                        GTK_WIDGET (pager)->style->dark_gc[GTK_STATE_SELECTED],
                        TRUE,
                        rect->x, rect->y, rect->width, rect->height);
  else if (bg_pixbuf)
    {
      gdk_pixbuf_render_to_drawable (bg_pixbuf,
                                     GTK_WIDGET (pager)->window,
                                     GTK_WIDGET (pager)->style->dark_gc[GTK_STATE_SELECTED],
                                     0, 0,
                                     rect->x, rect->y,
                                     -1, -1,
                                     GDK_RGB_DITHER_MAX,
                                     0, 0);
    }
  else
    gdk_draw_rectangle (GTK_WIDGET (pager)->window,
                        GTK_WIDGET (pager)->style->dark_gc[GTK_STATE_NORMAL],
                        TRUE,
                        rect->x, rect->y, rect->width, rect->height);

  if (pager->priv->display_mode == WNCK_PAGER_DISPLAY_CONTENT)
    {      
      windows = get_windows_for_workspace_in_bottom_to_top (pager->priv->screen,
							    wnck_screen_get_workspace (pager->priv->screen,
										       workspace));
      
      tmp = windows;
      while (tmp != NULL)
	{
	  WnckWindow *win = tmp->data;
	  GdkRectangle winrect;
	  
	  if (pager->priv->dragging &&
	      win == pager->priv->drag_window)
	    {
	      tmp = tmp->next;
	      continue;
	    }
	  
	  get_window_rect (win, rect, &winrect);
	  
	  draw_window (GTK_WIDGET (pager)->window,
		       GTK_WIDGET (pager),
		       win,
		       &winrect);
	  
	  tmp = tmp->next;
	}
      
      g_list_free (windows);
    }
  else
    {
      /* Workspace name mode */
      const char *workspace_name;
      PangoLayout *layout;
      int w, h;

      

      workspace_name = wnck_workspace_get_name (wnck_screen_get_workspace (pager->priv->screen,
									   workspace));
      layout = gtk_widget_create_pango_layout  (GTK_WIDGET (pager),
						workspace_name);
      
      pango_layout_get_pixel_size (layout, &w, &h);
      
      gdk_draw_layout  (GTK_WIDGET (pager)->window,
			is_current ?
			GTK_WIDGET (pager)->style->fg_gc[GTK_STATE_SELECTED] :
			GTK_WIDGET (pager)->style->fg_gc[GTK_STATE_NORMAL],
			rect->x + (rect->width - w) / 2,
			rect->y + (rect->height - h) / 2,
			layout);

      g_object_unref (layout);
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
  GdkPixbuf *bg_pixbuf;
  gboolean first;
  
  pager = WNCK_PAGER (widget);

  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);
  active_space = wnck_screen_get_active_workspace (pager->priv->screen);
  bg_pixbuf = NULL;
  first = TRUE;
  
  i = 0;
  while (i < n_spaces)
    {
      GdkRectangle rect;

      if (pager->priv->show_all_workspaces ||
	  (active_space && i == wnck_workspace_get_number (active_space)))
	{
	  get_workspace_rect (pager, i, &rect);

          /* We only want to do this once, even if w/h change,
           * for efficiency. width/height will only change by
           * one pixel at most.
           */
          if (first &&
              pager->priv->display_mode == WNCK_PAGER_DISPLAY_CONTENT)
            {
              bg_pixbuf = wnck_pager_get_background (pager,
                                                     rect.width,
                                                     rect.height);
              first = FALSE;
            }
          
	  wnck_pager_draw_workspace (pager, i, &rect, bg_pixbuf);
	}
 
      ++i;
    }

  /* Draw the drag window */
  if (pager->priv->dragging)
    {
      GdkRectangle rect;
      GdkRectangle winrect;
      int dx, dy;
      
      i = workspace_at_point (pager,
                              pager->priv->drag_window_x,
                              pager->priv->drag_window_y);

      if (i >= 0)
	{
	  get_workspace_rect (pager, i, &rect);          
	  get_window_rect (pager->priv->drag_window, &rect, &winrect);

	  dx = (pager->priv->drag_window_x - rect.x) -
	    pager->priv->drag_start_x_workspace_relative;
	  dy = (pager->priv->drag_window_y - rect.y) -
	    pager->priv->drag_start_y_workspace_relative;
	  
	  winrect.x += dx;
	  winrect.y += dy;
	  
	  draw_window (widget->window,
		       widget,
		       pager->priv->drag_window,
		       &winrect);
	}
    }
  
  return FALSE;
}

static gboolean
wnck_pager_button_press  (GtkWidget      *widget,
                          GdkEventButton *event)
{
  WnckPager *pager;
  int i;
  int n_spaces;
  gboolean handled = FALSE;
  
  pager = WNCK_PAGER (widget);

  n_spaces = wnck_screen_get_workspace_count (pager->priv->screen);
  
  i = 0;
  while (i < n_spaces)
    {
      GdkRectangle rect;

      get_workspace_rect (pager, i, &rect);

      if (POINT_IN_RECT (event->x, event->y, rect))
        {
          WnckWorkspace *space = wnck_screen_get_workspace (pager->priv->screen, i);

	  if (event->button == 1)
	    handled = TRUE;
	  
	  if (space)
            {
              GList *windows;
              GList *tmp;
              
              windows =
                get_windows_for_workspace_in_bottom_to_top (pager->priv->screen,
                                                            space);

              /* clicks on top windows first */
              windows = g_list_reverse (windows);

              tmp = windows;
              while (tmp != NULL)
                {
                  WnckWindow *win = WNCK_WINDOW (tmp->data);
                  GdkRectangle winrect;

                  get_window_rect (win, &rect, &winrect);

                  if (POINT_IN_RECT (event->x, event->y, winrect))
                    {
                      if (event->button == 1)
                        {
                          // wnck_window_activate (win);
                          pager->priv->drag_window = win;
                          pager->priv->drag_start_x = event->x;
                          pager->priv->drag_start_y = event->y;
                          pager->priv->drag_start_x_workspace_relative =
                            event->x - rect.x;
                          pager->priv->drag_start_y_workspace_relative =
                            event->y - rect.y;
                        }

                      goto window_search_out;
                    }
                  
                  tmp = tmp->next;
                }

            window_search_out:
              
              g_list_free (windows);
              goto workspace_search_out;
            }
        }
      
      ++i;
    }

 workspace_search_out:

  return handled;
}

static gboolean
wnck_pager_motion (GtkWidget        *widget,
                   GdkEventMotion   *event)
{
  WnckPager *pager;
  int x, y;

  pager = WNCK_PAGER (widget);

  gdk_window_get_pointer (widget->window, &x, &y, NULL);

  if (!pager->priv->dragging &&
      pager->priv->drag_window != NULL &&
      gtk_drag_check_threshold (widget,
                                pager->priv->drag_start_x,
                                pager->priv->drag_start_y,
                                x, y))
    pager->priv->dragging = TRUE;

  if (pager->priv->dragging)
    {
      gtk_widget_queue_draw (widget);
      pager->priv->drag_window_x = event->x;
      pager->priv->drag_window_y = event->y;
    }

  return TRUE;
}

static gboolean
wnck_pager_button_release (GtkWidget        *widget,
                           GdkEventButton   *event)
{
  WnckWorkspace *space;
  WnckPager *pager;
  int i;
  gboolean handled = FALSE;

  pager = WNCK_PAGER (widget);

  if (event->button == 1 && pager->priv->dragging)
    {
      
      i = workspace_at_point (pager,
                              event->x,
                              event->y);

      if (i >= 0)
	{
	  space = wnck_screen_get_workspace (pager->priv->screen, i);

	  if (space)
            {
              wnck_window_move_to_workspace (pager->priv->drag_window,
                                             space);
              if (space == wnck_screen_get_active_workspace (pager->priv->screen))
                wnck_window_activate (pager->priv->drag_window);
            }	  
	}
      
      wnck_pager_clear_drag (pager);
      handled = TRUE;
    }
  else if (event->button == 1)
    {
      i = workspace_at_point (pager,
                              event->x,
                              event->y);

      if (i >= 0)
	{
	  space = wnck_screen_get_workspace (pager->priv->screen, i);

	  if (space &&
	      space != wnck_screen_get_active_workspace (pager->priv->screen))
	    wnck_workspace_activate (space);
	}
      
      if (pager->priv->drag_window)
	{
	  wnck_window_activate (pager->priv->drag_window);
	  wnck_pager_clear_drag (pager);
	}
      handled = TRUE;
    }

  return FALSE;
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

void
wnck_pager_set_n_rows (WnckPager *pager,
		       int        n_rows)
{
  g_return_if_fail (WNCK_IS_PAGER (pager));

  if (pager->priv->n_rows == n_rows)
    return;

  pager->priv->n_rows = n_rows;
  gtk_widget_queue_resize (GTK_WIDGET (pager));
}

void
wnck_pager_set_display_mode (WnckPager            *pager,
			     WnckPagerDisplayMode  mode)
{
  g_return_if_fail (WNCK_IS_PAGER (pager));

  if (pager->priv->display_mode == mode)
    return;

  pager->priv->display_mode = mode;
  gtk_widget_queue_resize (GTK_WIDGET (pager));
}

void
wnck_pager_set_show_all (WnckPager *pager,
			 gboolean   show_all_workspaces)
{
  g_return_if_fail (WNCK_IS_PAGER (pager));

  show_all_workspaces = (show_all_workspaces != 0);

  if (pager->priv->show_all_workspaces == show_all_workspaces)
    return;

  pager->priv->show_all_workspaces = show_all_workspaces;
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

  if (pager->priv->drag_window == window)
    wnck_pager_clear_drag (pager);
  
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
background_changed_callback (WnckWindow *window,
                             gpointer    data)
{
  WnckPager *pager = WNCK_PAGER (data);

  if (pager->priv->bg_cache)
    {
      g_object_unref (G_OBJECT (pager->priv->bg_cache));
      pager->priv->bg_cache = NULL;
    }
  
  gtk_widget_queue_draw (GTK_WIDGET (pager));
}

static void
wnck_pager_connect_screen (WnckPager  *pager,
                           WnckScreen *screen)
{
  int i;
  guint *c;
  GList *tmp;
  
  g_return_if_fail (pager->priv->screen == NULL);
  
  pager->priv->screen = screen;

  tmp = wnck_screen_get_windows (screen);
  while (tmp != NULL)
    {
      wnck_pager_connect_window (pager, WNCK_WINDOW (tmp->data));
      tmp = tmp->next;
    }
  
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

  c[i] = g_signal_connect (G_OBJECT (screen), "background_changed",
                           G_CALLBACK (background_changed_callback),
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

static void
wnck_pager_clear_drag (WnckPager *pager)
{
  if (pager->priv->dragging)
    gtk_widget_queue_draw (GTK_WIDGET (pager));

  pager->priv->dragging = FALSE;
  pager->priv->drag_window = NULL;
  pager->priv->drag_start_x = -1;
  pager->priv->drag_start_y = -1;
  pager->priv->drag_window_x = -1;
  pager->priv->drag_window_y = -1;
}

static GdkPixbuf*
wnck_pager_get_background (WnckPager *pager,
                           int        width,
                           int        height)
{
  Pixmap p;
  GdkPixbuf *pix = NULL;
  
  /* We have to be careful not to keep alternating between
   * width/height values, otherwise this would get really slow.
   */
  if (pager->priv->bg_cache &&
      gdk_pixbuf_get_width (pager->priv->bg_cache) == width &&
      gdk_pixbuf_get_height (pager->priv->bg_cache) == height)
    return pager->priv->bg_cache;

  if (pager->priv->bg_cache)
    {
      g_object_unref (G_OBJECT (pager->priv->bg_cache));
      pager->priv->bg_cache = NULL;
    }

  if (pager->priv->screen == NULL)
    return NULL;

  /* FIXME this just globally disables the thumbnailing feature */
  return NULL;
  
#define MIN_BG_SIZE 10
  
  if (width < MIN_BG_SIZE || height < MIN_BG_SIZE)
    return NULL;
  
  p = wnck_screen_get_background_pixmap (pager->priv->screen);

  if (p != None)
    {
      _wnck_error_trap_push ();
      pix = _wnck_gdk_pixbuf_get_from_pixmap (NULL,
                                              p,
                                              0, 0, 0, 0,
                                              -1, -1);
      _wnck_error_trap_pop ();
    }

  if (pix)
    {
      pager->priv->bg_cache = gdk_pixbuf_scale_simple (pix,
                                                       width,
                                                       height,
                                                       GDK_INTERP_BILINEAR);

      g_object_unref (G_OBJECT (pix));
    }

  return pager->priv->bg_cache;
}
