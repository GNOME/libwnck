/* class group object */

/*
 * Copyright (C) 2003 Ximian, Inc.
 * Authors: Federico Mena-Quintero <federico@ximian.com>
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

#include <string.h>
#include "class-group.h"
#include "window.h"
#include "private.h"


/* Private part of the WnckClassGroup structure */
struct _WnckClassGroupPrivate {
  char *res_class;
  char *name;
  GList *windows;

  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;
};

#define ICON_SIZE 32
#define MINI_ICON_SIZE 16

/* Hash table that maps res_class strings -> WnckClassGroup instances */
static GHashTable *class_group_hash = NULL;



static void wnck_class_group_class_init  (WnckClassGroupClass *class);
static void wnck_class_group_init        (WnckClassGroup      *class_group);
static void wnck_class_group_finalize    (GObject             *object);

enum {
  NAME_CHANGED,
  ICON_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static gpointer *parent_class;


GType
wnck_class_group_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (WnckClassGroupClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) wnck_class_group_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (WnckClassGroup),
        0,              /* n_preallocs */
        (GInstanceInitFunc) wnck_class_group_init,
      };

      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "WnckClassGroup",
                                            &object_info, 0);
    }

  return object_type;
}

static void
wnck_class_group_class_init (WnckClassGroupClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = wnck_class_group_finalize;

  signals[NAME_CHANGED] =
    g_signal_new ("name_changed",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckClassGroupClass, name_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  signals[ICON_CHANGED] =
    g_signal_new ("icon_changed",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckClassGroupClass, icon_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
wnck_class_group_init (WnckClassGroup *class_group)
{
  WnckClassGroupPrivate *priv;

  priv = g_new (WnckClassGroupPrivate, 1);
  class_group->priv = priv;

  priv->res_class = NULL;
  priv->name = NULL;
  priv->windows = NULL;

  priv->icon = NULL;
  priv->mini_icon = NULL;
}

static void
wnck_class_group_finalize (GObject *object)
{
  WnckClassGroup *class_group;
  WnckClassGroupPrivate *priv;

  class_group = WNCK_CLASS_GROUP (object);
  priv = class_group->priv;

  if (priv->res_class)
    g_free (priv->res_class);

  if (priv->name)
    g_free (priv->name);

  g_list_free (priv->windows);

  if (priv->icon)
    g_object_unref (priv->icon);

  if (priv->mini_icon)
    g_object_unref (priv->mini_icon);

  g_free (priv);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * wnck_class_group_get:
 * @res_class: Name of the sought resource class.
 *
 * Gets an existing class group based on its resource class name.
 *
 * Return value: An existing #WnckClassGroup, or NULL if there is no groups with
 * the specified @res_class.
 **/
WnckClassGroup *
wnck_class_group_get (const char *res_class)
{
  if (!class_group_hash)
    return NULL;
  else
    return g_hash_table_lookup (class_group_hash, res_class ? res_class : "");
}

/**
 * _wnck_class_group_create:
 * @res_class: Name of the resource class for the group.
 *
 * Creates a new WnckClassGroup with the specified resource class name.  If
 * @res_class is #NULL, then windows without a resource class name will get
 * grouped under this class group.
 *
 * Return value: A newly-created #WnckClassGroup, or an existing one that
 * matches the @res_class.
 **/
WnckClassGroup *
_wnck_class_group_create (const char *res_class)
{
  WnckClassGroup *class_group;
  WnckClassGroupPrivate *priv;

  if (class_group_hash == NULL)
    class_group_hash = g_hash_table_new (g_str_hash, g_str_equal);

  g_return_val_if_fail (g_hash_table_lookup (class_group_hash, res_class ? res_class : "") == NULL,
			NULL);

  class_group = g_object_new (WNCK_TYPE_CLASS_GROUP, NULL);
  priv = class_group->priv;

  priv->res_class = g_strdup (res_class ? res_class : "");

  g_hash_table_insert (class_group_hash, priv->res_class, class_group);
  /* Hash now owns one ref, caller gets none */

  return class_group;
}

/**
 * _wnck_class_group_destroy:
 * @class_group: A window class group.
 *
 * Destroys the specified @class_group.
 **/
void
_wnck_class_group_destroy (WnckClassGroup *class_group)
{
  WnckClassGroupPrivate *priv;

  g_return_if_fail (WNCK_IS_CLASS_GROUP (class_group));

  priv = class_group->priv;

  g_hash_table_remove (class_group_hash, priv->res_class);

  g_free (priv->res_class);
  priv->res_class = NULL;

  /* remove hash's ref on the class group */
  g_object_unref (class_group);
}

static const char *
get_name_from_applications (WnckClassGroup *class_group)
{
  WnckClassGroupPrivate *priv;
  const char *first_name;
  GList *l;

  priv = class_group->priv;

  /* Try to get the name from the group leaders.  If all have the same name, we
   * can use that.
   */

  first_name = NULL;

  for (l = priv->windows; l; l = l->next)
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
  WnckClassGroupPrivate *priv;
  const char *first_name;
  GList *l;

  priv = class_group->priv;

  /* Try to get the name from windows, following the same rationale as
   * get_name_from_applications()
   */

  first_name = NULL;

  for (l = priv->windows; l; l = l->next)
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
  WnckClassGroupPrivate *priv;
  const char *new_name;

  priv = class_group->priv;

  if (priv->name)
    {
      g_free (priv->name);
      priv->name = NULL;
    }

  new_name = get_name_from_applications (class_group);

  if (!new_name)
    {
      new_name = get_name_from_windows (class_group);

      if (!new_name)
	new_name = priv->res_class;
    }

  g_assert (new_name != NULL);

  if (!priv->name || strcmp (priv->name, new_name) != 0)
    {
      g_free (priv->name);
      priv->name = g_strdup (new_name);

      g_signal_emit (G_OBJECT (class_group), signals[NAME_CHANGED], 0);
    }
}

/* Walks the list of applications, trying to get an icon from them */
static void
get_icons_from_applications (WnckClassGroup *class_group, GdkPixbuf **icon, GdkPixbuf **mini_icon)
{
  WnckClassGroupPrivate *priv;
  GList *l;

  priv = class_group->priv;

  *icon = NULL;
  *mini_icon = NULL;

  for (l = priv->windows; l; l = l->next)
    {
      WnckWindow *window;
      WnckApplication *app;

      window = WNCK_WINDOW (l->data);
      app = wnck_window_get_application (window);
      if (app)
	{
	  *icon = wnck_application_get_icon (app);
	  *mini_icon = wnck_application_get_mini_icon (app);

	  if (*icon && *mini_icon)
	    return;
	  else
	    {
	      *icon = NULL;
	      *mini_icon = NULL;
	    }
	}
    }
}

/* Walks the list of windows, trying to get an icon from them */
static void
get_icons_from_windows (WnckClassGroup *class_group, GdkPixbuf **icon, GdkPixbuf **mini_icon)
{
  WnckClassGroupPrivate *priv;
  GList *l;

  priv = class_group->priv;

  *icon = NULL;
  *mini_icon = NULL;

  for (l = priv->windows; l; l = l->next)
    {
      WnckWindow *window;

      window = WNCK_WINDOW (l->data);

      *icon = wnck_window_get_icon (window);
      *mini_icon = wnck_window_get_mini_icon (window);

      if (*icon && *mini_icon)
	return;
      else
	{
	  *icon = NULL;
	  *mini_icon = NULL;
	}
    }
}

/* Gets a sensible icon and mini_icon for the class group from the application
 * group leaders or from individual windows.
 */
static void
set_icon (WnckClassGroup *class_group)
{
  WnckClassGroupPrivate *priv;
  GdkPixbuf *icon, *mini_icon;

  priv = class_group->priv;

  get_icons_from_applications (class_group, &icon, &mini_icon);

  if (!icon || !mini_icon)
    get_icons_from_windows (class_group, &icon, &mini_icon);

  if (!icon || !mini_icon)
      _wnck_get_fallback_icons (&icon, ICON_SIZE, ICON_SIZE,
				&mini_icon, MINI_ICON_SIZE, MINI_ICON_SIZE);

  g_assert (icon && mini_icon);

  if (priv->icon)
    g_object_unref (priv->icon);

  if (priv->mini_icon)
    g_object_unref (priv->mini_icon);

  priv->icon = g_object_ref (icon);
  priv->mini_icon = g_object_ref (mini_icon);

  g_signal_emit (G_OBJECT (class_group), signals[ICON_CHANGED], 0);
}

/**
 * _wnck_class_group_add_window:
 * @class_group: A window class group.
 * @window: A window.
 *
 * Adds a window to a class group.  You should only do this if the resource
 * class of the window matches the @class_group<!-- -->'s.
 **/
void
_wnck_class_group_add_window (WnckClassGroup *class_group,
                              WnckWindow     *window)
{
  WnckClassGroupPrivate *priv;

  g_return_if_fail (WNCK_IS_CLASS_GROUP (class_group));
  g_return_if_fail (WNCK_IS_WINDOW (window));
  g_return_if_fail (wnck_window_get_class_group (window) == NULL);

  priv = class_group->priv;

  priv->windows = g_list_prepend (priv->windows, window);
  _wnck_window_set_class_group (window, class_group);

  set_name (class_group);
  set_icon (class_group);

  /* FIXME: should we monitor class group changes on the window?  The ICCCM says
   * that clients should never change WM_CLASS unless the window is withdrawn.
   */
}

/**
 * _wnck_class_group_remove_window:
 * @class_group: A window class group.
 * @window: A window.
 * 
 * Removes a window from the list of windows that are grouped under the
 * specified @class_group.
 **/
void
_wnck_class_group_remove_window (WnckClassGroup *class_group,
				 WnckWindow     *window)
{
  WnckClassGroupPrivate *priv;

  g_return_if_fail (WNCK_IS_CLASS_GROUP (class_group));
  g_return_if_fail (WNCK_IS_WINDOW (window));
  g_return_if_fail (wnck_window_get_class_group (window) == class_group);

  priv = class_group->priv;

  priv->windows = g_list_remove (priv->windows, window);
  _wnck_window_set_class_group (window, NULL);

  set_name (class_group);
  set_icon (class_group);
}

/**
 * wnck_class_group_get_windows:
 * @class_group: A window class group.
 * 
 * Gets the list of windows that are grouped in a @class_group.
 * 
 * Return value: A list of windows, or NULL if the group contains no windows.
 * The list should not be freed, as it belongs to the @class_group.
 **/
GList *
wnck_class_group_get_windows (WnckClassGroup *class_group)
{
  WnckClassGroupPrivate *priv;

  g_return_val_if_fail (class_group != NULL, NULL);

  priv = class_group->priv;
  return priv->windows;
}

/**
 * wnck_class_group_get_res_class:
 * @class_group: A window class group.
 * 
 * Queries the resource class name for a class group.
 * 
 * Return value: The resource class name of the specified @class_group, or the
 * empty string if the group has no name.  The string should not be freed.
 **/
const char *
wnck_class_group_get_res_class (WnckClassGroup *class_group)
{
  WnckClassGroupPrivate *priv;

  g_return_val_if_fail (class_group != NULL, NULL);

  priv = class_group->priv;
  return priv->res_class;
}

/**
 * wnck_class_group_get_name:
 * @class_group: A window class group.
 * 
 * Queries the human-readable name for a class group.
 * 
 * Return value: Name of the class group.
 **/
const char *
wnck_class_group_get_name (WnckClassGroup *class_group)
{
  WnckClassGroupPrivate *priv;

  g_return_val_if_fail (class_group != NULL, NULL);

  priv = class_group->priv;

  return priv->name;
}

/**
 * wnck_class_group_get_icon:
 * @class_group: A window class group.
 * 
 * Queries the icon to be used for a class group.
 * 
 * Return value: The icon to use.
 **/
GdkPixbuf *
wnck_class_group_get_icon (WnckClassGroup *class_group)
{
  WnckClassGroupPrivate *priv;

  g_return_val_if_fail (class_group != NULL, NULL);

  priv = class_group->priv;

  return priv->icon;
}

/**
 * wnck_class_group_get_mini_icon:
 * @class_group: A window class group.
 * 
 * Queries the mini-icon to be used for a class group.
 * 
 * Return value: The mini-icon to use.
 **/
GdkPixbuf *
wnck_class_group_get_mini_icon (WnckClassGroup *class_group)
{
  WnckClassGroupPrivate *priv;

  g_return_val_if_fail (class_group != NULL, NULL);

  priv = class_group->priv;

  return priv->mini_icon;
}
