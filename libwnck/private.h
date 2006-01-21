/* Private stuff */

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

#ifndef WNCK_PRIVATE_H
#define WNCK_PRIVATE_H

#include <config.h>
#include <libintl.h>
#include "screen.h"
#include "window.h"
#include "workspace.h"
#include "application.h"
#include "xutils.h"
#include "pager.h"
#include "util.h"
#ifdef HAVE_STARTUP_NOTIFICATION
#include <libsn/sn.h>
#endif

G_BEGIN_DECLS

#define _(x) dgettext (GETTEXT_PACKAGE, x)

#define WNCK_ACTIVATE_TIMEOUT 1000

WnckClientType _wnck_get_client_type ();

void _wnck_application_process_property_notify (WnckApplication *app,
                                                XEvent          *xevent);
void _wnck_window_process_property_notify (WnckWindow *window,
                                           XEvent     *xevent);

void _wnck_screen_process_property_notify (WnckScreen *screen,
                                           XEvent     *xevent);
void _wnck_window_process_configure_notify (WnckWindow *window,
                                            XEvent     *xevent);
WnckWindow* _wnck_window_create  (Window      xwindow,
                                  WnckScreen *screen,
                                  gint        sort_order);
void        _wnck_window_destroy (WnckWindow *window);

const char* _wnck_window_get_startup_id (WnckWindow *window);

const char* _wnck_window_get_resource_class (WnckWindow *window);
const char* _wnck_window_get_resource_name  (WnckWindow *window);

WnckWorkspace* _wnck_workspace_create  (int            number,
					WnckScreen    *screen);
void           _wnck_workspace_destroy (WnckWorkspace *space);

void _wnck_window_set_application    (WnckWindow      *window,
                                      WnckApplication *app);

void _wnck_window_set_class_group (WnckWindow     *window,
				   WnckClassGroup *class_group);

void _wnck_application_add_window    (WnckApplication *app,
                                      WnckWindow      *window);
void _wnck_application_remove_window (WnckApplication *app,
                                      WnckWindow      *window);

WnckApplication* _wnck_application_create  (Window           xwindow,
                                            WnckScreen      *screen);
void             _wnck_application_destroy (WnckApplication *app);

void _wnck_workspace_update_name (WnckWorkspace *workspace,
                                  const char    *name);
void _wnck_screen_change_workspace_name (WnckScreen *screen,
                                         int         number,
                                         const char *name);

gboolean _wnck_workspace_set_geometry (WnckWorkspace *space, int w, int h);
gboolean _wnck_workspace_set_viewport (WnckWorkspace *space, int x, int y);

void _wnck_init (void);

#define DEFAULT_ICON_WIDTH 32
#define DEFAULT_ICON_HEIGHT 32
#define DEFAULT_MINI_ICON_WIDTH 16
#define DEFAULT_MINI_ICON_HEIGHT 16

#define WNCK_SCREEN_XSCREEN(screen) (_wnck_screen_get_xscreen (screen))

Screen    *_wnck_screen_get_xscreen    (WnckScreen *screen);
int        _wnck_screen_get_number     (WnckScreen *screen);
GdkScreen *_wnck_screen_get_gdk_screen (WnckScreen *screen);

#ifdef HAVE_STARTUP_NOTIFICATION
SnDisplay* _wnck_screen_get_sn_display (WnckScreen *screen);
#endif

WnckScreen* _wnck_screen_get_existing (int number);

void           _wnck_pager_activate_workspace   (WnckWorkspace *wspace,
                                                 guint32        timestamp);
int            _wnck_pager_get_n_workspaces     (WnckPager     *pager);
const char*    _wnck_pager_get_workspace_name   (WnckPager     *pager,
                                                 int            i);
WnckWorkspace* _wnck_pager_get_active_workspace (WnckPager     *pager);
WnckWorkspace* _wnck_pager_get_workspace        (WnckPager     *pager,
                                                 int            i);
void           _wnck_pager_get_workspace_rect   (WnckPager     *pager,
                                                 int            i,
                                                 GdkRectangle  *rect);

void           _make_gtk_label_bold   (GtkLabel *label);
void           _make_gtk_label_normal (GtkLabel *label);


G_END_DECLS

#endif /* WNCK_PRIVATE_H */
