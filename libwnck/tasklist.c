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

struct _WnckTask
{
  char *text;
  GdkPixbuf *icon;
  gpointer data;
  GtkWidget *button;
};

static WnckTask* wnck_task_new  (void);
static void      wnck_task_free (WnckTask *task);

struct _WnckTasklistPrivate
{
  GList *tasks;
  int button_height;
  int all_button_widths;
  guint layout_pending : 1;
};

enum
{
  LAST_SIGNAL
};

static void wnck_tasklist_init        (WnckTasklist      *tasklist);
static void wnck_tasklist_class_init  (WnckTasklistClass *klass);
static void wnck_tasklist_finalize    (GObject        *object);

static void     wnck_tasklist_size_request  (GtkWidget        *widget,
                                             GtkRequisition   *requisition);
static void     wnck_tasklist_size_allocate (GtkWidget        *widget,
                                             GtkAllocation    *allocation);
static gboolean wnck_tasklist_expose_event  (GtkWidget        *widget,
                                             GdkEventExpose   *event);
static gboolean wnck_tasklist_focus         (GtkWidget        *widget,
                                             GtkDirectionType  direction);
static void     wnck_tasklist_forall        (GtkContainer     *container,
                                             gboolean	       include_internals,
                                             GtkCallback       callback,
                                             gpointer          callback_data);

static void wnck_tasklist_ensure_layout     (WnckTasklist *tasklist);
static void wnck_tasklist_invalidate_layout (WnckTasklist *tasklist);


static gpointer parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

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
  tasklist->priv = g_new0 (WnckTasklistPrivate, 1);
  
}

static void
wnck_tasklist_class_init (WnckTasklistClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = wnck_tasklist_finalize;

  widget_class->size_request = wnck_tasklist_size_request;
  widget_class->size_allocate = wnck_tasklist_size_allocate;
  widget_class->expose_event = wnck_tasklist_expose_event;

  container_class->forall = wnck_tasklist_forall;
}

static void
wnck_tasklist_finalize (GObject *object)
{
  WnckTasklist *tasklist;

  tasklist = WNCK_TASKLIST (object);
  
  g_free (tasklist->priv);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
wnck_tasklist_size_request  (GtkWidget      *widget,
                             GtkRequisition *requisition)
{
  WnckTasklist *tasklist;
  GList *tmp;
  
  tasklist = WNCK_TASKLIST (widget);

  tasklist->priv->button_height = 1;
  tasklist->priv->all_button_widths = 0;
  
  tmp = tasklist->priv->tasks;
  while (tmp != NULL)
    {
      WnckTask *task = tmp->data;
      GtkRequisition child_req;

      gtk_widget_size_request (task->button, &child_req);

      tasklist->priv->button_height = MAX (child_req.height,
                                           tasklist->priv->button_height);

      tasklist->priv->all_button_widths += child_req.width;
      
      tmp = tmp->next;
    }
  
  requisition->width = 1;
  requisition->height = tasklist->priv->button_height;
}

static void
wnck_tasklist_size_allocate (GtkWidget      *widget,
                             GtkAllocation  *allocation)
{
  WnckTasklist *tasklist;

  tasklist = WNCK_TASKLIST (widget);

  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

  /* don't use invalidate_layout here because it queues a resize */
  tasklist->priv->layout_pending = TRUE;
  wnck_tasklist_ensure_layout (tasklist);
}

static gboolean
wnck_tasklist_expose_event  (GtkWidget      *widget,
                             GdkEventExpose *event)
{
  WnckTasklist *tasklist;

  tasklist = WNCK_TASKLIST (widget);

  /* default impl OK? we may want a frame or something later */
  return GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
}

static gboolean
wnck_tasklist_focus (GtkWidget        *widget,
                     GtkDirectionType  direction)
{
  WnckTasklist *tasklist;

  tasklist = WNCK_TASKLIST (widget);

  /* Hmm, maybe the default impl. works if it's in forall order */
  return GTK_WIDGET_CLASS (parent_class)->focus (widget, direction);
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

  tmp = tasklist->priv->tasks;
  while (tmp != NULL)
    {
      WnckTask *task = tmp->data;
      
      (* callback) (task->button, callback_data);
      
      tmp = tmp->next;
    }
}

static void
wnck_tasklist_ensure_layout (WnckTasklist *tasklist)
{
  int rows;
  GtkWidget *widget;
  int row_height;
  int extra_height;
  int width_shortage;
  int n_buttons;
  int avg_width;
  
  if (!tasklist->priv->layout_pending)
    return;

#define ROW_SPACING 1
  
  widget = GTK_WIDGET (tasklist);

  /* this is slightly wrong, since we have (rows - 1) ROW_SPACING, but
   * (rows) button_height, and here we consider (rows) ROW_SPACING
   */
  rows = widget->allocation.height / (tasklist->priv->button_height + ROW_SPACING);
  if (rows == 0)
    rows = 1;
  
  row_height = widget->allocation.height / rows;
  extra_height = widget->allocation.height % rows;

  n_buttons = g_list_length (tasklist->priv->tasks);
  avg_width = tasklist->priv->all_button_widths / n_buttons;
  
  /* This is also slightly wrong, because you "lose" some width
   * at the end of each row, because buttons don't perfectly fit
   * into rows.
   */
  width_shortage = tasklist->priv->all_button_widths -
    (widget->allocation.width * rows);

  
}

static void
wnck_tasklist_invalidate_layout (WnckTasklist *tasklist)
{
  tasklist->priv->layout_pending = TRUE;
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}

GtkWidget*
wnck_tasklist_new (void)
{
  WnckTasklist *tasklist;

  tasklist = g_object_new (WNCK_TYPE_TASKLIST, NULL);

  return GTK_WIDGET (tasklist);
}

WnckTask*
wnck_tasklist_append_task (WnckTasklist *tasklist,
                           const char   *text,
                           GdkPixbuf    *icon,
                           gpointer      task_data)
{
  g_return_val_if_fail (WNCK_IS_TASKLIST (tasklist), NULL);
  
}

void
wnck_tasklist_remove_task (WnckTasklist *tasklist,
                           WnckTask     *task)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));

}

WnckTask*
wnck_tasklist_get_task_at_point (WnckTasklist *tasklist,
                                 int           x,
                                 int           y)
{
  g_return_val_if_fail (WNCK_IS_TASKLIST (tasklist), NULL);

}

WnckTask*
wnck_tasklist_get_focused_task  (WnckTasklist *tasklist)
{
  g_return_val_if_fail (WNCK_IS_TASKLIST (tasklist), NULL);

}

void
wnck_tasklist_set_active_task   (WnckTasklist *tasklist,
                                 WnckTask     *task)
{
  g_return_if_fail (WNCK_IS_TASKLIST (tasklist));
  
}

WnckTask*
wnck_tasklist_find_task_by_data (WnckTasklist *tasklist,
                                 gpointer      data)
{
  g_return_val_if_fail (WNCK_IS_TASKLIST (tasklist), NULL);
  
}

static WnckTask*
wnck_task_new (void)
{
  WnckTask *task;

  task = g_new0 (WnckTask, 1);

  return task;
}

static void
wnck_task_free (WnckTask *task)
{
  if (task->icon)
    g_object_unref (G_OBJECT (task->icon));
  g_free (task->text);

  g_free (task);
}

GType
wnck_task_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static ("WnckTask",
                                             NULL,
                                             NULL);

  return our_type;
}

const char*
wnck_task_get_text (WnckTask *task)
{
  return task->text;
}

GdkPixbuf*
wnck_task_get_icon (WnckTask *task)
{
  return task->icon;
}

gpointer
wnck_task_get_data (WnckTask *task)
{
  return task->data;
}
