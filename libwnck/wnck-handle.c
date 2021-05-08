/*
 * Copyright (C) 2021 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "wnck-handle-private.h"

#include "wnck-enum-types.h"

#define WNCK_TYPE_HANDLE (wnck_handle_get_type ())
G_DECLARE_FINAL_TYPE (WnckHandle, wnck_handle, WNCK, HANDLE, GObject)

struct _WnckHandle
{
  GObject        parent;

  WnckClientType client_type;
};

enum
{
  PROP_0,

  PROP_CLIENT_TYPE,

  LAST_PROP
};

static GParamSpec *handle_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (WnckHandle, wnck_handle, G_TYPE_OBJECT)

static void
wnck_handle_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  WnckHandle *self;

  self = WNCK_HANDLE (object);

  switch (property_id)
    {
      case PROP_CLIENT_TYPE:
        g_value_set_enum (value, self->client_type);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
wnck_handle_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  WnckHandle *self;

  self = WNCK_HANDLE (object);

  switch (property_id)
    {
      case PROP_CLIENT_TYPE:
        self->client_type = g_value_get_enum (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  handle_properties[PROP_CLIENT_TYPE] =
    g_param_spec_enum ("client-type",
                       "client-type",
                       "client-type",
                       WNCK_TYPE_CLIENT_TYPE,
                       WNCK_CLIENT_TYPE_APPLICATION,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     LAST_PROP,
                                     handle_properties);
}

static void
wnck_handle_class_init (WnckHandleClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->get_property = wnck_handle_get_property;
  object_class->set_property = wnck_handle_set_property;

  install_properties (object_class);
}

static void
wnck_handle_init (WnckHandle *self)
{
}

WnckHandle
*_wnck_handle_new (WnckClientType client_type)
{
  return g_object_new (WNCK_TYPE_HANDLE,
                       "client-type", client_type,
                       NULL);
}

WnckClientType
_wnck_handle_get_client_type (WnckHandle *self)
{
  return self->client_type;
}
