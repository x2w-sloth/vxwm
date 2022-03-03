#include <xcb/xproto.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include "draw.h"
#include "util.h"
#include "global.h"

#define M_PI            3.14159265358979323846
#define R256(color)     (color >> 16 & 255)
#define G256(color)     (color >> 8 & 255)
#define B256(color)     (color & 255)
#define D2R(deg)        (deg * M_PI / 180)

static xcb_visualtype_t *get_visual_type(xcb_screen_t *scr);
static void draw_set_color(color_t clr);
static void draw_set_line_width(double lw);
static xcb_gcontext_t gc;
static xcb_drawable_t pixmap;
static cairo_surface_t *surface;
static cairo_t *cr;
static color_t source_clr;
static double source_lw;
static double font_height, font_descent;

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
  cairo_font_extents_t fe;

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

  // initialize font
  cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 20);
  cairo_font_extents(cr, &fe);
  font_height = fe.height;
  font_descent = fe.descent;
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

void draw_rect(int x, int y, int w, int h, color_t clr, double lw)
{
  draw_set_color(clr);
  draw_set_line_width(lw);
  cairo_rectangle(cr, x, y, w, h);
  cairo_stroke(cr);
}

void draw_rect_filled(int x, int y, int w, int h, color_t clr)
{
  draw_set_color(clr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_fill(cr);
}

void draw_arc_filled(int x, int y, double r, double deg1, double deg2, color_t clr)
{
  draw_set_color(clr);
  cairo_move_to(cr, x, y);
  cairo_arc(cr, x, y, r, D2R(deg1), D2R(deg2));
  cairo_close_path(cr);
  cairo_fill(cr);
}

void draw_text_extents(const char *text, int *tw, int *th)
{
  cairo_text_extents_t te;

  cairo_text_extents(cr, text, &te);
  if (tw) // slighter larger than actual glyph width
    *tw = te.x_advance;
  if (th)
    *th = te.height;
}

// TODO: use parameter w
void draw_text(int x, int y, UNUSED int w, int h, const char *text, color_t clr, int lpad)
{
  if (strlen(text) == 0)
    return;
  draw_set_color(clr);
  cairo_move_to(cr, x + lpad, y + h/2. + font_height/2. - font_descent);
  cairo_show_text(cr, text);
}

// vim: ts=2:sw=2:et
