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

/**
 * SECTION: wnck-handle
 * @title: WnckHandle
 * @short_description: an object representing a handle.
 *
 * This is the main entry point into the libwnck library.
 */

#include "config.h"
#include "wnck-handle-private.h"

#include <X11/Xlib.h>
#ifdef HAVE_XRES
#include <X11/extensions/XRes.h>
#endif

#include "private.h"
#include "screen.h"
#include "window.h"
#include "wnck-enum-types.h"
#include "xutils.h"

/**
 * WnckHandle:
 *
 * The #WnckHandle struct contains only private fields and should not be
 * directly accessed.
 */
struct _WnckHandle
{
  GObject          parent;

  WnckScreen     **screens;

  WnckClientType   client_type;

  gboolean         have_xres;

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

static void
invalidate_icons (WnckHandle *self)
{
  Display *xdisplay;
  int i;

  if (self->screens == NULL)
    return;

  xdisplay = _wnck_get_default_display ();

  for (i = 0; i < ScreenCount (xdisplay); ++i)
    {
      WnckScreen *screen;
      GList *windows;
      GList *l;

      screen = self->screens[i];

      if (screen == NULL)
        continue;

      windows = wnck_screen_get_windows (screen);

      for (l = windows; l != NULL; l = l->next)
        {
          WnckWindow *window;
          WnckApplication *application;

          window = WNCK_WINDOW (l->data);
          application = wnck_window_get_application (window);

          _wnck_window_invalidate_icons (window);

          if (application != NULL)
            _wnck_application_invalidate_icons (application);
        }
    }
}

static GdkFilterReturn
filter_func (GdkXEvent *gdkxevent,
             GdkEvent  *event,
             gpointer   data)
{
  WnckHandle *self;
  XEvent *xevent = gdkxevent;

  self = WNCK_HANDLE (data);

  switch (xevent->type)
    {
    case PropertyNotify:
      {
        WnckScreen *screen;

        screen = wnck_handle_get_screen_for_root (self, xevent->xany.window);

        if (screen != NULL)
          {
            _wnck_screen_process_property_notify (screen, xevent);
          }
        else
          {
            WnckWindow *window;
            WnckApplication *app;

            window = wnck_handle_get_window (self, xevent->xany.window);
            app = wnck_handle_get_application (self, xevent->xany.window);

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

        window = wnck_handle_get_window (self, xevent->xconfigure.window);

        if (window)
          _wnck_window_process_configure_notify (window, xevent);
      }
      break;

    case SelectionClear:
      {
        _wnck_desktop_layout_manager_process_event (xevent);
      }
      break;

    default:
      break;
    }

  return GDK_FILTER_CONTINUE;
}

static void
init_xres (WnckHandle *self)
{
#ifdef HAVE_XRES
  Display *xdisplay;
  int event_base;
  int error_base;
  int major;
  int minor;

  xdisplay = _wnck_get_default_display ();

  if (xdisplay == NULL)
    return;

  event_base = error_base = major = minor = 0;

  if (XResQueryExtension (xdisplay, &event_base, &error_base) &&
      XResQueryVersion (xdisplay, &major, &minor) == 1)
    {
      if (major > 1 || (major == 1 && minor >= 2))
        self->have_xres = TRUE;
    }
#endif
}

static void
wnck_handle_constructed (GObject *object)
{
  WnckHandle *self;

  self = WNCK_HANDLE (object);

  G_OBJECT_CLASS (wnck_handle_parent_class)->constructed (object);

  init_xres (self);
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

      display = _wnck_get_default_display ();

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

  object_class->constructed = wnck_handle_constructed;
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
_wnck_handle_get_client_type (WnckHandle *self)
{
  return self->client_type;
}

gboolean
_wnck_handle_has_xres (WnckHandle *self)
{
  return self->have_xres;
}

/**
 * wnck_handle_get_default_screen:
 * @self: a #WnckHandle
 *
 * Gets the default #WnckScreen on the default display.
 *
 * Returns: (transfer none) (nullable): the default #WnckScreen. The
 * returned #WnckScreen is owned by #WnckHandle and must not be referenced
 * or unreferenced. This can return %NULL if not on X11.
 */
WnckScreen *
wnck_handle_get_default_screen (WnckHandle *self)
{
  Display *display;

  g_return_val_if_fail (WNCK_IS_HANDLE (self), NULL);

  display = _wnck_get_default_display ();
  if (display == NULL)
    return NULL;

  return wnck_handle_get_screen (self, DefaultScreen (display));
}

/**
 * wnck_handle_get_screen:
 * @self: a #WnckHandle
 * @index: screen number, starting from 0.
 *
 * Gets the #WnckScreen for a given screen on the default display.
 *
 * Returns: (transfer none): the #WnckScreen for screen @index, or %NULL
 * if no such screen exists. The returned #WnckScreen is owned by #WnckHandle
 * and must not be referenced or unreferenced.
 */
WnckScreen *
wnck_handle_get_screen (WnckHandle *self,
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

/**
 * wnck_handle_get_screen_for_root:
 * @self: a #WnckHandle
 * @root_window_id: an X window ID.
 *
 * Gets the #WnckScreen for the root window at @root_window_id, or
 * %NULL if no #WnckScreen exists for this root window.
 *
 * This function does not work if wnck_handle_get_screen() was not called
 * for the sought #WnckScreen before, and returns %NULL.
 *
 * Returns: (transfer none): the #WnckScreen for the root window at
 * @root_window_id, or %NULL. The returned #WnckScreen is owned by
 * #WnckHandle and must not be referenced or unreferenced.
 */
WnckScreen *
wnck_handle_get_screen_for_root (WnckHandle *self,
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

  if (self->default_icon_size == icon_size)
    return;

  self->default_icon_size = icon_size;

  invalidate_icons (self);
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

  if (self->default_mini_icon_size == icon_size)
    return;

  self->default_mini_icon_size = icon_size;

  invalidate_icons (self);
}

gsize
wnck_handle_get_default_mini_icon_size (WnckHandle *self)
{
  g_return_val_if_fail (WNCK_IS_HANDLE (self), WNCK_DEFAULT_MINI_ICON_SIZE);

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

/**
 * wnck_handle_get_class_group:
 * @self: a #WnckHandle
 * @id: identifier name of the sought resource class.
 *
 * Gets the #WnckClassGroup corresponding to @id.
 *
 * Returns: (transfer none): the #WnckClassGroup corresponding to
 * @id, or %NULL if there is no #WnckClassGroup with the specified
 * @id. The returned #WnckClassGroup is owned by libwnck and must not be
 * referenced or unreferenced.
 */
WnckClassGroup *
wnck_handle_get_class_group (WnckHandle *self,
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
_wnck_handle_get_application_from_res_class (WnckHandle *self,
                                             const char *res_class)
{
  GHashTableIter iter;
  gpointer value;

  if (res_class == NULL || *res_class == '\0')
    return NULL;

  g_hash_table_iter_init (&iter, self->app_hash);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      WnckApplication *app;
      GList *windows;
      const char *window_res_class;

      app = WNCK_APPLICATION (value);

      windows = wnck_application_get_windows (app);
      window_res_class = wnck_window_get_class_group_name (windows->data);

      if (g_strcmp0 (res_class, window_res_class) == 0)
        return app;
    }

  return NULL;
}

/**
 * wnck_handle_get_application:
 * @self: a #WnckHandle
 * @xwindow: the X window ID of a group leader.
 *
 * Gets the #WnckApplication corresponding to the group leader with @xwindow
 * as X window ID.
 *
 * Returns: (transfer none): the #WnckApplication corresponding to
 * @xwindow, or %NULL if there no such #WnckApplication could be found. The
 * returned #WnckApplication is owned by libwnck and must not be referenced or
 * unreferenced.
 */
WnckApplication *
wnck_handle_get_application (WnckHandle *self,
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

/**
 * wnck_handle_get_window:
 * @self: a #WnckHandle
 * @xwindow: an X window ID.
 *
 * Gets a preexisting #WnckWindow for the X window @xwindow. This will not
 * create a #WnckWindow if none exists. The function is robust against bogus
 * window IDs.
 *
 * Returns: (transfer none): the #WnckWindow for @xwindow. The returned
 * #WnckWindow is owned by libwnck and must not be referenced or unreferenced.
 */
WnckWindow *
wnck_handle_get_window (WnckHandle *self,
                        gulong      xwindow)
{
  g_return_val_if_fail (WNCK_IS_HANDLE (self), NULL);

  return g_hash_table_lookup (self->window_hash, &xwindow);
}
