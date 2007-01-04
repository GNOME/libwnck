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
#include "pager-accessible-factory.h"
#include "pager-accessible.h"

static void       wnck_pager_accessible_factory_class_init          (WnckPagerAccessibleFactoryClass *klass);
static AtkObject* wnck_pager_accessible_factory_create_accessible   (GObject                         *obj);

static GType      wnck_pager_accessible_factory_get_accessible_type (void);

GType
wnck_pager_accessible_factory_get_type (void)
{
  static GType type = 0;

  if (!type) 
    {
      const GTypeInfo tinfo = 
        {
          sizeof (WnckPagerAccessibleFactoryClass),
          (GBaseInitFunc) NULL, /* base init */
          (GBaseFinalizeFunc) NULL, /* base finalize */
          (GClassInitFunc) wnck_pager_accessible_factory_class_init,
          (GClassFinalizeFunc) NULL, /* class finalize */
          NULL, /* class data */
          sizeof (WnckPagerAccessibleFactory),
          0, /* nb preallocs */
          (GInstanceInitFunc) NULL, /* instance init */
          NULL /* value table */
        };
      type = g_type_register_static (ATK_TYPE_OBJECT_FACTORY, "WnckPagerAccessibleFactory", &tinfo, 0);
    }
  return type;
}

static void
wnck_pager_accessible_factory_class_init (WnckPagerAccessibleFactoryClass *klass)
{
  AtkObjectFactoryClass *class = ATK_OBJECT_FACTORY_CLASS (klass);

  class->create_accessible = wnck_pager_accessible_factory_create_accessible;
  class->get_accessible_type = wnck_pager_accessible_factory_get_accessible_type;
}

AtkObjectFactory*
wnck_pager_accessible_factory_new (void)
{
  GObject *factory;

  factory = g_object_new (WNCK_TYPE_PAGER_ACCESSIBLE_FACTORY, NULL);

  return ATK_OBJECT_FACTORY (factory);
}

static AtkObject*
wnck_pager_accessible_factory_create_accessible (GObject *obj)
{
  GtkWidget *widget;
  
  g_return_val_if_fail (GTK_IS_WIDGET (obj), NULL);
  
  widget = GTK_WIDGET (obj);
  return wnck_pager_accessible_new (widget);
}

static GType
wnck_pager_accessible_factory_get_accessible_type (void)
{
  return WNCK_PAGER_TYPE_ACCESSIBLE;
}
