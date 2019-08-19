/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Kim Woelders
 * Copyright (C) 2003 Red Hat, Inc.
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

#include <gdk/gdkx.h>

#ifdef HAVE_XRES
#include <X11/extensions/XRes.h>
#endif

#include "private.h"
#include "screen.h"
#include "window.h"
#include "wnck-enum-types.h"
#include "xutils.h"

typedef struct
{
  WnckHandle *handle;

  XResClient *clients;
  int         n_clients;
  int         next;
  Display    *xdisplay;
  GHashTable *hashtable_pid;
} xresclient_state;

typedef enum
{
  WNCK_EXT_UNKNOWN = 0,
  WNCK_EXT_FOUND = 1,
  WNCK_EXT_MISSING = 2
} WnckExtStatus;

struct _WnckHandle
{
  GObject            parent;

  WnckScreen       **screens;

  WnckClientType     client_type;

  gsize              default_icon_size;
  gsize              default_mini_icon_size;

  xresclient_state   xres_state;
  guint              xres_idleid;
  GHashTable        *xres_hashtable;
  time_t             start_update;
  time_t             end_update;
  guint              xres_removeid;
};

enum
{
  PROP_0,

  PROP_CLIENT_TYPE,

  LAST_PROP
};

static GParamSpec *handle_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (WnckHandle, wnck_handle, G_TYPE_OBJECT)

static WnckExtStatus
wnck_init_resource_usage (GdkDisplay *gdisplay)
{
  WnckExtStatus status;

  status = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (gdisplay),
                                               "wnck-xres-status"));

  if (status == WNCK_EXT_UNKNOWN)
    {
#ifdef HAVE_XRES
      Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdisplay);
      int event, error;

      if (!XResQueryExtension (xdisplay, &event, &error))
        status = WNCK_EXT_MISSING;
      else
        status = WNCK_EXT_FOUND;
#else
      status = WNCK_EXT_MISSING;
#endif

      g_object_set_data (G_OBJECT (gdisplay),
                         "wnck-xres-status",
                         GINT_TO_POINTER (status));
    }

  g_assert (status != WNCK_EXT_UNKNOWN);

  return status;
}

#ifdef HAVE_XRES
static void
wnck_pid_read_resource_usage_free_hash (gpointer data)
{
  g_slice_free (gulong, data);
}

static guint
wnck_gulong_hash (gconstpointer v)
{
  /* FIXME: this is obvioulsy wrong, but nearly 100% of the time, the gulong
   * only contains guint values */
  return *(const guint *) v;
}

static gboolean
wnck_gulong_equal (gconstpointer a,
                   gconstpointer b)
{
  return *((const gulong *) a) == *((const gulong *) b);
}

static gulong
wnck_check_window_for_pid (Screen *screen,
                           Window  win,
                           XID     match_xid,
                           XID     mask)
{
  if ((win & ~mask) == match_xid) {
    return _wnck_get_pid (screen, win);
  }

  return 0;
}

static void
wnck_find_pid_for_resource_r (Display *xdisplay,
                              Screen  *screen,
                              Window   win_top,
                              XID      match_xid,
                              XID      mask,
                              gulong  *xid,
                              gulong  *pid)
{
  Status   qtres;
  int      err;
  Window   dummy;
  Window  *children;
  guint    n_children;
  guint    i;
  gulong   found_pid = 0;

  while (gtk_events_pending ())
    gtk_main_iteration ();

  found_pid = wnck_check_window_for_pid (screen, win_top, match_xid, mask);
  if (found_pid != 0)
    {
      *xid = win_top;
      *pid = found_pid;
    }

  _wnck_error_trap_push (xdisplay);
  qtres = XQueryTree (xdisplay, win_top, &dummy, &dummy,
                      &children, &n_children);
  err = _wnck_error_trap_pop (xdisplay);

  if (!qtres || err != Success)
    return;

  for (i = 0; i < n_children; i++)
    {
      wnck_find_pid_for_resource_r (xdisplay, screen, children[i],
                                    match_xid, mask, xid, pid);

      if (*pid != 0)
	break;
    }

  if (children)
    XFree ((char *)children);
}

static void
wnck_pid_read_resource_usage_xres_state_free (gpointer data)
{
  xresclient_state *state;

  state = (xresclient_state *) data;

  if (state->clients)
    XFree (state->clients);
  state->clients = NULL;

  state->n_clients = 0;
  state->next = -1;
  state->xdisplay = NULL;

  if (state->hashtable_pid)
    g_hash_table_destroy (state->hashtable_pid);
  state->hashtable_pid = NULL;
}

static gboolean
wnck_pid_read_resource_usage_fill_cache (xresclient_state *state)
{
  WnckHandle *self;
  int    i;
  gulong pid;
  gulong xid;
  XID    match_xid;

  self = state->handle;

  if (state->next >= state->n_clients)
    {
      if (self->xres_hashtable)
        g_hash_table_destroy (self->xres_hashtable);
      self->xres_hashtable = state->hashtable_pid;
      state->hashtable_pid = NULL;

      time (&self->end_update);

      self->xres_idleid = 0;
      return FALSE;
    }

  match_xid = (state->clients[state->next].resource_base &
               ~state->clients[state->next].resource_mask);

  pid = 0;
  xid = 0;

  for (i = 0; i < ScreenCount (state->xdisplay); i++)
    {
      Screen *screen;
      Window  root;

      screen = ScreenOfDisplay (state->xdisplay, i);
      root = RootWindow (state->xdisplay, i);

      if (root == None)
        continue;

      wnck_find_pid_for_resource_r (state->xdisplay, screen, root, match_xid,
                                    state->clients[state->next].resource_mask,
                                    &xid, &pid);

      if (pid != 0 && xid != 0)
        break;
    }

  if (pid != 0 && xid != 0)
    {
      gulong *key;
      gulong *value;

      key = g_slice_new (gulong);
      value = g_slice_new (gulong);
      *key = pid;
      *value = xid;
      g_hash_table_insert (state->hashtable_pid, key, value);
    }

  state->next++;

  return TRUE;
}

static void
wnck_pid_read_resource_usage_start_build_cache (WnckHandle *self,
                                                GdkDisplay *gdisplay)
{
  Display *xdisplay;
  int err;

  if (self->xres_idleid != 0)
    return;

  time (&self->start_update);

  xdisplay = GDK_DISPLAY_XDISPLAY (gdisplay);

  _wnck_error_trap_push (xdisplay);
  XResQueryClients (xdisplay, &self->xres_state.n_clients, &self->xres_state.clients);
  err = _wnck_error_trap_pop (xdisplay);

  if (err != Success)
    return;

  self->xres_state.next = (self->xres_state.n_clients > 0) ? 0 : -1;
  self->xres_state.xdisplay = xdisplay;
  self->xres_state.hashtable_pid = g_hash_table_new_full (wnck_gulong_hash,
                                                          wnck_gulong_equal,
                                                          wnck_pid_read_resource_usage_free_hash,
                                                          wnck_pid_read_resource_usage_free_hash);

  self->xres_idleid = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                                       (GSourceFunc) wnck_pid_read_resource_usage_fill_cache,
                                       &self->xres_state,
                                       wnck_pid_read_resource_usage_xres_state_free);
}

static gboolean
wnck_pid_read_resource_usage_destroy_hash_table (gpointer data)
{
  WnckHandle *self;

  self = WNCK_HANDLE (data);

  self->xres_removeid = 0;

  if (self->xres_hashtable)
    {
      g_hash_table_destroy (self->xres_hashtable);
      self->xres_hashtable = NULL;
    }

  return G_SOURCE_REMOVE;
}

#define XRES_UPDATE_RATE_SEC 30
static gboolean
wnck_pid_read_resource_usage_from_cache (WnckHandle        *self,
                                         GdkDisplay        *gdisplay,
                                         gulong             pid,
                                         WnckResourceUsage *usage)
{
  gboolean need_rebuild;
  gulong *xid_p;
  int cache_validity;

  if (self->end_update == 0)
    time (&self->end_update);

  cache_validity = MAX (XRES_UPDATE_RATE_SEC, (self->end_update - self->start_update) * 2);

  /* we rebuild the cache if it was never built or if it's old */
  need_rebuild = (self->xres_hashtable == NULL ||
                  (self->end_update < time (NULL) - cache_validity));

  if (self->xres_hashtable)
    {
      /* clear the cache after quite some time, because it might not be used
       * anymore */
      if (self->xres_removeid != 0)
        g_source_remove (self->xres_removeid);
      self->xres_removeid = g_timeout_add_seconds (cache_validity * 2,
                                                   wnck_pid_read_resource_usage_destroy_hash_table,
                                                   self);
    }

  if (need_rebuild)
    wnck_pid_read_resource_usage_start_build_cache (self, gdisplay);

  if (self->xres_hashtable)
    xid_p = g_hash_table_lookup (self->xres_hashtable, &pid);
  else
    xid_p = NULL;

  if (xid_p)
    {
      wnck_handle_read_resource_usage_xid (self, gdisplay, *xid_p, usage);
      return TRUE;
    }

  return FALSE;
}

static void
wnck_pid_read_resource_usage_no_cache (WnckHandle        *self,
                                       GdkDisplay        *gdisplay,
                                       gulong             pid,
                                       WnckResourceUsage *usage)
{
  Display *xdisplay;
  int i;

  xdisplay = GDK_DISPLAY_XDISPLAY (gdisplay);

  i = 0;
  while (i < ScreenCount (xdisplay))
    {
      WnckScreen *screen;
      GList *windows;
      GList *tmp;

      screen = wnck_handle_get_screen (self, i);

      g_assert (screen != NULL);

      windows = wnck_screen_get_windows (screen);
      tmp = windows;
      while (tmp != NULL)
        {
          if (wnck_window_get_pid (tmp->data) == (int) pid)
            {
              wnck_handle_read_resource_usage_xid (self, gdisplay,
                                                   wnck_window_get_xid (tmp->data),
                                                   usage);

              /* stop on first window found */
              return;
            }

          tmp = tmp->next;
        }

      ++i;
    }
}
#endif /* HAVE_XRES */

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

        screen = wnck_handle_get_screen_for_root (self, xevent->xany.window);

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
          WnckScreen *screen;

          screen = wnck_handle_get_existing_screen (self, i);

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
   * Shutting down all WnckScreen objects will automatically unreference (and
   * finalize) all WnckWindow objects, but not the WnckClassGroup and
   * WnckApplication objects.
   * Therefore we need to manually shut down all WnckClassGroup and
   * WnckApplication objects first, since they reference the WnckScreen they're
   * on.
   * On the other side, shutting down the WnckScreen objects will results in
   * all WnckWindow objects getting unreferenced and finalized, and must
   * actually be done before shutting down global WnckWindow structures
   * (because the WnckScreen has a list of WnckWindow that will get mis-used
   * otherwise). */
  _wnck_class_group_shutdown_all ();
  _wnck_application_shutdown_all ();
  _wnck_window_shutdown_all ();

  if (self->screens != NULL)
    {
      Display *display;
      int i;

      for (i = 0; i < ScreenCount (display); ++i)
        g_clear_object (&self->screens[i]);

      g_clear_pointer (&self->screens, g_free);
    }

#ifdef HAVE_XRES
  if (self->xres_removeid != 0)
    {
      g_source_remove (self->xres_removeid);
      self->xres_removeid = 0;
    }

  wnck_pid_read_resource_usage_destroy_hash_table (self);
#endif

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

  self->xres_state = (xresclient_state) { self, NULL, 0, -1, NULL, NULL };

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

WnckScreen *
wnck_handle_get_existing_screen (WnckHandle *self,
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

/**
 * wnck_handle_read_resource_usage_xid:
 * @self: a #WnckHandle
 * @gdk_display: a <classname>GdkDisplay</classname>.
 * @xid: an X window ID.
 * @usage: return location for the X resource usage of the application owning
 * the X window ID @xid.
 *
 * Looks for the X resource usage of the application owning the X window ID
 * @xid on display @gdk_display. If no resource usage can be found, then all
 * fields of @usage are set to 0.
 *
 * To properly work, this function requires the XRes extension on the X server.
 */
void
wnck_handle_read_resource_usage_xid (WnckHandle        *self,
                                     GdkDisplay        *gdk_display,
                                     gulong             xid,
                                     WnckResourceUsage *usage)
{
  g_return_if_fail (WNCK_IS_HANDLE (self));
  g_return_if_fail (usage != NULL);

  memset (usage, '\0', sizeof (*usage));

  if (wnck_init_resource_usage (gdk_display) == WNCK_EXT_MISSING)
    return;

#ifdef HAVE_XRES
 {
   Display *xdisplay;
   XResType *types;
   int n_types;
   unsigned long pixmap_bytes;
   int i;
   Atom pixmap_atom;
   Atom window_atom;
   Atom gc_atom;
   Atom picture_atom;
   Atom glyphset_atom;
   Atom font_atom;
   Atom colormap_entry_atom;
   Atom passive_grab_atom;
   Atom cursor_atom;

   types = NULL;
   n_types = 0;
   pixmap_bytes = 0;

  xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display);

  _wnck_error_trap_push (xdisplay);

  XResQueryClientResources (xdisplay,
                             xid, &n_types,
                             &types);

   XResQueryClientPixmapBytes (xdisplay,
                               xid, &pixmap_bytes);
   _wnck_error_trap_pop (xdisplay);

   usage->pixmap_bytes = pixmap_bytes;

   pixmap_atom = _wnck_atom_get ("PIXMAP");
   window_atom = _wnck_atom_get ("WINDOW");
   gc_atom = _wnck_atom_get ("GC");
   font_atom = _wnck_atom_get ("FONT");
   glyphset_atom = _wnck_atom_get ("GLYPHSET");
   picture_atom = _wnck_atom_get ("PICTURE");
   colormap_entry_atom = _wnck_atom_get ("COLORMAP ENTRY");
   passive_grab_atom = _wnck_atom_get ("PASSIVE GRAB");
   cursor_atom = _wnck_atom_get ("CURSOR");

   i = 0;
   while (i < n_types)
     {
       guint t = types[i].resource_type;

       if (t == pixmap_atom)
         usage->n_pixmaps += types[i].count;
       else if (t == window_atom)
         usage->n_windows += types[i].count;
       else if (t == gc_atom)
         usage->n_gcs += types[i].count;
       else if (t == picture_atom)
         usage->n_pictures += types[i].count;
       else if (t == glyphset_atom)
         usage->n_glyphsets += types[i].count;
       else if (t == font_atom)
         usage->n_fonts += types[i].count;
       else if (t == colormap_entry_atom)
         usage->n_colormap_entries += types[i].count;
       else if (t == passive_grab_atom)
         usage->n_passive_grabs += types[i].count;
       else if (t == cursor_atom)
         usage->n_cursors += types[i].count;
       else
         usage->n_other += types[i].count;

       ++i;
     }

   XFree(types);

   usage->total_bytes_estimate = usage->pixmap_bytes;

   /* FIXME look in the X server source and come up with better
    * answers here. Ideally we change XRes to return a number
    * like this since it can do things like divide the cost of
    * a shared resource among those sharing it.
    */
   usage->total_bytes_estimate += usage->n_windows * 24;
   usage->total_bytes_estimate += usage->n_gcs * 24;
   usage->total_bytes_estimate += usage->n_pictures * 24;
   usage->total_bytes_estimate += usage->n_glyphsets * 24;
   usage->total_bytes_estimate += usage->n_fonts * 1024;
   usage->total_bytes_estimate += usage->n_colormap_entries * 24;
   usage->total_bytes_estimate += usage->n_passive_grabs * 24;
   usage->total_bytes_estimate += usage->n_cursors * 24;
   usage->total_bytes_estimate += usage->n_other * 24;
 }
#else /* HAVE_XRES */
  g_assert_not_reached ();
#endif /* HAVE_XRES */
}

/**
 * wnck_handle_read_resource_usage_pid:
 * @self: a #WnckHandle
 * @gdk_display: a <classname>GdkDisplay</classname>.
 * @pid: a process ID.
 * @usage: return location for the X resource usage of the application with
 * process ID @pid.
 *
 * Looks for the X resource usage of the application with process ID @pid on
 * display @gdk_display. If no resource usage can be found, then all fields of
 * @usage are set to 0.
 *
 * In order to find the resource usage of an application that does not have an
 * X window visible to libwnck (panel applets do not have any toplevel windows,
 * for example), wnck_pid_read_resource_usage() walks through the whole tree of
 * X windows. Since this walk is expensive in CPU, a cache is created. This
 * cache is updated in the background. This means there is a non-null
 * probability that no resource usage will be found for an application, even if
 * it is an X client. If this happens, calling wnck_pid_read_resource_usage()
 * again after a few seconds should work.
 *
 * To properly work, this function requires the XRes extension on the X server.
 */
void
wnck_handle_read_resource_usage_pid (WnckHandle        *self,
                                     GdkDisplay        *gdk_display,
                                     gulong             pid,
                                     WnckResourceUsage *usage)
{
  g_return_if_fail (WNCK_IS_HANDLE (self));
  g_return_if_fail (usage != NULL);

  memset (usage, '\0', sizeof (*usage));

  if (wnck_init_resource_usage (gdk_display) == WNCK_EXT_MISSING)
    return;

#ifdef HAVE_XRES
  if (!wnck_pid_read_resource_usage_from_cache (self, gdk_display, pid, usage))
    /* the cache might not be built, might be outdated or might not contain
     * data for a new X client, so try to fallback to something else */
    wnck_pid_read_resource_usage_no_cache (self, gdk_display, pid, usage);
#endif /* HAVE_XRES */
}
