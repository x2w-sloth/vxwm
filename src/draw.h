#ifndef VXWM_DRAW_H
#define VXWM_DRAW_H

#include <xcb/xcb.h>

typedef uint32_t color_t;

void draw_setup(void);
void draw_cleanup(void);
void draw_copy(xcb_drawable_t dst, int x, int y, int w, int h);
void draw_rect(int x, int y, int w, int h, uint32_t clr, double lw);
void draw_rect_filled(int x, int y, int w, int h, color_t clr);
void draw_text(int x, int y, int w, int h, const char *text, uint32_t clr);

#endif // VXWM_DRAW_H
