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

#include "screen.h"
#include "window.h"
#include "workspace.h"
#include "application.h"
#include "xutils.h"

G_BEGIN_DECLS

void _wnck_window_process_property_notify (WnckWindow *window,
                                           XEvent     *xevent);

void _wnck_screen_process_property_notify (WnckScreen *screen,
                                           XEvent     *xevent);

WnckWindow* _wnck_window_create  (Window      xwindow,
                                  WnckScreen *screen);
void        _wnck_window_destroy (WnckWindow *window);

WnckWorkspace* _wnck_workspace_create  (int            number);
void           _wnck_workspace_destroy (WnckWorkspace *space);

void _wnck_window_set_application    (WnckWindow      *window,
                                      WnckApplication *app);
void _wnck_application_add_window    (WnckApplication *app,
                                      WnckWindow      *window);
void _wnck_application_remove_window (WnckApplication *app,
                                      WnckWindow      *window);

WnckApplication* _wnck_application_create  (Window           xwindow,
                                            WnckScreen      *screen);
void             _wnck_application_destroy (WnckApplication *app);

#define DEFAULT_ICON_WIDTH 32
#define DEFAULT_ICON_HEIGHT 32
#define DEFAULT_MINI_ICON_WIDTH 16
#define DEFAULT_MINI_ICON_HEIGHT 16

G_END_DECLS

#endif /* WNCK_PRIVATE_H */
