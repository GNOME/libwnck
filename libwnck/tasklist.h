/* tasklist object */
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

#ifndef WNCK_TASKLIST_H
#define WNCK_TASKLIST_H

#include <gtk/gtk.h>
#include <libwnck/screen.h>
#include <libwnck/wnck-macros.h>

G_BEGIN_DECLS

#define WNCK_TYPE_TASKLIST              (wnck_tasklist_get_type ())
#define WNCK_TASKLIST(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WNCK_TYPE_TASKLIST, WnckTasklist))
#define WNCK_TASKLIST_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WNCK_TYPE_TASKLIST, WnckTasklistClass))
#define WNCK_IS_TASKLIST(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WNCK_TYPE_TASKLIST))
#define WNCK_IS_TASKLIST_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WNCK_TYPE_TASKLIST))
#define WNCK_TASKLIST_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WNCK_TYPE_TASKLIST, WnckTasklistClass))

typedef struct _WnckTasklist        WnckTasklist;
typedef struct _WnckTasklistClass   WnckTasklistClass;
typedef struct _WnckTasklistPrivate WnckTasklistPrivate;

/**
 * WnckTasklist:
 *
 * The #WnckTasklist struct contains only private fields and should not be
 * directly accessed.
 */
struct _WnckTasklist
{
  GtkContainer parent_instance;

  WnckTasklistPrivate *priv;
};

struct _WnckTasklistClass
{
  GtkContainerClass parent_class;
  
  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

/**
 * WnckTasklistGroupingType:
 * @WNCK_TASKLIST_NEVER_GROUP: never group multiple #WnckWindow of the same
 * #WnckApplication.
 * @WNCK_TASKLIST_AUTO_GROUP: group multiple #WnckWindow of the same
 * #WnckApplication for some #WnckApplication, when there is not enough place
 * to have a good-looking list of all #WnckWindow.
 * @WNCK_TASKLIST_ALWAYS_GROUP: always group multiple #WnckWindow of the same
 * #WnckApplication, for all #WnckApplication.
 *
 * Type defining the policy of the #WnckTasklist for grouping multiple
 * #WnckWindow of the same #WnckApplication.
 */
typedef enum {
  WNCK_TASKLIST_NEVER_GROUP,
  WNCK_TASKLIST_AUTO_GROUP,
  WNCK_TASKLIST_ALWAYS_GROUP
} WnckTasklistGroupingType;

WNCK_EXPORT
GType wnck_tasklist_get_type (void) G_GNUC_CONST;

WNCK_EXPORT
GtkWidget *wnck_tasklist_new_with_handle (WnckHandle *handle);

WNCK_EXPORT
void wnck_tasklist_set_grouping (WnckTasklist             *tasklist,
				 WnckTasklistGroupingType  grouping);

WNCK_EXPORT
void wnck_tasklist_set_switch_workspace_on_unminimize (WnckTasklist  *tasklist,
						       gboolean       switch_workspace_on_unminimize);

WNCK_EXPORT
void wnck_tasklist_set_middle_click_close (WnckTasklist  *tasklist,
					   gboolean       middle_click_close);

WNCK_EXPORT
void wnck_tasklist_set_grouping_limit (WnckTasklist *tasklist,
				       gint          limit);

WNCK_EXPORT
void wnck_tasklist_set_include_all_workspaces (WnckTasklist *tasklist,
					       gboolean      include_all_workspaces);

WNCK_EXPORT
void wnck_tasklist_set_button_relief (WnckTasklist *tasklist,
                                      GtkReliefStyle relief);

WNCK_EXPORT
void wnck_tasklist_set_orientation (WnckTasklist *tasklist,
                                    GtkOrientation orient);

WNCK_EXPORT
void wnck_tasklist_set_scroll_enabled (WnckTasklist *tasklist,
                                       gboolean      scroll_enabled);

WNCK_EXPORT
gboolean wnck_tasklist_get_scroll_enabled (WnckTasklist *tasklist);

WNCK_EXPORT
void     wnck_tasklist_set_tooltips_enabled (WnckTasklist *self,
                                             gboolean      tooltips_enabled);

WNCK_EXPORT
gboolean wnck_tasklist_get_tooltips_enabled (WnckTasklist *self);

G_END_DECLS

#endif /* WNCK_TASKLIST_H */
