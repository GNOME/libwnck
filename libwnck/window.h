/* window object */

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

#ifndef WNCK_WINDOW_H
#define WNCK_WINDOW_H

#include <glib-object.h>
#include <libwnck/screen.h>

G_BEGIN_DECLS

#define WNCK_TYPE_WINDOW              (wnck_window_get_type ())
#define WNCK_WINDOW(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WNCK_TYPE_WINDOW, WnckWindow))
#define WNCK_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WNCK_TYPE_WINDOW, WnckWindowClass))
#define WNCK_IS_WINDOW(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WNCK_TYPE_WINDOW))
#define WNCK_IS_WINDOW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WNCK_TYPE_WINDOW))
#define WNCK_WINDOW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WNCK_TYPE_WINDOW, WnckWindowClass))

typedef struct _WnckWindowClass   WnckWindowClass;
typedef struct _WnckWindowPrivate WnckWindowPrivate;

struct _WnckWindow
{
  GObject parent_instance;

  WnckWindowPrivate *priv;
};

struct _WnckWindowClass
{
  GObjectClass parent_class;

  /* window name or icon name changed */
  void (* name_changed) (WnckWindow *window);

  /* minimized, maximized, sticky, skip pager, skip task, shaded
   * may have changed
   */
  void (* state_changed) (WnckWindow *window);
};

GType wnck_window_get_type (void) G_GNUC_CONST;

WnckWindow* wnck_window_get (gulong xwindow);

const char* wnck_window_get_name      (WnckWindow *window);
const char* wnck_window_get_icon_name (WnckWindow *window);
gboolean    wnck_window_is_minimized  (WnckWindow *window);

G_END_DECLS

#endif /* WNCK_WINDOW_H */
