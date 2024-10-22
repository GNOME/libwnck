/* window object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Kim Woelders
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

#include "config.h"

#include "private.h"

static void
_wnck_ensure_fallback_style (void)
{
  static gboolean css_loaded = FALSE;
  GtkCssProvider *provider;
  guint priority;

  if (css_loaded)
    return;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/libwnck/wnck.css");

  priority = GTK_STYLE_PROVIDER_PRIORITY_FALLBACK;
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             priority);

  g_object_unref (provider);

  css_loaded = TRUE;
}

char *
_wnck_window_get_name_for_display (WnckWindow *window,
                                   gboolean    use_icon_name,
                                   gboolean    use_state_decorations)
{
  const char *name;

  g_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);

  if (use_icon_name && wnck_window_has_icon_name (window))
    name = wnck_window_get_icon_name (window);
  else
    name = wnck_window_get_name (window);

  if (use_state_decorations)
    {
      if (wnck_window_is_shaded (window))
        return g_strdup_printf ("=%s=", name);
      else if (wnck_window_is_minimized (window))
        return g_strdup_printf ("[%s]", name);
      else
        return g_strdup (name);
    }
  else
    return g_strdup (name);
}

void
_make_gtk_label_bold (GtkLabel *label)
{
  GtkStyleContext *context;

  _wnck_ensure_fallback_style ();

  context = gtk_widget_get_style_context (GTK_WIDGET (label));
  gtk_style_context_add_class (context, "wnck-needs-attention");
}

void
_make_gtk_label_normal (GtkLabel *label)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (label));
  gtk_style_context_remove_class (context, "wnck-needs-attention");
}
