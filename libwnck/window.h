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

  /* Changed workspace or pinned/unpinned state */
  void (* workspace_changed) (WnckWindow *window);
};

GType wnck_window_get_type (void) G_GNUC_CONST;

WnckWindow* wnck_window_get (gulong xwindow);

WnckScreen* wnck_window_get_screen    (WnckWindow *window);

const char* wnck_window_get_name      (WnckWindow *window);
const char* wnck_window_get_icon_name (WnckWindow *window);

WnckApplication* wnck_window_get_application  (WnckWindow *window);
gulong           wnck_window_get_group_leader (WnckWindow *window);

const char* wnck_window_get_session_id        (WnckWindow *window);
const char* wnck_window_get_session_id_utf8   (WnckWindow *window);
int         wnck_window_get_pid               (WnckWindow *window);

gboolean wnck_window_is_minimized              (WnckWindow *window);
gboolean wnck_window_is_maximized_horizontally (WnckWindow *window);
gboolean wnck_window_is_maximized_vertically   (WnckWindow *window);
gboolean wnck_window_is_maximized              (WnckWindow *window);
gboolean wnck_window_is_shaded                 (WnckWindow *window);
gboolean wnck_window_is_skip_pager             (WnckWindow *window);
gboolean wnck_window_is_skip_tasklist          (WnckWindow *window);
gboolean wnck_window_is_sticky                 (WnckWindow *window);

void wnck_window_minimize                (WnckWindow *window);
void wnck_window_unminimize              (WnckWindow *window);
void wnck_window_maximize                (WnckWindow *window);
void wnck_window_unmaximize              (WnckWindow *window);
void wnck_window_maximize_horizontally   (WnckWindow *window);
void wnck_window_unmaximize_horizontally (WnckWindow *window);
void wnck_window_maximize_vertically     (WnckWindow *window);
void wnck_window_unmaximize_vertically   (WnckWindow *window);
void wnck_window_shade                   (WnckWindow *window);
void wnck_window_unshade                 (WnckWindow *window);
void wnck_window_stick                   (WnckWindow *window);
void wnck_window_unstick                 (WnckWindow *window);

WnckWorkspace* wnck_window_get_workspace     (WnckWindow    *window);
void           wnck_window_move_to_workspace (WnckWindow    *window,
                                              WnckWorkspace *space);

/* pinned = on all workspaces */
gboolean wnck_window_is_pinned (WnckWindow *window);
void     wnck_window_pin       (WnckWindow *window);
void     wnck_window_unpin     (WnckWindow *window);


G_END_DECLS

#endif /* WNCK_WINDOW_H */
