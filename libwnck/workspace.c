/* workspace object */

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
#include <libintl.h>
#include "workspace.h"
#include "xutils.h"
#include "private.h"
#include <string.h>

struct _WnckWorkspacePrivate
{
  WnckScreen *screen;
  int number;
  char *name;
  int width, height;            /* Workspace size */
  int viewport_x, viewport_y;   /* Viewport origin */
  gboolean is_virtual;
};

enum {
  NAME_CHANGED,
  LAST_SIGNAL
};

static void wnck_workspace_init        (WnckWorkspace      *workspace);
static void wnck_workspace_class_init  (WnckWorkspaceClass *klass);
static void wnck_workspace_finalize    (GObject        *object);


static void emit_name_changed (WnckWorkspace *space);

static gpointer parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

GType
wnck_workspace_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();
  
  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (WnckWorkspaceClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) wnck_workspace_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (WnckWorkspace),
        0,              /* n_preallocs */
        (GInstanceInitFunc) wnck_workspace_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "WnckWorkspace",
                                            &object_info, 0);
    }
  
  return object_type;
}

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
  
  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = wnck_workspace_finalize;

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
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * wnck_workspace_get_number:
 * @space: a #WnckWorkspace
 * 
 * 
 * 
 * Return value: get the index of the workspace
 **/
int
wnck_workspace_get_number (WnckWorkspace *space)
{
  g_return_val_if_fail (WNCK_IS_WORKSPACE (space), 0);
  
  return space->priv->number;
}

/**
 * wnck_workspace_get_name:
 * @space: a #WnckWorkspace
 * 
 * Gets the name that should be used to refer to the workspace
 * in the user interface. If the user hasn't set a special name,
 * will be something like "Workspace 3" - otherwise whatever name
 * the user set.
 * 
 * Return value: workspace name, never %NULL 
 **/
const char*
wnck_workspace_get_name (WnckWorkspace *space)
{
  g_return_val_if_fail (WNCK_IS_WORKSPACE (space), NULL);
  
  return space->priv->name;
}

/**
 * wnck_workspace_change_name:
 * @space: a #WnckWorkspace
 * @name: new workspace name
 *
 * Try changing the name of the workspace.
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
 * @space: a #WnckWorkspace
 * 
 * Ask window manager to make @space the active workspace.
 * 
 **/
void
wnck_workspace_activate (WnckWorkspace *space)
{
  g_return_if_fail (WNCK_IS_WORKSPACE (space));

  _wnck_activate_workspace (WNCK_SCREEN_XSCREEN (space->priv->screen), space->priv->number);
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

int
wnck_workspace_get_width (WnckWorkspace *space)
{
  return space->priv->width;
}

int
wnck_workspace_get_height (WnckWorkspace *space)
{
  return space->priv->height;
}

int
wnck_workspace_get_viewport_x (WnckWorkspace *space)
{
  return space->priv->viewport_x;
}

int
wnck_workspace_get_viewport_y (WnckWorkspace *space)
{
  return space->priv->viewport_y;
}

gboolean
wnck_workspace_is_virtual (WnckWorkspace *space)
{
  return space->priv->is_virtual;
}
