/* workspace object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Kim Woelders
 * Copyright (C) 2003 Red Hat, Inc.
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

#include <config.h>

#include <glib/gi18n-lib.h>
#include "workspace.h"
#include "xutils.h"
#include "private.h"
#include <string.h>

/**
 * SECTION:workspace
 * @short_description: an object representing a workspace.
 * @see_also: #WnckScreen
 * @stability: Unstable
 *
 * The #WnckWorkspace represents what is called <ulink
 * url="http://standards.freedesktop.org/wm-spec/wm-spec-latest.html&num;largedesks">virtual
 * desktops</ulink> in the <ulink
 * url="http://standards.freedesktop.org/wm-spec/wm-spec-latest.html">EWMH</ulink>.
 * A workspace is a virtualization of a #WnckScreen<!-- -->: only one workspace
 * can be shown on a #WnckScreen at a time. It makes it possible, for example,
 * to put windows on different workspaces to organize them.
 *
 * If the #WnckWorkspace size is bigger that the #WnckScreen size, the
 * workspace contains a viewport. Viewports are defined in the <ulink
 * url="http://standards.freedesktop.org/wm-spec/wm-spec-latest.html&num;id2457064">large
 * desktops</ulink> section of the <ulink
 * url="http://standards.freedesktop.org/wm-spec/wm-spec-latest.html">EWMH</ulink>.
 * The notion of workspaces and viewports are quite similar, and generally both
 * are not used at the same time: there are generally either multiple
 * workspaces with no viewport, or one workspace with a viewport. libwnck
 * supports all situations, even multiple workspaces with viewports.
 *
 * Workspaces are organized according to a layout set on the #WnckScreen. See
 * wnck_screen_calc_workspace_layout(), wnck_screen_try_set_workspace_layout(),
 * wnck_screen_release_workspace_layout() and
 * wnck_screen_free_workspace_layout() for more information about the layout.
 *
 * The #WnckWorkspace objects are always owned by libwnck and must not be
 * referenced or unreferenced.
 */

struct _WnckWorkspacePrivate
{
  WnckScreen *screen;
  int number;
  char *name;
  int width, height;            /* Workspace size */
  int viewport_x, viewport_y;   /* Viewport origin */
  gboolean is_virtual;
};

G_DEFINE_TYPE (WnckWorkspace, wnck_workspace, G_TYPE_OBJECT);

enum {
  NAME_CHANGED,
  LAST_SIGNAL
};

static void wnck_workspace_init        (WnckWorkspace      *workspace);
static void wnck_workspace_class_init  (WnckWorkspaceClass *klass);
static void wnck_workspace_finalize    (GObject        *object);


static void emit_name_changed (WnckWorkspace *space);

static guint signals[LAST_SIGNAL] = { 0 };

static void
wnck_workspace_init (WnckWorkspace *workspace)
{
  workspace->priv = g_new0 (WnckWorkspacePrivate, 1);

  workspace->priv->number = -1;
}

static void
wnck_workspace_class_init (WnckWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  object_class->finalize = wnck_workspace_finalize;

  /**
   * WnckWorkspace::name-changed:
   * @space: the #WnckWorkspace which emitted the signal.
   *
   * Emitted when the name of @space changes.
   */
  signals[NAME_CHANGED] =
    g_signal_new ("name_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (WnckWorkspaceClass, name_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
wnck_workspace_finalize (GObject *object)
{
  WnckWorkspace *workspace;

  workspace = WNCK_WORKSPACE (object);

  g_free (workspace->priv->name);
  
  g_free (workspace->priv);
  
  G_OBJECT_CLASS (wnck_workspace_parent_class)->finalize (object);
}

/**
 * wnck_workspace_get_number:
 * @space: a #WnckWorkspace.
 * 
 * Returns the index of @space on the #WnckScreen to which it belongs. The
 * first workspace has an index of 0.
 * 
 * Return value: the index of @space on its #WnckScreen.
 **/
int
wnck_workspace_get_number (WnckWorkspace *space)
{
  g_return_val_if_fail (WNCK_IS_WORKSPACE (space), 0);
  
  return space->priv->number;
}

/**
 * wnck_workspace_get_name:
 * @space: a #WnckWorkspace.
 * 
 * Returns the human-readable name that should be used to refer to @space. If
 * the user has not set a special name, a fallback like "Workspace 3" will be
 * used.
 * 
 * Return value: the name of @space.
 **/
const char*
wnck_workspace_get_name (WnckWorkspace *space)
{
  g_return_val_if_fail (WNCK_IS_WORKSPACE (space), NULL);
  
  return space->priv->name;
}

/**
 * wnck_workspace_change_name:
 * @space: a #WnckWorkspace.
 * @name: new name for @space.
 *
 * Changes the name of @space.
 * 
 **/
void
wnck_workspace_change_name (WnckWorkspace *space,
                            const char    *name)
{
  g_return_if_fail (WNCK_IS_WORKSPACE (space));
  g_return_if_fail (name != NULL);

  _wnck_screen_change_workspace_name (space->priv->screen,
                                      space->priv->number,
                                      name);
}

/**
 * wnck_workspace_activate:
 * @space: a #WnckWorkspace.
 * @timestamp: the X server timestamp of the user interaction event that caused
 * this call to occur.
 * 
 * Asks the window manager to make @space the active workspace. The window
 * manager may decide to refuse the request (to not steal the focus if there is
 * a more recent user activity, for example).
 * 
 **/
void
wnck_workspace_activate (WnckWorkspace *space,
                         guint32        timestamp)
{
  g_return_if_fail (WNCK_IS_WORKSPACE (space));

  _wnck_activate_workspace (WNCK_SCREEN_XSCREEN (space->priv->screen),
                            space->priv->number,
                            timestamp);
}

WnckWorkspace*
_wnck_workspace_create (int number, WnckScreen *screen)
{
  WnckWorkspace *space;
  
  space = g_object_new (WNCK_TYPE_WORKSPACE, NULL);
  space->priv->number = number;
  space->priv->name = NULL;
  space->priv->screen = screen;

  _wnck_workspace_update_name (space, NULL);
  
  /* Just set reasonable defaults */
  space->priv->width = wnck_screen_get_width (screen);
  space->priv->height = wnck_screen_get_height (screen);
  space->priv->is_virtual = FALSE;

  space->priv->viewport_x = 0;
  space->priv->viewport_y = 0;

  return space;
}

void
_wnck_workspace_update_name (WnckWorkspace *space,
                             const char    *name)
{
  char *old;
  
  g_return_if_fail (WNCK_IS_WORKSPACE (space));

  old = space->priv->name;
  space->priv->name = g_strdup (name);

  if (space->priv->name == NULL)
    space->priv->name = g_strdup_printf (_("Workspace %d"),
                                         space->priv->number + 1);

  if ((old && !name) ||
      (!old && name) ||
      (old && name && strcmp (old, name) != 0))
    emit_name_changed (space);

  g_free (old);
}

static void
emit_name_changed (WnckWorkspace *space)
{
  g_signal_emit (G_OBJECT (space),
                 signals[NAME_CHANGED],
                 0);
}

gboolean
_wnck_workspace_set_geometry (WnckWorkspace *space,
                              int            w,
                              int            h)
{
  if (space->priv->width != w || space->priv->height != h)
    {
      space->priv->width = w;
      space->priv->height = h;

      space->priv->is_virtual = w > wnck_screen_get_width (space->priv->screen) ||
				h > wnck_screen_get_height (space->priv->screen);

      return TRUE;  /* change was made */
    }
  else
    return FALSE; 
}

gboolean
_wnck_workspace_set_viewport (WnckWorkspace *space,
                              int            x,
                              int            y)
{
  if (space->priv->viewport_x != x || space->priv->viewport_y != y)
    {
      space->priv->viewport_x = x;
      space->priv->viewport_y = y;

      return TRUE; /* change was made */
    }
  else
    return FALSE;
}

/**
 * wnck_workspace_get_width:
 * @space: a #WnckWorkspace.
 *
 * Returns the width of @space.
 *
 * Returns: the width of @space.
 */
int
wnck_workspace_get_width (WnckWorkspace *space)
{
  return space->priv->width;
}

/**
 * wnck_workspace_get_height:
 * @space: a #WnckWorkspace.
 *
 * Returns the height of @space.
 *
 * Returns: the height of @space.
 */
int
wnck_workspace_get_height (WnckWorkspace *space)
{
  return space->priv->height;
}

/**
 * wnck_workspace_get_viewport_x:
 * @space: a #WnckWorkspace.
 *
 * Returns the X coordinate of the viewport in @space.
 *
 * Returns: the X coordinate of the viewport in @space, or 0 if @space does not
 * contain a viewport.
 */
int
wnck_workspace_get_viewport_x (WnckWorkspace *space)
{
  return space->priv->viewport_x;
}

/**
 * wnck_workspace_get_viewport_y:
 * @space: a #WnckWorkspace.
 *
 * Returns the Y coordinate of the viewport in @space.
 *
 * Returns: the Y coordinate of the viewport in @space, or 0 if @space does not
 * contain a viewport.
 */
int
wnck_workspace_get_viewport_y (WnckWorkspace *space)
{
  return space->priv->viewport_y;
}

/**
 * wnck_workspace_is_virtual:
 * @space: a #WnckWorkspace.
 *
 * Returns whether @space contains a viewport.
 *
 * Returns: %TRUE if @space contains a viewport, %FALSE otherwise.
 */
gboolean
wnck_workspace_is_virtual (WnckWorkspace *space)
{
  return space->priv->is_virtual;
}
