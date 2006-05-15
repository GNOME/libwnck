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

#include <config.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <glib/gi18n-lib.h>
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

#define MAX_WIDTH_CHARS 250

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

  guint32 dnd_timestamp;

  GdkPixbuf *screenshot;
  GdkPixbuf *screenshot_fade1;
  GdkPixbuf *screenshot_fade2;

  gdouble glow_start_time;
  
  guint button_glow;
  
  guint row;
  guint col;
};

struct _WnckTaskClass
{
  GObjectClass parent_class;
};

typedef struct _skipped_window
{
  WnckWindow *window;
  gulong tag;
} skipped_window;

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
  
  /* windows with _NET_WM_STATE_SKIP_TASKBAR set; connected to 
   * "state_changed" signal, but excluded from tasklist.
   */
  GList *skipped_windows; 

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
  GtkReliefStyle relief;
  
  GdkPixmap *background;
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
static gboolean wnck_task_get_needs_attention (WnckTask *task);


static char      *wnck_task_get_text (WnckTask *task, gboolean icon_text);
static GdkPixbuf *wnck_task_get_icon (WnckTask *task);
static gint       wnck_task_compare_alphabetically (gconstpointer  a,
                                                    gconstpointer  b);
static gint       wnck_task_compare  (gconstpointer  a,
				      gconstpointer  b);
static void       wnck_task_update_visible_state (WnckTask *task);
static void       wnck_task_state_changed        (WnckWindow      *window,
                                                  WnckWindowState  changed_mask, 
                                                  WnckWindowState  new_state,
                                                  gpointer         data);
static void wnck_tasklist_init        (WnckTasklist      *tasklist);
static void wnck_tasklist_class_init  (WnckTasklistClass *klass);
static void wnck_tasklist_finalize    (GObject        *object);

static void     wnck_tasklist_size_request  (GtkWidget        *widget,
                                             GtkRequisition   *requisition);
static void     wnck_tasklist_size_allocate (GtkWidget        *widget,
                                             GtkAllocation    *allocation);
static void     wnck_tasklist_realize       (GtkWidget        *widget);
static void     wnck_tasklist_unrealize     (GtkWidget        *widget);
static gint     wnck_tasklist_expose        (GtkWidget        *widget,
                                             GdkEventExpose    *event);
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
static void     wnck_tasklist_activate_task_window     (WnckTask *task,
                                                        guint32   timestamp);

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

static GType
wnck_task_get_type (void) G_GNUC_CONST;

static void
cleanup_screenshots (WnckTask *task)
{
  if (task->screenshot != NULL)
    {
      g_object_unref (task->screenshot);
      task->screenshot = NULL;
    }
  if (task->screenshot_fade1 != NULL)
    {
      g_object_unref (task->screenshot_fade1);
      task->screenshot_fade1 = NULL;
    }
  if (task->screenshot_fade2 != NULL)
    {
      g_object_unref (task->screenshot_fade2);
      task->screenshot_fade2 = NULL;
    }
}

static GType
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
        NULL            /* value_table */
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
  task->glow_start_time = 0.0;
  task->button_glow = 0;
  task->screenshot = NULL;
  task->screenshot_fade1 = NULL;
  task->screenshot_fade2 = NULL;
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

static GdkPixbuf *
glow_pixbuf (WnckTask        *task,
             gdouble          factor)
{
  GdkPixbuf *destination;
  
  destination = gdk_pixbuf_copy (task->screenshot);
  if (destination == NULL)
    return NULL;

  if (factor > 0)
    {
      gdk_pixbuf_composite (task->screenshot_fade1, destination, 0, 0,
                            gdk_pixbuf_get_width (task->screenshot),
                            gdk_pixbuf_get_height (task->screenshot),
                            0, 0, 1, 1, GDK_INTERP_NEAREST,
                            ABS((int)(factor * G_MAXUINT8)));
    }
  else
    {
      gdk_pixbuf_composite (task->screenshot_fade2, destination, 0, 0,
                            gdk_pixbuf_get_width (task->screenshot),
                            gdk_pixbuf_get_height (task->screenshot),
                            0, 0, 1, 1, GDK_INTERP_NEAREST,
                            ABS((int)(factor * G_MAXUINT8)));
    }
  
  return destination;
}

static gboolean
wnck_task_button_glow (WnckTask *task)
{
  GdkPixbuf *glowing_screenshot;
  GTimeVal tv;
  gdouble glow_factor, now;
  gfloat fade_opacity, loop_time;

  if (task->screenshot == NULL)
    return TRUE;

  g_get_current_time (&tv);
  now = (tv.tv_sec * (1.0 * G_USEC_PER_SEC) +
        tv.tv_usec) / G_USEC_PER_SEC;

  if (task->glow_start_time <= G_MINDOUBLE)
    task->glow_start_time = now;

  gtk_widget_style_get (GTK_WIDGET (task->tasklist), "fade-opacity", &fade_opacity,
                                                     "fade-loop-time", &loop_time,
                                                     NULL);
  
  /* This is basically a cos curve which is moved up by 1, and every second
   * iteration is flipped down.
   * So below is a cos function, that is between 0 and 1 starting at 0. */
  glow_factor = fade_opacity * (0.5 - 0.5 * cos ((now - task->glow_start_time) * M_PI * 4.0 / loop_time));

  /* and this will flip every second iteration down. */
  if ((int)((now - task->glow_start_time) / loop_time * 2.0) % 2 == 1)
  	glow_factor = -glow_factor;

  glowing_screenshot = glow_pixbuf (task, glow_factor);
  if (glowing_screenshot == NULL)
    return TRUE;

  gdk_draw_pixbuf (task->button->window,
                   task->button->style->fg_gc[GTK_WIDGET_STATE (task->button)],
                   glowing_screenshot,
                   0, 0, 
                   task->button->allocation.x, 
                   task->button->allocation.y,
                   gdk_pixbuf_get_width (glowing_screenshot),
                   gdk_pixbuf_get_height (glowing_screenshot),
                   GDK_RGB_DITHER_NORMAL, 0, 0);
  gdk_pixbuf_unref (glowing_screenshot);

  return TRUE;
}

static void
wnck_task_clear_glow_start_timeout_id (WnckTask *task)
{
  task->button_glow = 0;
}

static void
wnck_task_queue_glow (WnckTask *task)
{
  if (task->button_glow == 0)
    {
      task->glow_start_time = 0.0;

      /* The animation doesn't speed up or slow down based on the
       * timeout value, but instead will just appear smoother or 
       * choppier. To adjust the speed change the parameter to 
       * sine in the timeout callback.
       */
      task->button_glow =
        g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 
                            50,
                            (GSourceFunc) wnck_task_button_glow, task,
                            (GDestroyNotify) wnck_task_clear_glow_start_timeout_id);
    }
}

static void
wnck_task_stop_glow (WnckTask *task)
{
  if (task->button_glow != 0)
    {
      g_source_remove (task->button_glow);
      task->button_glow = 0;
      task->glow_start_time = 0.0;
    }
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
      g_object_unref (task->menu);
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

  cleanup_screenshots (task);

  wnck_task_stop_glow (task);

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
        NULL            /* value_table */
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
  tasklist->priv->relief = GTK_RELIEF_NORMAL;
  
  tasklist->priv->background = NULL;

  tasklist->priv->skipped_windows = NULL;

  atk_obj = gtk_widget_get_accessible (widget);
  atk_object_set_name (atk_obj, _("Window List"));
  atk_object_set_description (atk_obj, _("Tool to switch between visible windows"));
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
  widget_class->expose_event = wnck_tasklist_expose;
  
  container_class->forall = wnck_tasklist_forall;
  container_class->remove = wnck_tasklist_remove;
  
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("fade-using-state",
                                                                 "Using state",
                                                                 "If set to TRUE the Tasklist will fade between two states. If set to FALSE, it will fade between two colors. Default: FALSE",
                                                                 FALSE,
                                                                 G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_enum ("fade-state1",
                                                              "First state",
                                                              "Sets the first state used to do the fading. Default: GTK_STATE_PRELIGHT",
                                                              GTK_TYPE_STATE_TYPE,
                                                              GTK_STATE_PRELIGHT,
                                                              G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_enum ("fade-state2",
                                                              "Second state",
                                                              "Sets the second state used to do the fading. Default: GTK_STATE_PRELIGHT",
                                                              GTK_TYPE_STATE_TYPE,
                                                              GTK_STATE_PRELIGHT,
                                                              G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boxed ("fade-color1",
                                                               "First color",
                                                               "Sets the first color used for the fading.",
                                                               GDK_TYPE_COLOR,
                                                               G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boxed ("fade-color2",
                                                               "Second color",
                                                               "Sets the second color used for the fading.",
                                                               GDK_TYPE_COLOR,
                                                               G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
  
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("fade-opacity",
                                                              "Final opacity",
                                                              "The final opacity that will be reached. Default: 0.4",
                                                              0.0, 1.0, 0.4,
                                                              G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
  
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("fade-loop-time",
                                                              "Loop time",
                                                              "The time one loop takes when fading, in seconds. Default: 3.0",
                                                              0.2, 10.0, 3.0,
                                                              G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
}

static void
wnck_tasklist_free_skipped_windows (WnckTasklist  *tasklist)
{
  GList *l;
  
  l = tasklist->priv->skipped_windows;
  
  while (l != NULL)
    {
      skipped_window *skipped = (skipped_window*) l->data;
      g_signal_handler_disconnect (skipped->window, skipped->tag);
      g_object_unref (skipped->window);
      g_free (skipped);
      l = l->next;
    }
  
  g_list_free (tasklist->priv->skipped_windows);
}

static void
wnck_tasklist_finalize (GObject *object)
{
  WnckTasklist *tasklist;

  tasklist = WNCK_TASKLIST (object);

  if (tasklist->priv->free_icon_loader_data != NULL)
    (* tasklist->priv->free_icon_loader_data) (tasklist->priv->icon_loader_data);
  
  if (tasklist->priv->skipped_windows)
    {
      wnck_tasklist_free_skipped_windows (tasklist);
      tasklist->priv->skipped_windows = NULL;
    }
  
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

  g_object_unref (tasklist->priv->background);
  tasklist->priv->background = NULL;
  
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

static void
wnck_tasklist_set_relief_callback (WnckWindow   *win,
				   WnckTask     *task,
				   WnckTasklist *tasklist)
{
  gtk_button_set_relief (GTK_BUTTON (task->button), tasklist->priv->relief);
}

void
wnck_tasklist_set_button_relief (WnckTasklist *tasklist, GtkReliefStyle relief)
{
  if (relief == tasklist->priv->relief)
    return;

  tasklist->priv->relief = relief;

  g_hash_table_foreach (tasklist->priv->win_hash,
                        (GHFunc) wnck_tasklist_set_relief_callback,
                        tasklist);
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
  g_return_val_if_fail (n_elements != NULL, NULL);

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
  GList *windows_sorted = NULL;
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

          /* Sort */
          class_group_task->windows = g_list_sort (class_group_task->windows,
                                                   wnck_task_compare_alphabetically);
	  
	  /* Hide all this group's windows */
	  l = class_group_task->windows;
	  while (l != NULL)
	    {
	      win_task = WNCK_TASK (l->data);
	      
	      gtk_widget_set_child_visible (GTK_WIDGET (win_task->button), FALSE);

              cleanup_screenshots (win_task);
              
	      l = l->next;
	    }
	}
      else
	{
	  visible_tasks = g_list_prepend (visible_tasks, class_group_task->windows->data);
	  gtk_widget_set_child_visible (GTK_WIDGET (class_group_task->button), FALSE);

          cleanup_screenshots (class_group_task);
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
      
      cleanup_screenshots (class_group_task);

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
      int row = i % n_rows;
      int col = i / n_rows;
      
      child_allocation.x = total_width*col / n_cols;
      child_allocation.y = allocation->height*row / n_rows;
      child_allocation.width = total_width*(col + 1) / n_cols - child_allocation.x;
      child_allocation.height = allocation->height*(row + 1) / n_rows - child_allocation.y;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      gtk_widget_size_allocate (task->button, &child_allocation);
      gtk_widget_set_child_visible (GTK_WIDGET (task->button), TRUE);

      if (task->type != WNCK_TASK_STARTUP_SEQUENCE)
        {
          GList *ll;
          
          /* Build sorted windows list */
          if (g_list_length (task->windows) > 1)
            windows_sorted = g_list_concat (windows_sorted,
                                           g_list_copy (task->windows));
          else
            windows_sorted = g_list_append (windows_sorted, task);
          task->row = row;
          task->col = col;
          for (ll = task->windows; ll; ll = ll->next)
            {
              WNCK_TASK (ll->data)->row = row;
              WNCK_TASK (ll->data)->col = col;
            }
        }
      i++;
      l = l->next;
    }

  /* Update icon geometries. */
  wnck_tasklist_update_icon_geometries (tasklist, visible_tasks);
  
  g_list_free (visible_tasks);
  g_list_free (tasklist->priv->windows);
  tasklist->priv->windows = windows_sorted;
 
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

  tasklist_instances = g_slist_append (tasklist_instances, tasklist);
  g_slist_foreach (tasklist_instances,
		   (GFunc) wnck_tasklist_update_lists,
		   NULL);
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
  
  tasklist_instances = g_slist_remove (tasklist_instances, tasklist);
  g_slist_foreach (tasklist_instances,
		   (GFunc) wnck_tasklist_update_lists,
		   NULL);
}

static gint
wnck_tasklist_expose (GtkWidget      *widget,
                      GdkEventExpose *event)
{
  WnckTasklist *tasklist;
  GdkGC *gc;
  
  g_return_val_if_fail (WNCK_IS_TASKLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      tasklist = WNCK_TASKLIST (widget);
      /* get a screenshot of the background */
      
      if (tasklist->priv->background != NULL)
        g_object_unref (tasklist->priv->background);
      
      tasklist->priv->background = gdk_pixmap_new (widget->window,
                                                   widget->allocation.width,
                                                   widget->allocation.height,
                                                   -1);
      gc = gdk_gc_new (tasklist->priv->background);
      gdk_draw_drawable (tasklist->priv->background, gc, widget->window,
                         widget->allocation.x, widget->allocation.y, 0, 0,
                         widget->allocation.width, widget->allocation.height);
      
      g_object_unref (gc);
    }
  
  return (* GTK_WIDGET_CLASS (tasklist_parent_class)->expose_event) (widget, event);
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

static gboolean
wnck_tasklist_scroll_cb (WnckTasklist *tasklist,
                         GdkEventScroll *event,
                         gpointer user_data)
{
  /* use the fact that tasklist->priv->windows is sorted is sorted
   * see wnck_tasklist_size_allocate() */
  GList *window;
  gint row = 0;
  gint col = 0;

  window = g_list_find (tasklist->priv->windows,
                        tasklist->priv->active_task);
  if (window)
    {
      row = WNCK_TASK (window->data)->row;
      col = WNCK_TASK (window->data)->col;
    }
  else
    if (tasklist->priv->activate_timeout_id)
      /* There is no active_task yet, but there will be one after the timeout.
       * It occurs if we change the active task too fast. */
      return TRUE;

  switch (event->direction)
    {
      case GDK_SCROLL_UP:
        if (!window)
          window = g_list_last (tasklist->priv->windows);
        else
          window = window->prev;
      break;

      case GDK_SCROLL_DOWN:
        if (!window)
          window = tasklist->priv->windows;
        else
          window = window->next;
      break;

      case GDK_SCROLL_LEFT:
        if (!window)
          window = g_list_last (tasklist->priv->windows);
        else
          {
            /* Search the first window on the previous colomn at same row */
            while (window && (WNCK_TASK(window->data)->row != row ||
                              WNCK_TASK(window->data)->col != col-1))
              window = window->prev;
            /* If no window found, select the first one */
            if (!window)
              window = tasklist->priv->windows;
          }
      break;

      case GDK_SCROLL_RIGHT:
        if (!window)
          window = tasklist->priv->windows;
        else
          {
            /* Search the first window on the next colomn at same row */
            while (window && (WNCK_TASK(window->data)->row != row ||
                              WNCK_TASK(window->data)->col != col+1))
              window = window->next;
            /* If no window found, select the last one */
            if (!window)
              window = g_list_last (tasklist->priv->windows);
          }
      break;

      default:
        g_assert_not_reached ();
    }

  if (window)
    wnck_tasklist_activate_task_window (window->data, event->time);

  return TRUE;
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

  /* callback when there is a scroll-event for switching to the next window  */
  g_signal_connect_object (G_OBJECT (tasklist),
                           "scroll-event",
                           G_CALLBACK (wnck_tasklist_scroll_cb),
                           G_OBJECT (tasklist),
                           0);
  
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
  
  if (tasklist->priv->skipped_windows)
    {
      wnck_tasklist_free_skipped_windows (tasklist);
      tasklist->priv->skipped_windows = NULL;
    }
  
  g_assert (tasklist->priv->class_groups == NULL);
  g_assert (g_hash_table_size (tasklist->priv->class_group_hash) == 0);
}


/*
 * This function determines if a window should be included in the tasklist.
 */
static gboolean
tasklist_include_window_impl (WnckTasklist *tasklist,
                              WnckWindow   *win,
                              gboolean      check_for_skipped_list)
{
  WnckWorkspace *active_workspace;
  int x, y, w, h;

  if (!check_for_skipped_list &&
      wnck_window_get_state (win) & WNCK_WINDOW_STATE_SKIP_TASKLIST)
    return FALSE;

  if (tasklist->priv->monitor_num != -1)
    {
      wnck_window_get_geometry (win, &x, &y, &w, &h);
      /* Don't include the window if its center point is not on the same monitor */
      if (gdk_screen_get_monitor_at_point (_wnck_screen_get_gdk_screen (tasklist->priv->screen),
                                           x + w / 2, y + h / 2) != tasklist->priv->monitor_num)
        return FALSE;
    }

  /* Remainder of checks aren't relevant for checking if the window should
   * be in the skipped list.
   */
  if (check_for_skipped_list)
    return TRUE;

  if (tasklist->priv->include_all_workspaces)
    return TRUE;

  if (wnck_window_is_pinned (win))
    return TRUE;

  active_workspace = wnck_screen_get_active_workspace (tasklist->priv->screen);  
  if (active_workspace == NULL)
    return TRUE;

  if (active_workspace != wnck_window_get_workspace (win) &&
      !wnck_window_or_transient_needs_attention (win))
    return FALSE;

  if (!wnck_workspace_is_virtual (active_workspace))
    return TRUE;

  return wnck_window_is_in_viewport (win, active_workspace);
}

static gboolean
tasklist_include_in_skipped_list (WnckTasklist *tasklist, WnckWindow *win)
{
  return tasklist_include_window_impl (tasklist, 
                                       win,
                                       TRUE /* check_for_skipped_list */);
}

static gboolean
wnck_tasklist_include_window (WnckTasklist *tasklist, WnckWindow *win)
{
  return tasklist_include_window_impl (tasklist, 
                                       win,
                                       FALSE /* check_for_skipped_list */);
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
      else if (tasklist_include_in_skipped_list (tasklist, win))
        {
          skipped_window *skipped = g_new0 (skipped_window, 1);
          skipped->window = g_object_ref (win);
          skipped->tag = g_signal_connect (G_OBJECT (win),
                                           "state_changed",
                                           G_CALLBACK (wnck_task_state_changed),
                                           tasklist);
          tasklist->priv->skipped_windows = 
            g_list_prepend (tasklist->priv->skipped_windows,
                            (gpointer) skipped);
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
  WnckWindow *initial_window;
  WnckTask *active_task = NULL;

  /* FIXME: check for group modal window */
  initial_window = active_window = wnck_screen_get_active_window (screen);
  active_task = g_hash_table_lookup (tasklist->priv->win_hash, active_window);
  while (active_window && !active_task)
    {
      active_window = wnck_window_get_transient (active_window);
      active_task = g_hash_table_lookup (tasklist->priv->win_hash, 
                                         active_window);
      /* Check for transient cycles */
      if (active_window == initial_window)
        break;
    }

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
  show = wnck_tasklist_include_window (tasklist, window);
  if (((win_task == NULL && !show) || (win_task != NULL && show)) &&
      !monitor_changed)
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
  gint pointer_x;
  gint pointer_y;
  
  gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

  gdk_window_get_origin (widget->window, &menu_xpos, &menu_ypos);

  menu_xpos += widget->allocation.x;
  menu_ypos += widget->allocation.y;

  if (menu_ypos >  gdk_screen_height () / 2)
    menu_ypos -= requisition.height;
  else
    menu_ypos += widget->allocation.height;

  gtk_widget_get_pointer (widget, &pointer_x, &pointer_y);
  if (requisition.width < pointer_x)
    menu_xpos += MIN (pointer_x, widget->allocation.width - requisition.width);

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

  /* This is an "activate" callback function so gtk_get_current_event_time()
   * will suffice.
   */
  wnck_tasklist_activate_task_window (task, gtk_get_current_event_time ());
}

static void
wnck_tasklist_activate_task_window (WnckTask *task,
                                    guint32   timestamp)
{
  WnckTasklist *tasklist;
  WnckWindowState state;
  WnckWorkspace *active_ws;
  WnckWorkspace *window_ws;

  tasklist = task->tasklist;

  if (task->window == NULL)
    return;
      
  state = wnck_window_get_state (task->window);

  active_ws = wnck_screen_get_active_workspace (tasklist->priv->screen);
  window_ws = wnck_window_get_workspace (task->window);

  if (state & WNCK_WINDOW_STATE_MINIMIZED)
    {
      if (window_ws &&
          active_ws != window_ws &&
          !tasklist->priv->switch_workspace_on_unminimize)
        wnck_workspace_activate (window_ws, timestamp);

      wnck_window_activate_transient (task->window, timestamp);
    }
  else
    {
      if ((task->was_active || 
           wnck_window_transient_is_most_recently_activated (task->window)) &&
          (!window_ws || active_ws == window_ws))
	{
	  task->was_active = FALSE;
	  wnck_window_minimize (task->window);
	  return;
	}
      else
	{
          /* FIXME: THIS IS SICK AND WRONG AND BUGGY.  See the end of
           * http://mail.gnome.org/archives/wm-spec-list/2005-July/msg00032.html
           * There should only be *one* activate call.
           */
          if (window_ws)
            wnck_workspace_activate (window_ws, timestamp);

          wnck_window_activate_transient (task->window, timestamp);
        }
    }
  

  if (tasklist->priv->activate_timeout_id)
    gtk_timeout_remove (tasklist->priv->activate_timeout_id);

  tasklist->priv->activate_timeout_id = 
    gtk_timeout_add (500, &wnck_tasklist_change_active_timeout, tasklist);

  wnck_tasklist_change_active_task (tasklist, task);
}

static void 
wnck_task_close_all (GtkMenuItem *menu_item,
 		     gpointer     data)
{
  WnckTask *task = WNCK_TASK (data);
  GList *l;

  l = task->windows;
  while (l)
    {
      WnckTask *child = WNCK_TASK (l->data);
      wnck_window_close (child->window, gtk_get_current_event_time ());
      l = l->next;
    }
}

static void
wnck_task_unminimize_all (GtkMenuItem *menu_item,
		          gpointer     data)
{
  WnckTask *task = WNCK_TASK (data);
  GList *l;

  l = task->windows;
  while (l)
    {
      WnckTask *child = WNCK_TASK (l->data);
      /* This is inside an activate callback, so gtk_get_current_event_time()
       * will work.
       */
      wnck_window_unminimize (child->window, gtk_get_current_event_time ());
      l = l->next;
    }
}

static void 
wnck_task_minimize_all (GtkMenuItem *menu_item,
  		        gpointer     data)
{
  WnckTask *task = WNCK_TASK (data);
  GList *l;

  l = task->windows;
  while (l)
    {
      WnckTask *child = WNCK_TASK (l->data);
      wnck_window_minimize (child->window);
      l = l->next;
    }
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
      
      text = wnck_task_get_text (win_task, FALSE);
      menu_item = gtk_image_menu_item_new_with_label (text);
      if (wnck_task_get_needs_attention (win_task)) 
        _make_gtk_label_bold (GTK_LABEL (GTK_BIN (menu_item)->child));
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

  /* In case of Right click, show Close All, Minimize All, Unminimize All*/
  if (action_submenu) 
    {
      GtkWidget *separator;
      GtkWidget *image;
		
      separator = gtk_separator_menu_item_new ();
      gtk_widget_show (separator);
      gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), separator);

      menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Close All"));
      image = gtk_image_new_from_stock (WNCK_STOCK_DELETE, GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);    
      gtk_widget_show (menu_item);
      gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
      g_signal_connect_object (G_OBJECT (menu_item), "activate",
			       G_CALLBACK (wnck_task_close_all),
			       G_OBJECT (task),
			       0);

      separator = gtk_separator_menu_item_new ();
      gtk_widget_show (separator);
      gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), separator);
	    
      menu_item = gtk_image_menu_item_new_with_mnemonic (_("_Minimize All"));
      image = gtk_image_new_from_stock (WNCK_STOCK_MINIMIZE, GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);  	
      gtk_widget_show (menu_item);
      gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
      g_signal_connect_object (G_OBJECT (menu_item), "activate",
	    		       G_CALLBACK (wnck_task_minimize_all),
			       G_OBJECT (task),
			       0);
		
      menu_item =  gtk_image_menu_item_new_with_mnemonic (_("_Unminimize All"));
      gtk_widget_show (menu_item);
      gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);			
      g_signal_connect_object (G_OBJECT (menu_item), "activate",
  			       G_CALLBACK (wnck_task_unminimize_all),
			       G_OBJECT (task),
			       0);							
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

      /* This should only be called by clicking on the task button, so
       * gtk_get_current_event_time() should be fine here...
       */
      wnck_tasklist_activate_task_window (task, gtk_get_current_event_time ());
      break;
    case WNCK_TASK_STARTUP_SEQUENCE:
      break;
    }
}

static char *
wnck_task_get_text (WnckTask *task, gboolean icon_text)
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
      if (icon_text)
        name = wnck_window_get_icon_name (task->window);
      else 
        name = wnck_window_get_name (task->window);
       
      state = wnck_window_get_state (task->window);
      
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
wnck_task_get_needs_attention (WnckTask *task)
{
  GList *l;
  WnckTask *win_task;
  gboolean needs_attention;

  needs_attention = FALSE;

  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      l = task->windows;
      while (l)
	{
	  win_task = WNCK_TASK (l->data);

	  if (wnck_window_or_transient_needs_attention (win_task->window))
	    {
	      needs_attention = TRUE;
	      break;
	    }

	  l = l->next;
	}
      break;

    case WNCK_TASK_WINDOW:
      needs_attention =
	wnck_window_or_transient_needs_attention (task->window);
      break;

    case WNCK_TASK_STARTUP_SEQUENCE:
      break;
    }

  return needs_attention != FALSE;
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

  text = wnck_task_get_text (task, TRUE);
  if (text != NULL)
    {
      gtk_label_set_text (GTK_LABEL (task->label), text);
      if (wnck_task_get_needs_attention (task))
        {
          _make_gtk_label_bold ((GTK_LABEL (task->label)));
          wnck_task_queue_glow (task);
        }
      else
        {
          _make_gtk_label_normal ((GTK_LABEL (task->label)));
          wnck_task_stop_glow (task);
        }
      g_free (text);
      text = wnck_task_get_text (task, FALSE);
      if (text != NULL)
        {
          gtk_tooltips_set_tip (task->tasklist->priv->tooltips, task->button, text, NULL);
          g_free (text);
        }
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
  
  if ((changed_mask & WNCK_WINDOW_STATE_DEMANDS_ATTENTION) ||
      (changed_mask & WNCK_WINDOW_STATE_URGENT))
    {
      WnckWorkspace *active_workspace =
        wnck_screen_get_active_workspace (tasklist->priv->screen);

      if (active_workspace                              &&
          active_workspace != wnck_window_get_workspace (window))
        {
          wnck_tasklist_update_lists (tasklist);
          gtk_widget_queue_resize (GTK_WIDGET (tasklist));
        }
    }
    
  if ((changed_mask & WNCK_WINDOW_STATE_MINIMIZED)         ||
      (changed_mask & WNCK_WINDOW_STATE_DEMANDS_ATTENTION) ||
      (changed_mask & WNCK_WINDOW_STATE_URGENT))
    {
      WnckTask *win_task = NULL;

      /* FIXME: Handle group modal dialogs */
      for (; window && !win_task; window = wnck_window_get_transient (window))
        win_task = g_hash_table_lookup (tasklist->priv->win_hash, window);

      if (win_task)
	{
	  WnckTask *class_group_task;

	  wnck_task_update_visible_state (win_task);
	  
	  class_group_task =
            g_hash_table_lookup (tasklist->priv->class_group_hash,
                                 win_task->class_group);

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

  wnck_window_activate_transient (task->window, task->dnd_timestamp);

  task->dnd_timestamp = 0;

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
  task->dnd_timestamp = time;
  if (task->button_activate == 0 && task->type == WNCK_TASK_WINDOW)
      task->button_activate = g_timeout_add (WNCK_ACTIVATE_TIMEOUT,
                                             wnck_task_motion_timeout,
                                             task);
  gdk_drag_status (context,0,time);

  return TRUE;
}

void  
wnck_task_drag_begin (GtkWidget          *widget,
		      GdkDragContext     *context,
		      WnckTask           *task)
{
  _wnck_window_set_as_drag_icon (task->window, context, widget);
}

void  
wnck_task_drag_data_get (GtkWidget          *widget,
		         GdkDragContext     *context,
		         GtkSelectionData   *selection_data,
		         guint               info,
		 	 guint               time,
		         WnckTask           *task)
{
  gulong xid;    

  xid = wnck_window_get_xid (task->window);
  gtk_selection_data_set (selection_data,
 		          selection_data->target,
			  8, (guchar *)&xid, sizeof (gulong));
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

static gboolean
wnck_task_expose (GtkWidget        *widget,
                  GdkEventExpose   *event,
                  gpointer          data);

static void
wnck_task_create_widgets (WnckTask *task, GtkReliefStyle relief)
{
  GtkWidget *hbox;
  GdkPixbuf *pixbuf;
  char *text;
  static GQuark disable_sound_quark = 0;
  static const GtkTargetEntry targets[] = {
    { "application/x-wnck-window-id", 0, 0 }
  };
  
  if (!disable_sound_quark)
    disable_sound_quark = g_quark_from_static_string ("gnome_disable_sound_events");

  if (task->type == WNCK_TASK_STARTUP_SEQUENCE)
    task->button = gtk_button_new ();
  else
    task->button = gtk_toggle_button_new ();

  gtk_button_set_relief (GTK_BUTTON (task->button), relief);

  task->button_activate = 0;
  g_object_set_qdata (G_OBJECT (task->button),
		      disable_sound_quark, GINT_TO_POINTER (TRUE));
  g_object_add_weak_pointer (G_OBJECT (task->button),
                             (void**) &task->button);
  
  gtk_widget_set_name (task->button,
		       "tasklist-button");

  gtk_drag_dest_set (GTK_WIDGET(task->button), 0, NULL, 0, 0);

  if (task->type == WNCK_TASK_WINDOW)
    {
      gtk_drag_source_set (GTK_WIDGET (task->button),
		           GDK_BUTTON1_MASK,
		           targets, 1,
		           GDK_ACTION_MOVE);
    }

  hbox = gtk_hbox_new (FALSE, 0);

  pixbuf = wnck_task_get_icon (task);
  if (pixbuf)
    {
      task->image = gtk_image_new_from_pixbuf (pixbuf);
      g_object_unref (pixbuf);
    }
  else
    task->image = gtk_image_new ();
  
  gtk_widget_show (task->image);

  text = wnck_task_get_text (task, TRUE);
  task->label = gtk_label_new (text);
  gtk_misc_set_alignment (GTK_MISC (task->label), 0.0, 0.5);
  gtk_label_set_ellipsize (GTK_LABEL (task->label),
                          PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (GTK_LABEL (task->label), MAX_WIDTH_CHARS);

  if (wnck_task_get_needs_attention (task))
    {
      _make_gtk_label_bold ((GTK_LABEL (task->label)));
      wnck_task_queue_glow (task);
    }

  gtk_widget_show (task->label);

  gtk_box_pack_start (GTK_BOX (hbox), task->image, FALSE, FALSE, 4);
  gtk_box_pack_start (GTK_BOX (hbox), task->label, TRUE, TRUE, 2);

  gtk_container_add (GTK_CONTAINER (task->button), hbox);
  gtk_widget_show (hbox);
  g_free (text);
  
  text = wnck_task_get_text (task, FALSE);
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

  if (task->type == WNCK_TASK_WINDOW) {
      g_signal_connect_object (G_OBJECT(task->button), "drag_data_get",
                               G_CALLBACK (wnck_task_drag_data_get),
                               G_OBJECT (task),
                               0); 

      g_signal_connect_object (G_OBJECT(task->button), "drag_begin",
                               G_CALLBACK (wnck_task_drag_begin),
                               G_OBJECT (task),
                               0); 
  }
  
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

  g_signal_connect_object (task->button, "expose_event",
                           G_CALLBACK (wnck_task_expose),
                           G_OBJECT (task),
                           G_CONNECT_AFTER);
}

static void
draw_dot (GdkWindow *window, GdkGC *lgc, GdkGC *dgc, int x, int y)
{
  gdk_draw_point (window, dgc, x,   y);
  gdk_draw_point (window, lgc, x+1, y+1);
}

static void
fake_expose_widget (GtkWidget *widget,
                    GdkPixmap *pixmap,
                    gint       x,
                    gint       y)
{
  GdkWindow *tmp_window;
  GdkEventExpose event;

  event.type = GDK_EXPOSE;
  event.window = pixmap;
  event.send_event = FALSE;
  event.region = NULL;
  event.count = 0;

  tmp_window = widget->window;
  widget->window = pixmap;
  widget->allocation.x += x;
  widget->allocation.y += y;

  event.area = widget->allocation;

  gtk_widget_send_expose (widget, (GdkEvent *) &event);

  widget->window = tmp_window;
  widget->allocation.x -= x;
  widget->allocation.y -= y;
}

static GdkPixbuf*
take_screenshot (WnckTask *task, gint pos)
{
  WnckTasklist *tasklist;
  GtkWidget    *tasklist_widget;
  GdkGC *gc;
  GdkPixmap *pixmap;
  GdkPixbuf *screenshot;
  gint width, height;
  gboolean fade_using_state;
  
  width = task->button->allocation.width;
  height = task->button->allocation.height;
  
  pixmap = gdk_pixmap_new (task->button->window, width, height, -1);
  
  tasklist = WNCK_TASKLIST (task->tasklist);
  tasklist_widget = GTK_WIDGET (task->tasklist);


  gc = gdk_gc_new (pixmap);
  
  /* first draw the button */
  gtk_widget_style_get (tasklist_widget, "fade-using-state", &fade_using_state, NULL);
  if (!fade_using_state)
    {
      GdkColor *color;
      
      if (pos == 1)
        gtk_widget_style_get (tasklist_widget, "fade-color1", &color, NULL);
      else
        gtk_widget_style_get (tasklist_widget, "fade-color2", &color, NULL);
      
      if (color == NULL)
        color = gdk_color_copy (&task->button->style->base[GTK_STATE_SELECTED]);
        
      gdk_gc_set_rgb_fg_color (gc, color);
      gdk_draw_rectangle (pixmap, gc, TRUE, 0, 0, width + 1, height + 1);
      
      gdk_color_free (color);
    }
  else
    {
      GtkStateType state;
      if (pos == 1)
        gtk_widget_style_get (tasklist_widget, "fade-state1", &state, NULL);
      else
        gtk_widget_style_get (tasklist_widget, "fade-state2", &state, NULL);
      
      /* copy the background */
      gdk_draw_drawable (pixmap, gc, tasklist->priv->background,
                         task->button->allocation.x, task->button->allocation.y,
                         0, 0, width, height);
      
      /* draw the button */
      gtk_paint_box (task->button->style, (GdkWindow*) pixmap,
                     state, GTK_SHADOW_OUT, NULL, task->button, "button",
                     0, 0, width, height);
    }
  g_object_unref (gc);
  
  /* then the image and label */
  fake_expose_widget (task->image, pixmap,
                      -task->button->allocation.x, -task->button->allocation.y);
  fake_expose_widget (task->label, pixmap,
                      -task->button->allocation.x, -task->button->allocation.y);
  
  /* get the screenshot, and return */
  screenshot = gdk_pixbuf_get_from_drawable (NULL, pixmap, NULL, 0, 0,
                                             0, 0, width, height);
  g_object_unref (pixmap);
  
  return screenshot;
}

static gboolean
wnck_task_expose (GtkWidget        *widget,
                  GdkEventExpose   *event,
                  gpointer          data)
{
  GtkStyle *style;
  GdkGC *lgc, *dgc;
  int x, y, i, j;
  WnckTask *task;

  task = WNCK_TASK (data);
  
  cleanup_screenshots (task);
  
  switch (task->type)
    {
    case WNCK_TASK_CLASS_GROUP:
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

      /* Fall through to get screenshot
       */
    case WNCK_TASK_WINDOW:
      if ((event->area.x <= widget->allocation.x) &&
          (event->area.y <= widget->allocation.y) &&
          (event->area.width >= widget->allocation.width) &&
          (event->area.height >= widget->allocation.height))
        {
          if (task->button_glow != 0)
            {
              task->screenshot = gdk_pixbuf_get_from_drawable (NULL,
                                                               widget->window,
                                                               NULL,
                                                               widget->allocation.x, 
                                                               widget->allocation.y,
                                                               0, 0,
                                                               widget->allocation.width, 
                                                               widget->allocation.height);
              
              /* we also need to take screenshots for screenshot_fade1 and screenshot_fade2 */
              task->screenshot_fade1 = take_screenshot (task, 1);
              task->screenshot_fade2 = take_screenshot (task, 2);
            }
        }

    case WNCK_TASK_STARTUP_SEQUENCE:
      break;
    }

  return FALSE;
}

static gint
wnck_task_compare_alphabetically (gconstpointer a,
                                  gconstpointer b)
{
  char *text1;
  char *text2;
  gint  result;

  text1 = wnck_task_get_text (WNCK_TASK (a), FALSE);
  text2 = wnck_task_get_text (WNCK_TASK (b), FALSE);

  result= g_utf8_collate (text1, text2);

  g_free (text1);
  g_free (text2);

  return result;
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
  gint pos1, pos2;

  pos1 = pos2 = 0;  /* silence the compiler */

  switch (task1->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      if (task2->type == WNCK_TASK_CLASS_GROUP)
	return compare_class_group_tasks (task1, task2);
      else
	return -1; /* Sort groups before everything else */

    case WNCK_TASK_WINDOW:
      pos1 = wnck_window_get_sort_order (task1->window);
      break;
    case WNCK_TASK_STARTUP_SEQUENCE:
      pos1 = G_MAXINT; /* startup sequences are sorted at the end. */
      break;           /* Changing this will break scrolling.      */
    }

  switch (task2->type)
    {
    case WNCK_TASK_CLASS_GROUP:
      if (task1->type == WNCK_TASK_CLASS_GROUP)
	return compare_class_group_tasks (task1, task2);
      else
	return 1; /* Sort groups before everything else */

    case WNCK_TASK_WINDOW:
      pos2 = wnck_window_get_sort_order (task2->window);
      break;
    case WNCK_TASK_STARTUP_SEQUENCE:
      pos2 = G_MAXINT;
      break;
    }
      
  if (pos1 < pos2)
    return -1;
  else if (pos1 > pos2)
    return 1;
  else
    return 0; /* should only happen if there's multiple processes being
               * started, and then who cares about sort order... */
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
  
  wnck_task_create_widgets (task, tasklist->priv->relief);

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

  wnck_task_create_widgets (task, tasklist->priv->relief);

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
  
  wnck_task_create_widgets (task, tasklist->priv->relief);

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
