#ifndef VXWM_WIN_H
#define VXWM_WIN_H

// X11 WINDOW OPERATIONS

#include <stdbool.h>
#include <xcb/xproto.h>
#include "vxwm.h"

bool win_get_geometry(xcb_window_t, int *, int *, int *, int *, int *);
bool win_get_atom_prop(xcb_window_t, xcb_atom_t, xcb_atom_t *);
bool win_get_text_prop(xcb_window_t, xcb_atom_t, char *, uint32_t);
bool win_get_attr(xcb_window_t, bool *, uint8_t *);
bool win_get_state(xcb_window_t, uint32_t *);
void win_stack(xcb_window_t, pos_t);
void win_focus(xcb_window_t);
void win_set_state(xcb_window_t, uint32_t);
bool win_has_proto(xcb_window_t, xcb_atom_t);
void win_send_proto(xcb_window_t, xcb_atom_t);
void win_kill(xcb_window_t);

#endif // VXWM_WIN_H
