#include "draw.h"
#include "util.h"
#include "global.h"

static xcb_gcontext_t gc;
static xcb_drawable_t pixmap;

void draw_setup(void)
{
  uint32_t mask = XCB_GC_FUNCTION   | XCB_GC_FOREGROUND | XCB_GC_BACKGROUND |
                  XCB_GC_LINE_WIDTH | XCB_GC_LINE_STYLE | XCB_GC_GRAPHICS_EXPOSURES;
  uint32_t vals[] = {
    XCB_GX_COPY, scr->white_pixel, scr->black_pixel,
    2, XCB_LINE_STYLE_SOLID, 0
  };

  gc = xcb_generate_id(conn);
  xcb_create_gc(conn, gc, root, mask, vals);
  pixmap = xcb_generate_id(conn);
  xcb_create_pixmap(conn, scr->root_depth, pixmap, root,
    scr->width_in_pixels, scr->height_in_pixels);
}

void draw_cleanup(void)
{
  xcb_free_pixmap(conn, pixmap);
  xcb_free_gc(conn, gc);
}

void draw_copy(xcb_drawable_t dst, int x, int y, int w, int h)
{
  xcb_copy_area(conn, pixmap, dst, gc, x, y, x, y, w, h);
  xcb_flush(conn);
}

void draw_rect(int x, int y, int w, int h, uint32_t color)
{
  xcb_rectangle_t rect = { x, y, w, h };
  xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &color);
  xcb_poly_fill_rectangle(conn, pixmap, gc, 1, &rect);
}

// vim: ts=2:sw=2:et
