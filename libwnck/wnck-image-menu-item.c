/*
 * Copyright (C) 2016 Alberts Muktupāvels
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "wnck-image-menu-item-private.h"
#include "private.h"

#define SPACING 6

struct _WnckImageMenuItem
{
  GtkMenuItem  parent;

  GtkWidget   *box;
  GtkWidget   *image;
  GtkWidget   *accel_label;

  gchar       *label;
};

G_DEFINE_TYPE (WnckImageMenuItem, wnck_image_menu_item, GTK_TYPE_MENU_ITEM)

static void
wnck_image_menu_item_finalize (GObject *object)
{
  WnckImageMenuItem *item;

  item = WNCK_IMAGE_MENU_ITEM (object);

  g_clear_pointer (&item->label, g_free);

  G_OBJECT_CLASS (wnck_image_menu_item_parent_class)->finalize (object);
}

static void
wnck_image_menu_item_get_preferred_width (GtkWidget *widget,
                                          gint      *minimum,
                                          gint      *natural)
{
  GtkWidgetClass *widget_class;
  WnckImageMenuItem *item;
  GtkRequisition image_requisition;

  widget_class = GTK_WIDGET_CLASS (wnck_image_menu_item_parent_class);
  item = WNCK_IMAGE_MENU_ITEM (widget);

  widget_class->get_preferred_width (widget, minimum, natural);

  if (!gtk_widget_get_visible (item->image))
    return;

  gtk_widget_get_preferred_size (item->image, &image_requisition, NULL);

  if (image_requisition.width > 0)
    {
      *minimum -= image_requisition.width + SPACING;
      *natural -= image_requisition.width + SPACING;
    }
}

static void
wnck_image_menu_item_size_allocate (GtkWidget     *widget,
                                    GtkAllocation *allocation)
{
  GtkWidgetClass *widget_class;
  WnckImageMenuItem *item;
  GtkRequisition image_requisition;
  GtkAllocation box_allocation;

  widget_class = GTK_WIDGET_CLASS (wnck_image_menu_item_parent_class);
  item = WNCK_IMAGE_MENU_ITEM (widget);

  widget_class->size_allocate (widget, allocation);

  if (!gtk_widget_get_visible (item->image))
    return;

  gtk_widget_get_preferred_size (item->image, &image_requisition, NULL);
  gtk_widget_get_allocation (item->box, &box_allocation);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    {
      if (image_requisition.width > 0)
        box_allocation.x -= image_requisition.width + SPACING;
    }
  else
    {
      if (image_requisition.width > 0)
        box_allocation.x += image_requisition.width + SPACING;
    }

  gtk_widget_size_allocate (item->box, &box_allocation);
}

static const gchar *
wnck_image_menu_item_get_label (GtkMenuItem *menu_item)
{
  WnckImageMenuItem *item;

  item = WNCK_IMAGE_MENU_ITEM (menu_item);

  return item->label;
}

static void
wnck_image_menu_item_toggle_size_request (GtkMenuItem *menu_item,
                                          gint        *requisition)
{
  WnckImageMenuItem *item;
  GtkRequisition image_requisition;

  item = WNCK_IMAGE_MENU_ITEM (menu_item);

  *requisition = 0;

  if (!gtk_widget_get_visible (item->image))
    return;

  gtk_widget_get_preferred_size (item->image, &image_requisition, NULL);

  if (image_requisition.width > 0)
    *requisition = image_requisition.width + SPACING;
}

static void
wnck_image_menu_item_set_label (GtkMenuItem *menu_item,
                                const gchar *label)
{
  WnckImageMenuItem *item;

  item = WNCK_IMAGE_MENU_ITEM (menu_item);

  if (g_strcmp0 (item->label, label) != 0)
    {
      g_free (item->label);
      item->label = g_strdup (label);

      gtk_label_set_text (GTK_LABEL (item->accel_label), label);
      g_object_notify (G_OBJECT (menu_item), "label");
    }
}

static void
wnck_image_menu_item_class_init (WnckImageMenuItemClass *item_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkMenuItemClass *menu_item_class;

  object_class = G_OBJECT_CLASS (item_class);
  widget_class = GTK_WIDGET_CLASS (item_class);
  menu_item_class = GTK_MENU_ITEM_CLASS (item_class);

  object_class->finalize = wnck_image_menu_item_finalize;

  widget_class->get_preferred_width = wnck_image_menu_item_get_preferred_width;
  widget_class->size_allocate = wnck_image_menu_item_size_allocate;

  menu_item_class->get_label = wnck_image_menu_item_get_label;
  menu_item_class->toggle_size_request = wnck_image_menu_item_toggle_size_request;
  menu_item_class->set_label = wnck_image_menu_item_set_label;
}

static void
wnck_image_menu_item_init (WnckImageMenuItem *item)
{
  GtkAccelLabel *accel_label;

  item->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, SPACING);
  gtk_container_add (GTK_CONTAINER (item), item->box);
  gtk_widget_show (item->box);

  item->image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (item->box), item->image, FALSE, FALSE, 0);

  item->accel_label = gtk_accel_label_new ("");
  gtk_box_pack_end (GTK_BOX (item->box), item->accel_label, TRUE, TRUE, 0);
  gtk_label_set_xalign (GTK_LABEL (item->accel_label), 0.0);
  gtk_widget_show (item->accel_label);

  accel_label = GTK_ACCEL_LABEL (item->accel_label);
  gtk_accel_label_set_accel_widget (accel_label, GTK_WIDGET (item));
  gtk_label_set_ellipsize (GTK_LABEL (accel_label), PANGO_ELLIPSIZE_END);
}

GtkWidget *
wnck_image_menu_item_new (void)
{
  return g_object_new (WNCK_TYPE_IMAGE_MENU_ITEM, NULL);
}

GtkWidget *
wnck_image_menu_item_new_with_label (const gchar *label)
{
  return g_object_new (WNCK_TYPE_IMAGE_MENU_ITEM, "label", label, NULL);
}

void
wnck_image_menu_item_set_image_from_icon_pixbuf (WnckImageMenuItem *item,
                                                 GdkPixbuf         *pixbuf)
{
  gtk_image_set_from_pixbuf (GTK_IMAGE (item->image), pixbuf);
  gtk_widget_show (item->image);
}

void
wnck_image_menu_item_set_image_from_window (WnckImageMenuItem *item,
                                            WnckWindow        *window)
{
  _wnck_selector_set_window_icon (item->image, window);
  gtk_widget_show (item->image);
}

void
wnck_image_menu_item_make_label_bold (WnckImageMenuItem *item)
{
  _make_gtk_label_bold (GTK_LABEL (item->accel_label));
}

void
wnck_image_menu_item_make_label_normal (WnckImageMenuItem *item)
{
  _make_gtk_label_normal (GTK_LABEL (item->accel_label));
}

void
_wnck_image_menu_item_set_max_chars (WnckImageMenuItem *self,
                                     int                n_chars)
{
  gtk_label_set_max_width_chars (GTK_LABEL (self->accel_label), n_chars);
}
