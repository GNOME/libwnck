/* util header */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Red Hat, Inc.
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

#ifndef WNCK_UTIL_H
#define WNCK_UTIL_H

#include <gtk/gtk.h>

typedef struct
{
  unsigned long total_bytes_estimate;
  
  unsigned long pixmap_bytes;

  unsigned int n_pixmaps;
  unsigned int n_windows;
  unsigned int n_gcs;
  unsigned int n_pictures;
  unsigned int n_glyphsets;
  unsigned int n_fonts;
  unsigned int n_colormap_entries;
  unsigned int n_passive_grabs;
  unsigned int n_cursors;
  unsigned int n_other;

  unsigned int pad1;
  unsigned int pad2;
  unsigned int pad3;
  unsigned int pad4;
  unsigned int pad5;
  unsigned long pad6;
  unsigned long pad7;
  unsigned long pad8;
  unsigned long pad9;
  
} WnckResourceUsage;

void wnck_gtk_window_set_dock_type (GtkWindow *window);

void wnck_xid_read_resource_usage (GdkDisplay        *gdk_display,
                                   unsigned long      xid,
                                   WnckResourceUsage *usage);

void wnck_pid_read_resource_usage (GdkDisplay        *gdk_display,
                                   unsigned long      pid,
                                   WnckResourceUsage *usage);

#endif /* WNCK_UTIL_H */


