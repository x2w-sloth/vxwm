#ifndef VXWM_GLOBAL_H
#define VXWM_GLOBAL_H

#include <xcb/xcb.h>

// keep this list short
extern xcb_connection_t *conn;
extern xcb_screen_t *scr;
extern xcb_window_t root;

#endif // VXWM_GLOBAL_H
