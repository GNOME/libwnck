/* application object */

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

#include "application.h"
#include "private.h"

#define FALLBACK_NAME _("untitled application")

static GHashTable *app_hash = NULL;

struct _WnckApplicationPrivate
{
  Window xwindow; /* group leader */
  WnckScreen *screen;
  GList *windows;
  int pid;
  char *name;

  WnckWindow *name_window;    /* window we are using name of */

  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;
  
  WnckIconCache *icon_cache;

  WnckWindow *icon_window;

  char *startup_id;
  
  guint name_from_leader : 1; /* name is from group leader */
  guint icon_from_leader : 1;
  
  guint need_emit_icon_changed : 1;
};

enum {
  NAME_CHANGED,
  ICON_CHANGED,
  LAST_SIGNAL
};

static void emit_name_changed (WnckApplication *app);
static void emit_icon_changed (WnckApplication *app);

static void reset_name  (WnckApplication *app);
static void update_name (WnckApplication *app);

static void wnck_application_init        (WnckApplication      *application);
static void wnck_application_class_init  (WnckApplicationClass *klass);
static void wnck_application_finalize    (GObject        *object);


static gpointer parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

GType
wnck_application_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();
  
  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (WnckApplicationClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) wnck_application_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (WnckApplication),
        0,              /* n_preallocs */
        (GInstanceInitFunc) wnck_application_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "WnckApplication",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
wnck_application_init (WnckApplication *application)
{  
  application->priv = g_new0 (WnckApplicationPrivate, 1);

  application->priv->icon_cache = _wnck_icon_cache_new ();
}

static void
wnck_application_class_init (WnckApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = wnck_application_finalize;
  
  signals[NAME_CHANGED] =
    g_signal_new ("name_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckApplicationClass, name_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[ICON_CHANGED] =
    g_signal_new ("icon_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckApplicationClass, icon_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
wnck_application_finalize (GObject *object)
{
  WnckApplication *application;

  application = WNCK_APPLICATION (object);

  if (application->priv->icon)
    g_object_unref (G_OBJECT (application->priv->icon));

  if (application->priv->mini_icon)
    g_object_unref (G_OBJECT (application->priv->mini_icon));
  
  _wnck_icon_cache_free (application->priv->icon_cache);
  
  g_free (application->priv->name);
  
  g_free (application->priv);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

WnckApplication*
wnck_application_get (gulong xwindow)
{
  if (app_hash == NULL)
    return NULL;
  else
    return g_hash_table_lookup (app_hash, &xwindow);
}

/**
 * wnck_application_get_xid:
 * @app: a #WnckApplication
 * 
 * Gets the X id of the group leader window for the app.
 * 
 * Return value: X id for the app
 **/
gulong
wnck_application_get_xid (WnckApplication *app)
{
  g_return_val_if_fail (WNCK_IS_APPLICATION (app), 0);
  
  return app->priv->xwindow;
}

/**
 * wnck_application_get_windows:
 * @app: a #WnckApplication
 * 
 * Gets a list of all windows belonging to @app. The list
 * is returned by reference and should not be freed.
 * 
 * Return value: list of #WnckWindow in this app
 **/
GList*
wnck_application_get_windows (WnckApplication *app)
{
  g_return_val_if_fail (WNCK_IS_APPLICATION (app), NULL);

  return app->priv->windows;
}

int
wnck_application_get_n_windows (WnckApplication *app)
{
  g_return_val_if_fail (WNCK_IS_APPLICATION (app), 0);

  return g_list_length (app->priv->windows);
}

/**
 * wnck_application_get_name:
 * @app: a #WnckApplication
 * 
 * Gets the name of an application, employing various
 * suboptimal heuristics to try to figure it out.
 * Probably GTK should have a function to allow apps to
 * set _NET_WM_NAME on the group leader as the app name,
 * and the WM spec should say that's where the app name
 * goes.
 * 
 * Return value: name of the application
 **/
const char*
wnck_application_get_name (WnckApplication *app)
{
  g_return_val_if_fail (WNCK_IS_APPLICATION (app), NULL);

  if (app->priv->name)
    return app->priv->name;
  else
    return FALLBACK_NAME;
}

/**
 * wnck_application_get_icon_name:
 * @app: a #WnckApplication
 * 
 * Gets the icon name of an application, employing various
 * suboptimal heuristics to try to figure it out.
 * 
 * Return value: name of the application when minimized
 **/
const char*
wnck_application_get_icon_name (WnckApplication *app)
{
  g_return_val_if_fail (WNCK_IS_APPLICATION (app), NULL);

  /* FIXME this isn't actually implemented, should be different
   * from regular name
   */
  
  if (app->priv->name)
    return app->priv->name;
  else
    return FALLBACK_NAME;
}

/**
 * wnck_application_get_pid:
 * @app: a #WnckApplication
 * 
 * Gets the process ID of an application, or 0 if none
 * is available.
 * 
 * Return value: process ID or 0
 **/
int
wnck_application_get_pid (WnckApplication *app)
{
  g_return_val_if_fail (WNCK_IS_APPLICATION (app), 0);

  return app->priv->pid;
}

static void
get_icons (WnckApplication *app)
{
  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;

  icon = NULL;
  mini_icon = NULL;
  
  if (_wnck_read_icons (app->priv->xwindow,
                        app->priv->icon_cache,
                        &icon,
                        DEFAULT_ICON_WIDTH, DEFAULT_ICON_HEIGHT,
                        &mini_icon,
                        DEFAULT_MINI_ICON_WIDTH,
                        DEFAULT_MINI_ICON_HEIGHT))
    {
      app->priv->need_emit_icon_changed = TRUE;
      app->priv->icon_from_leader = TRUE;
      
      if (icon)
        g_object_ref (G_OBJECT (icon));
      if (mini_icon)
        g_object_ref (G_OBJECT (mini_icon));

      if (app->priv->icon)
        g_object_unref (G_OBJECT (app->priv->icon));

      if (app->priv->mini_icon)
        g_object_unref (G_OBJECT (app->priv->mini_icon));

      app->priv->icon = icon;
      app->priv->mini_icon = mini_icon;
    }

  /* FIXME we should really fall back to using the icon
   * for one of the windows. But then we need to be more
   * complicated about icon_changed and when the icon
   * needs updating and all that.
   */
  
  g_assert ((app->priv->icon && app->priv->mini_icon) ||
            !(app->priv->icon || app->priv->mini_icon));
}

GdkPixbuf*
wnck_application_get_icon (WnckApplication *app)
{
  get_icons (app);
  if (app->priv->need_emit_icon_changed)
    emit_icon_changed (app);

  return app->priv->icon;
}

GdkPixbuf*
wnck_application_get_mini_icon (WnckApplication *app)
{
  get_icons (app);
  if (app->priv->need_emit_icon_changed)
    emit_icon_changed (app);

  return app->priv->mini_icon;
}

/**
 * wnck_application_get_icon_is_fallback:
 * @application: a #WnckApplication
 *
 * Checks if we are using a default fallback icon because
 * none was set on the application.
 * 
 * Return value: %TRUE if icon is a fallback
 **/
gboolean
wnck_application_get_icon_is_fallback (WnckApplication *app)
{
  g_return_val_if_fail (WNCK_IS_APPLICATION (app), FALSE);

  return _wnck_icon_cache_get_is_fallback (app->priv->icon_cache);
}

const char*
wnck_application_get_startup_id (WnckApplication *app)
{
  g_return_val_if_fail (WNCK_IS_APPLICATION (app), NULL);

  return app->priv->startup_id;
}

/* xwindow is a group leader */
WnckApplication*
_wnck_application_create (Window      xwindow,
                          WnckScreen *screen)
{
  WnckApplication *application;
  
  if (app_hash == NULL)
    app_hash = g_hash_table_new (_wnck_xid_hash, _wnck_xid_equal);
  
  g_return_val_if_fail (g_hash_table_lookup (app_hash, &xwindow) == NULL,
                        NULL);
  
  application = g_object_new (WNCK_TYPE_APPLICATION, NULL);
  application->priv->xwindow = xwindow;
  application->priv->screen = screen;

  application->priv->name = _wnck_get_name (xwindow);

  if (application->priv->name == NULL)
    application->priv->name = _wnck_get_res_class_utf8 (xwindow);
  
  if (application->priv->name)
    application->priv->name_from_leader = TRUE;
  
  application->priv->pid = _wnck_get_pid (application->priv->xwindow);

  application->priv->startup_id = _wnck_get_utf8_property (application->priv->xwindow,
                                                           _wnck_atom_get ("_NET_STARTUP_ID"));
  
  g_hash_table_insert (app_hash, &application->priv->xwindow, application);
  
  /* Hash now owns one ref, caller gets none */

  /* Note that xwindow may correspond to a WnckWindow's xwindow,
   * so we select events needed by either
   */
  _wnck_select_input (application->priv->xwindow,
                      WNCK_APP_WINDOW_EVENT_MASK);
  
  return application;
}

void
_wnck_application_destroy (WnckApplication *application)
{
  g_return_if_fail (wnck_application_get (application->priv->xwindow) == application);
  
  g_hash_table_remove (app_hash, &application->priv->xwindow);

  g_return_if_fail (wnck_application_get (application->priv->xwindow) == NULL);

  application->priv->xwindow = None;
  
  /* remove hash's ref on the application */
  g_object_unref (G_OBJECT (application));
}

static void
window_name_changed (WnckWindow      *window,
                     WnckApplication *app)
{
  if (window == app->priv->name_window)
    {
      reset_name (app);
      update_name (app);
    }
}

void
_wnck_application_add_window (WnckApplication *app,
                              WnckWindow      *window)
{
  g_return_if_fail (WNCK_IS_APPLICATION (app));
  g_return_if_fail (WNCK_IS_WINDOW (window));
  g_return_if_fail (wnck_window_get_application (window) == NULL);
  
  app->priv->windows = g_list_prepend (app->priv->windows, window);  
  _wnck_window_set_application (window, app);

  g_signal_connect (G_OBJECT (window), "name_changed",
                    G_CALLBACK (window_name_changed), app);
  
  /* emits signals, so do it last */
  reset_name (app);
  update_name (app);
}

void
_wnck_application_remove_window (WnckApplication *app,
                                 WnckWindow      *window)
{
  g_return_if_fail (WNCK_IS_APPLICATION (app));
  g_return_if_fail (WNCK_IS_WINDOW (window));
  g_return_if_fail (wnck_window_get_application (window) == app);
  
  app->priv->windows = g_list_remove (app->priv->windows, window);
  _wnck_window_set_application (window, NULL);

  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        window_name_changed, app);
  
  /* emits signals, so do it last */
  reset_name (app);
  update_name (app);
}

void
_wnck_application_process_property_notify (WnckApplication *app,
                                           XEvent          *xevent)
{
  /* This prop notify is on the leader window */
  
  if (xevent->xproperty.atom == XA_WM_NAME ||
      xevent->xproperty.atom ==
      _wnck_atom_get ("_NET_WM_NAME") ||
      xevent->xproperty.atom ==
      _wnck_atom_get ("_NET_WM_VISIBLE_NAME"))
    {
      /* FIXME should change the name */
    }
  else if (xevent->xproperty.atom ==
           XA_WM_ICON_NAME ||
           xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_WM_ICON_NAME") ||
           xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_WM_VISIBLE_ICON_NAME"))
    {
      /* FIXME */
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_WM_ICON") ||
           xevent->xproperty.atom ==
           _wnck_atom_get ("KWM_WIN_ICON") ||
           xevent->xproperty.atom ==
           _wnck_atom_get ("WM_NORMAL_HINTS"))
    {
      _wnck_icon_cache_property_changed (app->priv->icon_cache,
                                         xevent->xproperty.atom);
      emit_icon_changed (app);
    }
  else if (xevent->xproperty.atom ==
           _wnck_atom_get ("_NET_STARTUP_ID"))
    {
      /* FIXME update startup id */
    }
}

static void
emit_name_changed (WnckApplication *app)
{
  g_signal_emit (G_OBJECT (app),
                 signals[NAME_CHANGED],
                 0);
}

static void
emit_icon_changed (WnckApplication *app)
{
  app->priv->need_emit_icon_changed = FALSE;
  g_signal_emit (G_OBJECT (app),
                 signals[ICON_CHANGED],
                 0);
}

static void
reset_name  (WnckApplication *app)
{
  if (!app->priv->name_from_leader)
    {
      g_free (app->priv->name);
      app->priv->name = NULL;
      app->priv->name_window = NULL;
    }
}

static void
update_name (WnckApplication *app)
{
  if (app->priv->name == NULL)
    {
      /* if only one window, get name from there. If more than one and
       * they all have the same res_class, use that. Else we want to
       * use the fallback name, since using the title of one of the
       * windows would look wrong.
       */
      if (app->priv->windows &&
          app->priv->windows->next == NULL)
        {
          app->priv->name =
            g_strdup (wnck_window_get_name (app->priv->windows->data));
          app->priv->name_window = app->priv->windows->data;
          emit_name_changed (app);
        }
      else if (app->priv->windows)
        {
          /* more than one */
          app->priv->name =
            _wnck_get_res_class_utf8 (wnck_window_get_xid (app->priv->windows->data));
          if (app->priv->name)
            {
              app->priv->name_window = app->priv->windows->data;
              emit_name_changed (app);
            }
        }
    }
}
