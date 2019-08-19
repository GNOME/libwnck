/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2005-2007 Vincent Untz
 * Copyright (C) 2019 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION: wnck-handle
 * @short_description: an object representing a handle.
 *
 * This is the main entry point into the libwnck library.
 */

#include "config.h"
#include "wnck-handle-private.h"

#include "private.h"
#include "screen.h"
#include "window.h"
#include "wnck-enum-types.h"
#include "xutils.h"

struct _WnckHandle
{
  GObject        parent;

  WnckClientType client_type;

  gsize          default_icon_size;
  gsize          default_mini_icon_size;
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
  XEvent *xevent = gdkxevent;
#ifdef HAVE_STARTUP_NOTIFICATION
  int i;
  Display *display;
#endif /* HAVE_STARTUP_NOTIFICATION */

  switch (xevent->type)
    {
    case PropertyNotify:
      {
        WnckScreen *screen;

        screen = wnck_screen_get_for_root (xevent->xany.window);
        if (screen != NULL)
          _wnck_screen_process_property_notify (screen, xevent);
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
          WnckScreen *s;

          s = _wnck_screen_get_existing (i);
          if (s != NULL)
            sn_display_process_event (_wnck_screen_get_sn_display (s),
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
    g_param_spec_enum ("client-type", "client-type", "client-type",
                       WNCK_TYPE_CLIENT_TYPE,
                       WNCK_CLIENT_TYPE_APPLICATION,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, handle_properties);
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

  gdk_window_add_filter (NULL, filter_func, self);
}

/**
 * wnck_handle_new:
 * @client_type: a role for the client
 *
 * Creates a new #WnckHandle object with a given @client_type.
 *
 * Returns: (transfer full): newly created #WnckHandle.
 */
WnckHandle *
wnck_handle_new (WnckClientType client_type)
{
  return g_object_new (WNCK_TYPE_HANDLE,
                       "client-type", client_type,
                       NULL);
}

WnckClientType
wnck_handle_get_client_type (WnckHandle *self)
{
  g_return_val_if_fail (WNCK_IS_HANDLE (self), WNCK_CLIENT_TYPE_APPLICATION);

  return self->client_type;
}

/**
 * wnck_handle_set_default_icon_size:
 * @self: a #WnckHandle
 * @icon_size: the default size for windows and application standard icons.
 *
 * The default main icon size is %WNCK_DEFAULT_ICON_SIZE. This function allows
 * to change this value.
 */
void
wnck_handle_set_default_icon_size (WnckHandle *self,
                                   gsize       icon_size)
{
  g_return_if_fail (WNCK_IS_HANDLE (self));

  self->default_icon_size = icon_size;
}

gsize
wnck_handle_get_default_icon_size (WnckHandle *self)
{
  g_return_val_if_fail (WNCK_IS_HANDLE (self), WNCK_DEFAULT_ICON_SIZE);

  return self->default_icon_size;
}

/**
 * wnck_handle_set_default_mini_icon_size:
 * @self: a #WnckHandle
 * @icon_size: the default size for windows and application mini icons.
 *
 * The default main icon size is %WNCK_DEFAULT_MINI_ICON_SIZE. This function
 * allows to change this value.
 */
void
wnck_handle_set_default_mini_icon_size (WnckHandle *self,
                                        gsize       icon_size)
{
  g_return_if_fail (WNCK_IS_HANDLE (self));

  self->default_mini_icon_size = icon_size;
}

gsize
wnck_handle_get_default_mini_icon_size (WnckHandle *self)
{
  g_return_val_if_fail (WNCK_IS_HANDLE (self), WNCK_DEFAULT_MINI_ICON_SIZE);

  return self->default_mini_icon_size;
}
