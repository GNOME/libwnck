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
#include <string.h>
#include <stdio.h>
#include "tasklist.h"
#include "window.h"
#include "application.h"
#include "xutils.h"

/* TODO:
 * 
 *  Add total focused time to the grouping score function
 *  Fine tune the grouping scoring function
 *  Fix naming and icon for groups/applications
 *  Maybe fine tune size_allocate() some more...
 *  Better vertical layout handling
 *  prefs
 *  ellipsizing lables
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
#define DEFAULT_GROUPING_LIMIT 80

#define DEFAULT_WIDTH 300
#define DEFAULT_HEIGHT 48

struct _WnckTask
{
  GObject parent_instance;

  WnckTasklist *tasklist;
  
  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *label;
  
  gboolean is_application;
  WnckApplication *application;
  WnckWindow *window;

  gdouble grouping_score;

  GList *windows; /* List of the WnckTask for the window,
		     if this is an application */
  gboolean really_toggling; /* Set when tasklist really wants
			       to change the togglebutton state */
  gulong state_changed_tag;
  gulong icon_changed_tag;
  gulong name_changed_tag;

  GtkWidget *menu;
};

struct _WnckTaskClass
{
  GObjectClass parent_class;
};

struct _WnckTasklistPrivate
{
  WnckScreen *screen;

  WnckTask *active_task; /* NULL if active window not in tasklist */
  WnckTask *active_app; /* NULL if active window not in tasklist */
  
  gboolean include_all_workspaces;
  gboolean include_unminimized;
  
  /* Calculated by update_lists */
  GList *windows;
  GList *applications;
  
  GHashTable *win_hash;
  GHashTable *app_hash;
  
  GtkTooltips *tooltips;

  gint max_button_width;
  gint max_button_height;

  gboolean grouping_enabled;
  gint grouping_limit;

  guint activate_timeout_id;
};


static void wnck_task_init        (WnckTask      *task);
static void wnck_task_class_init  (WnckTaskClass *klass);
static void wnck_task_finalize    (GObject       *object);

static WnckTask *wnck_task_new_from_window      (WnckTasklist    *tasklist,
						 WnckWindow      *window);
static WnckTask *wnck_task_new_from_application (WnckTasklist    *tasklist,
						 WnckApplication *application);

static char      *wnck_task_get_text (WnckTask *task);
static GdkPixbuf *wnck_task_get_icon (WnckTask *task);
static gint       wnck_task_compare  (gconstpointer  a,
				      gconstpointer  b);


static void wnck_tasklist_init        (WnckTasklist      *tasklist);
static void wnck_tasklist_class_init  (WnckTasklistClass *klass);
static void wnck_tasklist_finalize    (GObject        *object);

static void     wnck_tasklist_size_request  (GtkWidget        *widget,
                                             GtkRequisition   *requisition);
static void     wnck_tasklist_size_allocate (GtkWidget        *widget,
                                             GtkAllocation    *allocation);
static void     wnck_tasklist_forall        (GtkContainer     *container,
                                             gboolean	       include_internals,
                                             GtkCallback       callback,
                                             gpointer          callback_data);
static void     wnck_tasklist_remove	    (GtkContainer   *container,
					     GtkWidget	    *widget);
static void     wnck_tasklist_update_lists  (WnckTasklist   *tasklist);
static int      wnck_tasklist_layout        (GtkAllocation  *allocation,
					     int             max_width,
					     int             max_height,
					     int             n_buttons,
					     int            *n_cols_out,
					     int            *n_rows_out);

static void     wnck_tasklist_active_window_changed    (WnckScreen   *screen,
							WnckTasklist *tasklist);
static void     wnck_tasklist_active_workspace_changed (WnckScreen   *screen,
							WnckTasklist *tasklist);
static void     wnck_tasklist_window_added_or_removed  (WnckScreen   *screen,
							WnckWindow   *win,
							WnckTasklist *tasklist);

static void     wnck_tasklist_change_active_task       (WnckTasklist *tasklist,
							WnckTask *active_task);
static gboolean wnck_tasklist_change_active_timeout    (gpointer data);
static void     wnck_tasklist_activate_task_window     (WnckTask *task);

static void     wnck_tasklist_update_icon_geometries   (WnckTasklist *tasklist);

static gpointer task_parent_class;
static gpointer tasklist_parent_class;

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

  gtk_rc_parse_string ("
    style \"tasklist-button-style\"
    {
       GtkWidget::focus-line-width=0
       GtkWidget::focus-padding=0
    }

    widget \"*.tasklist-button\" style \"tasklist-button-style\"
  ");
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

  if (task->state_changed_tag)
    {
      g_signal_handler_disconnect (task->window,
				   task->state_changed_tag);
      task->state_changed_tag = 0;
    }

  if (task->icon_changed_tag)
    {
      g_signal_handler_disconnect (task->window,
				   task->icon_changed_tag);
      task->icon_changed_tag = 0;
    }
  
  if (task->name_changed_tag)
    {
      g_signal_handler_disconnect (task->window,
				   task->name_changed_tag);
      task->name_changed_tag = 0;
    }

  if (task->menu)
    {
      gtk_widget_destroy (task->menu);
      task->menu = NULL;
    }

  if (task->window)
    {
      g_object_unref (task->window);
      task->window = NULL;
    }

  if (task->application)
    {
      g_object_unref (task->application);
      task->application = NULL;
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
  
  tasklist->priv->grouping_enabled = TRUE;
  tasklist->priv->grouping_limit = DEFAULT_GROUPING_LIMIT;
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
  tasklist->priv->win_hash = NULL;
  
  g_hash_table_destroy (tasklist->priv->app_hash);
  tasklist->priv->app_hash = NULL;
  
  if (tasklist->priv->activate_timeout_id != 0)
    gtk_timeout_remove (tasklist->priv->activate_timeout_id);
    
  if (tasklist->priv->tooltips)
    {
      g_object_unref (tasklist->priv->tooltips);
      tasklist->priv->tooltips = NULL;
    }

  g_free (tasklist->priv);
  tasklist->priv = NULL;  
  
  G_OBJECT_CLASS (tasklist_parent_class)->finalize (object);
}

void
wnck_tasklist_set_allow_grouping (WnckTasklist *tasklist,
				  gboolean allow_grouping)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));
  
  tasklist->priv->grouping_enabled = allow_grouping;
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

void
wnck_tasklist_set_grouping_limit (WnckTasklist *tasklist,
				  gint          limit)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));
  
  tasklist->priv->grouping_limit = limit;
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

static void
wnck_tasklist_size_request  (GtkWidget      *widget,
                             GtkRequisition *requisition)
{
  WnckTasklist *tasklist;
  GtkRequisition child_req;
  int max_height = 1;
  int max_width = 1;
  int u_width, u_height;
  GList *l;
  
  tasklist = WNCK_TASKLIST (widget);

  /* Calculate max needed height and width of the buttons */
  l = tasklist->priv->windows;
  while (l != NULL)
    {
      WnckTask *task = WNCK_TASK (l->data);

      gtk_widget_size_request (task->button, &child_req);
      
      max_height = MAX (child_req.height,
			max_height);
      max_width = MAX (child_req.width,
		       max_width);
      
      l = l->next;
    }

  l = tasklist->priv->applications;
  while (l != NULL)
    {
      WnckTask *task = WNCK_TASK (l->data);
      
      gtk_widget_size_request (task->button, &child_req);
      
      max_height = MAX (child_req.height,
			max_height);
      max_width = MAX (child_req.width,
		       max_width);
      
      l = l->next;
    }

  tasklist->priv->max_button_width = max_width;
  tasklist->priv->max_button_height = max_height;


  gtk_widget_get_size_request (widget, &u_width, &u_height);

  requisition->width = DEFAULT_WIDTH;
  requisition->height = DEFAULT_HEIGHT;
  
  if (u_height != -1)
    {
      requisition->height = u_height;
    }
  else if (u_width != -1)
    {
      requisition->width = u_width;
      requisition->height = 4 * max_height;
    }
}

/* returns the maximal possible button width (i.e. if you
 * don't want to stretch the buttons to fill the alloctions
 * the width can be smaller) */
static int
wnck_tasklist_layout (GtkAllocation *allocation,
		      int            max_width,
		      int            max_height,
		      int            n_buttons,
		      int           *n_cols_out,
		      int           *n_rows_out)
{
  int n_cols, n_rows;

  /* How many rows fit in the allocation */
  n_rows = allocation->height / max_height;

  /* Don't have more rows than buttons */
  n_rows = MIN (n_rows, n_buttons);

  /* At least one row */
  n_rows = MAX (n_rows, 1);
  
  /* We want to use as many rows as possible to limit the width */
  n_cols = (n_buttons + n_rows - 1) / n_rows;

  /* At least one column */
  n_cols = MAX (n_cols, 1);

  *n_cols_out = n_cols;
  *n_rows_out = n_rows;
  
  return allocation->width / n_cols;
}

static void
wnck_tasklist_score_groups (WnckTasklist *tasklist,
			    GList        *ungrouped_apps)
{
  WnckTask *app_task;
  WnckTask *win_task;
  GList *l, *w;
  const char *first_name = NULL;
  int n_windows;
  int n_same_title;
  double same_window_ratio;

  l = ungrouped_apps;
  while (l != NULL)
    {
      app_task = WNCK_TASK (l->data);

      n_windows = g_list_length (app_task->windows);

      n_same_title = 0;
      w = app_task->windows;
      while (w != NULL)
	{
	  win_task = WNCK_TASK (w->data);

	  if (first_name == NULL)
	    {
	      first_name = wnck_window_get_name (win_task->window);
	      n_same_title++;
	    }
	  else
	    {
	      if (strcmp (wnck_window_get_name (win_task->window),
			  first_name) == 0)
		n_same_title++;
	    }
	  
	  w = w->next;
	}
      same_window_ratio = (double)n_same_title/(double)n_windows;

      /* FIXME: This is fairly bogus and should be researched more.
       *        XP groups by least used, so we probably want to add
       *        total focused time to this expression.
       */
      app_task->grouping_score = -same_window_ratio * 5 + n_windows;

      l = l->next;
    }
}

static GList *
wnck_task_get_highest_scored (GList     *ungrouped_apps,
			      WnckTask **app_task_out)
{
  WnckTask *app_task;
  WnckTask *best_task = NULL;
  double max_score = -1000000000.0; /* Large negative score */
  GList *l;
 
  l = ungrouped_apps;
  while (l != NULL)
    {
      app_task = WNCK_TASK (l->data);

      if (app_task->grouping_score >= max_score)
	{
	  max_score = app_task->grouping_score;
	  best_task = app_task;
	}
      
      l = l->next;
    }

  *app_task_out = best_task;

  return g_list_remove (ungrouped_apps, best_task);
}


static void
wnck_tasklist_size_allocate (GtkWidget      *widget,
                             GtkAllocation  *allocation)
{
  GtkAllocation child_allocation;
  WnckTasklist *tasklist;
  WnckTask *app_task;
  int n_windows;
  GList *l;
  int button_width;
  int total_width;
  int n_rows;
  int n_cols;
  int n_grouped_buttons;
  int i;
  gboolean score_set;
  GList *ungrouped_apps;
  WnckTask *win_task;
  GList *visible_tasks = NULL;
  
  tasklist = WNCK_TASKLIST (widget);

  n_windows = g_list_length (tasklist->priv->windows);
  n_grouped_buttons = 0;
  ungrouped_apps = g_list_copy (tasklist->priv->applications);
  score_set = FALSE;

  /* Try ungrouped mode */
  button_width = wnck_tasklist_layout (allocation,
				       tasklist->priv->max_button_width,
				       tasklist->priv->max_button_height,
				       n_windows,
				       &n_cols, &n_rows);
  while (tasklist->priv->grouping_enabled &&
	 (ungrouped_apps != NULL) &&
	 (button_width < tasklist->priv->grouping_limit))
    {
      if (!score_set)
	{
	  wnck_tasklist_score_groups (tasklist, ungrouped_apps);
	  score_set = TRUE;
	}

      ungrouped_apps = wnck_task_get_highest_scored (ungrouped_apps, &app_task);

      n_grouped_buttons += g_list_length (app_task->windows) - 1;

      if (g_list_length (app_task->windows) > 1)
	{
	  visible_tasks = g_list_prepend (visible_tasks, app_task);
	  
	  /* Hide all this apps windows */
	  l = app_task->windows;
	  while (l != NULL)
	    {
	      win_task = WNCK_TASK (l->data);
	      
	      gtk_widget_set_child_visible (GTK_WIDGET (win_task->button), FALSE);
	      l = l->next;
	    }
	}
      else
	{
	  visible_tasks = g_list_prepend (visible_tasks, app_task->windows->data);
	  gtk_widget_set_child_visible (GTK_WIDGET (app_task->button), FALSE);
	}
      
      button_width = wnck_tasklist_layout (allocation,
					   tasklist->priv->max_button_width,
					   tasklist->priv->max_button_height,
					   n_windows - n_grouped_buttons,
					   &n_cols, &n_rows);
    }

  /* Add all ungrouped windows to visible_tasks, and hide their apps */
  l = ungrouped_apps;
  while (l != NULL)
    {
      app_task = WNCK_TASK (l->data);
      
      visible_tasks = g_list_concat (visible_tasks, g_list_copy (app_task->windows));
      gtk_widget_set_child_visible (GTK_WIDGET (app_task->button), FALSE);
      l = l->next;
    }

  visible_tasks = g_list_sort (visible_tasks, wnck_task_compare);

  /* Allocate children */
  l = visible_tasks;
  i = 0;
  total_width = tasklist->priv->max_button_width * n_cols;
  total_width = MIN (total_width, allocation->width);
  while (l != NULL)
    {
      WnckTask *task = WNCK_TASK (l->data);
      int col = i % n_cols;
      int row = i / n_cols;
      
      child_allocation.x = total_width*col / n_cols;
      child_allocation.y = allocation->height*row / n_rows;
      child_allocation.width = total_width*(col + 1) / n_cols - child_allocation.x;
      child_allocation.height = allocation->height*(row + 1) / n_rows - child_allocation.y;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      gtk_widget_size_allocate (task->button, &child_allocation);
      gtk_widget_set_child_visible (GTK_WIDGET (task->button), TRUE);

      i++;
      l = l->next;
    }
  g_list_free (visible_tasks);

  /* Update icon geometries. */
  wnck_tasklist_update_icon_geometries (tasklist);
  
  GTK_WIDGET_CLASS (tasklist_parent_class)->size_allocate (widget, allocation);
}

static void
wnck_tasklist_forall (GtkContainer *container,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  WnckTasklist *tasklist;
  GList *tmp;
  
  tasklist = WNCK_TASKLIST (container);

  tmp = tasklist->priv->windows;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;
      
      (* callback) (task->button, callback_data);
    }
  
  tmp = tasklist->priv->applications;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;
      
      (* callback) (task->button, callback_data);
    }
}

static void
wnck_tasklist_remove (GtkContainer   *container,
		      GtkWidget	    *widget)
{
  WnckTasklist *tasklist;
  GList *tmp;
  gboolean found = FALSE;
  
  g_return_if_fail (WNCK_IS_TASKLIST (container));
  g_return_if_fail (widget != NULL);

  tasklist = WNCK_TASKLIST (container);

  tmp = tasklist->priv->windows;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;

      if (task->button == widget)
	{
	  g_hash_table_remove (tasklist->priv->win_hash,
			       task->window);
	  tasklist->priv->windows =
	    g_list_remove (tasklist->priv->windows,
			   task);
	  gtk_widget_destroy (task->button);
	  found = TRUE;
	  break;
	}
      
    }
  
  tmp = tasklist->priv->applications;
  while (!found && (tmp != NULL))
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;
      
      if (task->button == widget)
	{
	  g_hash_table_remove (tasklist->priv->app_hash,
			       task->application);
	  tasklist->priv->applications =
	    g_list_remove (tasklist->priv->applications,
			   task);
	  gtk_widget_destroy (task->button);
	  break;
	}
    }

  gtk_widget_unparent (widget);
}


GtkWidget*
wnck_tasklist_new (WnckScreen *screen)
{
  WnckTasklist *tasklist;

  tasklist = g_object_new (WNCK_TYPE_TASKLIST, NULL);

  tasklist->priv->screen = screen;
  
  tasklist->priv->tooltips = gtk_tooltips_new ();
  gtk_object_ref (GTK_OBJECT (tasklist->priv->tooltips));
  gtk_object_sink (GTK_OBJECT (tasklist->priv->tooltips));

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

  tasklist->priv->active_task = NULL;
  tasklist->priv->active_app = NULL;
  
  if (tasklist->priv->windows)
    {
      l = tasklist->priv->windows;
      while (l != NULL)
	{
	  WnckTask *task = WNCK_TASK (l->data);
	  l = l-> next;
	  g_object_unref (task);
	}
    }
  g_assert (tasklist->priv->windows == NULL);
  g_assert (g_hash_table_size (tasklist->priv->win_hash) == 0);

  if (tasklist->priv->applications)
    {
      l = tasklist->priv->applications;
      while (l != NULL)
	{
	  WnckTask *task = WNCK_TASK (l->data);
	  l = l-> next;
	  g_object_unref (task);
	}
    }
  g_assert (tasklist->priv->applications == NULL);
  g_assert (g_hash_table_size (tasklist->priv->app_hash) == 0);

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
	  gtk_widget_show (win_task->button);
	  
	  app = wnck_window_get_application (win);
	  app_task = g_hash_table_lookup (tasklist->priv->app_hash, app);

	  if (app_task == NULL)
	    {
	      app_task = wnck_task_new_from_application (tasklist, app);
	      gtk_widget_set_parent (app_task->button, GTK_WIDGET (tasklist));
	      gtk_widget_show (app_task->button);
	      
	      tasklist->priv->applications = g_list_prepend (tasklist->priv->applications,
							     app_task);
	      g_hash_table_insert (tasklist->priv->app_hash, app, app_task);
	    }
	  
	    app_task->windows = g_list_prepend (app_task->windows, win_task);
	}
      
      l = l->next;
    }

  /* Sort the application lists */
  l = tasklist->priv->applications;
  while (l)
    {
      app_task = WNCK_TASK (l->data);

      app_task->windows = g_list_sort (app_task->windows, wnck_task_compare);
      
      l = l->next;
    }

  
  /* since we cleared active_window we need to reset it */
  wnck_tasklist_active_window_changed (tasklist->priv->screen, tasklist);
}

static void
wnck_tasklist_change_active_task (WnckTasklist *tasklist, WnckTask *active_task)
{
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

  if (active_task)
    {
      active_task = g_hash_table_lookup (tasklist->priv->app_hash,
					 active_task->application);

      if (active_task &&
	  active_task == tasklist->priv->active_app)
	return;

      if (tasklist->priv->active_app)
	{
	  tasklist->priv->active_app->really_toggling = TRUE;
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tasklist->priv->active_app->button),
					FALSE);
	  tasklist->priv->active_app->really_toggling = FALSE;
	}
  
      tasklist->priv->active_app = active_task;
  
      if (tasklist->priv->active_app)
	{
	  tasklist->priv->active_app->really_toggling = TRUE;
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tasklist->priv->active_app->button),
					TRUE);
	  tasklist->priv->active_app->really_toggling = FALSE;
	}
    }
}

static void
wnck_tasklist_update_icon_geometries (WnckTasklist *tasklist)
{
	gint x, y, width, height;
	GList *list;
	
	for (list = tasklist->priv->windows; list; list = list->next) {
		WnckTask *task = WNCK_TASK (list->data);
		
		if (!GTK_WIDGET_REALIZED (task->button))
			continue;

		gdk_window_get_origin (GTK_BUTTON (task->button)->event_window,
					    &x, &y);
		width = task->button->allocation.width;
		height = task->button->allocation.height;

		wnck_window_set_icon_geometry (task->window,
					       x, y, width, height);
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

  wnck_tasklist_change_active_task (tasklist, active_task);
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
wnck_task_position_menu (GtkMenu   *menu,
			 gint      *x,
			 gint      *y,
			 gboolean  *push_in,
			 gpointer   user_data)
{
  GtkWidget *widget = GTK_WIDGET (user_data);
  GtkRequisition requisition;
  gint menu_xpos;
  gint menu_ypos;

  gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

  gdk_window_get_origin (widget->window, &menu_xpos, &menu_ypos);

  menu_xpos += widget->allocation.x;
  menu_ypos += widget->allocation.y;

  if (menu_ypos >  gdk_screen_height () / 2)
    menu_ypos -= requisition.height;
  else
    menu_ypos += widget->allocation.height;

  *x = menu_xpos;
  *y = menu_ypos;
  *push_in = TRUE;
}

static gboolean
wnck_tasklist_change_active_timeout (gpointer data)
{
  WnckTasklist *tasklist = WNCK_TASKLIST (data);

  tasklist->priv->activate_timeout_id = 0;

  wnck_tasklist_active_window_changed (tasklist->priv->screen, tasklist);

  return FALSE;
}

static void
wnck_task_menu_activated (GtkMenuItem *menu_item,
			  gpointer     data)
{
  WnckTask *task = WNCK_TASK (data);

  wnck_tasklist_activate_task_window (task);
}

static void
wnck_tasklist_activate_task_window     (WnckTask *task)
{
  WnckTasklist *tasklist;
  WnckWindowState state;

  tasklist = task->tasklist;

  if (task->window == NULL)
    return;
      
  state = wnck_window_get_state (task->window);

  if (state & WNCK_WINDOW_STATE_MINIMIZED)
    {
      wnck_window_unminimize (task->window);
      wnck_window_activate (task->window);
    }
  else
    {
      if (wnck_window_is_active (task->window))
	{
	  wnck_window_minimize (task->window);
	  return;
	}
      else
	{
	  wnck_window_activate (task->window);
	}
    }
  

  if (tasklist->priv->activate_timeout_id)
    gtk_timeout_remove (tasklist->priv->activate_timeout_id);

  tasklist->priv->activate_timeout_id = 
    gtk_timeout_add (500, &wnck_tasklist_change_active_timeout, tasklist);

  wnck_tasklist_change_active_task (tasklist, task);
}


static void
wnck_task_popup_menu (WnckTask  *task)
{
  GtkWidget *menu;
  WnckTask *win_task;
  char *text;
  GdkPixbuf *pixbuf;
  GtkWidget *menu_item;
  GtkWidget *image;
  GList *l;
  
  if (task->application == NULL)
    return;
  
  if (task->menu == NULL)
    task->menu = gtk_menu_new ();
  
  menu = task->menu;
  
  /* Remove old menu content */
  l = gtk_container_get_children (GTK_CONTAINER (menu));
  while (l)
    {
      GtkWidget *child = GTK_WIDGET (l->data);
      gtk_container_remove (GTK_CONTAINER (menu), child);
      l = l->next;
    }
  
  l = task->windows;
  while (l)
    {
      win_task = WNCK_TASK (l->data);
      
      text = wnck_task_get_text (win_task);
      menu_item = gtk_image_menu_item_new_with_label (text);
      g_free (text);
      
      pixbuf = wnck_task_get_icon (win_task);
      if (pixbuf)
	{
	  image = gtk_image_new_from_pixbuf (pixbuf);
	  gtk_widget_show (image);
	  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
					 image);
	  g_object_unref (pixbuf);
	}
      
      gtk_widget_show (menu_item);
      
      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (wnck_task_menu_activated), win_task);
      
      
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
      
      l = l->next;
    }
  
  gtk_widget_show (menu);
  gtk_menu_popup (GTK_MENU (menu),
		  NULL, NULL,
		  wnck_task_position_menu, task->button,
		  1, gtk_get_current_event_time ());
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
      wnck_task_popup_menu (task);
    }
  else
    {
      if (task->window == NULL)
	return;

      wnck_tasklist_activate_task_window (task);

      state = wnck_window_get_state (task->window);
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
      return g_strdup ("An application");
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
wnck_dimm_icon (GdkPixbuf *pixbuf)
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

static GdkPixbuf *
wnck_task_scale_icon (GdkPixbuf *orig, gboolean minimized)
{
  int w, h;
  GdkPixbuf *pixbuf;
  
  w = gdk_pixbuf_get_width (orig);
  h = gdk_pixbuf_get_height (orig);

  if (h != MINI_ICON_SIZE ||
      !gdk_pixbuf_get_has_alpha (orig))
    {
      double scale;
      
      pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
			       TRUE,
			       8,
			       MINI_ICON_SIZE * w / (double) h,
			       MINI_ICON_SIZE);
      
      scale = MINI_ICON_SIZE / (double) gdk_pixbuf_get_height (orig);
      
      gdk_pixbuf_scale (orig,
			pixbuf,
			0, 0,
			gdk_pixbuf_get_width (pixbuf),
			gdk_pixbuf_get_height (pixbuf),
			0, 0,
			scale, scale,
			GDK_INTERP_HYPER);
    }
  else
    pixbuf = orig;
  
  if (minimized)
    {
      if (orig == pixbuf)
	pixbuf = gdk_pixbuf_copy (orig);
      
      wnck_dimm_icon (pixbuf);
    }

  if (orig == pixbuf)
    g_object_ref (pixbuf);
  
  return pixbuf;
}


static GdkPixbuf *
wnck_task_get_icon (WnckTask *task)
{
  WnckWindowState state;
  GdkPixbuf *pixbuf;
  gboolean minimized;
  WnckWindow *window;
  
  if (task->is_application)
    {
      GList *l;
      /* FIXME: Implement this correct, Right now, just get a random icon from some window */
      
      l = wnck_application_get_windows (task->application);

      minimized = TRUE;

      while (l)
	{
	  window = WNCK_WINDOW (l->data);
	  if (!(wnck_window_get_state (window) & WNCK_WINDOW_STATE_MINIMIZED))
	    {
	      minimized = FALSE;
	      break;
	    }
	  
	  l = l->next;
	}
      
      l = wnck_application_get_windows (task->application);
      pixbuf =  wnck_task_scale_icon (wnck_window_get_mini_icon (WNCK_WINDOW (l->data)),
				      minimized);
    }
  else
    {
      state = wnck_window_get_state (task->window);

      pixbuf =  wnck_task_scale_icon (wnck_window_get_mini_icon (task->window),
				      state & WNCK_WINDOW_STATE_MINIMIZED);
    }

  return pixbuf;
}


static void
wnck_task_update_visible_state (WnckTask *task)
{
  GdkPixbuf *pixbuf;
  char *text;

  pixbuf = wnck_task_get_icon (task);
  gtk_image_set_from_pixbuf (GTK_IMAGE (task->image),
			     pixbuf);
  if (pixbuf)
    g_object_unref (pixbuf);

  text = wnck_task_get_text (task);
  gtk_label_set_text (GTK_LABEL (task->label), text);
  gtk_tooltips_set_tip (task->tasklist->priv->tooltips, task->button, text, NULL);
  g_free (text);

  gtk_widget_queue_resize (GTK_WIDGET (task->tasklist));
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
      WnckTask *win_task, *app_task;

      win_task = g_hash_table_lookup (tasklist->priv->win_hash,
						window);
      if (win_task)
	{
	  wnck_task_update_visible_state (win_task);
	  
	  app_task = g_hash_table_lookup (tasklist->priv->app_hash,
					  win_task->application);

	  if (app_task)
	    wnck_task_update_visible_state (app_task);
	}
    }
    
}

static void
wnck_task_icon_changed (WnckWindow *window,
			gpointer    data)
{
  WnckTask *task = WNCK_TASK (data);

  if (task)
    wnck_task_update_visible_state (task);
}

static void
wnck_task_name_changed (WnckWindow *window,
			gpointer    data)
{
  WnckTask *task = WNCK_TASK (data);

  if (task)
    wnck_task_update_visible_state (task);
}

static gboolean
wnck_task_button_press_event (GtkWidget	      *widget,
			      GdkEventButton  *event,
			      gpointer         data)
{
  WnckTask *task = WNCK_TASK (data);
  
  if (event->button != 1)
    {
      g_signal_stop_emission_by_name (widget,
				      "button_press_event");
    }
  else
    {
      if (task->is_application)
	{
	  wnck_task_popup_menu (task);
	  g_signal_stop_emission_by_name (widget,
					  "button_press_event");
	}
    }
  return FALSE;
}


static void
wnck_task_create_widgets (WnckTask *task)
{
  GtkWidget *table;
  GdkPixbuf *pixbuf;
  GtkRcStyle *rc_style;
  char *text;
  
  task->button = gtk_toggle_button_new ();

  GTK_WIDGET_UNSET_FLAGS(task->button, GTK_CAN_FOCUS);

  gtk_widget_set_name (task->button,
		       "tasklist-button");

  table = gtk_table_new (1, 2, FALSE);

  pixbuf = wnck_task_get_icon (task);
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
  gtk_widget_show (task->label);

  gtk_table_attach (GTK_TABLE (table),
		    task->image,
		    0, 1,
		    0, 1,
		    0, GTK_EXPAND,
		    0, 0);
  gtk_table_attach (GTK_TABLE (table),
		    task->label,
		    1, 2,
		    0, 1,
		    GTK_EXPAND, GTK_EXPAND,
		    0, 0);

  gtk_container_add (GTK_CONTAINER (task->button), table);
  gtk_widget_show (table);
  
  gtk_tooltips_set_tip (task->tasklist->priv->tooltips, task->button, text, NULL);
  g_free (text);
  
  /* Set up signals */
  g_signal_connect (G_OBJECT (task->button), "toggled",
		    G_CALLBACK (wnck_task_button_toggled), task);

  
  g_signal_connect (G_OBJECT (task->button), "button_press_event",
		    G_CALLBACK (wnck_task_button_press_event), task);

  if (!task->is_application)
    {
      task->state_changed_tag = g_signal_connect (G_OBJECT (task->window), "state_changed",
						  G_CALLBACK (wnck_task_state_changed), task->tasklist);
      task->icon_changed_tag = g_signal_connect (G_OBJECT (task->window), "icon_changed",
						 G_CALLBACK (wnck_task_icon_changed), task);
      task->name_changed_tag = g_signal_connect (G_OBJECT (task->window), "name_changed",
						 G_CALLBACK (wnck_task_name_changed), task);
    }

}

static void
draw_dot (GdkWindow *window, GdkGC *lgc, GdkGC *dgc, int x, int y)
{
  gdk_draw_point (window, dgc, x,   y);
  gdk_draw_point (window, lgc, x+1, y+1);
}


gboolean
wnck_task_app_expose (GtkWidget        *widget,
		      GdkEventExpose   *event,
		      gpointer          data)
{
  GtkStyle *style;
  GdkGC *lgc, *dgc;
  int x, y, i, j;

  style = widget->style;
  
  lgc = style->light_gc[GTK_STATE_NORMAL];
  dgc = style->dark_gc[GTK_STATE_NORMAL];

  x = widget->allocation.x + widget->allocation.width -
    (GTK_CONTAINER (widget)->border_width + style->ythickness + 10);
  y = widget->allocation.y + style->xthickness + 2;
 
  for (i = 0; i < 3; i++) {
    for (j = i; j < 3; j++) {
      draw_dot (widget->window, lgc, dgc, x + j*3, y + i*3);
    }
  }
  return FALSE;
}

static gint
wnck_task_compare (gconstpointer  a,
		   gconstpointer  b)
{
  WnckTask *task1 = WNCK_TASK (a);
  WnckTask *task2 = WNCK_TASK (b);
  gulong xid1_1, xid1_2;
  gulong xid2_1, xid2_2;

  if (task1->is_application)
    xid1_1 = xid1_2 = wnck_application_get_xid (task1->application);
  else
    {
      xid1_1 = wnck_window_get_group_leader (task1->window);
      xid1_2 = wnck_window_get_xid (task1->window);
    }

  if (task2->is_application)
    xid2_1 = xid2_2 = wnck_application_get_xid (task2->application);
  else
    {
      xid2_1 = wnck_window_get_group_leader (task2->window);
      xid2_2 = wnck_window_get_xid (task2->window);
    }
    
  if ((xid1_1 < xid2_1) || ((xid1_1 == xid2_1) && (xid1_2 < xid2_2)))
    return -1;
  else if ((xid1_1 == xid2_1) && (xid1_2 == xid2_2))
    return 0;
  else
    return 1;
}

static WnckTask *
wnck_task_new_from_window (WnckTasklist *tasklist,
			   WnckWindow   *window)
{
  WnckTask *task;

  task = g_object_new (WNCK_TYPE_TASK, NULL);

  task->is_application = FALSE;
  task->window = g_object_ref (window);
  task->application = g_object_ref (wnck_window_get_application (window));
  task->tasklist = tasklist;
  
  wnck_task_create_widgets (task);

  return task;
}

static WnckTask *
wnck_task_new_from_application (WnckTasklist    *tasklist,
				WnckApplication *application)
{
  WnckTask *task;

  task = g_object_new (WNCK_TYPE_TASK, NULL);

  task->is_application = TRUE;
  task->application = g_object_ref (application);
  task->window = NULL;
  task->tasklist = tasklist;
  
  wnck_task_create_widgets (task);
  
  g_signal_connect_after (task->button, "expose_event",
			  G_CALLBACK (wnck_task_app_expose), task);
  
  return task;
}
