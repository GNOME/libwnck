/* util header */

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

#include "util.h"
#include "xutils.h"
#include <gdk/gdkx.h>

static void
set_dock_realize_handler (GtkWidget *widget, gpointer data)
{
  _wnck_set_dock_type_hint (GDK_WINDOW_XWINDOW (widget->window));
}

void
wnck_gtk_window_set_dock_type (GtkWindow *window)
{
  /*  FIXME don't set it 10 times if this function is called 10 times */
  g_signal_connect (G_OBJECT (window),
                    "realize",
                    G_CALLBACK (set_dock_realize_handler),
                    NULL);
}


