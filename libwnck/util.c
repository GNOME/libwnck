/* util header */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2006-2007 Vincent Untz
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <glib/gi18n-lib.h>
#include "util.h"
#include "xutils.h"
#include "private.h"
#include <gdk/gdkx.h>
#include <string.h>
#ifdef HAVE_XRES
#include <X11/extensions/XRes.h>
#endif

/**
 * SECTION:resource
 * @short_description: reading resource usage of X clients.
 * @see_also: wnck_window_get_xid(), wnck_application_get_xid(), wnck_window_get_pid(), wnck_application_get_pid()
 * @stability: Unstable
 *
 * libwnck provides an easy-to-use interface to the XRes X server extension to
 * read resource usage of X clients, which can be defined either by the X
 * window ID of one of their windows or by the process ID of their process.
 */

/**
 * SECTION:misc
 * @short_description: other additional features.
 * @stability: Unstable
 *
 * These functions are utility functions providing some additional features to
 * libwnck users.
 */

/**
 * SECTION:icons
 * @short_description: icons related functions.
 * @stability: Unstable
 *
 * These functions are utility functions to manage icons for #WnckWindow and
 * #WnckApplication.
 */

typedef enum
{
  WNCK_EXT_UNKNOWN = 0,
  WNCK_EXT_FOUND = 1,
  WNCK_EXT_MISSING = 2
} WnckExtStatus;


#if 0
/* useful for debugging */
static void
_wnck_print_resource_usage (WnckResourceUsage *usage)
{
  if (!usage)
    return;

  g_print ("\twindows       : %d\n"
           "\tGCs           : %d\n"
           "\tfonts         : %d\n"
           "\tpixmaps       : %d\n"
           "\tpictures      : %d\n"
           "\tglyphsets     : %d\n"
           "\tcolormaps     : %d\n"
           "\tpassive grabs : %d\n"
           "\tcursors       : %d\n"
           "\tunknowns      : %d\n"
           "\tpixmap bytes  : %ld\n"
           "\ttotal bytes   : ~%ld\n",
           usage->n_windows,
           usage->n_gcs,
           usage->n_fonts,
           usage->n_pixmaps,
           usage->n_pictures,
           usage->n_glyphsets,
           usage->n_colormap_entries,
           usage->n_passive_grabs,
           usage->n_cursors,
           usage->n_other,
           usage->pixmap_bytes,
           usage->total_bytes_estimate);
}
#endif

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

/**
 * wnck_xid_read_resource_usage:
 * @gdk_display: a <classname>GdkDisplay</classname>.
 * @xid: an X window ID.
 * @usage: return location for the X resource usage of the application owning
 * the X window ID @xid.
 *
 * Looks for the X resource usage of the application owning the X window ID
 * @xid on display @gdisplay. If no resource usage can be found, then all
 * fields of @usage are set to 0.
 *
 * To properly work, this function requires the XRes extension on the X server.
 *
 * Since: 2.6
 */
void
wnck_xid_read_resource_usage (GdkDisplay        *gdisplay,
                              gulong             xid,
                              WnckResourceUsage *usage)
{
  g_return_if_fail (usage != NULL);

  memset (usage, '\0', sizeof (*usage));

  if (wnck_init_resource_usage (gdisplay) == WNCK_EXT_MISSING)
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

  xdisplay = GDK_DISPLAY_XDISPLAY (gdisplay);

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

struct xresclient_state
{
  XResClient *clients;
  int         n_clients;
  int         next;
  Display    *xdisplay;
  GHashTable *hashtable_pid;
};

static struct xresclient_state xres_state = { NULL, 0, -1, NULL, NULL };
static guint       xres_idleid = 0;
static GHashTable *xres_hashtable = NULL;
static time_t      start_update = 0;
static time_t      end_update = 0;
static guint       xres_removeid = 0;

static void
wnck_pid_read_resource_usage_xres_state_free (gpointer data)
{
  struct xresclient_state *state;

  state = (struct xresclient_state *) data;

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
wnck_pid_read_resource_usage_fill_cache (struct xresclient_state *state)
{
  int    i;
  gulong pid;
  gulong xid;
  XID    match_xid;

  if (state->next >= state->n_clients)
    {
      if (xres_hashtable)
        g_hash_table_destroy (xres_hashtable);
      xres_hashtable = state->hashtable_pid;
      state->hashtable_pid = NULL;

      time (&end_update);

      xres_idleid = 0;
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
wnck_pid_read_resource_usage_start_build_cache (GdkDisplay *gdisplay)
{
  Display *xdisplay;
  int      err;

  if (xres_idleid != 0)
    return;

  time (&start_update);

  xdisplay = GDK_DISPLAY_XDISPLAY (gdisplay);

  _wnck_error_trap_push (xdisplay);
  XResQueryClients (xdisplay, &xres_state.n_clients, &xres_state.clients);
  err = _wnck_error_trap_pop (xdisplay);

  if (err != Success)
    return;

  xres_state.next = (xres_state.n_clients > 0) ? 0 : -1;
  xres_state.xdisplay = xdisplay;
  xres_state.hashtable_pid = g_hash_table_new_full (
                                     wnck_gulong_hash,
                                     wnck_gulong_equal,
                                     wnck_pid_read_resource_usage_free_hash,
                                     wnck_pid_read_resource_usage_free_hash);

  xres_idleid = g_idle_add_full (
                        G_PRIORITY_HIGH_IDLE,
                        (GSourceFunc) wnck_pid_read_resource_usage_fill_cache,
                        &xres_state, wnck_pid_read_resource_usage_xres_state_free);
}

static gboolean
wnck_pid_read_resource_usage_destroy_hash_table (gpointer data)
{
  xres_removeid = 0;

  if (xres_hashtable)
    g_hash_table_destroy (xres_hashtable);

  xres_hashtable = NULL;

  return FALSE;
}

#define XRES_UPDATE_RATE_SEC 30
static gboolean
wnck_pid_read_resource_usage_from_cache (GdkDisplay        *gdisplay,
                                         gulong             pid,
                                         WnckResourceUsage *usage)
{
  gboolean  need_rebuild;
  gulong   *xid_p;
  int       cache_validity;

  if (end_update == 0)
    time (&end_update);

  cache_validity = MAX (XRES_UPDATE_RATE_SEC, (end_update - start_update) * 2);

  /* we rebuild the cache if it was never built or if it's old */
  need_rebuild = (xres_hashtable == NULL ||
                  (end_update < time (NULL) - cache_validity));

  if (xres_hashtable)
    {
      /* clear the cache after quite some time, because it might not be used
       * anymore */
      if (xres_removeid != 0)
        g_source_remove (xres_removeid);
      xres_removeid = g_timeout_add_seconds (cache_validity * 2,
                                             wnck_pid_read_resource_usage_destroy_hash_table,
                                             NULL);
    }

  if (need_rebuild)
    wnck_pid_read_resource_usage_start_build_cache (gdisplay);

  if (xres_hashtable)
    xid_p = g_hash_table_lookup (xres_hashtable, &pid);
  else
    xid_p = NULL;

  if (xid_p)
    {
      wnck_xid_read_resource_usage (gdisplay, *xid_p, usage);
      return TRUE;
    }

  return FALSE;
}

static void
wnck_pid_read_resource_usage_no_cache (GdkDisplay        *gdisplay,
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

      screen = wnck_screen_get (i);

      g_assert (screen != NULL);

      windows = wnck_screen_get_windows (screen);
      tmp = windows;
      while (tmp != NULL)
        {
          if (wnck_window_get_pid (tmp->data) == (int) pid)
            {
              wnck_xid_read_resource_usage (gdisplay,
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

/**
 * wnck_pid_read_resource_usage:
 * @gdk_display: a <classname>GdkDisplay</classname>.
 * @pid: a process ID.
 * @usage: return location for the X resource usage of the application with
 * process ID @pid.
 *
 * Looks for the X resource usage of the application with process ID @pid on
 * display @gdisplay. If no resource usage can be found, then all fields of
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
 *
 * Since: 2.6
 */
void
wnck_pid_read_resource_usage (GdkDisplay        *gdisplay,
                              gulong             pid,
                              WnckResourceUsage *usage)
{
  g_return_if_fail (usage != NULL);

  memset (usage, '\0', sizeof (*usage));

  if (wnck_init_resource_usage (gdisplay) == WNCK_EXT_MISSING)
    return;

#ifdef HAVE_XRES
  if (!wnck_pid_read_resource_usage_from_cache (gdisplay, pid, usage))
    /* the cache might not be built, might be outdated or might not contain
     * data for a new X client, so try to fallback to something else */
    wnck_pid_read_resource_usage_no_cache (gdisplay, pid, usage);
#endif /* HAVE_XRES */
}

static WnckClientType client_type = 0;

/**
 * wnck_set_client_type:
 * @ewmh_sourceindication_client_type: a role for the client.
 *
 * Sets the role of the libwnck user.
 *
 * The default role is %WNCK_CLIENT_TYPE_APPLICATION. Therefore, for
 * applications providing some window management features, like pagers or
 * tasklists, it is important to set the role to %WNCK_CLIENT_TYPE_PAGER for
 * libwnck to properly work.
 *
 * This function should only be called once per program. Additional calls
 * with the same client type will be silently ignored. An attempt to change
 * the client type to a differnet value after it has already been set will
 * be ignored and a critical warning will be logged.
 *
 * Since: 2.14
 */
void
wnck_set_client_type (WnckClientType ewmh_sourceindication_client_type)
{
  /* Clients constantly switching types makes no sense; this should only be
   * set once.
   */
  if (client_type != 0 && client_type != ewmh_sourceindication_client_type)
    g_critical ("wnck_set_client_type: changing the client type is not supported.\n");
  else
    client_type = ewmh_sourceindication_client_type;
}

static WnckHandle *wnck_handle = NULL;

WnckHandle *
_wnck_get_handle (void)
{
  if (wnck_handle == NULL)
    {
      /* If the type hasn't been set yet, use the default--treat it as a
       * normal application.
       */
      if (client_type == 0)
        client_type = WNCK_CLIENT_TYPE_APPLICATION;

      wnck_handle = _wnck_handle_new (client_type);
    }

  return wnck_handle;
}

WnckClientType
_wnck_get_client_type (void)
{
  return _wnck_handle_get_client_type (_wnck_get_handle ());
}

static gsize default_icon_size = WNCK_DEFAULT_ICON_SIZE;

/**
 * wnck_set_default_icon_size:
 * @size: the default size for windows and application standard icons.
 *
 * The default main icon size is %WNCK_DEFAULT_ICON_SIZE. This function allows
 * to change this value.
 *
 * Since: 2.4.6
 */
void
wnck_set_default_icon_size (gsize size)
{
  default_icon_size = size;
}

gsize
_wnck_get_default_icon_size (void)
{
  return default_icon_size;
}

static gsize default_mini_icon_size = WNCK_DEFAULT_MINI_ICON_SIZE;

/**
 * wnck_set_default_mini_icon_size:
 * @size: the default size for windows and application mini icons.
 *
 * The default main icon size is %WNCK_DEFAULT_MINI_ICON_SIZE. This function
 * allows to change this value.
 *
 * Since: 2.4.6
 */
void
wnck_set_default_mini_icon_size (gsize size)
{
  int default_screen;
  WnckScreen *screen;
  GList *l;

  default_mini_icon_size = size;

  default_screen = DefaultScreen (_wnck_get_default_display ());
  screen = _wnck_screen_get_existing (default_screen);

  if (WNCK_IS_SCREEN (screen))
    {
      /* Make applications and icons to reload their icons */
      for (l = wnck_screen_get_windows (screen); l; l = l->next)
        {
          WnckWindow *window = WNCK_WINDOW (l->data);
          WnckApplication *application = wnck_window_get_application (window);

          _wnck_window_load_icons (window);

          if (WNCK_IS_APPLICATION (application))
            _wnck_application_load_icons (application);
        }
    }
}

gsize
_wnck_get_default_mini_icon_size (void)
{
  return default_mini_icon_size;
}

/**
 * _make_gtk_label_bold:
 * @label: The label.
 *
 * Switches the font of label to a bold equivalent.
 **/
void
_make_gtk_label_bold (GtkLabel *label)
{
  GtkStyleContext *context;

  _wnck_ensure_fallback_style ();

  context = gtk_widget_get_style_context (GTK_WIDGET (label));
  gtk_style_context_add_class (context, "wnck-needs-attention");
}

void
_make_gtk_label_normal (GtkLabel *label)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (label));
  gtk_style_context_remove_class (context, "wnck-needs-attention");
}

#ifdef HAVE_STARTUP_NOTIFICATION
static gboolean
_wnck_util_sn_utf8_validator (const char *str,
                              int         max_len)
{
  return g_utf8_validate (str, max_len, NULL);
}
#endif /* HAVE_STARTUP_NOTIFICATION */

void
_wnck_init (void)
{
  static gboolean done = FALSE;

  if (!done)
    {
      bindtextdomain (GETTEXT_PACKAGE, WNCK_LOCALEDIR);
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

#ifdef HAVE_STARTUP_NOTIFICATION
      sn_set_utf8_validator (_wnck_util_sn_utf8_validator);
#endif /* HAVE_STARTUP_NOTIFICATION */

      done = TRUE;
    }
}

Display *
_wnck_get_default_display (void)
{
  GdkDisplay *display = gdk_display_get_default ();
  /* FIXME: when we fix libwnck to not use the GDK default display, we will
   * need to fix wnckprop accordingly. */
  if (!GDK_IS_X11_DISPLAY (display))
    {
      g_warning ("libwnck is designed to work in X11 only, no valid display found");
      return NULL;
    }

  return GDK_DISPLAY_XDISPLAY (display);
}

/**
 * wnck_shutdown:
 *
 * Makes libwnck stop listening to events and tear down all resources from
 * libwnck. This should be done if you are not going to need the state change
 * notifications for an extended period of time, to avoid wakeups with every
 * key and focus event.
 *
 * After this, all pointers to Wnck object you might still hold are invalid.
 *
 * Due to the fact that <link
 * linkend="getting-started.pitfalls.memory-management">Wnck objects are all
 * owned by libwnck</link>, users of this API through introspection should be
 * extremely careful: they must explicitly clear variables referencing objects
 * before this call. Failure to do so might result in crashes.
 *
 * Since: 3.4
 */
void
wnck_shutdown (void)
{
  _wnck_event_filter_shutdown ();

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
  _wnck_screen_shutdown_all ();
  _wnck_window_shutdown_all ();

  g_clear_object (&wnck_handle);

#ifdef HAVE_XRES
  if (xres_removeid != 0)
    g_source_remove (xres_removeid);
  xres_removeid = 0;
  wnck_pid_read_resource_usage_destroy_hash_table (NULL);
#endif
}

void
_wnck_ensure_fallback_style (void)
{
  static gboolean css_loaded = FALSE;
  GtkCssProvider *provider;
  guint priority;

  if (css_loaded)
    return;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/libwnck/wnck.css");

  priority = GTK_STYLE_PROVIDER_PRIORITY_FALLBACK;
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             priority);

  g_object_unref (provider);

  css_loaded = TRUE;
}
