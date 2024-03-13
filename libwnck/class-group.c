/* class group object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2003 Ximian, Inc.
 * Authors: Federico Mena-Quintero <federico@ximian.com>
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

#include <string.h>
#include "class-group.h"
#include "window.h"
#include "wnck-handle-private.h"
#include "private.h"

/**
 * SECTION:class-group
 * @short_description: an object representing a group of windows of the same
 * class.
 * @see_also: wnck_window_get_class_group()
 * @stability: Unstable
 *
 * The #WnckClassGroup is a group of #WnckWindow that are all in the same
 * class. It can be used to represent windows by classes, group windows by
 * classes or to manipulate all windows of a particular class.
 *
 * The class of a window is defined by the WM_CLASS property of this window.
 * More information about the WM_CLASS property is available in the <ulink
 * url="http://tronche.com/gui/x/icccm/sec-4.html&num;s-4.1.2.5">WM_CLASS Property</ulink>
 * section (section 4.1.2.5) of the <ulink
 * url="http://tronche.com/gui/x/icccm/">ICCCM</ulink>.
 *
 * The #WnckClassGroup objects are always owned by libwnck and must not be
 * referenced or unreferenced.
 */

/* Private part of the WnckClassGroup structure */
struct _WnckClassGroupPrivate {
  WnckScreen *screen;

  char *res_class;
  char *name;
  GList *windows;
  GHashTable *window_icon_handlers;
  GHashTable *window_name_handlers;

  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;
};

G_DEFINE_TYPE_WITH_PRIVATE (WnckClassGroup, wnck_class_group, G_TYPE_OBJECT);

static void wnck_class_group_finalize    (GObject             *object);

enum {
  NAME_CHANGED,
  ICON_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
get_icon_internal (WnckClassGroup  *self,
                   GdkPixbuf      **ret_icon,
                   int              icon_size)
{
  GdkPixbuf *icon;
  GList *l;

  icon = NULL;

  for (l = self->priv->windows; l; l = l->next)
    {
      WnckWindow *window;
      WnckApplication *app;

      window = WNCK_WINDOW (l->data);
      app = wnck_window_get_application (window);

      if (app == NULL)
        continue;

      icon = wnck_application_get_icon (app);

      if (icon != NULL)
        break;
    }

  if (icon == NULL)
    {
      for (l = self->priv->windows; l; l = l->next)
        {
          WnckWindow *window;

          window = WNCK_WINDOW (l->data);

          icon = wnck_window_get_icon (window);

          if (icon != NULL)
            break;
        }
    }

  if (icon != NULL)
    {
      *ret_icon = g_object_ref (icon);
    }
  else
    {
      *ret_icon = _wnck_get_fallback_icon (icon_size);
    }
}

static void
wnck_class_group_class_init (WnckClassGroupClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = wnck_class_group_finalize;

  /**
   * WnckClassGroup::name-changed:
   * @class_group: the #WnckClassGroup which emitted the signal.
   *
   * Emitted when the name of @class_group changes.
   */
  signals[NAME_CHANGED] =
    g_signal_new ("name_changed",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckClassGroupClass, name_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
  /**
   * WnckClassGroup::icon-changed:
   * @class_group: the #WnckClassGroup which emitted the signal.
   *
   * Emitted when the icon of @class_group changes.
   */
  signals[ICON_CHANGED] =
    g_signal_new ("icon_changed",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckClassGroupClass, icon_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
wnck_class_group_init (WnckClassGroup *class_group)
{
  class_group->priv = wnck_class_group_get_instance_private (class_group);
  class_group->priv->window_icon_handlers = g_hash_table_new (g_direct_hash,
                                                              g_direct_equal);
  class_group->priv->window_name_handlers = g_hash_table_new (g_direct_hash,
                                                              g_direct_equal);
}

static void
remove_signal_handler (gpointer key,
                       gpointer value,
                       gpointer user_data)
{
  g_signal_handler_disconnect (key, (gulong) value);
}

static void
wnck_class_group_finalize (GObject *object)
{
  WnckClassGroup *class_group;

  class_group = WNCK_CLASS_GROUP (object);

  if (class_group->priv->res_class)
    {
      g_free (class_group->priv->res_class);
      class_group->priv->res_class = NULL;
    }

  if (class_group->priv->name)
    {
      g_free (class_group->priv->name);
      class_group->priv->name = NULL;
    }

  if (class_group->priv->windows)
    {
      g_list_free (class_group->priv->windows);
      class_group->priv->windows = NULL;
    }

  if (class_group->priv->window_icon_handlers)
    {
      g_hash_table_foreach (class_group->priv->window_icon_handlers,
                            remove_signal_handler,
                            NULL);
      g_hash_table_destroy (class_group->priv->window_icon_handlers);
      class_group->priv->window_icon_handlers = NULL;
    }

  if (class_group->priv->window_name_handlers)
    {
      g_hash_table_foreach (class_group->priv->window_name_handlers,
                            remove_signal_handler,
                            NULL);
      g_hash_table_destroy (class_group->priv->window_name_handlers);
      class_group->priv->window_name_handlers = NULL;
    }

  if (class_group->priv->icon)
    {
      g_object_unref (class_group->priv->icon);
      class_group->priv->icon = NULL;
    }

  if (class_group->priv->mini_icon)
    {
      g_object_unref (class_group->priv->mini_icon);
      class_group->priv->mini_icon = NULL;
    }

  G_OBJECT_CLASS (wnck_class_group_parent_class)->finalize (object);
}

/**
 * _wnck_class_group_create:
 * @screen: a #WnckScreen.
 * @res_class: name of the resource class for the group.
 *
 * Creates a new WnckClassGroup with the specified resource class name.  If
 * @res_class is #NULL, then windows without a resource class name will get
 * grouped under this class group.
 *
 * Return value: a newly-created #WnckClassGroup, or an existing one that
 * matches the @res_class.
 **/
WnckClassGroup *
_wnck_class_group_create (WnckScreen *screen,
                          const char *res_class)
{
  WnckHandle *handle;
  WnckClassGroup *class_group;

  handle = wnck_screen_get_handle (screen);
  class_group = wnck_handle_get_class_group (handle, res_class);

  g_return_val_if_fail (class_group == NULL, NULL);

  class_group = g_object_new (WNCK_TYPE_CLASS_GROUP, NULL);
  class_group->priv->screen = screen;

  class_group->priv->res_class = g_strdup (res_class ? res_class : "");

  _wnck_handle_insert_class_group (handle,
                                   class_group->priv->res_class,
                                   class_group);

  /* Handle now owns one ref, caller gets none */

  return class_group;
}

/**
 * _wnck_class_group_destroy:
 * @class_group: a #WnckClassGroup.
 *
 * Destroys the specified @class_group.
 **/
void
_wnck_class_group_destroy (WnckClassGroup *class_group)
{
  WnckHandle *handle;

  g_return_if_fail (WNCK_IS_CLASS_GROUP (class_group));

  handle = wnck_screen_get_handle (class_group->priv->screen);
  _wnck_handle_remove_class_group (handle, class_group->priv->res_class);

  /* Removing from handle also removes the only ref WnckClassGroup had */
}

static const char *
get_name_from_applications (WnckClassGroup *class_group)
{
  const char *first_name;
  GList *l;

  /* Try to get the name from the group leaders.  If all have the same name, we
   * can use that.
   */

  first_name = NULL;

  for (l = class_group->priv->windows; l; l = l->next)
    {
      WnckWindow *w;
      WnckApplication *app;

      w = WNCK_WINDOW (l->data);
      app = wnck_window_get_application (w);

      if (!first_name)
	{
	  if (app)
	    first_name = wnck_application_get_name (app);
	}
      else
	{
	  if (!app || strcmp (first_name, wnck_application_get_name (app)) != 0)
	    break;
	}
    }

  if (!l)
    {
      /* All names are the same, so use one of them */
      return first_name;
    }
  else
    return NULL;
}

static const char *
get_name_from_windows (WnckClassGroup *class_group)
{
  const char *first_name;
  GList *l;

  /* Try to get the name from windows, following the same rationale as
   * get_name_from_applications()
   */

  first_name = NULL;

  for (l = class_group->priv->windows; l; l = l->next)
    {
      WnckWindow *window;

      window = WNCK_WINDOW (l->data);

      if (!first_name)
	first_name = wnck_window_get_name (window);
      else
	if (strcmp (first_name, wnck_window_get_name (window)) != 0)
	  break;
    }

  if (!l)
    {
      /* All names are the same, so use one of them */
      return first_name;
    }
  else
    return NULL;
}


/* Gets a sensible name for the class group from the application group leaders
 * or from individual windows.
 */
static void
set_name (WnckClassGroup *class_group)
{
  const char *new_name;

  new_name = get_name_from_applications (class_group);

  if (!new_name)
    {
      new_name = get_name_from_windows (class_group);

      if (!new_name)
	new_name = class_group->priv->res_class;
    }

  g_assert (new_name != NULL);

  if (!class_group->priv->name ||
      strcmp (class_group->priv->name, new_name) != 0)
    {
      g_free (class_group->priv->name);
      class_group->priv->name = g_strdup (new_name);

      g_signal_emit (G_OBJECT (class_group), signals[NAME_CHANGED], 0);
    }
}

static void
set_icon (WnckClassGroup *class_group)
{
  g_clear_object (&class_group->priv->icon);
  g_clear_object (&class_group->priv->mini_icon);

  g_signal_emit (G_OBJECT (class_group), signals[ICON_CHANGED], 0);
}

/* Handle window's icon_changed signal, update class group icon */
static void
update_class_group_icon (WnckWindow     *window,
                         WnckClassGroup *class_group)
{
  set_icon (class_group);
}

/* Handle window's name_changed signal, update class group name */
static void
update_class_group_name (WnckWindow     *window,
                         WnckClassGroup *class_group)
{
  set_name (class_group);
}

static void
window_weak_notify_cb (gpointer  data,
                       GObject  *where_the_window_was)
{
  WnckClassGroup *class_group;
  WnckClassGroupPrivate *priv;

  class_group = WNCK_CLASS_GROUP (data);
  priv = class_group->priv;

  g_hash_table_remove (priv->window_icon_handlers, where_the_window_was);
  g_hash_table_remove (priv->window_name_handlers, where_the_window_was);
}

/**
 * _wnck_class_group_add_window:
 * @class_group: a #WnckClassGroup.
 * @window: a #WnckWindow.
 *
 * Adds a window to @class_group.  You should only do this if the resource
 * class of the window matches the @class_group<!-- -->'s.
 **/
void
_wnck_class_group_add_window (WnckClassGroup *class_group,
                              WnckWindow     *window)
{
  gulong signal_id;

  g_return_if_fail (WNCK_IS_CLASS_GROUP (class_group));
  g_return_if_fail (WNCK_IS_WINDOW (window));
  g_return_if_fail (wnck_window_get_class_group (window) == NULL);

  class_group->priv->windows = g_list_prepend (class_group->priv->windows,
                                               window);
  _wnck_window_set_class_group (window, class_group);

  signal_id = g_signal_connect (window,
                                "icon-changed",
                                G_CALLBACK (update_class_group_icon),
                                class_group);
  g_hash_table_insert (class_group->priv->window_icon_handlers,
                       window,
                       (gpointer) signal_id);

  signal_id = g_signal_connect (window,
                                "name-changed",
                                G_CALLBACK (update_class_group_name),
                                class_group);
  g_hash_table_insert (class_group->priv->window_name_handlers,
                       window,
                       (gpointer) signal_id);

  g_object_weak_ref (G_OBJECT (window), window_weak_notify_cb, class_group);

  set_name (class_group);
  set_icon (class_group);

  /* FIXME: should we monitor class group changes on the window?  The ICCCM says
   * that clients should never change WM_CLASS unless the window is withdrawn.
   */
}

/**
 * _wnck_class_group_remove_window:
 * @class_group: a #WnckClassGroup.
 * @window: a #WnckWindow.
 *
 * Removes a window from the list of windows that are grouped under the
 * specified @class_group.
 **/
void
_wnck_class_group_remove_window (WnckClassGroup *class_group,
				 WnckWindow     *window)
{
  gulong icon_handler, name_handler;

  g_return_if_fail (WNCK_IS_CLASS_GROUP (class_group));
  g_return_if_fail (WNCK_IS_WINDOW (window));
  g_return_if_fail (wnck_window_get_class_group (window) == class_group);

  g_object_weak_unref (G_OBJECT (window), window_weak_notify_cb, class_group);

  class_group->priv->windows = g_list_remove (class_group->priv->windows,
                                              window);
  _wnck_window_set_class_group (window, NULL);
  icon_handler = (gulong) g_hash_table_lookup (class_group->priv->window_icon_handlers,
                                               window);
  if (icon_handler != 0)
    {
      g_signal_handler_disconnect (window,
                                   icon_handler);
      g_hash_table_remove (class_group->priv->window_icon_handlers,
                           window);
    }
  name_handler = (gulong) g_hash_table_lookup (class_group->priv->window_name_handlers,
                                               window);
  if (name_handler != 0)
    {
      g_signal_handler_disconnect (window,
                                   name_handler);
      g_hash_table_remove (class_group->priv->window_name_handlers,
                           window);
    }

  set_name (class_group);
  set_icon (class_group);
}

/**
 * wnck_class_group_get_windows:
 * @class_group: a #WnckClassGroup.
 *
 * Gets the list of #WnckWindow that are grouped in @class_group.
 *
 * Return value: (element-type WnckWindow) (transfer none): the list of
 * #WnckWindow grouped in @class_group, or %NULL if the group contains no
 * window. The list should not be modified nor freed, as it is owned by
 * @class_group.
 *
 * Since: 2.2
 **/
GList *
wnck_class_group_get_windows (WnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->windows;
}

/**
 * wnck_class_group_get_id:
 * @class_group: a #WnckClassGroup.
 *
 * Gets the identifier name for @class_group. This is the resource class for
 * @class_group.
 *
 * Return value: the identifier name of @class_group, or an
 * empty string if the group has no identifier name.
 *
 * Since: 3.2
 **/
const char *
wnck_class_group_get_id (WnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->res_class;
}

/**
 * wnck_class_group_get_name:
 * @class_group: a #WnckClassGroup.
 *
 * Gets an human-readable name for @class_group. Since there is no way to
 * properly find this name, a suboptimal heuristic is used to find it. The name
 * is the name of all #WnckApplication for each #WnckWindow in @class_group if
 * they all have the same name. If all #WnckApplication don't have the same
 * name, the name is the name of all #WnckWindow in @class_group if they all
 * have the same name. If all #WnckWindow don't have the same name, the
 * resource class name is used.
 *
 * Return value: an human-readable name for @class_group.
 *
 * Since: 2.2
 **/
const char *
wnck_class_group_get_name (WnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->name;
}

/**
 * wnck_class_group_get_icon:
 * @class_group: a #WnckClassGroup.
 *
 * Gets the icon to be used for @class_group. Since there is no way to
 * properly find the icon, a suboptimal heuristic is used to find it. The icon
 * is the first icon found by looking at all the #WnckApplication for each
 * #WnckWindow in @class_group, then at all the #WnckWindow in @class_group. If
 * no icon was found, a fallback icon is used.
 *
 * Return value: (transfer none): the icon for @class_group. The caller should
 * reference the returned <classname>GdkPixbuf</classname> if it needs to keep
 * the icon around.
 *
 * Since: 2.2
 **/
GdkPixbuf *
wnck_class_group_get_icon (WnckClassGroup *class_group)
{
  WnckHandle *handle;

  g_return_val_if_fail (class_group != NULL, NULL);

  if (class_group->priv->icon != NULL)
    return class_group->priv->icon;

  handle = wnck_screen_get_handle (class_group->priv->screen);

  get_icon_internal (class_group,
                     &class_group->priv->icon,
                     wnck_handle_get_default_icon_size (handle));

  return class_group->priv->icon;
}

/**
 * wnck_class_group_get_mini_icon:
 * @class_group: a #WnckClassGroup.
 *
 * Gets the mini-icon to be used for @class_group. Since there is no way to
 * properly find the mini-icon, the same suboptimal heuristic as the one for
 * wnck_class_group_get_icon() is used to find it.
 *
 * Return value: (transfer none): the mini-icon for @class_group. The caller
 * should reference the returned <classname>GdkPixbuf</classname> if it needs
 * to keep the mini-icon around.
 *
 * Since: 2.2
 **/
GdkPixbuf *
wnck_class_group_get_mini_icon (WnckClassGroup *class_group)
{
  WnckHandle *handle;

  g_return_val_if_fail (class_group != NULL, NULL);

  if (class_group->priv->mini_icon != NULL)
    return class_group->priv->mini_icon;

  handle = wnck_screen_get_handle (class_group->priv->screen);

  get_icon_internal (class_group,
                     &class_group->priv->mini_icon,
                     wnck_handle_get_default_mini_icon_size (handle));

  return class_group->priv->mini_icon;
}
