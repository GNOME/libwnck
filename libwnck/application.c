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

static GHashTable *app_hash = NULL;

struct _WnckApplicationPrivate
{
  Window xwindow;
  WnckScreen *screen;
  GList *windows;
};

enum {
  
  LAST_SIGNAL
};

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

}

static void
wnck_application_class_init (WnckApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = wnck_application_finalize;

}

static void
wnck_application_finalize (GObject *object)
{
  WnckApplication *application;

  application = WNCK_APPLICATION (object);

  
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
  
  g_hash_table_insert (app_hash, &application->priv->xwindow, application);
  
  /* Hash now owns one ref, caller gets none */

  /* Note that xwindow may correspond to a WnckWindow's xwindow,
   * so we select events needed by either
   */
  _wnck_error_trap_push ();
  XSelectInput (gdk_display,
                application->priv->xwindow,
                WNCK_APP_WINDOW_EVENT_MASK);
  _wnck_error_trap_pop ();  
  
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

void
_wnck_application_add_window (WnckApplication *app,
                              WnckWindow      *window)
{
  g_return_if_fail (WNCK_IS_APPLICATION (app));
  g_return_if_fail (WNCK_IS_WINDOW (window));
  g_return_if_fail (wnck_window_get_application (window) == NULL);
  
  app->priv->windows = g_list_prepend (app->priv->windows, window);
  _wnck_window_set_application (window, app);
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
}

