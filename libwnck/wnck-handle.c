/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Kim Woelders
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2005-2007 Vincent Untz
 * Copyright (C) 2021 Alberts Muktupāvels
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "wnck-handle-private.h"

#include "private.h"
#include "screen.h"
#include "window.h"
#include "wnck-enum-types.h"
#include "xutils.h"

#define WNCK_TYPE_HANDLE (wnck_handle_get_type ())
G_DECLARE_FINAL_TYPE (WnckHandle, wnck_handle, WNCK, HANDLE, GObject)

struct _WnckHandle
{
  GObject          parent;

  WnckScreen     **screens;

  WnckClientType   client_type;

  gsize            default_icon_size;
  gsize            default_mini_icon_size;

  GHashTable      *class_group_hash;
  GHashTable      *app_hash;
  GHashTable      *window_hash;
};

enum
{
  PROP_0,

  PROP_CLIENT_TYPE,

  LAST_PROP
};

static GParamSpec *handle_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (WnckHandle, wnck_handle, G_TYPE_OBJECT)

static GdkFilterReturn
filter_func (GdkXEvent *gdkxevent,
             GdkEvent  *event,
             gpointer   data)
{
  WnckHandle *self;
  XEvent *xevent = gdkxevent;
#ifdef HAVE_STARTUP_NOTIFICATION
  int i;
  Display *display;
#endif /* HAVE_STARTUP_NOTIFICATION */

  self = WNCK_HANDLE (data);

  switch (xevent->type)
    {
    case PropertyNotify:
      {
        WnckScreen *screen;

        screen = _wnck_handle_get_screen_for_root (self, xevent->xany.window);

        if (screen != NULL)
          {
            _wnck_screen_process_property_notify (screen, xevent);
          }
        else
          {
            WnckWindow *window;
            WnckApplication *app;

            window = wnck_window_get (xevent->xany.window);
            app = wnck_application_get (xevent->xany.window);

            if (app)
              _wnck_application_process_property_notify (app, xevent);

            if (window)
              _wnck_window_process_property_notify (window, xevent);
          }
      }
      break;

    case ConfigureNotify:
      {
        WnckWindow *window;

        window = wnck_window_get (xevent->xconfigure.window);

        if (window)
          _wnck_window_process_configure_notify (window, xevent);
      }
      break;

    case SelectionClear:
      {
        _wnck_desktop_layout_manager_process_event (xevent);
      }
      break;

    case ClientMessage:
#ifdef HAVE_STARTUP_NOTIFICATION
      /* We're cheating as officially libsn requires
       * us to send all events through sn_display_process_event
       */
      i = 0;
      display = ((XAnyEvent *) xevent)->display;

      while (i < ScreenCount (display))
        {
          WnckScreen *screen;

          screen = _wnck_handle_get_existing_screen (self, i);

          if (screen != NULL)
            sn_display_process_event (_wnck_screen_get_sn_display (screen),
                                      xevent);

          ++i;
        }
#endif /* HAVE_STARTUP_NOTIFICATION */
      break;

    default:
      break;
    }

  return GDK_FILTER_CONTINUE;
}

static void
wnck_handle_finalize (GObject *object)
{
  WnckHandle *self;

  self = WNCK_HANDLE (object);

  gdk_window_remove_filter (NULL, filter_func, self);

  /* Warning: this is hacky :-)
   *
   * Shutting down all WnckScreen objects will automatically unreference
   * (and finalize) all WnckWindow objects, but not the WnckClassGroup and
   * WnckApplication objects.
   * Therefore we need to manually shut down all WnckClassGroup and
   * WnckApplication objects first, since they reference the WnckScreen they
   * are on.
   * On the other side, shutting down the WnckScreen objects will results in
   * all WnckWindow objects getting unreferenced and finalized, and must
   * actually be done before shutting down global WnckWindow structures
   * (because the WnckScreen has a list of WnckWindow that will get mis-used
   * otherwise).
   */

  if (self->class_group_hash != NULL)
    {
      g_hash_table_destroy (self->class_group_hash);
      self->class_group_hash = NULL;
    }

  if (self->app_hash != NULL)
    {
      g_hash_table_destroy (self->app_hash);
      self->app_hash = NULL;
    }

  if (self->screens != NULL)
    {
      Display *display;
      int i;

      for (i = 0; i < ScreenCount (display); ++i)
        g_clear_object (&self->screens[i]);

      g_clear_pointer (&self->screens, g_free);
    }

  if (self->window_hash != NULL)
    {
      g_hash_table_destroy (self->window_hash);
      self->window_hash = NULL;
    }

  G_OBJECT_CLASS (wnck_handle_parent_class)->finalize (object);
}

static void
wnck_handle_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  WnckHandle *self;

  self = WNCK_HANDLE (object);

  switch (property_id)
    {
      case PROP_CLIENT_TYPE:
        g_value_set_enum (value, self->client_type);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wnck_handle_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  WnckHandle *self;

  self = WNCK_HANDLE (object);

  switch (property_id)
    {
      case PROP_CLIENT_TYPE:
        self->client_type = g_value_get_enum (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  handle_properties[PROP_CLIENT_TYPE] =
    g_param_spec_enum ("client-type",
                       "client-type",
                       "client-type",
                       WNCK_TYPE_CLIENT_TYPE,
                       WNCK_CLIENT_TYPE_APPLICATION,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     LAST_PROP,
                                     handle_properties);
}

static void
wnck_handle_class_init (WnckHandleClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->finalize = wnck_handle_finalize;
  object_class->get_property = wnck_handle_get_property;
  object_class->set_property = wnck_handle_set_property;

  install_properties (object_class);
}

static void
wnck_handle_init (WnckHandle *self)
{
  self->default_icon_size = WNCK_DEFAULT_ICON_SIZE;
  self->default_mini_icon_size = WNCK_DEFAULT_MINI_ICON_SIZE;

  self->class_group_hash = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  NULL,
                                                  g_object_unref);

  self->app_hash = g_hash_table_new_full (_wnck_xid_hash,
                                          _wnck_xid_equal,
                                          NULL,
                                          g_object_unref);

  self->window_hash = g_hash_table_new_full (_wnck_xid_hash,
                                             _wnck_xid_equal,
                                             NULL,
                                             g_object_unref);

  gdk_window_add_filter (NULL, filter_func, self);
}

WnckHandle *
_wnck_handle_new (WnckClientType client_type)
{
  return g_object_new (WNCK_TYPE_HANDLE,
                       "client-type", client_type,
                       NULL);
}

WnckClientType
_wnck_handle_get_client_type (WnckHandle *self)
{
  return self->client_type;
}

WnckScreen *
_wnck_handle_get_default_screen (WnckHandle *self)
{
  Display *display;

  g_return_val_if_fail (WNCK_IS_HANDLE (self), NULL);

  display = _wnck_get_default_display ();
  if (display == NULL)
    return NULL;

  return _wnck_handle_get_screen (self, DefaultScreen (display));
}

WnckScreen *
_wnck_handle_get_screen (WnckHandle *self,
                         int         index)
{
  Display *display;

  display = _wnck_get_default_display ();

  g_return_val_if_fail (WNCK_IS_HANDLE (self), NULL);
  g_return_val_if_fail (display != NULL, NULL);

  if (index >= ScreenCount (display))
    return NULL;

  if (self->screens == NULL)
    self->screens = g_new0 (WnckScreen*, ScreenCount (display));

  if (self->screens[index] == NULL)
    {
      self->screens[index] = g_object_new (WNCK_TYPE_SCREEN, NULL);

      _wnck_screen_construct (self->screens[index], self, display, index);
    }

  return self->screens[index];
}

WnckScreen *
_wnck_handle_get_screen_for_root (WnckHandle *self,
                                  gulong      root_window_id)
{
  Display *display;
  int i;

  g_return_val_if_fail (WNCK_IS_HANDLE (self), NULL);

  if (self->screens == NULL)
    return NULL;

  display = _wnck_get_default_display ();
  i = 0;

  while (i < ScreenCount (display))
    {
      WnckScreen *screen;

      screen = self->screens[i];
      if (screen != NULL && _wnck_screen_get_xroot (screen) == root_window_id)
        return screen;

      ++i;
    }

  return NULL;
}

WnckScreen *
_wnck_handle_get_existing_screen (WnckHandle *self,
                                  int         number)
{
  Display *display;

  display = _wnck_get_default_display ();

  g_return_val_if_fail (WNCK_IS_HANDLE (self), NULL);
  g_return_val_if_fail (display != NULL, NULL);
  g_return_val_if_fail (number < ScreenCount (display), NULL);

  if (self->screens != NULL)
    return self->screens[number];

  return NULL;
}

void
_wnck_handle_set_default_icon_size (WnckHandle *self,
                                    gsize       icon_size)
{
  self->default_icon_size = icon_size;
}

gsize
_wnck_handle_get_default_icon_size (WnckHandle *self)
{
  return self->default_icon_size;
}

void
_wnck_handle_set_default_mini_icon_size (WnckHandle *self,
                                         gsize       icon_size)
{
  self->default_mini_icon_size = icon_size;
}

gsize
_wnck_handle_get_default_mini_icon_size (WnckHandle *self)
{
  return self->default_mini_icon_size;
}


void
_wnck_handle_insert_class_group (WnckHandle     *self,
                                 const char     *id,
                                 WnckClassGroup *class_group)
{
  g_return_if_fail (id != NULL);

  g_hash_table_insert (self->class_group_hash, (gpointer) id, class_group);
}

void
_wnck_handle_remove_class_group (WnckHandle *self,
                                 const char *id)
{
  g_hash_table_remove (self->class_group_hash, id);
}

WnckClassGroup *
_wnck_handle_get_class_group (WnckHandle *self,
                              const char *id)
{
  g_return_val_if_fail (WNCK_IS_HANDLE (self), NULL);

  return g_hash_table_lookup (self->class_group_hash, id ? id : "");
}

void
_wnck_handle_insert_application (WnckHandle      *self,
                                 gpointer         xwindow,
                                 WnckApplication *app)
{
  g_hash_table_insert (self->app_hash, xwindow, app);
}

void
_wnck_handle_remove_application (WnckHandle *self,
                                 gpointer    xwindow)
{
  g_hash_table_remove (self->app_hash, xwindow);
}

WnckApplication *
_wnck_handle_get_application (WnckHandle *self,
                              gulong      xwindow)
{
  g_return_val_if_fail (WNCK_IS_HANDLE (self), NULL);

  return g_hash_table_lookup (self->app_hash, &xwindow);
}

void
_wnck_handle_insert_window (WnckHandle *self,
                            gpointer    xwindow,
                            WnckWindow *window)
{
  g_hash_table_insert (self->window_hash, xwindow, window);
}

void
_wnck_handle_remove_window (WnckHandle *self,
                            gpointer    xwindow)
{
  g_hash_table_remove (self->window_hash, xwindow);
}

WnckWindow *
_wnck_handle_get_window (WnckHandle *self,
                         gulong      xwindow)
{
  g_return_val_if_fail (WNCK_IS_HANDLE (self), NULL);

  return g_hash_table_lookup (self->window_hash, &xwindow);
}
