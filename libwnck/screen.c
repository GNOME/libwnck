/* screen object */

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

#include "screen.h"

struct _WnckScreenPrivate
{

};

enum {
  
  LAST_SIGNAL
};

static void wnck_screen_init        (WnckScreen      *screen);
static void wnck_screen_class_init  (WnckScreenClass *klass);
static void wnck_screen_finalize    (GObject        *object);


static gpointer parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

GType
wnck_screen_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();
  
  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (WnckScreenClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) wnck_screen_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (WnckScreen),
        0,              /* n_preallocs */
        (GInstanceInitFunc) wnck_screen_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "WnckScreen",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
wnck_screen_init (WnckScreen *screen)
{  
  screen->priv = g_new (WnckScreenPrivate, 1);

}

static void
wnck_screen_class_init (WnckScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = wnck_screen_finalize;

}

static void
wnck_screen_finalize (GObject *object)
{
  WnckScreen *screen;

  screen = WNCK_SCREEN (object);

  
  g_free (screen->priv);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}
