/* workspace object */

/*
 * Copyright (C) 2001 Havoc Pennington
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

#include "workspace.h"


struct _WnckWorkspacePrivate
{

};

enum {
  
  LAST_SIGNAL
};

static void wnck_workspace_init        (WnckWorkspace      *workspace);
static void wnck_workspace_class_init  (WnckWorkspaceClass *klass);
static void wnck_workspace_finalize    (GObject        *object);


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
  workspace->priv = g_new (WnckWorkspacePrivate, 1);

}

static void
wnck_workspace_class_init (WnckWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = wnck_workspace_finalize;

}

static void
wnck_workspace_finalize (GObject *object)
{
  WnckWorkspace *workspace;

  workspace = WNCK_WORKSPACE (object);

  
  g_free (workspace->priv);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}
