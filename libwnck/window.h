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

#ifndef WNCK_I_KNOW_THIS_IS_UNSTABLE
#error "libwnck should only be used if you understand that it's subject to frequent change, and is not supported as a fixed API/ABI or as part of the platform"
#endif

#include <glib-object.h>
#include <libwnck/screen.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

typedef enum
{
  WNCK_WINDOW_STATE_MINIMIZED              = 1 << 0,
  WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY = 1 << 1,
  WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY   = 1 << 2,
  WNCK_WINDOW_STATE_SHADED                 = 1 << 3,
  WNCK_WINDOW_STATE_SKIP_PAGER             = 1 << 4,
  WNCK_WINDOW_STATE_SKIP_TASKLIST          = 1 << 5,
  WNCK_WINDOW_STATE_STICKY                 = 1 << 6,
  WNCK_WINDOW_STATE_HIDDEN                 = 1 << 7,
  WNCK_WINDOW_STATE_FULLSCREEN             = 1 << 8
} WnckWindowState;

typedef enum
{
  WNCK_WINDOW_ACTION_MOVE                    = 1 << 0,
  WNCK_WINDOW_ACTION_RESIZE                  = 1 << 1,
  WNCK_WINDOW_ACTION_SHADE                   = 1 << 2,
  WNCK_WINDOW_ACTION_STICK                   = 1 << 3,
  WNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY   = 1 << 4,
  WNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY     = 1 << 5,
  WNCK_WINDOW_ACTION_CHANGE_WORKSPACE        = 1 << 6, /* includes pin/unpin */
  WNCK_WINDOW_ACTION_CLOSE                   = 1 << 7,
  WNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY = 1 << 8,
  WNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY   = 1 << 9,
  WNCK_WINDOW_ACTION_UNSHADE                 = 1 << 10,
  WNCK_WINDOW_ACTION_UNSTICK                 = 1 << 11,
  WNCK_WINDOW_ACTION_MINIMIZE                = 1 << 12,
  WNCK_WINDOW_ACTION_UNMINIMIZE              = 1 << 13,
  WNCK_WINDOW_ACTION_MAXIMIZE                = 1 << 14,
  WNCK_WINDOW_ACTION_UNMAXIMIZE              = 1 << 15
} WnckWindowActions;

typedef enum
{
  WNCK_WINDOW_NORMAL,       /* document/app window */
  WNCK_WINDOW_DESKTOP,      /* desktop background */
  WNCK_WINDOW_DOCK,         /* panel */
  WNCK_WINDOW_DIALOG,       /* dialog */
  WNCK_WINDOW_MODAL_DIALOG, /* modal dialog */
  WNCK_WINDOW_TOOLBAR,      /* tearoff toolbar */
  WNCK_WINDOW_MENU,         /* tearoff menu */
  WNCK_WINDOW_UTILITY,      /* palette/toolbox window */
  WNCK_WINDOW_SPLASHSCREEN  /* splash screen */
} WnckWindowType;

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
  void (* state_changed) (WnckWindow     *window,
                          WnckWindowState changed_mask,
                          WnckWindowState new_state);

  /* Changed workspace or pinned/unpinned state */
  void (* workspace_changed) (WnckWindow *window);

  /* Changed icon */
  void (* icon_changed)      (WnckWindow *window);

  /* Changed actions */
  void (* actions_changed)   (WnckWindow       *window,
                              WnckWindowActions changed_mask,
                              WnckWindowActions new_actions);

  /* Changed size/position */
  void (* geometry_changed)      (WnckWindow       *window);
};

GType wnck_window_get_type (void) G_GNUC_CONST;

WnckWindow* wnck_window_get (gulong xwindow);

WnckScreen* wnck_window_get_screen    (WnckWindow *window);

const char* wnck_window_get_name      (WnckWindow *window);
const char* wnck_window_get_icon_name (WnckWindow *window);

WnckApplication* wnck_window_get_application  (WnckWindow *window);
gulong           wnck_window_get_group_leader (WnckWindow *window);
gulong           wnck_window_get_xid          (WnckWindow *window);

WnckClassGroup *wnck_window_get_class_group (WnckWindow *window);

const char* wnck_window_get_session_id        (WnckWindow *window);
const char* wnck_window_get_session_id_utf8   (WnckWindow *window);
int         wnck_window_get_pid               (WnckWindow *window);

WnckWindowType wnck_window_get_window_type    (WnckWindow *window);

gboolean wnck_window_is_minimized              (WnckWindow *window);
gboolean wnck_window_is_maximized_horizontally (WnckWindow *window);
gboolean wnck_window_is_maximized_vertically   (WnckWindow *window);
gboolean wnck_window_is_maximized              (WnckWindow *window);
gboolean wnck_window_is_shaded                 (WnckWindow *window);
gboolean wnck_window_is_skip_pager             (WnckWindow *window);
gboolean wnck_window_is_skip_tasklist          (WnckWindow *window);
gboolean wnck_window_is_sticky                 (WnckWindow *window);

void wnck_window_set_skip_pager    (WnckWindow *window,
                                    gboolean skip);
void wnck_window_set_skip_tasklist (WnckWindow *window,
                                    gboolean skip);

void wnck_window_close                   (WnckWindow *window);
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
void wnck_window_keyboard_move           (WnckWindow *window);
void wnck_window_keyboard_size           (WnckWindow *window);

WnckWorkspace* wnck_window_get_workspace     (WnckWindow    *window);
void           wnck_window_move_to_workspace (WnckWindow    *window,
                                              WnckWorkspace *space);

/* pinned = on all workspaces */
gboolean wnck_window_is_pinned (WnckWindow *window);
void     wnck_window_pin       (WnckWindow *window);
void     wnck_window_unpin     (WnckWindow *window);

void     wnck_window_activate  (WnckWindow *window);
gboolean wnck_window_is_active (WnckWindow *window);
void     wnck_window_activate_transient (WnckWindow *window);


GdkPixbuf* wnck_window_get_icon      (WnckWindow *window);
GdkPixbuf* wnck_window_get_mini_icon (WnckWindow *window);

gboolean wnck_window_get_icon_is_fallback (WnckWindow *window);

void wnck_window_set_icon_geometry        (WnckWindow *window,
					   int         x,
					   int         y,
					   int         width,
					   int         height);

WnckWindowActions wnck_window_get_actions (WnckWindow *window);
WnckWindowState   wnck_window_get_state   (WnckWindow *window);

void wnck_window_get_geometry (WnckWindow *window,
                               int        *xp,
                               int        *yp,
                               int        *widthp,
                               int        *heightp);

gboolean wnck_window_is_visible_on_workspace (WnckWindow    *window,
                                              WnckWorkspace *workspace);
gboolean wnck_window_is_on_workspace         (WnckWindow    *window,
                                              WnckWorkspace *workspace);
gboolean wnck_window_is_in_viewport          (WnckWindow    *window,
                                              WnckWorkspace *workspace);

G_END_DECLS

#endif /* WNCK_WINDOW_H */
