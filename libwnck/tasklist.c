/* tasklist object */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Kim Woelders
 * Copyright (C) 2003 Red Hat, Inc.
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

#include <config.h>
#include <string.h>
#include <stdio.h>
#include "tasklist.h"
#include "window.h"
#include "class-group.h"
#include "window-action-menu.h"
#include "workspace.h"
#include "xutils.h"
#include "private.h"

/* TODO:
 * 
 *  Add total focused time to the grouping score function
 *  Fine tune the grouping scoring function
 *  Fix "changes" to icon for groups/applications 
 *  Maybe fine tune size_allocate() some more...
 *  Better vertical layout handling
 *  prefs
 *  ellipsizing labels
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

#define DEFAULT_WIDTH 1
#define DEFAULT_HEIGHT 48

#define TIMEOUT_ACTIVATE 1000

#define N_SCREEN_CONNECTIONS 5

#define POINT_IN_RECT(xcoord, ycoord, rect) \
 ((xcoord) >= (rect).x &&                   \
  (xcoord) <  ((rect).x + (rect).width) &&  \
  (ycoord) >= (rect).y &&                   \
  (ycoord) <  ((rect).y + (rect).height))

typedef enum
{
  WNCK_TASK_CLASS_GROUP,
  WNCK_TASK_WINDOW,
  WNCK_TASK_STARTUP_SEQUENCE
} WnckTaskType;

struct _WnckTask
{
  GObject parent_instance;

  WnckTasklist *tasklist;
  
  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *label;

  WnckTaskType type;

  WnckClassGroup *class_group;
  WnckWindow *window;
#ifdef HAVE_STARTUP_NOTIFICATION
  SnStartupSequence *startup_sequence;
#endif
  
  gdouble grouping_score;

  GList *windows; /* List of the WnckTask for the window,
		     if this is a class group */
  gulong state_changed_tag;
  gulong icon_changed_tag;
  gulong name_changed_tag;
  gulong class_name_changed_tag;
  gulong class_icon_changed_tag;
  
  /* task menu */
  GtkWidget *menu;
  /* ops menu */
  GtkWidget *action_menu;

  guint really_toggling : 1; /* Set when tasklist really wants
                              * to change the togglebutton state
                              */
  guint was_active : 1;      /* used to fixup activation behavior */ 

  guint button_activate;
};

struct _WnckTaskClass
{
  GObjectClass parent_class;
};

struct _WnckTasklistPrivate
{
  WnckScreen *screen;

  WnckTask *active_task; /* NULL if active window not in tasklist */
  WnckTask *active_class_group; /* NULL if active window not in tasklist */
  
  gboolean include_all_workspaces;
  
  /* Calculated by update_lists */
  GList *class_groups;
  GList *windows;

  /* Not handled by update_lists */
  GList *startup_sequences;
  
  GHashTable *class_group_hash;
  GHashTable *win_hash;
  
  GtkTooltips *tooltips;

  gint max_button_width;
  gint max_button_height;

  gboolean switch_workspace_on_unminimize;
  
  WnckTasklistGroupingType grouping;
  gint grouping_limit;

  guint activate_timeout_id;
  guint screen_connections [N_SCREEN_CONNECTIONS];

  guint idle_callback_tag;

  int *size_hints;
  int size_hints_len;

  gint minimum_width;
  gint minimum_height;

  WnckLoadIconFunction icon_loader;
  void *icon_loader_data;
  GDestroyNotify free_icon_loader_data;
  
#ifdef HAVE_STARTUP_NOTIFICATION
  SnMonitorContext *sn_context;
  guint startup_sequence_timeout;
#endif

  gint monitor_num;
  GdkRectangle monitor_geometry;
};


static void wnck_task_init        (WnckTask      *task);
static void wnck_task_class_init  (WnckTaskClass *klass);
static void wnck_task_finalize    (GObject       *object);

static WnckTask *wnck_task_new_from_window      (WnckTasklist    *tasklist,
						 WnckWindow      *window);
static WnckTask *wnck_task_new_from_class_group (WnckTasklist    *tasklist,
						 WnckClassGroup  *class_group);
#ifdef HAVE_STARTUP_NOTIFICATION
static WnckTask *wnck_task_new_from_startup_sequence (WnckTasklist      *tasklist,
                                                      SnStartupSequence *sequence);
#endif

static char      *wnck_task_get_text (WnckTask *task);
static GdkPixbuf *wnck_task_get_icon (WnckTask *task);
static gint       wnck_task_compare  (gconstpointer  a,
				      gconstpointer  b);
static void       wnck_task_update_visible_state (WnckTask *task);


static void wnck_tasklist_init        (WnckTasklist      *tasklist);
static void wnck_tasklist_class_init  (WnckTasklistClass *klass);
static void wnck_tasklist_finalize    (GObject        *object);

static void     wnck_tasklist_size_request  (GtkWidget        *widget,
                                             GtkRequisition   *requisition);
static void     wnck_tasklist_size_allocate (GtkWidget        *widget,
                                             GtkAllocation    *allocation);
static void     wnck_tasklist_realize       (GtkWidget        *widget);
static void     wnck_tasklist_unrealize     (GtkWidget        *widget);
static void     wnck_tasklist_forall        (GtkContainer     *container,
                                             gboolean	       include_internals,
                                             GtkCallback       callback,
                                             gpointer          callback_data);
static void     wnck_tasklist_remove	    (GtkContainer   *container,
					     GtkWidget	    *widget);
static void     wnck_tasklist_free_tasks    (WnckTasklist   *tasklist);
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
static void     wnck_tasklist_window_added             (WnckScreen   *screen,
							WnckWindow   *win,
							WnckTasklist *tasklist);
static void     wnck_tasklist_window_removed           (WnckScreen   *screen,
							WnckWindow   *win,
							WnckTasklist *tasklist);
static void     wnck_tasklist_viewports_changed        (WnckScreen   *screen,
							WnckTasklist *tasklist);
static void     wnck_tasklist_connect_window           (WnckTasklist *tasklist,
							WnckWindow   *window);

static void     wnck_tasklist_change_active_task       (WnckTasklist *tasklist,
							WnckTask *active_task);
static gboolean wnck_tasklist_change_active_timeout    (gpointer data);
static void     wnck_tasklist_activate_task_window     (WnckTask *task);

static void     wnck_tasklist_update_icon_geometries   (WnckTasklist *tasklist,
							GList        *visible_tasks);
static void     wnck_tasklist_connect_screen           (WnckTasklist *tasklist,
                                                        WnckScreen   *screen);
static void     wnck_tasklist_disconnect_screen        (WnckTasklist *tasklist);

#ifdef HAVE_STARTUP_NOTIFICATION
static void     wnck_tasklist_sn_event                 (SnMonitorEvent *event,
                                                        void           *user_data);
static void     wnck_tasklist_check_end_sequence       (WnckTasklist   *tasklist,
                                                        WnckWindow     *window);
#endif


static gpointer task_parent_class;
static gpointer tasklist_parent_class;

/*
 * Keep track of all tasklist instances so we can decide
 * whether to show windows from all monitors in the
 * tasklist
 */
static GSList *tasklist_instances;

/**
 * eel_gtk_label_make_bold.
 *
 * Switches the font of label to a bold equivalent.
 * @label: The label.
 **/
void
eel_gtk_label_make_bold (GtkLabel *label)
{
  PangoFontDescription *font_desc;

  font_desc = pango_font_description_new ();

  pango_font_description_set_weight (font_desc,
                                     PANGO_WEIGHT_BOLD);

  /* This will only affect the weight of the font, the rest is
   * from the current state of the widget, which comes from the
   * theme or user prefs, since the font desc only has the
   * weight flag turned on.
   */
  gtk_widget_modify_font (GTK_WIDGET (label), font_desc);

  pango_font_description_free (font_desc);
}

void
wnck_gtk_label_make_normal (GtkLabel *label)
{
  PangoFontDescription *font_desc;

  font_desc = pango_font_description_new ();

  pango_font_description_set_weight (font_desc,
                                     PANGO_WEIGHT_NORMAL);

  /* This will only affect the weight of the font, the rest is
   * from the current state of the widget, which comes from the
   * theme or user prefs, since the font desc only has the
   * weight flag turned on.
   */
  gtk_widget_modify_font (GTK_WIDGET (label), font_desc);

  pango_font_description_free (font_desc);
}

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

  gtk_rc_parse_string ("\n"
    "   style \"tasklist-button-style\"\n"
    "   {\n"
    "      GtkWidget::focus-line-width=0\n"
    "      GtkWidget::focus-padding=0\n"
    "   }\n"
    "\n"
    "    widget \"*.tasklist-button\" style \"tasklist-button-style\"\n"
    "\n");
}

static void
wnck_task_finalize (GObject *object)
{
  WnckTask *task;

  task = WNCK_TASK (object);

  if (task->tasklist->priv->active_task == task)
    wnck_tasklist_change_active_task (task->tasklist, NULL);

  if (task->button)
    {
      gtk_widget_destroy (task->button);
      task->button = NULL;
    }

  g_list_free (task->windows);
  task->windows = NULL;

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

  if (task->class_name_changed_tag)
    {
      g_signal_handler_disconnect (task->class_group,
				   task->class_name_changed_tag);
      task->class_name_changed_tag = 0;
    }

  if (task->class_icon_changed_tag)
    {
      g_signal_handler_disconnect (task->class_group,
				   task->class_icon_changed_tag);
      task->class_icon_changed_tag = 0;
    }

  if (task->menu)
    {
      gtk_widget_destroy (task->menu);
      task->menu = NULL;
    }

  if (task->action_menu)
    {
      g_object_unref (task->action_menu);
      task->action_menu = NULL;
    }

  if (task->window)
    {
      g_object_unref (task->window);
      task->window = NULL;
    }

  if (task->class_group)
    {
      g_object_unref (task->class_group);
      task->class_group = NULL;
    }

#ifdef HAVE_STARTUP_NOTIFICATION
  if (task->startup_sequence)
    {
      sn_startup_sequence_unref (task->startup_sequence);
      task->startup_sequence = NULL;
    }
#endif
  
  if (task->button_activate != 0)
    {
      g_source_remove (task->button_activate);
      task->button_activate = 0;
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
  GtkWidget *widget;
  AtkObject *atk_obj;

  widget = GTK_WIDGET (tasklist);

  GTK_WIDGET_SET_FLAGS (widget, GTK_NO_WINDOW);
  
  tasklist->priv = g_new0 (WnckTasklistPrivate, 1);

  tasklist->priv->include_all_workspaces = FALSE;
  
  tasklist->priv->win_hash = g_hash_table_new (NULL, NULL);
  tasklist->priv->class_group_hash = g_hash_table_new (NULL, NULL);
  
  tasklist->priv->grouping = WNCK_TASKLIST_AUTO_GROUP;
  tasklist->priv->grouping_limit = DEFAULT_GROUPING_LIMIT;

  tasklist->priv->minimum_width = DEFAULT_WIDTH;
  tasklist->priv->minimum_height = DEFAULT_HEIGHT;

  tasklist->priv->idle_callback_tag = 0;

  tasklist->priv->monitor_num = -1;

  atk_obj = gtk_widget_get_accessible (widget);
  atk_object_set_name (atk_obj, _("Window List"));
  atk_object_set_description (atk_obj, _("Tool to switch between visible windows"));

  tasklist_instances = g_slist_append (tasklist_instances, tasklist);
  g_slist_foreach (tasklist_instances,
		   (GFunc) wnck_tasklist_update_lists,
		   NULL);
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
  widget_class->realize = wnck_tasklist_realize;
  widget_class->unrealize = wnck_tasklist_unrealize;
  
  container_class->forall = wnck_tasklist_forall;
  container_class->remove = wnck_tasklist_remove;
}

static void
wnck_tasklist_finalize (GObject *object)
{
  WnckTasklist *tasklist;

  tasklist = WNCK_TASKLIST (object);
  
  tasklist_instances = g_slist_remove (tasklist_instances, tasklist);
  g_slist_foreach (tasklist_instances,
		   (GFunc) wnck_tasklist_update_lists,
		   NULL);

  if (tasklist->priv->free_icon_loader_data != NULL)
    (* tasklist->priv->free_icon_loader_data) (tasklist->priv->icon_loader_data);
  
  wnck_tasklist_disconnect_screen (tasklist);

  /* Tasks should have gone away due to removing their
   * buttons in container destruction
   */
  g_assert (tasklist->priv->windows == NULL);
  g_assert (tasklist->priv->class_groups == NULL);
  g_assert (tasklist->priv->startup_sequences == NULL);
  /* wnck_tasklist_free_tasks (tasklist); */
  
  g_hash_table_destroy (tasklist->priv->win_hash);
  tasklist->priv->win_hash = NULL;
  
  g_hash_table_destroy (tasklist->priv->class_group_hash);
  tasklist->priv->class_group_hash = NULL;
  
  if (tasklist->priv->activate_timeout_id != 0)
    gtk_timeout_remove (tasklist->priv->activate_timeout_id);
  
  if (tasklist->priv->idle_callback_tag != 0)
    g_source_remove (tasklist->priv->idle_callback_tag);
    
  if (tasklist->priv->tooltips)
    {
      g_object_unref (tasklist->priv->tooltips);
      tasklist->priv->tooltips = NULL;
    }

  g_free (tasklist->priv->size_hints);
  tasklist->priv->size_hints = NULL;
  tasklist->priv->size_hints_len = 0;

  g_free (tasklist->priv);
  tasklist->priv = NULL;  
  
  G_OBJECT_CLASS (tasklist_parent_class)->finalize (object);
}

void
wnck_tasklist_set_grouping (WnckTasklist            *tasklist,
			    WnckTasklistGroupingType grouping)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

  if (tasklist->priv->grouping == grouping)
    return;
  
  tasklist->priv->grouping = grouping;
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

void
wnck_tasklist_set_switch_workspace_on_unminimize (WnckTasklist  *tasklist,
						  gboolean       switch_workspace_on_unminimize)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

  tasklist->priv->switch_workspace_on_unminimize = switch_workspace_on_unminimize;
}

void
wnck_tasklist_set_include_all_workspaces (WnckTasklist *tasklist,
					  gboolean      include_all_workspaces)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

  include_all_workspaces = (include_all_workspaces != 0);

  if (tasklist->priv->include_all_workspaces == include_all_workspaces)
    return;
  
  tasklist->priv->include_all_workspaces = include_all_workspaces;
  wnck_tasklist_update_lists (tasklist);
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

void
wnck_tasklist_set_grouping_limit (WnckTasklist *tasklist,
				  gint          limit)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

  if (tasklist->priv->grouping_limit == limit)
    return;

  tasklist->priv->grouping_limit = limit;
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

/* set the minimum width 
 * use -1 to unset it (resulting in the default width */
void 
wnck_tasklist_set_minimum_width (WnckTasklist *tasklist, gint size)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

  if (size == -1) size = DEFAULT_WIDTH;

  if (tasklist->priv->minimum_width == size)
    return;

  tasklist->priv->minimum_width = size;
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}
 
/* get the minimum width */
gint
wnck_tasklist_get_minimum_width (WnckTasklist *tasklist)
{
  g_return_val_if_fail (WNCK_IS_TASKLIST (tasklist), 0);
	
  return tasklist->priv->minimum_width;
}

/* set the minimum height
 * use -1 to unset it (resulting in the default height */
void 
wnck_tasklist_set_minimum_height (WnckTasklist *tasklist, gint size)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

  if (size == -1) size = DEFAULT_HEIGHT;

  if (tasklist->priv->minimum_height == size)
    return;

  tasklist->priv->minimum_height = size;
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}
 
/* get the minimum height */
gint
wnck_tasklist_get_minimum_height (WnckTasklist *tasklist)
{
  g_return_val_if_fail (WNCK_IS_TASKLIST (tasklist), 0);
	
  return tasklist->priv->minimum_height;
}

/**
 * wnck_tasklist_set_icon_loader:
 * @tasklist: a #WnckTasklist
 * @load_icon_func: icon loader function
 * @data: data for icon loader function
 * @free_data_func: function to free the data
 *
 * Sets a function to be used for loading icons. The icon
 * loader function takes an icon name as in the Icon field
 * in a .desktop file. The "flags" field for the function
 * is not defined to do anything yet.
 * 
 **/
void
wnck_tasklist_set_icon_loader (WnckTasklist         *tasklist,
                               WnckLoadIconFunction  load_icon_func,
                               void                 *data,
                               GDestroyNotify        free_data_func)
{
  if (tasklist->priv->free_icon_loader_data != NULL)
    (* tasklist->priv->free_icon_loader_data) (tasklist->priv->icon_loader_data);

  tasklist->priv->icon_loader = load_icon_func;
  tasklist->priv->icon_loader_data = data;
  tasklist->priv->free_icon_loader_data = free_data_func;  
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
			    GList        *ungrouped_class_groups)
{
  WnckTask *class_group_task;
  WnckTask *win_task;
  GList *l, *w;
  const char *first_name = NULL;
  int n_windows;
  int n_same_title;
  double same_window_ratio;

  l = ungrouped_class_groups;
  while (l != NULL)
    {
      class_group_task = WNCK_TASK (l->data);

      n_windows = g_list_length (class_group_task->windows);

      n_same_title = 0;
      w = class_group_task->windows;
      while (w != NULL)
	{
	  win_task = WNCK_TASK (w->data);

	  if (first_name == NULL)
	    {
	      first_name = wnck_window_get_icon_name (win_task->window);
	      n_same_title++;
	    }
	  else
	    {
	      if (strcmp (wnck_window_get_icon_name (win_task->window),
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
      class_group_task->grouping_score = -same_window_ratio * 5 + n_windows;

      l = l->next;
    }
}

static GList *
wnck_task_get_highest_scored (GList     *ungrouped_class_groups,
			      WnckTask **class_group_task_out)
{
  WnckTask *class_group_task;
  WnckTask *best_task = NULL;
  double max_score = -1000000000.0; /* Large negative score */
  GList *l;
 
  l = ungrouped_class_groups;
  while (l != NULL)
    {
      class_group_task = WNCK_TASK (l->data);

      if (class_group_task->grouping_score >= max_score)
	{
	  max_score = class_group_task->grouping_score;
	  best_task = class_group_task;
	}
      
      l = l->next;
    }

  *class_group_task_out = best_task;

  return g_list_remove (ungrouped_class_groups, best_task);
}

static void
wnck_tasklist_size_request  (GtkWidget      *widget,
                             GtkRequisition *requisition)
{
  WnckTasklist *tasklist;
  GtkRequisition child_req;
  GtkAllocation  fake_allocation;
  int max_height = 1;
  int max_width = 1;
  /* int u_width, u_height; */
  GList *l;
  GArray *array;
  GList *ungrouped_class_groups;
  int n_windows;
  int n_startup_sequences;
  int n_rows;
  int n_cols, last_n_cols;
  int n_grouped_buttons;
  gboolean score_set;
  int val;
  WnckTask *class_group_task;
  int lowest_range;
  int grouping_limit;
  
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

  l = tasklist->priv->class_groups;
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

  l = tasklist->priv->startup_sequences;
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


  /* this snippet of code seems to want to allocate at least
   * four buttons' worth of height for the widget.  I think this isn't
   * necessary any more now that we have the minimum_size option, so
   * I'm commenting this out - thomasvs */
  /*
  gtk_widget_get_size_request (widget, &u_width, &u_height);

  if (u_height != -1)
    {
      requisition->height = u_height;
    }
  else if (u_width != -1)
    {
      requisition->width = u_width;
      requisition->height = 4 * max_height;
    }
  
  requisition->width = MAX(requisition->width, tasklist->priv->minimum_width);
  requisition->height = MAX(requisition->height, tasklist->priv->minimum_height);
  */
  requisition->width = tasklist->priv->minimum_width;
  requisition->height = tasklist->priv->minimum_height;
  
  fake_allocation.width = requisition->width;
  fake_allocation.height = requisition->height;

  array = g_array_new (FALSE, FALSE, sizeof (int));

  /* Calculate size_hints list */
  
  n_windows = g_list_length (tasklist->priv->windows);
  n_startup_sequences = g_list_length (tasklist->priv->startup_sequences);
  n_grouped_buttons = 0;
  ungrouped_class_groups = g_list_copy (tasklist->priv->class_groups);
  score_set = FALSE;

  grouping_limit = MIN (tasklist->priv->grouping_limit,
			tasklist->priv->max_button_width);
  
  /* Try ungrouped mode */
  wnck_tasklist_layout (&fake_allocation,
			tasklist->priv->max_button_width,
			tasklist->priv->max_button_height,
			n_windows + n_startup_sequences,
			&n_cols, &n_rows);

  last_n_cols = G_MAXINT;
  lowest_range = G_MAXINT;
  if (tasklist->priv->grouping != WNCK_TASKLIST_ALWAYS_GROUP)
    {
      val = n_cols * tasklist->priv->max_button_width;
      g_array_insert_val (array, array->len, val);
      val = n_cols * grouping_limit;
      g_array_insert_val (array, array->len, val);

      last_n_cols = n_cols;
      lowest_range = val;
    }

  while (ungrouped_class_groups != NULL &&
	 tasklist->priv->grouping != WNCK_TASKLIST_NEVER_GROUP)
    {
      if (!score_set)
	{
	  wnck_tasklist_score_groups (tasklist, ungrouped_class_groups);
	  score_set = TRUE;
	}

      ungrouped_class_groups = wnck_task_get_highest_scored (ungrouped_class_groups, &class_group_task);

      n_grouped_buttons += g_list_length (class_group_task->windows) - 1;

      wnck_tasklist_layout (&fake_allocation,
			    tasklist->priv->max_button_width,
			    tasklist->priv->max_button_height,
			    n_startup_sequences + n_windows - n_grouped_buttons,
			    &n_cols, &n_rows);
      if (n_cols != last_n_cols &&
	  (tasklist->priv->grouping == WNCK_TASKLIST_AUTO_GROUP ||
	   ungrouped_class_groups == NULL))
	{
	  val = n_cols * tasklist->priv->max_button_width;
	  if (val >= lowest_range)
	    { /* Overlaps old range */
	      lowest_range = n_cols * grouping_limit;
	      if (array->len > 0)
		g_array_index(array, int, array->len-1) = lowest_range;
	      else
		g_array_insert_val (array, 0, lowest_range);
	    }
	  else
	    {
	      /* Full new range */
	      g_array_insert_val (array, array->len, val);
	      val = n_cols * grouping_limit;
	      g_array_insert_val (array, array->len, val);
	      lowest_range = val;
	    }

	  last_n_cols = n_cols;
	}
    }

  /* Always let you go down to a zero size: */
  if (array->len > 0)
    g_array_index(array, int, array->len-1) = 0;
  else
    {
      val = 0;
      g_array_insert_val (array, 0, val);
    }
  
  if (tasklist->priv->size_hints)
    g_free (tasklist->priv->size_hints);

  tasklist->priv->size_hints_len = array->len;
    
  tasklist->priv->size_hints = (int *)g_array_free (array, FALSE);
}

const int *
wnck_tasklist_get_size_hint_list (WnckTasklist  *tasklist,
				  int           *n_elements)
{
  *n_elements = tasklist->priv->size_hints_len;
  return tasklist->priv->size_hints;
}
  
static void
wnck_tasklist_size_allocate (GtkWidget      *widget,
                             GtkAllocation  *allocation)
{
  GtkAllocation child_allocation;
  WnckTasklist *tasklist;
  WnckTask *class_group_task;
  int n_windows;
  int n_startup_sequences;
  GList *l;
  int button_width;
  int total_width;
  int n_rows;
  int n_cols;
  int n_grouped_buttons;
  int i;
  gboolean score_set;
  GList *ungrouped_class_groups;
  WnckTask *win_task;
  GList *visible_tasks = NULL;
  int grouping_limit;
  
  tasklist = WNCK_TASKLIST (widget);

  n_windows = g_list_length (tasklist->priv->windows);
  n_startup_sequences = g_list_length (tasklist->priv->startup_sequences);
  n_grouped_buttons = 0;
  ungrouped_class_groups = g_list_copy (tasklist->priv->class_groups);
  score_set = FALSE;

  grouping_limit = MIN (tasklist->priv->grouping_limit,
			tasklist->priv->max_button_width);

  /* Try ungrouped mode */
  button_width = wnck_tasklist_layout (allocation,
				       tasklist->priv->max_button_width,
				       tasklist->priv->max_button_height,
				       n_startup_sequences + n_windows,
				       &n_cols, &n_rows);
  while (ungrouped_class_groups != NULL &&
	 ((tasklist->priv->grouping == WNCK_TASKLIST_ALWAYS_GROUP) ||
	  ((tasklist->priv->grouping == WNCK_TASKLIST_AUTO_GROUP) &&
	   (button_width < grouping_limit))))
    {
      if (!score_set)
	{
	  wnck_tasklist_score_groups (tasklist, ungrouped_class_groups);
	  score_set = TRUE;
	}

      ungrouped_class_groups = wnck_task_get_highest_scored (ungrouped_class_groups, &class_group_task);

      n_grouped_buttons += g_list_length (class_group_task->windows) - 1;

      if (g_list_length (class_group_task->windows) > 1)
	{
	  visible_tasks = g_list_prepend (visible_tasks, class_group_task);
	  
	  /* Hide all this group's windows */
	  l = class_group_task->windows;
	  while (l != NULL)
	    {
	      win_task = WNCK_TASK (l->data);
	      
	      gtk_widget_set_child_visible (GTK_WIDGET (win_task->button), FALSE);
	      l = l->next;
	    }
	}
      else
	{
	  visible_tasks = g_list_prepend (visible_tasks, class_group_task->windows->data);
	  gtk_widget_set_child_visible (GTK_WIDGET (class_group_task->button), FALSE);
	}
      
      button_width = wnck_tasklist_layout (allocation,
					   tasklist->priv->max_button_width,
					   tasklist->priv->max_button_height,
					   n_startup_sequences + n_windows - n_grouped_buttons,
					   &n_cols, &n_rows);
    }

  /* Add all ungrouped windows to visible_tasks, and hide their class groups */
  l = ungrouped_class_groups;
  while (l != NULL)
    {
      class_group_task = WNCK_TASK (l->data);
      
      visible_tasks = g_list_concat (visible_tasks, g_list_copy (class_group_task->windows));
      gtk_widget_set_child_visible (GTK_WIDGET (class_group_task->button), FALSE);
      l = l->next;
    }

  /* Add all startup sequences */
  visible_tasks = g_list_concat (visible_tasks, g_list_copy (tasklist->priv->startup_sequences));

  /* Sort */
  visible_tasks = g_list_sort (visible_tasks, wnck_task_compare);

  /* Allocate children */
  l = visible_tasks;
  i = 0;
  total_width = tasklist->priv->max_button_width * n_cols;
  total_width = MIN (total_width, allocation->width);
  total_width = allocation->width;
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

  /* Update icon geometries. */
  wnck_tasklist_update_icon_geometries (tasklist, visible_tasks);
  
  g_list_free (visible_tasks);
  
  GTK_WIDGET_CLASS (tasklist_parent_class)->size_allocate (widget, allocation);
}

static void
wnck_tasklist_realize (GtkWidget *widget)
{
  WnckTasklist *tasklist;

  tasklist = WNCK_TASKLIST (widget);  

#ifdef HAVE_STARTUP_NOTIFICATION
  tasklist->priv->sn_context =
    sn_monitor_context_new (_wnck_screen_get_sn_display (tasklist->priv->screen),
                            _wnck_screen_get_number (tasklist->priv->screen),
                            wnck_tasklist_sn_event,
                            tasklist,
                            NULL);
#endif
  
  (* GTK_WIDGET_CLASS (tasklist_parent_class)->realize) (widget);

  wnck_tasklist_update_lists (tasklist);
}

static void
wnck_tasklist_unrealize (GtkWidget *widget)
{
  WnckTasklist *tasklist;

  tasklist = WNCK_TASKLIST (widget);

#ifdef HAVE_STARTUP_NOTIFICATION
  sn_monitor_context_unref (tasklist->priv->sn_context);
  tasklist->priv->sn_context = NULL;
#endif
  
  (* GTK_WIDGET_CLASS (tasklist_parent_class)->unrealize) (widget);
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
  
  tmp = tasklist->priv->class_groups;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;
      
      (* callback) (task->button, callback_data);
    }

  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;
      
      (* callback) (task->button, callback_data);
    }
}

static void
wnck_tasklist_remove (GtkContainer   *container,
		      GtkWidget	     *widget)
{
  WnckTasklist *tasklist;
  GList *tmp;
  
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

          gtk_widget_unparent (widget);
          g_object_unref (task);
	  break;
	}
    }

  tmp = tasklist->priv->class_groups;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;
      
      if (task->button == widget)
	{
	  g_hash_table_remove (tasklist->priv->class_group_hash,
			       task->class_group);
	  tasklist->priv->class_groups =
	    g_list_remove (tasklist->priv->class_groups,
			   task);

          gtk_widget_unparent (widget);
          g_object_unref (task);
	  break;
	}
    }

  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      tmp = tmp->next;
      
      if (task->button == widget)
	{
	  tasklist->priv->startup_sequences =
	    g_list_remove (tasklist->priv->startup_sequences,
			   task);

          gtk_widget_unparent (widget);
          g_object_unref (task);
	  break;
	}
    }

  gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
wnck_tasklist_connect_screen (WnckTasklist *tasklist,
			      WnckScreen   *screen)
{
  GList *windows;
  guint *c;
  int    i;

  i = 0;
  c = tasklist->priv->screen_connections;

  c [i++] = g_signal_connect_object (G_OBJECT (screen), "active_window_changed",
                                     G_CALLBACK (wnck_tasklist_active_window_changed),
                                     tasklist, 0);
  c [i++] = g_signal_connect_object (G_OBJECT (screen), "active_workspace_changed",
                                     G_CALLBACK (wnck_tasklist_active_workspace_changed),
                                     tasklist, 0);
  c [i++] = g_signal_connect_object (G_OBJECT (screen), "window_opened",
                                     G_CALLBACK (wnck_tasklist_window_added),
                                     tasklist, 0);
  c [i++] = g_signal_connect_object (G_OBJECT (screen), "window_closed",
                                     G_CALLBACK (wnck_tasklist_window_removed),
                                     tasklist, 0);
  c [i++] = g_signal_connect_object (G_OBJECT (screen), "viewports_changed",
                                     G_CALLBACK (wnck_tasklist_viewports_changed),
                                     tasklist, 0);


  g_assert (i == N_SCREEN_CONNECTIONS);

  windows = wnck_screen_get_windows (screen);
  while (windows != NULL)
    {
      wnck_tasklist_connect_window (tasklist, windows->data);
      windows = windows->next;
    }
}

static void
wnck_tasklist_disconnect_screen (WnckTasklist *tasklist)
{
  int i;

  i = 0;
  while (i < N_SCREEN_CONNECTIONS)
    {
      if (tasklist->priv->screen_connections [i] != 0)
        g_signal_handler_disconnect (G_OBJECT (tasklist->priv->screen),
                                     tasklist->priv->screen_connections [i]);

      tasklist->priv->screen_connections [i] = 0;

      ++i;
    }

#ifdef HAVE_STARTUP_NOTIFICATION
  if (tasklist->priv->startup_sequence_timeout != 0)
    {
      g_source_remove (tasklist->priv->startup_sequence_timeout);
      tasklist->priv->startup_sequence_timeout = 0;
    }
#endif
}

void
wnck_tasklist_set_screen (WnckTasklist *tasklist,
			  WnckScreen   *screen)
{
  if (tasklist->priv->screen == screen)
    return;

  if (tasklist->priv->screen)
    wnck_tasklist_disconnect_screen (tasklist);

  tasklist->priv->screen = screen;

  wnck_tasklist_update_lists (tasklist);

  wnck_tasklist_connect_screen (tasklist, screen);
}

GtkWidget*
wnck_tasklist_new (WnckScreen *screen)
{
  WnckTasklist *tasklist;

  tasklist = g_object_new (WNCK_TYPE_TASKLIST, NULL);

  tasklist->priv->tooltips = gtk_tooltips_new ();
  gtk_object_ref (GTK_OBJECT (tasklist->priv->tooltips));
  gtk_object_sink (GTK_OBJECT (tasklist->priv->tooltips));

  wnck_tasklist_set_screen (tasklist, screen);

  return GTK_WIDGET (tasklist);
}

static void
wnck_tasklist_free_tasks (WnckTasklist *tasklist)
{
  GList *l;
  
  tasklist->priv->active_task = NULL;
  tasklist->priv->active_class_group = NULL;
  
  if (tasklist->priv->windows)
    {
      l = tasklist->priv->windows;
      while (l != NULL)
	{
	  WnckTask *task = WNCK_TASK (l->data);
	  l = l->next;
          /* if we just unref the task it means we lose our ref to the
           * task before we unparent the button, which breaks stuff.
           */
	  gtk_widget_destroy (task->button);
	}
    }
  g_assert (tasklist->priv->windows == NULL);
  g_assert (g_hash_table_size (tasklist->priv->win_hash) == 0);

  if (tasklist->priv->class_groups)
    {
      l = tasklist->priv->class_groups;
      while (l != NULL)
	{
	  WnckTask *task = WNCK_TASK (l->data);
	  l = l->next;
          /* if we just unref the task it means we lose our ref to the
           * task before we unparent the button, which breaks stuff.
           */
	  gtk_widget_destroy (task->button);
	}
    }
  g_assert (tasklist->priv->class_groups == NULL);
  g_assert (g_hash_table_size (tasklist->priv->class_group_hash) == 0);
}


/*
 * This function determines if a window should be included in the tasklist.
 */
static gboolean
wnck_tasklist_include_window (WnckTasklist *tasklist, WnckWindow *win)
{
  WnckWorkspace *active_workspace;
  int x, y, w, h;

  if (wnck_window_get_state (win) & WNCK_WINDOW_STATE_SKIP_TASKLIST)
    return FALSE;

  if (tasklist->priv->monitor_num != -1)
    {
      wnck_window_get_geometry (win, &x, &y, &w, &h);
      /* Don't include the window if its center point is not on the same monitor */
      if (gdk_screen_get_monitor_at_point (_wnck_screen_get_gdk_screen (tasklist->priv->screen),
                                           x + w / 2, y + h / 2) != tasklist->priv->monitor_num)
        return FALSE;
    }

  if (tasklist->priv->include_all_workspaces)
    return TRUE;

  if (wnck_window_is_pinned (win))
    return TRUE;

  active_workspace = wnck_screen_get_active_workspace (tasklist->priv->screen);  
  if (active_workspace == NULL)
    return TRUE;

  if (active_workspace != wnck_window_get_workspace (win))
    return FALSE;

  if (!wnck_workspace_is_virtual (active_workspace))
    return TRUE;

  return wnck_window_is_in_viewport (win, active_workspace);
}

static void
wnck_tasklist_update_lists (WnckTasklist *tasklist)
{
  GList *windows;
  WnckWindow *win;
  WnckClassGroup *class_group;
  GList *l;
  WnckTask *win_task;
  WnckTask *class_group_task;

  wnck_tasklist_free_tasks (tasklist);
  
  if (GTK_WIDGET (tasklist)->window != NULL)
    {
      /*
       * only show windows from this monitor if there is more than one tasklist running
       */
      if (tasklist_instances == NULL || tasklist_instances->next == NULL)
	{
	  tasklist->priv->monitor_num = -1;
        }
      else
	{
	  int monitor_num;

	  monitor_num = gdk_screen_get_monitor_at_window (_wnck_screen_get_gdk_screen (tasklist->priv->screen),
							  GTK_WIDGET (tasklist)->window);

	  if (monitor_num != tasklist->priv->monitor_num)
	    {
	      tasklist->priv->monitor_num = monitor_num;
	      gdk_screen_get_monitor_geometry (_wnck_screen_get_gdk_screen (tasklist->priv->screen),
					       tasklist->priv->monitor_num,
					       &tasklist->priv->monitor_geometry);
	    }
	}
    }
 
  l = windows = wnck_screen_get_windows (tasklist->priv->screen);
  while (l != NULL)
    {
      win = WNCK_WINDOW (l->data);

      if (wnck_tasklist_include_window (tasklist, win))
	{
	  win_task = wnck_task_new_from_window (tasklist, win);
	  tasklist->priv->windows = g_list_prepend (tasklist->priv->windows, win_task);
	  g_hash_table_insert (tasklist->priv->win_hash, win, win_task);
	  
	  gtk_widget_set_parent (win_task->button, GTK_WIDGET (tasklist));
	  gtk_widget_show (win_task->button);
	  
	  /* Class group */

	  class_group = wnck_window_get_class_group (win);
	  class_group_task = g_hash_table_lookup (tasklist->priv->class_group_hash, class_group);

	  if (class_group_task == NULL)
	    {
	      class_group_task = wnck_task_new_from_class_group (tasklist, class_group);
	      gtk_widget_set_parent (class_group_task->button, GTK_WIDGET (tasklist));
	      gtk_widget_show (class_group_task->button);

	      tasklist->priv->class_groups = g_list_prepend (tasklist->priv->class_groups,
							     class_group_task);
	      g_hash_table_insert (tasklist->priv->class_group_hash, class_group, class_group_task);
	    }
	  
	  class_group_task->windows = g_list_prepend (class_group_task->windows, win_task);
	}
      
      l = l->next;
    }

  /* Sort the class group list */
  l = tasklist->priv->class_groups;
  while (l)
    {
      class_group_task = WNCK_TASK (l->data);

      class_group_task->windows = g_list_sort (class_group_task->windows, wnck_task_compare);

      /* so the number of windows in the task gets reset on the
       * task label
       */
      wnck_task_update_visible_state (class_group_task);
      
      l = l->next;
    }

  
  /* since we cleared active_window we need to reset it */
  wnck_tasklist_active_window_changed (tasklist->priv->screen, tasklist);

  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

static void
wnck_tasklist_change_active_task (WnckTasklist *tasklist, WnckTask *active_task)
{
  if (active_task &&
      active_task == tasklist->priv->active_task)
    return;

  g_assert (active_task == NULL ||
            active_task->type != WNCK_TASK_STARTUP_SEQUENCE);
  
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
      active_task = g_hash_table_lookup (tasklist->priv->class_group_hash,
					 active_task->class_group);

      if (active_task &&
	  active_task == tasklist->priv->active_class_group)
	return;

      if (tasklist->priv->active_class_group)
	{
	  tasklist->priv->active_class_group->really_toggling = TRUE;
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tasklist->priv->active_class_group->button),
					FALSE);
	  tasklist->priv->active_class_group->really_toggling = FALSE;
	}
  
      tasklist->priv->active_class_group = active_task;
  
      if (tasklist->priv->active_class_group)
	{
	  tasklist->priv->active_class_group->really_toggling = TRUE;
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tasklist->priv->active_class_group->button),
					TRUE);
	  tasklist->priv->active_class_group->really_toggling = FALSE;
	}
    }
}

static void
wnck_tasklist_update_icon_geometries (WnckTasklist *tasklist,
				      GList        *visible_tasks)
{
	gint x, y, width, height;
	GList *l1;
	
	for (l1 = visible_tasks; l1; l1 = l1->next) {
		WnckTask *task = WNCK_TASK (l1->data);
		
		if (!GTK_WIDGET_REALIZED (task->button))
			continue;

		gdk_window_get_origin (GTK_BUTTON (task->button)->event_window,
				       &x, &y);
		width = task->button->allocation.width;
		height = task->button->allocation.height;

		if (task->window)
			wnck_window_set_icon_geometry (task->window,
						       x, y, width, height);
		else {
			GList *l2;

			for (l2 = task->windows; l2; l2 = l2->next) {
				WnckTask *win_task = WNCK_TASK (l2->data);

				g_assert (win_task->window);

				wnck_window_set_icon_geometry (win_task->window,
							       x, y, width, height);
			}
		}
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
wnck_tasklist_window_changed_workspace (WnckWindow   *window,
					WnckTasklist *tasklist)
{
  WnckWorkspace *active_ws;
  WnckWorkspace *window_ws;
  gboolean       need_update;
  GList         *l;

  active_ws = wnck_screen_get_active_workspace (tasklist->priv->screen);
  window_ws = wnck_window_get_workspace (window);

  if (!window_ws)
    return;

  need_update = FALSE;

  if (active_ws == window_ws)
    need_update = TRUE;

  l = tasklist->priv->windows;
  while (!need_update && l != NULL)
    {
      WnckTask *task = l->data;

      if (task->type == WNCK_TASK_WINDOW &&
          task->window == window)
        need_update = TRUE;

      l = l->next;
    }

  if (need_update)
    {
      wnck_tasklist_update_lists (tasklist);
      gtk_widget_queue_resize (GTK_WIDGET (tasklist));
    }
}

static gboolean
do_wnck_tasklist_update_lists (gpointer data)
{
  WnckTasklist *tasklist = WNCK_TASKLIST (data);

  tasklist->priv->idle_callback_tag = 0;

  wnck_tasklist_update_lists (tasklist);

  return FALSE;
}

static void
wnck_tasklist_window_changed_geometry (WnckWindow   *window,
				       WnckTasklist *tasklist)
{
  WnckTask *win_task;
  gboolean show;
  gboolean monitor_changed;
  int x, y, w, h;

  if (tasklist->priv->idle_callback_tag != 0)
    return;

  /*
   * If the (parent of the) tasklist itself skips
   * the tasklist, we need an extra check whether
   * the tasklist itself possibly changed monitor.
   */
  monitor_changed = FALSE;
  if (tasklist->priv->monitor_num != -1 &&
      (wnck_window_get_state (window) & WNCK_WINDOW_STATE_SKIP_TASKLIST) &&
      GTK_WIDGET (tasklist)->window != NULL)
    {
      /* Do the extra check only if there is a suspect of a monitor change (= this window is off monitor) */
      wnck_window_get_geometry (window, &x, &y, &w, &h);
      if (!POINT_IN_RECT (x + w / 2, y + h / 2, tasklist->priv->monitor_geometry))
        {
          monitor_changed = (gdk_screen_get_monitor_at_window (_wnck_screen_get_gdk_screen (tasklist->priv->screen),
                                                     GTK_WIDGET (tasklist)->window) != tasklist->priv->monitor_num);
        }
    }

  /*
   * We want to re-generate the task list if
   * the window is shown but shouldn't be or
   * the window isn't shown but should be or
   * the tasklist itself changed monitor.
   */
  win_task = g_hash_table_lookup (tasklist->priv->win_hash, window);
  show = wnck_tasklist_include_window(tasklist, window);
  if (((win_task == NULL && !show) || (win_task != NULL && show)) && !monitor_changed)
    return;

  /* Don't keep any stale references */
  gtk_widget_queue_clear (GTK_WIDGET (tasklist));
  
  tasklist->priv->idle_callback_tag = g_idle_add (do_wnck_tasklist_update_lists, tasklist);
}

static void
wnck_tasklist_connect_window (WnckTasklist *tasklist,
			      WnckWindow   *window)
{
  g_signal_connect_object (window, "workspace_changed",
			   G_CALLBACK (wnck_tasklist_window_changed_workspace),
			   tasklist, 0);
  g_signal_connect_object (window, "geometry_changed",
			   G_CALLBACK (wnck_tasklist_window_changed_geometry),
			   tasklist, 0);
}

static void
wnck_tasklist_window_added (WnckScreen   *screen,
			    WnckWindow   *win,
			    WnckTasklist *tasklist)
{
#ifdef HAVE_STARTUP_NOTIFICATION
  wnck_tasklist_check_end_sequence (tasklist, win);
#endif
  
  wnck_tasklist_connect_window (tasklist, win);

  wnck_tasklist_update_lists (tasklist);
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

static void
wnck_tasklist_window_removed (WnckScreen   *screen,
			      WnckWindow   *win,
			      WnckTasklist *tasklist)
{
  wnck_tasklist_update_lists (tasklist);
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

static void
wnck_tasklist_viewports_changed (WnckScreen   *screen,
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
      WnckWorkspace *active_ws;
      WnckWorkspace *window_ws;

      active_ws = wnck_screen_get_active_workspace (tasklist->priv->screen);
      window_ws = wnck_window_get_workspace (task->window);
      if (window_ws &&
          active_ws != window_ws &&
	  !tasklist->priv->switch_workspace_on_unminimize)
	wnck_workspace_activate (window_ws);
	  
      wnck_window_activate_transient (task->window);
    }
  else
    {
      if (task->was_active || wnck_window_transient_is_active (task->window))
	{
	  task->was_active = FALSE;
	  wnck_window_minimize (task->window);
	  return;
	}
      else
	{
          WnckWorkspace *window_ws;
          
          window_ws = wnck_window_get_workspace (task->window);
          if (window_ws)
            wnck_workspace_activate (window_ws);

	  wnck_window_activate_transient (task->window);
	}
    }
  

  if (tasklist->priv->activate_timeout_id)
    gtk_timeout_remove (tasklist->priv->activate_timeout_id);

  tasklist->priv->activate_timeout_id = 
    gtk_timeout_add (500, &wnck_tasklist_change_active_timeout, tasklist);

  wnck_tasklist_change_active_task (tasklist, task);
}


static void
wnck_task_popup_menu (WnckTask *task,
                      gboolean  action_submenu)
{
  GtkWidget *menu;
  WnckTask *win_task;
  char *text;
  GdkPixbuf *pixbuf;
  GtkWidget *menu_item;
  GtkWidget *image;
  GList *l, *list;
  
  if (task->class_group == NULL)
    return;
  
  if (task->menu == NULL)
    task->menu = gtk_menu_new ();
  
  menu = task->menu;
  
  /* Remove old menu content */
  list = gtk_container_get_children (GTK_CONTAINER (menu));
  l = list;
  while (l)
    {
      GtkWidget *child = GTK_WIDGET (l->data);
      gtk_container_remove (GTK_CONTAINER (menu), child);
      l = l->next;
    }
  g_list_free (list);
  
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

       if (action_submenu)
         gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item),
                                    wnck_create_window_action_menu (win_task->window));
       else
         g_signal_connect_object (G_OBJECT (menu_item), "activate",
                                  G_CALLBACK (wnck_task_menu_activated),
                                  G_OBJECT (win_task),
                                  0);
      
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
      
      l = l->next;
    }

  gtk_menu_set_screen (GTK_MENU (menu),
		       _wnck_screen_get_gdk_screen (task->tasklist->priv->screen));
  
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
  /* Did we really want to change the state of the togglebutton? */
  if (task->really_toggling)
    return;
  
  /* Undo the toggle */
  task->really_toggling = TRUE;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
  task->really_toggling = FALSE;

  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      wnck_task_popup_menu (task, FALSE);
      break;
    case WNCK_TASK_WINDOW:
      if (task->window == NULL)
	return;
      
      wnck_tasklist_activate_task_window (task);
      break;
    case WNCK_TASK_STARTUP_SEQUENCE:
      break;
    }
}

static char *
wnck_task_get_text (WnckTask *task)
{
  WnckWindowState state;
  const char *name;
  
  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      name = wnck_class_group_get_name (task->class_group);
      if (name[0] != 0)
	return g_strdup_printf ("%s (%d)",
				name,
				g_list_length (task->windows));
      else
	return g_strdup_printf ("(%d)",
				g_list_length (task->windows));

    case WNCK_TASK_WINDOW:
      state = wnck_window_get_state (task->window);
      name = wnck_window_get_name (task->window);
      
      if (state & WNCK_WINDOW_STATE_SHADED)
	return g_strdup_printf ("=%s=", name);
      else if (state & WNCK_WINDOW_STATE_MINIMIZED)
	return g_strdup_printf ("[%s]", name);
      else
	return g_strdup (name);
      break;

    case WNCK_TASK_STARTUP_SEQUENCE:
#ifdef HAVE_STARTUP_NOTIFICATION
      name = sn_startup_sequence_get_description (task->startup_sequence);
      if (name == NULL)
        name = sn_startup_sequence_get_name (task->startup_sequence);
      if (name == NULL)
        name = sn_startup_sequence_get_binary_name (task->startup_sequence);
      
      return g_strdup (name);
#else
      return NULL;
#endif
      break;
    }

  return NULL;
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

  pixbuf = NULL;
  
  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      pixbuf = wnck_task_scale_icon (wnck_class_group_get_mini_icon (task->class_group),
				     FALSE);
      break;

    case WNCK_TASK_WINDOW:
      state = wnck_window_get_state (task->window);

      pixbuf =  wnck_task_scale_icon (wnck_window_get_mini_icon (task->window),
				      state & WNCK_WINDOW_STATE_MINIMIZED);
      break;
    case WNCK_TASK_STARTUP_SEQUENCE:
#ifdef HAVE_STARTUP_NOTIFICATION
      if (task->tasklist->priv->icon_loader != NULL)
        {
          const char *icon;
          
          icon = sn_startup_sequence_get_icon_name (task->startup_sequence);
          if (icon != NULL)
            {
              GdkPixbuf *loaded;
              
              loaded =  (* task->tasklist->priv->icon_loader) (icon,
                                                               MINI_ICON_SIZE,
                                                               0,
                                                               task->tasklist->priv->icon_loader_data);

              if (loaded != NULL)
                {
                  pixbuf = wnck_task_scale_icon (loaded, FALSE);
                  g_object_unref (G_OBJECT (loaded));
                }
            }
        }

      if (pixbuf == NULL)
        {
          _wnck_get_fallback_icons (NULL, 0, 0,
                                    &pixbuf, MINI_ICON_SIZE, MINI_ICON_SIZE);
        }
#endif
      break;
    }

  return pixbuf;
}

static gboolean
wnck_task_get_demands_attention (WnckTask *task)
{
  GList *l;
  WnckTask *win_task;
  gboolean demands_attention;

  demands_attention = FALSE;

  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      l = task->windows;
      while (l)
	{
	  win_task = WNCK_TASK (l->data);

	  if (wnck_window_get_state (win_task->window) & WNCK_WINDOW_STATE_DEMANDS_ATTENTION)
	    {
	      demands_attention = TRUE;
	      break;
	    }

	  l = l->next;
	}
      break;

    case WNCK_TASK_WINDOW:
      demands_attention =
	wnck_window_get_state (task->window) & WNCK_WINDOW_STATE_DEMANDS_ATTENTION;
      break;

    case WNCK_TASK_STARTUP_SEQUENCE:
      break;
    }

  return demands_attention != FALSE;
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
  if (text != NULL)
    {
      gtk_label_set_text (GTK_LABEL (task->label), text);
      if (wnck_task_get_demands_attention (task))
        eel_gtk_label_make_bold ((GTK_LABEL (task->label)));
      else
        wnck_gtk_label_make_normal ((GTK_LABEL (task->label)));
      gtk_tooltips_set_tip (task->tasklist->priv->tooltips, task->button, text, NULL);
      g_free (text);
    }

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
  
  if ((changed_mask & WNCK_WINDOW_STATE_MINIMIZED) ||
      (changed_mask & WNCK_WINDOW_STATE_DEMANDS_ATTENTION))
    {
      WnckTask *win_task;

      win_task = g_hash_table_lookup (tasklist->priv->win_hash,
						window);

      if (win_task)
	{
	  WnckTask *class_group_task;

	  wnck_task_update_visible_state (win_task);
	  
	  class_group_task = g_hash_table_lookup (tasklist->priv->class_group_hash, win_task->class_group);

	  if (class_group_task)
	    wnck_task_update_visible_state (class_group_task);
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

static void
wnck_task_class_name_changed (WnckClassGroup *class_group,
			      gpointer        data)
{
  WnckTask *task = WNCK_TASK (data);

  if (task)
    wnck_task_update_visible_state (task);
}

static void
wnck_task_class_icon_changed (WnckClassGroup *class_group,
			      gpointer        data)
{
  WnckTask *task = WNCK_TASK (data);

  if (task)
    wnck_task_update_visible_state (task);
}

static gboolean
wnck_task_motion_timeout (gpointer data)
{
  WnckTask *task = WNCK_TASK (data);

  task->button_activate = 0;

  wnck_window_activate_transient (task->window);

  return FALSE;
}

static void
wnck_task_drag_leave (GtkWidget          *widget,
		      GdkDragContext     *context,
		      guint               time,
		      WnckTask           *task)
{
  if (task->button_activate != 0)
    {
      g_source_remove (task->button_activate);
      task->button_activate = 0;
    }
}

static gboolean
wnck_task_drag_motion (GtkWidget          *widget,
		       GdkDragContext     *context,
		       gint                x,
		       gint                y,
		       guint               time,
		       WnckTask            *task)
{

   if (task->button_activate == 0 && task->type == WNCK_TASK_WINDOW)
       task->button_activate = g_timeout_add (TIMEOUT_ACTIVATE,
                                              wnck_task_motion_timeout,
                                              task);
   gdk_drag_status (context,0,time);

   return TRUE;
}

static gboolean
wnck_task_button_press_event (GtkWidget	      *widget,
			      GdkEventButton  *event,
			      gpointer         data)
{
  WnckTask *task = WNCK_TASK (data);

  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      wnck_task_popup_menu (task,
			    event->button == 3);
      return TRUE;

    case WNCK_TASK_WINDOW:
      if (event->button == 1)
        {
          /* is_most_recently_activated == is_active for click &
	   * sloppy focus methods.  We use the former here because
	   * 'mouse' focus provides a special case.  In that case, no
	   * window will be active, but if a window was the most
	   * recently active one (i.e. user moves mouse straight from
	   * window to tasklist), then we should still minimize it.
           */
          if (wnck_window_is_most_recently_activated (task->window))
            task->was_active = TRUE;
          else
            task->was_active = FALSE;

          return FALSE;
        } 
      else if (event->button == 3)
        {
          if (task->action_menu)
            gtk_widget_destroy (task->action_menu);

          g_assert (task->action_menu == NULL);
      
          task->action_menu = wnck_create_window_action_menu (task->window);

          g_object_add_weak_pointer (G_OBJECT (task->action_menu),
                                     (void**) &task->action_menu);
      
          gtk_menu_set_screen (GTK_MENU (task->action_menu),
                               _wnck_screen_get_gdk_screen (task->tasklist->priv->screen));

          gtk_widget_show (task->action_menu);
          gtk_menu_popup (GTK_MENU (task->action_menu),
                          NULL, NULL,
                          wnck_task_position_menu, task->button,
                          event->button,
                          gtk_get_current_event_time ());

          /* FIXME we should really arrange to destroy the menu on popdown */
      
          return TRUE;
        }
      break;
    case WNCK_TASK_STARTUP_SEQUENCE:
      break;
    }

  return FALSE;
}


static void
wnck_task_create_widgets (WnckTask *task)
{
  GtkWidget *table;
  GdkPixbuf *pixbuf;
  char *text;
  static GQuark disable_sound_quark = 0;
  
  if (!disable_sound_quark)
    disable_sound_quark = g_quark_from_static_string ("gnome_disable_sound_events");

  if (task->type == WNCK_TASK_STARTUP_SEQUENCE)
    task->button = gtk_button_new ();
  else
    task->button = gtk_toggle_button_new ();
  task->button_activate = 0;
  g_object_set_qdata (G_OBJECT (task->button),
		      disable_sound_quark, GINT_TO_POINTER (TRUE));
  g_object_add_weak_pointer (G_OBJECT (task->button),
                             (void**) &task->button);
  
  gtk_widget_set_name (task->button,
		       "tasklist-button");

  gtk_drag_dest_set (GTK_WIDGET(task->button), 0, NULL, 0, 0);

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
  if (wnck_task_get_demands_attention (task))
    eel_gtk_label_make_bold ((GTK_LABEL (task->label)));
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
		    GTK_FILL, GTK_EXPAND,
		    4, 0);

  gtk_container_add (GTK_CONTAINER (task->button), table);
  gtk_widget_show (table);
  
  gtk_tooltips_set_tip (task->tasklist->priv->tooltips, task->button, text, NULL);
  g_free (text);
  
  /* Set up signals */
  if (GTK_IS_TOGGLE_BUTTON (task->button))
    g_signal_connect_object (G_OBJECT (task->button), "toggled",
                             G_CALLBACK (wnck_task_button_toggled),
                             G_OBJECT (task),
                             0);

  
  g_signal_connect_object (G_OBJECT (task->button), "button_press_event",
                           G_CALLBACK (wnck_task_button_press_event),
                           G_OBJECT (task),
                           0);

  g_signal_connect_object (G_OBJECT(task->button), "drag_motion",
                           G_CALLBACK (wnck_task_drag_motion),
                           G_OBJECT (task),
                           0); 

  g_signal_connect_object (G_OBJECT(task->button), "drag_leave",
                           G_CALLBACK (wnck_task_drag_leave),
                           G_OBJECT (task),
                           0);

  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      task->class_name_changed_tag = g_signal_connect (G_OBJECT (task->class_group), "name_changed",
						       G_CALLBACK (wnck_task_class_name_changed), task);
      task->class_icon_changed_tag = g_signal_connect (G_OBJECT (task->class_group), "icon_changed",
						       G_CALLBACK (wnck_task_class_icon_changed), task);
      break;

    case WNCK_TASK_WINDOW:
      task->state_changed_tag = g_signal_connect (G_OBJECT (task->window), "state_changed",
                                                  G_CALLBACK (wnck_task_state_changed), task->tasklist);
      task->icon_changed_tag = g_signal_connect (G_OBJECT (task->window), "icon_changed",
                                                 G_CALLBACK (wnck_task_icon_changed), task);
      task->name_changed_tag = g_signal_connect (G_OBJECT (task->window), "name_changed",
						 G_CALLBACK (wnck_task_name_changed), task);
      break;

    case WNCK_TASK_STARTUP_SEQUENCE:
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
draw_dot (GdkWindow *window, GdkGC *lgc, GdkGC *dgc, int x, int y)
{
  gdk_draw_point (window, dgc, x,   y);
  gdk_draw_point (window, lgc, x+1, y+1);
}


gboolean
wnck_task_class_group_expose (GtkWidget        *widget,
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
compare_class_group_tasks (WnckTask *task1, WnckTask *task2)
{
  const char *name1, *name2;

  name1 = wnck_class_group_get_name (task1->class_group);
  name2 = wnck_class_group_get_name (task2->class_group);

  return g_utf8_collate (name1, name2);
}

static gint
wnck_task_compare (gconstpointer  a,
		   gconstpointer  b)
{
  WnckTask *task1 = WNCK_TASK (a);
  WnckTask *task2 = WNCK_TASK (b);
  gulong xid1_1, xid1_2;
  gulong xid2_1, xid2_2;
  
  xid1_1 = xid1_2 = xid2_1 = xid2_2 = 0;   /* silence compiler */
  
  switch (task1->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      if (task2->type == WNCK_TASK_CLASS_GROUP)
	return compare_class_group_tasks (task1, task2);
      else
	return -1; /* Sort groups before everything else */

    case WNCK_TASK_WINDOW:
      xid1_1 = wnck_window_get_group_leader (task1->window);
      xid1_2 = wnck_window_get_xid (task1->window);
      break;
    case WNCK_TASK_STARTUP_SEQUENCE:
      xid1_1 = xid1_2 = G_MAXULONG;
      break;
    }

  switch (task2->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      if (task1->type == WNCK_TASK_CLASS_GROUP)
	return compare_class_group_tasks (task1, task2);
      else
	return 1; /* Sort groups before everything else */

    case WNCK_TASK_WINDOW:
      xid2_1 = wnck_window_get_group_leader (task2->window);
      xid2_2 = wnck_window_get_xid (task2->window);
      break;
    case WNCK_TASK_STARTUP_SEQUENCE:
      xid2_1 = xid2_2 = G_MAXULONG;
      break;
    }
      
  if ((xid1_1 < xid2_1) || ((xid1_1 == xid2_1) && (xid1_2 < xid2_2)))
    return -1;
  else if ((xid1_1 == xid2_1) && (xid1_2 == xid2_2))
    return 0;
  else
    return 1;
}

static void
remove_startup_sequences_for_window (WnckTasklist *tasklist,
                                     WnckWindow   *window)
{
#ifdef HAVE_STARTUP_NOTIFICATION
  const char *win_id;
  GList *tmp;
  
  win_id = _wnck_window_get_startup_id (window);
  if (win_id == NULL)
    return;

  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      WnckTask *task = tmp->data;
      GList *next = tmp->next;
      const char *task_id;
      
      g_assert (task->type == WNCK_TASK_STARTUP_SEQUENCE);

      task_id = sn_startup_sequence_get_id (task->startup_sequence);

      if (task_id && strcmp (task_id, win_id) == 0)
        gtk_widget_destroy (task->button);
      
      tmp = next;
    }
#else
  ; /* nothing */
#endif
}

static WnckTask *
wnck_task_new_from_window (WnckTasklist *tasklist,
			   WnckWindow   *window)
{
  WnckTask *task;

  task = g_object_new (WNCK_TYPE_TASK, NULL);

  task->type = WNCK_TASK_WINDOW;
  task->window = g_object_ref (window);
  task->class_group = g_object_ref (wnck_window_get_class_group (window));
  task->tasklist = tasklist;
  
  wnck_task_create_widgets (task);

  remove_startup_sequences_for_window (tasklist, window);
  
  return task;
}

static WnckTask *
wnck_task_new_from_class_group (WnckTasklist   *tasklist,
				WnckClassGroup *class_group)
{
  WnckTask *task;

  task = g_object_new (WNCK_TYPE_TASK, NULL);

  task->type = WNCK_TASK_CLASS_GROUP;
  task->window = NULL;
  task->class_group = g_object_ref (class_group);
  task->tasklist = tasklist;

  wnck_task_create_widgets (task);

  g_signal_connect_object (task->button, "expose_event",
                           G_CALLBACK (wnck_task_class_group_expose),
                           G_OBJECT (task),
                           G_CONNECT_AFTER);

  return task;
}

#ifdef HAVE_STARTUP_NOTIFICATION
static WnckTask*
wnck_task_new_from_startup_sequence (WnckTasklist      *tasklist,
                                     SnStartupSequence *sequence)
{
  WnckTask *task;

  task = g_object_new (WNCK_TYPE_TASK, NULL);

  task->type = WNCK_TASK_STARTUP_SEQUENCE;
  task->window = NULL;
  task->class_group = NULL;
  task->startup_sequence = sequence;
  sn_startup_sequence_ref (task->startup_sequence);
  task->tasklist = tasklist;
  
  wnck_task_create_widgets (task);

  return task;
}

/* This should be fairly long, as it should never be required unless
 * apps or .desktop files are buggy, and it's confusing if
 * OpenOffice or whatever seems to stop launching - people
 * might decide they need to launch it again.
 */
#define STARTUP_TIMEOUT 15000

static gboolean
sequence_timeout_callback (void *user_data)
{
  WnckTasklist *tasklist = user_data;
  GList *tmp;
  GTimeVal now;
  long tv_sec, tv_usec;
  double elapsed;

  g_get_current_time (&now);

 restart:
  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);

      sn_startup_sequence_get_last_active_time (task->startup_sequence,
                                                &tv_sec, &tv_usec);
      
      elapsed =
        ((((double)now.tv_sec - tv_sec) * G_USEC_PER_SEC +
          (now.tv_usec - tv_usec))) / 1000.0;

      if (elapsed > STARTUP_TIMEOUT)
        {
          g_assert (task->button != NULL);
          /* removes task from list as a side effect */
          gtk_widget_destroy (task->button);      

          goto restart; /* don't iterate over changed list, just restart;
                         * not efficient but who cares here.
                         */
        }
      
      tmp = tmp->next;
    }
  
  if (tasklist->priv->startup_sequences == NULL)
    {
      tasklist->priv->startup_sequence_timeout = 0;
      return FALSE;
    }
  else
    return TRUE;
}

static void
wnck_tasklist_sn_event (SnMonitorEvent *event,
                        void           *user_data)
{
  WnckTasklist *tasklist;

  tasklist = WNCK_TASKLIST (user_data);

  switch (sn_monitor_event_get_type (event))
    {
    case SN_MONITOR_EVENT_INITIATED:
      {
        WnckTask *task;
        
        task = wnck_task_new_from_startup_sequence (tasklist,
                                                    sn_monitor_event_get_startup_sequence (event));
        
        gtk_widget_set_parent (task->button, GTK_WIDGET (tasklist));
        gtk_widget_show (task->button);
	      
        tasklist->priv->startup_sequences =
          g_list_prepend (tasklist->priv->startup_sequences,
                          task);

        if (tasklist->priv->startup_sequence_timeout == 0)
          {
            tasklist->priv->startup_sequence_timeout =
              g_timeout_add (1000, sequence_timeout_callback,
                             tasklist);
          }
        
        gtk_widget_queue_resize (GTK_WIDGET (tasklist));
      }
      break;

    case SN_MONITOR_EVENT_COMPLETED:
      {
        GList *tmp;
        tmp = tasklist->priv->startup_sequences;
        while (tmp != NULL)
          {
            WnckTask *task = WNCK_TASK (tmp->data);

            if (task->startup_sequence ==
                sn_monitor_event_get_startup_sequence (event))
              {
                g_assert (task->button != NULL);
                /* removes task from list as a side effect */
                gtk_widget_destroy (task->button);      
                break;
              }
            
            tmp = tmp->next;
          }
      }
      break;

    case SN_MONITOR_EVENT_CHANGED:
      break;

    case SN_MONITOR_EVENT_CANCELED:
      break;
    }

  if (tasklist->priv->startup_sequences == NULL &&
      tasklist->priv->startup_sequence_timeout != 0)
    {
      g_source_remove (tasklist->priv->startup_sequence_timeout);
      tasklist->priv->startup_sequence_timeout = 0;
    }
}

static void
wnck_tasklist_check_end_sequence (WnckTasklist   *tasklist,
                                  WnckWindow     *window)
{
  const char *res_class;
  const char *res_name;
  GList *tmp;
  
  if (tasklist->priv->startup_sequences == NULL)
    return;
  
  res_class = _wnck_window_get_resource_class (window);
  res_name = _wnck_window_get_resource_name (window);

  if (res_class == NULL && res_name == NULL)
    return;
  
  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      WnckTask *task = WNCK_TASK (tmp->data);
      const char *wmclass;

      wmclass = sn_startup_sequence_get_wmclass (task->startup_sequence);
      
      if (wmclass != NULL &&
          ((res_class && strcmp (res_class, wmclass) == 0) ||
           (res_name && strcmp (res_name, wmclass) == 0)))
        {
          sn_startup_sequence_complete (task->startup_sequence);

          g_assert (task->button != NULL);
          /* removes task from list as a side effect */
          gtk_widget_destroy (task->button);      

          /* only match one */
          return;
        }
      
      tmp = tmp->next;
    }
}

#endif /* HAVE_STARTUP_NOTIFICATION */
