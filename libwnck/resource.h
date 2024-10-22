/* util header */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2006-2007 Vincent Untz
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__LIBWNCK_H_INSIDE__) && !defined (WNCK_COMPILATION)
#error "Only <libwnck/libwnck.h> can be included directly."
#endif

#ifndef WNCK_RESOURCE_H
#define WNCK_RESOURCE_H

#include <gtk/gtk.h>
#include <libwnck/wnck-macros.h>

G_BEGIN_DECLS

typedef struct _WnckResourceUsage WnckResourceUsage;

/**
 * WnckResourceUsage:
 * @total_bytes_estimate: estimation of the total number of bytes allocated in
 * the X server.
 * @pixmap_bytes: number of bytes allocated in the X server for resources of
 * type Pixmap.
 * @n_pixmaps: number of Pixmap resources allocated.
 * @n_windows: number of Window resources allocated.
 * @n_gcs: number of GContext resources allocated.
 * @n_pictures: number of Picture resources allocated.
 * @n_glyphsets: number of Glyphset resources allocated.
 * @n_fonts: number of Font resources allocated.
 * @n_colormap_entries: number of Colormap resources allocated.
 * @n_passive_grabs: number of PassiveGrab resources allocated.
 * @n_cursors: number of Cursor resources allocated.
 * @n_other: number of other resources allocated.
 *
 * The #WnckResourceUsage struct contains information about the total resource
 * usage of an X client, and the number of resources allocated for each
 * resource type.
 *
 * Since: 2.6
 */
struct _WnckResourceUsage
{
  gulong        total_bytes_estimate;

  gulong        pixmap_bytes;

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

  /*< private >*/
  unsigned int pad1;
  unsigned int pad2;
  unsigned int pad3;
  unsigned int pad4;
  unsigned int pad5;
  unsigned long pad6;
  unsigned long pad7;
  unsigned long pad8;
  unsigned long pad9;
};

WNCK_EXPORT
void wnck_xid_read_resource_usage (GdkDisplay        *gdk_display,
                                   gulong             xid,
                                   WnckResourceUsage *usage);

WNCK_EXPORT
void wnck_pid_read_resource_usage (GdkDisplay        *gdk_display,
                                   gulong             pid,
                                   WnckResourceUsage *usage);

G_END_DECLS

#endif /* WNCK_RESOURCE_H */
