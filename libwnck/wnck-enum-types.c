
/* Generated data (by glib-mkenums) */

#include <libwnck/libwnck.h>

/* enumerations from "window.h" */
GType
wnck_window_state_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GFlagsValue values[] = {
      { WNCK_WINDOW_STATE_MINIMIZED, "WNCK_WINDOW_STATE_MINIMIZED", "minimized" },
      { WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY, "WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY", "maximized-horizontally" },
      { WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY, "WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY", "maximized-vertically" },
      { WNCK_WINDOW_STATE_SHADED, "WNCK_WINDOW_STATE_SHADED", "shaded" },
      { WNCK_WINDOW_STATE_SKIP_PAGER, "WNCK_WINDOW_STATE_SKIP_PAGER", "skip-pager" },
      { WNCK_WINDOW_STATE_SKIP_TASKLIST, "WNCK_WINDOW_STATE_SKIP_TASKLIST", "skip-tasklist" },
      { WNCK_WINDOW_STATE_STICKY, "WNCK_WINDOW_STATE_STICKY", "sticky" },
      { 0, NULL, NULL }
    };
    etype = g_flags_register_static ("WnckWindowState", values);
  }
  return etype;
}


/* Generated data ends here */

