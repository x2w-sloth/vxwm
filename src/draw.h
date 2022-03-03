#ifndef VXWM_DRAW_H
#define VXWM_DRAW_H

#include <xcb/xcb.h>

typedef uint32_t color_t;

void draw_setup(void);
void draw_cleanup(void);
void draw_copy(xcb_drawable_t dst, int x, int y, int w, int h);
void draw_rect(int x, int y, int w, int h, color_t clr, double lw);
void draw_rect_filled(int x, int y, int w, int h, color_t clr);
void draw_arc_filled(int x, int y, double r, double deg1, double deg2, color_t clr);
void draw_text_extents(const char *text, int *tw, int *th);
void draw_text(int x, int y, int w, int h, const char *text, color_t clr, int lpad);

#endif // VXWM_DRAW_H
