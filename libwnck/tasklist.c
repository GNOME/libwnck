/* tasklist object */

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

#include "tasklist.h"
#include "window.h"
#include "application.h"

/* TODO:
 * 
 *  Grouping
 *  Fine tune size_allocate()
 *  Vertical layout handling
 *  prefs
 *  ellipsizing lables
 *  sort windows
 *  bug that makes labels not resize on workspace change sometimes
 *  support for right-click menu merging w/ bonobo for the applet
 *
 */ 


#define WNCK_TYPE_TASK              (wnck_task_get_type ())
#define WNCK_TASK(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WNCK_TYPE_TASK, WnckTask))
#define WNCK_TASK_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WNCK_TYPE_TASK, WnckTaskClass))
#define WNCK_IS_TASK(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WNCK_TYPE_TASK))
#define WNCK_IS_TASK_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WNCK_TYPE_TASK))
#define WNCK_TASK_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WNCK_TYPE_TASK, WnckTaskClass))

typedef struct _WnckTask        WnckTask;
typedef struct _WnckTaskClass   WnckTaskClass;

#define MINI_ICON_SIZE 16

struct _WnckTask
{
  GObject parent_instance;

  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *label;

  gboolean is_application;
  WnckApplication *application;
  WnckWindow *window;

  GList *windows; /* List of the WnckTask for the window,
		     if this is an application */

  gboolean really_toggling; /* Set when tasklist really wants
			       to change the togglebutton state */
};

struct _WnckTaskClass
{
  GObjectClass parent_class;
};

struct _WnckTasklistPrivate
{
  WnckScreen *screen;

  WnckTask *active_task; /* NULL if active window not in tasklist */
  
  gboolean include_all_workspaces;
  gboolean include_unminimized;
  
  /* Calculated by update_lists */
  GList *windows;
  GList *applications;
  
  /* Calculated by size_allocate */
  GList *tasks;

  GHashTable *win_hash;
  GHashTable *app_hash;
};

enum
{
  dummy, /* remove this when you add more signals */
  LAST_SIGNAL
};

static void wnck_task_init        (WnckTask      *task);
static void wnck_task_class_init  (WnckTaskClass *klass);
static void wnck_task_finalize    (GObject       *object);

static WnckTask *wnck_task_new_from_window      (WnckTasklist    *tasklist,
						 WnckWindow      *window);
static WnckTask *wnck_task_new_from_application (WnckTasklist    *tasklist,
						 WnckApplication *application);

static void wnck_tasklist_init        (WnckTasklist      *tasklist);
static void wnck_tasklist_class_init  (WnckTasklistClass *klass);
static void wnck_tasklist_finalize    (GObject        *object);

static void     wnck_tasklist_size_request  (GtkWidget        *widget,
                                             GtkRequisition   *requisition);
static void     wnck_tasklist_size_allocate (GtkWidget        *widget,
                                             GtkAllocation    *allocation);
static gboolean wnck_tasklist_focus         (GtkWidget        *widget,
                                             GtkDirectionType  direction);
static void     wnck_tasklist_forall        (GtkContainer     *container,
                                             gboolean	       include_internals,
                                             GtkCallback       callback,
                                             gpointer          callback_data);
static void     wnck_tasklist_remove	    (GtkContainer   *container,
					     GtkWidget	    *widget);
static void     wnck_tasklist_update_lists  (WnckTasklist     *tasklist);

static void     wnck_tasklist_active_window_changed    (WnckScreen   *screen,
							WnckTasklist *tasklist);
static void     wnck_tasklist_active_workspace_changed (WnckScreen   *screen,
							WnckTasklist *tasklist);
static void     wnck_tasklist_window_added_or_removed  (WnckScreen   *screen,
							WnckWindow   *win,
							WnckTasklist *tasklist);

static gpointer task_parent_class;
static gpointer tasklist_parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

GType
wnck_task_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();
  
  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (WnckTaskClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) wnck_task_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (WnckTask),
        0,              /* n_preallocs */
        (GInstanceInitFunc) wnck_task_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "WnckTask",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
wnck_task_init (WnckTask *task)
{  
}

static void
wnck_task_class_init (WnckTaskClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  task_parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = wnck_task_finalize;
}

static void
wnck_task_finalize (GObject *object)
{
  WnckTask *task;

  task = WNCK_TASK (object);

  if (task->button)
    {
      gtk_widget_destroy (task->button);
      task->button = NULL;
    }

  if (task->is_application)
    {
      g_list_free (task->windows);
      task->windows = NULL;
    }

  G_OBJECT_CLASS (task_parent_class)->finalize (object);
}

GType
wnck_tasklist_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();
  
  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (WnckTasklistClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) wnck_tasklist_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (WnckTasklist),
        0,              /* n_preallocs */
        (GInstanceInitFunc) wnck_tasklist_init,
      };
      
      object_type = g_type_register_static (GTK_TYPE_CONTAINER,
                                            "WnckTasklist",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
wnck_tasklist_init (WnckTasklist *tasklist)
{
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (tasklist), GTK_NO_WINDOW);
  
  tasklist->priv = g_new0 (WnckTasklistPrivate, 1);

  tasklist->priv->include_all_workspaces = FALSE;
  tasklist->priv->include_unminimized = TRUE;
  
  tasklist->priv->win_hash = g_hash_table_new (NULL, NULL);
  tasklist->priv->app_hash = g_hash_table_new (NULL, NULL);

}

static void
wnck_tasklist_class_init (WnckTasklistClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  
  tasklist_parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = wnck_tasklist_finalize;

  widget_class->size_request = wnck_tasklist_size_request;
  widget_class->size_allocate = wnck_tasklist_size_allocate;

  container_class->forall = wnck_tasklist_forall;
  container_class->remove = wnck_tasklist_remove;
}

static void
wnck_tasklist_finalize (GObject *object)
{
  WnckTasklist *tasklist;

  tasklist = WNCK_TASKLIST (object);

  g_hash_table_destroy (tasklist->priv->win_hash);
  g_hash_table_destroy (tasklist->priv->app_hash);
  
  g_free (tasklist->priv);
  
  G_OBJECT_CLASS (tasklist_parent_class)->finalize (object);
}

static void
wnck_tasklist_size_request  (GtkWidget      *widget,
                             GtkRequisition *requisition)
{
  WnckTasklist *tasklist;
  
  g_print ("wnck_tasklist_size_request()\n");
  tasklist = WNCK_TASKLIST (widget);

  requisition->width = 400;
  requisition->height = 48;
}

static void
wnck_tasklist_size_allocate (GtkWidget      *widget,
                             GtkAllocation  *allocation)
{
  GtkRequisition child_req;
  GtkAllocation child_allocation;
  WnckTasklist *tasklist;
  int n_windows;
  int n_applications;
  GList *l;
  int max_height = 1;
  int max_width = 1;
  int button_height;
  int button_width;
  int total_width;
  int n_rows;
  int n_cols;
  int i;
  gboolean grouped;

  g_print ("wnck_tasklist_size_allocate()\n");
  
  tasklist = WNCK_TASKLIST (widget);

  n_windows = g_list_length (tasklist->priv->windows);
  n_applications = g_list_length (tasklist->priv->applications);
 
  /* Calculate max height from the windows only, should be ok.
   * The applications mostly use data from the leading windows.
   */
  l = tasklist->priv->windows;
  while (l != NULL)
    {
      WnckTask *task = l->data;
      
      gtk_widget_size_request (task->button, &child_req);
      
      max_height = MAX (child_req.height,
			max_height);
      max_width = MAX (child_req.width,
		       max_width);
      
      l = l->next;
    }

  n_rows = allocation->height / max_height;
  n_rows = MIN (n_rows, n_windows);
  n_rows = MAX (n_rows, 1);
  button_height = allocation->height / n_rows;

  /* Try ungrouped mode */
  n_cols = (n_windows + n_rows - 1) / n_rows;
  n_cols = MAX (n_cols, 1);
  button_width = allocation->width / n_cols;

  g_print ("n_rows: %d, n_cols: %d\n", n_rows, n_cols);
  
  //  if (button_width > 30)
    {
      grouped = FALSE;
      tasklist->priv->tasks = g_list_copy (tasklist->priv->windows);
    }
#if 0
  else
    {
      n_cols = (n_applications + n_rows - 1) / n_rows;
      n_cols = MAX (n_cols, 1);
      button_width = allocation.width / n_cols;
      grouped = TRUE;
      tasklist->priv->tasks = g_list_copy (tasklist->priv->applications);
    }
#endif

  /* Allocate children */
  l = tasklist->priv->tasks;
  i = 0;
  total_width = max_width * n_cols;
  total_width = MIN (total_width, allocation->width);
  while (l != NULL)
    {
      WnckTask *task = l->data;
      int col = i % n_cols;
      int row = i / n_cols;
      
      child_allocation.x = total_width*col / n_cols;
      child_allocation.y = allocation->height*row / n_rows;
      child_allocation.width = total_width*(col + 1) / n_cols - child_allocation.x;
      child_allocation.height = allocation->height*(row + 1) / n_rows - child_allocation.y;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;
      
      gtk_widget_size_allocate (task->button, &child_allocation);
      gtk_widget_show (task->button);

      i++;
      l = l->next;
    }
  
  GTK_WIDGET_CLASS (tasklist_parent_class)->size_allocate (widget, allocation);
}

static gboolean
wnck_tasklist_focus (GtkWidget        *widget,
                     GtkDirectionType  direction)
{
  WnckTasklist *tasklist;

  tasklist = WNCK_TASKLIST (widget);

  /* Hmm, maybe the default impl. works if it's in forall order */
  return GTK_WIDGET_CLASS (tasklist_parent_class)->focus (widget, direction);
}

static void
wnck_tasklist_forall (GtkContainer *container,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  WnckTasklist *tasklist;
  GList *tmp;
  
  g_print ("wnck_tasklist_forall()\n");
  
  tasklist = WNCK_TASKLIST (container);

  tmp = tasklist->priv->tasks;
  while (tmp != NULL)
    {
      WnckTask *task = tmp->data;
      
      (* callback) (task->button, callback_data);
      
      tmp = tmp->next;
    }
}

static void
wnck_tasklist_remove (GtkContainer   *container,
		      GtkWidget	    *widget)
{
  WnckTasklist *tasklist;
  GList *children;
  
  g_return_if_fail (WNCK_IS_TASKLIST (container));
  g_return_if_fail (widget != NULL);

  tasklist = WNCK_TASKLIST (container);

  children = tasklist->priv->tasks;
  
  while (children)
    {
      WnckTask *task = children->data;
      children = children->next;
      
      if (task->button == widget)
	{
	  gtk_widget_unparent (widget);
	  tasklist->priv->tasks = g_list_remove (tasklist->priv->tasks, task);

	  if (tasklist->priv->active_task == task)
	    tasklist->priv->active_task = NULL;
	  
	  break;
	}
    }
}


GtkWidget*
wnck_tasklist_new (WnckScreen *screen)
{
  WnckTasklist *tasklist;

  g_print ("wnck_tasklist_new()\n");
  
  tasklist = g_object_new (WNCK_TYPE_TASKLIST, NULL);

  tasklist->priv->screen = screen;

  wnck_tasklist_update_lists (tasklist);


  g_signal_connect (G_OBJECT (screen), "active_window_changed",
		    G_CALLBACK (wnck_tasklist_active_window_changed),
		    tasklist);
  g_signal_connect (G_OBJECT (screen), "active_workspace_changed",
		    G_CALLBACK (wnck_tasklist_active_workspace_changed),
		    tasklist);
  g_signal_connect (G_OBJECT (screen), "window_opened",
		    G_CALLBACK (wnck_tasklist_window_added_or_removed),
		    tasklist);
  g_signal_connect (G_OBJECT (screen), "window_closed",
		    G_CALLBACK (wnck_tasklist_window_added_or_removed),
		    tasklist);
  
  return GTK_WIDGET (tasklist);
}

static gboolean remove_all (gpointer  key,
			    gpointer  value,
			    gpointer  user_data)
{
  return TRUE;
}

static void
wnck_tasklist_update_lists (WnckTasklist *tasklist)
{
  GList *windows;
  WnckWindow *win;
  WnckApplication *app;
  GList *l;
  WnckWorkspace *active_workspace;
  WnckTask *app_task;
  WnckTask *win_task;

  g_print ("wnck_tasklist_update_lists()\n");
  
  /* Remove old contents of hashtable and lists */
  g_hash_table_foreach_remove (tasklist->priv->win_hash, remove_all, NULL);
  g_hash_table_foreach_remove (tasklist->priv->app_hash, remove_all, NULL);

  if (tasklist->priv->windows)
    {
      l = tasklist->priv->windows;
      while (l != NULL)
	{
	  g_object_unref (l->data);
	  l = l-> next;
	}
      g_list_free (tasklist->priv->windows);
      tasklist->priv->windows = NULL;
    }

  if (tasklist->priv->applications)
    {
      l = tasklist->priv->applications;
      while (l != NULL)
	{
	  g_object_unref (l->data);
	  l = l-> next;
	}
      g_list_free (tasklist->priv->applications);
      tasklist->priv->applications = NULL;
    }

  active_workspace = wnck_screen_get_active_workspace (tasklist->priv->screen);  
  windows = wnck_screen_get_windows (tasklist->priv->screen);
  
  l = windows;
  while (l != NULL)
    {
      WnckWindowState state;
      win = WNCK_WINDOW (l->data);

      state = wnck_window_get_state (win);
      if ((state & WNCK_WINDOW_STATE_SKIP_TASKLIST) == 0 &&
	  (tasklist->priv->include_all_workspaces ||
	   wnck_window_is_on_workspace (win, active_workspace)) &&
	  (tasklist->priv->include_unminimized ||
	   (state & WNCK_WINDOW_STATE_MINIMIZED)))
	{
	  win_task = wnck_task_new_from_window (tasklist, win);
	  tasklist->priv->windows = g_list_prepend (tasklist->priv->windows, win_task);
	  g_hash_table_insert (tasklist->priv->win_hash, win, win_task);
	  
	  gtk_widget_set_parent (win_task->button, GTK_WIDGET (tasklist));
      
	  app = wnck_window_get_application (win);
	  app_task = g_hash_table_lookup (tasklist->priv->app_hash, app);

	  if (app_task)
	    app_task->windows = g_list_prepend (app_task->windows, win_task);
	  else
	    {
	      app_task = wnck_task_new_from_application (tasklist, app);
	      gtk_widget_set_parent (app_task->button, GTK_WIDGET (tasklist));
	      
	      tasklist->priv->applications = g_list_prepend (tasklist->priv->applications,
							     app_task);
	      g_hash_table_insert (tasklist->priv->app_hash, app, app_task);
	    }
	}
      
      l = l->next;
    }
}

static void
wnck_tasklist_active_window_changed (WnckScreen   *screen,
				     WnckTasklist *tasklist)
{
  WnckWindow *active_window;
  WnckTask *active_task;
  
  active_window = wnck_screen_get_active_window (screen);

  active_task = g_hash_table_lookup (tasklist->priv->win_hash,
				     active_window);

  if (active_task &&
      active_task == tasklist->priv->active_task)
    return;

  if (tasklist->priv->active_task)
    {
      tasklist->priv->active_task->really_toggling = TRUE;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tasklist->priv->active_task->button),
				    FALSE);
      tasklist->priv->active_task->really_toggling = FALSE;
    }
  
  tasklist->priv->active_task = active_task;
  
  if (tasklist->priv->active_task)
    {
      tasklist->priv->active_task->really_toggling = TRUE;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tasklist->priv->active_task->button),
				    TRUE);
      tasklist->priv->active_task->really_toggling = FALSE;
    }
}

static void
wnck_tasklist_active_workspace_changed (WnckScreen   *screen,
					WnckTasklist *tasklist)
{
  wnck_tasklist_update_lists (tasklist);
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

static void
wnck_tasklist_window_added_or_removed (WnckScreen   *screen,
				       WnckWindow   *win,
				       WnckTasklist *tasklist)
{
  wnck_tasklist_update_lists (tasklist);
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

static void
wnck_task_button_toggled (GtkButton *button,
			  WnckTask  *task)
{
  WnckWindowState state;

  /* Did we really want to change the state of the togglebutton? */
  if (task->really_toggling)
    return;
  
  /* Undo the toggle */
  task->really_toggling = TRUE;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
  task->really_toggling = FALSE;

  if (task->is_application)
    {
      /* FIXME: Implement this */
    }
  else
    {
      state = wnck_window_get_state (task->window);
      if (state & WNCK_WINDOW_STATE_MINIMIZED)
	{
	  wnck_window_unminimize (task->window);
	  wnck_window_activate (task->window);
	}
      else
	{
	  if (wnck_window_is_active (task->window))
	    wnck_window_minimize (task->window);
	  else
	    wnck_window_activate (task->window);
	}
    }
  
}

static char *
wnck_task_get_text (WnckTask *task)
{
  WnckWindowState state;
  const char *name;
  
  if (task->is_application)
    {
      /* FIXME: Implement this */
      return g_strdup ("app");
    }
  else
    {
      state = wnck_window_get_state (task->window);
      name = wnck_window_get_name (task->window);
      
      if (state & WNCK_WINDOW_STATE_MINIMIZED)
	return g_strdup_printf ("[%s]", name);
      else
	return g_strdup (name);
    }
}

static void
wnck_modify_icon (GdkColor *background, GdkPixbuf *pixbuf)
{
  int x, y, pixel_stride, row_stride;
  guchar *row, *pixels;
  int w, h;
  gint32 red, green, blue;
  
  red = background->red / 255;
  blue = background->blue / 255;
  green = background->green / 255;

  w = gdk_pixbuf_get_width (pixbuf);
  h = gdk_pixbuf_get_height (pixbuf);

  if (gdk_pixbuf_get_has_alpha (pixbuf))
    pixel_stride = 4;
  else
    pixel_stride = 3;
	    

  row = gdk_pixbuf_get_pixels (pixbuf);
  row_stride = gdk_pixbuf_get_rowstride (pixbuf);

  for (y = 0; y < h; y++)
    {
      pixels = row;

      for (x = 0; x < w; x++)
	{
	  pixels[0] = ((pixels[0] - red) >> 1) + red;
	  pixels[1] = ((pixels[1] - green) >> 1) + green;
	  pixels[2] = ((pixels[2] - blue) >> 1) + blue;
	  
	  pixels += pixel_stride;
	}

      row += row_stride;
    }
}

static GdkPixbuf *
wnck_task_get_icon (WnckTasklist *tasklist, WnckTask *task)
{
  WnckWindowState state;
  GdkPixbuf *orig;
  GdkPixbuf *pixbuf;
  int w, h;
  
  if (task->is_application)
    {
      /* FIXME: Implement this */
      return NULL;
    }
  else
    {
      state = wnck_window_get_state (task->window);
      
      orig = pixbuf = wnck_window_get_mini_icon (task->window);

      if (orig == NULL)
	return NULL;
      
      w = gdk_pixbuf_get_width (pixbuf);
      h = gdk_pixbuf_get_height (pixbuf);

      if (h != MINI_ICON_SIZE)
	pixbuf = gdk_pixbuf_scale_simple (orig,
					  MINI_ICON_SIZE * w / (double) h,
					  MINI_ICON_SIZE,
					  GDK_INTERP_HYPER);
      
      if (state & WNCK_WINDOW_STATE_MINIMIZED)
	{
	  if (orig == pixbuf)
	    pixbuf = gdk_pixbuf_copy (orig);

	  wnck_modify_icon (&GTK_WIDGET (tasklist)->style->bg[GTK_STATE_NORMAL], pixbuf);
	}
    }

  if (orig == pixbuf)
    g_object_ref (pixbuf);

  return pixbuf;
}


static void
wnck_task_update_visible_state (WnckTasklist *tasklist,
				WnckTask     *task)
{
  GdkPixbuf *pixbuf;
  char *text;
  
  pixbuf = wnck_task_get_icon (tasklist, task);
  gtk_image_set_from_pixbuf (GTK_IMAGE (task->image),
			     pixbuf);
  if (pixbuf)
    g_object_unref (pixbuf);

  text = wnck_task_get_text (task);
  gtk_label_set_text (GTK_LABEL (task->label), text);
  g_free (text);

  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

static void
wnck_task_state_changed (WnckWindow     *window,
			 WnckWindowState changed_mask,
			 WnckWindowState new_state,
			 gpointer        data)
{
  WnckTasklist *tasklist = WNCK_TASKLIST (data);
  
  if (changed_mask & WNCK_WINDOW_STATE_SKIP_TASKLIST)
    {
      wnck_tasklist_update_lists  (tasklist);
      gtk_widget_queue_resize (GTK_WIDGET (tasklist));
      return;
    }
  
  if (changed_mask & WNCK_WINDOW_STATE_MINIMIZED)
    {
      WnckTask *task = g_hash_table_lookup (tasklist->priv->win_hash,
					    window);

      if (task)
	wnck_task_update_visible_state (tasklist, task);
    }
    
}

static void
wnck_task_icon_or_name_changed (WnckWindow *window,
				gpointer        data)
{
  WnckTasklist *tasklist = WNCK_TASKLIST (data);
  WnckTask *task = g_hash_table_lookup (tasklist->priv->win_hash,
					window);
  
  if (task)
    wnck_task_update_visible_state (tasklist, task);
}
  
static void
wnck_task_create_widgets (WnckTasklist *tasklist, WnckTask *task)
{
  GtkWidget *hbox;
  GdkPixbuf *pixbuf;
  char *text;
  
  g_print ("wnck_task_create_widgets()\n");
  task->button = gtk_toggle_button_new ();

  hbox = gtk_hbox_new (FALSE, 2);

  pixbuf = wnck_task_get_icon (tasklist, task);
  if (pixbuf)
    {
      task->image = gtk_image_new_from_pixbuf (pixbuf);
      g_object_unref (pixbuf);
    }
  else
    task->image = gtk_image_new ();
  
  gtk_widget_show (task->image);

  text = wnck_task_get_text (task);
  task->label = gtk_label_new (text);
  g_free (text);
  gtk_widget_show (task->label);

  if (task->image)
    gtk_box_pack_start (GTK_BOX (hbox), task->image, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), task->label, FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (task->button), hbox);
  gtk_widget_show (hbox);


  /* Set up signals */
  g_signal_connect (G_OBJECT (task->button), "toggled",
		    G_CALLBACK (wnck_task_button_toggled), task);

  if (!task->is_application)
    {
      g_signal_connect (G_OBJECT (task->window), "state_changed",
			G_CALLBACK (wnck_task_state_changed), tasklist);
      g_signal_connect (G_OBJECT (task->window), "icon_changed",
			G_CALLBACK (wnck_task_icon_or_name_changed), tasklist);
      g_signal_connect (G_OBJECT (task->window), "name_changed",
			G_CALLBACK (wnck_task_icon_or_name_changed), tasklist);
    }

}
			  


static WnckTask *
wnck_task_new_from_window (WnckTasklist *tasklist, WnckWindow      *window)
{
  WnckTask *task;

  g_print ("wnck_task_new_from_window()\n");
  task = g_object_new (WNCK_TYPE_TASK, NULL);

  task->is_application = FALSE;
  task->window = window;
  task->application = wnck_window_get_application (window);
  
  wnck_task_create_widgets (tasklist, task);
  
  return task;
}

static WnckTask *
wnck_task_new_from_application (WnckTasklist *tasklist, WnckApplication *application)
{
  WnckTask *task;

  g_print ("wnck_task_new_from_app()\n");
  task = g_object_new (WNCK_TYPE_TASK, NULL);

  task->is_application = TRUE;
  task->application = application;
  task->window = NULL;
  
  wnck_task_create_widgets (tasklist, task);
  
  return task;
}
