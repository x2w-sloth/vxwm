#ifndef VXWM_DRAW_H
#define VXWM_DRAW_H

#include <xcb/xcb.h>

void draw_setup(void);
void draw_cleanup(void);
void draw_copy(xcb_drawable_t, int, int, int, int);
void draw_rect(int, int, int, int, uint32_t);

#endif // VXWM_DRAW_H
