/* pager object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003, 2005-2007 Vincent Untz
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

#ifndef WNCK_PAGER_H
#define WNCK_PAGER_H

#include <gtk/gtk.h>
#include <libwnck/screen.h>
#include <libwnck/wnck-macros.h>

G_BEGIN_DECLS

#define WNCK_TYPE_PAGER              (wnck_pager_get_type ())
#define WNCK_PAGER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WNCK_TYPE_PAGER, WnckPager))
#define WNCK_PAGER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WNCK_TYPE_PAGER, WnckPagerClass))
#define WNCK_IS_PAGER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WNCK_TYPE_PAGER))
#define WNCK_IS_PAGER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WNCK_TYPE_PAGER))
#define WNCK_PAGER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WNCK_TYPE_PAGER, WnckPagerClass))

typedef struct _WnckPager        WnckPager;
typedef struct _WnckPagerClass   WnckPagerClass;
typedef struct _WnckPagerPrivate WnckPagerPrivate;

/**
 * WnckPager:
 *
 * The #WnckPager struct contains only private fields and should not be
 * directly accessed.
 */
struct _WnckPager
{
  GtkContainer parent_instance;

  WnckPagerPrivate *priv;
};

struct _WnckPagerClass
{
  GtkContainerClass parent_class;
  
  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

/**
 * WnckPagerDisplayMode:
 * @WNCK_PAGER_DISPLAY_NAME: the #WnckPager will only display the names of the
 * workspaces.
 * @WNCK_PAGER_DISPLAY_CONTENT: the #WnckPager will display a representation
 * for each window in the workspaces.
 *
 * Mode defining what a #WnckPager will display.
 */
typedef enum {
  WNCK_PAGER_DISPLAY_NAME,
  WNCK_PAGER_DISPLAY_CONTENT
} WnckPagerDisplayMode;

/**
 * WnckPagerScrollMode:
 * @WNCK_PAGER_SCROLL_2D: given that the workspaces are set up in multiple rows,
 * scrolling on the #WnckPager will cycle through the workspaces as if on a
 * 2-dimensional map. Example cycling order with 2 rows and 4 workspaces: 1 3 2 4.
 * @WNCK_PAGER_SCROLL_1D: the #WnckPager will always cycle workspaces in a linear
 * manner, irrespective of how many rows are configured. (Hint: Better for mice)
 * Example cycling order with 2 rows and 4 workspaces: 1 2 3 4.
 * @WNCK_PAGER_SCROLL_NONE: the #WnckPager will not cycle workspaces. Since 3.40.
 *
 * Mode defining in which order scrolling on a #WnckPager will cycle through workspaces.
 *
 * Since: 3.36
 */
typedef enum {
  WNCK_PAGER_SCROLL_2D,
  WNCK_PAGER_SCROLL_1D,
  WNCK_PAGER_SCROLL_NONE
} WnckPagerScrollMode;

WNCK_EXPORT
GType      wnck_pager_get_type           (void) G_GNUC_CONST;

WNCK_EXPORT
GtkWidget* wnck_pager_new_with_handle    (WnckHandle           *handle);

WNCK_EXPORT
gboolean   wnck_pager_set_orientation    (WnckPager            *pager,
                                          GtkOrientation        orientation);

WNCK_EXPORT
gboolean   wnck_pager_set_n_rows         (WnckPager            *pager,
                                          int                   n_rows);

WNCK_EXPORT
void       wnck_pager_set_display_mode   (WnckPager            *pager,
                                          WnckPagerDisplayMode  mode);

WNCK_EXPORT
void       wnck_pager_set_scroll_mode    (WnckPager            *pager,
                                          WnckPagerScrollMode   scroll_mode);

WNCK_EXPORT
void       wnck_pager_set_show_all       (WnckPager            *pager,
                                          gboolean              show_all_workspaces);

WNCK_EXPORT
void       wnck_pager_set_shadow_type    (WnckPager            *pager,
                                          GtkShadowType         shadow_type);

WNCK_EXPORT
void       wnck_pager_set_wrap_on_scroll (WnckPager            *pager,
                                          gboolean              wrap_on_scroll);

WNCK_EXPORT
gboolean   wnck_pager_get_wrap_on_scroll (WnckPager            *pager);

G_END_DECLS

#endif /* WNCK_PAGER_H */
