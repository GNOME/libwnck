/*
 * Copyright 2002 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include "workspace-accessible-factory.h"
#include "workspace-accessible.h"

static void       wnck_workspace_accessible_factory_class_init        (WnckWorkspaceAccessibleFactoryClass *klass);
static AtkObject* wnck_workspace_accessible_factory_create_accessible (GObject                          *obj);

static GType wnck_workspace_accessible_factory_get_accessible_type (void);

GType
wnck_workspace_accessible_factory_get_type (void)
{
  static GType type = 0;

  if (!type) 
    {
      static const GTypeInfo tinfo = 
      {
        sizeof (WnckWorkspaceAccessibleFactoryClass),
        (GBaseInitFunc) NULL, /* base init */
        (GBaseFinalizeFunc) NULL, /* base finalize */
        (GClassInitFunc) wnck_workspace_accessible_factory_class_init,
        (GClassFinalizeFunc) NULL, /* class finalize */
        NULL, /* class data */
        sizeof (WnckWorkspaceAccessibleFactory),
        0, /* nb preallocs */
        (GInstanceInitFunc) NULL, /* instance init */
        NULL /* value table */
      };
      type = g_type_register_static (ATK_TYPE_OBJECT_FACTORY, "WnckWorkspaceAccessibleFactory" , &tinfo, 0);
    }
  return type;
}

static void
wnck_workspace_accessible_factory_class_init (WnckWorkspaceAccessibleFactoryClass *klass)
{
  AtkObjectFactoryClass *class = ATK_OBJECT_FACTORY_CLASS (klass);

  class->create_accessible = wnck_workspace_accessible_factory_create_accessible;
  class->get_accessible_type = wnck_workspace_accessible_factory_get_accessible_type;
}

AtkObjectFactory*
wnck_workspace_accessible_factory_new (void)
{
  GObject *factory;
  factory = g_object_new (WNCK_TYPE_WORKSPACE_ACCESSIBLE_FACTORY, NULL);
  return ATK_OBJECT_FACTORY (factory);
}

static AtkObject*
wnck_workspace_accessible_factory_create_accessible (GObject *obj)
{
  return wnck_workspace_accessible_new (obj);
}

static GType
wnck_workspace_accessible_factory_get_accessible_type (void)
{
  return WNCK_WORKSPACE_TYPE_ACCESSIBLE;
}
