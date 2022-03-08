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
#include "wnck-resource-usage-private.h"
#include <gdk/gdkx.h>
#include <string.h>

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
  _wnck_read_resource_usage_xid (gdisplay, xid, usage);
}

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
  _wnck_read_resource_usage_pid (gdisplay, pid, usage);
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

      wnck_handle = wnck_handle_new (client_type);
    }

  return wnck_handle;
}

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
  wnck_handle_set_default_icon_size (_wnck_get_handle(), size);
}

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
  wnck_handle_set_default_mini_icon_size (_wnck_get_handle (), size);
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

void
_wnck_init (void)
{
  static gboolean done = FALSE;

  if (!done)
    {
      bindtextdomain (GETTEXT_PACKAGE, WNCK_LOCALEDIR);
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

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
  g_clear_object (&wnck_handle);

  _wnck_read_resources_shutdown_all ();
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
