#include <xcb/xproto.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include "draw.h"
#include "util.h"
#include "global.h"

#define R256(color)     ((color >> 16) & 255)
#define G256(color)     ((color >> 8) & 255)
#define B256(color)     (color & 255)

static xcb_visualtype_t *get_visual_type(xcb_screen_t *scr);
static void draw_set_color(color_t clr);
static void draw_set_line_width(double lw);
static xcb_gcontext_t gc;
static xcb_drawable_t pixmap;
static cairo_surface_t *surface;
static cairo_t *cr;
static color_t source_clr;
static double source_lw;

xcb_visualtype_t *get_visual_type(xcb_screen_t *scr)
{
	xcb_depth_iterator_t di;
	xcb_visualtype_iterator_t vi;
	xcb_visualtype_t *vt = NULL;

  di = xcb_screen_allowed_depths_iterator(scr);
  for (; !vt && di.rem; xcb_depth_next(&di)) {
    vi = xcb_depth_visuals_iterator(di.data);
    for (; !vt && vi.rem; xcb_visualtype_next(&vi))
      if (scr->root_visual == vi.data->visual_id)
        vt = vi.data;
  }
  return vt;
}

inline
void draw_set_color(color_t clr)
{
  if (source_clr != clr) {
    source_clr = clr;
    cairo_set_source_rgb(cr, R256(clr)/255.0, G256(clr)/255.0, B256(clr)/255.0);
  }
}

inline
void draw_set_line_width(double lw)
{
  if (source_lw != lw) {
    source_lw = lw;
    cairo_set_line_width(cr, lw);
  }
}

void draw_setup(void)
{
  uint32_t scrw = scr->width_in_pixels, scrh = scr->height_in_pixels;
  uint32_t val = 0;
  xcb_visualtype_t *vt;

  // setup xcb graphics context and pixmap buffer
  gc = xcb_generate_id(conn);
  xcb_create_gc(conn, gc, root, XCB_GC_GRAPHICS_EXPOSURES, &val);
  pixmap = xcb_generate_id(conn);
  xcb_create_pixmap(conn, scr->root_depth, pixmap, root, scrw, scrh);

  // create cairo surface and context
  vt = get_visual_type(scr);
  surface = cairo_xcb_surface_create(conn, pixmap, vt, scrw, scrh);
  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
    die("failed to allocate surface on pixmap");
  cr = cairo_create(surface);
}

void draw_cleanup(void)
{
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  xcb_free_pixmap(conn, pixmap);
  xcb_free_gc(conn, gc);
}

void draw_copy(xcb_drawable_t dst, int x, int y, int w, int h)
{
  xcb_copy_area(conn, pixmap, dst, gc, x, y, x, y, w, h);
  xcb_flush(conn);
}

void draw_rect(int x, int y, int w, int h, uint32_t clr, double lw)
{
  draw_set_color(clr);
  draw_set_line_width(lw);
  cairo_rectangle(cr, x, y, w, h);
  cairo_stroke(cr);
}

void draw_rect_filled(int x, int y, int w, int h, uint32_t clr)
{
  draw_set_color(clr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_fill(cr);
}

// vim: ts=2:sw=2:et
