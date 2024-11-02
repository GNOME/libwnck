/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2005-2007 Vincent Untz
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
#include "wnck-icon-cache-private.h"

#include <cairo-xlib.h>
#if HAVE_CAIRO_XLIB_XRENDER
#include <cairo-xlib-xrender.h>
#endif

#include "private.h"
#include "xutils.h"

typedef enum
{
  /* These MUST be in ascending order of preference;
   * i.e. if we get _NET_WM_ICON and already have
   * WM_HINTS, we prefer _NET_WM_ICON
   */
  USING_NO_ICON,
  USING_FALLBACK_ICON,
  USING_WM_HINTS,
  USING_NET_WM_ICON
} IconOrigin;

struct _WnckIconCache
{
  GObject parent;

  Window xwindow;
  WnckScreen *screen;

  IconOrigin origin;
  Pixmap pixmap;
  Pixmap mask;
  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;
  guint want_fallback : 1;
  /* TRUE if these props have changed */
  guint wm_hints_dirty : 1;
  guint net_wm_icon_dirty : 1;
};

enum
{
  INVALIDATED,
  LAST_SIGNAL
};

static guint icon_cache_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (WnckIconCache, _wnck_icon_cache, G_TYPE_OBJECT)

static gboolean
_wnck_icon_cache_get_icon_invalidated (WnckIconCache *icon_cache);

static gboolean
find_best_size (gulong  *data,
                gulong   nitems,
                int      ideal_size,
                int     *width,
                int     *height,
                gulong **start)
{
  int best_w;
  int best_h;
  gulong *best_start;

  *width = 0;
  *height = 0;
  *start = NULL;

  best_w = 0;
  best_h = 0;
  best_start = NULL;

  while (nitems > 0)
    {
      int w, h;
      gboolean replace;

      replace = FALSE;

      if (nitems < 3)
        return FALSE; /* no space for w, h */

      w = data[0];
      h = data[1];

      if (nitems < ((gulong) (w * h) + 2))
        break; /* not enough data */

      if (best_start == NULL)
        {
          replace = TRUE;
        }
      else
        {
          /* work with averages */
          int best_size = (best_w + best_h) / 2;
          int this_size = (w + h) / 2;

          /* larger than desired is always better than smaller */
          if (best_size < ideal_size &&
              this_size >= ideal_size)
            replace = TRUE;
          /* if we have too small, pick anything bigger */
          else if (best_size < ideal_size &&
                   this_size > best_size)
            replace = TRUE;
          /* if we have too large, pick anything smaller
           * but still >= the ideal
           */
          else if (best_size > ideal_size &&
                   this_size >= ideal_size &&
                   this_size < best_size)
            replace = TRUE;
        }

      if (replace)
        {
          best_start = data + 2;
          best_w = w;
          best_h = h;
        }

      data += (w * h) + 2;
      nitems -= (w * h) + 2;
    }

  if (best_start)
    {
      *start = best_start;
      *width = best_w;
      *height = best_h;
      return TRUE;
    }
  else
    return FALSE;
}

static void
argbdata_to_pixdata (gulong *argb_data, int len, guchar **pixdata)
{
  guchar *p;
  int i;

  *pixdata = g_new (guchar, len * 4);
  p = *pixdata;

  /* One could speed this up a lot. */
  i = 0;
  while (i < len)
    {
      guint argb;
      guint rgba;

      argb = argb_data[i];
      rgba = (argb << 8) | (argb >> 24);

      *p = rgba >> 24;
      ++p;
      *p = (rgba >> 16) & 0xff;
      ++p;
      *p = (rgba >> 8) & 0xff;
      ++p;
      *p = rgba & 0xff;
      ++p;

      ++i;
    }
}

static gboolean
read_rgb_icon (Screen  *screen,
               Window   xwindow,
               int      ideal_size,
               int     *width,
               int     *height,
               guchar **pixdata)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  int result, err;
  gulong *data;
  gulong *best;
  int w, h;

  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);
  type = None;
  data = NULL;
  result = XGetWindowProperty (display,
			       xwindow,
			       _wnck_atom_get ("_NET_WM_ICON"),
			       0, G_MAXLONG,
			       False, XA_CARDINAL, &type, &format, &nitems,
			       &bytes_after, (void*)&data);

  err = _wnck_error_trap_pop (display);

  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_CARDINAL)
    {
      XFree (data);
      return FALSE;
    }

  if (!find_best_size (data, nitems, ideal_size, &w, &h, &best))
    {
      XFree (data);
      return FALSE;
    }

  *width = w;
  *height = h;

  argbdata_to_pixdata (best, w * h, pixdata);

  XFree (data);

  return TRUE;
}

static gboolean
try_pixmap_and_mask (Screen     *screen,
                     Pixmap      src_pixmap,
                     Pixmap      src_mask,
                     GdkPixbuf **iconp,
                     int         ideal_size)
{
  cairo_surface_t *surface, *mask_surface, *image;
  GdkDisplay *gdk_display;
  GdkPixbuf *unscaled;
  int width, height;
  cairo_t *cr;

  if (src_pixmap == None)
    return FALSE;

  surface = _wnck_cairo_surface_get_from_pixmap (screen, src_pixmap);

  if (surface && src_mask != None)
    mask_surface = _wnck_cairo_surface_get_from_pixmap (screen, src_mask);
  else
    mask_surface = NULL;

  if (surface == NULL)
    return FALSE;

  gdk_display = gdk_x11_lookup_xdisplay (XDisplayOfScreen (screen));
  g_assert (gdk_display != NULL);

  gdk_x11_display_error_trap_push (gdk_display);

  width = cairo_xlib_surface_get_width (surface);
  height = cairo_xlib_surface_get_height (surface);

  image = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                      width, height);
  cr = cairo_create (image);

  /* Need special code for alpha-only surfaces. We only get those
   * for bitmaps. And in that case, it's a differentiation between
   * foreground (white) and background (black).
   */
  if (cairo_surface_get_content (surface) & CAIRO_CONTENT_ALPHA)
    {
      cairo_push_group (cr);

      /* black background */
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_paint (cr);
      /* mask with white foreground */
      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_mask_surface (cr, surface, 0, 0);

      cairo_pop_group_to_source (cr);
    }
  else
    cairo_set_source_surface (cr, surface, 0, 0);

  if (mask_surface)
    {
      cairo_mask_surface (cr, mask_surface, 0, 0);
      cairo_surface_destroy (mask_surface);
    }
  else
    cairo_paint (cr);

  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  if (gdk_x11_display_error_trap_pop (gdk_display) != Success)
    {
      cairo_surface_destroy (image);
      return FALSE;
    }

  unscaled = gdk_pixbuf_get_from_surface (image,
                                          0, 0,
                                          width, height);

  cairo_surface_destroy (image);

  if (unscaled)
    {
      *iconp =
        gdk_pixbuf_scale_simple (unscaled,
                                 ideal_size,
                                 ideal_size,
                                 GDK_INTERP_BILINEAR);

      g_object_unref (G_OBJECT (unscaled));
      return TRUE;
    }
  else
    return FALSE;
}

static void
clear_icon_cache (WnckIconCache *icon_cache,
                  gboolean       dirty_all)
{
  if (icon_cache->icon)
    g_object_unref (G_OBJECT (icon_cache->icon));
  icon_cache->icon = NULL;

  if (icon_cache->mini_icon)
    g_object_unref (G_OBJECT (icon_cache->mini_icon));
  icon_cache->mini_icon = NULL;

  icon_cache->origin = USING_NO_ICON;

  if (dirty_all)
    {
      icon_cache->wm_hints_dirty = TRUE;
      icon_cache->net_wm_icon_dirty = TRUE;
    }
}

static void
free_pixels (guchar   *pixels,
             gpointer  data)
{
  g_free (pixels);
}

static GdkPixbuf*
scaled_from_pixdata (guchar *pixdata,
                     int     w,
                     int     h,
                     int     new_w,
                     int     new_h)
{
  GdkPixbuf *src;
  GdkPixbuf *dest;

  src = gdk_pixbuf_new_from_data (pixdata,
                                  GDK_COLORSPACE_RGB,
                                  TRUE,
                                  8,
                                  w, h, w * 4,
                                  free_pixels,
                                  NULL);

  if (src == NULL)
    return NULL;

  if (w != h)
    {
      GdkPixbuf *tmp;
      int size;

      size = MAX (w, h);

      tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, size, size);

      if (tmp != NULL)
        {
          gdk_pixbuf_fill (tmp, 0);
          gdk_pixbuf_copy_area (src, 0, 0, w, h,
                                tmp,
                                (size - w) / 2, (size - h) / 2);

          g_object_unref (src);
          src = tmp;
        }
    }

  if (w != new_w || h != new_h)
    {
      dest = gdk_pixbuf_scale_simple (src, new_w, new_h, GDK_INTERP_BILINEAR);

      g_object_unref (G_OBJECT (src));
    }
  else
    {
      dest = src;
    }

  return dest;
}

static void
emit_invalidated (WnckIconCache *self)
{
  g_signal_emit (self, icon_cache_signals[INVALIDATED], 0);
}

static void
_wnck_icon_cache_finalize (GObject *object)
{
  WnckIconCache *self;

  self = WNCK_ICON_CACHE (object);

  clear_icon_cache (self, FALSE);

  G_OBJECT_CLASS (_wnck_icon_cache_parent_class)->finalize (object);
}

static void
_wnck_icon_cache_class_init (WnckIconCacheClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->finalize = _wnck_icon_cache_finalize;

  icon_cache_signals[INVALIDATED] =
    g_signal_new ("invalidated",
                  WNCK_TYPE_ICON_CACHE,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static void
_wnck_icon_cache_init (WnckIconCache *self)
{
}

WnckIconCache*
_wnck_icon_cache_new (Window      xwindow,
                      WnckScreen *screen)
{
  WnckIconCache *icon_cache;

  icon_cache = g_object_new (WNCK_TYPE_ICON_CACHE, NULL);

  icon_cache->xwindow = xwindow;
  icon_cache->screen = screen;

  icon_cache->origin = USING_NO_ICON;
  icon_cache->pixmap = None;
  icon_cache->mask = None;
  icon_cache->icon = NULL;
  icon_cache->mini_icon = NULL;
  icon_cache->want_fallback = TRUE;
  icon_cache->wm_hints_dirty = TRUE;
  icon_cache->net_wm_icon_dirty = TRUE;

  return icon_cache;
}

void
_wnck_icon_cache_property_changed (WnckIconCache *icon_cache,
                                   Atom           atom,
                                   XWMHints      *hints)
{
  if (atom == _wnck_atom_get ("_NET_WM_ICON"))
    icon_cache->net_wm_icon_dirty = TRUE;
  else if (atom == _wnck_atom_get ("WM_HINTS"))
    {
      if (hints != NULL)
        {
          Pixmap pixmap;
          Pixmap mask;

          pixmap = None;
          mask = None;

          if (hints->flags & IconPixmapHint)
            pixmap = hints->icon_pixmap;

          if (hints->flags & IconMaskHint)
            mask = hints->icon_mask;

          /* We won't update if pixmap is unchanged;
           * avoids a get_from_drawable() on every geometry
           * hints change
           */
          if (icon_cache->pixmap == pixmap &&
              icon_cache->mask == mask)
            return;

          icon_cache->pixmap = pixmap;
          icon_cache->mask = mask;
        }
      else
        {
          icon_cache->pixmap = None;
          icon_cache->mask = None;
        }

      icon_cache->wm_hints_dirty = TRUE;
    }

  if (!_wnck_icon_cache_get_icon_invalidated (icon_cache))
    return;

  clear_icon_cache (icon_cache, TRUE);
  emit_invalidated (icon_cache);
}

static gboolean
_wnck_icon_cache_get_icon_invalidated (WnckIconCache *icon_cache)
{
  if (icon_cache->origin <= USING_WM_HINTS &&
      icon_cache->wm_hints_dirty)
    return TRUE;
  else if (icon_cache->origin <= USING_NET_WM_ICON &&
           icon_cache->net_wm_icon_dirty)
    return TRUE;
  else if (icon_cache->origin < USING_FALLBACK_ICON &&
           icon_cache->want_fallback)
    return TRUE;
  else if (icon_cache->origin == USING_NO_ICON)
    return TRUE;
  else if (icon_cache->origin == USING_FALLBACK_ICON &&
           !icon_cache->want_fallback)
    return TRUE;
  else
    return FALSE;
}

void
_wnck_icon_cache_set_want_fallback (WnckIconCache *icon_cache,
                                    gboolean       setting)
{
  icon_cache->want_fallback = setting;
}

gboolean
_wnck_icon_cache_get_is_fallback (WnckIconCache *icon_cache)
{
  return icon_cache->origin == USING_FALLBACK_ICON;
}

static GdkPixbuf *
_wnck_read_icon (WnckIconCache *icon_cache,
                 int            ideal_size)
{
  Screen *xscreen;
  GdkPixbuf *icon;
  guchar *pixdata;
  int w, h;

  xscreen = _wnck_screen_get_xscreen (icon_cache->screen);

  icon = NULL;
  pixdata = NULL;

  /* Our algorithm here assumes that we can't have for example origin
   * < USING_NET_WM_ICON and icon_cache->net_wm_icon_dirty == FALSE
   * unless we have tried to read NET_WM_ICON.
   *
   * Put another way, if an icon origin is not dirty, then we have
   * tried to read it at the current size. If it is dirty, then
   * we haven't done that since the last change.
   */

  if ((icon_cache->origin <= USING_NET_WM_ICON &&
       icon_cache->net_wm_icon_dirty) ||
      icon_cache->origin == USING_NET_WM_ICON)
    {
      icon_cache->net_wm_icon_dirty = FALSE;

      if (read_rgb_icon (xscreen, icon_cache->xwindow,
                         ideal_size,
                         &w, &h, &pixdata))
        {
          icon = scaled_from_pixdata (pixdata, w, h, ideal_size, ideal_size);

          icon_cache->origin = USING_NET_WM_ICON;

          return icon;
        }
    }

  if ((icon_cache->origin <= USING_WM_HINTS &&
       icon_cache->wm_hints_dirty) ||
      icon_cache->origin == USING_WM_HINTS)
    {
      icon_cache->wm_hints_dirty = FALSE;

      if (icon_cache->pixmap != None &&
          try_pixmap_and_mask (xscreen,
                               icon_cache->pixmap,
                               icon_cache->mask,
                               &icon,
                               ideal_size))
        {
          icon_cache->origin = USING_WM_HINTS;

          return icon;
        }
    }

  if ((icon_cache->want_fallback &&
       icon_cache->origin < USING_FALLBACK_ICON) ||
      icon_cache->origin == USING_FALLBACK_ICON)
    {
      icon = _wnck_get_fallback_icon (ideal_size);

      icon_cache->origin = USING_FALLBACK_ICON;

      return icon;
    }

  return NULL;
}

GdkPixbuf *
_wnck_icon_cache_get_icon (WnckIconCache *self)
{
  WnckHandle *handle;
  int ideal_size;

  if (self->icon != NULL)
    return self->icon;

  handle = wnck_screen_get_handle (self->screen);
  ideal_size = wnck_handle_get_default_icon_size (handle);

  self->icon = _wnck_read_icon (self, ideal_size);

  return self->icon;
}

GdkPixbuf *
_wnck_icon_cache_get_mini_icon (WnckIconCache *self)
{
  WnckHandle *handle;
  int ideal_size;

  if (self->mini_icon != NULL)
    return self->mini_icon;

  handle = wnck_screen_get_handle (self->screen);
  ideal_size = wnck_handle_get_default_mini_icon_size (handle);

  self->mini_icon = _wnck_read_icon (self, ideal_size);

  return self->mini_icon;
}

void
_wnck_icon_cache_invalidate (WnckIconCache *self)
{
  clear_icon_cache (self, TRUE);
  emit_invalidated (self);
}
