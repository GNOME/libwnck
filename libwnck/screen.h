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

/* forward decls */
typedef struct _WnckApplication WnckApplication;
typedef struct _WnckClassGroup  WnckClassGroup;
typedef struct _WnckWindow      WnckWindow;
typedef struct _WnckWorkspace   WnckWorkspace;

/* Screen */

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

  /* focused window changed */
  void (* active_window_changed)    (WnckScreen *screen);
  /* current workspace changed */
  void (* active_workspace_changed) (WnckScreen *screen);
  /* stacking order changed */
  void (* window_stacking_changed)  (WnckScreen *screen);
  /* window added */
  void (* window_opened)            (WnckScreen *screen,
                                     WnckWindow *window);
  /* window removed */
  void (* window_closed)            (WnckScreen *screen,
                                     WnckWindow *window);
  /* new workspace */
  void (* workspace_created)        (WnckScreen *screen,
                                     WnckWorkspace *space);
  /* workspace gone */
  void (* workspace_destroyed)      (WnckScreen *screen,
                                     WnckWorkspace *space);
  /* new app */
  void (* application_opened)       (WnckScreen      *screen,
                                     WnckApplication *app);
  /* app gone */
  void (* application_closed)       (WnckScreen      *screen,
                                     WnckApplication *app);

  /* New background */
  void (* background_changed)       (WnckScreen      *screen);

#if 0
  /* FIXME uncomment all this next time we feel like breaking ABI */

  /* new class group */
  void (* class_group_opened)       (WnckScreen     *screen,
                                     WnckClassGroup *class_group);
  /* class group gone */
  void (* class_group_closed)       (WnckScreen     *screen,
                                     WnckClassGroup *class_group);
  /* Toggle showing desktop */
  void (* showing_desktop_changed)  (WnckScreen      *screen);

  /* Viewport stuff changed */
  void (* viewports_changed)        (WnckScreen      *screen);
  
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
  void (* pad5) (void);
  void (* pad6) (void);
  void (* pad7) (void);
  void (* pad8) (void);
#endif
};

GType wnck_screen_get_type (void) G_GNUC_CONST;

WnckScreen*    wnck_screen_get_default              (void);
WnckScreen*    wnck_screen_get                      (int         index);
WnckScreen*    wnck_screen_get_for_root             (gulong      root_window_id);
WnckWorkspace* wnck_screen_get_workspace            (WnckScreen *screen,
                                                     int         workspace);
WnckWorkspace* wnck_screen_get_active_workspace     (WnckScreen *screen);
WnckWindow*    wnck_screen_get_active_window        (WnckScreen *screen);
GList*         wnck_screen_get_windows              (WnckScreen *screen);
GList*         wnck_screen_get_windows_stacked      (WnckScreen *screen);
void           wnck_screen_force_update             (WnckScreen *screen);
int            wnck_screen_get_workspace_count      (WnckScreen *screen);
void           wnck_screen_change_workspace_count   (WnckScreen *screen,
                                                     int         count);
gboolean       wnck_screen_net_wm_supports          (WnckScreen *screen,
                                                     const char *atom);
gulong         wnck_screen_get_background_pixmap    (WnckScreen *screen);
int            wnck_screen_get_width                (WnckScreen *screen);
int            wnck_screen_get_height               (WnckScreen *screen);
gboolean       wnck_screen_get_showing_desktop      (WnckScreen *screen);
void           wnck_screen_toggle_showing_desktop   (WnckScreen *screen,
                                                     gboolean    show);
void           wnck_screen_move_viewport            (WnckScreen *screen,
                                                     int         x,
                                                     int         y);
int            wnck_screen_try_set_workspace_layout (WnckScreen *screen,
                                                     int         current_token,
                                                     int         rows,
                                                     int         columns);
void           wnck_screen_release_workspace_layout (WnckScreen *screen,
                                                     int         current_token);


G_END_DECLS

#endif /* WNCK_SCREEN_H */
