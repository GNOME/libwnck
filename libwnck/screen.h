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

#ifndef WNCK_SCREEN_H
#define WNCK_SCREEN_H

#include <glib-object.h>

G_BEGIN_DECLS

#define WNCK_TYPE_SCREEN              (wnck_screen_get_type ())
#define WNCK_SCREEN(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WNCK_TYPE_SCREEN, WnckScreen))
#define WNCK_SCREEN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WNCK_TYPE_SCREEN, WnckScreenClass))
#define WNCK_IS_SCREEN(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WNCK_TYPE_SCREEN))
#define WNCK_IS_SCREEN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WNCK_TYPE_SCREEN))
#define WNCK_SCREEN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WNCK_TYPE_SCREEN, WnckScreenClass))

typedef struct _WnckScreen        WnckScreen;
typedef struct _WnckScreenClass   WnckScreenClass;
typedef struct _WnckScreenPrivate WnckScreenPrivate;

struct _WnckScreen
{
  GObject parent_instance;

  WnckScreenPrivate *priv;
};

struct _WnckScreenClass
{
  GObjectClass parent_class;

};

GType wnck_screen_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif /* WNCK_SCREEN_H */
